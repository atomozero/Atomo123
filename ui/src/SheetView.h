/*
	SheetView.h

	Vista griglia del foglio di calcolo: disegna intestazioni di
	colonna (lettere) e riga (numeri), il contenuto delle celle
	esistenti, e gestisce selezione (mouse/tastiera). L'editing vero
	e proprio del contenuto passa dalla barra formula di
	MainWindow, non da un editor in-cella (limite noto della prima
	versione della UI, vedi ROADMAP.md Fase 4).
*/

#ifndef SHEET_VIEW_H
#define SHEET_VIEW_H

#include <View.h>

#include "Cell.h"

class CContainer;
class MainWindow;

class SheetView : public BView {
public:
	SheetView(BRect frame, CContainer* doc);
	virtual ~SheetView();

	virtual void Draw(BRect updateRect);
	virtual void MouseDown(BPoint where);
	virtual void KeyDown(const char* bytes, int32 numBytes);
	virtual void AttachedToWindow();
	virtual void FrameResized(float width, float height);

	void SetDocument(CContainer* doc);
	CContainer* Document() const { return fDoc; }

	cell Selection() const { return fSelection; }
	void SetSelection(cell c);

private:
	CContainer* fDoc;
	cell fSelection;

	static const int kColWidth = 80;
	static const int kRowHeight = 20;
	static const int kHeaderWidth = 40;
	static const int kHeaderHeight = 20;

	BRect CellRect(cell c) const;
	void ScrollToShowSelection();
	void NotifySelectionChanged();
	void FixupScrollBars();
};

#endif
