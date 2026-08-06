/*
	test_preferences.cpp

	Verifica la finestra Preferenze (Fase 7, sezioni ampliate in Fase 13,
	MainWindow::HandlePreferencesRequest): mostra/nascondi griglia
	(SheetView::ShowGrid, per-vista), i separatori decimale/di elenco
	usati dal parser di formule (gDecimalPoint/gListSeparator, globali
	al motore -- vedi CParser/TryToParseString), e il numero di file
	recenti da ricordare (MainWindow::fMaxRecentFiles, prima un limite
	fisso nel codice). Non copre la persistenza su disco tramite gPrefs
	(Preferences.h): in questo harness, come tutti gli altri test UI,
	gPrefs resta NULL (nessuna App::App() reale, solo una BApplication
	semplice), quindi HandlePreferencesRequest applica l'effetto in
	memoria e basta -- verificato esplicitamente qui sotto, cosi' non
	resta un ramo di codice mai testato in questo file.
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

	win->HandlePreferencesRequest(false, ',', ',', 5, true);
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

	win->HandlePreferencesRequest(true, '.', ';', 5, true);
	Check(view->ShowGrid(), "un secondo HandlePreferencesRequest riattiva la griglia");
	Check(gDecimalPoint == '.' && gListSeparator == ';',
		"e ripristina i separatori originali");

	// Numero di file recenti da ricordare (prima MainWindow::
	// kMaxRecentFiles fisso a 5, ora scelto in Preferenze): fuori
	// range viene bloccato ai limiti [1, kMaxRecentFilesLimit] invece
	// di accettare valori assurdi (0, negativi, o oltre gli slot
	// riservati in gPrefs).
	win->HandlePreferencesRequest(true, '.', ';', 10, true);
	Check(win->MaxRecentFiles() == 10,
		"HandlePreferencesRequest accetta un numero di file recenti valido (10)");

	win->HandlePreferencesRequest(true, '.', ';', 0, true);
	Check(win->MaxRecentFiles() == 1,
		"un numero di file recenti troppo basso (0) viene riportato al minimo (1)");

	win->HandlePreferencesRequest(true, '.', ';', 999, true);
	Check(win->MaxRecentFiles() == 15,
		"un numero di file recenti troppo alto (999) viene riportato al massimo (kMaxRecentFilesLimit)");

	win->HandlePreferencesRequest(true, '.', ';', 5, true); // ripristina il predefinito

	// showSplash (Fase 13): non tocca nessuno stato di MainWindow (letta
	// solo da App::ReadyToRun al prossimo avvio, vedi il commento li'),
	// quindi qui interessa solo che passare false non causi un crash.
	win->HandlePreferencesRequest(true, '.', ';', 5, false);
	Check(true, "HandlePreferencesRequest accetta showSplash=false senza crash");

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
