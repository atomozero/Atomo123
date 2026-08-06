/*
	SheetView.h

	Vista griglia del foglio di calcolo: disegna intestazioni di
	colonna (lettere) e riga (numeri), il contenuto delle celle
	esistenti, e gestisce selezione (mouse/tastiera) ed editing.
	L'editing e' doppio: dalla barra formula di MainWindow (sempre
	visibile) oppure in-cella (doppio click su una cella, o si inizia
	a digitare mentre e' selezionata), tramite un BTextControl
	temporaneo posizionato sopra la cella.
*/

#ifndef SHEET_VIEW_H
#define SHEET_VIEW_H

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <NumberFormat.h>
#include <View.h>

#include "Cell.h"
#include "Chart.h"
#include "EmbeddedImage.h"
#include "Range.h"

class BTextControl;
class CContainer;
class MainWindow;
struct CellStyle;

class SheetView : public BView {
public:
	SheetView(CContainer* doc);
	virtual ~SheetView();

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void MouseUp(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage);
	virtual void KeyDown(const char* bytes, int32 numBytes);
	virtual void AttachedToWindow();
	virtual void FrameResized(float width, float height);
	virtual void MessageReceived(BMessage* message);
	virtual void ScrollTo(BPoint where);

	void SetDocument(CContainer* doc);
	CContainer* Document() const { return fDoc; }

	// Cella "attiva": quella su cui agiscono la barra formula e
	// l'editing in-cella, e da cui ripartono i movimenti da tastiera
	// senza Maiusc. Coincide sempre con uno dei due angoli di
	// SelectionRange() (quello dove si trovava il cursore l'ultima
	// volta che la selezione e' stata estesa/mossa).
	cell Selection() const { return fSelection; }
	void SetSelection(cell c);

	// Estende la selezione dalla cella "ancora" (dove e' iniziato il
	// trascinamento o l'ultimo click senza Maiusc) fino a "c", che
	// diventa la nuova cella attiva -- usata da Maiusc+click,
	// Maiusc+frecce e dal trascinamento del mouse. A differenza di
	// SetSelection, non sposta l'ancora.
	void ExtendSelection(cell c);

	// Rettangolo pieno della selezione corrente (ancora e cella attiva
	// come angoli opposti) -- un singolo cliccato senza trascinare
	// produce un range di una sola cella, indistinguibile per chi
	// legge da "nessuna selezione multipla in corso".
	range SelectionRange() const;

	// Seleziona l'intero foglio con dati (menu Modifica > "Seleziona
	// tutto" in MainWindow, non una scorciatoia da tastiera -- vedi il
	// commento in HandleKey per il perche' Ctrl+A non e' praticabile
	// qui): dai limiti del documento, o solo A1 se il foglio e' vuoto.
	void SelectAll();

	// Cancella il contenuto di tutte le celle in SelectionRange() (non
	// solo la cella attiva) e ricalcola -- usata sia da Backspace/Canc
	// sia dal comando "Cancella" del menu Modifica, cosi' selezionare
	// piu' celle e cancellarle si comporta come in Excel/LibreOffice
	// Calc invece di svuotare solo la cella attiva.
	void ClearSelection();

	// Riempi in basso/a destra (menu Dati, Ctrl+D/Ctrl+R): copia il
	// contenuto della prima riga/colonna di SelectionRange() nelle
	// altre righe/colonne dell'intervallo, tramite CContainer::CopyCell
	// senza isDragMove -- un riferimento relativo in una formula e'
	// sempre interpretato rispetto alla posizione della cella che lo
	// contiene, quindi copiare lo stesso testo di formula in una nuova
	// posizione lo fa gia' puntare alla cella "spostata" corrispondente
	// da solo (=B1*2 in C1 copiato in C2 diventa =B2*2 senza bisogno di
	// toccare i riferimenti a mano). isDragMove=true farebbe l'esatto
	// opposto: compensa uno spostamento in modo che la formula continui
	// a puntare alle STESSE celle di prima (comportamento corretto per
	// spostare/tagliare una cella, sbagliato per riempire). Non fanno
	// nulla se la selezione e' una sola cella (niente da cui copiare).
	void FillDown();
	void FillRight();

