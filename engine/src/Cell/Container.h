/*
	Copyright 1996, 1997, 1998, 2000
	        Hekkelman Programmatuur B.V.  All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.
	3. All advertising materials mentioning features or use of this software
	   must display the following acknowledgement:
	   
	    This product includes software developed by Hekkelman Programmatuur B.V.
	
	4. The name of Hekkelman Programmatuur B.V. may not be used to endorse or
	   promote products derived from this software without specific prior
	   written permission.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
	AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
	OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/
/*
	Container.h

	Copyright 1997, Hekkelman Programmatuur
	Portions Copyright 2026 Andrea Bernardi

	Part of Sum-It for the BeBox version 1.1.

*/

#ifndef CONTAINER_H
#define CONTAINER_H

/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.pch++"
 *** TPV (2000-Feb-06) Added Headers Guards
 ***/

#include <map>
#include <string>
#include <vector>

#include <GraphicsDefs.h>

#ifndef   CELL_H
#include "Cell.h"
#endif

#ifndef   RANGE_H
#include "Range.h"
#endif

#ifndef   RUNARRAY2_H
#include "RunArray2.h"
#endif

#ifndef   CELLDATA_H
#include "CellData.h"
#endif

#include <Locker.h>

class CCellView;
class CCellIterator;
class CContainer;
class CFormula;
class CNameTable;
class CCalculateJob;
struct CellStyle;
class CSet;
class CFormula;

enum SplitType {
	noSplit, hSplit, vSplit
};

typedef std::map<cell,CellData> cellmap;
//typedef btree<cell,CellData> cellmap;

// Risolve un riferimento incrociato fra fogli di una stessa cartella
// di lavoro ("NomeFoglio!Cella", Fase 9) -- implementata dalla UI
// (MainWindow), non dal motore: l'engine isolato non sa nulla di
// "cartella di lavoro", solo di singoli CContainer. Stesso principio
// gia' esistente per CFormula::ResolveName (nomi di intervallo) ma
// separato da esso: qui la chiave e' il foglio, non un nome di
// intervallo dentro un foglio. Vedi il commento storico "R4Hack" in
// Formula.h/.cpp -- Sum-It aveva gia' previsto un meccanismo quasi
// identico (XRef con un fileNr per riferimenti fra file separati),
// mai completato: qui e' lo stesso principio, riusato per fogli della
// stessa cartella invece che file esterni.
//
// Risoluzione per NOME (non per indice numerico), sempre in fase di
// CALCOLO, mai in fase di analisi (CParser): un nome incorporato nel
// bytecode resta valido anche se, al momento in cui la formula viene
// compilata, il foglio referenziato non esiste ancora o non e' ancora
// collegato a nessun resolver -- esattamente il caso di un file
// caricato da disco, dove ogni foglio viene analizzato singolarmente
// (LoadASCD) prima che tutti i fogli della cartella di lavoro esistano
// e siano collegati fra loro (bug reale scoperto scrivendo
// ui/tests/test_xsheet_formulas.cpp: con una risoluzione per indice
// decisa dal parser, un riferimento incrociato appena ricaricato da
// disco falliva sempre, perche' nessun resolver era ancora collegato
// nel momento in cui quella singola cella veniva ri-analizzata).
class ISheetResolver {
public:
	virtual ~ISheetResolver() {}

	// NULL se nessun foglio ha (ancora, o piu') questo nome esatto
	// (case-sensitive) -- mai un errore fatale, solo un riferimento
	// che resta non risolto (vedi CFormula::Calculate, valXRef).
	virtual CContainer* ResolveSheetByName(const char* inName) = 0;

	// Riferimenti a tabella strutturata di Excel ("Tabella12[Colonna]",
	// Fase 14) fra fogli diversi (Fase 15): a differenza di
	// ResolveSheetByName sopra (che cerca per NOME del foglio, la
	// sintassi esplicita "Foglio!Cella"), un riferimento a tabella non
	// nomina mai il foglio -- in Excel come qui il nome della tabella
	// e' unico in tutta la cartella di lavoro, quindi la ricerca deve
	// scorrere i fogli finche' non trova quello che l'ha registrata
	// (CContainer::AddTable, chiamato una sola volta dal translator XLSX
	// sul foglio che possiede davvero i dati). NULL se nessun foglio
	// (ancora, o piu') ha una tabella con questo nome esatto -- stesso
	// principio di ResolveSheetByName, mai un errore fatale.
	virtual CContainer* FindSheetWithTable(const std::string& tableName) = 0;
};

