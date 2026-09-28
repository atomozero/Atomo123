/*
	test_chart_row_orientation.cpp

	Verifica il nuovo interruttore "Scambia righe/colonne" nell'editor
	dei grafici (richiesta esplicita dell'utente, "possiamo migliorare
	ancora di piu' l'editor dei grafici?"): espone SOLO il caso semplice
	di Excel (stesso intervallo rettangolare, l'asse opposto diventa le
	categorie, righe contigue implicite) -- l'elenco di righe non
	contigue (ChartObject::valueRows quando popolato dall'importazione
	XLSX) resta un campo di sola importazione, mai esposto da questa
	finestra.

	Stesso schema di test_edit_chart.cpp: MainWindow vera,
	HandleChartInsert/HandleChartUpdate chiamati direttamente (il
	checkbox stesso e' solo un bool passato a un parametro, non serve
	simulare un vero clic per testare la logica che conta).

	Punto piu' importante di questo file: il "prima" catturato da
	SaveChartEditUndoState e' l'INTERO ChartObject (vedi
	SheetView::UndoSnapshot::isChartEditSnapshot, gia' esistente prima
	di questa modifica), quindi Annulla/Ripristina devono funzionare
	per rowOriented/valueRows senza NESSUN codice nuovo per l'undo --
	verificato esplicitamente sotto, non solo assunto.
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
	BApplication app("application/x-vnd.Atomo-TestChartRowOrientation");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer* doc = win->GetSheetView()->Document();

	// Intervallo "per riga" (A1:D3): riga 1 = categorie (B1:D1), colonna
	// A = nome serie, righe 2-3 = le due serie -- stesso schema
	// documentato in Chart.h per un grafico importato da XLSX, qui
	// costruito a mano per il caso "creato dall'editor".
	TryToParseString("Gen", cell(2, 1), doc, true);
	TryToParseString("Feb", cell(3, 1), doc, true);
	TryToParseString("Mar", cell(4, 1), doc, true);
	TryToParseString("Vendite", cell(1, 2), doc, true);
	TryToParseString("10", cell(2, 2), doc, true);
	TryToParseString("20", cell(3, 2), doc, true);
	TryToParseString("30", cell(4, 2), doc, true);
	TryToParseString("Costi", cell(1, 3), doc, true);
	TryToParseString("5", cell(2, 3), doc, true);
	TryToParseString("15", cell(3, 3), doc, true);
	TryToParseString("25", cell(4, 3), doc, true);

	win->HandleChartInsert("A1:D3", "F1", eBarChart, "Per riga", /*rowOriented=*/true);
	Check(win->Charts().size() == 1, "HandleChartInsert con rowOriented=true aggiunge un grafico");
	if (win->Charts().size() == 1)
	{
		const ChartObject& obj = win->Charts()[0];
		Check(obj.rowOriented == true, "il grafico ricorda rowOriented=true");
		Check(obj.valueRows.size() == 2 && obj.valueRows[0] == 2 && obj.valueRows[1] == 3,
			"valueRows e' [2,3] (le due righe serie, MAI la riga 1 di categoria)");
	}

	// Aggiornamento: torna a colonna (rowOriented=false) sullo stesso
	// intervallo -- ora pero' l'intervallo A1:D3 non ha piu' la forma
	// giusta per il percorso a colonna (4 colonne, nessuna riga
	// numerica valida nella prima colonna B come valori). Usiamo invece
	// un secondo aggiornamento su un intervallo diverso, a due colonne,
	// per verificare che il flag torni a spegnersi correttamente.
	win->HandleChartUpdate(0, "A1:D3", eLineChart, "Ancora per riga", /*rowOriented=*/true);
	Check(win->Charts()[0].type == eLineChart, "HandleChartUpdate cambia il tipo mantenendo l'orientamento");
	Check(win->Charts()[0].rowOriented == true, "rowOriented resta true quando la casella resta spuntata");

	// La casella "Scambia righe/colonne" e' l'unica fonte di verita' per
	// rowOriented/valueRows una volta che l'editor tocca un grafico:
	// spegnerla (anche con lo stesso testo di intervallo) li azzera
	// SEMPRE, mai "preservati" come farebbe valueColumns.
	win->HandleChartUpdate(0, "A1:D3", eLineChart, "Ancora per riga", /*rowOriented=*/false);
	Check(win->Charts()[0].rowOriented == false,
		"spegnere la casella azzera rowOriented anche se l'intervallo (testo) non cambia");
	Check(win->Charts()[0].valueRows.empty(),
		"spegnere la casella azzera anche valueRows");

	// Annullabile SENZA nessun codice nuovo per l'undo (vedi il
	// commento in cima al file): riaccende rowOriented per poi
	// verificare che Annulla/Ripristina lo riportino com'era.
	win->HandleChartUpdate(0, "A1:D3", eBarChart, "Per riga di nuovo", /*rowOriented=*/true);
	Check(win->Charts()[0].rowOriented == true, "rowOriented riacceso prima del giro annulla/ripristina");

	win->GetSheetView()->Undo();
	Check(win->Charts()[0].rowOriented == false,
		"Annulla ripristina rowOriented=false (stato prima dell'ultimo aggiornamento)");
	Check(win->Charts()[0].valueRows.empty(), "Annulla ripristina anche valueRows vuoto");

	win->GetSheetView()->Redo();
	Check(win->Charts()[0].rowOriented == true,
		"Ripristina riapplica rowOriented=true appena annullato");
	Check(win->Charts()[0].valueRows.size() == 2,
		"Ripristina riapplica anche valueRows");

	// Un normale grafico a colonne (rowOriented mai toccato, il caso
	// comune) continua a funzionare esattamente come prima di questa
	// modifica -- stessa identica chiamata di test_insert_chart.cpp,
	// qui per confermare che il nuovo parametro predefinito a false non
	// ha cambiato nulla per chi non lo usa.
	win->HandleChartInsert("A1:D3", "F10", eBarChart, "Grafico normale");
	Check(win->Charts().size() == 2, "un HandleChartInsert senza rowOriented (default) continua a funzionare");
	if (win->Charts().size() == 2)
		Check(win->Charts()[1].rowOriented == false,
			"il valore predefinito di rowOriented resta false quando non specificato");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
