/*
	MainWindow.h

	Finestra principale: barra dei menu, barra formula, griglia del
	foglio (SheetView) dentro una BScrollView. Apertura file passa
	dal Translation Kit (BTranslatorRoster), che sceglie
	automaticamente il translator installato adatto (CSV/XLS/XLSX/
	ODS/ASCD nativo) in base al contenuto del file.
*/

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <Window.h>

#include "Cell.h"

class BFilePanel;
class BTextControl;
class BStringView;
class SheetView;
class CContainer;

class MainWindow : public BWindow {
public:
	MainWindow();
	virtual ~MainWindow();

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

	void OpenFile(const entry_ref& ref);
	void SelectionChanged(cell c);

private:
	SheetView* fSheetView;
	BTextControl* fFormulaBar;
	BStringView* fCellLabel;
	CContainer* fDoc;
	BFilePanel* fOpenPanel;
	BFilePanel* fSavePanel;

	void NewDocument();
	void CommitFormulaBar();
	void SaveToFile(const entry_ref& dir, const char* name);
	void CopySelection(bool cut);
	void PasteSelection();
	void DeleteSelection();
	void PrintDocument();
};

#endif
