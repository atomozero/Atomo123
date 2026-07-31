/*
	test_sort.cpp

	Verifica Ordina crescente/decrescente (SheetView::SortSelection),
	terzo passo del recupero delle funzionalita' di Sum-It (ROADMAP.md,
	Fase 7): ordina l'intervallo selezionato per righe intere,
	confrontando la colonna piu' a sinistra -- confronto numerico se
	entrambi i valori sono numeri, altrimenti testuale; ordinamento
	stabile; ogni cella si sposta come testo grezzo (non tocca i
	riferimenti nelle formule, a differenza di Riempi).

	Usa SheetView::SetSelection/ExtendSelection direttamente invece di
	un vero ciclo di dispatch di mouse/tastiera -- stesso principio di
	test_selection.cpp/test_fill.cpp.
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
		: BWindow(BRect(100, 100, 700, 500), "test-sort", B_TITLED_WINDOW, 0)
	{
	}
};

static double NumAt(CContainer* doc, int col, int row)
{
	Value v;
	doc->GetValue(cell(col, row), v);
	return (v.fType == eNumData) ? (double)v : -999.0;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestSort");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(400, 300);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Tabella A1:B3 (colonna chiave A, colonna "passeggero" B che deve
	// seguire la riga durante il riordino):
	//   A1=30 B1="c"
	//   A2=10 B2="a"
	//   A3=20 B3="b"
	// Ordinando crescente su A, l'ordine atteso e' 10/a, 20/b, 30/c.
	TryToParseString("30", cell(1, 1), doc, true);
	TryToParseString("c", cell(2, 1), doc, true);
	TryToParseString("10", cell(1, 2), doc, true);
	TryToParseString("a", cell(2, 2), doc, true);
	TryToParseString("20", cell(1, 3), doc, true);
	TryToParseString("b", cell(2, 3), doc, true);

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 3)); // A1:B3
	view->SortSelection(true);

	Check(NumAt(doc, 1, 1) == 10.0 && NumAt(doc, 1, 2) == 20.0 && NumAt(doc, 1, 3) == 30.0,
		"Ordina crescente riordina la colonna chiave (10, 20, 30)");

	char b1[512], b2[512], b3[512];
	doc->GetCellFormula(cell(2, 1), b1, false);
	doc->GetCellFormula(cell(2, 2), b2, false);
	doc->GetCellFormula(cell(2, 3), b3, false);
	Check(strcmp(b1, "a") == 0 && strcmp(b2, "b") == 0 && strcmp(b3, "c") == 0,
		"la colonna passeggero (B) segue la riga durante il riordino (a, b, c)");

	// Ordina decrescente sulla stessa selezione: torna a 30, 20, 10.
	view->SortSelection(false);
	Check(NumAt(doc, 1, 1) == 30.0 && NumAt(doc, 1, 2) == 20.0 && NumAt(doc, 1, 3) == 10.0,
		"Ordina decrescente inverte l'ordine (30, 20, 10)");

	// Ordinamento stabile: due righe con la stessa chiave mantengono
	// l'ordine relativo originale invece di uno arbitrario. Il
	// documento precedente va rilasciato esplicitamente prima di
	// sostituirlo (CContainer e' un BLocker, non solo un blocco di
	// memoria: lasciarne parecchi senza rilasciarli esaurisce i
	// semafori del kernel dopo un po', causando un blocco indefinito
	// alla creazione del successivo -- scoperto proprio da questo
	// test, che all'inizio non rilasciava mai i documenti intermedi).
	doc->Release();
	CContainer* doc2 = new CContainer(NULL, NULL);
	view->SetDocument(doc2);
	TryToParseString("5", cell(1, 1), doc2, true);
	TryToParseString("primo", cell(2, 1), doc2, true);
	TryToParseString("5", cell(1, 2), doc2, true);
	TryToParseString("secondo", cell(2, 2), doc2, true);
	TryToParseString("1", cell(1, 3), doc2, true);
	TryToParseString("terzo", cell(2, 3), doc2, true);

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 3));
	view->SortSelection(true);

	char stableB1[512], stableB2[512];
	doc2->GetCellFormula(cell(2, 1), stableB1, false); // dovrebbe restare "terzo" (chiave 1)
	doc2->GetCellFormula(cell(2, 2), stableB2, false); // poi "primo" (chiave 5, era prima di "secondo")
	Check(strcmp(stableB1, "terzo") == 0,
		"la chiave piu' piccola (1) va prima anche se e' l'ultima riga originale");
	Check(strcmp(stableB2, "primo") == 0,
		"a parita' di chiave (5), 'primo' resta prima di 'secondo' (ordinamento stabile)");

	// Selezione di una sola riga: non deve fare nulla ne' andare in
	// crash, non c'e' niente da riordinare.
	doc2->Release();
	CContainer* doc3 = new CContainer(NULL, NULL);
	view->SetDocument(doc3);
	TryToParseString("99", cell(1, 1), doc3, true);
	view->SetSelection(cell(1, 1));
	view->SortSelection(true);
	Check(NumAt(doc3, 1, 1) == 99.0,
		"Ordina su una sola riga selezionata non fa nulla (nessun crash)");
	doc3->Release();

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
