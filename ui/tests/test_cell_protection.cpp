/*
	test_cell_protection.cpp

	Protezione foglio/blocco celle (Fase 32, roadmap Tier 1). Verifica,
	con un vero MainWindow (non un mock):
	1) il default reale: ogni cella nuova e' bloccata (CellStyle::
	   fLocked=true, vedi CellStyle.cpp) -- il foglio non e' protetto
	   finche' non lo si dice esplicitamente, esattamente come Excel;
	2) i comandi "Blocca/Sblocca celle selezionate" e "Proteggi foglio"
	   (menu Dati, vedi MainWindow::MessageReceived);
	3) la PRECONDIZIONE della guardia di modifica (SheetView::
	   RangeHasLockedCell + IsProtected) e il suo effetto reale sul
	   ramo CONSENTITO (SheetView::GuardProtectedEdit su una cella
	   sbloccata, e un vero comando pubblico -- MainWindow::ToggleBold
	   -- su quella stessa cella). Il ramo NEGATO non viene invocato
	   qui apposta: BAlert::Go() e' bloccante in questo ambiente
	   headless (verificato con un piccolo programma di prova a parte:
	   un processo che chiama BAlert::Go() senza un vero utente resta
	   appeso finche' non lo si termina a mano), quindi un test che
	   arrivasse davvero a mostrarlo bloccherebbe l'intera suite
	   invece di fallire in modo pulito;
	4) il round-trip nativo ASCD/ASCB (SaveASCDBook/LoadASCDBook)
	   preserva sia il blocco per cella sia la protezione per foglio;
	5) il round-trip XLSX vero (attraverso il translator installato via
	   BTranslatorRoster, stesso principio di test_xlsm_macro_
	   preservation.cpp): un file con <protection locked="0"/> e
	   <sheetProtection/> apre con lo stato giusto, e risalvandolo come
	   .xlsx quello stato sopravvive byte per byte riconoscibile.
*/

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <DataIO.h>
#include <Message.h>

#include "Cell.h"
#include "CellStyle.h"
#include "Container.h"
#include "CellParser.h"
#include "AscdIO.h"
#include "SheetView.h"
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

// Manda un vero BMessage a MainWindow, stesso principio di ogni altro
// test che esercita un comando di menu senza un vero click (vedi
// test_chart_export_xlsx.cpp). 'ulks'/'lcks'/'prsh' sono gli stessi
// codici a 4 lettere di kMsgUnlockSelection/kMsgLockSelection/
// kMsgToggleProtectSheet in MainWindow.cpp.
static void Send(MainWindow* win, uint32 what)
{
	BMessage msg(what);
	win->MessageReceived(&msg);
}

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

