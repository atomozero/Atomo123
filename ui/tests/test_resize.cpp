/*
	test_resize.cpp

	Verifica il ridimensionamento riga/colonna (SheetView::MouseDown/
	MouseMoved/MouseUp trascinando un confine di intestazione),
	ultimo dei quattro punti scelti dall'utente per la Fase 8 (qualita'
	UI/UX): la larghezza/altezza di ogni colonna/riga era finora fissa
	(kColWidth/kRowHeight per tutte), ora e' un array per colonna/riga
	modificabile trascinando il confine fra due intestazioni.

	Verifica anche SetColumnWidths/CustomColumnWidths, l'API con cui
	MainWindow applica/cattura le larghezze di colonna al cambio di
	foglio/documento e al salvataggio nel formato nativo (Fase 9): il
	ridimensionamento trascinando un confine, sopra, e l'applicazione
	programmatica di una larghezza letta da un file, qui, condividono
	lo stesso array fColWidths.

	Coordinate della griglia usate qui (stesse gia' note e verificate
	nel resto della suite, es. test_selection.cpp): intestazioni a
	kHeaderWidth=30/kHeaderHeight=20, colonne/righe di default
	kColWidth=80/kRowHeight=20 -- valori privati a SheetView, non
	esposti, quindi ripresi qui per costruire le coordinate del mouse
	esattamente come negli altri test di questa suite.
*/

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <Application.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
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
		: BWindow(BRect(100, 100, 900, 700), "test-resize", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestResize");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(600, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Stato iniziale: A1 e B1 hanno entrambe la larghezza predefinita
	// (80), quindi B1 inizia esattamente dove finisce A1 (nessuna
	// sovrapposizione ne' spazio vuoto fra le due).
	BRect a1 = view->CellRect(cell(1, 1));
	BRect b1 = view->CellRect(cell(2, 1));
	Check(a1.Width() == 80 && b1.left == a1.right,
		"A1 e B1 hanno la larghezza predefinita e sono contigue");

	// Trascina il confine fra A1 e B1 (a x=30+80=110, dentro la banda
	// dell'intestazione di colonna, y=10) verso destra di 30 pixel:
	// A1 deve allargarsi, B1 deve spostarsi a destra della stessa
	// quantita' senza cambiare larghezza.
	view->MouseDown(BPoint(110, 10));
	view->MouseMoved(BPoint(140, 10), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(140, 10));

	BRect a1After = view->CellRect(cell(1, 1));
	BRect b1After = view->CellRect(cell(2, 1));
	Check(a1After.Width() == 110, "trascinare il confine di 30px allarga A1 a 110 (80+30)");
	Check(b1After.Width() == 80, "B1 mantiene la sua larghezza predefinita (80)");
	Check(b1After.left == a1After.right,
		"B1 resta contigua ad A1 anche dopo l'allargamento (nessun buco ne' sovrapposizione)");

	// Colonne piu' a destra (non toccate) restano alla larghezza
	// predefinita, solo spostate a destra di conseguenza.
	BRect c1After = view->CellRect(cell(3, 1));
	Check(c1After.Width() == 80 && c1After.left == b1After.right,
		"C1 (non toccata) mantiene la larghezza predefinita, solo spostata a destra");

	// Non si puo' stringere una colonna sotto la larghezza minima:
	// trascinare il confine molto a sinistra (oltre il bordo iniziale
	// della colonna) la blocca comunque a un valore minimo positivo,
	// non a zero ne' a un valore negativo.
	view->MouseDown(BPoint(140, 10)); // confine fra A1 (ora 110 larga) e B1
	view->MouseMoved(BPoint(10, 10), B_INSIDE_VIEW, NULL); // molto a sinistra
	view->MouseUp(BPoint(10, 10));

	BRect a1Min = view->CellRect(cell(1, 1));
	Check(a1Min.Width() == 20,
		"stringere oltre il limite blocca la larghezza al minimo (20), non a zero o sotto zero");

	// CellAt (hit-testing) riflette le nuove larghezze: un punto dentro
	// la A1 ridimensionata deve ancora mappare alla colonna 1, non
	// "scivolare" nella colonna successiva per via del confine spostato.
	BRect a1Final = view->CellRect(cell(1, 1));
	BPoint midA1(a1Final.left + a1Final.Width() / 2, 15);
	cell atPoint = view->CellAt(midA1);
	Check(atPoint.h == 1 && atPoint.v == 1,
		"CellAt individua ancora la colonna 1 per un punto dentro la A1 ridimensionata");

	// Ridimensionamento riga (intestazione a sinistra, banda verticale
	// invece che orizzontale): trascina il confine fra la riga 1 e la
	// riga 2 (a y=20+20=40, dentro la colonna delle intestazioni di
	// riga, x=10) verso il basso di 15 pixel.
	BRect row1Before = view->CellRect(cell(1, 1));
	view->MouseDown(BPoint(10, 40));
	view->MouseMoved(BPoint(10, 55), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(10, 55));

	BRect row1After = view->CellRect(cell(1, 1));
	Check(row1After.Height() == row1Before.Height() + 15,
		"trascinare il confine di riga di 15px allarga l'altezza della riga 1");

	BRect row2After = view->CellRect(cell(1, 2));
	Check(row2After.top == row1After.bottom,
		"la riga 2 resta contigua alla riga 1 anche dopo l'allargamento");

	// SetColumnWidths/CustomColumnWidths (Fase 9): l'API con cui
	// MainWindow applica le larghezze lette da un file importato (o dal
	// foglio precedentemente attivo) e cattura quelle correnti prima di
	// cambiare foglio/documento o salvare.
	std::vector<std::pair<int, float> > custom;
	custom.push_back(std::make_pair(2, 150.0f));
	custom.push_back(std::make_pair(5, 40.0f));
	view->SetColumnWidths(custom);

	Check(view->CellRect(cell(2, 1)).Width() == 150,
		"SetColumnWidths applica la larghezza indicata per la colonna 2");
	Check(view->CellRect(cell(5, 1)).Width() == 40,
		"SetColumnWidths applica la larghezza indicata per la colonna 5");
	Check(view->CellRect(cell(1, 1)).Width() == 80,
		"SetColumnWidths riporta alla larghezza predefinita le colonne non indicate "
		"(A1, allargata in precedenza trascinando, torna a 80)");

	std::vector<std::pair<int, float> > captured = view->CustomColumnWidths();
	Check(captured.size() == 2, "CustomColumnWidths restituisce solo le colonne non predefinite");
	if (captured.size() == 2)
	{
		Check(captured[0].first == 2 && captured[0].second == 150.0f,
			"CustomColumnWidths riporta la colonna 2 con la larghezza applicata");
		Check(captured[1].first == 5 && captured[1].second == 40.0f,
			"CustomColumnWidths riporta la colonna 5 con la larghezza applicata");
	}

	// Una larghezza sotto il minimo viene comunque bloccata al minimo,
	// come per il trascinamento diretto verificato sopra.
	std::vector<std::pair<int, float> > tooSmall;
	tooSmall.push_back(std::make_pair(1, 2.0f));
	view->SetColumnWidths(tooSmall);
	Check(view->CellRect(cell(1, 1)).Width() == 20,
		"SetColumnWidths blocca comunque una larghezza troppo piccola al minimo (20)");

	// Un vettore vuoto riporta tutte le colonne alla larghezza
	// predefinita -- il caso di un documento nuovo o di un foglio senza
	// nessuna colonna personalizzata.
	view->SetColumnWidths(std::vector<std::pair<int, float> >());
	Check(view->CellRect(cell(1, 1)).Width() == 80 && view->CustomColumnWidths().empty(),
		"un vettore vuoto riporta tutte le colonne alla larghezza predefinita");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
