/*
	AscdIO.h

	Lettura/scrittura del formato nativo provvisorio ASCD ("Atomo
	Sheet Cell Data") usato anche dai translator (vedi
	translators/csv/CsvTranslator.cpp per la definizione originale
	del formato). L'app non e' un add-on Translation Kit, quindi
	questa e' una copia standalone della stessa logica di
	lettura/scrittura -- stesso approccio gia' seguito da ogni
	translator, che duplica WriteASCD() per non introdurre una
	dipendenza di link tra translator e app.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef ASCD_IO_H
#define ASCD_IO_H

#include <utility>
#include <vector>

#include <DataIO.h>
#include <GraphicsDefs.h>
#include <Point.h>
#include <String.h>
#include <SupportDefs.h>

#include "Chart.h"
#include "EmbeddedImage.h"

class CContainer;

// Margini/scala di "Imposta pagina" (Fase 29), PER FOGLIO -- prima
// un'unica preferenza globale dell'app (gPrefs, vedi ROADMAP.md),
// spostata qui su richiesta esplicita dell'utente: ogni foglio ricorda
// i propri margini/scala nel file, invece di condividerli con
// qualunque altro documento aperto. "hasSettings"=false (il default)
// vuol dire "mai impostati per questo foglio": MainWindow ricade sulla
// preferenza globale in quel caso, esattamente come si comportava
// sempre prima di questa fase -- vedi MainWindow::GetActivePrintSettings.
// I valori di default qui (2cm, scala fissa 100%) combaciano con quelli
// gia' usati da gPrefs, cosi' un foglio con hasSettings=true ma mai
// davvero toccato dall'utente (caso raro, solo se costruito a mano) si
// comporta comunque in modo sensato.
struct AscdPrintSettings {
	bool hasSettings = false;
	double marginTopCm = 2.0;
	double marginBottomCm = 2.0;
	double marginLeftCm = 2.0;
	double marginRightCm = 2.0;
	int scaleMode = 0; // 0=percentuale fissa, vedi kPrintFitWidth/Height/Both in PrintLayout.h
	double scalePercent = 100.0;
};

// "charts"/"colWidths" sono opzionali (NULL = non legge/scrive nulla
// di quella sezione, comportamento invariato per chi non ne ha
// bisogno, es. i test di round-trip gia' esistenti) -- entrambe
// sezioni aggiunte in coda al formato: un file ASCD scritto prima di
// una di queste modifiche semplicemente non ce l'ha, e LoadASCD lo
// riconosce distinguendo "fine del file" (sezione assente, non un
// errore) da un file davvero troncato/corrotto -- vedi il commento in
// AscdIO.cpp. I byte di ciascuna sezione vengono comunque sempre
// consumati dallo stream se presenti, anche passando NULL: altrimenti
// una chiamata con NULL lascerebbe la posizione di lettura sbagliata
// per chi legge subito dopo (es. LoadASCDBook, che concatena piu'
// blocchi ASCD sullo stesso flusso).
//
// "colWidths" e' l'elenco (colonna 1-based, larghezza in pixel) delle
// SOLE colonne con una larghezza diversa da quella predefinita
// (SheetView::kColWidth) -- letta da un file importato (vedi
// XlsxTranslator) o da un ridimensionamento fatto a mano
// (SheetView::CustomColumnWidths), non un array denso su tutte le
// kColCount colonne.
//
// I colori di sfondo/testo (CellStyle::fLowColor/fHighColor, per cella
// e per colonna) NON hanno un parametro dedicato come charts/colWidths
// sopra: a differenza della larghezza di colonna (che vive solo in
// SheetView, mai nel motore), il colore vive gia' dentro CContainer
// stesso tramite CContainer::GetCellStyle/SetCellStyle/GetColumnStyle/
// SetColumnStyle (gia' usate per il formato numerico) -- SaveASCD lo
// legge direttamente da "doc" e LoadASCD lo scrive direttamente li',
// esattamente come gia' avviene per il testo/formula di ogni cella.
// "rowHeights"/"frozenRows"/"frozenCols" (Fase 10) seguono lo stesso
// principio opzionale di "charts"/"colWidths" sopra -- sezioni
// aggiunte in coda al formato, un file scritto prima di questa
// modifica semplicemente non le ha. "rowHeights" e' l'elenco delle
// SOLE righe con un'altezza diversa da quella predefinita
// (SheetView::kRowHeight), speculare a "colWidths". "frozenRows"/
// "frozenCols" sono un solo intero ciascuno (quante righe/colonne
// dall'alto/da sinistra sono bloccate, 0 = nessuna -- vedi
// SheetView::SetFreezePanes), non una lista: puntatori anziche'
// riferimenti per lo stesso motivo di charts/colWidths, NULL = non
// legge/scrive nulla di quella sezione.
// "images" (Fase 12) segue lo stesso principio opzionale di "charts"
// sopra: elenco delle immagini incorporate nel foglio (import XLSX,
// vedi EmbeddedImage.h), NULL = non legge/scrive nulla di quella
// sezione, i byte vengono comunque sempre consumati se presenti per lo
// stesso motivo di "charts" (LoadASCDBook concatena piu' blocchi ASCD
// sullo stesso flusso).
// "showGrid" segue lo stesso principio di "frozenRows"/"frozenCols"
// sopra: un solo booleano per foglio (import XLSX da <sheetView
// showGridLines="...">, o un salvataggio precedente), non una
// preferenza globale dell'app -- vedi il commento su AscdSheet::
// showGrid sotto sul perche' e' diventato un attributo per-foglio.
// "hasTabColor"/"tabColor" seguono lo stesso principio: import XLSX da
// <sheetPr><tabColor rgb="..."/></sheetPr>, vedi AscdSheet::tabColor
// sotto. "hasTabColor" separato da un colore "vuoto" sentinella per lo
// stesso motivo di CellStyle: un colore a caso (es. nero) non si
// distingue da "nessun colore scelto" senza un booleano a parte.
status_t LoadASCD(BPositionIO* source, CContainer* doc,
	std::vector<ChartObject>* charts = NULL,
	std::vector<std::pair<int, float> >* colWidths = NULL,
	std::vector<std::pair<int, float> >* rowHeights = NULL,
	int* frozenRows = NULL, int* frozenCols = NULL,
	std::vector<EmbeddedImage>* images = NULL,
	bool* showGrid = NULL,
	bool* hasTabColor = NULL, rgb_color* tabColor = NULL,
	std::vector<int>* hiddenRows = NULL,
	bool* hasAutoFilter = NULL, range* autoFilterRange = NULL,
	bool* hasPrintArea = NULL, range* printArea = NULL,
	AscdPrintSettings* printSettings = NULL,
	// Fase 30: un chiamante che ricalchera' comunque l'intera cartella
	// di lavoro subito dopo (MainWindow::OpenFile, sempre seguito da
	// RecalculateWorkbook una volta collegato ISheetResolver) puo'
	// saltare il ricalcolo interno qui -- vedi il commento sopra
	// RecalculateAll in AscdIO.cpp per il bug reale (bloccato al limite
	// di 50 passate) che questo evita. false di default: ogni chiamante
	// esistente (test compresi, che spesso leggono un valore calcolato
	// SUBITO dopo LoadASCD senza mai chiamare RecalculateWorkbook a
	// parte) continua a ricevere un documento gia' ricalcolato, come
	// sempre.
	bool skipInitialRecalc = false,
	// XLSM (Fase 31): bytes grezzi di "xl/vbaProject.bin", mai analizzati
	// ne' eseguiti -- solo trasportati cosi' come sono cosi' che riaprire
	// e risalvare un file con macro non le distrugga piu' in silenzio
	// (vedi ROADMAP.md, Tier 1). NULL/vettore vuoto = nessun progetto
	// VBA, il comportamento di sempre. Ultimo parametro apposta: aggiunto
	// in coda cosi' ogni chiamata esistente resta valida senza modifiche.
	std::vector<unsigned char>* vbaProject = NULL,
	// Protezione foglio (Fase 32): vedi il commento su AscdSheet::
	// isProtected sotto. Il blocco per-cella (CellStyle::fLocked) non ha
	// un parametro qui: viaggia gia' dentro "doc" come ogni altro
	// attributo di stile (allineamento, bordi, ecc.), stesso principio
	// gia' seguito da quei campi. NULL/false = foglio non protetto, il
	// comportamento di sempre.
	bool* isProtected = NULL,
	// BUG REALE (Fase 32b): un file .ascd/.ascb multi-foglio scritto
	// PRIMA delle sezioni vbaProject/celle sbloccate/protezione (Fase
	// 31/32) smetteva di aprirsi -- non solo perdeva quei dati, l'intero
	// file falliva con B_BAD_DATA. Motivo: dentro una cartella di lavoro
	// concatenata (LoadASCDBook), "fine sezione assente" e "fine del
	// blocco di QUESTO foglio, inizia quello successivo" sono
	// indistinguibili senza un confine esplicito -- un foglio vecchio
	// SENZA queste sezioni fa leggere a LoadASCD i primi byte del foglio
	// SUCCESSIVO come se fossero l'inizio di queste sezioni, corrompendo
	// tutto il resto della lettura. Vedi LoadASCDBook in questo stesso
	// file: la cartella "ASCB" legacy (congelata per sempre a questo
	// elenco di sezioni, mai piu' toccata) passa true qui per SALTARE
	// del tutto queste tre sezioni via ogni chiamata a LoadASCD, esatto
	// comportamento di prima che esistessero. La cartella "ASC2" (nuovo
	// formato, con un confine di lunghezza esplicito per foglio) passa
	// false: li' il confine stesso protegge dallo stesso problema per
	// QUALUNQUE sezione futura, non serve piu' un interruttore dedicato.
	bool skipVbaAndProtectionSections = false);
status_t SaveASCD(CContainer* doc, BPositionIO* dest,
	const std::vector<ChartObject>* charts = NULL,
	const std::vector<std::pair<int, float> >* colWidths = NULL,
	const std::vector<std::pair<int, float> >* rowHeights = NULL,
	const int* frozenRows = NULL, const int* frozenCols = NULL,
	const std::vector<EmbeddedImage>* images = NULL,
	const bool* showGrid = NULL,
	const bool* hasTabColor = NULL, const rgb_color* tabColor = NULL,
	const std::vector<int>* hiddenRows = NULL,
	const bool* hasAutoFilter = NULL, const range* autoFilterRange = NULL,
	const bool* hasPrintArea = NULL, const range* printArea = NULL,
	const AscdPrintSettings* printSettings = NULL,
	// Vedi il commento gemello sopra in LoadASCD.
	const std::vector<unsigned char>* vbaProject = NULL,
	const bool* isProtected = NULL);

// Vero solo se "source" comincia con la firma nativa ASCD (riporta
// la posizione di lettura a dove si trovava prima di controllare).
// MainWindow la usa per leggere un file nativo direttamente con
// LoadASCD invece di farlo passare -- inutilmente e con perdita
// della sezione grafici incorporati -- dal Translation Kit, che per
// un file gia' ASCD lo farebbe comunque rileggere/riscrivere tramite
// la copia duplicata di ReadASCD/WriteASCD di un translator
// qualunque (vedi translators/csv/CsvTranslator.cpp), che non
// conosce quella sezione.
bool IsASCDFile(BPositionIO* source);

// Un foglio di una cartella di lavoro multi-foglio: nome piu'
// documento (creato da LoadASCDBook, di proprieta' del chiamante --
// va rilasciato con Release() quando non serve piu', stesso discorso
// gia' valido per qualunque CContainer). "charts"/"colWidths"
// seguono lo stesso significato di LoadASCD/SaveASCD sopra -- i
// colori invece non compaiono qui: vivono gia' dentro "doc".
struct AscdSheet {
	BString name;
	CContainer* doc;
	std::vector<ChartObject> charts;
	std::vector<std::pair<int, float> > colWidths;
	// Fase 10: vedi il commento sopra LoadASCD/SaveASCD.
	std::vector<std::pair<int, float> > rowHeights;
	int frozenRows = 0;
	int frozenCols = 0;
	// Fase 12: vedi il commento sopra LoadASCD/SaveASCD.
	std::vector<EmbeddedImage> images;
	// Visibilita' della griglia: era una preferenza GLOBALE dell'app
	// (gPrefs, un solo interruttore per tutti i documenti) finche' un
	// file reale confrontato con Excel non ha mostrato il problema --
	// un foglio con "showGridLines=0" nell'originale (un look pulito da
	// documento ufficiale, scelto apposta dall'autore) veniva comunque
	// aperto con la griglia visibile, perche' l'unica preferenza era
	// quella dell'utente di Atomo123, non quella salvata nel file.
	// true di default: qualunque documento scritto prima di questo
	// campo (o creato da capo) mantiene il comportamento di sempre.
	bool showGrid = true;
	// Colore della linguetta del foglio (import XLSX da <sheetPr>
	// <tabColor rgb="..."/></sheetPr>, o scelto a mano in futuro --
	// ancora nessuna UI per farlo, solo lettura/persistenza per ora).
	// false di default: nessun documento scritto prima di questo campo
	// aveva mai un colore, la linguetta resta con lo stile predefinito
	// di SheetTabView.
	bool hasTabColor = false;
	rgb_color tabColor = { 0, 0, 0, 255 };
	// Righe nascoste (AutoFilter o "nascondi" manuale, import XLSX da
	// <row hidden="1">) e intervallo dell'AutoFilter stesso (import
	// XLSX da <autoFilter ref="...">) -- vedi il commento sopra
	// LoadASCD/SaveASCD e SheetView::SetHiddenRows/SetAutoFilter.
	// hasAutoFilter=false di default: nessun documento scritto prima di
	// questi campi aveva mai un filtro.
	std::vector<int> hiddenRows;
	bool hasAutoFilter = false;
	range autoFilterRange;
	// Posizione di scorrimento (Fase 17, richiesta esplicita
	// dell'utente: "se mi sposto su un foglio non vorrei che anche gli
	// altri fogli si spostassero"): un solo SheetView e' condiviso da
	// tutti i fogli della cartella (vedi MainWindow::SwitchToSheet), la
	// sua posizione di scorrimento (Bounds().LeftTop()) rimaneva quindi
	// quella di qualunque foglio fosse attivo per ultimo invece di
	// seguire il foglio -- bug reale, non solo un dettaglio estetico
	// (passare a un altro foglio con una tabella diversa mostrava un
	// punto scorso a caso invece dell'angolo in alto a sinistra o di
	// dove l'utente l'aveva lasciato). Solo per la sessione corrente:
	// non c'e' spazio riservato per questo nel formato ASCD/ASCB e non
	// e' stato richiesto, quindi non viene ne' letto ne' scritto da
	// LoadASCD/SaveASCD -- un file riaperto parte sempre dall'angolo in
	// alto a sinistra di ogni foglio, come sempre.
	BPoint scrollPosition = BPoint(0, 0);
	// Area di stampa (Fase 27, vedi ROADMAP.md "v3.0 Consolidation"): se
	// impostata, MainWindow::PrintDocument stampa SOLO questo intervallo
	// invece di SheetView::ContentRect() (tutte le celle con contenuto).
	// Persistita nel formato ASCD/ASCB (sezione in coda, vedi
	// SaveASCD/LoadASCD sotto) -- a differenza di scrollPosition sopra,
	// che resta deliberatamente solo di sessione, un'area di stampa e'
	// una vera decisione dell'utente su COSA stampare, non un dettaglio
	// di navigazione della UI.
	bool hasPrintArea = false;
	range printArea;
	// Margini/scala di "Imposta pagina" (Fase 29): PER FOGLIO, non piu'
	// una preferenza globale dell'app (gPrefs) come prima -- vedi
	// AscdPrintSettings sotto. hasSettings=false (il default) vuol dire
	// "questo foglio non ha mai avuto un'impostazione propria", nel
	// qual caso MainWindow ricade su gPrefs esattamente come sempre
	// (vedi MainWindow::GetActivePrintSettings) -- un file scritto prima
	// di questo campo, o un foglio nuovo mai passato da "Imposta
	// pagina", si comporta quindi in modo identico a prima.
	AscdPrintSettings printSettings;
	// Progetto VBA (Fase 31), vedi il commento su LoadASCD/SaveASCD
	// sopra: quasi sempre vuoto. Quando un'importazione XLSM lo popola,
	// lo fa SOLO sul primo foglio della cartella (un progetto VBA e' un
	// concetto per l'intera cartella di lavoro, non per foglio, ma
	// questa struct e' per-foglio) -- MainWindow lo cerca scandendo
	// fSheets, non assumendo l'indice 0 in particolare.
	std::vector<unsigned char> vbaProject;
	// Protezione foglio (Fase 32, "Proteggi foglio"): quando true, la
	// UI rifiuta di modificare contenuto/formattazione delle celle con
	// CellStyle::fLocked=true (il default, vedi il costruttore di
	// CellStyle) su QUESTO foglio -- una cella esplicitamente sbloccata
	// resta modificabile anche a foglio protetto, esattamente come in
	// Excel. false di default: nessun documento scritto prima di questo
	// campo era mai protetto.
	bool isProtected = false;
};

// Vero solo se "source" comincia con la firma di una cartella di
// lavoro multi-foglio (riporta la posizione di lettura a dove si
// trovava prima di controllare) -- distingue un file "ASCB" da un
// vecchio ".ascd" a un solo foglio ("ASCD", senza wrapper), che resta
// leggibile con IsASCDFile/LoadASCD come prima.
bool IsASCDBookFile(BPositionIO* source);

// Formato "cartella di lavoro" (piu' fogli): firma "ASCB" piu' il
// numero di fogli, seguiti per ciascuno dal nome e da un blocco ASCD
// completo -- la stessa identica cosa che scrivono/leggono
// SaveASCD/LoadASCD sopra, riusate cosi' come sono (nessuna
// duplicazione del formato per cella) invece di reinventare la
// serializzazione: ogni blocco ASCD e' gia' auto-delimitante tramite
// i propri conteggi interni, quindi concatenarne N in sequenza sullo
// stesso stream e rileggerli con lo stesso numero di chiamate a
// LoadASCD funziona senza bisogno di offset o lunghezze esplicite fra
// un foglio e il successivo.
status_t SaveASCDBook(const std::vector<AscdSheet>& sheets, BPositionIO* dest);
status_t LoadASCDBook(BPositionIO* source, std::vector<AscdSheet>* outSheets,
	bool skipInitialRecalc = false);

// Ricalcola tutte le celle con formula del documento fino a
// convergenza (o a un limite di passate). Usata da LoadASCD dopo aver
// popolato le celle (TryToParseString non calcola, quindi senza
// questo passo le celle con formula restano vuote finche' l'utente
// non le tocca a mano) e, soprattutto, da MainWindow/SheetView dopo
// OGNI modifica di una cella (editing, taglia/incolla/cancella,
// trova e sostituisci) invece del solo CalcCell() sulla cella
// toccata: CContainer non tiene un grafo delle dipendenze, quindi
// CalcCell() su una sola cella non si propaga a chi la referenzia in
// formula altrove -- bug scoperto in Fase 6 ("se modifico A1, B1
// (=A1+5) resta al valore vecchio finche' non lo tocco anch'esso").
// Il costo (piu' passate su tutte le celle CON CONTENUTO, non
// sull'intero foglio virtuale -- vedi GetBounds()) resta accettabile
// anche su fogli grandi: e' lo stesso meccanismo gia' usato e
// verificato al caricamento file.
void RecalculateAll(CContainer* doc);

// Chiamato da RecalculateWorkbook dopo ogni foglio elaborato in ogni
// passata (Fase 31, apertura di file grandi su un thread separato con
// una finestra di avanzamento): "sheetIndex"/"sheetCount" e
// "passIndex" descrivono l'avanzamento, "sheetName" e' quello del
// foglio appena elaborato. NULL (il default) non chiama nulla, stesso
// costo di prima per ogni chiamante che non ne ha bisogno.
typedef void (*RecalcProgressFunc)(void* context, int sheetIndex, int sheetCount,
	int passIndex, const char* sheetName);

// Stessa convergenza di RecalculateAll, estesa a tutti i fogli di
// "sheets" invece di un solo documento: una formula puo' referenziare
// un foglio diverso dal proprio (Fase 9, "NomeFoglio!Cella" --
// ISheetResolver in Container.h), quindi modificare un foglio puo'
// richiedere di ricalcolare anche gli altri. Usata da MainWindow al
// posto di RecalculateAll quando la cartella di lavoro ha piu' di un
// foglio (vedi MainWindow::RecalculateActiveWorkbook).
void RecalculateWorkbook(std::vector<AscdSheet>& sheets,
	RecalcProgressFunc progress = NULL, void* progressContext = NULL);

#endif
