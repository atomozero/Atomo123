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

#include <vector>

#include <NumberFormat.h>
#include <View.h>

#include "Cell.h"
#include "Chart.h"
#include "Range.h"

class BTextControl;
class CContainer;
class MainWindow;

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

	// Elenco dei grafici incorporati da disegnare sopra la griglia
	// (di proprieta' di MainWindow, che lo passa qui solo per
	// disegnarlo: SheetView non lo possiede ne' lo modifica mai).
	void SetCharts(const std::vector<ChartObject>* charts) { fCharts = charts; }

	// Rettangolo in pixel (a partire da 0,0, intestazioni comprese) che
	// copre le celle con contenuto -- usato da MainWindow per la stampa
	// (Print Kit), per sapere quanto foglio serve davvero senza
	// stampare l'intero intervallo virtuale del motore (702x16384).
	BRect ContentRect() const;

private:
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

	const std::vector<ChartObject>* fCharts;

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
	static const int kHeaderWidth = 40;
	static const int kHeaderHeight = 20;

	BRect CellRect(cell c) const;
	// Cella sotto un punto della vista (coordinate locali); non
	// controlla se il punto ricade sulle intestazioni -- i chiamanti
	// lo fanno gia' a parte, dove serve.
	cell CellAt(BPoint where) const;
	void ScrollToShowSelection();
	void NotifySelectionChanged();
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
