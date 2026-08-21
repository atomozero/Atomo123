/*
	test_chart_resize.cpp

	Verifica il ridimensionamento con il mouse di un grafico incorporato
	(Fase 20, richiesta esplicita dell'utente: "vorrei la possibilita'
	di ridimensionare il grafico una volta inserito nel foglio").
	Stesso schema di test_image_resize.cpp (indice + punto di partenza +
	frame di partenza, armati da MouseDown, applicati da MouseMoved), ma
	su ChartObject::frame (gia' un BRect assoluto, non ancora+scarto
	come le immagini) invece di width/height separati -- il ridimensio-
	namento cambia solo right/bottom, l'angolo in alto a sinistra resta
	fermo.
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
		: BWindow(BRect(100, 100, 900, 700), "test-chart-resize", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestChartResize");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Due grafici finti (nessun dato reale servito, il ridimensionamento
	// non legge mai fDoc, stesso principio di test_chart_drag.cpp): il
	// primo con un frame noto, il secondo ben separato dal primo, cosi'
	// da verificare che ridimensionare uno non tocchi l'altro.
	std::vector<ChartObject> charts;
	{
		ChartObject obj;
		obj.frame = BRect(100, 100, 250, 200);
		obj.type = eBarChart;
		charts.push_back(obj);
	}
	{
		ChartObject obj;
		obj.frame = BRect(400, 400, 550, 500); // ben separato, nessuna sovrapposizione
		obj.type = eBarChart;
		charts.push_back(obj);
	}
	view->SetCharts(&charts);

	BRect handle0Before = view->ChartResizeHandle(charts[0]);
	BRect frame0Before = charts[0].frame;

	// --- Un clic FUORI dalla maniglia e dal corpo del grafico non
	// ridimensiona ne' sposta nulla. ---
	BPoint farAway(700, 600);
	Check(!frame0Before.Contains(farAway),
		"il punto di controllo scelto e' davvero fuori dal grafico");
	view->MouseDown(farAway);
	view->MouseMoved(BPoint(720, 620), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(720, 620));
	Check(charts[0].frame == frame0Before,
		"un clic fuori dal grafico non lo ridimensiona (resta al suo frame originale)");

	// --- Un clic sulla maniglia (angolo in basso a destra) avvia il
	// ridimensionamento, non lo spostamento: dopo il trascinamento
	// right/bottom cambiano ma left/top restano gli stessi. ---
	BPoint onHandle(handle0Before.left + 2, handle0Before.top + 2);
	Check(frame0Before.Contains(onHandle),
		"la maniglia scelta cade davvero dentro il corpo del grafico (dove si sovrappongono)");

	view->MouseDown(onHandle);
	view->MouseMoved(onHandle + BPoint(15, 10), B_INSIDE_VIEW, NULL);
	Check(charts[0].frame.right == frame0Before.right + 15
			&& charts[0].frame.bottom == frame0Before.bottom + 10,
		"right/bottom si aggiornano gia' al primo MouseMoved del trascinamento, non solo al rilascio");
	Check(charts[0].frame.left == frame0Before.left && charts[0].frame.top == frame0Before.top,
		"trascinare la maniglia cambia solo right/bottom, non left/top (niente spostamento)");

	view->MouseMoved(onHandle + BPoint(25, 5), B_INSIDE_VIEW, NULL);
	Check(charts[0].frame.right == frame0Before.right + 25
			&& charts[0].frame.bottom == frame0Before.bottom + 5,
		"un secondo MouseMoved ridimensiona rispetto al punto di PARTENZA del trascinamento, non in modo incrementale");

	view->MouseUp(onHandle + BPoint(25, 5));
	Check(charts[1].frame == BRect(400, 400, 550, 500),
		"ridimensionare il primo grafico non tocca il secondo");

	// --- Annulla/Ripristina: un ridimensionamento di grafico deve
	// essere annullabile come uno spostamento (stesso meccanismo,
	// SaveChartUndoState). Il blocco sopra e' un UNICO trascinamento
	// (un solo MouseDown/MouseUp): Annulla riporta quindi al frame di
	// PRIMA del MouseDown, non a uno intermedio. ---
	Check(view->CanUndo(), "dopo aver ridimensionato un grafico, Annulla e' disponibile");
	view->Undo();
	Check(charts[0].frame == frame0Before,
		"Annulla riporta il primo grafico al frame di PRIMA dell'intero ridimensionamento, non a uno intermedio");
	Check(view->CanRedo(), "dopo Annulla, Ripristina e' disponibile");
	view->Redo();
	Check(charts[0].frame == BRect(100, 100, 275, 205),
		"Ripristina riapplica il ridimensionamento appena annullato");

	// --- Il ridimensionamento non puo' andare sotto un minimo:
	// trascinare la maniglia verso l'alto/sinistra oltre il minimo
	// blocca il frame al minimo invece di farlo diventare invertito o
	// nullo. ---
	BRect handle0Now = view->ChartResizeHandle(charts[0]);
	BPoint onHandleNow(handle0Now.left + 2, handle0Now.top + 2);
	view->MouseDown(onHandleNow);
	view->MouseMoved(onHandleNow + BPoint(-500, -500), B_INSIDE_VIEW, NULL);
	Check(charts[0].frame.Width() >= 39 && charts[0].frame.Height() >= 39,
		"il ridimensionamento non scende mai sotto il minimo, anche trascinando molto oltre");
	Check(charts[0].frame.left == 100 && charts[0].frame.top == 100,
		"anche al minimo, l'angolo in alto a sinistra resta fermo");
	view->MouseUp(onHandleNow + BPoint(-500, -500));

	// --- Un clic dentro il corpo del grafico ma FUORI dalla maniglia
	// continua a spostarlo (comportamento gia' testato in
	// test_chart_drag.cpp), non a ridimensionarlo: verifica solo che i
	// due percorsi restino distinti dopo l'aggiunta della maniglia. ---
	charts[0].frame = BRect(100, 100, 250, 200);
	BPoint insideNotHandle(charts[0].frame.left + 3, charts[0].frame.top + 3);
	Check(!view->ChartResizeHandle(charts[0]).Contains(insideNotHandle),
		"il punto di controllo scelto cade dentro il grafico ma fuori dalla maniglia");
	view->MouseDown(insideNotHandle);
	view->MouseMoved(insideNotHandle + BPoint(7, 3), B_INSIDE_VIEW, NULL);
	view->MouseUp(insideNotHandle + BPoint(7, 3));
	Check(charts[0].frame == BRect(107, 103, 257, 203),
		"un clic fuori dalla maniglia (ma dentro il grafico) sposta l'intero frame, non lo ridimensiona");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
