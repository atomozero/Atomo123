/*
	test_open_async.cpp

	Verifica l'apertura file su un thread separato (Fase 31,
	MainWindow::OpenFileAsync): richiesta esplicita dell'utente dopo
	aver misurato ~3 minuti di finestra completamente bloccata (nessuna
	risposta nemmeno a "hey", vedi ROADMAP.md) aprendo un file XLSX
	reale a 13 fogli. A differenza di OpenFile() (sincrona, verificata
	da test_multisheet.cpp e molti altri), qui il risultato arriva in
	un momento SUCCESSIVO tramite kMsgFileLoadResult -- questo test
	verifica sia il risultato finale (stessi identici fogli/valori che
	produrrebbe OpenFile) sia, soprattutto, che IsOpeningFile() torni
	subito true e resti tale finche' il thread di lavoro non ha
	finito, la prova diretta che la chiamata non blocca.

	Stesso principio di test_multisheet.cpp per il file di prova su
	disco (MainWindow::OpenFile[Async] prende un entry_ref, non uno
	stream).
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <OS.h>

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
	BApplication app("application/x-vnd.Atomo-TestOpenAsync");

	const char* path = "/tmp/test_open_async.ascd";

	CContainer* doc1 = new CContainer(NULL, NULL);
	TryToParseString("100", cell(1, 1), doc1, true);
	CContainer* doc2 = new CContainer(NULL, NULL);
	// Riferimento incrociato: verifica che il resolver locale del
	// thread di lavoro (OpenFileLocalResolver in MainWindow.cpp) trovi
	// davvero il primo foglio, esattamente come farebbe il resolver
	// vero (this) usato da OpenFile() sincrona.
	TryToParseString("=Primo!A1+5", cell(1, 1), doc2, true);

	std::vector<AscdSheet> sheets;
	AscdSheet s1; s1.name = "Primo"; s1.doc = doc1; sheets.push_back(s1);
	AscdSheet s2; s2.name = "Secondo"; s2.doc = doc2; sheets.push_back(s2);

	{
		BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCDBook(sheets, &file);
		Check(err == B_OK, "preparazione del file di prova a due fogli riuscita");
	}
	doc1->Release();
	doc2->Release();

	MainWindow* win = new MainWindow();
	win->Show();

	entry_ref ref;
	BEntry entry(path);
	Check(entry.GetRef(&ref) == B_OK, "entry_ref del file di prova ottenuto correttamente");

	win->Lock();
	Check(!win->IsOpeningFile(), "IsOpeningFile() e' falso prima di qualunque apertura");
	Check(!win->IsFooterProgressVisible(),
		"la barra di avanzamento nel footer e' nascosta prima di qualunque apertura");
	win->OpenFileAsync(ref);
	// Vero subito dopo che OpenFileAsync ritorna, PRIMA che il thread
	// di lavoro possa aver gia' finito -- la prova diretta che la
	// chiamata non e' bloccante (l'intero senso di questa fase: la
	// vecchia OpenFile sincrona restituiva il controllo solo a
	// caricamento GIA' finito, IsOpeningFile() sarebbe sempre stato
	// falso a questo punto).
	bool wasOpeningRightAfterCall = win->IsOpeningFile();
	bool footerVisibleRightAfterCall = win->IsFooterProgressVisible();
	win->Unlock();
	Check(wasOpeningRightAfterCall,
		"OpenFileAsync ritorna subito (IsOpeningFile() e' vero appena dopo la chiamata, "
		"il thread di lavoro sta ancora caricando)");
	Check(footerVisibleRightAfterCall,
		"la barra di avanzamento nel footer (Fase 33) e' visibile appena dopo la chiamata, "
		"al posto dell'indicatore di modalita'/statistiche di selezione");

	// Il thread della finestra deve restare libero di elaborare i
	// propri messaggi (incluso kMsgFileLoadResult a fine caricamento)
	// SENZA che il thread del test tenga Lock() nel frattempo -- stesso
	// motivo per cui questo test non puo' verificare lo stato subito
	// dopo OpenFileAsync come fa test_multisheet.cpp con OpenFile.
	bigtime_t start = system_time();
	bool finished = false;
	while (system_time() - start < 10000000) // 10s, ampio margine per un file di prova minuscolo
	{
		snooze(10000);
		win->Lock();
		finished = !win->IsOpeningFile();
		win->Unlock();
		if (finished)
			break;
	}
	Check(finished, "il caricamento asincrono finisce entro il timeout di 10s");

	win->Lock();
	Check(!win->IsFooterProgressVisible(),
		"la barra di avanzamento nel footer torna nascosta a caricamento finito");
	Check(win->SheetCount() == 2, "l'apertura asincrona legge entrambi i fogli");
	if (win->SheetCount() == 2)
	{
		Check(strcmp(win->SheetName(0), "Primo") == 0,
			"il nome del primo foglio e' preservato dopo l'apertura asincrona");
		Check(strcmp(win->SheetName(1), "Secondo") == 0,
			"il nome del secondo foglio e' preservato dopo l'apertura asincrona");
	}

	CContainer* activeDoc = win->GetSheetView()->Document();
	Value v;
	activeDoc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 100,
		"A1 del primo foglio vale 100 dopo l'apertura asincrona");

	win->SwitchToSheet(1);
	CContainer* secondDoc = win->GetSheetView()->Document();
	secondDoc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 105,
		"A1 del secondo foglio (=Primo!A1+5) calcola 105: il riferimento incrociato si e' "
		"risolto durante il ricalcolo fatto SUL THREAD DI LAVORO (resolver locale), non "
		"lasciato a NaN in attesa di un ricalcolo successivo");
	win->Unlock();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
