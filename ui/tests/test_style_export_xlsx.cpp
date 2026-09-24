/*
	test_style_export_xlsx.cpp

	Real per-cell style export to XLSX (ROADMAP.md "Path to full Excel
	parity" Tier 2, "border color export"): before this work, WriteXLSX
	wrote a two-entry placeholder xl/styles.xml (locked/unlocked only) --
	every cell exported as plain, uncolored, unformatted text, no matter
	what CellStyle actually held. Closing the border-color-export gap
	for real required building the same <fonts>/<fills>/<borders>/
	<cellXfs> machinery that also fixes bold/italic/fill color/font
	color on export, so this test covers all four together, not just
	the border. Verifies the real end-to-end cycle from the user's
	point of view (MainWindow -> real "Salva con nome" -> real ZIP
	bytes), same pattern as test_chart_export_xlsx.cpp.

	CZipWriter (translators/xlsx/MiniZip.cpp) writes "stored" (no
	compression) entries: the styles.xml text is therefore directly
	searchable as a raw byte substring, no need to unzip here.
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
#include "CellStyle.h"
#include "Container.h"
#include "CellParser.h"
#include "FontMetrics.h"
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

// Same reasoning as test_chart_export_xlsx.cpp: raw byte search, not
// strstr, since the file is a real binary ZIP archive that can contain
// NUL bytes before the text we're looking for.
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
	BApplication app("application/x-vnd.Atomo-TestStyleExportXlsx");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer* doc = win->GetSheetView()->Document();
	TryToParseString("Bold", cell(1, 1), doc, true); // A1: bold font
	TryToParseString("Fill", cell(1, 2), doc, true); // A2: colored background
	TryToParseString("Border", cell(1, 3), doc, true); // A3: colored top border

	CellStyle boldStyle;
	doc->GetCellStyle(cell(1, 1), boldStyle);
	boldStyle.fFont = (int)gFontSizeTable.GetFontID("Arial", "Bold", 11.0f);
	doc->SetCellStyle(cell(1, 1), boldStyle);

	CellStyle fillStyle;
	doc->GetCellStyle(cell(1, 2), fillStyle);
	fillStyle.fLowColor.red = 255;
	fillStyle.fLowColor.green = 200;
	fillStyle.fLowColor.blue = 0;
	fillStyle.fLowColor.alpha = 255;
	doc->SetCellStyle(cell(1, 2), fillStyle);

	CellStyle borderStyle;
	doc->GetCellStyle(cell(1, 3), borderStyle);
	borderStyle.fTBorderColor = 1; // thin top border
	borderStyle.fBorderColor.red = 200;
	borderStyle.fBorderColor.green = 0;
	borderStyle.fBorderColor.blue = 0;
	borderStyle.fBorderColor.alpha = 255;
	doc->SetCellStyle(cell(1, 3), borderStyle);

	SaveViaRealMessage(win, "/tmp", "test_style_export.xlsx");
	win->Unlock();

	Check(FileContains("/tmp/test_style_export.xlsx", "PK"),
		"il file .xlsx con celle formattate e' un vero archivio ZIP");
	Check(FileContains("/tmp/test_style_export.xlsx", "xl/styles.xml"),
		"il file .xlsx contiene xl/styles.xml");
	Check(FileContains("/tmp/test_style_export.xlsx", "<b/>"),
		"lo stile grassetto (A1) esporta un vero <b/> in <fonts>, non piu' scartato");
	Check(FileContains("/tmp/test_style_export.xlsx", "FFFFC800"),
		"il colore di sfondo (A2, 255/200/0) esporta il vero colore in <fills>, non piu' scartato");
	Check(FileContains("/tmp/test_style_export.xlsx", "FFC80000"),
		"il colore del bordo (A3, 200/0/0) esporta il vero colore in <borders>, non piu' scartato");
	Check(FileContains("/tmp/test_style_export.xlsx", "style=\"thin\""),
		"il bordo superiore di A3 esporta uno spessore reale (thin), non solo un colore senza bordo");

	// Reimport round-trip: apre lo stesso file appena esportato e
	// verifica che i tre stili tornino indietro come CellStyle veri,
	// non solo che l'XML grezzo li contenga.
	entry_ref reopenRef;
	BEntry reopenEntry("/tmp/test_style_export.xlsx");
	reopenEntry.GetRef(&reopenRef);

	MainWindow* win2 = new MainWindow();
	win2->Show();
	win2->Lock();
	win2->OpenFile(reopenRef);

	CContainer* doc2 = win2->GetSheetView()->Document();
	CellStyle reboldStyle;
	doc2->GetCellStyle(cell(1, 1), reboldStyle);
	font_family refamily; font_style restyle; float resize; rgb_color recolor;
	gFontSizeTable.GetFontInfo(reboldStyle.fFont, &refamily, &restyle, &resize, &recolor);
	Check(strstr(restyle, "Bold") != NULL,
		"A1 riaperto ha ancora un font in grassetto (round-trip export -> import)");

	CellStyle refillStyle;
	doc2->GetCellStyle(cell(1, 2), refillStyle);
	Check(refillStyle.fLowColor.red == 255 && refillStyle.fLowColor.green == 200
			&& refillStyle.fLowColor.blue == 0,
		"A2 riaperto ha ancora il vero colore di sfondo (round-trip export -> import)");

	CellStyle reborderStyle;
	doc2->GetCellStyle(cell(1, 3), reborderStyle);
	Check(reborderStyle.fTBorderColor > 0,
		"A3 riaperto ha ancora un bordo superiore (round-trip export -> import)");
	Check(reborderStyle.fBorderColor.red == 200 && reborderStyle.fBorderColor.green == 0
			&& reborderStyle.fBorderColor.blue == 0,
		"A3 riaperto ha ancora il vero colore del bordo (round-trip export -> import)");
	win2->Unlock();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
