/*
	test_undo.cpp

	Verifica Annulla/Ripeti (SheetView::SaveUndoState/Undo/Redo), terzo
	passo del recupero delle funzionalita' di Sum-It (ROADMAP.md, Fase
	7): pila di istantanee testuali per intervallo, condivisa da tutte
	le operazioni che mutano il documento (editing in-cella, Riempi,
	Ordina, Cancella, Taglia/Incolla, Trova e sostituisci) tramite
	SaveUndoState(), chiamato prima della mutazione.

	Usa SheetView::SetSelection/ExtendSelection/SaveUndoState
	direttamente invece di un vero ciclo di dispatch di mouse/tastiera
	o menu -- stesso principio di test_selection.cpp/test_fill.cpp/
	test_sort.cpp.
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
		: BWindow(BRect(100, 100, 700, 500), "test-undo", B_TITLED_WINDOW, 0)
	{
	}
};

static double NumAt(CContainer* doc, int col, int row)
{
	Value v;
	doc->GetValue(cell(col, row), v);
	return (v.fType == eNumData) ? (double)v : -999.0;
}

static bool IsEmpty(CContainer* doc, int col, int row)
{
	char text[512];
	doc->GetCellFormula(cell(col, row), text, false);
	return text[0] == 0;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestUndo");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(400, 300);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Un documento appena creato non ha nulla da annullare/ripetere.
	Check(!view->CanUndo() && !view->CanRedo(),
		"un documento appena creato non ha nulla da annullare o ripetere");

	// Modifica di una sola cella (simula quello che fa
	// SheetView::CommitEditing internamente): A1=10, poi A1=20 con
	// SaveUndoState() chiamato prima della seconda scrittura.
	TryToParseString("10", cell(1, 1), doc, true);
	view->SaveUndoState(cell(1, 1));
	TryToParseString("20", cell(1, 1), doc, true);

	Check(NumAt(doc, 1, 1) == 20.0, "A1 vale 20 dopo la modifica");
	Check(view->CanUndo() && !view->CanRedo(),
		"dopo una modifica si puo' annullare ma non ancora ripetere");

	view->Undo();
	Check(NumAt(doc, 1, 1) == 10.0, "Annulla riporta A1 al valore precedente (10)");
	Check(!view->CanUndo() && view->CanRedo(),
		"dopo l'unico annulla non resta altro da annullare, ma si puo' ripetere");

	view->Redo();
	Check(NumAt(doc, 1, 1) == 20.0, "Ripeti riporta A1 al valore successivo (20)");
	Check(view->CanUndo() && !view->CanRedo(),
		"dopo ripeti si puo' di nuovo annullare, ma non ripetere oltre");

	// Cancella su un intervallo: A2=5, B2=6; ClearSelection() chiama
	// gia' SaveUndoState() da sola (vedi SheetView.cpp).
	TryToParseString("5", cell(1, 2), doc, true); // A2
	TryToParseString("6", cell(2, 2), doc, true); // B2
	view->SetSelection(cell(1, 2));
	view->ExtendSelection(cell(2, 2)); // A2:B2
	view->ClearSelection();

	Check(IsEmpty(doc, 1, 2) && IsEmpty(doc, 2, 2),
		"Cancella svuota l'intervallo A2:B2");

	view->Undo();
	Check(NumAt(doc, 1, 2) == 5.0 && NumAt(doc, 2, 2) == 6.0,
		"Annulla dopo Cancella ripristina entrambe le celle dell'intervallo (A2=5, B2=6)");

	// Riempi in basso: A3=42, A4 vuota; FillDown() chiama gia'
	// SaveUndoState() da sola.
	TryToParseString("42", cell(1, 3), doc, true); // A3
	view->SetSelection(cell(1, 3));
	view->ExtendSelection(cell(1, 4)); // A3:A4
	view->FillDown();

	Check(NumAt(doc, 1, 4) == 42.0, "Riempi in basso scrive A4=42");

	view->Undo();
	Check(IsEmpty(doc, 1, 4),
		"Annulla dopo Riempi in basso svuota di nuovo A4 (non conteneva nulla prima)");

	// Ordina: due righe A5=20/B5="b", A6=10/B6="a"; SortSelection()
	// chiama gia' SaveUndoState() da sola.
	TryToParseString("20", cell(1, 5), doc, true);
	TryToParseString("b", cell(2, 5), doc, true);
	TryToParseString("10", cell(1, 6), doc, true);
	TryToParseString("a", cell(2, 6), doc, true);

	view->SetSelection(cell(1, 5));
	view->ExtendSelection(cell(2, 6)); // A5:B6
	view->SortSelection(true);

	Check(NumAt(doc, 1, 5) == 10.0 && NumAt(doc, 1, 6) == 20.0,
		"Ordina crescente riordina A5:A6 (10, 20)");

	view->Undo();
	Check(NumAt(doc, 1, 5) == 20.0 && NumAt(doc, 1, 6) == 10.0,
		"Annulla dopo Ordina ripristina l'ordine originale (20, 10)");

	view->Redo();
	Check(NumAt(doc, 1, 5) == 10.0 && NumAt(doc, 1, 6) == 20.0,
		"Ripeti dopo Annulla riordina di nuovo (10, 20)");

	// Una nuova modifica dopo un Annulla rende irraggiungibile il
	// "ripeti" rimasto (come in Excel/LibreOffice Calc).
	view->Undo(); // torna a (20, 10), un "ripeti" resterebbe disponibile
	Check(view->CanRedo(), "prima della nuova modifica il ripeti e' ancora disponibile");

	view->SaveUndoState(cell(1, 8));
	TryToParseString("99", cell(1, 8), doc, true);
	Check(!view->CanRedo(),
		"una nuova modifica dopo Annulla cancella il ripeti rimasto");

	// Annullare/ripetere oltre i limiti della pila non fa nulla ne'
	// va in crash.
	while (view->CanUndo())
		view->Undo();
	view->Undo();
	view->Undo();
	Check(!view->CanUndo(), "annullare oltre l'inizio della pila non fa nulla di male");

	// SetDocument() (apertura di un nuovo file) svuota le pile: le
	// istantanee si riferiscono al documento precedente.
	CContainer* doc2 = new CContainer(NULL, NULL);
	TryToParseString("1", cell(1, 1), doc2, true);
	view->SetDocument(doc2);
	view->SaveUndoState(cell(1, 1));
	TryToParseString("2", cell(1, 1), doc2, true);
	Check(view->CanUndo(), "il nuovo documento puo' comunque accumulare la propria storia");

	CContainer* doc3 = new CContainer(NULL, NULL);
	view->SetDocument(doc3);
	Check(!view->CanUndo() && !view->CanRedo(),
		"SetDocument() svuota le pile di annulla/ripeti del documento precedente");

	doc->Release();
	doc2->Release();
	doc3->Release();

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
