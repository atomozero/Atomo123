/*
	test_chart_export_xlsx.cpp

	Esportazione dei grafici incorporati verso XLSX (Fase 24, richiesta
	esplicita dell'utente: "i grafici di Atomo123 sono visibili da
	Excel?" -- prima di questo lavoro no, WriteXLSX scriveva solo i
	dati delle celle). Verifica il vero CICLO END-TO-END dal punto di
	vista dell'utente: MainWindow::HandleChartInsert (lo stesso comando
	del menu Inserisci > Grafico) seguito da un vero "Salva con nome"
	in .xlsx (lo stesso B_SAVE_REQUESTED che manderebbe un vero
	BFilePanel, vedi test_export_formats.cpp), non solo il translator
	in isolamento (gia' verificato a fondo, XML incluso, in
	translators/xlsx/tests/test_xlsx_translator.cpp).

	CZipWriter (translators/xlsx/MiniZip.cpp) scrive le voci "stored"
	(senza compressione): il testo XML dei grafici e' quindi cercabile
	come sottostringa diretta nei byte grezzi del file, senza bisogno
	di scompattare l'archivio qui.
*/

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <Message.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "Chart.h"

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

static void SaveViaRealMessage(MainWindow* win, const char* dirPath, const char* fileName)
{
	BEntry dirEntry(dirPath);
	entry_ref dirRef;
	dirEntry.GetRef(&dirRef);

	BMessage msg(B_SAVE_REQUESTED);
	msg.AddRef("directory", &dirRef);
	msg.AddString("name", fileName);
	win->MessageReceived(&msg);
}

// NON strstr(): il file e' un vero archivio ZIP binario (le intestazioni
// locali contengono campi a 2/4 byte, es. la versione, quasi sempre con
// il byte alto a 0x00) -- un byte NUL incontrato PRIMA del testo cercato
// interromperebbe subito la ricerca in stile stringa-C, anche se il
// testo compare piu' avanti nel file. std::search lavora sui byte grezzi,
// senza questo limite.
static bool FileContains(const char* path, const char* needle)
{
	BFile file(path, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return false;
	off_t size;
	file.GetSize(&size);
	std::vector<char> buf(size);
	if (file.Read(&buf[0], size) != size)
		return false;
	size_t needleLen = strlen(needle);
	return std::search(buf.begin(), buf.end(), needle, needle + needleLen) != buf.end();
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestChartExportXlsx");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer* doc = win->GetSheetView()->Document();
	TryToParseString("Gen", cell(1, 1), doc, true);
	TryToParseString("10", cell(2, 1), doc, true);
	TryToParseString("Feb", cell(1, 2), doc, true);
	TryToParseString("20", cell(2, 2), doc, true);
	TryToParseString("Mar", cell(1, 3), doc, true);
	TryToParseString("30", cell(2, 3), doc, true);

	win->HandleChartInsert("A1:B3", "E1", eBarChart);
	Check(win->Charts().size() == 1, "il grafico si aggiunge davvero al documento prima di salvare");

	SaveViaRealMessage(win, "/tmp", "test_chart_export.xlsx");
	win->Unlock();

	Check(FileContains("/tmp/test_chart_export.xlsx", "PK"),
		"il file .xlsx con un grafico e' un vero archivio ZIP");
	Check(FileContains("/tmp/test_chart_export.xlsx", "xl/charts/chart1.xml"),
		"il file .xlsx contiene davvero xl/charts/chart1.xml (non solo i dati delle celle)");
	Check(FileContains("/tmp/test_chart_export.xlsx", "xl/drawings/drawing1.xml"),
		"il file .xlsx contiene davvero xl/drawings/drawing1.xml (il grafico e' ancorato al foglio)");
	Check(FileContains("/tmp/test_chart_export.xlsx", "c:barChart"),
		"il grafico esportato e' davvero un grafico a barre (c:barChart), come inserito");
	Check(FileContains("/tmp/test_chart_export.xlsx", "Foglio1!$A$1:$A$3"),
		"il grafico esportato referenzia le celle vere del foglio (Foglio1!$A$1:$A$3)");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
