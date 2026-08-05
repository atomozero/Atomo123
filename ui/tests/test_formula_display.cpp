/*
	test_formula_display.cpp

	Verifica che una formula venga mostrata con il simbolo "=" iniziale,
	come in Excel/LibreOffice Calc -- chiesto dall'utente ("è normale
	che le formule non abbiano il = all'inizio?") dopo aver notato che
	la barra della formula mostrava "A1+B1" invece di "=A1+B1".

	Causa reale: CFormula::UnMangle (engine/src/Formula/Formula.cpp)
	antepone "=" solo se il flag globale gWithEqualSign è vero, ma
	quel flag non viene mai impostato da nessuna parte nel codice
	(resta al valore predefinito C++ per un bool globale, false) --
	un interruttore storico mai davvero collegato a nulla nel porting.
	Corretto aggiungendo "=" nei due punti dove l'utente legge il
	contenuto di una cella come formula (barra della formula in
	MainWindow::SelectionChanged, editing in-cella in
	SheetView::StartEditing), non riattivando il flag globale: quello
	influenzerebbe anche l'export XLSX/ODS, dove l'elemento <f> non
	deve mai avere "=" per specifica ECMA-376/OpenDocument.
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
	BApplication app("application/x-vnd.Atomo-TestFormulaDisplay");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("10", cell(1, 1), doc, true); // A1
	TryToParseString("20", cell(2, 1), doc, true); // B1
	TryToParseString("=A1+B1", cell(3, 1), doc, true); // C1, una vera formula
	TryToParseString("Ciao Atomo123", cell(4, 1), doc, true); // D1, testo normale

	view->SetSelection(cell(3, 1)); // C1
	Check(strcmp(win->FormulaBarText(), "=A1+B1") == 0,
		"la barra della formula mostra \"=A1+B1\" (con il segno di uguale) per una formula");

	view->SetSelection(cell(1, 1)); // A1, un numero semplice
	Check(strcmp(win->FormulaBarText(), "10") == 0,
		"la barra della formula mostra \"10\" (senza \"=\") per un numero semplice, nessuna formula");

	view->SetSelection(cell(4, 1)); // D1, testo
	Check(strcmp(win->FormulaBarText(), "Ciao Atomo123") == 0,
		"la barra della formula mostra il testo cosi' com'e' (senza \"=\") per una cella di testo");

	view->SetSelection(cell(5, 5)); // E5, cella vuota
	Check(strcmp(win->FormulaBarText(), "") == 0,
		"la barra della formula resta vuota (senza \"=\") per una cella vuota");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
