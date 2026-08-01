/*
	test_borders.cpp

	Verifica i bordi di cella (Fase 11, MainWindow::ToggleBorder/
	ClearBorders): CellStyle::fTBorderColor/fLBorderColor/
	fBBorderColor/fRBorderColor, mai implementati ne' nel Sum-It
	storico ne' nel motore moderno prima d'ora (vedi ROADMAP.md Fase
	11) -- qui trattati come un byte booleano per lato (0 = nessun
	bordo, diverso da 0 = bordo nero pieno), non un vero colore
	nonostante il nome del campo. ToggleBorder si applica a tutto
	SelectionRange() invertendo lo stato letto dalla sola cella
	attiva, stesso principio di ToggleBold/ToggleItalic (Fase 7).
	Richiede una vera MainWindow (i metodi sono pubblici apposta per
	essere testabili, stesso principio di CopySelection/PasteSelection).
*/

#include <cstdio>

#include <Application.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "SheetView.h"
#include "MainWindow.h"

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

int main()
{
	BApplication app("application/x-vnd.Atomo-TestBorders");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("1", cell(1, 1), doc, true); // A1
	TryToParseString("2", cell(2, 1), doc, true); // B1

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 1)); // A1:B1

	// Bordo superiore: nessuna cella ce l'ha all'inizio (attiva = B1),
	// quindi l'intera selezione lo ottiene.
	win->ToggleBorder(0);
	CellStyle cs;
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fTBorderColor != 0, "ToggleBorder(superiore) su A1:B1 lo applica ad A1, non solo all'attiva");
	doc->GetCellStyle(cell(2, 1), cs);
	Check(cs.fTBorderColor != 0, "e anche a B1 (l'attiva)");

	// Un secondo ToggleBorder sullo stesso lato/stessa selezione lo
	// toglie di nuovo da entrambe.
	win->ToggleBorder(0);
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fTBorderColor == 0, "un secondo ToggleBorder(superiore) lo toglie da tutta la selezione");

	// I quattro lati sono indipendenti: bordo sinistro su A1 non tocca
	// gli altri tre lati.
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));
	win->ToggleBorder(1); // sinistro
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fLBorderColor != 0 && cs.fTBorderColor == 0
		&& cs.fBBorderColor == 0 && cs.fRBorderColor == 0,
		"ToggleBorder(sinistro) tocca solo il lato sinistro, gli altri tre restano senza bordo");

	// Bordo inferiore e destro, aggiunti separatamente sulla stessa
	// cella: si accumulano, non si sostituiscono a vicenda.
	win->ToggleBorder(2); // inferiore
	win->ToggleBorder(3); // destro
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fLBorderColor != 0 && cs.fBBorderColor != 0 && cs.fRBorderColor != 0,
		"tre lati diversi aggiunti in sequenza convivono sulla stessa cella");

	// ClearBorders toglie tutti e quattro i lati in un colpo solo.
	win->ClearBorders();
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fTBorderColor == 0 && cs.fLBorderColor == 0
		&& cs.fBBorderColor == 0 && cs.fRBorderColor == 0,
		"ClearBorders toglie tutti e quattro i lati insieme");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
