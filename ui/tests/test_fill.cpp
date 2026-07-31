/*
	test_fill.cpp

	Verifica Riempi in basso/a destra (SheetView::FillDown/FillRight),
	secondo passo del recupero delle funzionalita' di Sum-It (ROADMAP.md,
	Fase 7): copia la prima riga/colonna dell'intervallo selezionato
	nel resto dell'intervallo, aggiornando i riferimenti relativi nelle
	formule -- riusa CContainer::CopyCell(..., isDragMove=true), lo
	stesso meccanismo storico di Sum-It per il trascinamento della
	maniglia di riempimento (Commands/DragCommand.cpp nel codice
	storico), invece di reinventare da zero lo spostamento dei
	riferimenti nelle formule.

	Usa SheetView::SetSelection/ExtendSelection direttamente invece di
	un vero ciclo di dispatch di mouse/tastiera -- stesso principio di
	test_selection.cpp/test_navigation.cpp.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <Window.h>

#include "Cell.h"
#include "Value.h"
#include "Range.h"
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
		: BWindow(BRect(100, 100, 700, 500), "test-fill", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestFill");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(400, 300);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Riempi in basso su un valore semplice: A1=42, selezione A1:A3,
	// dopo il riempimento A2 e A3 devono valere 42 come A1.
	TryToParseString("42", cell(1, 1), doc, true); // A1
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 3)); // A1:A3
	view->FillDown();

	Value a2, a3;
	doc->GetValue(cell(1, 2), a2);
	doc->GetValue(cell(1, 3), a3);
	Check(a2.fType == eNumData && (double)a2 == 42.0,
		"Riempi in basso copia un valore semplice (A1=42 -> A2=42)");
	Check(a3.fType == eNumData && (double)a3 == 42.0,
		"Riempi in basso copia fino in fondo all'intervallo (A3=42)");

	// Riempi in basso su una formula relativa: B1=10, B2=20, B3=30,
	// C1="=B1*2" (=20). Selezionando C1:C3 e riempiendo in basso, C2
	// deve diventare "=B2*2" (=40) e C3 "=B3*2" (=60) -- il
	// riferimento relativo si sposta con la riga, non resta fisso su
	// B1 come farebbe una copia letterale.
	TryToParseString("10", cell(2, 1), doc, true); // B1
	TryToParseString("20", cell(2, 2), doc, true); // B2
	TryToParseString("30", cell(2, 3), doc, true); // B3
	TryToParseString("=B1*2", cell(3, 1), doc, true); // C1
	doc->CalcCell(cell(3, 1));

	view->SetSelection(cell(3, 1));
	view->ExtendSelection(cell(3, 3)); // C1:C3
	view->FillDown();

	char c2Formula[512], c3Formula[512];
	doc->GetCellFormula(cell(3, 2), c2Formula, false);
	doc->GetCellFormula(cell(3, 3), c3Formula, false);
	Check(strstr(c2Formula, "B2") != NULL,
		"Riempi in basso sposta il riferimento relativo (C2 diventa =B2*2, non =B1*2)");
	Check(strstr(c3Formula, "B3") != NULL,
		"Riempi in basso sposta il riferimento relativo anche su C3 (=B3*2)");

	Value c2, c3;
	doc->GetValue(cell(3, 2), c2);
	doc->GetValue(cell(3, 3), c3);
	Check(c2.fType == eNumData && (double)c2 == 40.0,
		"C2 (=B2*2) calcola 40 dopo il riempimento");
	Check(c3.fType == eNumData && (double)c3 == 60.0,
		"C3 (=B3*2) calcola 60 dopo il riempimento");

	// Riempi a destra: simmetrico, sulla colonna invece che sulla
	// riga -- la colonna piu' a sinistra della selezione e' la
	// sorgente (stessa convenzione di Riempi in basso, dove la riga
	// piu' in alto e' la sorgente; coerente con Excel). D1=4, E1=5;
	// F1="=D1+E1" (=9); selezionando F1:G1 e riempiendo a destra, G1
	// deve diventare "=E1+F1" (ogni riferimento spostato di una
	// colonna, non solo quello a D1).
	TryToParseString("4", cell(4, 1), doc, true);   // D1
	TryToParseString("5", cell(5, 1), doc, true);   // E1
	TryToParseString("=D1+E1", cell(6, 1), doc, true); // F1
	doc->CalcCell(cell(6, 1));

	view->SetSelection(cell(6, 1));
	view->ExtendSelection(cell(7, 1)); // F1:G1
	view->FillRight();

	char g1Formula[512];
	doc->GetCellFormula(cell(7, 1), g1Formula, false);
	Check(strstr(g1Formula, "E1") != NULL && strstr(g1Formula, "F1") != NULL,
		"Riempi a destra sposta entrambi i riferimenti relativi (G1 diventa =E1+F1)");

	Value g1;
	doc->GetValue(cell(7, 1), g1);
	Check(g1.fType == eNumData && (double)g1 == 14.0,
		"G1 (=E1+F1, 5+9) calcola 14 dopo il riempimento a destra");

	// Selezione di una sola cella (nessun intervallo): non deve fare
	// nulla ne' andare in crash, non c'e' niente da cui copiare.
	view->SetSelection(cell(1, 10));
	view->FillDown();
	view->FillRight();
	Value untouched;
	doc->GetValue(cell(1, 10), untouched);
	Check(untouched.fType != eNumData,
		"Riempi su una sola cella selezionata non fa nulla (nessun crash, nessuna scrittura)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