	// Ordina l'intervallo selezionato per righe intere, confrontando
	// il valore nella colonna piu' a sinistra dell'intervallo (come
	// il pulsante "Ordina crescente/decrescente" di Excel quando non
	// si sceglie una chiave esplicita). Confronto numerico se entrambi
	// i valori sono numeri, altrimenti testuale case-insensitive;
	// ordinamento stabile (righe con lo stesso valore chiave
	// mantengono l'ordine relativo originale). Sposta ogni cella come
	// testo opaco (stessa rappresentazione di GetCellFormula/
	// TryToParseString, non tocca i riferimenti nelle formule) --
	// scelta deliberata: una formula che fa riferimento a un'altra
	// cella nello stesso intervallo ordinato avrebbe un significato
	// ambiguo dopo il riordino delle righe (limite noto, condiviso
	// anche dai fogli di calcolo reali in casi simili). Non fa nulla
	// se la selezione e' una sola riga (niente da ordinare).
	void SortSelection(bool ascending);

	// Inserisci/elimina riga e colonna: il numero di righe/colonne
	// coinvolte e il punto di inserimento/cancellazione vengono da
	// SelectionRange() (una selezione di 3 righe inserisce/elimina 3
	// righe, non serve selezionare l'intestazione -- Atomo123 non ha
	// ancora un modo per farlo). Sposta le celle esistenti oltre il
	// punto E aggiorna i riferimenti nelle formule di OGNI cella del
	// documento, non solo di quelle che si spostano fisicamente (una
	// formula sopra il punto di inserimento puo' comunque riferirsi a
	// una cella sotto che si sposta: il suo testo cambia pur restando
	// ferma) -- riusa CContainer::MoveCell con uno SplitType, lo
	// stesso meccanismo (mai cambiato) del Sum-It storico, vedi
	// legacy/opensumit/.../Commands/InsertCommands.cpp. Rifiuta
	// l'inserimento (senza modificare nulla, con un avviso) se
	// spingerebbe fuori dal limite fisso del foglio (kColCount/
	// kRowCount) celle che contengono gia' dati, per non perderli
	// silenziosamente. Limite noto: non sposta ne' ridimensiona i
	// grafici incorporati (ChartObject vive in MainWindow, non qui) --
	// restano dove sono anche se la loro area dati era nelle righe/
	// colonne spostate.
	void InsertRows();
	void InsertColumns();
	void DeleteRows();
	void DeleteColumns();

	// Annulla/Ripeti: pila di istantanee testuali di un intervallo (lo
	// stesso formato grezzo per cella usato da Ordina/Riempi --
	// GetCellFormula in lettura, TryToParseString in scrittura), cosi'
	// un solo meccanismo copre sia le mutazioni fatte qui dentro
	// (editing in-cella, Riempi, Ordina, Cancella) sia quelle fatte da
	// MainWindow (Taglia/Incolla, Trova e sostituisci), che le
	// possiede ma non ha accesso alle pile private: chiama
	// SaveUndoState() PRIMA di mutare il documento, esattamente come
	// fanno i metodi di questa classe. Annullare/ripetere seleziona
	// l'intervallo coinvolto, cosi' l'utente vede subito cosa e'
	// cambiato.
	void SaveUndoState(range affected);
	void SaveUndoState(cell affected);
	bool CanUndo() const { return !fUndoStack.empty(); }
	bool CanRedo() const { return !fRedoStack.empty(); }
	void Undo();
	void Redo();

	// Logica di navigazione/modifica da tastiera, con i modificatori
	// (Ctrl/Maiusc) gia' risolti -- KeyDown() li legge dal vero
	// messaggio B_KEY_DOWN e poi chiama questa, che e' pubblica
	// apposta per essere testabile direttamente (i test non passano
	// da un vero ciclo dei messaggi/dispatch della tastiera, quindi
	// Window()->CurrentMessage() non rifletterebbe modificatori
	// "finti" impostati a mano). Restituisce false per i tasti non
	// gestiti (KeyDown() passa allora a BView::KeyDown, per il
	// comportamento predefinito del Interface Kit).
	bool HandleKey(char key, bool ctrl, bool shift);

	// Angolo in alto a sinistra di una cella, in pixel: usato da
	// MainWindow per posizionare un grafico incorporato (vedi
	// ChartObject in Chart.h) alla cella di destinazione scelta
	// dall'utente, senza duplicare qui i costanti di layout
	// (kHeaderWidth/kColWidth/ecc., gia' privati a questa classe).
	BPoint CellOrigin(cell c) const { return CellRect(c).LeftTop(); }

	// Rettangolo in pixel di una cella (posizione e dimensione:
	// riflette la larghezza/altezza corrente di quella colonna/riga,
	// non piu' necessariamente kColWidth/kRowHeight se l'utente l'ha
	// ridimensionata trascinando un'intestazione). Pubblici apposta
	// per essere testabili direttamente -- vedi tests/test_resize.cpp.
	BRect CellRect(cell c) const;
	// Cella sotto un punto della vista (coordinate locali); non
	// controlla se il punto ricade sulle intestazioni -- i chiamanti
	// lo fanno gia' a parte, dove serve.
	cell CellAt(BPoint where) const;

