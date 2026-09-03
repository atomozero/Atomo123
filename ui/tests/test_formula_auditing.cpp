/*
	test_formula_auditing.cpp

	Verifica il primo pezzo delle "Formula auditing views" (ROADMAP.md,
	Path to full Excel parity, Tier 2): il toggle "Mostra formule"
	(SheetView::ToggleShowFormulas/ShowFormulas, Ctrl+`) che mostra il
	testo grezzo della formula al posto del suo valore calcolato, come
	Excel. Test successivi di questo stesso file copriranno Traccia
	precedenti/dipendenti e la Finestra di controllo man mano che
	vengono implementati.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
#include "Range.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "AscdIO.h"

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
	BApplication app("application/x-vnd.Atomo-TestFormulaAuditing");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("10", cell(1, 1), doc, true); // A1
	TryToParseString("20", cell(2, 1), doc, true); // B1
	TryToParseString("=A1+B1", cell(3, 1), doc, true); // C1, una vera formula
	TryToParseString("Ciao Atomo123", cell(4, 1), doc, true); // D1, testo normale
	RecalculateAll(doc);

	Check(!view->ShowFormulas(), "il toggle Mostra formule parte spento");

	BString c1Value = view->FormattedCellText(cell(3, 1));
	Check(c1Value == "30", "con il toggle spento C1 mostra il valore calcolato (30)");

	view->ToggleShowFormulas();
	Check(view->ShowFormulas(), "ToggleShowFormulas lo accende");

	BString c1Formula = view->FormattedCellText(cell(3, 1));
	Check(c1Formula == "=A1+B1",
		"con il toggle acceso C1 mostra il testo della formula con \"=\" iniziale, non il valore");

	BString a1Text = view->FormattedCellText(cell(1, 1));
	Check(a1Text == "10",
		"una cella senza formula (A1, un numero digitato a mano) resta invariata col toggle acceso");

	BString d1Text = view->FormattedCellText(cell(4, 1));
	Check(d1Text == "Ciao Atomo123",
		"una cella di solo testo (D1) resta invariata col toggle acceso");

	view->ToggleShowFormulas();
	Check(!view->ShowFormulas(), "un secondo ToggleShowFormulas lo rispegne");

	BString c1Again = view->FormattedCellText(cell(3, 1));
	Check(c1Again == "30", "con il toggle di nuovo spento C1 torna a mostrare il valore calcolato");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
