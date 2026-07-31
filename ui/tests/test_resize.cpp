/*
	test_resize.cpp

	Verifica il ridimensionamento riga/colonna (SheetView::MouseDown/
	MouseMoved/MouseUp trascinando un confine di intestazione),
	ultimo dei quattro punti scelti dall'utente per la Fase 8 (qualita'
	UI/UX): la larghezza/altezza di ogni colonna/riga era finora fissa
	(kColWidth/kRowHeight per tutte), ora e' un array per colonna/riga
	modificabile trascinando il confine fra due intestazioni.

	Limite noto (documentato anche in SheetView.h): il ridimensionamento
	vale solo per la sessione corrente, non e' salvato nel file .ascd.

	Coordinate della griglia usate qui (stesse gia' note e verificate
	nel resto della suite, es. test_selection.cpp): intestazioni a
	kHeaderWidth=40/kHeaderHeight=20, colonne/righe di default
	kColWidth=80/kRowHeight=20 -- valori privati a SheetView, non
	esposti, quindi ripresi qui per costruire le coordinate del mouse
	esattamente come negli altri test di questa suite.
*/

#include <cstdio>
#include <cstring>

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

	// Trascina il confine fra A1 e B1 (a x=40+80=120, dentro la banda
	// dell'intestazione di colonna, y=10) verso destra di 30 pixel:
	// A1 deve allargarsi, B1 deve spostarsi a destra della stessa
	// quantita' senza cambiare larghezza.
	view->MouseDown(BPoint(120, 10));
	view->MouseMoved(BPoint(150, 10), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(150, 10));

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
	view->MouseDown(BPoint(120 + 30, 10)); // confine fra A1 (ora 110 larga) e B1
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

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
