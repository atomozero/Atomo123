/*
	test_xlsx_perf_profile.cpp

	Profiling reale (non ipotetico) di Translate()/LoadASCDBook dopo
	le due ottimizzazioni gia' spedite in una sessione precedente
	(-O2 ovunque, CContainer::NewCell a una sola discesa d'albero) e
	dopo il lavoro di questa sessione sul grafo delle dipendenze (che
	ha gia' reso RecalculateWorkbook/RecalculateAll quasi irrilevanti
	nel tempo totale di apertura file). Domanda: resta un collo di
	bottiglia algoritmico genuino nella sola pipeline di parsing/
	serializzazione, o l'overhead residuo e' ormai proporzionale al
	numero di celle senza nessun punto caldo sproporzionato?

	Genera un foglio XLSX REALE (non un file sintetico costruito a
	mano byte per byte) passando dal vero percorso di esportazione di
	questa stessa app (SaveASCD -> BTranslatorRoster verso XLSX,
	esattamente come farebbe MainWindow::SaveToFile su ".xlsx"), poi
	lo riapre misurando Translate() e LoadASCDBook separatamente.

	Nota di scope: l'esportazione XLSX di questo translator resta a un
	solo foglio (vedi il commento in CXlsxTranslator::Translate,
	ramo ASCD->XLSX) -- questo banco di prova quindi usa UN foglio
	grande invece di tredici piccoli, come il file reale della
	sessione precedente. Per il tipo di collo di bottiglia cercato qui
	(overhead per cella nel parsing/nella scrittura ASCD interna) la
	differenza non conta: il costo e' dominato dal numero totale di
	celle attraversate, non da come sono distribuite fra i fogli --
	il percorso multi-foglio (Translate cross-sheet, WriteASCDBook per
	piu' fogli in sequenza) e' gia' coperto separatamente da
	test_translator_multisheet_import.cpp su un fixture piccolo.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <File.h>
#include <Path.h>
#include <Roster.h>
#include <TranslatorRoster.h>

#include "AscdIO.h"
#include "Cell.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "Container.h"
#include "FontMetrics.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "ResourceManager.h"
#include "Value.h"

static const uint32 kAtomoNativeFormat = 'ASCD';
static const uint32 kAtomoXlsxFormat = 'AXSX';

static int gFailures = 0;

static void Check(bool condition, const char* description)
{
	printf("%s   %s\n", condition ? "OK" : "FAIL", description);
	if (!condition)
		gFailures++;
}

// Righe/colonne di dati letterali (testo + numeri, con stile su una
// parte delle celle) piu' un blocco di formule locali che leggono
// quei dati -- la stessa forma "grande tabella dati quasi senza
// formule" descritta nel CHANGELOG per il file reale che ha innescato
// l'indagine originale (Fase 32/34).
static const int kDataRows = 6000;
static const int kDataCols = 8;
static const int kFormulaRows = 800;

static void BuildLargeSheet(CContainer* doc)
{
	static const char* kWords[] = {
		"Nord", "Sud", "Est", "Ovest", "Centro", "Alfa", "Beta", "Gamma",
		"Delta", "Epsilon", "Zeta", "Eta"
	};
	const int kWordCount = sizeof(kWords) / sizeof(kWords[0]);

	// Riga di intestazione, in grassetto (esercita la risoluzione di
	// stile, non solo il valore grezzo).
	CellStyle headerStyle;
	headerStyle.fFont = (int)gFontSizeTable.GetFontID("Arial", "Bold", 11.0f);
	for (int col = 1; col <= kDataCols; col++)
	{
		char label[16];
		snprintf(label, sizeof(label), "Col%d", col);
		TryToParseString(label, cell(col, 1), doc, true);
		doc->SetCellStyle(cell(col, 1), headerStyle);
	}

	// Dati: alternanza testo/numero per colonna, una banda di colore
	// ogni due righe su META' delle colonne (realismo: non ogni cella
	// del file reale ha uno stile esplicito).
	CellStyle bandStyle;
	bandStyle.fLowColor.red = 235;
	bandStyle.fLowColor.green = 235;
	bandStyle.fLowColor.blue = 245;
	bandStyle.fLowColor.alpha = 255;

	for (int row = 2; row <= kDataRows + 1; row++)
	{
		for (int col = 1; col <= kDataCols; col++)
		{
			char buf[32];
			if (col % 2 == 1)
				// Niente trattino: "Ovest-2" verrebbe analizzato come
				// una formula di sottrazione ("Ovest" - 2), non come
				// testo letterale -- lo stesso bug di testo ambiguo gia'
				// tracciato a parte in questo progetto (vedi la memoria
				// "ASCD ambiguous-text corruption"), non l'oggetto di
				// questo banco di prova sulle prestazioni.
				snprintf(buf, sizeof(buf), "%s%d", kWords[(row + col) % kWordCount], row);
			else
				snprintf(buf, sizeof(buf), "%.2f", (double)((row * 37 + col * 13) % 10000) / 3.0);
			TryToParseString(buf, cell(col, row), doc, true);
			if (col <= kDataCols / 2 && row % 2 == 0)
				doc->SetCellStyle(cell(col, row), bandStyle);
		}
	}

	// Blocco formule (colonna kDataCols+2), ognuna un SUM locale su
	// una piccola finestra di righe della colonna numerica B.
	int formulaCol = kDataCols + 2;
	for (int i = 0; i < kFormulaRows; i++)
	{
		int row = 2 + i;
		char formula[64];
		int windowStart = row;
		int windowEnd = row + 4;
		if (windowEnd > kDataRows + 1)
			windowEnd = kDataRows + 1;
		snprintf(formula, sizeof(formula), "=SUM(B%d:B%d)", windowStart, windowEnd);
		TryToParseString(formula, cell(formulaCol, row), doc, true);
	}
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestXlsxPerfProfile");

	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	CContainer* doc = new CContainer(NULL, NULL);
	BuildLargeSheet(doc);

	int32 totalCells = 0;
	{
		CCellIterator iter(doc, NULL);
		cell c;
		while (iter.NextExisting(c))
			totalCells++;
	}
	printf("Documento sintetico: %d celle totali (%d dati, %d intestazione, %d formule)\n",
		(int)totalCells, kDataRows * kDataCols, kDataCols, kFormulaRows);

	// 1) Documento -> ASCD nativo in memoria (percorso reale di
	// MainWindow::SaveToFile per un'estensione non nativa).
	BMallocIO ascdOut;
	status_t saveErr = SaveASCD(doc, &ascdOut);
	Check(saveErr == B_OK, "SaveASCD del documento sintetico riesce");

	// 2) ASCD -> XLSX reale su disco, passando per il vero
	// BTranslatorRoster (stesso codice che gira per un utente reale).
	BPath tmpPath("/boot/system/cache/tmp/claude-0/-Magazzino-Atomo123/"
		"cd81906d-e1b1-405f-87fb-acbceb50d709/scratchpad/perf_profile_large.xlsx");
	BFile xlsxFile(tmpPath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	Check(xlsxFile.InitCheck() == B_OK, "creazione del file .xlsx temporaneo riesce");

	// Stesso schema esatto di MainWindow::SaveToFile: un sorgente ASCD
	// generico viene riconosciuto allo stesso modo da OGNI translator
	// installato (concordano tutti su kAtomoNativeFormat), quindi
	// Identify() da solo non basta a scegliere quello che sa scrivere
	// XLSX -- va scelto a mano cercando chi dichiara kAtomoXlsxFormat
	// nei propri formati di uscita.
	translator_id chosenId = 0;
	translator_id* allIds = NULL;
	int32 idCount = 0;
	if (BTranslatorRoster::Default()->GetAllTranslators(&allIds, &idCount) == B_OK)
	{
		for (int32 i = 0; i < idCount && chosenId == 0; i++)
		{
			const translation_format* formats = NULL;
			int32 numFormats = 0;
			if (BTranslatorRoster::Default()->GetOutputFormats(allIds[i], &formats, &numFormats) != B_OK)
				continue;
			for (int32 j = 0; j < numFormats; j++)
			{
				if (formats[j].type == kAtomoXlsxFormat)
				{
					chosenId = allIds[i];
					break;
				}
			}
		}
	}
	Check(chosenId != 0, "un translator installato dichiara di saper scrivere XLSX");

	ascdOut.Seek(0, SEEK_SET);
	status_t exportErr = (chosenId != 0)
		? BTranslatorRoster::Default()->Translate(chosenId, &ascdOut, NULL, &xlsxFile, kAtomoXlsxFormat)
		: B_NO_TRANSLATOR;
	Check(exportErr == B_OK, "esportazione ASCD -> XLSX reale riesce");
	xlsxFile.Unset();

	off_t xlsxSize = 0;
	{
		BFile check(tmpPath.Path(), B_READ_ONLY);
		check.GetSize(&xlsxSize);
	}
	printf("File XLSX generato: %.2f MB\n", (double)xlsxSize / (1024.0 * 1024.0));

	// 3) XLSX -> ASCD (Translate reale, stesso percorso di
	// MainWindow::OpenFile), cronometrato.
	BFile reopenFile(tmpPath.Path(), B_READ_ONLY);
	Check(reopenFile.InitCheck() == B_OK, "riapertura del file .xlsx temporaneo riesce");

	BMallocIO ascdBack;
	bigtime_t translateStart = system_time();
	status_t translateErr = BTranslatorRoster::Default()->Translate(&reopenFile, NULL, NULL,
		&ascdBack, kAtomoNativeFormat);
	bigtime_t translateElapsed = system_time() - translateStart;
	Check(translateErr == B_OK, "Translate() XLSX -> ASCD riesce");

	// 4) ASCD -> documento reale (LoadASCDBook), cronometrato
	// separatamente da Translate().
	ascdBack.Seek(0, SEEK_SET);
	bool isBook = IsASCDBookFile(&ascdBack);
	Check(isBook, "l'output tradotto e' una cartella di lavoro (ASCB)");

	std::vector<AscdSheet> sheets;
	bigtime_t loadStart = system_time();
	status_t loadErr = isBook ? LoadASCDBook(&ascdBack, &sheets) : B_BAD_DATA;
	bigtime_t loadElapsed = system_time() - loadStart;
	Check(loadErr == B_OK, "LoadASCDBook riesce");
	Check(sheets.size() == 1, "un solo foglio, come atteso dall'esportazione XLSX di questo translator");

	if (sheets.size() == 1)
	{
		Value v;
		sheets[0].doc->GetValue(cell(1, 2), v);
		Check(v.fType != eNoData, "il primo dato importato non e' vuoto (round-trip riuscito)");
	}

	printf("\nPROFILO PRESTAZIONI (foglio singolo, %d celle):\n"
		"  Translate() (XLSX -> ASCD):    %8lld us  (%.3f us/cella)\n"
		"  LoadASCDBook (ASCD -> doc):    %8lld us  (%.3f us/cella)\n"
		"  Totale pipeline import:        %8lld us\n\n",
		(int)totalCells,
		(long long)translateElapsed, (double)translateElapsed / totalCells,
		(long long)loadElapsed, (double)loadElapsed / totalCells,
		(long long)(translateElapsed + loadElapsed));

	for (size_t i = 0; i < sheets.size(); i++)
		sheets[i].doc->Release();
	doc->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
