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

	Include anche il sottolineato (Fase 12, MainWindow::
	ToggleUnderline): CellStyle::fUnderline, un booleano a parte
	(BFont non ha un attributo sottolineato nativo), stesso principio
	di ToggleBorder sopra -- aggiunto qui invece che in un file a
	parte per non duplicare il setup di MainWindow/documento.
*/

#include <cstdio>
#include <utility>
#include <vector>

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

	// Sottolineato (Fase 12): stesso principio di ToggleBorder sopra,
	// ma un booleano semplice (nessun "lato").
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 1)); // A1:B1
	win->ToggleUnderline();
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fUnderline, "ToggleUnderline su A1:B1 lo applica ad A1, non solo all'attiva");
	doc->GetCellStyle(cell(2, 1), cs);
	Check(cs.fUnderline, "e anche a B1 (l'attiva)");

	win->ToggleUnderline();
	doc->GetCellStyle(cell(1, 1), cs);
	Check(!cs.fUnderline, "un secondo ToggleUnderline lo toglie da tutta la selezione");

	// A capo automatico (Fase 12): stesso principio di ToggleUnderline
	// sopra, ma con l'effetto collaterale di far crescere l'altezza
	// della riga se il testo non entra piu' su una riga sola alla
	// larghezza di colonna corrente.
	{
		std::vector<std::pair<int, float> > narrowCol;
		narrowCol.push_back(std::make_pair(1, 40.0f)); // colonna 1 stretta
		view->SetColumnWidths(narrowCol);

		TryToParseString("Testo lungo che sicuramente non entra su una riga sola",
			cell(1, 3), doc, true); // A3

		view->SetSelection(cell(1, 3));
		view->ExtendSelection(cell(1, 3));
		win->ToggleWrapText();
		doc->GetCellStyle(cell(1, 3), cs);
		Check(cs.fWrapText, "ToggleWrapText imposta CellStyle::fWrapText su A3");

		std::vector<std::pair<int, float> > heights = view->CustomRowHeights();
		bool row3Grew = false;
		for (size_t i = 0; i < heights.size(); i++)
			if (heights[i].first == 3 && heights[i].second > 20.0f)
				row3Grew = true;
		Check(row3Grew,
			"l'altezza della riga 3 cresce oltre il predefinito per contenere il testo a capo");
	}

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
