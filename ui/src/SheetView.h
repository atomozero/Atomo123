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

class BTextControl;
class CContainer;
class MainWindow;

class SheetView : public BView {
public:
	SheetView(CContainer* doc);
	virtual ~SheetView();

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void KeyDown(const char* bytes, int32 numBytes);
	virtual void AttachedToWindow();
	virtual void FrameResized(float width, float height);
	virtual void MessageReceived(BMessage* message);
	virtual void ScrollTo(BPoint where);

	void SetDocument(CContainer* doc);
	CContainer* Document() const { return fDoc; }

	cell Selection() const { return fSelection; }
	void SetSelection(cell c);

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