	// Rettangolo di disegno del testo di una cella singola (non unita),
	// espanso sulle colonne vuote vicine se "textWidth" non entra in
	// "r" (CellRect(c) di partenza) -- il testo che trabocca in Excel,
	// vedi il commento esteso sopra la definizione in SheetView.cpp.
	// Pubblico apposta per essere testabile direttamente, stesso
	// principio di CellRect sopra -- vedi tests/test_overflow.cpp.
	BRect ExpandOverflowRect(BRect r, cell c, char alignment, float textWidth) const;

	// Testo effettivo che verra' disegnato per una cella, con la
	// riformattazione locale-aware valuta/percentuale/generico gia'
	// applicata sopra al testo del motore -- vedi il commento esteso
	// sopra la definizione in SheetView.cpp. Stringa vuota se la cella
	// non ha contenuto. Pubblico apposta per essere testabile
	// direttamente, stesso principio di CellRect/ExpandOverflowRect
	// sopra -- vedi tests/test_currency_format.cpp. Non "const": i
	// metodi Format*/di BNumberFormat (Locale Kit) non lo sono.
	BString FormattedCellText(cell c);

	// Applica larghezze di colonna personalizzate (coppie colonna
	// 1-based/larghezza in pixel), sostituendo quelle correnti:
	// ripristina prima ogni colonna alla larghezza predefinita
	// (kColWidth), poi applica solo quelle indicate in "widths" -- un
	// vettore vuoto riporta quindi tutte le colonne alla larghezza
	// predefinita. Usata da MainWindow dopo Nuovo/Apri e a ogni cambio
	// di foglio attivo (SwitchToSheet), cosi' un ridimensionamento
	// fatto su un foglio non "sopravvive" mostrandosi su un altro.
	// Non e' annullabile ne' marca il documento come modificato: fa
	// parte del cambio di documento/foglio, non e' una modifica
	// dell'utente. Pubblico apposta per essere testabile direttamente,
	// stesso principio di CellRect/CellAt sopra.
	void SetColumnWidths(const std::vector<std::pair<int, float> >& widths);

	// Le sole colonne la cui larghezza attuale differisce dalla
	// predefinita -- usata da MainWindow per catturare lo stato
	// corrente prima di cambiare foglio/documento o salvare, cosi' un
	// ridimensionamento fatto a mano sopravvive al cambio foglio e al
	// salvataggio nel formato nativo.
	std::vector<std::pair<int, float> > CustomColumnWidths() const;

	// Speculari a SetColumnWidths/CustomColumnWidths sopra, sull'altro
	// asse (Fase 10: persistenza dell'altezza di riga, prima un limite
	// noto -- solo per la sessione corrente).
	void SetRowHeights(const std::vector<std::pair<int, float> >& heights);
	std::vector<std::pair<int, float> > CustomRowHeights() const;

	// Testo a capo (Fase 12): il motore non fa layout di testo (vedi
	// il commento su CellStyle::fWrapText), quindi e' la UI a dover
	// far crescere l'altezza di riga quando serve. Ricalcola tutte le
	// righe con almeno una cella fWrapText, prendendo il MASSIMO fra
	// l'altezza gia' impostata (manuale o da Fase 10) e quella
	// necessaria a contenere il testo a capo alla larghezza di
	// colonna corrente -- non riduce mai una riga gia' piu' alta del
	// necessario. Chiamata da MainWindow dopo aver aperto un
	// documento (nativo o importato, in entrambi i casi il testo a
	// capo arriva gia' impostato sulle celle: qui si calcola solo
	// l'altezza) e dopo ToggleWrapText.
	void RecalculateWrappedRowHeights();

	// Mostra/nascondi la griglia sottile fra le celle (Fase 7,
	// finestra Preferenze): letta all'avvio da gPrefs (Preferences.h,
	// se esiste -- vedi App::App) e sovrascrivibile in ogni momento.
	// Non tocca mai la selezione/il contenuto delle celle, solo se
	// Draw() disegna le linee di griglia.
	void SetShowGrid(bool show);
	bool ShowGrid() const { return fShowGrid; }

