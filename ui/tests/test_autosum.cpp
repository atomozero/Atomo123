/*
	test_autosum.cpp

	Verifica la Somma automatica (MainWindow::AutoSum, menu Formule/
	toolbar): scrive nella cella attiva =SUM() sull'intervallo di
	numeri contigui sopra di essa, altrimenti a sinistra -- come Excel.
	Il testo si ferma alla prima cella non numerica; senza numeri
	adiacenti non scrive nulla. Richiede una vera MainWindow (stesso
	principio di test_comments.cpp).
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

int main()
{
	BApplication app("application/x-vnd.Atomo-TestAutoSum");

	// Le funzioni con nome (SUM) vivono nelle risorse 'Func' legate al
	// binario (vedi App::ReadyToRun): il Makefile allega Atomo123.rsrc
	// a questo eseguibile con xres, qui si ripete la stessa
	// inizializzazione dell'app vera. Senza, ogni =SUM() resterebbe un
	// identificatore sconosciuto (stesso limite documentato in
	// test_unary_plus.cpp per i binari senza risorse).
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

	// Caso verticale: tre numeri sopra la cella attiva. Il testo
	// canonico dell'engine usa ".." senza "=" iniziale (vedi
	// test_fill.cpp, che confronta con strstr per lo stesso motivo),
	// quindi qui si cerca la sottostringa, non l'uguaglianza esatta.
	TryToParseString("10", cell(1, 1), doc, true); // A1
	TryToParseString("20", cell(1, 2), doc, true); // A2
	TryToParseString("30", cell(1, 3), doc, true); // A3
	view->SetSelection(cell(1, 4)); // A4
	win->AutoSum();
	char formula[512] = { 0 };
	doc->GetCellFormula(cell(1, 4), formula, sizeof(formula), false);
	Check(strstr(formula, "SUM") != NULL && strstr(formula, "A1") != NULL
			&& strstr(formula, "A3") != NULL,
		"AutoSum sopra tre numeri scrive SUM(A1:A3)");
	char result[512] = { 0 };
	doc->GetCellResult(cell(1, 4), result, sizeof(result), true);
	Check(strcmp(result, "60") == 0,
		"...e la somma vale 60");

	// Caso orizzontale: colonna sopra vuota, numeri a sinistra.
	TryToParseString("1", cell(1, 6), doc, true); // A6
	TryToParseString("2", cell(2, 6), doc, true); // B6
	view->SetSelection(cell(3, 6)); // C6
	win->AutoSum();
	formula[0] = 0;
	doc->GetCellFormula(cell(3, 6), formula, sizeof(formula), false);
	Check(strstr(formula, "SUM") != NULL && strstr(formula, "A6") != NULL
			&& strstr(formula, "B6") != NULL,
		"AutoSum senza numeri sopra ripiega a sinistra (SUM(A6:B6))");

	// Il testo interrompe la scansione: solo A9 conta, non A7.
	TryToParseString("99", cell(1, 7), doc, true); // A7
	TryToParseString("txt", cell(1, 8), doc, true); // A8
	TryToParseString("5", cell(1, 9), doc, true); // A9
	view->SetSelection(cell(1, 10)); // A10
	win->AutoSum();
	formula[0] = 0;
	doc->GetCellFormula(cell(1, 10), formula, sizeof(formula), false);
	Check(strstr(formula, "SUM") != NULL && strstr(formula, "A9") != NULL
			&& strstr(formula, "A7") == NULL,
		"AutoSum si ferma al testo (solo A9, non A7)");

	// Senza numeri adiacenti non scrive nulla (e non crasha).
	view->SetSelection(cell(20, 100)); // T100, zona vuota
	win->AutoSum();
	formula[0] = 0;
	doc->GetCellFormula(cell(20, 100), formula, sizeof(formula), false);
	Check(formula[0] == 0,
		"AutoSum senza numeri adiacenti non scrive nessuna formula");

	// La ricerca dell'intervallo e' logica pura (nessuna SUM da
	// risolvere): verificata direttamente, senza passare dalla UI.
	range found;
	Check(MainWindow::FindAutoSumRange(doc, cell(1, 4), &found)
			&& found.left == 1 && found.top == 1
			&& found.right == 1 && found.bottom == 3,
		"FindAutoSumRange trova A1:A3 sopra A4");
	Check(MainWindow::FindAutoSumRange(doc, cell(3, 6), &found)
			&& found.left == 1 && found.top == 6
			&& found.right == 2 && found.bottom == 6,
		"FindAutoSumRange ripiega a sinistra (A6:B6 per C6)");
	Check(MainWindow::FindAutoSumRange(doc, cell(1, 10), &found)
			&& found.left == 1 && found.top == 9
			&& found.right == 1 && found.bottom == 9,
		"FindAutoSumRange si ferma al testo (solo A9)");
	Check(!MainWindow::FindAutoSumRange(doc, cell(20, 100), &found),
		"FindAutoSumRange torna false senza numeri adiacenti");
	Check(!MainWindow::FindAutoSumRange(NULL, cell(1, 1), &found),
		"FindAutoSumRange torna false senza documento");
	Check(!MainWindow::FindAutoSumRange(doc, cell(1, 1), NULL),
		"FindAutoSumRange torna false senza intervallo di uscita");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
