/*
	test_recalc_seeding.cpp

	Verifica che il seme preciso aggiunto a RecalculateActiveWorkbook/
	RecalculateOwningWorkbook (varianti range/vector<cell>, oltre a
	quella gia' esistente a singola cella) per incolla/riempi/ordina/
	inserisci/elimina riga/colonna/annulla non perda nessun dipendente
	reale -- il rischio dichiarato di questa fase (vedi ROADMAP.md,
	"grafo delle dipendenze", seguito diretto della Fase 3). Non
	misura QUANTE celle vengono ricalcolate (nessun contatore esposto
	da CalcCell in produzione, non vale la pena aggiungerne uno solo
	per questo), verifica invece la CORRETTEZZA end-to-end: un
	dipendente reale (anche fuori dall'intervallo toccato direttamente)
	deve comunque aggiornarsi.

	Serve una vera MainWindow (non la sola SheetView), stesso principio
	di test_paste_range.cpp: le operazioni testate sono suoi metodi
	pubblici.
*/

#include <cstdio>

#include <Application.h>
#include <Clipboard.h>
#include <Message.h>

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
	BApplication app("application/x-vnd.Atomo-TestRecalcSeeding");

	// Incolla: una formula FUORI dall'intervallo incollato che legge
	// una cella DENTRO l'intervallo deve comunque vedersi aggiornare.
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		CContainer* doc = win->GetSheetView()->Document();

		TryToParseString("=A1*10", cell(5, 5), doc, true); // E5, fuori dall'incolla
		win->RecalculateActiveWorkbook();

		if (be_clipboard->Lock())
		{
			BMessage* clip = be_clipboard->Data();
			clip->MakeEmpty();
			clip->AddData("text/plain", B_MIME_TYPE, "7", 1);
			be_clipboard->Commit();
			be_clipboard->Unlock();
		}
		win->GetSheetView()->SetSelection(cell(1, 1)); // A1
		win->PasteSelection();

		Check(NumAt(doc, 1, 1) == 7, "incolla: A1 riceve il valore incollato");
		Check(NumAt(doc, 5, 5) == 70,
			"incolla: E5 (=A1*10, fuori dall'intervallo incollato) si aggiorna comunque");

		win->Unlock();
	}

	// Ordina: una formula che legge la colonna chiave DOPO l'ordinamento
	// deve riflettere il nuovo ordine.
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		CContainer* doc = win->GetSheetView()->Document();

		TryToParseString("3", cell(1, 1), doc, true);
		TryToParseString("1", cell(1, 2), doc, true);
		TryToParseString("2", cell(1, 3), doc, true);
		TryToParseString("=A1", cell(3, 1), doc, true); // C1, fuori dall'intervallo ordinato
		win->RecalculateActiveWorkbook();

		SheetView* sv = win->GetSheetView();
		sv->SetSelection(cell(1, 1));
		sv->ExtendSelection(cell(1, 3));
		sv->SortSelection(true);

		Check(NumAt(doc, 1, 1) == 1 && NumAt(doc, 1, 2) == 2 && NumAt(doc, 1, 3) == 3,
			"ordina: A1:A3 ora in ordine crescente");
		Check(NumAt(doc, 3, 1) == 1,
			"ordina: C1 (=A1) si aggiorna al nuovo valore di A1 dopo l'ordinamento");

		win->Unlock();
	}

	// Inserisci riga: una formula sotto il punto di inserimento, che
	// referenzia una cella ANCORA piu' sotto (quindi anche lei si
	// sposta), deve continuare a dare il risultato giusto dopo lo
	// spostamento di entrambe.
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		CContainer* doc = win->GetSheetView()->Document();

		TryToParseString("5", cell(1, 10), doc, true); // A10
		TryToParseString("=A10*2", cell(1, 5), doc, true); // A5, sopra il punto di inserimento
		win->RecalculateActiveWorkbook();
		Check(NumAt(doc, 1, 5) == 10, "banco di prova: A5=A10*2=10 prima dell'inserimento");

		SheetView* sv = win->GetSheetView();
		sv->SetSelection(cell(1, 3)); // inserisce una riga sopra la riga 3
		sv->InsertRows();

		// A5 (che referenziava A10) e' ora scesa a A6, A10 e' ora A11:
		// A6 deve ancora dare 2 * il valore di A11.
		Check(NumAt(doc, 1, 11) == 5, "inserisci riga: il valore originale segue lo spostamento (A10->A11)");
		Check(NumAt(doc, 1, 6) == 10,
			"inserisci riga: la formula spostata (A5->A6) continua a leggere il riferimento spostato correttamente");

		win->Unlock();
	}

	// Elimina riga: sia la cella referenziata SIA chi la referenzia
	// vivono SOTTO la riga cancellata (nessuna delle due e' lei stessa
	// cancellata) -- entrambe si spostano in su di una riga, e il
	// riferimento interno alla formula deve seguirle correttamente.
	// (Nota: una formula che referenzia DIRETTAMENTE la riga cancellata
	// e' un caso a parte, con un problema gia' preesistente e
	// indipendente da questa modifica in CFormula::UpdateReferences --
	// non verificato qui.)
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		CContainer* doc = win->GetSheetView()->Document();

		TryToParseString("42", cell(1, 10), doc, true); // A10
		TryToParseString("=A10+1", cell(1, 15), doc, true); // A15
		win->RecalculateActiveWorkbook();
		Check(NumAt(doc, 1, 15) == 43, "banco di prova: A15=A10+1=43 prima della cancellazione");

		SheetView* sv = win->GetSheetView();
		sv->SetSelection(cell(1, 3)); // cancella la riga 3, sopra entrambe
		sv->DeleteRows();

		// Entrambe scendono di una riga (A10->A9, A15->A14); il
		// riferimento interno alla formula spostata deve seguire A10
		// fino alla sua nuova posizione (A9), non restare ancorato al
		// vecchio numero di riga.
		Check(NumAt(doc, 1, 9) == 42, "elimina riga: il valore originale segue lo spostamento (A10->A9)");
		Check(NumAt(doc, 1, 14) == 43,
			"elimina riga: la formula spostata (A15->A14) segue correttamente il riferimento spostato (A10->A9)");

		win->Unlock();
	}

	// Annulla: ripristinare uno stato precedente deve far ricalcolare
	// correttamente un dipendente esterno all'intervallo annullato.
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		CContainer* doc = win->GetSheetView()->Document();
		SheetView* sv = win->GetSheetView();

		TryToParseString("1", cell(1, 1), doc, true); // A1
		TryToParseString("=A1+100", cell(5, 1), doc, true); // E1, fuori dalla cella annullata
		win->RecalculateActiveWorkbook();
		Check(NumAt(doc, 5, 1) == 101, "banco di prova: E1=A1+100=101 prima della modifica");

		cell a1(1, 1);
		sv->SetSelection(a1);
		sv->SaveUndoState(range(1, 1, 1, 1));
		TryToParseString("9", a1, doc, true);
		win->RecalculateActiveWorkbook(&a1);
		Check(NumAt(doc, 5, 1) == 109, "banco di prova: E1=109 dopo la modifica ad A1");

		sv->Undo();
		Check(NumAt(doc, 1, 1) == 1, "annulla: A1 torna a 1");
		Check(NumAt(doc, 5, 1) == 101,
			"annulla: E1 (fuori dall'intervallo annullato) si aggiorna comunque dopo il ripristino");

		win->Unlock();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
