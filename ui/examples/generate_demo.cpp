/*
	generate_demo.cpp

	Genera atomo123_demo.ascd: una cartella di lavoro a due fogli che
	mostra un sottoinsieme delle funzionalita' del formato nativo ASCD
	(formule, formule fra fogli, formattazione valuta, colori, bordi,
	allineamento, sottolineato, testo a capo, celle unite, larghezza
	colonna/altezza riga, Blocca riquadri, AutoFilter, colore
	linguetta, commento, collegamento ipertestuale, grafico a barre
	incorporato). Stesso approccio headless di
	ui/tests/test_ascd_book.cpp/test_persistence.cpp: nessuna vera
	MainWindow, solo CContainer + SaveASCDBook (nessuna sessione
	grafica necessaria per generare il file).
*/

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <Application.h>
#include <File.h>
#include <NodeInfo.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "Chart.h"
#include "Formatter.h"
#include "Range.h"

template<typename F>
static void Style(CContainer* doc, cell c, F f)
{
	CellStyle cs;
	doc->GetCellStyle(c, cs);
	f(cs);
	doc->SetCellStyle(c, cs);
}

static void Border(CContainer* doc, cell c)
{
	Style(doc, c, [](CellStyle& cs) {
		cs.fTBorderColor = cs.fLBorderColor = cs.fBBorderColor = cs.fRBorderColor = 1;
	});
}

static void Currency(CContainer* doc, cell c)
{
	Style(doc, c, [](CellStyle& cs) {
		cs.fFormat = eCurrency | (2 << 4) | (1 << 9); // 2 cifre decimali, separatore delle migliaia
		cs.fAlignment = eAlignRight;
	});
}

