/*
	VbaEnvironment.h

	Ambiente di variabili per l'esecuzione di UNA procedura VBA (Fase
	3, interprete -- vedi la decisione del sottoinsieme in ROADMAP.md):
	un'unica mappa nome->Value, senza ambiti annidati (questo
	sottoinsieme non supporta funzioni/procedure annidate ne' chiusure,
	solo una Sub top-level per volta). I nomi sono confrontati senza
	distinguere maiuscole/minuscole, come VBA stesso (una variabile
	dichiarata "Dim Total" e usata come "total = 5" e' la STESSA
	variabile).

	Porta anche il documento attivo (Fase 3c, modello a oggetti Excel
	minimo): Range/Cells lo leggono per sapere su quale CContainer
	risolvere una cella vera.

	Riusa Value (engine/src/Cell/Value.h), lo stesso tipo gia' usato in
	tutto il motore di calcolo per il valore di una cella -- non un
	tipo Variant separato scritto da zero: numero/stringa/booleano/
	range sono gia' tutti supportati li', inclusi gli operatori
	aritmetici e i confronti che la Fase 3a riusa direttamente.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_ENVIRONMENT_H
#define VBA_ENVIRONMENT_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Value.h"

class CContainer;
struct VbaToken;
struct VbaTypeDecl;

// Fase 6d: un'istanza di un tipo definito dall'utente ("Private Type
// ... End Type", VbaAst.h) -- una mappa nome-campo normalizzato (come
// VbaEnvironment stesso) -> VbaStructField. Volutamente NON copiabile
// (il costruttore di copia e' implicitamente eliminato dagli
// std::unique_ptr nei campi che sono a loro volta una struct, vedi
// VbaStructField sotto): questo sottoinsieme non supporta l'assegnazione
// di un'intera struct ("b = a" dove entrambe sono UDT) ne' il passaggio
// di una struct come parametro di Sub/Function -- limiti onesti, non
// dimenticati, vedi ROADMAP.md Fase 6d.
struct VbaStructInstance;

// Un singolo campo di una VbaStructInstance -- esattamente UNO dei
// quattro membri di storage sotto e' valido, scelto da isArray/isStruct
// al momento della costruzione (VbaExecutor.cpp, BuildVbaStructInstance):
//   !isArray && !isStruct -> scalarValue       (es. "PC As Long")
//    isArray && !isStruct -> scalarArray       (es. "V(15) As Byte")
//   !isArray &&  isStruct -> structValue       (un campo che e' esso
//                                                stesso un altro Type)
//    isArray &&  isStruct -> structArray       (es. "Screen(63,31)
//                                                As ScreenPixel")
struct VbaStructField {
	bool isArray;
	bool isStruct;
	std::vector<int> dims; // solo se isArray: conteggio elementi per asse, come VbaArrayStorage
	Value scalarValue;
	std::vector<Value> scalarArray;
	std::unique_ptr<VbaStructInstance> structValue;
	std::vector<std::unique_ptr<VbaStructInstance>> structArray;

	VbaStructField() : isArray(false), isStruct(false), scalarValue(0.0) {}
};

struct VbaStructInstance {
	std::map<std::string, VbaStructField> fields; // chiave: nome campo normalizzato (minuscolo)

	// NULL se "name" non e' un campo di questa istanza -- il chiamante
	// (VbaEvaluator.cpp/VbaExecutor.cpp) decide poi se isArray/isStruct
	// combaciano con la forma di accesso tentata (scalare, indicizzato,
	// struct annidata...).
	VbaStructField* FindField(const std::string& name);
};

class VbaEnvironment {
public:
	VbaEnvironment() : fActiveDoc(NULL), fModuleTokens(NULL), fCallDepth(0), fTypeRegistry(NULL) {}

	// Il foglio di calcolo (CContainer) su cui questa esecuzione lavora
	// -- Fase 3c, modello a oggetti Excel: Range/Cells lo usano per
	// leggere/scrivere celle vere. Puntatore in prestito, MAI
	// posseduto da VbaEnvironment (stesso principio di
	// Value::fRangeContainer); NULL di default, cosi' un programma
	// fatto solo di variabili (Fase 3a/3b) continua a funzionare senza
	// che il chiamante debba collegare nulla.
	void SetActiveDocument(CContainer* doc) { fActiveDoc = doc; }
	CContainer* GetActiveDocument() const { return fActiveDoc; }
	// Dim: assicura che la variabile esista, inizializzata a vuoto
	// (0.0, come una variabile Variant VBA appena dichiarata) se non
	// gia' presente -- NON reimposta un valore gia' assegnato in
	// precedenza (un secondo "Dim x" sulla stessa variabile, raro ma
	// non deve perdere il valore corrente).
	void Declare(const std::string& name);

	// Legge il valore corrente di una variabile. NULL se non ancora
	// dichiarata (ne' con Dim ne' con una prima assegnazione --
	// questo sottoinsieme, come il vero VBA senza "Option Explicit",
	// permette un'assegnazione implicita: vedi Set sotto).
	const Value* Find(const std::string& name) const;

	// Scrive il valore di una variabile, dichiarandola implicitamente
	// se non ancora presente (stesso comportamento del vero VBA senza
	// "Option Explicit" in cima al modulo -- questo sottoinsieme non
	// supporta "Option Explicit" ne' la verifica che richiederebbe).
	void Set(const std::string& name, const Value& value);

	// Fase 6a: i token del MODULO (non solo della singola Sub) a cui
	// appartiene l'esecuzione corrente -- un puntatore in prestito,
	// mai posseduto, stesso principio di fActiveDoc. Permette
	// all'evaluator (VbaEvaluator.cpp, EvalCall) di risolvere una
	// chiamata a una Sub "sorella" dello stesso modulo con
	// FindVbaSubByName (VbaMacroRunner.h) invece di fermarsi al solo
	// elenco di funzioni predefinite. NULL se l'esecuzione non e'
	// legata a un modulo vero (es. nei test che costruiscono una Sub a
	// mano): in quel caso una chiamata non riconosciuta resta un
	// errore "funzione sconosciuta", come prima di questa fase.
	void SetModuleTokens(const std::vector<VbaToken>* tokens) { fModuleTokens = tokens; }
	const std::vector<VbaToken>* GetModuleTokens() const { return fModuleTokens; }

	// Quante chiamate a Sub annidate hanno portato a QUESTA esecuzione
	// (0 per la macro top-level lanciata da ExecuteVbaMacro) -- cresce
	// di uno per ogni VbaEnvironment creato per una chiamata a Sub
	// sorella (EvalCall), cosi' una ricorsione infinita diventa un
	// errore di runtime pulito ("troppi livelli di chiamata annidata")
	// invece di uno stack overflow nativo -- vedi kMaxVbaCallDepth in
	// VbaEvaluator.cpp.
	void SetCallDepth(int depth) { fCallDepth = depth; }
	int GetCallDepth() const { return fCallDepth; }

	// Fase 6c: array a dimensione FISSA, dichiarati da "Dim
	// nome(limiteSuperiore, ...)" (VbaStmtParser.cpp) -- una mappa
	// SEPARATA da fVars (niente "Value" di tipo array: aggiungere un
	// tipo array al motore di calcolo condiviso da tutto il resto del
	// programma sarebbe un cambiamento enormemente piu' invasivo di
	// quanto serva qui). "dims" e' il conteggio di elementi per ogni
	// asse (gia' "limite superiore + 1"); gli elementi si appiattiscono
	// in un unico vettore riga per riga (stesso ordine del vero VBA per
	// un array multidimensionale), inizializzati a 0.0 (Empty), stesso
	// valore iniziale di una variabile scalare dichiarata con "Dim".
	//
	// Un secondo "Dim" sullo stesso nome RIDIMENSIONA e azzera di nuovo
	// l'array (niente "ReDim Preserve" in questo sottoinsieme -- vedi
	// ROADMAP.md Fase 6c, scope limitato ai limiti statici dichiarati
	// al momento del "Dim").
	void DeclareArray(const std::string& name, const std::vector<int>& dims);
	bool IsArray(const std::string& name) const;

	// "indices" DEVE avere la stessa lunghezza delle dimensioni passate
	// a DeclareArray, un valore per asse. NULL/falso se "name" non e'
	// un array dichiarato, il numero di indici non combacia, o un
	// indice e' fuori dai limiti di quell'asse -- MAI un accesso fuori
	// dai limiti del vettore sottostante, qualunque sia "indices".
	const Value* GetArrayElement(const std::string& name, const std::vector<int>& indices) const;
	bool SetArrayElement(const std::string& name, const std::vector<int>& indices, const Value& value);

	// Fase 6d: il registro dei Type del MODULO a cui appartiene questa
	// esecuzione -- stesso principio di fModuleTokens (puntatore in
	// prestito, costruito una volta da ExecuteVbaMacro scandendo il
	// modulo per intero, propagato cosi' com'e' a ogni VbaEnvironment
	// creato per una chiamata a Sub/Function sorella). NULL se
	// l'esecuzione non e' legata a un modulo vero (es. nei test che
	// costruiscono una Sub a mano): in quel caso "Dim x As NomeTipo"
	// tratta sempre "NomeTipo" come uno scalare incorporato, non
	// essendoci alcun registro da consultare.
	void SetTypeRegistry(const std::map<std::string, VbaTypeDecl>* registry) { fTypeRegistry = registry; }
	const std::map<std::string, VbaTypeDecl>* GetTypeRegistry() const { return fTypeRegistry; }

	// Una ISTANZA di un Type, dichiarata da "Dim nome As NomeTipo"
	// (VbaExecutor.cpp costruisce il valore vero via
	// BuildVbaStructInstance usando il registro sopra, poi la passa
	// qui) -- mappa SEPARATA sia da fVars sia da fArrays, stesso
	// principio. Un secondo "Dim" sullo stesso nome sovrascrive
	// l'istanza con una nuova, azzerata (nessuna preservazione dei
	// valori precedenti, stesso comportamento di DeclareArray).
	void DeclareStruct(const std::string& name, VbaStructInstance&& instance);
	bool IsStruct(const std::string& name) const;

	// NULL se "name" non e' stato dichiarato con "Dim ... As
	// NomeTipo". La versione non-const serve a VbaExecutor.cpp per
	// scrivere dentro un campo (es. "ThisVM.PC = 5"): VbaStructField
	// stesso non e' un Value, quindi non c'e' un "SetStructField"
	// separato da scrivere/mantenere in sincronia -- il chiamante
	// prende il puntatore al campo giusto e lo modifica direttamente.
	VbaStructInstance* GetStruct(const std::string& name);
	const VbaStructInstance* GetStruct(const std::string& name) const;

private:
	static std::string NormalizeName(const std::string& name);

	struct VbaArrayStorage {
		std::vector<int> dims;
		std::vector<Value> elements;
	};

	std::map<std::string, Value> fVars; // chiave: nome normalizzato (minuscolo)
	std::map<std::string, VbaArrayStorage> fArrays; // stessa normalizzazione, mappa separata
	std::map<std::string, VbaStructInstance> fStructs; // stessa normalizzazione, mappa separata
	CContainer* fActiveDoc;
	const std::vector<VbaToken>* fModuleTokens;
	int fCallDepth;
	const std::map<std::string, VbaTypeDecl>* fTypeRegistry;
};

#endif
