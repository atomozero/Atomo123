/*
	test_export_formats.cpp

	Verifica che MainWindow::SaveToFile riconosca davvero le estensioni
	".xlsx"/".ods" e passi dal vero XlsxTranslator/OdsTranslator (bug
	segnalato dall'utente: "perche' i translator non salvano piu' le
	formule? vengono usati solo in apertura?" -- la causa reale era che
	SaveToFile scriveva SEMPRE byte ASCD sotto qualunque estensione
	diversa da ".csv", XLSX/ODS non erano mai davvero collegati al
	salvataggio nonostante il loro Translate() sapesse gia' scrivere).

	Aziona il ciclo reale: costruisce un documento con una formula,
	invia lo stesso B_SAVE_REQUESTED che manderebbe un vero BFilePanel
	(MainWindow::MessageReceived lo gestisce chiamando SaveToFile,
	privata), poi riapre il file scritto con una SECONDA MainWindow
	tramite OpenFile (il vero giro BTranslatorRoster::Identify/
	Translate, non una scorciatoia) per verificare che sia un file
	XLSX/ODS autentico (non byte ASCD sotto un nome sbagliato) e che la
	formula sia sopravvissuta viva, non solo il suo valore calcolato.

	Richiede i translator XLSX/ODS gia' installati (make install nelle
	rispettive cartelle), altrimenti BTranslatorRoster non li trova --
	stessa premessa di test_xlsx_named_functions_dynamic.cpp.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <Message.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"

static const uint32 kMsgSaveRequestedForTest = B_SAVE_REQUESTED;

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

// Manda lo stesso identico messaggio che un vero BFilePanel manderebbe
// dopo "Salva" (vedi MainWindow::MessageReceived, case
// B_SAVE_REQUESTED): "directory" (entry_ref della cartella) e "name"
// (il nome file scelto), non un percorso completo in un solo campo.
static void SaveViaRealMessage(MainWindow* win, const char* dirPath, const char* fileName)
{
	BEntry dirEntry(dirPath);
	entry_ref dirRef;
	dirEntry.GetRef(&dirRef);

	BMessage msg(kMsgSaveRequestedForTest);
	msg.AddRef("directory", &dirRef);
	msg.AddString("name", fileName);
	win->MessageReceived(&msg);
}

static bool FileStartsWith(const char* path, const char* magic, size_t magicLen)
{
	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;
	char buf[16];
	if (magicLen > sizeof(buf))
		return false;
	if (file.Read(buf, magicLen) != (ssize_t)magicLen)
		return false;
	return memcmp(buf, magic, magicLen) == 0;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestExportFormats");

	// --- XLSX ---------------------------------------------------------
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();

		SheetView* view = win->GetSheetView();
		CContainer* doc = view->Document();
		TryToParseString("12", cell(1, 1), doc, true);        // A1
		TryToParseString("8", cell(2, 1), doc, true);         // B1
		TryToParseString("=A1+B1", cell(3, 1), doc, true);    // C1
		TryToParseString("Ciao", cell(1, 2), doc, true);      // A2
		doc->CalcCell(cell(3, 1));

		SaveViaRealMessage(win, "/tmp", "test_export_formats.xlsx");
		win->Unlock();

		Check(FileStartsWith("/tmp/test_export_formats.xlsx", "PK", 2),
			"il file scritto per \".xlsx\" e' un vero archivio ZIP (\"PK\"), "
			"non byte ASCD sotto un nome sbagliato");

		MainWindow* reopened = new MainWindow();
		reopened->Show();
		reopened->Lock();

		BEntry entry("/tmp/test_export_formats.xlsx");
		entry_ref ref;
		entry.GetRef(&ref);
		reopened->OpenFile(ref);

		CContainer* reDoc = reopened->GetSheetView()->Document();
		Value v;
		reDoc->GetValue(cell(1, 1), v);
		Check(v.fType == eNumData && (double)v == 12.0, "XLSX riaperto: A1 vale ancora 12");
		reDoc->GetValue(cell(2, 1), v);
		Check(v.fType == eNumData && (double)v == 8.0, "XLSX riaperto: B1 vale ancora 8");
		reDoc->GetValue(cell(3, 1), v);
		Check(v.fType == eNumData && (double)v == 20.0, "XLSX riaperto: C1 vale 20 (12+8)");
		Check(reDoc->GetCellFormula(cell(3, 1)) != NULL,
			"XLSX riaperto: C1 e' una formula viva, non solo il valore calcolato");

		// Prova piu' rigorosa di "e' viva": cambiare A1 deve far
		// ricalcolare C1 da solo, non lasciarlo fermo al vecchio 20.
		TryToParseString("100", cell(1, 1), reDoc, true);
		reDoc->CalcCell(cell(3, 1));
		reDoc->GetValue(cell(3, 1), v);
		Check(v.fType == eNumData && (double)v == 108.0,
			"XLSX riaperto: modificando A1 (100), C1 ricalcola da solo a 108 (100+8), "
			"prova che la formula e' viva e non un valore congelato");

		reDoc->GetValue(cell(1, 2), v);
		Check(v.fType == eTextData && strcmp((const char*)v, "Ciao") == 0,
			"XLSX riaperto: A2 vale ancora \"Ciao\"");

		reopened->Unlock();
	}

	// --- ODS ------------------------------------------------------------
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();

		SheetView* view = win->GetSheetView();
		CContainer* doc = view->Document();
		TryToParseString("12", cell(1, 1), doc, true);        // A1
		TryToParseString("8", cell(2, 1), doc, true);         // B1
		TryToParseString("=A1+B1", cell(3, 1), doc, true);    // C1
		TryToParseString("Ciao", cell(1, 2), doc, true);      // A2
		doc->CalcCell(cell(3, 1));

		SaveViaRealMessage(win, "/tmp", "test_export_formats.ods");
		win->Unlock();

		Check(FileStartsWith("/tmp/test_export_formats.ods", "PK", 2),
			"il file scritto per \".ods\" e' un vero archivio ZIP (\"PK\"), "
			"non byte ASCD sotto un nome sbagliato");

		MainWindow* reopened = new MainWindow();
		reopened->Show();
		reopened->Lock();

		BEntry entry("/tmp/test_export_formats.ods");
		entry_ref ref;
		entry.GetRef(&ref);
		reopened->OpenFile(ref);

		CContainer* reDoc = reopened->GetSheetView()->Document();
		Value v;
		reDoc->GetValue(cell(1, 1), v);
		Check(v.fType == eNumData && (double)v == 12.0, "ODS riaperto: A1 vale ancora 12");
		reDoc->GetValue(cell(2, 1), v);
		Check(v.fType == eNumData && (double)v == 8.0, "ODS riaperto: B1 vale ancora 8");
		reDoc->GetValue(cell(3, 1), v);
		Check(v.fType == eNumData && (double)v == 20.0, "ODS riaperto: C1 vale 20 (12+8)");
		Check(reDoc->GetCellFormula(cell(3, 1)) != NULL,
			"ODS riaperto: C1 e' una formula viva, non solo il valore calcolato");

		TryToParseString("100", cell(1, 1), reDoc, true);
		reDoc->CalcCell(cell(3, 1));
		reDoc->GetValue(cell(3, 1), v);
		Check(v.fType == eNumData && (double)v == 108.0,
			"ODS riaperto: modificando A1 (100), C1 ricalcola da solo a 108 (100+8), "
			"prova che la formula e' viva e non un valore congelato");

		reDoc->GetValue(cell(1, 2), v);
		Check(v.fType == eTextData && strcmp((const char*)v, "Ciao") == 0,
			"ODS riaperto: A2 vale ancora \"Ciao\"");

		reopened->Unlock();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