	// Blocca riquadri (Fase 7): righe/colonne "congelate" -- restano
	// sempre visibili, ferme sullo schermo, invece di scorrere col
	// resto del foglio (stessa tecnica gia' usata per le intestazioni,
	// vedi Draw()/ScrollTo()). Contate dall'alto/da sinistra, 0 =
	// nessuna. ToggleFreezePanes usa la cella attiva come Excel: se non
	// e' gia' bloccato nulla, congela tutto cio' che sta sopra/a
	// sinistra della cella selezionata (fSelection); se lo e' gia',
	// sblocca. Pubblici apposta per essere testabili direttamente,
	// stesso principio di CellRect/CellAt sopra -- vedi tests/test_freeze.cpp.
	void ToggleFreezePanes();
	bool HasFreezePanes() const { return fFrozenRows > 0 || fFrozenCols > 0; }
	int FrozenRows() const { return fFrozenRows; }
	int FrozenCols() const { return fFrozenCols; }
	// Applica righe/colonne bloccate gia' note (dal formato nativo, non
	// derivate dalla selezione corrente) -- usata da MainWindow dopo
	// Apri/cambio foglio, stesso principio di SetColumnWidths.
	void SetFreezePanes(int rows, int cols);

	// Elenco dei grafici incorporati da disegnare sopra la griglia
	// (di proprieta' di MainWindow, che lo passa qui solo per
	// disegnarlo: SheetView non lo possiede ne' lo modifica mai).
	void SetCharts(const std::vector<ChartObject>* charts) { fCharts = charts; }

	// Elenco delle immagini incorporate (import XLSX, Fase 12) da
	// disegnare sopra la griglia -- stesso principio di SetCharts
	// sopra (di proprieta' di MainWindow), ma ogni immagine e' ancorata
	// a una cella (EmbeddedImage::anchor) invece di un BRect assoluto:
	// la posizione a schermo si ricalcola da CellRect(anchor) a ogni
	// disegno, cosi' l'immagine segue la cella se righe/colonne
	// cambiano dimensione.
	// Non piu' const: trascinare un'immagine con MouseDown/MouseMoved
	// sotto scrive direttamente offsetX/offsetY sull'elemento del
	// vettore -- lo stesso vettore di proprieta' di MainWindow (mai una
	// copia, vedi il commento sopra), quindi lo spostamento e' gia'
	// "salvato" nel modello non appena il trascinamento finisce, senza
	// bisogno di ricopiare nulla indietro verso MainWindow.
	void SetImages(std::vector<EmbeddedImage>* images) { fImages = images; }

	// AutoFilter (import XLSX, <autoFilter ref="...">): l'intervallo
	// (riga di intestazione + colonne) su cui disegnare le frecce a
	// tendina nell'intestazione e su cui rispondono i clic. Un solo
	// filtro attivo alla volta per foglio (come XLSX stesso: un
	// secondo <autoFilter> su un'altra tabella dello stesso foglio non
	// e' previsto dallo standard). "range" e' quello del motore (Cell.h),
	// 1-based, top==bottom perche' l'intestazione e' sempre una riga
	// sola -- i dati filtrati si intendono da riga top+1 in giu', fino
	// all'ultima riga con contenuto nell'intervallo di colonne
	// (GetBounds(), non un secondo limite esplicito: limite dichiarato,
	// non distingue una tabella vera e propria da altro contenuto sotto
	// che condivide le stesse colonne).
	void SetAutoFilter(range headerRange);
	void ClearAutoFilter();
	bool HasAutoFilter() const { return fHasAutoFilter; }
	range AutoFilterRange() const { return fAutoFilterRange; }

	// Valori distinti presenti nella colonna "col" fra i dati filtrati
	// (GetCellResult, il testo visualizzato -- stesso valore che
	// comparirebbe nell'elenco a tendina di Excel), in ordine di
	// comparsa nelle righe -- usata per costruire il menu a tendina.
	// Vuoto se non c'e' un AutoFilter attivo o "col" e' fuori dal suo
	// intervallo di colonne.
	std::vector<BString> UniqueColumnValues(int col) const;
	// Vero se ALMENO una riga con quel valore in quella colonna e'
	// attualmente visibile -- usata per lo stato iniziale (spuntato/
	// no) di ogni voce del menu a tendina.
	bool IsColumnValueVisible(int col, const BString& value) const;
	// Nasconde (hidden=true) o mostra (hidden=false) TUTTE le righe dei
	// dati filtrati il cui valore nella colonna "col" e' esattamente
	// "value" -- il criterio si combina con quello di altre colonne
	// gia' filtrate (AND fra colonne, come Excel vero: una riga resta
	// nascosta se ESCLUSA da almeno una colonna filtrata), non le
	// sostituisce. Ricalcola SUBITO le righe nascoste/visibili
	// dell'intero intervallo filtrato, non solo quelle toccate da
	// questa chiamata (necessario per l'AND fra colonne).
	void SetColumnValueHidden(int col, const BString& value, bool hidden);
	// Azzera ogni criterio impostato su ogni colonna e mostra di nuovo
	// tutte le righe dell'intervallo filtrato -- "Mostra tutto" nel
	// menu a tendina.
	void ClearColumnFilters();

