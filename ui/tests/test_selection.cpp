/*
	test_selection.cpp

	Verifica la selezione multi-cella (Maiusc+frecce, trascinamento
	del mouse, Maiusc+click, Ctrl+A) e il suo uso da Backspace/Canc
	per svuotare l'intero intervallo selezionato, non solo la cella
	attiva -- primo passo del recupero delle funzionalita' di Sum-It
	rimaste indietro rispetto alla UI riscritta da zero (ROADMAP.md,
	Fase 7): quasi tutto il resto (Ordina, Riempi, formattazione a
	intervallo) dipende da questa base.

	Usa SheetView::HandleKey/MouseDown/MouseMoved/MouseUp direttamente
	invece di un vero ciclo di dispatch di mouse/tastiera -- stesso
	principio di test_navigation.cpp/test_scroll.cpp/test_editing.cpp
	(vedi quei file per il perche').
*/

#include <cstdio>

#include <Application.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <Window.h>

#include "Cell.h"
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
		: BWindow(BRect(100, 100, 700, 500), "test-selection", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestSelection");

	CContainer* doc = new CContainer(NULL, NULL);
	TryToParseString("10", cell(1, 1), doc, true);	// A1
	TryToParseString("20", cell(2, 1), doc, true);	// B1
	TryToParseString("30", cell(1, 2), doc, true);	// A2
	TryToParseString("40", cell(2, 2), doc, true);	// B2

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(400, 300);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Una selezione appena creata (un solo SetSelection, nessuna
	// estensione) e' un range di una sola cella.
	view->SetSelection(cell(1, 1));
	range single = view->SelectionRange();
	Check(single.left == 1 && single.right == 1 && single.top == 1 && single.bottom == 1,
		"una selezione singola e' un range di una sola cella (A1)");

	// Maiusc+Freccia estende il range dall'ancora (A1) senza spostarla;
	// la cella attiva (Selection()) segue il movimento.
	view->HandleKey(B_RIGHT_ARROW, false, true);	// Maiusc+Destra: A1 -> B1
	view->HandleKey(B_DOWN_ARROW, false, true);	// Maiusc+Giu: B1 -> B2
	range extended = view->SelectionRange();
	Check(extended.left == 1 && extended.right == 2 && extended.top == 1 && extended.bottom == 2,
		"Maiusc+frecce estende il range da A1 a B2 (A1:B2)");
	cell active = view->Selection();
	Check(active.h == 2 && active.v == 2,
		"la cella attiva segue il movimento durante l'estensione (arriva su B2)");

	// Una freccia senza Maiusc dopo un'estensione collassa la
	// selezione a una sola cella (quella di arrivo), come Excel.
	view->HandleKey(B_RIGHT_ARROW, false, false);
	range collapsed = view->SelectionRange();
	Check(collapsed.left == collapsed.right && collapsed.top == collapsed.bottom,
		"una freccia senza Maiusc dopo un'estensione ricollassa a una sola cella");

	// Trascinamento: MouseDown su A1 (senza Maiusc) inizia una nuova
	// selezione e arma il tracking; MouseMoved mentre "premuto" (stato
	// interno fDragging) estende fino alla cella sotto il mouse;
	// MouseUp lo interrompe.
	view->MouseDown(BPoint(30 + 5, 20 + 5));		// A1: colonna 1, riga 1
	view->MouseMoved(BPoint(30 + 85, 20 + 25), B_INSIDE_VIEW, NULL);	// dentro B2
	view->MouseUp(BPoint(30 + 85, 20 + 25));
	range dragged = view->SelectionRange();
	Check(dragged.left == 1 && dragged.right == 2 && dragged.top == 1 && dragged.bottom == 2,
		"trascinare il mouse da A1 a B2 seleziona il range A1:B2");

	// Dopo MouseUp, MouseMoved non deve piu' estendere la selezione
	// (il trascinamento e' finito).
	view->MouseMoved(BPoint(30 + 165, 20 + 65), B_INSIDE_VIEW, NULL);	// D4, lontano
	range afterMouseUp = view->SelectionRange();
	Check(afterMouseUp.left == 1 && afterMouseUp.right == 2
		&& afterMouseUp.top == 1 && afterMouseUp.bottom == 2,
		"MouseMoved dopo MouseUp non estende piu' la selezione (trascinamento gia' finito)");

	// SelectAll() seleziona tutto il foglio con dati (qui A1:B2, per
	// via delle quattro celle scritte all'inizio) -- esposta dal menu
	// Modifica > "Seleziona tutto" in MainWindow, non da una
	// scorciatoia da tastiera (Ctrl+A collide con Ctrl+Inizio su
	// Haiku: B_HOME vale 0x01, lo stesso byte generato da Ctrl+A, vedi
	// il commento in SheetView::HandleKey).
	view->SetSelection(cell(1, 1));
	view->SelectAll();
	range all = view->SelectionRange();
	Check(all.left == 1 && all.right == 2 && all.top == 1 && all.bottom == 2,
		"SelectAll() seleziona l'intero foglio con dati (A1:B2)");

	// Backspace/Canc su un range svuota tutte le celle selezionate,
	// non solo quella attiva -- comportamento nuovo, prima cancellava
	// solo la cella attiva anche con piu' celle "evidenziate" (che
	// prima di questa modifica non potevano nemmeno esistere).
	view->HandleKey(B_DELETE, false, false);
	Value a1, b1, a2, b2;
	doc->GetValue(cell(1, 1), a1);
	doc->GetValue(cell(2, 1), b1);
	doc->GetValue(cell(1, 2), a2);
	doc->GetValue(cell(2, 2), b2);
	Check(a1.fType != eNumData && b1.fType != eNumData
		&& a2.fType != eNumData && b2.fType != eNumData,
		"Canc su un range svuota tutte le celle selezionate (A1:B2), non solo quella attiva");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
