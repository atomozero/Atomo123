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
	logica di scroll): costruisce una finestra con un layout a piu'
	righe simile (menu, barra strumenti, barra formula, poi la
	griglia), riproducendo lo stesso pattern (BScrollView classica +
	ResizeTo esplicito) usato in MainWindow::MainWindow. Richiede una
	sessione grafica reale (il Layout Kit passa dall'app_server) —
	target Makefile separato, come test-clipboard.

	Nota su un secondo bug della stessa famiglia (parte 2 sotto): il
	controllo "dopo un ridimensionamento della finestra" qui sotto
	NON riproduce quel bug in questo harness (passa anche senza il
	fix) -- la diagnosi e la verifica di quel fix sono state fatte
	dal vivo sull'app vera (vedi docs/UI_ARCHITECTURE.md), interrogando
	Frame() della BScrollView con `hey <app> get Frame of View "scroll"
	of Window 0` prima e dopo il fix. Il controllo resta qui come
	guardia aggiuntiva onesta (non fa mai male verificarlo), non come
	prova che il bug specifico sia coperto da questo test.
*/

#include <cstdio>

#include <Application.h>
#include <Button.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
#include "SheetView.h"

static int gFailures = 0;
static const char kRightArrow = B_RIGHT_ARROW;
static const char kDownArrow = B_DOWN_ARROW;

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

	// Un layout con la sola BScrollView (una versione precedente di
	// questo test) non riproduceva il bug della parte 2 sotto: serve
	// un layout a piu' righe sopra la griglia (menu, barra strumenti,
	// barra formula), come in MainWindow::MainWindow, perche' il
	// ricalcolo del layout con piu' elementi si comporti allo stesso
	// modo dell'app vera.
	BMenuBar* menuBar = new BMenuBar("menu");
	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("Esci", NULL));
	menuBar->AddItem(fileMenu);

	BButton* toolButton = new BButton("tool", "Nuovo", NULL);
	BStringView* cellLabel = new BStringView("cellLabel", "A1");
	BTextControl* formulaBar = new BTextControl("formula", NULL, "", NULL);

	BLayoutBuilder::Group<>(win, B_VERTICAL, 0)
		.Add(menuBar)
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(4, 4, 4, 4)
			.Add(toolButton)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(4, 4, 4, 4)
			.Add(cellLabel)
			.Add(formulaBar)
		.End()
		.Add(scroll);
	win->Show();

	win->Lock();

	BRect viewport = view->Parent()->Bounds();
	Check(viewport.Width() < 1000 && viewport.Height() < 1000,
		"la BScrollView ha una dimensione ragionevole (non eredita il canvas virtuale del target)");

	// Il bug originale non si manifestava alla primissima passata di
	// layout (dove il ResizeTo() esplicito sulla BScrollView "mascherava"
	// il problema): serviva un secondo ricalcolo del layout, come capita
	// nell'uso reale a ogni ridimensionamento della finestra. Senza un
	// limite esplicito di dimensione impostato su SheetView stessa (non
	// solo un ResizeTo() una tantum sulla BScrollView), il layout
	// tornava a interrogare il Frame() enorme del target a ogni nuovo
	// ricalcolo, riportando Parent()->Bounds() alla dimensione ereditata
	// -- bug realmente segnalato dall'utente e non riprodotto da questo
	// stesso test prima di aggiungere questo controllo.
	win->ResizeTo(900, 600);
	BRect viewportAfterResize = view->Parent()->Bounds();
	Check(viewportAfterResize.Width() < 2000 && viewportAfterResize.Height() < 2000,
		"la BScrollView resta di dimensione ragionevole anche dopo un "
		"ridimensionamento della finestra (non torna alla dimensione ereditata "
		"dal target a un ricalcolo successivo del layout)");

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

	// La segnalazione originale dell'utente era sulla navigazione con
	// le frecce, non su un salto diretto a una cella lontana (il caso
	// gia' testato sopra con SetSelection): SheetView::KeyDown chiama
	// SetSelection internamente per ogni pressione, quindi in teoria
	// e' esattamente lo stesso percorso di codice -- ma qui lo si
	// verifica con incrementi di una cella alla volta (com'e' la
	// tastiera vera), non con un solo salto, per escludere bug legati
	// a passi ripetuti (es. arrotondamenti, stato che non si aggiorna
	// tra una pressione e la successiva).
	bool scrolledRight = false;
	for (int i = 0; i < 40 && !scrolledRight; i++)
	{
		view->KeyDown(&kRightArrow, 1);
		if (view->Bounds().left > 0)
			scrolledRight = true;
	}
	Check(scrolledRight,
		"premendo Right ripetutamente la vista scorre orizzontalmente");

	bool scrolledDown = false;
	for (int i = 0; i < 320 && !scrolledDown; i++)
	{
		view->KeyDown(&kDownArrow, 1);
		if (view->Bounds().top > 0)
			scrolledDown = true;
	}
	Check(scrolledDown,
		"premendo Down ripetutamente la vista scorre verticalmente");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