// Costruisce un .xlsx sintetico minimo con una cella bloccata (A1,
// nessun s="...") e una sbloccata (B1, s="1" -> xf con <protection
// locked="0"/>), piu' <sheetProtection/> sul foglio -- stesso schema
// minimo di xl/styles.xml scritto da WriteXLSX (vedi XlsxTranslator.cpp).
static void WriteProtectedFixtureXlsx(const char* path)
{
	static const char kContentTypes[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
		"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
		"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
		"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
		"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
		"<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n"
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
		"<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n"
		"</Relationships>\n";
	static const char kStyles[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
		"<fonts count=\"1\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>\n"
		"<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
		"<fill><patternFill patternType=\"gray125\"/></fill></fills>\n"
		"<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>\n"
		"<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>\n"
		"<cellXfs count=\"2\">"
		"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
		"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"><protection locked=\"0\"/></xf>"
		"</cellXfs>\n"
		"</styleSheet>\n";
	static const char kSheet[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
		"<sheetData>"
		"<row r=\"1\"><c r=\"A1\" t=\"str\"><v>Bloccata</v></c>"
		"<c r=\"B1\" s=\"1\" t=\"str\"><v>Sbloccata</v></c></row>"
		"</sheetData>"
		"<sheetProtection sheetId=\"1\"/>"
		"</worksheet>\n";

	BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	CZipWriter zip;
	zip.Begin(&file);
	zip.AddEntry("[Content_Types].xml", kContentTypes, strlen(kContentTypes));
	zip.AddEntry("_rels/.rels", kRootRels, strlen(kRootRels));
	zip.AddEntry("xl/workbook.xml", kWorkbook, strlen(kWorkbook));
	zip.AddEntry("xl/_rels/workbook.xml.rels", kWorkbookRels, strlen(kWorkbookRels));
	zip.AddEntry("xl/styles.xml", kStyles, strlen(kStyles));
	zip.AddEntry("xl/worksheets/sheet1.xml", kSheet, strlen(kSheet));
	zip.Close();
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestCellProtection");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer* doc = win->GetSheetView()->Document();

	// 1) Default reale: una cella appena scritta e' bloccata.
	TryToParseString("42", cell(1, 1), doc, true);
	CellStyle cs;
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fLocked, "una cella nuova e' bloccata di default (CellStyle::fLocked=true)");
	Check(!win->GetSheetView()->IsProtected(), "un documento nuovo non e' protetto di default");

	// 2) Comandi Blocca/Sblocca/Proteggi.
	win->GetSheetView()->SetSelection(cell(1, 1));
	Send(win, 'ulks'); // kMsgUnlockSelection
	doc->GetCellStyle(cell(1, 1), cs);
	Check(!cs.fLocked, "\"Sblocca celle selezionate\" toglie il blocco dalla cella attiva (A1)");

	TryToParseString("7", cell(2, 1), doc, true); // B1, resta bloccata
	Send(win, 'prsh'); // kMsgToggleProtectSheet
	Check(win->GetSheetView()->IsProtected(), "\"Proteggi foglio\" attiva la protezione");

	// 3) Precondizione della guardia (vedi il commento in cima al file
	// sul perche' il ramo NEGATO non viene mai invocato qui davvero).
	Check(win->GetSheetView()->RangeHasLockedCell(range(2, 1, 2, 1)),
		"B1 (mai sbloccata) risulta bloccata per RangeHasLockedCell");
	Check(win->GetSheetView()->IsProtected()
			&& win->GetSheetView()->RangeHasLockedCell(range(2, 1, 2, 1)),
		"la precondizione di blocco (protetto + B1 bloccata) e' vera: GuardProtectedEdit(B1) negherebbe");
	Check(!win->GetSheetView()->RangeHasLockedCell(range(1, 1, 1, 1)),
		"A1 (sbloccata) NON risulta bloccata per RangeHasLockedCell");
	Check(win->GetSheetView()->GuardProtectedEdit(range(1, 1, 1, 1)),
		"GuardProtectedEdit su A1 (sbloccata) restituisce vero, nessun avviso");

	// Ramo CONSENTITO esercitato con un vero comando pubblico (non solo
	// la guardia isolata): grassetto su A1 (sbloccata) mentre il foglio
	// e' protetto deve funzionare comunque.
	win->GetSheetView()->SetSelection(cell(1, 1));
	win->ToggleBold();
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fFont != CellStyle().fFont,
		"a foglio protetto, il grassetto su una cella SBLOCCATA (A1) funziona comunque");

	// Disattiva la protezione: da qui in poi si puo' costruire/salvare
	// liberamente per il test del round-trip.
	Send(win, 'prsh');
	Check(!win->GetSheetView()->IsProtected(), "\"Proteggi foglio\" si disattiva di nuovo (interruttore)");

	win->Unlock();

	// 4) Round-trip nativo ASCD/ASCB.
	{
		win->Lock();
		Send(win, 'prsh'); // riprotegge, cosi' il round-trip verifica anche isProtected
		win->Unlock();

		std::vector<AscdSheet> sheets(1);
		sheets[0].name = "Foglio1";
		sheets[0].doc = doc;
		sheets[0].isProtected = true;

		BMallocIO io;
		Check(SaveASCDBook(sheets, &io) == B_OK, "SaveASCDBook riesce");

		io.Seek(0, SEEK_SET);
		std::vector<AscdSheet> loaded;
		Check(LoadASCDBook(&io, &loaded) == B_OK, "LoadASCDBook riesce");
		Check(loaded.size() == 1, "un solo foglio dopo il giro");
		Check(loaded[0].isProtected, "isProtected sopravvive al giro nativo");

		CellStyle a1, b1;
		loaded[0].doc->GetCellStyle(cell(1, 1), a1); // A1, sbloccata
		loaded[0].doc->GetCellStyle(cell(2, 1), b1); // B1, bloccata
		Check(!a1.fLocked, "A1 (sbloccata) resta sbloccata dopo il giro nativo");
		Check(b1.fLocked, "B1 (mai sbloccata) resta bloccata dopo il giro nativo (il default)");

		loaded[0].doc->Release();

		win->Lock();
		Send(win, 'prsh'); // disattiva di nuovo, per non interferire col resto
		win->Unlock();
	}

	// 5) Round-trip XLSX vero, attraverso il translator installato. Una
	// FINESTRA NUOVA apposta: "win" sopra ha gia' modifiche non salvate
	// (ToggleBold/i comandi Blocca ecc.), e MainWindow::OpenFile
	// comincia con ConfirmDiscardChanges(), che mostrerebbe un vero
	// BAlert bloccante ("Le modifiche non salvate andranno perse") --
	// stesso motivo del ramo NEGATO della guardia sopra, mai invocato
	// davvero in un test headless.
	WriteProtectedFixtureXlsx("/tmp/test_protection_fixture.xlsx");

	MainWindow* win2 = new MainWindow();
	win2->Show();
	win2->Lock();

	BEntry entry("/tmp/test_protection_fixture.xlsx");
	entry_ref ref;
	entry.GetRef(&ref);
	win2->OpenFile(ref);

	CContainer* xlsxDoc = win2->GetSheetView()->Document();
	CellStyle xa1, xb1;
	xlsxDoc->GetCellStyle(cell(1, 1), xa1); // A1, nessun s= -> bloccata
	xlsxDoc->GetCellStyle(cell(2, 1), xb1); // B1, s="1" -> sbloccata
	Check(xa1.fLocked, "importando XLSX, A1 (nessun s=\"1\") arriva bloccata");
	Check(!xb1.fLocked, "importando XLSX, B1 (s=\"1\", <protection locked=\"0\"/>) arriva sbloccata");
	Check(win2->GetSheetView()->IsProtected(),
		"importando XLSX, <sheetProtection/> arriva come foglio protetto");

	SaveViaRealMessage(win2, "/tmp", "test_protection_roundtrip.xlsx");
	win2->Unlock();

	Check(FileContains("/tmp/test_protection_roundtrip.xlsx", "sheetProtection"),
		"risalvando come .xlsx, <sheetProtection/> e' ancora presente");
	// NON basta cercare "protection locked=\"0\"" da solo: xl/styles.xml
	// scritto da WriteXLSX contiene SEMPRE quella stringa nella seconda
	// voce <xf> (Fase 32, sempre presente per poterla referenziare),
	// anche se nessuna cella la usa -- un falso positivo che non
	// dimostrerebbe nulla. "B1 s=\"1\"" invece compare SOLO se WriteXLSX
	// ha davvero riconosciuto B1 come sbloccata e le ha assegnato quella
	// voce di stile.
	Check(FileContains("/tmp/test_protection_roundtrip.xlsx", "r=\"B1\" s=\"1\""),
		"risalvando come .xlsx, B1 referenzia davvero la voce di stile sbloccata (s=\"1\")");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