// Formattazione condizionale VIVA (Fase 13): a differenza
// dell'importazione XLSX di Fase 12 (che valutava le regole una
// tantum e scriveva il colore risultante come CellStyle::fLowColor
// statico, congelato per sempre), qui la regola stessa vive nel
// documento e viene rivalutata a ogni ridisegno contro i valori
// CORRENTI delle celle (vedi CContainer::EvaluateConditionalFormatting
// e SheetView::Draw) -- se il valore di una cella cambia, il colore
// si aggiorna da solo, senza toccare mai CellStyle. Tre tipi di
// regola: eCondCellIsEqual (confronto testuale con un letterale) ed
// eCondDuplicateValues (valore che compare piu' di una volta nello
// stesso intervallo) erano gia' gli unici due davvero comuni in un
// file reale fra i tipi "un confronto indipendente per cella";
// eCondColorScale (Fase 33, vedi sotto) e' il primo dei tipi
// "relativi all'intervallo" (colorScale/dataBar/iconSet in XLSX),
// dove il risultato di ogni cella dipende dalle ALTRE celle
// dell'intervallo, non solo dal proprio valore. Solo il colore di
// SFONDO (non anche il colore del testo, a differenza del dxf XLSX
// che puo' portare entrambi): scelta di scope, lo sfondo e' di gran
// lunga l'uso piu' comune ("evidenzia i duplicati", "evidenzia se >
// soglia").
enum CondFormatRuleType {
	eCondCellIsEqual,
	eCondDuplicateValues,
	// Scala di colori (Fase 33, "Path to full Excel parity" Tier 1):
	// a differenza dei due tipi sopra (un confronto indipendente per
	// cella), qui il colore di OGNI cella dipende dal minimo/massimo
	// (o percentile) di TUTTE le celle numeriche dell'intervallo --
	// vedi CContainer::EvaluateConditionalFormatting per il calcolo
	// vero e ColorScalePoint sotto per come sono descritte le soglie.
	eCondColorScale
};

// Un punto di controllo di una scala di colori: stesso vocabolario di
// <cfvo type="..."> in XLSX (min/max/percentile/percent/num), cosi'
// l'importazione (Fase 33) non deve tradurre nulla in un formato
// diverso. "min"/"max" ignorano cfvoValue (si risolvono sempre al
// minimo/massimo REALE trovato fra le celle numeriche dell'intervallo
// al momento della valutazione, mai un valore fisso) -- "percentile"/
// "percent"/"num" lo usano per calcolare la soglia vera (vedi
// ResolveColorScaleThreshold in Container.styles.cpp).
struct ColorScalePoint {
	std::string cfvoType; // "min", "max", "percentile", "percent", "num"
	double cfvoValue;     // usato solo per percentile/percent/num
	rgb_color color;

	ColorScalePoint() : cfvoType("min"), cfvoValue(0)
	{
		color.red = color.green = color.blue = color.alpha = 255;
	}
};

struct ConditionalFormatRule {
	CondFormatRuleType type;
	std::string compareValue; // solo per eCondCellIsEqual
	rgb_color bgColor; // per eCondCellIsEqual/eCondDuplicateValues; ignorato per eCondColorScale
	std::vector<range> ranges;
	// Solo per eCondColorScale: 2 punti (scala a due colori) o 3
	// (scala a tre colori, il caso piu' comune in Excel) -- vuoto per
	// gli altri due tipi.
	std::vector<ColorScalePoint> colorScalePoints;

	ConditionalFormatRule() : type(eCondCellIsEqual)
	{
		bgColor.red = bgColor.green = bgColor.blue = bgColor.alpha = 255;
	}
};