int main()
{
	BApplication app("application/x-vnd.Atomo-GenerateDemo");

	const rgb_color kBlue   = { 0, 120, 215, 255 };
	const rgb_color kWhite  = { 255, 255, 255, 255 };
	const rgb_color kGray   = { 217, 217, 217, 255 };
	const rgb_color kYellow = { 255, 242, 204, 255 };
	const rgb_color kOrange = { 230, 126, 34, 255 };

	// ---- Foglio "Vendite" ----
	CContainer* sales = new CContainer(NULL, NULL);

	TryToParseString("Report Vendite Atomo123 - Esempio", cell(1, 1), sales, true); // A1
	sales->AddMergedRange(range(1, 1, 6, 1)); // A1:F1
	Style(sales, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kBlue; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});

	const char* headers[] = { "Prodotto", "Q1", "Q2", "Q3", "Q4", "Totale" };
	for (int col = 1; col <= 6; col++)
	{
		TryToParseString(headers[col - 1], cell(col, 2), sales, true);
		Style(sales, cell(col, 2), [&](CellStyle& cs) {
			cs.fLowColor = kGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
		});
		Border(sales, cell(col, 2));
	}

	struct Product { const char* name; int q1, q2, q3, q4; };
	Product products[] = {
		{ "Notebook Pro", 1200, 1350, 1500, 1600 },
		{ "Tastiera Meccanica", 300, 320, 280, 350 },
		{ "Monitor 27\"", 800, 850, 900, 780 },
		{ "Mouse Wireless", 150, 180, 170, 200 },
		{ "Webcam HD", 220, 240, 260, 280 },
	};

	for (int i = 0; i < 5; i++)
	{
		int row = i + 3; // righe 3..7
		const Product& p = products[i];
		TryToParseString(p.name, cell(1, row), sales, true);
		Border(sales, cell(1, row));

		char buf[32];
		snprintf(buf, sizeof(buf), "%d", p.q1); TryToParseString(buf, cell(2, row), sales, true);
		snprintf(buf, sizeof(buf), "%d", p.q2); TryToParseString(buf, cell(3, row), sales, true);
		snprintf(buf, sizeof(buf), "%d", p.q3); TryToParseString(buf, cell(4, row), sales, true);
		snprintf(buf, sizeof(buf), "%d", p.q4); TryToParseString(buf, cell(5, row), sales, true);

		char formula[64];
		snprintf(formula, sizeof(formula), "=B%d+C%d+D%d+E%d", row, row, row, row);
		TryToParseString(formula, cell(6, row), sales, true);

		for (int col = 2; col <= 6; col++)
		{
			Currency(sales, cell(col, row));
			Border(sales, cell(col, row));
		}
		sales->CalcCell(cell(6, row));
	}

	TryToParseString("Totale Generale", cell(1, 8), sales, true); // A8
	sales->AddMergedRange(range(1, 8, 5, 8)); // A8:E8
	TryToParseString("=F3+F4+F5+F6+F7", cell(6, 8), sales, true); // F8
	for (int col = 1; col <= 6; col++)
	{
		Style(sales, cell(col, 8), [&](CellStyle& cs) {
			cs.fLowColor = kYellow;
			cs.fAlignment = (col == 1) ? eAlignCenter : eAlignRight;
		});
		Border(sales, cell(col, 8));
	}
	Currency(sales, cell(6, 8));
	sales->CalcCell(cell(6, 8));

	sales->SetComment(cell(6, 8),
		"Totale calcolato automaticamente sommando i totali di riga di ogni prodotto.");

	// ---- Foglio "Grafico" (formule fra fogli + grafico incorporato) ----
	CContainer* chartSheet = new CContainer(NULL, NULL);

	TryToParseString("Prodotto", cell(1, 1), chartSheet, true);
	TryToParseString("Totale", cell(2, 1), chartSheet, true);
	for (int col = 1; col <= 2; col++)
		Style(chartSheet, cell(col, 1), [&](CellStyle& cs) {
			cs.fLowColor = kGray; cs.fAlignment = eAlignCenter;
		});

	// Formule "Vendite!..." lasciate deliberatamente NON calcolate qui:
	// senza un ISheetResolver collegato (nessuna vera MainWindow in
	// questo generatore headless) restano "vive" ma non risolvibili,
	// esattamente come in ui/tests/test_xsheet_formulas.cpp -- si
	// risolvono da sole alla riapertura in Atomo123 (MainWindow::
	// OpenFile collega il resolver e ricalcola l'intera cartella).
	for (int i = 0; i < 5; i++)
	{
		int row = i + 2; // righe 2..6
		char formula[32];
		snprintf(formula, sizeof(formula), "=Vendite!A%d", i + 3);
		TryToParseString(formula, cell(1, row), chartSheet, true);
		snprintf(formula, sizeof(formula), "=Vendite!F%d", i + 3);
		TryToParseString(formula, cell(2, row), chartSheet, true);
	}

	TryToParseString("Note", cell(4, 1), chartSheet, true);
	TryToParseString(
		"Questo foglio mostra un grafico incorporato e formule che leggono i totali "
		"dal foglio Vendite.",
		cell(4, 2), chartSheet, true);
	Style(chartSheet, cell(4, 2), [](CellStyle& cs) { cs.fWrapText = true; });
	chartSheet->SetHyperlink(cell(4, 2), "https://example.com");

	ChartObject chart;
	chart.dataRange = range(1, 2, 2, 6); // A2:B6 (etichette + valori)
	chart.frame = BRect(260, 10, 600, 220);

	// ---- Cartella di lavoro ----
	AscdSheet vendite;
	vendite.name = "Vendite";
	vendite.doc = sales;
	vendite.colWidths.push_back(std::make_pair(1, 160.0f)); // colonna A (prodotto) piu' larga
	vendite.rowHeights.push_back(std::make_pair(1, 30.0f)); // riga del titolo piu' alta
	vendite.frozenRows = 2; // titolo + intestazione restano ferme allo scroll
	vendite.hasTabColor = true;
	vendite.tabColor = kBlue;
	vendite.hasAutoFilter = true;
	vendite.autoFilterRange = range(1, 2, 6, 7); // intestazione + righe prodotto (non il totale)

	AscdSheet grafico;
	grafico.name = "Grafico";
	grafico.doc = chartSheet;
	grafico.charts.push_back(chart);
	grafico.colWidths.push_back(std::make_pair(4, 220.0f)); // colonna Note piu' larga
	grafico.frozenRows = 1;
	grafico.hasTabColor = true;
	grafico.tabColor = kOrange;

	std::vector<AscdSheet> sheets;
	sheets.push_back(vendite);
	sheets.push_back(grafico);

	const char* outPath = "atomo123_demo.ascd";
	BFile file(outPath, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
	{
		fprintf(stderr, "Impossibile creare %s\n", outPath);
		sales->Release();
		chartSheet->Release();
		return 1;
	}

	status_t err = SaveASCDBook(sheets, &file);

	// Senza questo, un doppio clic su questo stesso file in Tracker non
	// apre Atomo123: nessuna applicazione preferita per il tipo generico
	// a cui il file ricadrebbe altrimenti -- bug reale scoperto
	// dall'utente proprio su un file generato cosi', vedi il commento
	// gemello su kAtomoNativeMimeType in MainWindow.cpp (che soffriva
	// dello stesso problema in "Salva"/"Salva con nome").
	if (err == B_OK)
	{
		BNodeInfo nodeInfo(&file);
		if (nodeInfo.InitCheck() == B_OK)
			nodeInfo.SetType("application/x-vnd.atomo-sheet-data");
	}
	file.Unset();

	sales->Release();
	chartSheet->Release();

	if (err != B_OK)
	{
		fprintf(stderr, "SaveASCDBook fallita (errore %d)\n", (int)err);
		return 1;
	}

	printf("Creato %s con %zu fogli.\n", outPath, sheets.size());
	return 0;
}
