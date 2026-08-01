/*
	test_preferences.cpp

	Verifica la finestra Preferenze (Fase 7,
	MainWindow::HandlePreferencesRequest): mostra/nascondi griglia
	(SheetView::ShowGrid, per-vista) e i separatori decimale/di elenco
	usati dal parser di formule (gDecimalPoint/gListSeparator, globali
	al motore -- vedi CParser/TryToParseString). Non copre la
	persistenza su disco tramite gPrefs (Preferences.h): in questo
	harness, come tutti gli altri test UI, gPrefs resta NULL (nessuna
	App::App() reale, solo una BApplication semplice), quindi
	HandlePreferencesRequest applica l'effetto in memoria e basta --
	verificato esplicitamente qui sotto, cosi' non resta un ramo di
	codice mai testato in questo file.
*/

#include <cstdio>

#include <Application.h>

#include "Cell.h"
#include "Value.h"
#include "Globals.h"
#include "Container.h"
#include "CellParser.h"
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
	BApplication app("application/x-vnd.Atomo-TestPreferences");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	Check(view->ShowGrid(), "la griglia e' visibile per impostazione predefinita");

	char originalDecimal = gDecimalPoint;
	char originalList = gListSeparator;

	win->HandlePreferencesRequest(false, ',', ',');
	Check(!view->ShowGrid(), "HandlePreferencesRequest(showGrid=false) nasconde la griglia");
	Check(gDecimalPoint == ',', "HandlePreferencesRequest imposta il separatore decimale globale");
	Check(gListSeparator == ',', "HandlePreferencesRequest imposta il separatore di elenco globale");

	// Il nuovo separatore decimale si applica davvero al parser: senza
	// passare esplicitamente decSep/listSep, TryToParseString ricade
	// su gDecimalPoint/gListSeparator (vedi CellParser.cpp).
	TryToParseString("1,5", cell(1, 1), doc, true); // A1, "1,5" con la virgola come decimale
	Value v;
	doc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 1.5,
		"col nuovo separatore decimale (virgola), \"1,5\" si interpreta come 1.5, non come testo");

	win->HandlePreferencesRequest(true, '.', ';');
	Check(view->ShowGrid(), "un secondo HandlePreferencesRequest riattiva la griglia");
	Check(gDecimalPoint == '.' && gListSeparator == ';',
		"e ripristina i separatori originali");

	// Ripristina i globali com'erano prima del test: sono processo-globali
	// al motore, non locali a questo documento, e questo processo di test
	// potrebbe eseguire altri controlli dopo questo punto (qui non ce ne
	// sono, ma e' la stessa disciplina gia' seguita altrove nel progetto
	// per non lasciare stato globale alterato).
	gDecimalPoint = originalDecimal;
	gListSeparator = originalList;

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