// Validazione dati (Fase 13): eListValidation limita alla lista di
// valori in "list" (separati da virgola, es. "Rosso,Verde,Blu"),
// eNumberRangeValidation limita a [min, max] -- stesso schema minimo
// dei due tipi piu' comuni del vero "Convalida dati" di Excel
// (l'elenco a discesa e l'intervallo numerico), non l'intera gamma
// (data, ora, lunghezza testo, formula personalizzata).
enum ValidationType {
	eNoValidation,
	eListValidation,
	eNumberRangeValidation
};

struct ValidationRule {
	ValidationType type;
	std::string list;	// eListValidation
	double min, max;	// eNumberRangeValidation

	ValidationRule() : type(eNoValidation), min(0), max(0) {}
};

// Tabella strutturata di Excel (Fase 14, "Tabella12[Codice]"): popolata
// SOLO dall'importazione XLSX (vedi Excel.cpp), mai dalla UI (nessuna
// finestra per creare/modificare tabelle esiste ancora). dataRange
// esclude sempre la riga di intestazione (dove Excel scrive i nomi di
// colonna) -- un riferimento a colonna come "Tabella12[Codice]" indica
// solo i dati, mai l'intestazione stessa. columnNames e' nello stesso
// ordine delle colonne di dataRange da sinistra a destra (indice 0 =
// dataRange.left).
struct CTableDef {
	range dataRange;
	std::vector<std::string> columnNames;
};

class CContainer : public BLocker {
	friend class CCellIterator;
	friend class CCalculateJob;

	virtual ~CContainer();

public:
	CContainer(CCellView *inPane = NULL, CNameTable *inNames = NULL);

	void Reference();
	void Release();

	virtual bool Lock();
	virtual void Unlock();
	
	bool WriteLock();
	void WriteUnlock();

	long GetCellCount()			{ return fCellData.size(); }
	void GetBounds(range& r);

/* cell aanmaak en verwijder routines */

	void NewCell(const cell& inLocation, const Value& inValue, void *inFormula);
	void DisposeCell(const cell& c);
	// Come DisposeCell, ma per il tasto Canc/Backspace (Fase 18):
	// svuota solo valore/formula, mStyle intatto -- l'utente vuole
	// poter cancellare il contenuto di una cella colorata/bordata senza
	// perdere la formattazione, che DisposeCell invece cancella insieme
	// (erase() dalla mappa, tutta la CellData compresa). Nessun effetto
	// su una cella gia' vuota o mai esistita (non c'e' nulla da
	// svuotare, e non ha senso creare una entry solo per questo).
	void ClearCellContent(const cell& c);
	void CopyCell(CContainer *destContainer, const cell& srcLoc, const cell& destLoc,
			range *inFrom = NULL, bool isDragMove = false);
	void MoveCell(CContainer *destContainer, const cell& srcLoc, const cell& destLoc,
			SplitType split = noSplit, int first = 0, int count = 0);
	void ExchangeCells(const cell& a, const cell& b);

/* Celllijst manipulaties */

	bool GetNextCellInRow(cell&, bool mayBeEmpty = false);
	bool GetPreviousCellInRow(cell&, bool mayBeEmpty = false);
	bool GetNextCell(cell&, bool mayBeEmpty = false);
	bool GetPreviousCell(cell&, bool mayBeEmpty = false);

/* cell inhoud manipulaties */

	bool CalcCell(const cell&);
	int GetCellResult(const cell&, char *, size_t bufSize, bool);
	void GetCellFormula(const cell&, char *, size_t bufSize, bool rcStyle = false);
	void* GetCellFormula(const cell&);
	void SetCellFormula(const cell& inLoc, void *inFormula);

