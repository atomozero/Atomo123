/*
	test_multisheet.cpp

	Verifica il supporto multi-foglio in MainWindow (Fase 9):
	apertura di una cartella di lavoro nativa multi-foglio (formato
	"ASCB", vedi AscdIO.h) con SheetCount()/SheetName()/
	ActiveSheetIndex()/SwitchToSheet(), e che cambiare foglio attivo
	ripunti davvero SheetView al documento giusto.

	Serve un vero file su disco (non solo un BPositionIO in memoria):
	MainWindow::OpenFile prende un entry_ref, non uno stream --
	stesso principio di test_ascd_io.cpp per il giro salva/ricarica,
	ma passando per l'API pubblica realmente usata dal menu File >
	Apri invece delle sole funzioni di AscdIO.
*/

#include <cstdio>
#include <cstring>
#include <utility>
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
	BApplication app("application/x-vnd.Atomo-TestMultiSheet");

	// Prepara una cartella di lavoro di prova con tre fogli su disco,
	// stesso formato "ASCB" che MainWindow::OpenFile riconosce per un
	// file nativo multi-foglio.
	const char* path = "/tmp/test_multisheet.ascd";

	CContainer* doc1 = new CContainer(NULL, NULL);
	TryToParseString("Primo foglio", cell(1, 1), doc1, true);

	CContainer* doc2 = new CContainer(NULL, NULL);
	TryToParseString("100", cell(1, 1), doc2, true);

	CContainer* doc3 = new CContainer(NULL, NULL);
	TryToParseString("Terzo foglio", cell(1, 1), doc3, true);

	std::vector<AscdSheet> sheets;
	AscdSheet s1; s1.name = "Alfa"; s1.doc = doc1;
	s1.colWidths.push_back(std::make_pair(1, 200.0f)); // larghezza personalizzata per A
	sheets.push_back(s1);
	AscdSheet s2; s2.name = "Beta"; s2.doc = doc2; sheets.push_back(s2); // nessuna larghezza personalizzata
	AscdSheet s3; s3.name = "Gamma"; s3.doc = doc3; sheets.push_back(s3);

	{
		BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCDBook(sheets, &file);
		Check(err == B_OK, "preparazione del file di prova a tre fogli riuscita");
	}
	doc1->Release();
	doc2->Release();
	doc3->Release();

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	Check(win->SheetCount() == 1,
		"un documento nuovo ha un solo foglio (\"Foglio1\")");

	entry_ref ref;
	BEntry entry(path);
	Check(entry.GetRef(&ref) == B_OK, "entry_ref del file di prova ottenuto correttamente");

	win->OpenFile(ref);

	Check(win->SheetCount() == 3, "l'apertura legge tutti e tre i fogli, non solo il primo");
	if (win->SheetCount() == 3)
	{
		Check(strcmp(win->SheetName(0), "Alfa") == 0, "il nome del primo foglio e' preservato");
		Check(strcmp(win->SheetName(1), "Beta") == 0, "il nome del secondo foglio e' preservato");
		Check(strcmp(win->SheetName(2), "Gamma") == 0, "il nome del terzo foglio e' preservato");
	}

	Check(win->ActiveSheetIndex() == 0, "il foglio attivo dopo l'apertura e' il primo (indice 0)");

	SheetView* view = win->GetSheetView();
	char text[512];
	view->Document()->GetCellFormula(cell(1, 1), text, sizeof(text), false);
	Check(strcmp(text, "Primo foglio") == 0,
		"SheetView mostra il contenuto del primo foglio subito dopo l'apertura");

	// La larghezza personalizzata del foglio "Alfa" (letta dal file di
	// prova) e' gia' applicata subito dopo l'apertura, non solo dopo un
	// cambio di foglio.
	Check(view->CellRect(cell(1, 1)).Width() == 200,
		"la larghezza di colonna personalizzata di Alfa e' applicata subito dopo l'apertura");

	win->SwitchToSheet(1);
	Check(win->ActiveSheetIndex() == 1, "SwitchToSheet(1) aggiorna l'indice del foglio attivo");
	Value v;
	win->GetSheetView()->Document()->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 100.0,
		"dopo il cambio SheetView mostra il contenuto del secondo foglio (100), non piu' del primo");
	Check(view->CellRect(cell(1, 1)).Width() == 80,
		"Beta (senza larghezze personalizzate) mostra la larghezza predefinita, "
		"non quella di Alfa");

	// Ridimensiona una colonna mentre Beta e' attivo, poi passa a Gamma
	// e torna a Beta: la larghezza scelta a mano deve sopravvivere al
	// cambio foglio avanti e indietro, non solo alla singola apertura.
	std::vector<std::pair<int, float> > betaWidth;
	betaWidth.push_back(std::make_pair(3, 55.0f));
	view->SetColumnWidths(betaWidth);
	Check(view->CellRect(cell(3, 1)).Width() == 55,
		"la larghezza di colonna 3 di Beta e' applicata subito dopo SetColumnWidths");

	win->SwitchToSheet(2);
	win->GetSheetView()->Document()->GetCellFormula(cell(1, 1), text, sizeof(text), false);
	Check(strcmp(text, "Terzo foglio") == 0,
		"dopo un secondo cambio SheetView mostra il contenuto del terzo foglio");
	Check(view->CellRect(cell(3, 1)).Width() == 80,
		"Gamma non eredita la larghezza personalizzata appena impostata su Beta");

	win->SwitchToSheet(1);
	Check(view->CellRect(cell(3, 1)).Width() == 55,
		"tornando su Beta la larghezza di colonna scelta a mano e' ancora quella (55), "
		"non e' andata persa nel cambio foglio avanti e indietro");

	// Torna su Gamma (indice 2) prima dei controlli sotto, che
	// verificano che un indice fuori dai limiti non sposti il foglio
	// attivo -- devono ripartire da uno stato noto.
	win->SwitchToSheet(2);

	// Indici fuori dai limiti non fanno nulla (ne' un crash ne' un
	// cambio di foglio silenzioso e sbagliato).
	win->SwitchToSheet(99);
	Check(win->ActiveSheetIndex() == 2,
		"SwitchToSheet con un indice fuori dai limiti non cambia nulla (resta sul terzo foglio)");
	win->SwitchToSheet(-1);
	Check(win->ActiveSheetIndex() == 2,
		"SwitchToSheet con un indice negativo non cambia nulla");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
