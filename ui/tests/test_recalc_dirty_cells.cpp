/*
	test_recalc_dirty_cells.cpp

	Verifica l'ottimizzazione "celle sporche" di RecalculateWorkbook
	(Fase 32, richiesta esplicita dell'utente dopo aver misurato 91s di
	ricalcolo su un file XLSX reale a 13 fogli): l'elenco delle celle
	CON formula di ogni foglio viene raccolto UNA SOLA VOLTA prima di
	tutte le passate (CollectFormulaCells in AscdIO.cpp), invece di
	riscandire ogni cella con contenuto -- comprese quelle letterali --
	a ogni passata.

	Il rischio principale di questo tipo di ottimizzazione e' che
	l'elenco raccolto all'inizio smetta di essere valido durante le
	passate successive -- qui si verifica esplicitamente il caso che
	potrebbe romperlo: una formula ad array (SEQUENCE) che "spilla" il
	proprio risultato nelle celle vicine scrivendo VALORI (mai
	formule), su una cartella di lavoro multi-foglio con anche un
	riferimento incrociato fra fogli e centinaia di celle puramente
	letterali (il caso reale che ha motivato l'ottimizzazione).
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <Entry.h>
#include <File.h>

#include <Path.h>
#include <Roster.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"
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
	BApplication app("application/x-vnd.Atomo-TestRecalcDirtyCells");

	// Stessa identica sequenza di App::ReadyToRun() nella vera app
	// (vedi il commento gemello in test_xlsx_named_functions_dynamic.cpp):
	// senza questa, SEQUENCE (usata sotto) non e' riconosciuta come
	// funzione con nome in questo eseguibile di test.
	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	const char* path = "/tmp/test_recalc_dirty_cells.ascd";

	// Foglio "Dati": centinaia di celle puramente letterali (il caso
	// reale che rende utile l'ottimizzazione -- un elenco di dati senza
	// nessuna formula) piu' UN valore che il foglio "Calc" referenzia
	// fra fogli.
	CContainer* dataDoc = new CContainer(NULL, NULL);
	for (int row = 1; row <= 500; row++)
	{
		char text[32];
		snprintf(text, sizeof(text), "riga%d", row);
		TryToParseString(text, cell(1, row), dataDoc, true);
	}
	TryToParseString("7", cell(2, 1), dataDoc, true); // B1, referenziato fra fogli sotto

	// Foglio "Calc": una formula ad array (SEQUENCE, spilla 3 celle in
	// verticale scrivendo VALORI, mai formule proprie) e una formula
	// con riferimento incrociato al foglio "Dati".
	CContainer* calcDoc = new CContainer(NULL, NULL);
	// decSep/listSep espliciti ('.'/','), come in engine/tests/
	// named_functions_test.cpp: SEQUENCE(3,1) ha bisogno di "," come
	// separatore di argomenti, non del listSep di locale predefinito di
	// questo eseguibile di test (';' in italiano), che leggerebbe "3,1"
	// come un unico argomento malformato.
	TryToParseString("=SEQUENCE(3,1)", cell(1, 1), calcDoc, true, '.', ','); // A1:A3
	TryToParseString("=Dati!B1*10", cell(2, 1), calcDoc, true, '.', ','); // B1

	std::vector<AscdSheet> sheets;
	AscdSheet s1; s1.name = "Dati"; s1.doc = dataDoc; sheets.push_back(s1);
	AscdSheet s2; s2.name = "Calc"; s2.doc = calcDoc; sheets.push_back(s2);

	{
		BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCDBook(sheets, &file);
		Check(err == B_OK, "preparazione del file di prova riuscita");
	}
	dataDoc->Release();
	calcDoc->Release();

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	entry_ref ref;
	BEntry entry(path);
	Check(entry.GetRef(&ref) == B_OK, "entry_ref del file di prova ottenuto correttamente");

	win->OpenFile(ref); // sincrona: usa RecalculateWorkbook esattamente come l'apertura reale

	Check(win->SheetCount() == 2, "l'apertura legge entrambi i fogli");

	CContainer* dati = win->GetSheetView()->Document();
	Value v;
	dati->GetValue(cell(1, 1), v);
	Check(v.fType == eTextData && strcmp((const char*)v, "riga1") == 0,
		"la prima cella letterale del foglio Dati e' preservata dopo il ricalcolo");
	dati->GetValue(cell(1, 500), v);
	Check(v.fType == eTextData && strcmp((const char*)v, "riga500") == 0,
		"l'ultima delle 500 celle letterali e' preservata (non saltata/corrotta "
		"dall'elenco raccolto una sola volta)");

	win->SwitchToSheet(1);
	CContainer* calc = win->GetSheetView()->Document();

	calc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 1, "SEQUENCE(3,1): A1 (cella owner) vale 1");
	calc->GetValue(cell(1, 2), v);
	Check(v.fType == eNumData && (double)v == 2,
		"SEQUENCE(3,1): A2 (cella spillata, un VALORE non una formula) vale 2 dopo "
		"RecalculateWorkbook -- non e' nell'elenco delle celle con formula, ma non ha "
		"comunque bisogno di esserci per essere corretta");
	calc->GetValue(cell(1, 3), v);
	Check(v.fType == eNumData && (double)v == 3, "SEQUENCE(3,1): A3 (cella spillata) vale 3");

	calc->GetValue(cell(2, 1), v);
	Check(v.fType == eNumData && (double)v == 70,
		"=Dati!B1*10 calcola 70 (7*10): il riferimento incrociato si e' risolto, l'elenco "
		"delle celle con formula del foglio Calc include davvero questa cella");

	win->Unlock();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
