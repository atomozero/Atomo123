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

	Un modulo puo' contenere PIU' Sub/Function una dopo l'altra (e
	contenuto a livello di modulo che questo sottoinsieme non
	riconosce, es. "Attribute VB_Name = ...", "Option Explicit"): si
	scandisce il flusso di token un "Sub ... End Sub"/"Function ... End
	Function" alla volta (ogni blocco si analizza per intero con
	ParseVbaSub, anche quando non e' quello cercato, cosi' il suo
	annidamento di If/For/ElseIf viene sempre saltato correttamente
	invece di un secondo scanner ad hoc che rischierebbe di perdersi in
	un blocco annidato), e si confronta il nome con "macroName" senza
	distinguere maiuscole/minuscole.

	Resilienza della scansione: se una Sub/Function DIVERSA da quella
	cercata, nello stesso modulo, usa una sintassi che questo
	sottoinsieme non analizza ("Open ... For Binary", "ByRef" su un
	parametro...), SOLO quel singolo blocco si salta (cercando "End
	Sub"/"End Function" -- mai prodotta da un If/For/Do annidato,
	stesso trucco di ListVbaMacroNames sotto), non l'intera scansione
	del modulo: una Sub cercata DOPO quella rotta resta comunque
	raggiungibile. Trovato indagando un vero vbaProject.bin reale
	(github.com/rndtrash/chip-8-vba) dove una Sub con sintassi non
	supportata PRECEDEVA diverse altre altrimenti perfettamente valide
	-- con la vecchia regola ("l'intera scansione si ferma"), nessuna
	di quelle sarebbe mai stata raggiungibile, anche eseguendole per
	nome individualmente.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_MACRO_RUNNER_H
#define VBA_MACRO_RUNNER_H

#include <map>
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
//
// "persistentModuleEnv" (facoltativo, NULL di default -- il
// comportamento di sempre, un VbaEnvironment nuovo a ogni chiamata)
// e' lo stato di modulo PERSISTENTE (trovato controllando il
// sorgente reale del CHIP-8, "Dim ThisVM As VM" a livello di modulo
// -- vedi ROADMAP.md): quando non-NULL, questa chiamata lo RIUSA cosi'
// com'e' invece di crearne uno nuovo -- le sue variabili gia' presenti
// (da una chiamata PRECEDENTE a qualunque macro di questo stesso
// modulo) restano quelle che sono, e le variabili dichiarate a
// livello di modulo (FindAllVbaModuleLevelDims) si inizializzano
// SOLO la prima volta che mancano, mai azzerate di nuovo in seguito.
// Il chiamante possiede "persistentModuleEnv" e decide quando
// scartarlo (es. alla chiusura del file, un nuovo "progetto VBA" nel
// vero VBA) -- questa funzione non lo fa mai da sola.
bool ExecuteVbaMacro(const uint8* vbaProjectData, size_t vbaProjectLen,
	const std::string& macroName, CContainer* doc, VbaRuntimeError* outError,
	VbaEnvironment* persistentModuleEnv = NULL);

