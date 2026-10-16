/*
	test_control_drag.cpp

	Verifica lo spostamento con il mouse di un controllo ActiveX
	incorporato (import XLSX, EmbeddedControl.h) -- stesso schema di
	test_image_drag.cpp, con una differenza fondamentale: un clic sul
	corpo di un controllo NON sposta subito (come un'immagine), resta
	solo ARMATO (fPendingControlIndex) finche' il mouse non supera una
	soglia di 10px (kControlDragThreshold, SheetView.cpp), perche' un
	clic SENZA superare quella soglia deve invece invocare un gestore
	evento VBA (_MouseUp/_Click), non spostare nulla -- vedi Fase 7 in
	ROADMAP.md. Qui si verifica solo la geometria/Annulla-Ripristina:
	DispatchControlEvent ricade innocuamente su "nessuna finestra
	MainWindow" (TestWindow qui sotto e' un semplice BWindow), quindi
	ogni evento VBA tentato non fa mai nulla, senza bisogno di un vero
	progetto VBA per questo test.
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
#include "EmbeddedControl.h"
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
		: BWindow(BRect(100, 100, 900, 700), "test-control-drag", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestControlDrag");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Due controlli che si sovrappongono parzialmente, stesso principio
	// di test_image_drag.cpp: il secondo (ultimo nell'elenco, disegnato
	// sopra) deve avere la precedenza su un clic nella zona comune.
	std::vector<EmbeddedControl> controls;
	{
		EmbeddedControl ctrl;
		ctrl.anchor = cell(2, 2); // B2
		ctrl.offsetX = 10; ctrl.offsetY = 5;
		ctrl.width = 60; ctrl.height = 40;
		ctrl.name = "cb0";
		ctrl.caption = "0";
		controls.push_back(ctrl);
	}
	{
		EmbeddedControl ctrl;
		ctrl.anchor = cell(2, 2); // stessa cella di ancoraggio
		ctrl.offsetX = 40; ctrl.offsetY = 25; // si sovrappone alla prima
		ctrl.width = 60; ctrl.height = 40;
		ctrl.name = "cb1";
		ctrl.caption = "1";
		controls.push_back(ctrl);
	}
	view->SetControls(&controls);

	BRect frame0Before = view->ControlFrame(controls[0]);
	BRect frame1Before = view->ControlFrame(controls[1]);

	// --- Un clic FUORI da qualunque controllo non afferra nulla. ---
	BPoint farAway(500, 400);
	Check(!frame0Before.Contains(farAway) && !frame1Before.Contains(farAway),
		"il punto di controllo scelto e' davvero fuori da entrambi i controlli");
	view->MouseDown(farAway);
	view->MouseMoved(BPoint(520, 420), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(520, 420));
	Check(controls[0].offsetX == 10 && controls[0].offsetY == 5,
		"un clic fuori da ogni controllo non sposta il primo (resta al suo scarto originale)");
	Check(controls[1].offsetX == 40 && controls[1].offsetY == 25,
		"un clic fuori da ogni controllo non sposta il secondo (resta al suo scarto originale)");
	Check(!view->HasSelectedControl(), "un clic fuori da ogni controllo non seleziona nulla");

	// --- Un clic nella zona di sovrapposizione, SENZA superare la
	// soglia di trascinamento: seleziona il secondo controllo (sopra)
	// ma non lo sposta affatto -- resta "armato" finche' non si supera
	// la soglia. ---
	BPoint overlapPoint(frame1Before.left + 5, frame1Before.top + 5);
	Check(frame0Before.Contains(overlapPoint) && frame1Before.Contains(overlapPoint),
		"il punto di controllo scelto cade davvero dentro entrambi i controlli sovrapposti");

	view->MouseDown(overlapPoint);
	Check(view->HasSelectedControl() && view->SelectedControlIndex() == 1,
		"il clic nella zona di sovrapposizione seleziona il secondo controllo (sopra), non il primo");
	view->MouseMoved(overlapPoint + BPoint(3, 2), B_INSIDE_VIEW, NULL); // sotto la soglia di 10px
	Check(controls[1].offsetX == 40 && controls[1].offsetY == 25,
		"sotto la soglia di trascinamento il controllo resta armato, non si sposta ancora");
	view->MouseUp(overlapPoint + BPoint(3, 2));
	Check(controls[1].offsetX == 40 && controls[1].offsetY == 25,
		"un clic (anche con un piccolo tremore sotto soglia) senza mai superarla non sposta il controllo");
	Check(!view->CanUndo(),
		"un clic senza vero trascinamento non lascia nessuna istantanea nella pila di Annulla");

	// --- Un trascinamento che supera la soglia sposta davvero il
	// secondo controllo, lasciando il primo (sotto) invariato. ---
	view->MouseDown(overlapPoint);
	view->MouseMoved(overlapPoint + BPoint(30, 20), B_INSIDE_VIEW, NULL); // oltre la soglia di 10px
	Check(controls[0].offsetX == 10 && controls[0].offsetY == 5,
		"il primo controllo (sotto) non si muove quando si trascina quello sopra nella zona di sovrapposizione");
	Check(controls[1].offsetX == 70 && controls[1].offsetY == 45,
		"il secondo controllo (sopra) si sposta esattamente dello spostamento del mouse (+30,+20) una volta superata la soglia");
	view->MouseUp(overlapPoint + BPoint(30, 20));

	// --- Trascinare per intero (non solo il punto finale) il primo
	// controllo, senza sovrapposizione: aggiorna la posizione a ogni
	// MouseMoved (una volta superata la soglia), rispetto al punto di
	// PARTENZA del trascinamento, non in modo incrementale. ---
	BRect frame0Now = view->ControlFrame(controls[0]);
	BPoint insideFirst(frame0Now.left + 5, frame0Now.top + 5);
	view->MouseDown(insideFirst);
	view->MouseMoved(insideFirst + BPoint(12, 9), B_INSIDE_VIEW, NULL); // oltre la soglia
	Check(controls[0].offsetX == 22 && controls[0].offsetY == 14,
		"la posizione si aggiorna gia' al primo MouseMoved che supera la soglia, non solo al rilascio");
	view->MouseMoved(insideFirst + BPoint(25, 15), B_INSIDE_VIEW, NULL);
	Check(controls[0].offsetX == 35 && controls[0].offsetY == 20,
		"un secondo MouseMoved sposta rispetto al punto di PARTENZA del trascinamento, non in modo incrementale");
	view->MouseUp(insideFirst + BPoint(25, 15));
	Check(controls[1].offsetX == 70 && controls[1].offsetY == 45,
		"trascinare il primo controllo non tocca il secondo");

	// --- Annulla/Ripristina: lo stesso trascinamento sopra e' un UNICO
	// MouseDown/MouseUp (i due MouseMoved intermedi non creano istantanee
	// separate) -- Annulla riporta allo scarto di PRIMA del MouseDown
	// (10,5), non a uno intermedio. ---
	Check(view->CanUndo(), "dopo aver trascinato un controllo, Annulla e' disponibile");
	view->Undo();
	Check(controls[0].offsetX == 10 && controls[0].offsetY == 5,
		"Annulla riporta il primo controllo allo scarto di PRIMA dell'intero trascinamento (10,5), non a uno intermedio");
	Check(view->CanRedo(), "dopo Annulla, Ripristina e' disponibile");
	view->Redo();
	Check(controls[0].offsetX == 35 && controls[0].offsetY == 20,
		"Ripristina riapplica lo spostamento appena annullato");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
