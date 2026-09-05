/*
	test_formula_auditing.cpp

	Verifica le "Formula auditing views" (ROADMAP.md, Path to full Excel
	parity, Tier 2): il toggle "Mostra formule" (SheetView::
	ToggleShowFormulas/ShowFormulas, Ctrl+`), Traccia precedenti/
	dipendenti (CContainer::GetPrecedents/GetDependents +
	SheetView::ToggleTracePrecedents/ToggleTraceDependents). La Finestra
	di controllo verra' aggiunta a questo stesso file quando
	implementata.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Path.h>
#include <Roster.h>

#include "Cell.h"
#include "Range.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "AscdIO.h"
#include "WatchWindow.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "ResourceManager.h"
#include "MyError.h"

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
	setbuf(stdout, NULL);
	BApplication app("application/x-vnd.Atomo-TestFormulaAuditing");

	// Stessa identica sequenza di App::ReadyToRun() nella vera app (vedi
	// il commento gemello in test_recalc_dirty_cells.cpp): senza questa,
	// SUBTOTAL/OFFSET (usate sotto per gli intervalli dinamici) non
	// sarebbero riconosciute come funzioni con nome in questo eseguibile
	// di test.
	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("10", cell(1, 1), doc, true); // A1
	TryToParseString("20", cell(2, 1), doc, true); // B1
	TryToParseString("=A1+B1", cell(3, 1), doc, true); // C1, una vera formula
	TryToParseString("Ciao Atomo123", cell(4, 1), doc, true); // D1, testo normale
	RecalculateAll(doc);

	Check(!view->ShowFormulas(), "il toggle Mostra formule parte spento");

	BString c1Value = view->FormattedCellText(cell(3, 1));
	Check(c1Value == "30", "con il toggle spento C1 mostra il valore calcolato (30)");

	view->ToggleShowFormulas();
	Check(view->ShowFormulas(), "ToggleShowFormulas lo accende");

	BString c1Formula = view->FormattedCellText(cell(3, 1));
	Check(c1Formula == "=A1+B1",
		"con il toggle acceso C1 mostra il testo della formula con \"=\" iniziale, non il valore");

	BString a1Text = view->FormattedCellText(cell(1, 1));
	Check(a1Text == "10",
		"una cella senza formula (A1, un numero digitato a mano) resta invariata col toggle acceso");

	BString d1Text = view->FormattedCellText(cell(4, 1));
	Check(d1Text == "Ciao Atomo123",
		"una cella di solo testo (D1) resta invariata col toggle acceso");

	view->ToggleShowFormulas();
	Check(!view->ShowFormulas(), "un secondo ToggleShowFormulas lo rispegne");

	BString c1Again = view->FormattedCellText(cell(3, 1));
	Check(c1Again == "30", "con il toggle di nuovo spento C1 torna a mostrare il valore calcolato");

	// Traccia precedenti/dipendenti: catena A1,B1 -> C1 -> E1, piu' F1
	// con un riferimento sintatticamente tra fogli (anche se punta al
	// foglio stesso -- "Foglio1!Cella" e' SEMPRE codificato come valXRef
	// dal parser, vedi test_xsheet_formulas.cpp) per verificare che
	// venga escluso dai precedenti, limite noto e dichiarato di questa
	// prima versione (CFormulaIterator salta apposta valXRef/valXRange).
	TryToParseString("=C1*2", cell(5, 1), doc, true); // E1
	TryToParseString("=Foglio1!A1+A1", cell(6, 1), doc, true); // F1
	RecalculateAll(doc);

	std::vector<cell> precedents;
	doc->GetPrecedents(cell(3, 1), precedents); // C1 = A1+B1
	Check(precedents.size() == 2 && precedents[0] == cell(1, 1) && precedents[1] == cell(2, 1),
		"GetPrecedents(C1) restituisce esattamente {A1, B1}, nell'ordine in cui compaiono nella formula");

	std::vector<cell> noFormulaPrecedents;
	doc->GetPrecedents(cell(1, 1), noFormulaPrecedents); // A1, nessuna formula
	Check(noFormulaPrecedents.empty(), "GetPrecedents su una cella senza formula (A1) e' vuoto");

	// A1 e' referenziata sia da C1 (=A1+B1) sia da F1 (=Foglio1!A1+A1,
	// nel suo pezzo di riferimento LOCALE "+A1") -- entrambe sono
	// dipendenti dirette, F1 non e' esclusa: solo il riferimento
	// sintatticamente tra fogli dentro F1 lo e' (verificato sopra via
	// GetPrecedents(F1)).
	std::vector<cell> dependentsOfA1;
	doc->GetDependents(cell(1, 1), dependentsOfA1); // A1
	Check(dependentsOfA1.size() == 2 && dependentsOfA1[0] == cell(3, 1) && dependentsOfA1[1] == cell(6, 1),
		"GetDependents(A1) trova sia C1 che F1, le due formule che referenziano A1 localmente");

	std::vector<cell> dependentsOfC1;
	doc->GetDependents(cell(3, 1), dependentsOfC1); // C1
	Check(dependentsOfC1.size() == 1 && dependentsOfC1[0] == cell(5, 1),
		"GetDependents(C1) trova E1 (=C1*2), non A1/B1 (i suoi precedenti, non i suoi dipendenti)");

	std::vector<cell> precedentsOfF1;
	doc->GetPrecedents(cell(6, 1), precedentsOfF1); // F1 = Foglio1!A1 + A1
	Check(precedentsOfF1.size() == 1 && precedentsOfF1[0] == cell(1, 1),
		"GetPrecedents(F1) contiene solo il riferimento locale ad A1: quello con sintassi "
		"\"Foglio1!A1\" e' escluso (limite noto, stesso sheet o no non conta)");

	Check(!view->ShowPrecedents() && !view->ShowDependents(),
		"i due interruttori Traccia precedenti/dipendenti partono spenti");

	view->SetSelection(cell(3, 1)); // C1
	view->ToggleTracePrecedents();
	Check(view->ShowPrecedents(), "ToggleTracePrecedents lo accende");
	Check(view->PrecedentTargets().size() == 2
			&& view->PrecedentTargets()[0] == cell(1, 1) && view->PrecedentTargets()[1] == cell(2, 1),
		"con C1 attiva, PrecedentTargets() e' {A1, B1}, calcolato subito all'accensione");

	view->SetSelection(cell(1, 1)); // A1
	Check(view->PrecedentTargets().empty(),
		"spostando la selezione su A1 (senza formula), PrecedentTargets() si aggiorna a vuoto dal vivo");

	view->ToggleTraceDependents();
	Check(view->ShowDependents(), "ToggleTraceDependents lo accende (insieme a Precedenti, come Excel)");
	Check(view->DependentTargets().size() == 2
			&& view->DependentTargets()[0] == cell(3, 1) && view->DependentTargets()[1] == cell(6, 1),
		"con A1 attiva, DependentTargets() e' {C1, F1}");

	view->RemoveTraceArrows();
	Check(!view->ShowPrecedents() && !view->ShowDependents()
			&& view->PrecedentTargets().empty() && view->DependentTargets().empty(),
		"RemoveTraceArrows spegne entrambi gli interruttori e ripulisce le frecce");

	// Finestra di controllo: terzo e ultimo pezzo delle Formula auditing
	// views. Testato tramite i metodi pubblici di MainWindow (stesso
	// principio di HandleDefineName in test_names.cpp) invece che sulla
	// BListView interna di WatchWindow, che vive su un altro thread.
	Check(win->WatchedCellCount() == 0, "la Finestra di controllo parte senza celle appuntate");

	view->SetSelection(cell(3, 1)); // C1
	win->AddSelectionToWatchWindow();
	Check(win->WatchedCellCount() == 1 && win->WatchedCellAt(0) == cell(3, 1),
		"AddSelectionToWatchWindow su C1 aggiunge esattamente una riga per C1");

	win->AddSelectionToWatchWindow();
	Check(win->WatchedCellCount() == 1,
		"aggiungere di nuovo la stessa selezione (C1) non crea una riga duplicata");

	view->SetSelection(cell(1, 1)); // A1
	view->ExtendSelection(cell(2, 1)); // estende ad A1:B1
	win->AddSelectionToWatchWindow();
	Check(win->WatchedCellCount() == 3
			&& win->WatchedCellAt(1) == cell(1, 1) && win->WatchedCellAt(2) == cell(2, 1),
		"AddSelectionToWatchWindow su un intervallo (A1:B1) aggiunge una riga per ogni cella nuova");

	win->RefreshWatchWindow(); // non deve crashare, si limita a ricalcolare il testo

	BMessage removeFirst(kMsgWatchRemoveRow);
	removeFirst.AddInt32("row", 0);
	win->MessageReceived(&removeFirst);
	Check(win->WatchedCellCount() == 2 && win->WatchedCellAt(0) == cell(1, 1),
		"kMsgWatchRemoveRow con \"row\"=0 rimuove la prima riga (C1), le altre scalano su");

	// Intervalli dinamici (Fase 36, "OFFSET(...):OFFSET(...)"): la parte
	// di calcolo (valRefRange/opRangeOp, il primo argomento di OFFSET
	// come vero riferimento, GetPrecedents) e' gia' coperta a fondo in
	// engine/tests/dynamic_range_test.cpp -- qui si verifica SOLO
	// quello che richiede una vera app (round-trip nella barra della
	// formula, inserimento riga tramite la vera SheetView::InsertRows),
	// perche' CFormula::UnMangle/ftoa dipende da gFontSizeTable/
	// BFont::StringWidth, non affidabile nel binario headless
	// dell'engine (vedi il commento in cima a dynamic_range_test.cpp).
	TryToParseString("10", cell(8, 11), doc, true); // H11
	TryToParseString("100", cell(8, 12), doc, true); // H12
	TryToParseString("200", cell(8, 13), doc, true); // H13
	TryToParseString("300", cell(8, 14), doc, true); // H14
	TryToParseString("10", cell(8, 15), doc, true); // H15
	TryToParseString("=SUBTOTAL(9;OFFSET(H11;0;1):OFFSET(H15;0;-1))", cell(10, 5), doc, true); // J5
	doc->CalcCell(cell(10, 5));
	Value dynRangeVal;
	doc->GetValue(cell(10, 5), dynRangeVal);
	Check((double)dynRangeVal == 600.0,
		"SUBTOTAL(9,OFFSET(H11,0,1):OFFSET(H15,0,-1)) = 600 (H12+H13+H14), la formula esatta di agile-kanban-board.xlsx");

	char dynRangeText[256];
	doc->GetCellFormula(cell(10, 5), dynRangeText, sizeof(dynRangeText), false);
	Check(strstr(dynRangeText, "OFFSET") != NULL && strstr(dynRangeText, ":") != NULL,
		"la formula di J5 si ricostruisce ancora come OFFSET(...):OFFSET(...) nella barra della formula");

	// Inserimento riga (SheetView::InsertRows, la vera scorciatoia
	// "Inserisci riga" del menu Dati): selezionare la riga 1 e inserire
	// sposta OGNI riga sotto di 1 -- H11/H15 diventano H12/H16, la
	// formula J5 stessa si sposta a J6. Il valore resta 600 (le stesse
	// tre celle, ora una riga piu' in basso), il testo deve seguirle.
	view->SetSelection(cell(1, 1));
	view->InsertRows();
	Value dynRangeValAfterInsert;
	doc->GetValue(cell(10, 6), dynRangeValAfterInsert);
	Check((double)dynRangeValAfterInsert == 600.0,
		"dopo l'inserimento di una riga sopra la riga 1, la formula (ora in J6) vale ancora 600");

	char dynRangeTextAfterInsert[256];
	doc->GetCellFormula(cell(10, 6), dynRangeTextAfterInsert, sizeof(dynRangeTextAfterInsert), false);
	Check(strstr(dynRangeTextAfterInsert, "H12") != NULL && strstr(dynRangeTextAfterInsert, "H16") != NULL,
		"dopo l'inserimento la formula si aggiorna a OFFSET(H12,...):OFFSET(H16,...), era H11/H15");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
