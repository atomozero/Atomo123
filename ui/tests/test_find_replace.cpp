/*
	test_find_replace.cpp

	Verifica Trova successivo/Sostituisci/Sostituisci tutto (menu
	Modifica): nessuna copertura diretta esisteva prima di questo test
	-- vedi memoria project_command_audit_20260808. FindNext/
	ReplaceCurrent/ReplaceAll sono pubblici apposta per essere
	testabili direttamente (stesso principio di CopySelection/
	HandlePasteSpecialRequest in test_paste_special.cpp), richiede una
	vera MainWindow.

	Limite noto (non testabile in automatico): ReplaceAll mostra un
	vero BAlert col conteggio delle sostituzioni al termine -- stesso
	limite gia' documentato in test_unsaved_changes.cpp per
	ConfirmDiscardChanges. Qui si verifica solo l'effetto vero (le
	celle modificate), non il dialogo finale.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
#include "Value.h"
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
	BApplication app("application/x-vnd.Atomo-TestFindReplace");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("Mela rossa", cell(1, 1), doc, true);  // A1
	TryToParseString("Pera verde", cell(1, 2), doc, true);  // A2
	TryToParseString("Mela verde", cell(1, 3), doc, true);  // A3
	TryToParseString("Banana", cell(1, 4), doc, true);      // A4

	// FindNext parte dalla selezione corrente e trova la PROSSIMA
	// occorrenza (non necessariamente la prima in assoluto), senza
	// distinguere maiuscole/minuscole.
	view->SetSelection(cell(1, 1));
	win->FindNext("mela");
	Check(view->Selection() == cell(1, 3),
		"Trova successivo da A1 (che gia' contiene \"Mela\") trova la prossima occorrenza (A3), non resta fermo");

	// Da dopo l'ultima occorrenza, la ricerca ricomincia dall'inizio
	// (giro completo, come Excel/LibreOffice Calc).
	view->SetSelection(cell(1, 4));
	win->FindNext("mela");
	Check(view->Selection() == cell(1, 1),
		"Trova successivo oltre l'ultima occorrenza ricomincia dall'inizio (torna ad A1)");

	// Nessuna corrispondenza: la selezione non si sposta.
	view->SetSelection(cell(1, 2));
	win->FindNext("ananas");
	Check(view->Selection() == cell(1, 2),
		"Trova successivo senza nessuna corrispondenza non sposta la selezione");

	// ReplaceCurrent sostituisce SOLO se la cella selezionata contiene
	// davvero il testo cercato, poi avanza come Trova successivo.
	view->SetSelection(cell(1, 1)); // A1 = "Mela rossa"
	win->ReplaceCurrent("rossa", "gialla");
	char formulaA1[256];
	doc->GetCellFormula(cell(1, 1), formulaA1, sizeof(formulaA1), false);
	Check(strcmp(formulaA1, "Mela gialla") == 0,
		"Sostituisci sulla cella selezionata cambia \"rossa\" in \"gialla\"");

	// La cella selezionata NON contiene il testo cercato: nessuna
	// modifica, nessun crash.
	view->SetSelection(cell(1, 4)); // A4 = "Banana"
	win->ReplaceCurrent("rossa", "gialla");
	char formulaA4[256];
	doc->GetCellFormula(cell(1, 4), formulaA4, sizeof(formulaA4), false);
	Check(strcmp(formulaA4, "Banana") == 0,
		"Sostituisci su una cella che non contiene il testo cercato non la modifica");

	Check(view->CanUndo(), "Sostituisci sulla cella selezionata e' annullabile");
	view->Undo();
	doc->GetCellFormula(cell(1, 1), formulaA1, sizeof(formulaA1), false);
	Check(strcmp(formulaA1, "Mela rossa") == 0,
		"Annulla dopo Sostituisci ripristina il testo originale");

	// ReplaceAll sostituisce OGNI occorrenza nel foglio, non solo la
	// cella selezionata, senza distinguere maiuscole/minuscole nella
	// ricerca.
	win->ReplaceAll("mela", "kiwi");
	doc->GetCellFormula(cell(1, 1), formulaA1, sizeof(formulaA1), false);
	char formulaA3[256];
	doc->GetCellFormula(cell(1, 3), formulaA3, sizeof(formulaA3), false);
	char formulaA2[256];
	doc->GetCellFormula(cell(1, 2), formulaA2, sizeof(formulaA2), false);
	Check(strcmp(formulaA1, "kiwi rossa") == 0,
		"Sostituisci tutto cambia A1 (\"Mela rossa\" -> \"kiwi rossa\")");
	Check(strcmp(formulaA3, "kiwi verde") == 0,
		"Sostituisci tutto cambia ANCHE A3 (\"Mela verde\" -> \"kiwi verde\"), non solo la prima occorrenza");
	Check(strcmp(formulaA2, "Pera verde") == 0,
		"Sostituisci tutto non tocca A2, che non conteneva il testo cercato");

	Check(view->CanUndo(), "Sostituisci tutto e' annullabile in un colpo solo");
	view->Undo();
	doc->GetCellFormula(cell(1, 1), formulaA1, sizeof(formulaA1), false);
	doc->GetCellFormula(cell(1, 3), formulaA3, sizeof(formulaA3), false);
	Check(strcmp(formulaA1, "Mela rossa") == 0 && strcmp(formulaA3, "Mela verde") == 0,
		"un solo Annulla dopo Sostituisci tutto ripristina TUTTE le celle modificate");

	// Sostituisci tutto senza nessuna corrispondenza: nessuna modifica,
	// nessun crash, niente da annullare in piu'.
	int undoStackBefore = view->CanUndo() ? 1 : 0;
	win->ReplaceAll("ananas", "cocco");
	Check((view->CanUndo() ? 1 : 0) == undoStackBefore,
		"Sostituisci tutto senza corrispondenze non aggiunge nulla alla pila di Annulla");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
