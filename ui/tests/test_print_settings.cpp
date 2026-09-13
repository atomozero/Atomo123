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

	// HandlePageSetupRequest (tutte le impostazioni): con gPrefs
	// NULL in questo harness (nessuna App::App() reale, vedi
	// test_preferences.cpp) la richiesta viene comunque registrata
	// PER FOGLIO in fSheets -- verificabile via GetActivePrintSettings
	// (stesso schema di fallback per-foglio/globale del codice vero).
	AscdPrintSettings settings;
	win->HandlePageSetupRequest(settings);
	Check(true, "HandlePageSetupRequest accetta le impostazioni predefinite senza crash");

	settings.marginTopCm = 0.0;
	settings.marginBottomCm = 0.0;
	settings.marginLeftCm = 0.0;
	settings.marginRightCm = 0.0;
	settings.scaleMode = 3;
	settings.scalePercent = 10.0;
	settings.printHeaders = false;
	settings.printGrid = false;
	win->HandlePageSetupRequest(settings);
	Check(true, "HandlePageSetupRequest accetta margini nulli e scala 'adatta a una pagina' senza crash");

	{
		AscdPrintSettings active;
		win->GetActivePrintSettings(&active);
		Check(active.marginTopCm == 0.0 && active.marginBottomCm == 0.0
				&& active.marginLeftCm == 0.0 && active.marginRightCm == 0.0
				&& active.scaleMode == 3 && active.scalePercent == 10.0
				&& active.printHeaders == false && active.printGrid == false,
			"GetActivePrintSettings riporta l'ultima impostazione per-foglio "
			"(margini 0, scala 3/10%, niente intestazioni ne' griglia)");
	}

	// Adatta a N x M pagine: la struct viaggia intera, compresi i due
	// conteggi (stesso schema dei flag sopra, nessun parametro in piu').
	settings.scaleMode = 4; // kPrintFitPages
	settings.fitWide = 2;
	settings.fitTall = 3;
	win->HandlePageSetupRequest(settings);
	{
		AscdPrintSettings active;
		win->GetActivePrintSettings(&active);
		Check(active.scaleMode == 4 && active.fitWide == 2 && active.fitTall == 3,
			"GetActivePrintSettings riporta modo 4 con 2 pagine di larghezza e 3 di altezza");
	}

	// Centratura: due flag come gli altri, default spenti.
	{
		AscdPrintSettings active;
		win->GetActivePrintSettings(&active);
		Check(active.centerH == false && active.centerV == false,
			"senza richiesta esplicita la centratura resta spenta (default)");
	}
	settings.centerH = true;
	settings.centerV = true;
	win->HandlePageSetupRequest(settings);
	{
		AscdPrintSettings active;
		win->GetActivePrintSettings(&active);
		Check(active.centerH == true && active.centerV == true,
			"GetActivePrintSettings riporta la centratura orizzontale+verticale");
	}

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