// Fase 7 (controlli ActiveX incorporati): la prima meta' esatta della
// scansione multi-modulo di ExecuteVbaMacro sopra (FindVbaSubByName su
// ogni modulo finche' uno non la trova), ma SENZA mai eseguirla --
// serve a MainWindow::RunControlEvent per decidere in anticipo se
// "<nome>_MouseUp" esiste prima di ricadere su "<nome>_Click" (vedi
// SheetView::MouseUp), senza il costo/gli effetti collaterali di una
// vera esecuzione solo per scoprire che la Sub non c'e'. Volutamente
// NON un refactor di ExecuteVbaMacro (che resta cosi' com'e', gia'
// testato) -- una piccola duplicazione mirata, piu' sicura di un
// parametro "solo controlla" dentro una funzione che fa ben altro.
// Vero se "macroName" esiste in un qualunque modulo ed e' analizzabile
// per intero (stesso limite noto di FindVbaSubByName); falso altrimenti
// -- compreso un "vbaProjectData" non valido, mai un crash.
bool VbaSubExists(const uint8* vbaProjectData, size_t vbaProjectLen,
	const std::string& macroName);

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
// ("ByRef" su un parametro...): un nome puo' comparire in questo
// elenco ed essere comunque rifiutato da ExecuteVbaMacro quando
// davvero eseguito, se la sua stessa Sub usa una sintassi che questo
// sottoinsieme non analizza -- quell'errore arriva allora dalla vera
// esecuzione, non da qui. Una "Function" (cercata solo per "Sub", mai
// per "Function") e' esclusa a proposito, come il vero elenco Macro
// di Excel che non propone Function nella finestra "Esegui macro"; una
// "Private Sub" (il modificatore e' un semplice identificatore per
// questo lessico, non una parola chiave) e' esclusa per lo stesso
// motivo, come le Sub private del vero Excel. Vero sempre tranne
// quando "vbaProjectData" non e' affatto un progetto VBA valido;
// "outNames" resta vuoto (non un errore) se il progetto non contiene
// nessuna Sub pubblica.
bool ListVbaMacroNames(const uint8* vbaProjectData, size_t vbaProjectLen,
	std::vector<std::string>* outNames);

// Fase 6d: scandisce "tokens" (il sorgente GIA' tokenizzato di UN
// modulo) per TUTTI i blocchi "[Private|Public] Type Nome ... End
// Type" (a differenza di FindVbaSubByName, qui non si cerca UN nome
// -- serve l'intero registro in anticipo, perche' un Type puo'
// referenziarne un altro dichiarato PIU' IN BASSO nello stesso modulo,
// vedi VbaTypeField in VbaAst.h). Il corpo di ogni Sub/Function viene
// SALTATO per intero senza analizzarlo (un Type non puo' comparire al
// suo interno) individuando la coppia di parole chiave consecutive
// "End Sub"/"End Function" -- MAI prodotta da un If/For annidato
// (che userebbe "End If"/"Next"), stesso trucco di ListVbaMacroNames.
// Vero se ogni Type del modulo e' analizzabile per intero, con
// "outTypes" popolato (vuoto, non un errore, se il modulo non ne
// dichiara nessuno); falso con "outError" popolato altrimenti (un
// Type con una sintassi fuori da questo sottoinsieme).
bool FindAllVbaTypeDecls(const std::vector<VbaToken>& tokens,
	std::map<std::string, VbaTypeDecl>* outTypes, VbaParseError* outError);

// Stato di modulo persistente (trovato controllando il sorgente reale
// del CHIP-8, github.com/rndtrash/chip-8-vba: "Dim ThisVM As VM" a
// LIVELLO DI MODULO, condiviso da ogni Sub -- vedi ROADMAP.md):
// scandisce "tokens" per ogni "Dim nome[(limiti)] [As Tipo], ..."
// scritto FUORI da qualunque Sub/Function (che vengono saltati per
// intero senza analizzarli, stesso trucco "End" + parola chiave
// corrispondente di FindAllVbaTypeDecls sopra -- un "Type ... End
// Type" si salta allo stesso modo, gia' raccolto a parte da
// FindAllVbaTypeDecls). "outStmts" riceve un vbaStmtDim per ogni
// dichiaratore trovato, nell'ordine scritto -- eseguirli UNA SOLA
// VOLTA per sessione (non a ogni "Esegui macro") spetta al chiamante
// (ExecuteVbaMacro, quando gli viene passato un "VbaEnvironment"
// esterno da riusare fra chiamate diverse).
//
// A differenza di FindVbaSubByName/FindAllVbaTypeDecls, questa
// funzione NON fallisce MAI: un singolo gruppo "Dim" con una sintassi
// fuori da questo sottoinsieme si salta (un token alla volta, stesso
// trattamento di "Attribute VB_Name = ..."), non abortisce l'intera
// scansione -- una variabile di modulo che non si riesce a
// pre-dichiarare qui produrra' comunque un errore onesto
// "variabile non dichiarata" quando una Sub la usa davvero, non un
// dato inventato.
void FindAllVbaModuleLevelDims(const std::vector<VbaToken>& tokens,
	std::vector<std::unique_ptr<VbaStmt>>* outStmts);

#endif
