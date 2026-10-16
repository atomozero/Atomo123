/*
	test_control_resize.cpp

	Verifica il ridimensionamento con il mouse di un controllo ActiveX
	incorporato (import XLSX, EmbeddedControl.h): estensione naturale
	dello spostamento gia' testato in test_control_drag.cpp. A
	differenza del corpo del controllo (vedi la soglia di trascinamento
	in test_control_drag.cpp), la maniglia di ridimensionamento NON ha
	nessuna soglia -- non esiste un significato di "clic" per la
	maniglia (niente evento VBA alternativo da scegliere), quindi si
	comporta esattamente come ImageResizeHandle: il ridimensionamento
	parte SUBITO al primo MouseMoved, stesso schema di
	test_image_resize.cpp.
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
		: BWindow(BRect(100, 100, 900, 700), "test-control-resize", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestControlResize");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Due controlli ben separati, stesso principio di
	// test_image_resize.cpp: ridimensionare il primo non deve toccare
	// il secondo.
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
		ctrl.anchor = cell(2, 10); // molto piu' in basso, nessuna sovrapposizione
		ctrl.offsetX = 10; ctrl.offsetY = 5;
		ctrl.width = 60; ctrl.height = 40;
		ctrl.name = "cb1";
		ctrl.caption = "1";
		controls.push_back(ctrl);
	}
	view->SetControls(&controls);

	BRect handle0Before = view->ControlResizeHandle(controls[0]);
	BRect frame0Before = view->ControlFrame(controls[0]);

	// --- Un clic FUORI dalla maniglia e dal corpo del controllo non
	// ridimensiona ne' sposta nulla. ---
	BPoint farAway(500, 400);
	Check(!frame0Before.Contains(farAway),
		"il punto di controllo scelto e' davvero fuori dal controllo");
	view->MouseDown(farAway);
	view->MouseMoved(BPoint(520, 420), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(520, 420));
	Check(controls[0].width == 60 && controls[0].height == 40,
		"un clic fuori dal controllo non lo ridimensiona (resta alla sua dimensione originale)");

	// --- Un clic sulla maniglia (angolo in basso a destra) avvia il
	// ridimensionamento SUBITO, senza nessuna soglia (a differenza del
	// corpo, vedi test_control_drag.cpp): dopo il trascinamento width/
	// height cambiano ma offsetX/offsetY restano gli stessi. ---
	BPoint onHandle(handle0Before.left + 2, handle0Before.top + 2);
	Check(frame0Before.Contains(onHandle),
		"la maniglia scelta cade davvero dentro il corpo del controllo (dove si sovrappongono)");

	view->MouseDown(onHandle);
	view->MouseMoved(onHandle + BPoint(15, 10), B_INSIDE_VIEW, NULL);
	Check(controls[0].width == 75 && controls[0].height == 50,
		"la larghezza/altezza si aggiorna gia' al primo MouseMoved del trascinamento, senza nessuna soglia");
	Check(controls[0].offsetX == 10 && controls[0].offsetY == 5,
		"trascinare la maniglia cambia solo width/height, non offsetX/offsetY (niente spostamento)");

	view->MouseMoved(onHandle + BPoint(25, 5), B_INSIDE_VIEW, NULL);
	Check(controls[0].width == 85 && controls[0].height == 45,
		"un secondo MouseMoved ridimensiona rispetto al punto di PARTENZA del trascinamento, non in modo incrementale");

	view->MouseUp(onHandle + BPoint(25, 5));
	Check(controls[1].width == 60 && controls[1].height == 40
		&& controls[1].offsetX == 10 && controls[1].offsetY == 5,
		"ridimensionare il primo controllo non tocca il secondo");

	// --- Annulla/Ripristina: il blocco sopra e' un UNICO trascinamento
	// (un solo MouseDown/MouseUp) -- Annulla riporta alla dimensione di
	// PRIMA del MouseDown (60x40), non a quella intermedia (75x50). ---
	Check(view->CanUndo(), "dopo aver ridimensionato un controllo, Annulla e' disponibile");
	view->Undo();
	Check(controls[0].width == 60 && controls[0].height == 40,
		"Annulla riporta il primo controllo alla dimensione di PRIMA dell'intero ridimensionamento (60x40), non a una intermedia");
	Check(view->CanRedo(), "dopo Annulla, Ripristina e' disponibile");
	view->Redo();
	Check(controls[0].width == 85 && controls[0].height == 45,
		"Ripristina riapplica il ridimensionamento appena annullato");

	// --- Il ridimensionamento non puo' andare sotto un minimo. ---
	BRect handle0Now = view->ControlResizeHandle(controls[0]);
	BPoint onHandleNow(handle0Now.left + 2, handle0Now.top + 2);
	view->MouseDown(onHandleNow);
	view->MouseMoved(onHandleNow + BPoint(-500, -500), B_INSIDE_VIEW, NULL);
	Check(controls[0].width >= 10 && controls[0].height >= 10,
		"il ridimensionamento non scende mai sotto il minimo, anche trascinando molto oltre");
	view->MouseUp(onHandleNow + BPoint(-500, -500));

	// --- Un clic dentro il corpo del controllo ma FUORI dalla maniglia
	// arma lo spostamento (non il ridimensionamento): verifica solo che
	// i due percorsi restino distinti, usando un trascinamento oltre la
	// soglia (vedi test_control_drag.cpp) per renderlo effettivo. ---
	controls[0].width = 60; controls[0].height = 40;
	controls[0].offsetX = 10; controls[0].offsetY = 5;
	BRect frame0Reset = view->ControlFrame(controls[0]);
	BPoint insideNotHandle(frame0Reset.left + 3, frame0Reset.top + 3);
	Check(!view->ControlResizeHandle(controls[0]).Contains(insideNotHandle),
		"il punto di controllo scelto cade dentro il controllo ma fuori dalla maniglia");
	view->MouseDown(insideNotHandle);
	view->MouseMoved(insideNotHandle + BPoint(17, 13), B_INSIDE_VIEW, NULL); // oltre la soglia
	view->MouseUp(insideNotHandle + BPoint(17, 13));
	Check(controls[0].width == 60 && controls[0].height == 40,
		"un clic fuori dalla maniglia (ma dentro il controllo) sposta, non ridimensiona: width/height invariate");
	Check(controls[0].offsetX == 27 && controls[0].offsetY == 18,
		"un clic fuori dalla maniglia (ma dentro il controllo) sposta: offsetX/offsetY cambiano");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
