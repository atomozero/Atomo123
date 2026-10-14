/*
	VbaMacroRunner.h

	Punto d'ingresso unico, punta a punta, dell'interprete VBA (Fase 3d
	-- l'ultima della Fase 3, vedi la decisione del sottoinsieme in
	ROADMAP.md): dato il blob grezzo di un vero vbaProject.bin e il
	nome di UNA macro, lega insieme tutte le fasi precedenti --
	  - Fase 1 (VbaModuleSource.h): estrae il sorgente reale di ogni
	    modulo del progetto;
	  - Fase 2 (VbaToken.h/VbaStmtParser.h): tokenizza e analizza la Sub
	    richiesta all'interno del sorgente di ciascun modulo;
	  - Fase 3a/b/c (VbaEvaluator.h/VbaExecutor.h): la esegue in un
	    VbaEnvironment nuovo, legato a "doc" come documento attivo per
	    Range/Cells.

	Un modulo puo' contenere PIU' Sub una dopo l'altra (e contenuto a
	livello di modulo che questo sottoinsieme non riconosce, es.
	"Attribute VB_Name = ...", "Option Explicit", una Function --
	fuori dal sottoinsieme deciso in ROADMAP.md): si scandisce il
	flusso di token un "Sub ... End Sub" alla volta (ogni blocco si
	analizza per intero con ParseVbaSub, anche quando non e' quello
	cercato, cosi' il suo annidamento di If/For/ElseIf viene sempre
	saltato correttamente invece di un secondo scanner ad hoc che
	rischierebbe di perdersi in un blocco annidato), e si confronta il
	nome con "macroName" senza distinguere maiuscole/minuscole.

	LIMITE NOTO di questa fase: se una Sub DIVERSA da quella cercata,
	nello stesso modulo e PRIMA di essa, usa una sintassi che questo
	sottoinsieme non analizza (parametri sulla Sub, Select Case,
	Function...), l'intera scansione di QUEL modulo si ferma con un
	errore di sintassi, anche se la Sub cercata stessa sarebbe
	perfettamente valida per questo sottoinsieme -- si passa comunque
	al modulo SUCCESSIVO (non e' un errore fatale per l'intera
	ricerca), ma non si salta oltre il punto di errore all'interno
	dello STESSO modulo. Un limite onesto, non un dato sbagliato
	silenzioso: la Fase 4 (se mai servira' andare oltre questo
	sottoinsieme esplicito) e' il posto giusto per superarlo.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_MACRO_RUNNER_H
#define VBA_MACRO_RUNNER_H

#include <memory>
#include <string>
#include <vector>

#include <SupportDefs.h>

#include "VbaAst.h"
#include "VbaEvaluator.h"
#include "VbaStmtParser.h"
#include "VbaToken.h"

class CContainer;

// Scandisce "tokens" (il sorgente GIA' tokenizzato di UN modulo) un
// "Sub ... End Sub" alla volta finche' non trova una Sub di nome
// "subName" (senza distinguere maiuscole/minuscole) -- vedi il
// commento in cima al file sul perche' ogni blocco si analizza per
// intero anche quando non e' quello cercato. Esposto separatamente
// da ExecuteVbaMacro sotto per poterlo testare direttamente col solo
// sorgente di un modulo, senza costruire un vbaProject.bin -- la
// parte interessante qui e' la scansione multi-Sub, niente a che
// fare con CFB/compressione (gia' testate a parte in Fase 1). Vero
// se "subName" si trova ed e' analizzabile per intero, con "outSub"
// popolato SOLO in quel caso; falso con "outError" popolato
// altrimenti (non trovata in questo modulo, o un errore di sintassi
// -- vedi il limite noto in cima al file).
bool FindVbaSubByName(const std::vector<VbaToken>& tokens, const std::string& subName,
	std::unique_ptr<VbaSub>* outSub, VbaParseError* outError);

// Esegue la macro "macroName" (il nome di una Sub, in uno qualunque
// dei moduli del progetto) legata a "doc" come documento attivo per
// Range/Cells (Fase 3c -- "doc" puo' essere NULL se la macro non usa
// affatto il modello a oggetti, esattamente come VbaEnvironment
// stessa). Vero in caso di successo; falso con "outError" popolato
// altrimenti -- un vbaProject.bin non valido, nessuna Sub di nome
// "macroName" trovata in nessun modulo, o un vero errore di
// esecuzione della Sub stessa (stesse categorie di ExecuteVbaSub).
// Mai un crash, qualunque sia il contenuto di "vbaProjectData" o il
// sorgente dei moduli.
bool ExecuteVbaMacro(const uint8* vbaProjectData, size_t vbaProjectLen,
	const std::string& macroName, CContainer* doc, VbaRuntimeError* outError);

// Elenca ogni Sub "eseguibile" (vedi sotto) di ogni modulo del
// progetto, nell'ordine in cui compaiono -- per un elenco a scelta
// nella UI (Fase 4, "Esegui macro..."), MAI per eseguirle: a
// differenza di FindVbaSubByName/ExecuteVbaMacro sopra, questa
// funzione NON chiama ParseVbaSub su nessun blocco -- cerca solo la
// sequenza lessicale "parola chiave Sub" + "identificatore" (che
// "End Sub" non produce mai, dato che "End"/"Sub" sono due parole
// chiave consecutive, non una parola chiave seguita da un
// identificatore) -- quindi non fallisce mai, nemmeno su un modulo
// che contiene una Sub con una sintassi fuori da questo sottoinsieme
// (parametri, Select Case...): un nome puo' comparire in questo
// elenco ed essere comunque rifiutato da ExecuteVbaMacro quando
// davvero eseguito, se la sua Sub (o una PRECEDENTE nello stesso
// modulo, vedi il limite noto sopra) usa una sintassi che questo
// sottoinsieme non analizza -- quell'errore arriva allora dalla vera
// esecuzione, non da qui. Una "Private Sub" (il modificatore e' un
// semplice identificatore per questo lessico, non una parola chiave)
// e' esclusa, come il vero elenco Macro di Excel che nasconde le
// Sub private. Vero sempre tranne quando "vbaProjectData" non e'
// affatto un progetto VBA valido; "outNames" resta vuoto (non un
// errore) se il progetto non contiene nessuna Sub pubblica.
bool ListVbaMacroNames(const uint8* vbaProjectData, size_t vbaProjectLen,
	std::vector<std::string>* outNames);

#endif
