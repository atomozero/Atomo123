/*
	SheetTabView.h

	Striscia di schede per il foglio attivo (Fase 9), in basso sotto
	la griglia -- come Excel/LibreOffice Calc, al posto del menu a
	tendina iniziale: una cartella di lavoro reale puo' avere decine
	di fogli (vedi ROADMAP.md), quindi la striscia scorre con due
	frecce quando non entrano tutte nella larghezza della finestra,
	invece di crescere all'infinito o tagliare silenziosamente le
	schede in eccesso.
*/

#ifndef SHEET_TAB_VIEW_H
#define SHEET_TAB_VIEW_H

#include <GraphicsDefs.h>
#include <String.h>
#include <View.h>

#include <vector>

// Inviati da un clic destro su una scheda (Fase 13), entrambi con un
// campo int32 "index" -- stesso principio di kMsgSwitchSheet sotto,
// esportati qui (non definiti dentro MainWindow.cpp) cosi' questa
// vista resta ignara di AscdSheet/MainWindow pur avendo un "what"
// stabile da inviare.
const uint32 kMsgRenameSheetRequest = 'rnsr';
const uint32 kMsgDeleteSheetRequest = 'dlsr';

class SheetTabView : public BView {
public:
	// switchWhat e' il "what" del BMessage inviato a target quando si
	// clicca una scheda, con un campo int32 "index" -- stesso
	// messaggio (kMsgSwitchSheet) gia' gestito da
	// MainWindow::MessageReceived, cosi' la vista non deve conoscere
	// nulla di AscdSheet/MainWindow.
	SheetTabView(const char* name, uint32 switchWhat, BHandler* target);
	virtual ~SheetTabView();

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void FrameResized(float width, float height);

	virtual BSize MinSize();
	virtual BSize MaxSize();
	virtual BSize PreferredSize();

	// Ricostruisce le schede a partire dai nomi dei fogli e
	// dall'indice del foglio attivo -- chiamato da
	// MainWindow::RebuildSheetTabs() ogni volta che cambia l'elenco
	// dei fogli o il foglio attivo (Nuovo, Apri, cambio scheda).
	// Porta la scheda attiva in vista se non lo e' gia'.
	// "hasColor"/"colors" sono opzionali e paralleli a "names" (stesso
	// indice, NULL = nessuna scheda colorata): il colore scelto per
	// ciascun foglio (import XLSX da <sheetPr><tabColor>, vedi
	// AscdSheet::tabColor in AscdIO.h) -- questa vista resta comunque
	// ignara di AscdSheet/MainWindow (vedi il commento in cima al
	// file), riceve solo colori grezzi.
	void SetSheets(const std::vector<BString>& names, int activeIndex,
		const std::vector<bool>* hasColor = NULL,
		const std::vector<rgb_color>* colors = NULL);

	// Esposti pubblicamente apposta per essere testabili direttamente
	// (stesso principio di SheetView::CellRect/CellAt -- vedi
	// tests/test_resize.cpp): permettono di verificare la logica di
	// scorrimento (quante schede stanno nella larghezza disponibile,
	// quale sia la prima visibile, dove si trovi in pixel una data
	// scheda) senza dover leggere lo schermo o simulare un vero clic
	// alla cieca.
	bool IsScrolling() const { return fScrolling; }
	int FirstVisibleIndex() const { return fFirstVisible; }
	BRect TabRectFor(int index) const;
	BRect LeftArrowRect() const;
	BRect RightArrowRect() const;

private:
	struct TabInfo {
		BString name;
		float width;
		bool hasColor;
		rgb_color color;
	};
	struct VisibleTab {
		int index;
		BRect rect;
	};

	std::vector<TabInfo> fTabs;
	std::vector<VisibleTab> fVisible;
	int fActiveIndex;
	int fFirstVisible;
	bool fScrolling;
	uint32 fSwitchWhat;
	BHandler* fTarget;

	// Ricalcola fVisible/fScrolling da fTabs/fFirstVisible/Bounds()
	// correnti -- MAI forza la scheda attiva in vista (lo fa solo
	// SetSheets, cosi' scorrere manualmente con le frecce per guardare
	// altre schede non viene subito annullato dal ridisegno
	// successivo).
	void Layout();
	bool IsIndexVisible(int index) const;
};

#endif
