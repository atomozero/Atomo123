/*
	test_whatif.cpp

	Verifica le Tabelle dati What-if (Tier 4, "Path to full Excel
	parity"): MainWindow::ApplyWhatIfDataTable, chiamata direttamente
	(e' pubblica, stesso principio di AutoSum in test_autosum.cpp).
	Copre tutti e tre i casi (input riga, input colonna, entrambi/2D) e
	verifica che la cella input torni al valore originale dopo il giro,
	non resti all'ultimo valore sostitutivo provato dal ciclo interno.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Path.h>
#include <Roster.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "ResourceManager.h"
#include "SheetView.h"
#include "MainWindow.h"

static int gFailures = 0;

static void Check(bool condition, const char* what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

static double CellNumber(CContainer* doc, cell c)
{
	Value v;
	doc->GetValue(c, v);
	return (double)v;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestWhatIf");

	// Le funzioni con nome non servono qui (nessuna =SUM()/ecc. nelle
	// formule di prova, solo riferimenti/moltiplicazioni dirette), ma
	// si inizializzano comunque per coerenza con test_autosum.cpp --
	// ApplyWhatIfDataTable passa comunque da RecalculateActiveWorkbook,
	// che ricalcola l'intero foglio.
	app_info info;
	if (app.GetAppInfo(&info) == B_OK) {
		BPath path(&info.ref);
		if (path.InitCheck() == B_OK) {
			gAppName = path;
			gResourceManager.SetTo(&path);
			try {
				InitFunctions();
			} catch (CErr& e) {
				printf("FAIL impossibile caricare la tabella funzioni: %s\n", (char*)e);
				return 1;
			}
		}
	}

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// Celle input VERE, lontane dall'area della tabella dati -- H1/H2
	// (colonna 8), valori "a riposo" diversi da ogni valore sostitutivo
	// usato sotto, cosi' un mancato ripristino sarebbe evidente.
	TryToParseString("999", cell(8, 1), doc, true); // H1
	TryToParseString("888", cell(8, 2), doc, true); // H2

	// --- Caso 1: input RIGA (H1), tabella A1:C2. ---
	TryToParseString("=H1*2", cell(1, 1), doc, true); // A1: formula
	TryToParseString("5", cell(2, 1), doc, true);      // B1: valore sostitutivo 1
	TryToParseString("10", cell(3, 1), doc, true);     // C1: valore sostitutivo 2
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(3, 2));
	win->ApplyWhatIfDataTable("H1", "");

	Check(CellNumber(doc, cell(2, 2)) == 10.0, "input riga: B2 = H1(5)*2 = 10");
	Check(CellNumber(doc, cell(3, 2)) == 20.0, "input riga: C2 = H1(10)*2 = 20");
	Check(CellNumber(doc, cell(8, 1)) == 999.0,
		"input riga: H1 torna al valore originale (999) dopo il giro");

	// --- Caso 2: input COLONNA (H1), tabella A4:B6. ---
	TryToParseString("=H1*2", cell(1, 4), doc, true); // A4: formula
	TryToParseString("5", cell(1, 5), doc, true);      // A5: valore sostitutivo 1
	TryToParseString("10", cell(1, 6), doc, true);     // A6: valore sostitutivo 2
	view->SetSelection(cell(1, 4));
	view->ExtendSelection(cell(2, 6));
	win->ApplyWhatIfDataTable("", "H1");

	Check(CellNumber(doc, cell(2, 5)) == 10.0, "input colonna: B5 = H1(5)*2 = 10");
	Check(CellNumber(doc, cell(2, 6)) == 20.0, "input colonna: B6 = H1(10)*2 = 20");
	Check(CellNumber(doc, cell(8, 1)) == 999.0,
		"input colonna: H1 torna al valore originale (999) dopo il giro");

	// --- Caso 3: due variabili (input riga H1, input colonna H2),
	// tabella A10:C12. ---
	TryToParseString("=H1+H2", cell(1, 10), doc, true); // A10: formula
	TryToParseString("1", cell(2, 10), doc, true);       // B10: valore riga 1
	TryToParseString("2", cell(3, 10), doc, true);       // C10: valore riga 2
	TryToParseString("10", cell(1, 11), doc, true);      // A11: valore colonna 1
	TryToParseString("20", cell(1, 12), doc, true);      // A12: valore colonna 2
	view->SetSelection(cell(1, 10));
	view->ExtendSelection(cell(3, 12));
	win->ApplyWhatIfDataTable("H1", "H2");

	Check(CellNumber(doc, cell(2, 11)) == 11.0, "2D: B11 = H1(1)+H2(10) = 11");
	Check(CellNumber(doc, cell(3, 11)) == 12.0, "2D: C11 = H1(2)+H2(10) = 12");
	Check(CellNumber(doc, cell(2, 12)) == 21.0, "2D: B12 = H1(1)+H2(20) = 21");
	Check(CellNumber(doc, cell(3, 12)) == 22.0, "2D: C12 = H1(2)+H2(20) = 22");
	Check(CellNumber(doc, cell(8, 1)) == 999.0,
		"2D: H1 torna al valore originale (999) dopo il giro");
	Check(CellNumber(doc, cell(8, 2)) == 888.0,
		"2D: H2 torna al valore originale (888) dopo il giro");

	// --- Annulla: la tabella dati e' annullabile (solo l'intervallo dei
	// risultati, non le celle input -- vedi il commento in
	// MainWindow::ApplyWhatIfDataTable). ---
	Check(view->CanUndo(), "dopo una tabella dati, Annulla e' disponibile");
	view->Undo();
	char afterUndoText[64] = { 0 };
	doc->GetCellFormula(cell(2, 11), afterUndoText, sizeof(afterUndoText), false);
	Check(afterUndoText[0] == 0,
		"Annulla svuota di nuovo l'intervallo dei risultati dell'ultima tabella (2D)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
