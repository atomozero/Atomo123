/*
	test_insert_chart.cpp

	Verifica MainWindow::HandleChartInsert (menu Inserisci > Grafico):
	nessuna copertura diretta esisteva prima di questo test -- vedi
	memoria project_command_audit_20260808. A differenza di
	tests/test_chart.cpp (che verifica solo la logica pura di
	Chart.h/.cpp: BuildChartSeries/ComputeBarLayout ecc., senza mai
	passare da MainWindow), qui si verifica il vero COMANDO che
	aggiunge un grafico al documento: entra a far parte di fCharts (non
	solo un'anteprima), sopravvive a un secondo inserimento, e reagisce
	correttamente a un intervallo/destinazione non validi.

	HandleChartInsert e' pubblico apposta per essere testabile
	direttamente (stesso principio di CopySelection/HandlePasteSpecialRequest
	in test_paste_special.cpp), richiede una vera MainWindow.

	Limite noto (non testabile in automatico): un intervallo/una
	destinazione non validi mostrano un vero BAlert d'errore, che
	bloccherebbe questo test in attesa di un clic reale -- stesso
	limite gia' documentato in test_unsaved_changes.cpp per
	ConfirmDiscardChanges. Qui si verifica solo che il grafico NON
	venga aggiunto in quel caso (fCharts resta invariato), non il
	dialogo stesso.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "Chart.h"

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
	BApplication app("application/x-vnd.Atomo-TestInsertChart");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer* doc = win->GetSheetView()->Document();

	TryToParseString("Gen", cell(1, 1), doc, true);
	TryToParseString("10", cell(2, 1), doc, true);
	TryToParseString("Feb", cell(1, 2), doc, true);
	TryToParseString("20", cell(2, 2), doc, true);
	TryToParseString("Mar", cell(1, 3), doc, true);
	TryToParseString("30", cell(2, 3), doc, true);

	Check(win->Charts().empty(), "un documento appena creato non ha nessun grafico");

	win->HandleChartInsert("A1:B3", "E1", eBarChart);
	Check(win->Charts().size() == 1, "HandleChartInsert con un intervallo/destinazione validi aggiunge un grafico");
	if (win->Charts().size() == 1)
	{
		Check(win->Charts()[0].type == eBarChart, "il grafico aggiunto ha il tipo scelto (barre)");
		Check(win->Charts()[0].dataRange.TopLeft() == cell(1, 1)
				&& win->Charts()[0].dataRange.BotRight() == cell(2, 3),
			"il grafico aggiunto ricorda l'intervallo dati scelto (A1:B3)");
	}

	// Un secondo grafico si aggiunge, non sostituisce il primo.
	win->HandleChartInsert("A1:B3", "E10", ePieChart);
	Check(win->Charts().size() == 2, "un secondo Inserisci Grafico si aggiunge, non sostituisce il primo");
	if (win->Charts().size() == 2)
		Check(win->Charts()[1].type == ePieChart, "il secondo grafico ha il tipo scelto (torta), diverso dal primo");

	// Intervallo non valido (una sola colonna, non etichetta+valore):
	// nessun grafico in piu' viene aggiunto.
	size_t beforeInvalid = win->Charts().size();
	win->HandleChartInsert("A1:A3", "E20", eBarChart);
	Check(win->Charts().size() == beforeInvalid,
		"un intervallo dati non valido (una sola colonna) non aggiunge nessun grafico");

	// Cella di destinazione non valida (testo non analizzabile come
	// riferimento a cella): nessun grafico in piu'.
	win->HandleChartInsert("A1:B3", "NonUnaCella", eBarChart);
	Check(win->Charts().size() == beforeInvalid,
		"una destinazione non valida non aggiunge nessun grafico");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
