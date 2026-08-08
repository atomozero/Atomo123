/*
	test_paste_special.cpp

	Verifica Incolla speciale (Fase 7, MainWindow::HandlePasteSpecialRequest):
	Solo valori (converte una formula copiata nel suo risultato statico,
	non ricopia la formula), le quattro operazioni aritmetiche con la
	cella di destinazione (Somma/Sottrai/Moltiplica/Dividi, sempre su
	valori, mai su formule) e Trasponi. Stessa ragione di
	test_paste_range.cpp per il resto: richiede una vera MainWindow
	(CopySelection/HandlePasteSpecialRequest sono suoi metodi, esposti
	pubblicamente apposta per essere testabili -- vedi il commento in
	MainWindow.h).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Clipboard.h>

#include "Cell.h"
#include "Value.h"
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

static double NumAt(CContainer* doc, int col, int row)
{
	Value v;
	doc->GetValue(cell(col, row), v);
	return (v.fType == eNumData) ? (double)v : -999.0;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestPasteSpecial");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// Solo valori: A1=10, B1==A1*3 (30). Copiando B1 e incollando con
	// content=1 (Solo valori) su D1, D1 deve contenere il numero
	// statico 30, non la formula "=A1*3" (che a quella nuova posizione
	// significherebbe comunque qualcos'altro).
	TryToParseString("10", cell(1, 1), doc, true); // A1
	TryToParseString("=A1*3", cell(2, 1), doc, true); // B1
	doc->CalcCell(cell(2, 1));

	view->SetSelection(cell(2, 1));
	win->CopySelection(false);

	view->SetSelection(cell(4, 1)); // D1
	win->HandlePasteSpecialRequest(1 /* Solo valori */, 0 /* Sovrascrivi */, false);

	Check(NumAt(doc, 4, 1) == 30.0, "Incolla speciale > Solo valori scrive il risultato (30), non la formula");

	char formula[256];
	doc->GetCellFormula(cell(4, 1), formula, sizeof(formula), false);
	Check(strcmp(formula, "30") == 0,
		"la cella incollata con Solo valori non e' una formula viva (nessun \"=\")");

	// Operazioni aritmetiche: F1 parte da 5, si copia una cella con 3
	// e la si incolla su F1 con ciascuna delle quattro operazioni in
	// sequenza (Somma/Sottrai/Moltiplica/Dividi si applicano sempre al
	// valore corrente di F1, non a quello originale).
	TryToParseString("3", cell(1, 3), doc, true); // A3, la "sorgente" da copiare
	view->SetSelection(cell(1, 3));
	win->CopySelection(false);

	TryToParseString("5", cell(6, 1), doc, true); // F1
	view->SetSelection(cell(6, 1));
	win->HandlePasteSpecialRequest(0, 1 /* Somma */, false);
	Check(NumAt(doc, 6, 1) == 8.0, "Incolla speciale > Somma: F1 (5) + 3 = 8");

	win->HandlePasteSpecialRequest(0, 2 /* Sottrai */, false);
	Check(NumAt(doc, 6, 1) == 5.0, "Incolla speciale > Sottrai: F1 (8) - 3 = 5");

	win->HandlePasteSpecialRequest(0, 3 /* Moltiplica */, false);
	Check(NumAt(doc, 6, 1) == 15.0, "Incolla speciale > Moltiplica: F1 (5) * 3 = 15");

	win->HandlePasteSpecialRequest(0, 4 /* Dividi */, false);
	Check(NumAt(doc, 6, 1) == 5.0, "Incolla speciale > Dividi: F1 (15) / 3 = 5");

	// L'operazione aritmetica e' annullabile come le altre di questa
	// fase (usa comunque SaveUndoState, stesso meccanismo di
	// PasteSelection).
	Check(view->CanUndo(), "un'operazione di Incolla speciale e' annullabile");
	view->Undo();
	Check(NumAt(doc, 6, 1) == 15.0, "Annulla dopo Dividi ripristina il valore precedente (15)");

	// Dividi per una sorgente 0: prima del fix scriveva silenziosamente
	// 0 nella cella di destinazione (bug reale trovato nell'audit dei
	// comandi, Fase 15) -- deve invece mostrare l'errore #DIV/0!, come
	// una formula "=A1/0".
	TryToParseString("0", cell(1, 5), doc, true); // A5, la "sorgente" da copiare
	view->SetSelection(cell(1, 5));
	win->CopySelection(false);

	TryToParseString("42", cell(6, 5), doc, true); // F5
	view->SetSelection(cell(6, 5));
	win->HandlePasteSpecialRequest(0, 4 /* Dividi */, false);

	Value divResult;
	doc->GetValue(cell(6, 5), divResult);
	Check(divResult.fType == eNumData && divResult.IsNan(),
		"Incolla speciale > Dividi per una sorgente 0 produce l'errore #DIV/0!, non scrive 0");

	// Trasponi: copiare A1:B1 (10, 30) e incollarlo trasposto su H1
	// deve produrre una colonna (H1=10, H2=30), non una riga.
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 1)); // A1:B1
	win->CopySelection(false);

	view->SetSelection(cell(8, 1)); // H1
	win->HandlePasteSpecialRequest(0, 0, true /* transpose */);
	Check(NumAt(doc, 8, 1) == 10.0 && NumAt(doc, 8, 2) == 30.0,
		"Incolla speciale > Trasponi scambia una riga copiata in una colonna incollata (H1=10, H2=30)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