	// Formula auditing views (Path to full Excel parity, Tier 2):
	// precedenti (celle che la formula di "c" referenzia direttamente,
	// via CFormulaIterator -- lo stesso meccanismo che CCalcStack usa
	// gia' internamente per il ricalcolo) e dipendenti (celle la cui
	// formula referenzia "c", trovate scandendo tutte le celle del
	// foglio con NextExisting e chiamando GetPrecedents su ognuna --
	// nessun indice inverso mantenuto, ricalcolato ogni volta che serve,
	// stesso principio "niente grafo delle dipendenze" gia' documentato
	// in ROADMAP.md per il ricalcolo). Entrambe solo sullo stesso
	// foglio: CFormulaIterator salta di proposito i riferimenti tra
	// fogli (valXRef/valXRange), limite noto, non un bug qui.
	void GetPrecedents(const cell& c, std::vector<cell>& out);
	void GetDependents(const cell& c, std::vector<cell>& out);
	
	void GetCellStyle(const cell&, CellStyle&);
	void SetCellStyle(const cell&, CellStyle&);
	int GetCellStyleNr(const cell&);
	void SetCellStyleNr(const cell&, int);

	void GetColumnStyle(int, CellStyle&);
	void SetColumnStyle(int, CellStyle&);
	int GetColumnStyleNr(int);
	void SetColumnStyleNr(int, int);
	
	void GetDefaultCellStyle(CellStyle&);
	void SetDefaultCellStyle(CellStyle&);
	bool GetValue(const cell&, Value&);
	void SetValue(const cell&, const Value&);
	int GetType(const cell&);
	double GetDouble(const cell&);
	int CompareValue(const cell&, const cell&);
	int GetCellDepth(const cell&);
	void SetCellDepth(const cell&, int);
	bool CellIsConstant(const cell&);
	bool CellHasFormula(const cell&);
	bool RefersToNamedRange(const char *inName);
	void CollectFunctionNrs(CSet& funcs);
	int CollectFontList(int *fontList);
	int CollectFormats(int *formatList);
	int CollectStyles(int *styleList);

/* accessors */
	CCellView* GetOwner() const			{ return fInView; };
	CRunArray2& GetColumnStyles()		{ return fColumnStyles; }
	cell CalculatingCell() const		{ return fCalculatingCell; }
	CNameTable *GetNameTable() const	{ return fNames; }

	// Fase 7: crea la tabella dei nomi (vuota) se questo documento non
	// ne ha ancora una -- GetNameTable() da sola resta NULL finche'
	// nessuno definisce il primo intervallo con nome (nessun documento
	// nuovo/appena aperto ne alloca una a priori). Chiamata dalla UI
	// (MainWindow) prima di aggiungere/modificare una voce nella
	// mappa che GetNameTable() restituisce -- CNameTable eredita
	// pubblicamente da std::map<CName,range>, quindi non serve
	// nessun metodo dedicato per leggere/scrivere le singole voci.
	// fNewNames=true segna che e' CContainer stesso a possederla (la
	// libera nel proprio distruttore), a differenza di una passata
	// esplicitamente al costruttore.
	CNameTable *GetOrCreateNameTable();

	// outOwner (Fase 15, riferimenti a tabella fra fogli): se non NULL,
	// riceve il CContainer che possiede DAVVERO il nome risolto --
	// "this" per un nome/intervallo/tabella locale (il caso comune),
	// un foglio diverso SOLO per una tabella trovata tramite
	// ISheetResolver::FindSheetWithTable (mai per un nome/intervallo
	// normale, che restano sempre locali al foglio, vedi il commento
	// su fNames sotto). Chi chiama deve leggere le celle del range
	// restituito da *outOwner, non da "this", se sono diversi.
	// inLocation (Fase 16): la cella che contiene la formula --
	// serve SOLO per "Tabella[#Col]" (codifica di "Tabella[[#This
	// Row],[Col]]", vedi CParser::ParseTableReference), che risolve a
	// una singola cella sulla riga della formula stessa, non
	// all'intera colonna della tabella. Il valore di default (cell(),
	// riga/colonna 0) va bene per ogni altro chiamante che non passa
	// mai un nome con quel formato (es. MainWindow::HandleGoToRequest,
	// mai dentro una formula).
	range ResolveName(const char *name, cell inLocation = cell(), CContainer **outOwner = NULL);

	long CountCells(range *inRange = NULL);
	bool Exists(const cell& c);

