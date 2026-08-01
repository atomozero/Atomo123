/*
	test_goto.cpp

	Verifica "Vai a" (Fase 7, MainWindow::HandleGoToRequest): sposta la
	selezione della SheetView attiva su una cella singola o un
	intervallo digitato, con la stessa sintassi di RangeRef::ParseRangeRef
	(gia' condivisa da grafico e tabella pivot); un testo non valido non
	fa nulla. Stessa ragione di test_names.cpp per HandleGoToName (di
	cui questo e' l'analogo generico, non legato a un nome definito):
	richiede una vera MainWindow, HandleGoToRequest e' pubblico apposta
	per essere testabile direttamente.
*/

#include <cstdio>

#include <Application.h>

#include "Cell.h"
#include "Range.h"
#include "Container.h"
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
	BApplication app("application/x-vnd.Atomo-TestGoTo");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();

	view->SetSelection(cell(1, 1)); // A1, punto di partenza noto

	win->HandleGoToRequest("C15");
	Check(view->Selection() == cell(3, 15), "\"Vai a\" su \"C15\" sposta la selezione a C15");

	win->HandleGoToRequest("A1:B5");
	range sel = view->SelectionRange();
	Check(sel.TopLeft() == cell(1, 1) && sel.BotRight() == cell(2, 5),
		"\"Vai a\" su \"A1:B5\" estende la selezione all'intero intervallo");

	// Un testo non valido non fa nulla: la selezione resta quella di
	// prima (A1:B5), nessun crash.
	win->HandleGoToRequest("non e' un riferimento valido");
	range afterInvalid = view->SelectionRange();
	Check(afterInvalid.TopLeft() == cell(1, 1) && afterInvalid.BotRight() == cell(2, 5),
		"un testo non valido non sposta la selezione (resta A1:B5)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
