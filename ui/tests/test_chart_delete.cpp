/*
	test_chart_delete.cpp

	Verifica la cancellazione di un grafico incorporato (Fase 22,
	richiesta esplicita dell'utente: "posso cancellare i grafici dal
	foglio?"): un clic seleziona il grafico (fSelectedChartIndex,
	riquadro blu in Draw()), Canc/Backspace lo rimuove dal vettore.
	Selezionare una cella deve invece deselezionare il grafico, come in
	Excel e come gia' vale per le immagini (test_image_delete.cpp,
	stesso schema qui). Verifica in piu' (non presente per le sole
	immagini): selezionare un grafico deseleziona un'eventuale immagine
	selezionata e viceversa, cosi' Canc non e' mai ambiguo su quale dei
	due cancellare.
*/

#include <cstdio>
#include <vector>

#include <Application.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "Chart.h"
#include "EmbeddedImage.h"
#include "SheetView.h"

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

class TestWindow : public BWindow {
public:
	TestWindow()
		: BWindow(BRect(100, 100, 900, 700), "test-chart-delete", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestChartDelete");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	std::vector<ChartObject> charts;
	{
		ChartObject obj;
		obj.frame = BRect(100, 100, 250, 200);
		obj.type = eBarChart;
		charts.push_back(obj);
	}
	{
		ChartObject obj;
		obj.frame = BRect(400, 400, 550, 500); // ben separato dal primo
		obj.type = ePieChart;
		charts.push_back(obj);
	}
	view->SetCharts(&charts);

	Check(!view->HasSelectedChart(), "nessun grafico selezionato all'avvio");

	// --- Canc SENZA aver mai cliccato un grafico cancella la cella
	// selezionata come sempre, non fa nulla ai grafici. ---
	view->HandleKey(B_DELETE, false, false);
	Check(charts.size() == 2, "Canc senza selezione di grafico non tocca il vettore grafici");

	// --- Un clic dentro il primo grafico lo seleziona (senza
	// spostarlo, MouseUp nello stesso punto). ---
	BPoint insideFirst(charts[0].frame.left + 5, charts[0].frame.top + 5);
	view->MouseDown(insideFirst);
	view->MouseUp(insideFirst);
	Check(view->HasSelectedChart() && view->SelectedChartIndex() == 0,
		"un clic dentro un grafico lo seleziona (indice 0)");

	// --- Selezionare una cella (non un grafico) deseleziona il
	// grafico, come in Excel. ---
	view->SetSelection(cell(1, 1));
	Check(!view->HasSelectedChart(),
		"selezionare una cella deseleziona il grafico appena selezionato");

	// --- Ricliccare il primo grafico e premere Canc lo rimuove dal
	// vettore, senza toccare il secondo. ---
	view->MouseDown(insideFirst);
	view->MouseUp(insideFirst);
	Check(view->HasSelectedChart() && view->SelectedChartIndex() == 0,
		"riclic sul primo grafico: di nuovo selezionato");
	bool handled = view->HandleKey(B_DELETE, false, false);
	Check(handled, "Canc su un grafico selezionato e' un tasto gestito");
	Check(charts.size() == 1, "Canc rimuove il grafico selezionato dal vettore");
	Check(charts[0].frame == BRect(400, 400, 550, 500) && charts[0].type == ePieChart,
		"il secondo grafico (torta) e' l'unico rimasto dopo la cancellazione del primo");
	Check(!view->HasSelectedChart(),
		"dopo la cancellazione, nessun grafico resta selezionato");

	// --- Annulla/Ripristina: la cancellazione deve essere annullabile
	// come ogni altra mutazione (stesso principio di test_image_delete.cpp). ---
	Check(view->CanUndo(), "dopo aver cancellato un grafico, Annulla e' disponibile");
	view->Undo();
	Check(charts.size() == 2, "Annulla ripristina il grafico cancellato nel vettore");
	Check(charts[0].frame == BRect(100, 100, 250, 200) && charts[0].type == eBarChart,
		"il grafico ripristinato torna al suo frame/tipo originali (indice 0)");

	Check(view->CanRedo(), "dopo Annulla, Ripristina e' disponibile");
	view->Redo();
	Check(charts.size() == 1, "Ripristina rimuove di nuovo il grafico");
	Check(charts[0].type == ePieChart, "dopo Ripristina resta solo il secondo grafico (torta)");

	// --- Dopo Ripristina, un ulteriore Annulla deve ancora funzionare
	// (la ri-cancellazione ha ricatturato dati completi, non solo
	// l'indice). ---
	view->Undo();
	Check(charts.size() == 2,
		"un secondo Annulla dopo Ripristina funziona ancora (dati completi ricatturati da Redo)");

	// --- Selezionare un grafico deseleziona un'eventuale immagine
	// selezionata, e viceversa: i due indici non devono mai restare
	// entrambi validi insieme (altrimenti Canc sarebbe ambiguo). ---
	std::vector<EmbeddedImage> images;
	{
		EmbeddedImage img;
		img.anchor = cell(20, 20); // ben lontana da entrambi i grafici
		img.offsetX = 0; img.offsetY = 0;
		img.width = 50; img.height = 30;
		img.pngData.push_back(0x89);
		images.push_back(img);
	}
	view->SetImages(&images);

	BRect imgFrame = view->ImageFrame(images[0]);
	BPoint insideImage(imgFrame.left + 3, imgFrame.top + 3);
	view->MouseDown(insideImage);
	view->MouseUp(insideImage);
	Check(view->HasSelectedImage() && !view->HasSelectedChart(),
		"selezionare un'immagine parte da nessun grafico selezionato");

	BPoint insideChart0(charts[0].frame.left + 5, charts[0].frame.top + 5);
	view->MouseDown(insideChart0);
	view->MouseUp(insideChart0);
	Check(view->HasSelectedChart() && !view->HasSelectedImage(),
		"selezionare un grafico deseleziona l'immagine selezionata in precedenza");

	view->MouseDown(insideImage);
	view->MouseUp(insideImage);
	Check(view->HasSelectedImage() && !view->HasSelectedChart(),
		"riselezionare l'immagine deseleziona a sua volta il grafico");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
