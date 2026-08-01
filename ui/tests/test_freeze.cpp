/*
	test_freeze.cpp

	Verifica Blocca riquadri (Fase 7, SheetView::ToggleFreezePanes/
	SetFreezePanes): righe/colonne "congelate" restano ferme sullo
	schermo durante lo scroll invece di scorrere col resto del foglio
	-- stessa tecnica gia' usata per le intestazioni di riga/colonna
	(vedi il commento su ScrollTo() in SheetView.cpp), estesa al
	contenuto delle celle. Richiede una vera sessione grafica (il
	Layout Kit passa dall'app_server) -- stessa ragione di test-scroll,
	di cui questo file riusa lo stesso schema di finestra di prova.

	Copre in particolare due bug scoperti implementando questa
	funzionalita', non ovvi da un solo sguardo al codice:
	- SheetView::CellAt() interpretava un clic sulla banda congelata
	  (che dopo uno scroll si trova a una posizione sullo schermo
	  diversa dalla sua posizione "naturale" nel canvas virtuale) come
	  se fosse caduto sulla cella che si troverebbe li' SENZA il
	  blocco -- serve "riportare indietro" il punto sottraendo lo
	  scroll corrente, l'esatto inverso di come Draw() la disegna.
	- SheetView::MouseDown() scartava un clic sull'intestazione usando
	  un confronto con 0 fisso invece di Bounds().top/.left (che le
	  intestazioni, "congelate" allo stesso modo, seguono durante lo
	  scroll): un clic sull'intestazione dopo aver scorso il foglio
	  (ma non su una vera maniglia di ridimensionamento) selezionava
	  una cella quasi a caso invece di non fare nulla. Bug preesistente,
	  scoperto solo ora perche' Blocca riquadri tocca lo stesso identico
	  codice.
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
#include "CellParser.h"
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
		: BWindow(BRect(100, 100, 700, 500), "test-freeze", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestFreeze");

	CContainer* doc = new CContainer(NULL, NULL);
	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(400, 300);

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

	Check(!view->HasFreezePanes(), "un documento appena aperto non ha nulla di bloccato");

	// ToggleFreezePanes usa la cella attiva come Excel: seleziona C3
	// (colonna 3, riga 3), blocca le 2 righe e le 2 colonne PRIMA di
	// essa.
	view->SetSelection(cell(3, 3));
	view->ToggleFreezePanes();
	Check(view->HasFreezePanes() && view->FrozenRows() == 2 && view->FrozenCols() == 2,
		"ToggleFreezePanes su C3 blocca 2 righe e 2 colonne (tutto cio' che sta sopra/a sinistra)");

	view->ToggleFreezePanes();
	Check(!view->HasFreezePanes(), "un secondo ToggleFreezePanes sblocca di nuovo tutto");

	// SetFreezePanes non esce mai dai limiti validi, qualunque cosa gli
	// si passi.
	view->SetFreezePanes(-5, -5);
	Check(!view->HasFreezePanes(), "SetFreezePanes con valori negativi si blocca a 0 (nessun blocco)");

	view->SetFreezePanes(999999, 999999);
	Check(view->HasFreezePanes() && view->FrozenRows() > 0 && view->FrozenCols() > 0,
		"SetFreezePanes con valori enormi si blocca comunque a un valore valido, non crasha");

	// Righe/colonne bloccate note in anticipo (dal formato nativo, non
	// derivate dalla selezione corrente).
	view->SetFreezePanes(2, 2);
	Check(view->FrozenRows() == 2 && view->FrozenCols() == 2,
		"SetFreezePanes(2, 2) applica esattamente i valori indicati");

	// Una cella bloccata e' sempre visibile: selezionarla non deve mai
	// far scorrere la vista, anche se il foglio e' gia' scorso lontano
	// (altrimenti selezionarla vanificherebbe il blocco riportando
	// sempre la vista in cima/a sinistra).
	view->SetSelection(cell(30, 200)); // fuori dall'area visibile, fa scorrere
	Check(view->Bounds().left > 0 || view->Bounds().top > 0,
		"selezionare una cella lontana fa comunque scorrere la vista (invariato)");

	BRect boundsBeforeFrozenSelect = view->Bounds();
	view->SetSelection(cell(1, 1)); // dentro l'area bloccata (righe/colonne 1-2)
	Check(view->Bounds() == boundsBeforeFrozenSelect,
		"selezionare una cella bloccata (A1) non fa scorrere la vista: e' gia' sempre visibile");

	// CellAt() deve risolvere correttamente un clic sulla banda
	// bloccata anche con la vista scorsa (bug descritto in cima al
	// file): la posizione "incollata allo schermo" della cella A1
	// (bloccata su entrambi gli assi) e' la sua posizione naturale
	// spostata dello scroll corrente su entrambi gli assi -- lo stesso
	// spostamento con cui Draw() la disegna li'.
	BRect boundsScrolled = view->Bounds();
	Check(boundsScrolled.left > 0 && boundsScrolled.top > 0,
		"la vista e' ancora scorsa su entrambi gli assi per il resto della verifica");

	BRect naturalA1 = view->CellRect(cell(1, 1));
	BPoint pinnedA1Point(naturalA1.left + boundsScrolled.left + 2,
		naturalA1.top + boundsScrolled.top + 2);
	Check(view->CellAt(pinnedA1Point) == cell(1, 1),
		"un clic sulla posizione bloccata di A1 (angolo, righe E colonne congelate) risolve ad A1, "
		"non alla cella che si troverebbe li' senza il blocco");

	// Colonna 1 (bloccata), riga 5 (NON bloccata, fuori dalle 2 righe
	// congelate): solo l'asse orizzontale e' "incollato".
	BRect naturalA5 = view->CellRect(cell(1, 5));
	BPoint pinnedA5Point(naturalA5.left + boundsScrolled.left + 2, naturalA5.top + 2);
	Check(view->CellAt(pinnedA5Point) == cell(1, 5),
		"un clic sulla banda bloccata di sole colonne (A5) risolve alla cella corretta");

	// Un clic sull'intestazione dopo lo scroll (non su una maniglia di
	// ridimensionamento) non deve selezionare nulla di diverso -- bug
	// preesistente, non specifico di Blocca riquadri, descritto in
	// cima al file.
	cell beforeHeaderClick = view->Selection();
	view->MouseDown(BPoint(boundsScrolled.left + 5, boundsScrolled.top + 5));
	Check(view->Selection() == beforeHeaderClick,
		"un clic sull'angolo delle intestazioni (dopo lo scroll) non cambia la selezione");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
