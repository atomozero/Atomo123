/*
	test_clear_keeps_style.cpp

	Il tasto Canc/Backspace su una cella (SheetView::ClearSelection)
	svuotava valore/formula E la formattazione (colore, bordo,
	grassetto, ecc.), perche' passava da CContainer::DisposeCell, che
	cancella l'intera entry dalla mappa comprensiva di mStyle. Bug
	reale segnalato dall'utente: cancellare il contenuto di una cella
	colorata/bordata faceva sparire anche colore e bordo, non solo il
	contenuto -- comportamento diverso da Excel/LibreOffice Calc, dove
	Canc cancella solo il contenuto ("Clear Contents"), mai la
	formattazione (serve "Clear Formats" a parte per quello). Fissato
	con un nuovo CContainer::ClearCellContent, che libera testo/formula
	ma lascia mStyle intatto -- vedi il commento su quel metodo in
	Container.h/.cpp.
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
	BApplication app("application/x-vnd.Atomo-TestClearKeepsStyle");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("42", cell(1, 1), doc, true); // A1

	CellStyle cs;
	doc->GetCellStyle(cell(1, 1), cs);
	cs.fLowColor = (rgb_color){255, 255, 0, 255};
	cs.fTBorderColor = 2;
	doc->SetCellStyle(cell(1, 1), cs);

	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fLowColor.red == 255 && cs.fLowColor.green == 255 && cs.fLowColor.blue == 0,
		"A1 e' davvero gialla prima della cancellazione (verifica del banco di prova)");
	Check(cs.fTBorderColor == 2, "A1 ha davvero un bordo superiore prima della cancellazione");

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));
	view->ClearSelection();

	Value v;
	doc->GetValue(cell(1, 1), v);
	Check(v.fType == eNoData, "dopo Canc, A1 non ha piu' nessun valore");

	char formula[256];
	doc->GetCellFormula(cell(1, 1), formula, sizeof(formula), false);
	Check(formula[0] == 0, "dopo Canc, A1 non ha piu' nessuna formula");

	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fLowColor.red == 255 && cs.fLowColor.green == 255 && cs.fLowColor.blue == 0,
		"dopo Canc, A1 e' ancora gialla: la formattazione sopravvive alla cancellazione del contenuto");
	Check(cs.fTBorderColor == 2,
		"dopo Canc, A1 ha ancora il suo bordo superiore");

	// Annulla dopo Canc ripristina anche il contenuto (gia' testato in
	// test_undo.cpp per il caso senza formattazione): qui basta
	// verificare che il colore non sia mai andato perso nel giro,
	// quindi non serve nessun ripristino esplicito per lui.
	view->Undo();
	doc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 42.0,
		"Annulla dopo Canc ripristina anche il valore (42)");
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fLowColor.red == 255 && cs.fLowColor.green == 255 && cs.fLowColor.blue == 0,
		"...e il colore e' ancora quello, sia prima sia dopo Annulla");

	// Una cella mai esistita non deve crashare (nulla da svuotare).
	view->SetSelection(cell(5, 5));
	view->ExtendSelection(cell(5, 5));
	view->ClearSelection();
	doc->GetValue(cell(5, 5), v);
	Check(v.fType == eNoData, "Canc su una cella mai esistita non crasha e resta vuota");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	win->Unlock();
	win->Lock();
	win->Quit();

	return gFailures == 0 ? 0 : 1;
}
