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
#include <string>

#include "Value.h"

class CContainer;

class VbaEnvironment {
public:
	VbaEnvironment() : fActiveDoc(NULL) {}

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

private:
	static std::string NormalizeName(const std::string& name);

	std::map<std::string, Value> fVars; // chiave: nome normalizzato (minuscolo)
	CContainer* fActiveDoc;
};

#endif