	// Fase 9: chi possiede questo foglio (MainWindow) collega qui il
	// proprio ISheetResolver quando fa parte di una cartella di
	// lavoro multi-foglio -- NULL per un documento a un solo foglio
	// (mai collegato, o non ancora). Puntatore preso in prestito, MAI
	// posseduto/cancellato da CContainer: la cartella di lavoro vive
	// piu' a lungo di ogni singolo foglio aperto/chiuso al suo
	// interno, e piu' fogli condividono lo stesso resolver.
	void SetSheetResolver(ISheetResolver *inResolver)	{ fSheetResolver = inResolver; }
	ISheetResolver *GetSheetResolver() const			{ return fSheetResolver; }

	// Celle unite (Fase 12): un elenco di rettangoli per foglio, non un
	// campo per-cella -- una cella unita e' un'unica entita' logica che
	// occupa piu' coordinate, non N celle indipendenti con uno stile
	// condiviso (a differenza di colori/font/bordi/ecc, gia' tutti
	// per-cella). Nessuna infrastruttura preesistente (verificato anche
	// nel Sum-It storico), ne' nel motore ne' nella UI.
	void AddMergedRange(const range& r)	{ fMergedRanges.push_back(r); }
	void ClearMergedRanges()				{ fMergedRanges.clear(); }
	const std::vector<range>& GetMergedRanges() const	{ return fMergedRanges; }

	// Formule array/spill range (Fase 29, backlog v3.0 "Consolidation"
	// -- vedi SEQUENCEFunction in Functions.spreadsheet.cpp per l'unico
	// caso d'uso oggi): una formula come "=SEQUENCE(3,1)" scrive il suo
	// risultato in PIU' celle vicine vuote, non solo nella propria.
	// Bookkeeping SOLO in memoria (mai scritto su ASCD): non serve --
	// riaprire un file gia' ri-esegue ogni formula (compresa quella
	// dell'owner) al primo ricalcolo completo dopo l'apertura, che
	// ricostruisce questi elenchi da zero chiamando ApplySpill di
	// nuovo. fSpillOwnerOf include anche l'owner stesso (owner -> se
	// stesso), cosi' IsSpillMember/GetSpillOwner restano uniformi senza
	// un caso speciale per la cella con la formula vera.
	//
	// ApplySpill sostituisce SEMPRE l'intero contenuto spill
	// precedente di "owner" (ClearSpill interno) prima di scrivere
	// quello nuovo -- necessario perche' l'array puo' cambiare forma a
	// ogni ricalcolo (es. SEQUENCE(3,1) diventato SEQUENCE(2,1) dopo
	// una modifica: la terza cella, non piu' parte dell'array, va
	// ripulita, non lasciata con un valore ormai orfano).
	//
	// Collisione = la cella bersaglio ha una FORMULA PROPRIA (non
	// semplicemente "un valore qualunque"): una cella con solo un
	// valore letterale viene sovrascritta senza pieta'. Scelta
	// deliberata (Excel vero blocca su QUALUNQUE contenuto preesistente,
	// non solo su un'altra formula), che rende il ricalcolo dopo
	// l'apertura di un file idempotente SENZA bisogno di persistere
	// fSpillOwnerOf/fSpillRangeOf: le celle spill di un file appena
	// aperto sono gia' valori letterali (mai formule), quindi il primo
	// ApplySpill dopo l'apertura le sovrascrive silenziosamente con lo
	// stesso risultato (nessun cambiamento visibile), invece di
	// rifiutarsi credendole un conflitto reale. Vero limite, non un
	// bug nascosto: mai distrugge una formula altrui, ma NON riproduce
	// il blocco di Excel su un valore digitato a mano nell'area di
	// spill (verrebbe silenziosamente sovrascritto).
	//
	// "values" e' denso, ordine per righe (values[row*cols+col]),
	// values.size() deve essere rows*cols. Restituisce false (nessuna
	// scrittura, "owner" tiene il valore scalare gia' calcolato da
	// Calculate) se una cella bersaglio (diversa da "owner" stesso) ha
	// una formula propria.
	bool ApplySpill(const cell& owner, int rows, int cols, const std::vector<Value>& values);
	// Da chiamare quando una cella smette di essere una formula spill
	// (es. l'utente sovrascrive "owner" con un valore normale): pulisce
	// le celle che erano parte del suo array, altrimenti resterebbero
	// con un valore ormai orfano, indistinguibile da un valore digitato
	// a mano dall'utente.
	void ClearSpill(const cell& owner);
	bool IsSpillMember(const cell& c) const
		{ return fSpillOwnerOf.find(c) != fSpillOwnerOf.end(); }
	// "c" stesso se non fa parte di nessuno spill -- stessa convenzione
	// di comodo di GetMergedRange, che pero' restituisce l'intervallo
	// invece della sola cella proprietaria.
	cell GetSpillOwner(const cell& c) const;
	// Intervallo pieno dello spill di "owner" (owner incluso, angolo in
	// alto a sinistra) -- range() vuoto (IsValid()==false) se "owner"
	// non e' proprietario di nessuno spill attivo. Usato da SheetView
	// per disegnare un contorno intorno all'intera area, stesso
	// principio del contorno di selezione.
	range GetSpillRange(const cell& owner) const;