	// Righe nascoste (AutoFilter, o import XLSX da <row hidden="1">
	// indipendentemente dal motivo originale -- un nascondimento manuale
	// dell'utente in Excel usa lo stesso attributo, questo progetto non
	// li distingue): un elenco sparso di indici di riga 1-based, non un
	// campo per cella. "SetHiddenRows" SOSTITUISCE l'intero elenco
	// (usata da MainWindow dopo Apri/cambio foglio, stesso principio di
	// SetColumnWidths); "SetColumnValueHidden"/"ClearColumnFilters"
	// sopra invece lo ricalcolano internamente dai criteri per colonna.
	void SetHiddenRows(const std::vector<int>& rows);
	std::vector<int> HiddenRows() const;
	bool IsRowHidden(int row) const;

	// Rettangolo (coordinate locali della vista, non pinnate: vedi
	// PinnedCellRect) della freccia a discesa nella cella di
	// intestazione di "col" -- usato sia da Draw() per disegnarla sia
	// da MouseDown per riconoscere un clic su di essa, cosi' i due
	// posti concordano sempre sulla stessa area senza duplicare le
	// coordinate. Valido solo se HasAutoFilter() e "col" e' nel suo
	// intervallo di colonne.
	BRect AutoFilterArrowRect(int col) const;

	// Rettangolo a schermo di un'immagine incorporata (ancora + scarto,
	// vedi EmbeddedImage.h): un solo posto per la formula, usata sia da
	// Draw() per disegnarla sia da MouseDown/MouseMoved per riconoscere
	// un clic/trascinamento su di essa, cosi' i due non possono
	// disallinearsi. Pubblico apposta per essere testabile direttamente
	// (stesso principio di AutoFilterArrowRect sopra).
	BRect ImageFrame(const EmbeddedImage& img) const;
	// Maniglia di ridimensionamento (un piccolo quadrato nell'angolo in
	// basso a destra di ImageFrame): stesso principio di
	// AutoFilterArrowRect sopra, un solo posto per la formula usata sia
	// da Draw() per disegnarla sia da MouseDown per riconoscere il
	// clic. Sempre visibile (non solo al passaggio del mouse), stesso
	// motivo gia' scritto per i puntini di ridimensionamento riga/
	// colonna: senza un indizio visivo permanente l'interazione non
	// sarebbe scopribile guardando lo schermo.
	BRect ImageResizeHandle(const EmbeddedImage& img) const;
	// Rettangolo della cella attiva, esteso a tutto l'intervallo se "c"
	// e' l'angolo di una cella unita -- un solo posto per la formula,
	// usato sia da Draw() per disegnare il riquadro di selezione sia da
	// SetSelection/ExtendSelection per invalidare la zona giusta quando
	// la selezione si sposta (altrimenti un vecchio riquadro esteso su
	// una cella unita non veniva mai cancellato dallo schermo). Pubblico
	// apposta per essere testabile direttamente (stesso principio di
	// ImageFrame sopra).
	BRect ActiveCellRect(cell c) const;
	// Costruisce ed apre (bloccante, sincrono) il menu dei valori della
	// colonna "col" -- MouseDown lo richiama quando riconosce un clic
	// su AutoFilterArrowRect. Non testabile automaticamente (un vero
	// BPopUpMenu::Go() sincrono richiede un clic reale dell'utente,
	// stesso limite gia' documentato altrove in questo progetto per i
	// dialoghi modali -- vedi test_selection.cpp): la logica vera e
	// propria (UniqueColumnValues/IsColumnValueVisible/
	// SetColumnValueHidden sopra) e' invece testabile a parte, chiamata
	// direttamente senza passare dal menu.
	void ShowAutoFilterMenu(int col, BPoint screenAnchor);

	// Convalida dati (Fase 13): stesso principio di AutoFilterArrowRect
	// sopra, ma per singola cella (non un'intera colonna) -- valido
	// solo se la cella ha una regola di tipo eListValidation. Il menu
	// e' sincrono come ShowAutoFilterMenu, stesso limite di test
	// automatico.
	BRect ValidationArrowRect(cell c) const;
	void ShowValidationMenu(cell c, BPoint screenAnchor);

