/*
	test_chart_drag.cpp

	Verifica lo spostamento con il mouse (tasto sinistro) di un grafico
	incorporato (Fase 17, richiesta esplicita dell'utente: "possiamo
	spostare i grafici generati all'interno del foglio?"). Stesso
	schema di test_image_drag.cpp, ma su ChartObject::frame (un BRect
	gia' assoluto in Chart.h, niente ancora/scarto come EmbeddedImage)
	invece che sull'offset di un'immagine.
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
		: BWindow(BRect(100, 100, 900, 700), "test-chart-drag", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestChartDrag");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Due grafici finti (nessun dato reale servito, lo spostamento non
	// legge mai fDoc): il primo con un frame noto, il secondo che si
	// sovrappone in parte al primo -- verifica che un clic nella zona
	// di sovrapposizione afferri quello disegnato sopra (l'ultimo
	// nell'elenco), stesso principio gia' testato per le immagini.
	std::vector<ChartObject> charts;
	{
		ChartObject obj;
		obj.frame = BRect(100, 100, 250, 200);
		obj.type = eBarChart;
		charts.push_back(obj);
	}
	{
		ChartObject obj;
		obj.frame = BRect(200, 150, 350, 250); // si sovrappone al primo
		obj.type = ePieChart;
		charts.push_back(obj);
	}
	view->SetCharts(&charts);

	// --- Un clic FUORI da qualunque grafico non afferra nulla. ---
	BPoint farAway(500, 400);
	Check(!charts[0].frame.Contains(farAway) && !charts[1].frame.Contains(farAway),
		"il punto di controllo scelto e' davvero fuori da entrambi i grafici");
	BRect frame0Before = charts[0].frame;
	BRect frame1Before = charts[1].frame;
	view->MouseDown(farAway);
	view->MouseMoved(BPoint(520, 420), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(520, 420));
	Check(charts[0].frame == frame0Before,
		"un clic fuori da ogni grafico non sposta il primo");
	Check(charts[1].frame == frame1Before,
		"un clic fuori da ogni grafico non sposta il secondo");

	// --- Un clic nella zona di sovrapposizione afferra il secondo
	// grafico (disegnato sopra, ultimo nell'elenco), non il primo. ---
	BPoint overlapPoint(220, 170);
	Check(charts[0].frame.Contains(overlapPoint) && charts[1].frame.Contains(overlapPoint),
		"il punto di controllo scelto cade davvero dentro entrambi i grafici sovrapposti");

	view->MouseDown(overlapPoint);
	view->MouseMoved(overlapPoint + BPoint(30, 20), B_INSIDE_VIEW, NULL);
	view->MouseUp(overlapPoint + BPoint(30, 20));

	Check(charts[0].frame == frame0Before,
		"il primo grafico (sotto) non si muove quando si trascina quello sopra");
	Check(charts[1].frame == frame1Before.OffsetByCopy(30, 20),
		"il secondo grafico (sopra) si sposta esattamente dello spostamento del mouse (+30,+20)");

	// --- Trascinare senza sovrapposizione aggiorna la posizione a ogni
	// MouseMoved, non solo al rilascio -- stesso principio gia' testato
	// per le immagini. ---
	BPoint insideFirst(120, 120);
	view->MouseDown(insideFirst);
	view->MouseMoved(insideFirst + BPoint(5, 0), B_INSIDE_VIEW, NULL);
	Check(charts[0].frame == frame0Before.OffsetByCopy(5, 0),
		"la posizione si aggiorna gia' al primo MouseMoved, non solo al rilascio");
	view->MouseMoved(insideFirst + BPoint(12, 8), B_INSIDE_VIEW, NULL);
	Check(charts[0].frame == frame0Before.OffsetByCopy(12, 8),
		"un secondo MouseMoved sposta rispetto al punto di PARTENZA del trascinamento, non in modo incrementale");
	view->MouseUp(insideFirst + BPoint(12, 8));

	// --- Annulla/Ripristina: uno spostamento di grafico deve essere
	// annullabile come ogni altra mutazione. ---
	Check(view->CanUndo(), "dopo aver trascinato un grafico, Annulla e' disponibile");
	view->Undo();
	Check(charts[0].frame == frame0Before,
		"Annulla riporta il primo grafico al frame di PRIMA dell'intero trascinamento");
	Check(view->CanRedo(), "dopo Annulla, Ripristina e' disponibile");
	view->Redo();
	Check(charts[0].frame == frame0Before.OffsetByCopy(12, 8),
		"Ripristina riapplica lo spostamento appena annullato");

	// --- Un semplice clic senza trascinare non deve riempire la pila
	// di Annulla con un'istantanea inutile. ---
	BRect frame0Now = charts[0].frame;
	BPoint insideFirstNow(frame0Now.left + 5, frame0Now.top + 5);
	view->MouseDown(insideFirstNow);
	view->MouseMoved(insideFirstNow, B_INSIDE_VIEW, NULL); // stesso punto
	view->MouseUp(insideFirstNow);
	view->Undo();
	Check(charts[0].frame == frame0Before,
		"un clic senza trascinare non ha aggiunto un'istantanea: Annulla salta dritto al trascinamento vero precedente");
	view->Redo();

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