	// true se "c" fa parte di un intervallo unito (compresa la cella in
	// alto a sinistra, che lo "possiede" agli effetti del contenuto
	// disegnato/modificabile) -- "outRange" riceve l'intervallo intero,
	// utile sia per disegnare (SheetView) sia per rimappare un clic
	// sulla cella in alto a sinistra.
	bool GetMergedRange(cell c, range* outRange) const;

	// Commenti/note per cella (Fase 13): std::map sparso come
	// fMergedRanges sopra, non un campo diretto in CellData -- quella
	// struct porta gia' un'unione+puntatori per OGNI cella, anche le
	// migliaia che non avranno mai un commento, stesso ragionamento
	// gia' scritto per le celle unite. std::string, non BString:
	// Cell.h/Range.h/CellData.h non hanno mai avuto una dipendenza dal
	// Support Kit finora, restare cosi' anche qui. Un testo vuoto
	// rimuove il commento invece di lasciarne uno vuoto in giro (un
	// commento vuoto e "nessun commento" sono la stessa cosa agli
	// effetti pratici).
	void SetComment(cell c, const std::string& text)
	{
		if (text.empty())
			fComments.erase(c);
		else
			fComments[c] = text;
	}
	std::string GetComment(cell c) const
	{
		std::map<cell, std::string>::const_iterator it = fComments.find(c);
		return (it != fComments.end()) ? it->second : std::string();
	}
	bool HasComment(cell c) const { return fComments.find(c) != fComments.end(); }
	const std::map<cell, std::string>& GetComments() const { return fComments; }

	// Collegamenti ipertestuali (Fase 13): stesso schema esatto dei
	// commenti sopra -- un URL vuoto rimuove il collegamento invece di
	// lasciarne uno vuoto in giro.
	void SetHyperlink(cell c, const std::string& url)
	{
		if (url.empty())
			fHyperlinks.erase(c);
		else
			fHyperlinks[c] = url;
	}
	std::string GetHyperlink(cell c) const
	{
		std::map<cell, std::string>::const_iterator it = fHyperlinks.find(c);
		return (it != fHyperlinks.end()) ? it->second : std::string();
	}
	bool HasHyperlink(cell c) const { return fHyperlinks.find(c) != fHyperlinks.end(); }
	const std::map<cell, std::string>& GetHyperlinks() const { return fHyperlinks; }

	// Validazione dati (Fase 13): stesso schema sparso di commenti/
	// collegamenti sopra -- eNoValidation equivale a "nessuna regola",
	// rimuove la voce invece di lasciarne una vuota in giro.
	void SetValidation(cell c, const ValidationRule& rule)
	{
		if (rule.type == eNoValidation)
			fValidations.erase(c);
		else
			fValidations[c] = rule;
	}
	ValidationRule GetValidation(cell c) const
	{
		std::map<cell, ValidationRule>::const_iterator it = fValidations.find(c);
		return (it != fValidations.end()) ? it->second : ValidationRule();
	}
	bool HasValidation(cell c) const { return fValidations.find(c) != fValidations.end(); }
	const std::map<cell, ValidationRule>& GetValidations() const { return fValidations; }

