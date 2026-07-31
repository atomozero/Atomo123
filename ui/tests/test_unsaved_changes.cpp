/*
	test_unsaved_changes.cpp

	Verifica la protezione dalle modifiche non salvate (dirty flag +
	titolo della finestra + conferma prima di scartare), primo punto
	del giro di miglioramenti UI/UX chiesto dall'utente dopo la Fase 7:
	prima di questo, Nuovo/Apri/Esci scartavano il documento corrente
	senza alcun avviso, con un rischio concreto di perdita dati.

	Serve una vera MainWindow (non la sola SheetView): il flag
	"modificato", il nome del file e il titolo vivono li'. Verifica il
	titolo con BWindow::Title() (gia' pubblico) invece di analizzare
	direttamente fModified/fDocumentName, cosi' il test controlla lo
	stesso comportamento osservabile dall'utente.

	Limite noto (non testabile in automatico): ConfirmDiscardChanges()
	mostra un vero BAlert quando ci sono modifiche in sospeso, che
	richiede un clic reale per proseguire -- stesso limite gia'
	documentato altrove in questo progetto per i dialoghi modali
	interattivi (vedi test_selection.cpp per Maiusc+click). Il test
	verifica invece che, quando NON ci sono modifiche in sospeso,
	ConfirmDiscardChanges() torni true SENZA mostrare nessun BAlert
	(l'unico ramo raggiungibile senza un utente reale).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <String.h>

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
	BApplication app("application/x-vnd.Atomo-TestUnsavedChanges");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// Documento appena creato: nessuna modifica, titolo senza
	// l'asterisco, senza alcun nome di file (documento nuovo).
	Check(!win->IsModified(), "un documento appena creato non e' modificato");
	BString initialTitle(win->Title());
	Check(initialTitle.FindFirst('*') < 0,
		"il titolo iniziale non ha l'indicatore di modifica (nessun '*')");
	Check(initialTitle.FindFirst("Nuovo documento") >= 0,
		"il titolo iniziale indica un documento nuovo");

	// Senza modifiche in sospeso, ConfirmDiscardChanges() torna true
	// subito, senza mostrare nessun BAlert (altrimenti questo test si
	// bloccherebbe in attesa di un clic che non arrivera' mai).
	Check(win->ConfirmDiscardChanges(),
		"ConfirmDiscardChanges() torna true subito quando non ci sono modifiche");

	// Una modifica fatta tramite SheetView (non tramite MainWindow)
	// deve comunque marcare il documento come modificato: verifica
	// che il ponte SheetView::NotifyDocumentChanged() ->
	// MainWindow::DocumentChanged() funzioni davvero.
	TryToParseString("42", cell(1, 1), doc, true); // A1, scrittura diretta

	// ClearSelection chiama sempre NotifyDocumentChanged() (vedi
	// SheetView.cpp): la usiamo come mutazione di prova.
	view->SetSelection(cell(1, 1));
	view->ClearSelection();

	Check(win->IsModified(), "una mutazione fatta tramite SheetView marca il documento come modificato");
	BString modifiedTitle(win->Title());
	Check(modifiedTitle.FindFirst('*') == 0,
		"il titolo mostra l'indicatore di modifica ('*' in testa) dopo la mutazione");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
