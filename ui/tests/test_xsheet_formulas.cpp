/*
	test_xsheet_formulas.cpp

	Verifica i riferimenti fra fogli (Fase 9, "NomeFoglio!Cella") in
	una vera cartella di lavoro aperta tramite MainWindow::OpenFile:
	MainWindow implementa ISheetResolver (Container.h) e la collega a
	ogni CContainer aperto (AttachSheetResolver), poi ricalcola
	l'intera cartella di lavoro (RecalculateWorkbook, non solo il
	foglio attivo) cosi' una formula come "=Dettaglio!I56+8" mostra il
	valore corretto subito dopo l'apertura -- non solo nel motore
	isolato (vedi engine/tests/xsheet_test.cpp per la stessa verifica
	con un ISheetResolver di prova invece di una vera MainWindow/UI).

	Il file di prova si prepara SENZA nessun ISheetResolver collegato
	(nessuna MainWindow esiste ancora in questa fase): non serve, dato
	che "NomeFoglio!Cella" è sempre sintatticamente un riferimento
	incrociato (il nome del foglio si incorpora nel bytecode cosi'
	com'e', risolto solo in fase di calcolo -- vedi il commento su
	ISheetResolver in Container.h). Proprio questo e' il punto centrale
	di questo test: un file con un riferimento incrociato, salvato
	senza che quel foglio fosse ancora raggiungibile, deve comunque
	risolversi correttamente alla riapertura, una volta che
	MainWindow::OpenFile collega il resolver e ricalcola.
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <Entry.h>
#include <File.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"

static int gFailures = 0;

static void Check(bool condition, const char *what)
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
	BApplication app("application/x-vnd.Atomo-TestXSheetFormulas");

	const char *path = "/tmp/test_xsheet_formulas.ascd";

	CContainer *detail = new CContainer(NULL, NULL);
	CContainer *summary = new CContainer(NULL, NULL);

	TryToParseString("42", cell(9, 56), detail, true); // I56
	TryToParseString("=Dettaglio!I56+8", cell(2, 1), summary, true); // B1

	std::vector<AscdSheet> sheets;
	AscdSheet s1; s1.name = "Dettaglio"; s1.doc = detail;
	sheets.push_back(s1);
	AscdSheet s2; s2.name = "Riepilogo"; s2.doc = summary;
	sheets.push_back(s2);

	{
		BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCDBook(sheets, &file) == B_OK,
			"preparazione del file di prova a due fogli riuscita");
	}
	detail->Release();
	summary->Release();

	entry_ref ref;
	BEntry entry(path);
	Check(entry.GetRef(&ref) == B_OK, "entry_ref del file di prova ottenuto correttamente");

	MainWindow *win = new MainWindow();
	win->Show();
	win->Lock();

	win->OpenFile(ref);

	Check(win->SheetCount() == 2, "l'apertura legge entrambi i fogli");

	win->SwitchToSheet(1); // "Riepilogo"
	Value v;
	win->GetSheetView()->Document()->GetValue(cell(2, 1), v);
	Check((double)v == 50.0,
		"dopo l'apertura, B1 in Riepilogo (\"=Dettaglio!I56+8\") mostra già 50 (42+8), "
		"senza bisogno di toccare la cella");

	// La formula si ricostruisce correttamente anche nella barra
	// formule (stessa verifica di round-trip testuale di
	// engine/tests/xsheet_test.cpp, qui sulla vera MainWindow).
	char formula[256];
	win->GetSheetView()->Document()->GetCellFormula(cell(2, 1), formula, sizeof(formula), false);
	Check(strstr(formula, "'Dettaglio'!I56") != NULL,
		"la formula ricostruita mostra \"'Dettaglio'!I56\" (sempre fra apici, vedi UnMangle), "
		"non un indice numerico interno");

	// Se il foglio "Dettaglio" cambia dopo l'apertura, un nuovo
	// ricalcolo (lo stesso che ogni modifica innesca, vedi
	// MainWindow::RecalculateActiveWorkbook) propaga il cambiamento
	// anche a "Riepilogo" -- non solo al foglio toccato direttamente.
	win->SwitchToSheet(0); // "Dettaglio"
	TryToParseString("100", cell(9, 56), win->GetSheetView()->Document(), true);
	win->RecalculateActiveWorkbook();

	win->SwitchToSheet(1); // "Riepilogo"
	win->GetSheetView()->Document()->GetValue(cell(2, 1), v);
	Check((double)v == 108.0,
		"dopo aver modificato Dettaglio!I56 a 100, RecalculateActiveWorkbook propaga il cambiamento "
		"a Riepilogo (108 = 100+8), non solo al foglio toccato direttamente");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