	// Formattazione condizionale viva (Fase 13): un vettore, non una
	// mappa per cella come commenti/collegamenti/convalida sopra --
	// una regola si applica a uno o piu' INTERVALLI, non a una singola
	// cella (stesso schema di ChartObject::dataRange).
	void AddConditionalFormatRule(const ConditionalFormatRule& rule)
		{ fCondFormatRules.push_back(rule); }
	void ClearConditionalFormatRules() { fCondFormatRules.clear(); }
	const std::vector<ConditionalFormatRule>& GetConditionalFormatRules() const
		{ return fCondFormatRules; }
	// Fase 15 (Annulla/Ripristina per la formattazione condizionale,
	// bug reale: nessuno dei comandi che applicano/rimuovono regole
	// chiamava mai SaveUndoState): ripristina l'intero elenco in un
	// solo colpo, l'unico modo sensato di "annullare" una mutazione
	// che non riguarda un intervallo di celle ma l'intero documento.
	void SetConditionalFormatRules(const std::vector<ConditionalFormatRule>& rules)
		{ fCondFormatRules = rules; }

	// Rivaluta OGNI regola contro i valori CORRENTI delle celle e
	// restituisce il colore di sfondo risultante per ogni cella
	// coinvolta -- MAI memorizzato, chiamata di nuovo a ogni ridisegno
	// (vedi SheetView::Draw). Non const: GetCellResult sotto non lo e'.
	std::map<cell, rgb_color> EvaluateConditionalFormatting();

	// Tabelle strutturate di Excel (Fase 14): vedi CTableDef sopra --
	// stesso schema sparso di fComments/fHyperlinks/fValidations, ma
	// indicizzato per nome invece che per cella (un nome di tabella e'
	// unico nel foglio che la contiene, come un nome di intervallo).
	// ResolveName() sotto la interroga da sola quando un valName
	// contiene "[" -- non serve nessun metodo pubblico dedicato a
	// "risolvi un riferimento a tabella", solo a registrarle
	// (AddTable, chiamato dall'importazione XLSX) e a elencarle
	// (GetTables, usato dalla UI per un eventuale elenco/autocompletamento
	// futuro, non ancora collegato a nulla).
	void AddTable(const std::string& name, const CTableDef& def)
		{ fTables[name] = def; }
	const std::map<std::string, CTableDef>& GetTables() const { return fTables; }

private:
	void Visit(const cell&, void*);
	bool GetCellData(const cell&, CellData&);
	
	long DoCalculate();
	static long Calculate(void *inData);
	
/* en de fields */
	BLocker fWriteLocker;
	int32 fReferenceCount;
	cellmap fCellData;
	CCellView *fInView;
	CNameTable *fNames;
	bool fNewNames;
	int fDefaultCellStyle;
	CRunArray2 fColumnStyles;
	cell fCalculatingCell;
	ISheetResolver *fSheetResolver;
	std::vector<range> fMergedRanges;
	std::map<cell, std::string> fComments;
	std::map<cell, std::string> fHyperlinks;
	std::map<cell, ValidationRule> fValidations;
	std::vector<ConditionalFormatRule> fCondFormatRules;
	std::map<std::string, CTableDef> fTables;
	// Vedi ApplySpill/ClearSpill sopra: fSpillOwnerOf include anche
	// owner->owner (owner e' "membro" del proprio spill), fSpillRangeOf
	// e' indicizzato SOLO dalla cella owner.
	std::map<cell, cell> fSpillOwnerOf;
	std::map<cell, range> fSpillRangeOf;
};

inline bool CContainer::WriteLock()
{
	return fWriteLocker.Lock();
}

inline void CContainer::WriteUnlock()
{
	fWriteLocker.Unlock();
}

class StWriteLock {
public:
	StWriteLock(CContainer *container)
		{ fContainer = container; container->WriteLock(); };
	~StWriteLock()
		{ fContainer->WriteUnlock(); };
private:
	CContainer *fContainer;
};

#endif
