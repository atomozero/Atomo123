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

#include <View.h>

#include "Cell.h"

class BTextControl;
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
	virtual void MessageReceived(BMessage* message);

	void SetDocument(CContainer* doc);
	CContainer* Document() const { return fDoc; }

	cell Selection() const { return fSelection; }
	void SetSelection(cell c);

private:
	CContainer* fDoc;
	cell fSelection;

	BTextControl* fEditor;
	cell fEditingCell;

	static const int kColWidth = 80;
	static const int kRowHeight = 20;
	static const int kHeaderWidth = 40;
	static const int kHeaderHeight = 20;

	BRect CellRect(cell c) const;
	void ScrollToShowSelection();
	void NotifySelectionChanged();
	void FixupScrollBars();

	void StartEditing(cell c, const char* initialText = NULL);
	void CommitEditing(bool cancel);
};

#endif
