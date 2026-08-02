/*
	test_overflow.cpp

	Verifica il testo che trabocca sulle colonne vuote vicine (Fase 5,
	confronto diretto con Excel vero su una fattura reale: lo studio, un'intestazione piu' larga della propria
	colonna, veniva troncata al bordo di cella invece di espandersi
	sulle colonne vuote a destra come mostra Excel). Testa
	SheetView::ExpandOverflowRect direttamente, pubblico apposta per
	essere testabile (stesso principio di CellRect/CopySelection) --
	con una larghezza di testo passata a mano invece che misurata da
	StringWidth, cosi' il test resta deterministico e indipendente dal
	font di sistema.
*/

#include <cstdio>

#include <Application.h>

#include "Cell.h"
#include "Range.h"
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
	BApplication app("application/x-vnd.Atomo-TestOverflow");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// Testo che entra gia' nella colonna: il rettangolo non cambia.
	{
		BRect base = view->CellRect(cell(1, 1));
		BRect r = view->ExpandOverflowRect(base, cell(1, 1), eAlignGeneral, 10.0f);
		Check(r == base, "un testo che entra gia' nella colonna non espande il rettangolo");
	}

	// Allineamento generale (il predefinito, come il testo normale):
	// trabocca verso destra su colonne vuote.
	{
		BRect base = view->CellRect(cell(1, 3));
		float need = base.Width() * 2.5f;
		BRect r = view->ExpandOverflowRect(base, cell(1, 3), eAlignGeneral, need);
		Check(r.left == base.left, "il bordo sinistro non si muove mai traboccando a destra");
		Check(r.right > base.right, "il bordo destro si espande oltre la colonna originale");
		Check(r.Width() >= need + 6 - 0.01f, "il rettangolo espanso contiene tutta la larghezza richiesta");
	}

	// Bloccato da una cella non vuota: nessuna espansione.
	{
		TryToParseString("occupata", cell(2, 4), doc, true); // B4
		BRect base = view->CellRect(cell(1, 4));
		BRect r = view->ExpandOverflowRect(base, cell(1, 4), eAlignGeneral, base.Width() * 3.0f);
		Check(r == base,
			"una cella non vuota subito a destra blocca del tutto l'espansione (A4 col B4 occupata)");
	}

	// Allineamento a destra: trabocca verso sinistra.
	{
		BRect base = view->CellRect(cell(5, 5));
		float need = base.Width() * 2.5f;
		BRect r = view->ExpandOverflowRect(base, cell(5, 5), eAlignRight, need);
		Check(r.right == base.right, "il bordo destro non si muove mai traboccando a sinistra");
		Check(r.left < base.left, "il bordo sinistro si espande oltre la colonna originale");
	}

	// Allineamento a destra bloccato da una cella non vuota a sinistra:
	// l'espansione si ferma PRIMA di raggiungerla, non la scavalca.
	{
		TryToParseString("occupata", cell(2, 6), doc, true); // B6
		BRect base = view->CellRect(cell(4, 6));
		BRect r = view->ExpandOverflowRect(base, cell(4, 6), eAlignRight, base.Width() * 3.0f);
		BRect colC = view->CellRect(cell(3, 6));
		Check(r.left == colC.left,
			"l'espansione a sinistra si ferma al bordo di C6, senza toccare B6 (occupata)");
	}

	// Allineamento centrato: trabocca su entrambi i lati se entrambi
	// sono vuoti.
	{
		BRect base = view->CellRect(cell(10, 7));
		float need = base.Width() * 3.5f;
		BRect r = view->ExpandOverflowRect(base, cell(10, 7), eAlignCenter, need);
		Check(r.left < base.left && r.right > base.right,
			"il testo centrato che trabocca si espande su entrambi i lati, non solo uno");
	}

	// Celle gia' unite: bloccano l'espansione come una cella non vuota
	// (Fase 12, GetMergedRange).
	{
		view->SetSelection(cell(2, 8));
		view->ExtendSelection(cell(3, 8)); // B8:C8
		win->MergeCells();

		BRect base = view->CellRect(cell(1, 8));
		BRect r = view->ExpandOverflowRect(base, cell(1, 8), eAlignGeneral, base.Width() * 3.0f);
		Check(r == base,
			"un intervallo gia' unito subito a destra (B8:C8) blocca l'espansione di A8, come una cella occupata");
	}

	// Bordo del foglio: nessun crash ne' espansione oltre kColCount
	// (verificato indirettamente: l'espansione a destra dall'ultima
	// colonna del foglio non deve spostare il bordo destro).
	{
		cell lastCol(kColCount, 9);
		BRect base = view->CellRect(lastCol);
		BRect r = view->ExpandOverflowRect(base, lastCol, eAlignGeneral, base.Width() * 3.0f);
		Check(r.right == base.right,
			"l'ultima colonna del foglio non puo' espandersi oltre kColCount, nessuna colonna dopo");
	}

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
