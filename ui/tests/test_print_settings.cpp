/*
	test_print_settings.cpp

	Verifica la parte di "Impostazioni di stampa" (Fase 27) che vive in
	MainWindow, non in PrintLayout.cpp (gia' coperto da
	test_print_layout.cpp): SetPrintArea/ClearPrintArea/HasPrintArea
	(la selezione corrente diventa l'area di stampa del foglio ATTIVO,
	sessione soltanto -- vedi AscdSheet::printArea in AscdIO.h) e
	HandlePageSetupRequest (margini/scala, stesso schema gia' testato
	in test_preferences.cpp per HandlePreferencesRequest -- qui gPrefs
	resta NULL come in tutti i test UI di questo harness, quindi non
	c'e' nessuna scrittura su disco da verificare, solo che non causi
	un crash).

	PrintDocument() stesso non e' testabile qui: BPrintJob::ConfigJob()
	mostra il dialogo di stampa di sistema vero, che blocca un test
	automatico senza una stampante configurata (stesso limite gia'
	documentato per il resto del codice di stampa in questo progetto).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
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
	BApplication app("application/x-vnd.Atomo-TestPrintSettings");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();

	Check(!win->HasPrintArea(), "un documento nuovo non ha nessuna area di stampa");

	char text[64];
	win->PrintAreaText(text, sizeof(text));
	Check(text[0] == '\0', "PrintAreaText e' vuoto quando non c'e' nessuna area di stampa");

	// SetPrintArea prende la selezione CORRENTE della vista, non un
	// intervallo passato esplicitamente -- stesso principio gia' usato
	// da ShowChartWindow per precompilare l'intervallo del grafico.
	view->SetSelection(cell(2, 3));
	view->ExtendSelection(cell(4, 6));
	win->SetPrintArea();
	Check(win->HasPrintArea(), "SetPrintArea imposta l'area di stampa del foglio attivo");

	win->PrintAreaText(text, sizeof(text));
	Check(strcmp(text, "B3:D6") == 0,
		"PrintAreaText riporta esattamente la selezione usata da SetPrintArea (B3:D6)");

	win->ClearPrintArea();
	Check(!win->HasPrintArea(), "ClearPrintArea cancella l'area di stampa appena impostata");

	win->PrintAreaText(text, sizeof(text));
	Check(text[0] == '\0', "PrintAreaText torna vuoto dopo ClearPrintArea");

	// HandlePageSetupRequest (margini/scala/intestazioni): con gPrefs
	// NULL in questo harness (nessuna App::App() reale, vedi
	// test_preferences.cpp) la richiesta viene comunque registrata
	// PER FOGLIO in fSheets -- verificabile via GetActivePrintSettings
	// (stesso schema di fallback per-foglio/globale del codice vero).
	win->HandlePageSetupRequest(2.0, 2.0, 2.0, 2.0, 0, 100.0, true);
	Check(true, "HandlePageSetupRequest accetta margini/scala normali senza crash");

	win->HandlePageSetupRequest(0.0, 0.0, 0.0, 0.0, 3, 10.0, false);
	Check(true, "HandlePageSetupRequest accetta margini nulli e scala 'adatta a una pagina' senza crash");

	{
		double marginTop, marginBottom, marginLeft, marginRight, scalePercent;
		int scaleMode;
		bool printHeaders = true;
		win->GetActivePrintSettings(&marginTop, &marginBottom, &marginLeft, &marginRight,
			&scaleMode, &scalePercent, &printHeaders);
		Check(marginTop == 0.0 && marginBottom == 0.0 && marginLeft == 0.0 && marginRight == 0.0
				&& scaleMode == 3 && scalePercent == 10.0 && printHeaders == false,
			"GetActivePrintSettings riporta l'ultima impostazione per-foglio (margini 0, scala 3/10%, niente intestazioni)");
	}

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