	// Rettangolo in pixel (a partire da 0,0, intestazioni comprese) che
	// copre le celle con contenuto -- usato da MainWindow per la stampa
	// (Print Kit), per sapere quanto foglio serve davvero senza
	// stampare l'intero intervallo virtuale del motore (702x16384).
	BRect ContentRect() const;

private:
	// Larghezza/altezza di ogni colonna/riga: un array denso (non
	// sparso) indicizzato da 0 a kColCount-1/kRowCount-1, inizializzato
	// a kColWidth/kRowHeight per tutti e modificato solo dove l'utente
	// trascina un bordo di intestazione (vedi MouseDown/MouseMoved).
	// Occupazione trascurabile (~2,8 KiB/64 KiB) anche per un foglio
	// completamente vuoto. fColOffsets/fRowOffsets sono la somma
	// cumulativa corrispondente (fColOffsets[i] = distanza dal bordo
	// sinistro del foglio all'inizio della colonna i+1esima, in pixel,
	// fColOffsets[kColCount] = larghezza totale del foglio): tenerla
	// gia' calcolata rende CellRect() O(1) e la ricerca "che colonna
	// c'e' sotto x" O(log n) con una ricerca binaria, invece di
	// ricalcolare una somma da zero a ogni chiamata (Draw() ne fa
	// diverse per ogni ridisegno). Ricostruita per intero solo quando
	// una larghezza/altezza cambia davvero (RebuildColumnOffsets/
	// RebuildRowOffsets), non a ogni Draw().
	//
	// Larghezza di colonna e altezza di riga sopravvivono entrambe al
	// salvataggio/riapertura (Fase 9 per le colonne, Fase 10 per le
	// righe: vedi SetColumnWidths/CustomColumnWidths e SetRowHeights/
	// CustomRowHeights sopra, piu' la sezione dedicata in AscdIO.h).
	std::vector<float> fColWidths;
	std::vector<float> fRowHeights;
	std::vector<float> fColOffsets;
	std::vector<float> fRowOffsets;
	// Righe nascoste (AutoFilter o import XLSX <row hidden="1">): un
	// array denso parallelo a fRowHeights, non sovrapposto ad esso
	// (un'altezza 0 sarebbe stata riportata al minimo da SetRowHeights
	// sopra) -- RebuildRowOffsets sotto tratta una riga nascosta come
	// se avesse altezza 0 SENZA modificare fRowHeights, cosi' torna
	// alla sua altezza vera (personalizzata o predefinita) quando
	// mostrata di nuovo, esattamente come Excel ricorda l'altezza di
	// una riga nascosta.
	std::vector<bool> fRowHidden;
	void RebuildColumnOffsets();
	void RebuildRowOffsets();
	// Indice di colonna/riga (1-based, sempre in [1, kColCount]/
	// [1, kRowCount]) la cui area copre la coordinata x/y data --
	// ricerca binaria su fColOffsets/fRowOffsets (std::upper_bound).
	int ColumnAtX(float x) const;
	int RowAtY(float y) const;
	// Se x/y cade entro qualche pixel dal confine fra due colonne/
	// righe (la "maniglia" per trascinare un ridimensionamento),
	// l'indice della colonna/riga a SINISTRA/SOPRA quel confine;
	// altrimenti 0 (nessun confine abbastanza vicino).
	int ColumnBoundaryAt(float x) const;
	int RowBoundaryAt(float y) const;
	// Colonna/riga attualmente in trascinamento per il
	// ridimensionamento (0 = nessuno), la posizione di partenza del
	// trascinamento e la larghezza/altezza che aveva all'inizio --
	// bastano a calcolare la nuova dimensione a ogni MouseMoved senza
	// dover ricordare l'intera cronologia del trascinamento.
	int fResizingColumn;
	int fResizingRow;
	float fResizeDragStart;
	float fResizeStartSize;
	// Ridimensiona la vista stessa (ResizeTo) alla nuova dimensione
	// totale del foglio dopo che una colonna/riga ha cambiato
	// larghezza/altezza -- aggiorna Frame()/Bounds() cosi'
	// FixupScrollBars() (richiamato automaticamente da FrameResized)
	// riflette il nuovo spazio scorrevole totale.
	void UpdateCanvasSize();

	// Cursore mostrato passando sopra un confine ridimensionabile
	// (segnalato dall'utente insieme ai puntini in Draw(), come
	// indizio visivo aggiuntivo -- stessa idea del cursore a doppia
	// freccia di Excel/LibreOffice Calc sull'intestazione): 0 =
	// normale, 1 = confine di colonna (freccia orizzontale), 2 =
	// confine di riga (freccia verticale). Ricordato per evitare di
	// richiamare SetViewCursor a ogni singolo MouseMoved anche quando
	// non e' cambiato nulla.
	int fHoverCursor;

