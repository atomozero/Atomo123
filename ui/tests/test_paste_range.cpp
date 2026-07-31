/*
	test_paste_range.cpp

	Verifica Taglia/Copia/Incolla e Formato estesi all'intervallo
	selezionato (MainWindow::CopySelection/PasteSelection/SetCellFormat),
	ultimo passo del recupero delle funzionalita' di Sum-It (ROADMAP.md,
	Fase 7): prima operavano solo sulla cella attiva, ora su
	SheetView::SelectionRange() intero, con un formato TSV (tabulazioni
	fra colonne, ritorni a capo fra righe) sugli appunti di sistema --
	lo stesso capito da Excel/LibreOffice Calc.

	A differenza degli altri test di questa fase, serve una vera
	MainWindow (non la sola SheetView): CopySelection/PasteSelection/
	SetCellFormat sono suoi metodi, esposti pubblicamente apposta per
	essere testabili (vedi il commento in MainWindow.h). GetSheetView()
	da' accesso a Document()/SelectionRange() per preparare i dati e
	verificare i risultati, stesso principio di test_selection.cpp
	eccetera per SheetView.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Clipboard.h>
#include <Message.h>

#include "Cell.h"
#include "Value.h"
#include "Range.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "Formatter.h"
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

static bool IsEmpty(CContainer* doc, int col, int row)
{
	char text[512];
	doc->GetCellFormula(cell(col, row), text, false);
	return text[0] == 0;
}

static bool ReadClipboardText(BString& out)
{
	if (!be_clipboard->Lock())
		return false;

	BMessage* clip = be_clipboard->Data();
	const char* text = NULL;
	ssize_t len = 0;
	bool found = clip && clip->FindData("text/plain", B_MIME_TYPE,
		(const void**)&text, &len) == B_OK;
	if (found)
		out.SetTo(text, len);

	be_clipboard->Unlock();
	return found;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestPasteRange");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// Copia di un intervallo 2x2 (A1:B2 = 1,2 / 3,4): gli appunti
	// devono contenere il formato TSV riga per riga.
	TryToParseString("1", cell(1, 1), doc, true);
	TryToParseString("2", cell(2, 1), doc, true);
	TryToParseString("3", cell(1, 2), doc, true);
	TryToParseString("4", cell(2, 2), doc, true);

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 2)); // A1:B2
	win->CopySelection(false);

	BString clip;
	bool read = ReadClipboardText(clip);
	Check(read && clip == "1\t2\n3\t4",
		"Copia di un intervallo 2x2 scrive il testo TSV corretto sugli appunti");

	// Incolla lo stesso blocco altrove (D5, angolo in alto a
	// sinistra): deve ricreare la stessa griglia 2x2 e selezionare
	// l'intervallo appena incollato.
	view->SetSelection(cell(4, 5)); // D5
	win->PasteSelection();

	Check(NumAt(doc, 4, 5) == 1.0 && NumAt(doc, 5, 5) == 2.0 &&
		NumAt(doc, 4, 6) == 3.0 && NumAt(doc, 5, 6) == 4.0,
		"Incolla un blocco 2x2 ricrea la stessa griglia altrove (D5:E6)");

	range afterPaste = view->SelectionRange();
	Check(afterPaste.left == 4 && afterPaste.top == 5 &&
		afterPaste.right == 5 && afterPaste.bottom == 6,
		"dopo l'incolla la selezione copre esattamente il blocco incollato (D5:E6)");

	// Incolla un solo valore su un intervallo piu' grande di una
	// cella: riempie tutto l'intervallo, non solo l'angolo.
	TryToParseString("7", cell(1, 8), doc, true); // A8
	view->SetSelection(cell(1, 8));
	view->ExtendSelection(cell(1, 8)); // solo A8, poi lo si copia
	win->CopySelection(false);

	view->SetSelection(cell(3, 8));
	view->ExtendSelection(cell(4, 9)); // C8:D9, un intervallo 2x2
	win->PasteSelection();

	Check(NumAt(doc, 3, 8) == 7.0 && NumAt(doc, 4, 8) == 7.0 &&
		NumAt(doc, 3, 9) == 7.0 && NumAt(doc, 4, 9) == 7.0,
		"Incolla un solo valore su un intervallo piu' grande riempie tutto l'intervallo (C8:D9 = 7)");

	// Taglia su un intervallo: svuota tutte le celle coinvolte, non
	// solo la cella attiva.
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 2)); // A1:B2, ancora 1/2/3/4
	win->CopySelection(true); // cut

	Check(IsEmpty(doc, 1, 1) && IsEmpty(doc, 2, 1) &&
		IsEmpty(doc, 1, 2) && IsEmpty(doc, 2, 2),
		"Taglia su un intervallo svuota tutte e quattro le celle (A1:B2)");

	// L'operazione e' annullabile come le altre di questa fase.
	Check(view->CanUndo(), "Taglia su un intervallo e' annullabile");
	view->Undo();
	Check(NumAt(doc, 1, 1) == 1.0 && NumAt(doc, 2, 1) == 2.0 &&
		NumAt(doc, 1, 2) == 3.0 && NumAt(doc, 2, 2) == 4.0,
		"Annulla dopo Taglia su un intervallo ripristina tutte e quattro le celle");

	// Formato numerico esteso all'intervallo: applicato a F1:F3, deve
	// valere per tutte e tre le celle, non solo per la cella attiva.
	TryToParseString("1234.5", cell(6, 1), doc, true); // F1
	TryToParseString("2345.5", cell(6, 2), doc, true); // F2
	TryToParseString("3456.5", cell(6, 3), doc, true); // F3

	view->SetSelection(cell(6, 1));
	view->ExtendSelection(cell(6, 3)); // F1:F3
	win->SetCellFormat(eCurrency);

	CellStyle cs1, cs2, cs3;
	doc->GetCellStyle(cell(6, 1), cs1);
	doc->GetCellStyle(cell(6, 2), cs2);
	doc->GetCellStyle(cell(6, 3), cs3);
	Check(cs1.fFormat == eCurrency && cs2.fFormat == eCurrency && cs3.fFormat == eCurrency,
		"il formato valuta si applica a tutte le celle dell'intervallo (F1:F3), non solo a F1");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
