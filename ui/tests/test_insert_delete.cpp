/*
	test_insert_delete.cpp

	Verifica Inserisci/Elimina riga e colonna (SheetView::InsertRows/
	InsertColumns/DeleteRows/DeleteColumns), ultimo punto rimasto della
	Fase 7 (ROADMAP.md): sposta le celle esistenti oltre il punto di
	inserimento/cancellazione e aggiorna i riferimenti nelle formule di
	OGNI cella del documento -- anche di quelle che non si spostano
	fisicamente, se la loro formula fa riferimento a una cella che si
	sposta -- riusando CContainer::MoveCell con uno SplitType, lo
	stesso meccanismo (mai cambiato) del Sum-It storico.

	Ogni scenario usa una zona del foglio non condivisa con gli altri
	(invece di annullare/ripetere avanti e indietro sulle stesse
	celle), cosi' un test che fallisce non trascina in errore quelli
	dopo di lui. Usa SheetView::SetSelection/ExtendSelection
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
		: BWindow(BRect(100, 100, 700, 500), "test-insert-delete", B_TITLED_WINDOW, 0)
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
	BApplication app("application/x-vnd.Atomo-TestInsertDelete");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(400, 300);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// --- Inserisci riga (colonna A, righe 1-3) ---
	// A1=1, A2=2, A3=3. Selezionando A2 (una sola riga) e inserendo,
	// A2 deve svuotarsi e il vecchio A2/A3 spostarsi a A3/A4 -- A1
	// resta fermo (sopra il punto di inserimento).
	TryToParseString("1", cell(1, 1), doc, true);
	TryToParseString("2", cell(1, 2), doc, true);
	TryToParseString("3", cell(1, 3), doc, true);

	view->SetSelection(cell(1, 2));
	view->InsertRows();

	Check(NumAt(doc, 1, 1) == 1.0, "Inserisci riga: A1 (sopra il punto) resta fermo");
	Check(IsEmpty(doc, 1, 2), "Inserisci riga: la riga inserita (A2) e' vuota");
	Check(NumAt(doc, 1, 3) == 2.0 && NumAt(doc, 1, 4) == 3.0,
		"Inserisci riga: le righe successive si spostano in basso (A3=2, A4=3)");

	Check(view->CanUndo(), "Inserisci riga e' annullabile");
	view->Undo();
	Check(NumAt(doc, 1, 1) == 1.0 && NumAt(doc, 1, 2) == 2.0 && NumAt(doc, 1, 3) == 3.0,
		"Annulla dopo Inserisci riga ripristina lo stato originale (1, 2, 3)");

	// --- Inserisci piu' righe insieme (colonna B, righe 1-3) ---
	// Selezionando due righe si inseriscono due righe vuote, non una
	// sola.
	TryToParseString("1", cell(2, 1), doc, true); // B1
	TryToParseString("2", cell(2, 2), doc, true); // B2
	TryToParseString("3", cell(2, 3), doc, true); // B3

	view->SetSelection(cell(2, 2));
	view->ExtendSelection(cell(2, 3)); // B2:B3, due righe
	view->InsertRows();

	Check(IsEmpty(doc, 2, 2) && IsEmpty(doc, 2, 3),
		"Inserisci riga su una selezione di due righe inserisce due righe vuote");
	Check(NumAt(doc, 2, 4) == 2.0 && NumAt(doc, 2, 5) == 3.0,
		"le righe originali si spostano di due posizioni (B4=2, B5=3)");

	// --- Riferimento sopra il punto che punta a una cella sotto (colonna C) ---
	// C1="=C3" (=30 dopo aver scritto C3=30); inserendo una riga in
	// C2, C3 diventa C4: C1 deve seguirla e diventare "=C4", pur
	// restando ferma lei stessa.
	TryToParseString("30", cell(3, 3), doc, true); // C3
	TryToParseString("=C3", cell(3, 1), doc, true); // C1
	doc->CalcCell(cell(3, 1));
	Check(NumAt(doc, 3, 1) == 30.0, "C1 (=C3) calcola 30 prima dell'inserimento");

	view->SetSelection(cell(3, 2));
	view->InsertRows();

	char c1Formula[512];
	doc->GetCellFormula(cell(3, 1), c1Formula, false);
	Check(strstr(c1Formula, "C4") != NULL,
		"il riferimento di una cella FERMA (C1) segue la cella spostata (diventa =C4)");
	Check(NumAt(doc, 3, 1) == 30.0,
		"C1 continua a calcolare 30 (ora leggendo C4, dove il 30 si e' spostato)");

	// --- Inserisci colonna (riga 10, colonne A-C) ---
	// Speculare a riga, sull'asse orizzontale. A10=10, B10=20, C10=30;
	// selezionando B10 e inserendo una colonna, B10 deve svuotarsi e
	// il vecchio B10/C10 spostarsi a C10/D10.
	TryToParseString("10", cell(1, 10), doc, true); // A10
	TryToParseString("20", cell(2, 10), doc, true); // B10
	TryToParseString("30", cell(3, 10), doc, true); // C10

	view->SetSelection(cell(2, 10));
	view->InsertColumns();

	Check(NumAt(doc, 1, 10) == 10.0, "Inserisci colonna: A10 (a sinistra del punto) resta fermo");
	Check(IsEmpty(doc, 2, 10), "Inserisci colonna: la colonna inserita (B10) e' vuota");
	Check(NumAt(doc, 3, 10) == 20.0 && NumAt(doc, 4, 10) == 30.0,
		"Inserisci colonna: le colonne successive si spostano a destra (C10=20, D10=30)");

	// --- Elimina riga (colonna A, righe 20-22) ---
	// A20=100, A21=200, A22=300; eliminando la riga 21 (A21), A21
	// sparisce e A22 sale a A21.
	TryToParseString("100", cell(1, 20), doc, true);
	TryToParseString("200", cell(1, 21), doc, true);
	TryToParseString("300", cell(1, 22), doc, true);

	view->SetSelection(cell(1, 21));
	view->DeleteRows();

	Check(NumAt(doc, 1, 20) == 100.0, "Elimina riga: la riga sopra (A20) resta ferma");
	Check(NumAt(doc, 1, 21) == 300.0,
		"Elimina riga: la riga sotto sale a occupare il posto di quella eliminata (A21=300)");
	Check(IsEmpty(doc, 1, 22), "Elimina riga: la vecchia ultima posizione (A22) resta vuota");

	Check(view->CanUndo(), "Elimina riga e' annullabile");
	view->Undo();
	Check(NumAt(doc, 1, 20) == 100.0 && NumAt(doc, 1, 21) == 200.0 && NumAt(doc, 1, 22) == 300.0,
		"Annulla dopo Elimina riga ripristina lo stato originale (100, 200, 300)");

	// --- Elimina colonna (riga 30, colonne F-H) ---
	TryToParseString("1", cell(6, 30), doc, true); // F30
	TryToParseString("2", cell(7, 30), doc, true); // G30
	TryToParseString("3", cell(8, 30), doc, true); // H30

	view->SetSelection(cell(7, 30));
	view->DeleteColumns();

	Check(NumAt(doc, 6, 30) == 1.0, "Elimina colonna: la colonna a sinistra (F30) resta ferma");
	Check(NumAt(doc, 7, 30) == 3.0,
		"Elimina colonna: la colonna a destra prende il posto di quella eliminata (G30=3)");
	Check(IsEmpty(doc, 8, 30), "Elimina colonna: la vecchia ultima posizione (H30) resta vuota");

	// --- Inserimento rifiutato se spingerebbe dati fuori dal foglio ---
	// NON verificato chiamando InsertRows() con dati gia' nell'ultima
	// riga come si farebbe normalmente: quel ramo mostra un vero
	// BAlert bloccante (SheetView::InsertRows, il controllo prima di
	// mutare nulla) che in un test headless non ha nessun utente
	// pronto a cliccare "OK" -- resterebbe appeso per sempre. Stesso
	// limite gia' documentato altrove in questo progetto per i
	// dialoghi modali interattivi (vedi ConfirmDiscardChanges() in
	// test_unsaved_changes.cpp, testato solo sul ramo che NON mostra
	// alcun BAlert). Scoperto proprio scrivendo questo test: prima
	// funzionava per una fortuita coincidenza ambientale, non perche'
	// fosse davvero sicuro chiamarlo cosi'.

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
