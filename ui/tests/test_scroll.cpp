/*
	test_scroll.cpp

	Verifica che SheetView scorra automaticamente per mostrare la
	cella selezionata quando questa e' fuori dall'area visibile —
	bug segnalato dall'utente ("se mi sposto con il cursore e arrivo
	al bordo della pagina non avviene lo scroll") e riprodotto con
	questo stesso harness prima del fix.

	Causa non ovvia: SheetView::Frame() copre l'intero canvas virtuale
	del foglio (~56200x327700 pixel, vedi SheetView::FullCanvasFrame),
	non solo l'area visibile a schermo — pattern classico BeOS/Haiku
	per una vista scorrevole. Il problema e' che BScrollView, costruita
	con la forma classica (non tramite BLayoutBuilder), eredita di
	default la dimensione del proprio target invece di farsi vincolare
	dal layout: senza un ResizeTo() esplicito subito dopo la
	costruzione (vedi MainWindow::MainWindow), anche la BScrollView
	diventa enorme quanto SheetView, e Parent()->Bounds() — usato da
	SheetView::ScrollToShowSelection/FixupScrollBars per sapere quanto
	e' davvero visibile — riflette quella dimensione sbagliata invece
	della vera area visibile della finestra.

	Non passa dalla vera finestra MainWindow (per isolare la sola
	logica di scroll): costruisce una finestra e una BScrollView
	minime, riproducendo pero' esattamente lo stesso pattern
	(BScrollView classica + ResizeTo esplicito) usato in
	MainWindow::MainWindow. Richiede una sessione grafica reale (il
	Layout Kit passa dall'app_server) — target Makefile separato,
	come test-clipboard.
*/

#include <cstdio>

#include <Application.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
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
		: BWindow(BRect(100, 100, 700, 500), "test-scroll", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestScroll");

	CContainer* doc = new CContainer(NULL, NULL);
	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);

	// Lo stesso ResizeTo esplicito usato in MainWindow::MainWindow --
	// senza, BScrollView eredita la dimensione enorme del target.
	scroll->ResizeTo(400, 300);

	BLayoutBuilder::Group<>(win, B_VERTICAL, 0)
		.Add(scroll);
	win->Show();

	win->Lock();

	BRect viewport = view->Parent()->Bounds();
	Check(viewport.Width() < 1000 && viewport.Height() < 1000,
		"la BScrollView ha una dimensione ragionevole (non eredita il canvas virtuale del target)");

	cell start = view->Selection();
	Check(start.h == 1 && start.v == 1, "la selezione iniziale e' A1");

	BRect boundsBefore = view->Bounds();
	Check(boundsBefore.left == 0 && boundsBefore.top == 0,
		"nessuno scroll prima di selezionare una cella lontana");

	// Cella fuori dall'area visibile iniziale: deve far scorrere la vista.
	view->SetSelection(cell(30, 200));

	BRect boundsAfter = view->Bounds();
	Check(boundsAfter.left > 0 || boundsAfter.top > 0,
		"la vista scorre per mostrare una cella selezionata fuori schermo");

	// Tornando alla cella iniziale la vista deve scorrere indietro.
	view->SetSelection(cell(1, 1));
	BRect boundsBack = view->Bounds();
	Check(boundsBack.left == 0 && boundsBack.top == 0,
		"la vista torna all'origine selezionando di nuovo A1");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