	// Mostra/nascondi griglia: vedi SetShowGrid/ShowGrid sopra.
	bool fShowGrid;

	// Blocca riquadri: vedi ToggleFreezePanes/SetFreezePanes sopra.
	int fFrozenRows;
	int fFrozenCols;
	// Disegna un blocco di celle (sfondo, griglia, testo) con
	// l'origine indicata: (0,0) per il riquadro scorrevole normale,
	// (Bounds().left, 0)/(0, Bounds().top) per una banda congelata
	// (cosi' resta ferma sullo schermo durante lo scroll, esattamente
	// come le intestazioni -- vedi il commento sopra ScrollTo() per il
	// meccanismo). clipRect delimita l'area disegnata IN COORDINATE
	// SULLO SCHERMO (gia' comprensive di xOrigin/yOrigin).
	void DrawCellBand(BRect clipRect, int firstCol, int lastCol,
		int firstRow, int lastRow, float xOrigin, float yOrigin);
	// Disegna i quattro lati del bordo di "r" secondo lo spessore
	// (0=nessuno/1/2/3) e il colore condiviso di "cs" -- unico punto
	// condiviso dal ciclo per cella e da quello per cella unita in
	// Draw(), che avevano prima la stessa sequenza di quattro "if"
	// duplicata identica (Fase 13).
	void DrawBorderSides(const CellStyle& cs, BRect r);
	// CellRect(c), spostato come farebbe DrawCellBand se "c" ricade in
	// una banda congelata -- usata per il rettangolo di selezione, che
	// deve restare incollato allo schermo sopra una cella congelata
	// esattamente come il suo contenuto (altrimenti la selezione
	// "scapperebbe" scorrendo mentre la cella stessa resta ferma).
	BRect PinnedCellRect(cell c) const;

	CContainer* fDoc;
	cell fSelection;

	// Cella dove e' iniziata la selezione corrente (ultimo click senza
	// Maiusc, o inizio di un trascinamento): insieme a fSelection
	// definisce il rettangolo restituito da SelectionRange(). Un
	// singolo click la riporta a coincidere con fSelection (range di
	// una sola cella).
	cell fAnchor;

	// Trascinamento del mouse in corso (bottone premuto su una cella
	// valida): MouseMoved estende la selezione solo mentre e' vero,
	// per non confondere un trascinamento con un semplice movimento
	// del mouse a bottone rilasciato.
	bool fDragging;

	// Vedi SaveUndoState/Undo/Redo: un'istantanea e' l'intervallo
	// coinvolto piu' il testo grezzo delle sole celle che esistevano
	// davvero al suo interno nel momento in cui e' stata catturata
	// (non un blocco denso con una voce anche per le celle vuote --
	// per un intervallo piccolo come quelli di Riempi/Ordina la
	// differenza e' irrilevante, ma Inserisci/Elimina riga o colonna
	// deve poter catturare l'INTERO documento, dato che una formula
	// sopra il punto di inserimento puo' comunque riferirsi a una
	// cella sotto che si sposta: un blocco denso su kColCount x
	// kRowCount sarebbe milioni di celle anche per un foglio quasi
	// vuoto). Ripristinare svuota prima ogni cella che esiste ora
	// nell'intervallo (CCellIterator, non un doppio ciclo su ogni
	// posizione), poi riscrive solo le celle catturate.
	struct UndoSnapshot {
		range r;
		std::vector<std::pair<cell, std::string> > cells;
	};
	std::vector<UndoSnapshot> fUndoStack;
	std::vector<UndoSnapshot> fRedoStack;
	UndoSnapshot CaptureSnapshot(range r) const;
	void ApplySnapshot(const UndoSnapshot& snap);

