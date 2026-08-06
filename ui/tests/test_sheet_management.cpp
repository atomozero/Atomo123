/*
	test_sheet_management.cpp

	Verifica Nuovo/Elimina/Rinomina foglio (Fase 13): assenti prima di
	questo punto (confermato nel codice, un commento in MainWindow.cpp
	diceva letteralmente "in futuro"). NewSheet/DeleteSheetNoConfirm/
	RenameSheet sono pubblici apposta per essere testabili direttamente,
	stesso principio di HandleGoToRequest in test_goto.cpp -- richiede
	una vera MainWindow.

	Limite noto (non testabile in automatico): DeleteSheet() (a
	differenza di DeleteSheetNoConfirm() usata qui sotto) mostra un vero
	BAlert di conferma, e RenameSheet() ne mostra uno di errore quando
	il nome scelto e' gia' usato da un altro foglio -- entrambi
	bloccherebbero questo test in attesa di un clic reale, stesso
	limite gia' documentato in tests/test_unsaved_changes.cpp per
	ConfirmDiscardChanges. Qui si verifica solo la logica vera e
	propria (DeleteSheetNoConfirm, RenameSheet con un nome libero), non
	i dialoghi di conferma/errore che la precedono nel percorso normale.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
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
	BApplication app("application/x-vnd.Atomo-TestSheetManagement");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	Check(win->SheetCount() == 1, "una MainWindow appena creata ha un solo foglio");
	Check(strcmp(win->SheetName(0), "Foglio1") == 0,
		"il foglio iniziale si chiama \"Foglio1\"");

	// --- Nuovo foglio: nome libero scelto da solo, diventa subito
	// quello attivo. ---
	win->NewSheet();
	Check(win->SheetCount() == 2, "NewSheet() aggiunge un secondo foglio");
	Check(strcmp(win->SheetName(1), "Foglio2") == 0,
		"il secondo foglio si chiama \"Foglio2\" (primo nome libero, non un numero fisso)");
	Check(win->ActiveSheetIndex() == 1, "NewSheet() passa subito al foglio appena creato");

	win->NewSheet();
	Check(win->SheetCount() == 3, "NewSheet() aggiunge un terzo foglio");
	Check(strcmp(win->SheetName(2), "Foglio3") == 0,
		"il terzo foglio si chiama \"Foglio3\"");

	// --- Rinomina con un nome libero: il percorso normale, nessun
	// BAlert (mostrato solo nel caso di nome duplicato, non testabile
	// in automatico per lo stesso motivo di DeleteSheet -- vedi il
	// commento in cima al file). ---
	win->RenameSheet(0, "Dati");
	Check(strcmp(win->SheetName(0), "Dati") == 0,
		"RenameSheet(0, \"Dati\") rinomina davvero il primo foglio");

	// --- Scrivere in una cella di un foglio appena creato/rinominato
	// funziona come su qualunque altro foglio (nessuna corruzione di
	// fDoc durante NewSheet/RenameSheet). ---
	win->SwitchToSheet(0); // "Dati"
	TryToParseString("42", cell(1, 1), win->GetSheetView()->Document(), true);
	win->GetSheetView()->Document()->CalcCell(cell(1, 1));
	Value v;
	win->GetSheetView()->Document()->GetValue(cell(1, 1), v);
	Check((double)v == 42.0, "scrivere in A1 del foglio rinominato \"Dati\" funziona normalmente");

	// --- Elimina un foglio che NON e' quello attivo: l'indice attivo
	// resta sullo stesso foglio logico (non su quello che ha preso il
	// suo posto numerico dopo l'erase). ---
	win->SwitchToSheet(2); // "Foglio3", indice 2
	win->DeleteSheetNoConfirm(0); // elimina "Dati" (indice 0), non quello attivo
	Check(win->SheetCount() == 2, "DeleteSheet(0) rimuove un foglio, ne restano 2");
	Check(strcmp(win->SheetName(win->ActiveSheetIndex()), "Foglio3") == 0,
		"eliminare un foglio non attivo non sposta la selezione: resta su \"Foglio3\"");

	// --- Elimina il foglio ATTIVO: la selezione si sposta su un altro
	// foglio valido invece di restare su un indice ormai fuori
	// intervallo o su un CContainer gia' rilasciato. ---
	int activeBefore = win->ActiveSheetIndex();
	win->DeleteSheetNoConfirm(activeBefore);
	Check(win->SheetCount() == 1, "DeleteSheet(foglio attivo) rimuove il foglio, ne resta 1");
	Check(win->ActiveSheetIndex() >= 0 && win->ActiveSheetIndex() < win->SheetCount(),
		"dopo aver eliminato il foglio attivo, l'indice attivo punta a un foglio ancora valido");

	// --- L'unico foglio rimasto non si puo' eliminare. ---
	win->DeleteSheetNoConfirm(0);
	Check(win->SheetCount() == 1,
		"DeleteSheet sull'unico foglio rimasto non fa nulla (un foglio deve sempre restare)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
