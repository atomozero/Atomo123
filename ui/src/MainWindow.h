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

#include <vector>

#include <Window.h>

#include "Cell.h"
#include "Chart.h"

class BFilePanel;
class BTextControl;
class BStringView;
class SheetView;
class CContainer;
class FindWindow;
class ChartWindow;
class PivotWindow;

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
	FindWindow* fFindWindow;
	ChartWindow* fChartWindow;
	PivotWindow* fPivotWindow;
	std::vector<ChartObject> fCharts;

	void NewDocument();
	void CommitFormulaBar();
	void SaveToFile(const entry_ref& dir, const char* name);
	void CopySelection(bool cut);
	void PasteSelection();
	void DeleteSelection();
	void PrintDocument();
	void ShowFindWindow();
	void FindNext(const char* searchText);
	void ReplaceCurrent(const char* searchText, const char* replaceText);
	void ReplaceAll(const char* searchText, const char* replaceText);
	void SetCellFormat(int32 format);
	void ShowChartWindow();
	void ShowPivotWindow();
	void HandleChartRequest(const char* rangeText);
	void HandleChartInsert(const char* rangeText, const char* destText);
	void HandlePivotRequest(const char* sourceText, const char* destText, int32 agg);
};

#endif