	const std::vector<ChartObject>* fCharts;
	std::vector<EmbeddedImage>* fImages;
	// Trascinamento di un'immagine incorporata (MouseDown/MouseMoved/
	// MouseUp): stesso schema di fResizingColumn/fResizingRow sopra
	// (indice + punto di partenza + valore di partenza, la nuova
	// posizione e' sempre "valore di partenza piu' spostamento del
	// mouse dall'inizio"), ma qui l'indice puo' essere legittimamente 0
	// (la prima immagine), quindi -1 vuol dire "nessun trascinamento in
	// corso", non 0 come per le colonne/righe (1-based, 0 = nessuna).
	int fDraggingImageIndex;
	BPoint fDragImageStart;
	float fDragImageStartOffsetX, fDragImageStartOffsetY;
	// Ridimensionamento di un'immagine incorporata (maniglia nell'angolo
	// in basso a destra, vedi ImageResizeHandle pubblico sopra): stesso
	// schema esatto dello spostamento appena sopra, ma su width/height
	// invece di offsetX/offsetY -- un trascinamento e' o l'uno o
	// l'altro, mai entrambi insieme (MouseDown controlla prima la
	// maniglia, poi il corpo dell'immagine, vedi il commento li').
	int fResizingImageIndex;
	BPoint fResizeImageStart;
	float fResizeImageStartWidth, fResizeImageStartHeight;

	// AutoFilter: vedi SetAutoFilter/SetColumnValueHidden ecc. sopra.
	// fFilterHiddenValues e' per-colonna (indice di colonna 1-based),
	// mai persistito (solo le righe nascoste RISULTANTI lo sono, vedi
	// fRowHidden sopra e la sezione dedicata in AscdIO.h): riaprire un
	// file mostra di nuovo le righe giuste, ma il menu a tendina non
	// "ricorda" quali valori esatti le avevano escluse -- limite
	// dichiarato, non un bug.
	bool fHasAutoFilter;
	range fAutoFilterRange;
	std::map<int, std::vector<BString> > fFilterHiddenValues;
	void RecomputeAutoFilterVisibility();
	// Ultima riga con contenuto in una qualunque colonna dell'intervallo
	// di colonne di fAutoFilterRange, cercata a partire da GetBounds()
	// del documento -- il limite inferiore dei "dati filtrati" per
	// UniqueColumnValues/RecomputeAutoFilterVisibility sopra.
	int AutoFilterDataBottom() const;

	BTextControl* fEditor;
	cell fEditingCell;

	// Formattazione locale-aware dei numeri (separatore delle migliaia,
	// punto/virgola decimale secondo le preferenze di sistema) tramite
	// il Locale Kit -- il motore stesso formatta i numeri in modo
	// generico (CFormatter/eGeneral, non locale-aware), quindi questo
	// e' un livello di presentazione applicato solo per la griglia.
	BNumberFormat fNumberFormat;

	static const int kColWidth = 80;
	static const int kRowHeight = 20;
	static const int kHeaderWidth = 30;
	static const int kHeaderHeight = 20;
	// Non si puo' stringere una colonna/riga oltre questo, per non
	// farla sparire (e con lei la maniglia per riallargarla).
	static const int kMinColWidth = 20;
	static const int kMinRowHeight = 10;
	// Distanza massima (in pixel) da un confine fra due intestazioni
	// perche' MouseDown lo riconosca come maniglia di ridimensionamento
	// invece che come un clic ordinario sull'intestazione.
	static const int kResizeGrip = 4;

	void ScrollToShowSelection();
	void NotifySelectionChanged();
	// Avvisa MainWindow (dirty flag/titolo) ogni volta che una
	// mutazione del documento e' avvenuta qui dentro -- stesso
	// principio di NotifySelectionChanged.
	void NotifyDocumentChanged();
	// Ricalcola fDoc dopo una mutazione -- al posto di RecalculateAll
	// diretto (usato prima di Fase 9): se la finestra proprietaria e'
	// una vera MainWindow con piu' fogli, ricalcola l'intera cartella
	// di lavoro (una formula puo' referenziarne un altro), altrimenti
	// ricade su RecalculateAll(fDoc) da solo -- stesso principio di
	// NotifySelectionChanged/NotifyDocumentChanged (dynamic_cast su
	// Window()).
	void RecalculateOwningWorkbook();
	void FixupScrollBars();

	// Il Frame() della view copre l'intero intervallo virtuale del
	// motore (kColCount x kRowCount celle), non solo l'area visibile a
	// schermo: e' il pattern classico BeOS/Haiku per una vista
	// scorrevole (la BScrollView ritaglia e scorre questa vista
	// grande, non viceversa -- altrimenti Draw() non riceverebbe mai
	// un updateRect piu' grande della vista stessa). Bounds() riflette
	// quindi sempre questa dimensione piena, mai la porzione visibile:
	// per questo FixupScrollBars usa Parent()->Bounds() (la vera area
	// visibile della BScrollView), non Bounds() proprio, per calcolare
	// l'intervallo delle scrollbar.
	static BRect FullCanvasFrame();

	void StartEditing(cell c, const char* initialText = NULL);
	void CommitEditing(bool cancel);
};

#endif
