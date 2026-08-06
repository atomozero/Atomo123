/*
	test_format.cpp

	Verifica la formattazione grassetto/corsivo/allineamento/colore
	(Fase 7, MainWindow::ToggleBold/ToggleItalic/SetAlignment/
	SetTextColor/SetBackgroundColor): tutti si applicano a tutto
	SelectionRange(), non solo alla cella attiva -- stesso principio
	di SetCellFormat (Fase 7, formato numerico). Grassetto/Corsivo
	leggono lo stato di partenza dalla sola cella attiva (come il
	pulsante "risulta premuto" o no di Excel) e applicano lo stato
	OPPOSTO a tutta la selezione. Richiede una vera MainWindow (i
	metodi sono pubblici apposta per essere testabili direttamente,
	stesso principio di CopySelection/PasteSelection).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
#include "Range.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "FontMetrics.h"
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

static bool StyleHas(CContainer* doc, cell c, const char* what)
{
	CellStyle cs;
	doc->GetCellStyle(c, cs);
	font_family family;
	font_style style;
	float size;
	gFontSizeTable.GetFontInfo(cs.fFont, &family, &style, &size);
	return strstr(style, what) != NULL;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestFormat");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("1", cell(1, 1), doc, true); // A1
	TryToParseString("2", cell(2, 1), doc, true); // B1
	TryToParseString("3", cell(1, 2), doc, true); // A2
	TryToParseString("4", cell(2, 2), doc, true); // B2

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 2)); // A1:B2

	// Grassetto: nessuna cella e' in grassetto all'inizio (attiva =
	// B2, l'ultimo angolo esteso), quindi l'intera selezione lo
	// diventa.
	win->ToggleBold();
	Check(StyleHas(doc, cell(1, 1), "Bold") && StyleHas(doc, cell(2, 1), "Bold")
		&& StyleHas(doc, cell(1, 2), "Bold") && StyleHas(doc, cell(2, 2), "Bold"),
		"ToggleBold su A1:B2 mette in grassetto tutte e quattro le celle, non solo l'attiva");

	// Un secondo ToggleBold (stessa selezione, stessa attiva B2 --
	// ora in grassetto) lo toglie di nuovo da tutte.
	win->ToggleBold();
	Check(!StyleHas(doc, cell(1, 1), "Bold") && !StyleHas(doc, cell(2, 2), "Bold"),
		"un secondo ToggleBold toglie il grassetto da tutta la selezione");

	// Corsivo, sulla sola A1 stavolta.
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));
	win->ToggleItalic();
	Check(StyleHas(doc, cell(1, 1), "Italic"), "ToggleItalic su A1 la mette in corsivo");

	// Grassetto E corsivo insieme sulla stessa cella (applicati in
	// sequenza, come premere entrambi i pulsanti in Excel).
	win->ToggleBold();
	Check(StyleHas(doc, cell(1, 1), "Bold") && StyleHas(doc, cell(1, 1), "Italic"),
		"ToggleBold su una cella gia' in corsivo produce Grassetto E corsivo insieme, non uno al posto dell'altro");

	// Allineamento, sull'intera selezione A1:B2.
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 2));
	win->SetAlignment(eAlignRight);

	CellStyle cs;
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fAlignment == eAlignRight, "SetAlignment(eAlignRight) si applica ad A1");
	doc->GetCellStyle(cell(2, 2), cs);
	Check(cs.fAlignment == eAlignRight, "SetAlignment(eAlignRight) si applica anche a B2, non solo alla cella attiva");

	// Colore testo/sfondo, sulla sola A1.
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));
	rgb_color red = {255, 0, 0, 255};
	rgb_color blue = {0, 0, 255, 255};
	win->SetTextColor(red);
	win->SetBackgroundColor(blue);

	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fHighColor.red == 255 && cs.fHighColor.green == 0 && cs.fHighColor.blue == 0,
		"SetTextColor(rosso) imposta il colore del testo di A1");
	Check(cs.fLowColor.red == 0 && cs.fLowColor.green == 0 && cs.fLowColor.blue == 255,
		"SetBackgroundColor(blu) imposta il colore di sfondo di A1, indipendente dal testo");

	// Annulla/Ripeti sulla formattazione (bug reale segnalato
	// dall'utente: "Annulla" non toccava affatto lo stile, solo i
	// valori delle celle -- vedi SheetView::UndoCellSnapshot). Riparte
	// da una cella pulita apposta, per non dipendere dalle chiamate
	// sopra.
	view->SetSelection(cell(3, 1));
	view->ExtendSelection(cell(3, 1)); // C1, mai toccata finora
	Check(!StyleHas(doc, cell(3, 1), "Bold"), "C1 non e' in grassetto prima del test annulla/ripeti");

	win->ToggleBold();
	Check(StyleHas(doc, cell(3, 1), "Bold"), "ToggleBold mette C1 in grassetto");

	view->Undo();
	Check(!StyleHas(doc, cell(3, 1), "Bold"),
		"Annulla dopo ToggleBold toglie di nuovo il grassetto da C1 -- prima di questo fix non succedeva nulla");

	view->Redo();
	Check(StyleHas(doc, cell(3, 1), "Bold"), "Ripeti dopo Annulla rimette C1 in grassetto");

	// Stessa verifica per l'allineamento, giusto per non fidarsi che
	// funzioni solo perche' funziona per grassetto/corsivo.
	view->SetSelection(cell(3, 2));
	view->ExtendSelection(cell(3, 2)); // C2
	win->SetAlignment(eAlignRight);
	view->Undo();
	doc->GetCellStyle(cell(3, 2), cs);
	Check(cs.fAlignment != eAlignRight,
		"Annulla dopo SetAlignment(eAlignRight) ripristina l'allineamento precedente su C2");

	// SetCellFormat (Formato > Numero/Valuta/Percentuale/Generale):
	// stesso bug, trovato durante l'audit successivo al fix di
	// grassetto/corsivo/ecc. sopra -- mancava SaveUndoState() anche qui.
	view->SetSelection(cell(3, 3));
	view->ExtendSelection(cell(3, 3)); // C3
	win->SetCellFormat(eCurrency);
	doc->GetCellStyle(cell(3, 3), cs);
	Check(cs.fFormat == eCurrency, "SetCellFormat(eCurrency) imposta il formato su C3");

	view->Undo();
	doc->GetCellStyle(cell(3, 3), cs);
	Check(cs.fFormat != eCurrency,
		"Annulla dopo SetCellFormat(eCurrency) ripristina il formato precedente su C3");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
