/*
	test_image_autoscroll.cpp

	Verifica che trascinare (o ridimensionare) un'immagine incorporata
	oltre il bordo dell'area visibile faccia scorrere il foglio per
	seguirla (Fase 17, richiesta esplicita dell'utente dopo aver
	rinunciato al trascinamento d'esportazione: "se con il tasto
	sinistro la sposto e vado oltre alla dimensione della gui vorrei che
	il foglio di calcolo si spostasse mostrandomi sempre l'immagine").

	Stesso schema di test_image_drag.cpp (BScrollView reale di
	dimensione nota, chiamate dirette a MouseDown/MouseMoved/MouseUp).
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
		: BWindow(BRect(100, 100, 900, 700), "test-image-autoscroll", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestImageAutoscroll");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	std::vector<EmbeddedImage> images;
	{
		EmbeddedImage img;
		img.anchor = cell(2, 2); // B2, ben dentro l'area visibile iniziale
		img.offsetX = 10; img.offsetY = 5;
		img.width = 60; img.height = 40;
		img.pngData.push_back(0x89);
		images.push_back(img);
	}
	view->SetImages(&images);

	BRect boundsBefore = view->Bounds();
	Check(boundsBefore.left == 0 && boundsBefore.top == 0,
		"il foglio parte non scorso (in cima a sinistra)");

	// --- Trascinare l'immagine ben oltre il bordo destro/inferiore
	// dell'area visibile (la BScrollView e' larga 700x500) deve far
	// scorrere il foglio per seguirla, non lasciarla sparire fuori
	// dallo schermo. ---
	BRect frame0 = view->ImageFrame(images[0]);
	BPoint insideFirst(frame0.left + 5, frame0.top + 5);
	view->MouseDown(insideFirst);
	view->MouseMoved(insideFirst + BPoint(900, 700), B_INSIDE_VIEW, NULL);

	BRect boundsAfterDrag = view->Bounds();
	Check(boundsAfterDrag.left > 0 && boundsAfterDrag.top > 0,
		"trascinando l'immagine oltre il bordo destro/inferiore il foglio scorre per seguirla");

	BRect frameAfterDrag = view->ImageFrame(images[0]);
	BRect viewport(boundsAfterDrag.left, boundsAfterDrag.top,
		boundsAfterDrag.left + 700, boundsAfterDrag.top + 500);
	Check(viewport.Contains(frameAfterDrag.RightBottom()),
		"dopo lo scorrimento l'angolo dell'immagine trascinata e' di nuovo visibile");

	view->MouseUp(insideFirst + BPoint(900, 700));

	// --- Stesso principio, ma trascinando indietro verso l'alto a
	// sinistra oltre l'origine: il foglio deve tornare a scorrere in
	// quella direzione, non restare bloccato dove si trovava. ---
	BRect frame1 = view->ImageFrame(images[0]);
	BPoint insideNow(frame1.left + 5, frame1.top + 5);
	view->MouseDown(insideNow);
	view->MouseMoved(insideNow + BPoint(-2000, -2000), B_INSIDE_VIEW, NULL);

	BRect boundsAfterBack = view->Bounds();
	Check(boundsAfterBack.left == 0 && boundsAfterBack.top == 0,
		"trascinando indietro fino a oltre l'origine il foglio scorre di nuovo in cima a sinistra");
	view->MouseUp(insideNow + BPoint(-2000, -2000));

	// --- Stesso principio per il ridimensionamento: trascinare la
	// maniglia oltre il bordo visibile deve far scorrere il foglio,
	// non lasciare la maniglia irraggiungibile. Riporta prima
	// l'immagine a uno scarto ragionevole (i due trascinamenti sopra
	// l'hanno lasciata a uno scarto molto negativo, fuori foglio in
	// alto a sinistra -- comportamento gia' corretto e voluto per un
	// trascinamento vero, ma non il punto di partenza giusto per
	// isolare QUESTO scenario). ---
	images[0].offsetX = 10;
	images[0].offsetY = 5;
	BRect handle = view->ImageResizeHandle(images[0]);
	BPoint handlePoint(handle.left + 1, handle.top + 1);
	view->MouseDown(handlePoint);
	view->MouseMoved(handlePoint + BPoint(900, 700), B_INSIDE_VIEW, NULL);

	BRect boundsAfterResize = view->Bounds();
	Check(boundsAfterResize.left > 0 && boundsAfterResize.top > 0,
		"trascinando la maniglia di ridimensionamento oltre il bordo il foglio scorre per seguirla");
	view->MouseUp(handlePoint + BPoint(900, 700));

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
