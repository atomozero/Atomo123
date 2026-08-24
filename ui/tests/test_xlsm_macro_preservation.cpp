/*
	test_xlsm_macro_preservation.cpp

	Preservazione "alla cieca" del progetto VBA di un file XLSM (Fase
	31, roadmap Tier 1: "Data-loss risk beats missing features" -- vedi
	ROADMAP.md). Prima di questo lavoro un file .xlsm con macro apriva
	come un normale XLSX e xl/vbaProject.bin veniva silenziosamente
	scartato: riaprire e risalvare lo stesso file distruggeva le macro
	per sempre, anche se Atomo123 non le legge/esegue mai.

	Ciclo END-TO-END reale dal punto di vista dell'utente, stesso
	principio di test_chart_export_xlsx.cpp: costruisce a mano un file
	.xlsm sintetico (con CZipWriter, lo stesso scrittore ZIP minimale
	usato dal vero translator) con un blob VBA fittizio ma riconoscibile
	come sottostringa, lo apre con MainWindow::OpenFile (passa dal vero
	CXlsxTranslator installato via BTranslatorRoster, non un mock), poi
	lo risalva con un vero B_SAVE_REQUESTED e verifica che i byte del
	progetto VBA sopravvivano identici nel file scritto.

	Verifica anche il caso opposto (Fase 31, "isXlsm" in MainWindow::
	SaveToFile): salvare lo STESSO documento con estensione ".xlsx"
	esplicita deve continuare a spogliare le macro, esattamente come fa
	Excel stesso quando risalva un .xlsm come .xlsx.
*/

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <Message.h>

#include "MainWindow.h"
#include "MiniZip.h"

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

static const char kVbaMarker[] = "FAKE_VBA_PROJECT_MARKER_0123456789ABCDEF";

// Costruisce un .xlsm sintetico minimo ma valido (stesso schema letto
// da CXlsxTranslator::Identify/Translate: firma ZIP + [Content_Types].
// xml + un foglio + un progetto VBA riconoscibile).
static void WriteFixtureXlsm(const char* path)
{
	static const char kContentTypes[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
		"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
		"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
		"<Default Extension=\"bin\" ContentType=\"application/vnd.ms-office.vbaProject\"/>\n"
		"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.ms-excel.sheet.macroEnabled.main+xml\"/>\n"
		"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
		"</Types>\n";
	static const char kRootRels[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
		"</Relationships>\n";
	static const char kWorkbook[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
		"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
		"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
		"</workbook>\n";
	static const char kWorkbookRels[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
		"<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/vbaProject\" Target=\"vbaProject.bin\"/>\n"
		"</Relationships>\n";
	static const char kSheet[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
		"<sheetData><row r=\"1\"><c r=\"A1\" t=\"str\"><v>Ciao</v></c></row></sheetData>\n"
		"</worksheet>\n";

	BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	CZipWriter zip;
	zip.Begin(&file);
	zip.AddEntry("[Content_Types].xml", kContentTypes, strlen(kContentTypes));
	zip.AddEntry("_rels/.rels", kRootRels, strlen(kRootRels));
	zip.AddEntry("xl/workbook.xml", kWorkbook, strlen(kWorkbook));
	zip.AddEntry("xl/_rels/workbook.xml.rels", kWorkbookRels, strlen(kWorkbookRels));
	zip.AddEntry("xl/worksheets/sheet1.xml", kSheet, strlen(kSheet));
	zip.AddEntry("xl/vbaProject.bin", kVbaMarker, strlen(kVbaMarker));
	zip.Close();
}

// Stessa tecnica di test_chart_export_xlsx.cpp: il file e' un vero
// archivio ZIP binario, std::search sui byte grezzi invece di strstr
// (che si fermerebbe al primo NUL incontrato prima del testo cercato).
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

int main()
{
	BApplication app("application/x-vnd.Atomo-TestXlsmMacroPreservation");

	WriteFixtureXlsm("/tmp/test_xlsm_fixture.xlsm");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	BEntry entry("/tmp/test_xlsm_fixture.xlsm");
	entry_ref ref;
	entry.GetRef(&ref);
	win->OpenFile(ref);

	const std::vector<unsigned char>* vba = win->WorkbookVbaProject();
	Check(vba != NULL, "l'apertura di un .xlsm popola WorkbookVbaProject");
	if (vba != NULL)
	{
		std::string got(vba->begin(), vba->end());
		Check(got == kVbaMarker, "i byte del progetto VBA in memoria sono identici all'originale");
	}

	SaveViaRealMessage(win, "/tmp", "test_xlsm_roundtrip.xlsm");
	SaveViaRealMessage(win, "/tmp", "test_xlsm_roundtrip.xlsx");
	win->Unlock();

	Check(FileContains("/tmp/test_xlsm_roundtrip.xlsm", "xl/vbaProject.bin"),
		"risalvando come .xlsm il file contiene ancora xl/vbaProject.bin");
	Check(FileContains("/tmp/test_xlsm_roundtrip.xlsm", kVbaMarker),
		"risalvando come .xlsm i byte del progetto VBA sono identici all'originale");
	Check(FileContains("/tmp/test_xlsm_roundtrip.xlsm", "macroEnabled"),
		"risalvando come .xlsm il tipo MIME del foglio principale segnala le macro (Excel lo richiede per fidarsene)");
	Check(FileContains("/tmp/test_xlsm_roundtrip.xlsm", "relationships/vbaProject"),
		"risalvando come .xlsm la relazione verso vbaProject.bin e' presente");

	Check(!FileContains("/tmp/test_xlsm_roundtrip.xlsx", kVbaMarker),
		"risalvando esplicitamente come .xlsx le macro vengono spogliate, come fa Excel stesso");
	Check(!FileContains("/tmp/test_xlsm_roundtrip.xlsx", "xl/vbaProject.bin"),
		"il file .xlsx non contiene affatto xl/vbaProject.bin");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
