/*
	generate_cda_report.cpp

	Genera una cartella di lavoro Atomo123 a tre fogli a partire da un
	vero file XLSX (il dataset pubblico "Financial Sample" di
	Microsoft, 700 righe reali), pensata come dimostrazione pratica per
	l'utente: dati reali importati, tabelle raggruppate per categoria e
	tre grafici incorporati, e un foglio di sintesi gia' pronto per la
	stampa su una pagina A4 (riunione del Consiglio di Amministrazione).

	A differenza di una prima versione di questo generatore, QUI TUTTO
	(tabelle raggruppate comprese, non solo i KPI) e' scritto come
	FORMULA dal vivo sul foglio "Dati", non come valore gia' calcolato:
	l'utente ha chiesto esplicitamente un file che si aggiorni da solo
	se cambia qualcosa nei 700 record importati. Le tabelle pivot vere
	dell'app (Inserisci -> Tabella Pivot, vedi Pivot.h) sono invece
	deliberatamente statiche per design (un'istantanea, non ricalcolata
	da sola) -- qui si ottiene lo stesso raggruppamento con SUMIF su un
	elenco FISSO di categorie note (verificate a mano sullo XML sorgente
	di Financial Sample.xlsx: 5 segmenti, 5 paesi, 6 prodotti), che e'
	esattamente come un utente reale costruirebbe un riepilogo del
	genere in Atomo123 oggi (non c'e' ancora un pivot "vivo").

	Stesso approccio headless di generate_demo.cpp (nessuna vera
	MainWindow, solo CContainer + AscdIO), con l'aggiunta
	dell'importazione reale via BTranslatorRoster (stesso identico
	percorso di MainWindow::OpenFile per un file non nativo) per non
	duplicare il parsing XLSX.
*/

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <Application.h>
#include <DataIO.h>
#include <File.h>
#include <Message.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Roster.h>
#include <TranslatorRoster.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "Chart.h"
#include "Formatter.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "Range.h"
#include "ResourceManager.h"

// Stessa copia locale di MainWindow.cpp: kAtomoNativeFormat non e'
// esposta in un header condiviso (ogni translator/l'app ne tiene una
// propria, vedi il commento gia' presente in MainWindow.cpp).
static const uint32 kAtomoNativeFormat = 'ASCD';

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
		cs.fFormat = eCurrency | (2 << 4) | (1 << 9); // 2 decimali, separatore delle migliaia
		cs.fAlignment = eAlignRight;
	});
}

static void Percent(CContainer* doc, cell c)
{
	Style(doc, c, [](CellStyle& cs) {
		cs.fFormat = ePercent | (1 << 4); // 1 decimale
		cs.fAlignment = eAlignRight;
	});
}

static void Integer(CContainer* doc, cell c)
{
	Style(doc, c, [](CellStyle& cs) {
		cs.fFormat = eFixed | (0 << 4) | (1 << 9); // nessun decimale, separatore delle migliaia
		cs.fAlignment = eAlignRight;
	});
}

// Scrive una tabella "Categoria/Somma" a partire da (destCol, 3) con
// una riga di formula SUMIF per categoria (dal vivo sul foglio "Dati",
// mai un valore congelato) -- stesso principio di SUMIF(A1:A10;
// "Roma";B1:B10) gia' documentato in docs/USER_GUIDE.md, qui applicato
// a un elenco di categorie note invece che a un raggruppamento
// calcolato una tantum come farebbe Inserisci -> Tabella Pivot.
static void WriteLiveCategoryTable(CContainer* doc, int destCol,
	const std::vector<BString>& categories, char critCol, char valCol, bool currency, bool integer)
{
	TryToParseString("Categoria", cell(destCol, 3), doc, true);
	TryToParseString("Somma", cell(destCol + 1, 3), doc, true);
	Style(doc, cell(destCol, 3), [](CellStyle& cs) { cs.fLowColor = (rgb_color){ 217, 217, 217, 255 }; });
	Style(doc, cell(destCol + 1, 3), [](CellStyle& cs) { cs.fLowColor = (rgb_color){ 217, 217, 217, 255 }; });
	Border(doc, cell(destCol, 3));
	Border(doc, cell(destCol + 1, 3));

	for (size_t i = 0; i < categories.size(); i++)
	{
		int row = 4 + (int)i;
		TryToParseString(categories[i].String(), cell(destCol, row), doc, true);

		char formula[128];
		snprintf(formula, sizeof(formula), "=SUMIF(Dati!%c2:%c701;\"%s\";Dati!%c2:%c701)",
			critCol, critCol, categories[i].String(), valCol, valCol);
		TryToParseString(formula, cell(destCol + 1, row), doc, true);

		if (currency)
			Currency(doc, cell(destCol + 1, row));
		else if (integer)
			Integer(doc, cell(destCol + 1, row));
		Border(doc, cell(destCol, row));
		Border(doc, cell(destCol + 1, row));
	}
}

int main()
{
	BApplication app("application/x-vnd.Atomo-GenerateCdaReport");

	// Serve la tabella delle funzioni con nome (SUM, SUMIF, LARGE,
	// INDEX, MATCH...) prima di poter parsare QUALUNQUE formula che le
	// usi -- stesso identico bisogno di InitFunctions() in
	// App::ReadyToRun, qui replicato a mano perche' questo e' un
	// generatore headless, non la vera app. Richiede la risorsa 'Func'
	// allegata a questo stesso eseguibile (xres, vedi il comando di
	// link).
	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	const rgb_color kBlue    = { 0, 120, 215, 255 };
	const rgb_color kWhite   = { 255, 255, 255, 255 };
	const rgb_color kGray    = { 217, 217, 217, 255 };
	const rgb_color kLightGray = { 245, 245, 245, 255 };
	const rgb_color kYellow  = { 255, 242, 204, 255 };
	const rgb_color kGreen   = { 0, 150, 80, 255 };
	const rgb_color kOrange  = { 230, 126, 34, 255 };
	const rgb_color kDarkGray = { 90, 90, 90, 255 };

	// ---- Importa il file reale, esattamente come MainWindow::OpenFile
	// per un file non nativo: BTranslatorRoster sceglie XlsxTranslator
	// in base al contenuto, che restituisce un flusso ASCB (Fase 25),
	// riletto qui con LoadASCDBook. ----
	BFile srcFile("/boot/home/Desktop/xls/Financial Sample.xlsx", B_READ_ONLY);
	if (srcFile.InitCheck() != B_OK)
	{
		fprintf(stderr, "Impossibile aprire Financial Sample.xlsx\n");
		return 1;
	}

	BMallocIO ascd;
	BMessage extension;
	status_t translateErr = BTranslatorRoster::Default()->Translate(&srcFile, NULL,
		&extension, &ascd, kAtomoNativeFormat);
	if (translateErr != B_OK)
	{
		fprintf(stderr, "Translate() fallita (%d) - XlsxTranslator installato?\n", (int)translateErr);
		return 1;
	}

	ascd.Seek(0, SEEK_SET);
	std::vector<AscdSheet> imported;
	// CXlsxTranslator::Translate scrive sempre un flusso ASCB (Fase 25,
	// WriteASCDBook), anche per un file XLSX a un solo foglio come
	// questo -- IsASCDBookFile e' quindi sempre vera qui, a differenza
	// di MainWindow::OpenFile che gestisce anche CSV/XLS/ODS (un solo
	// foglio, formato ASCD semplice senza wrapper).
	if (IsASCDBookFile(&ascd))
		LoadASCDBook(&ascd, &imported);

	if (imported.empty())
	{
		fprintf(stderr, "Il file importato non contiene fogli validi\n");
		return 1;
	}

	CContainer* dati = imported[0].doc;
	imported[0].name = "Dati";
	imported[0].frozenRows = 1; // intestazione (riga 1) sempre visibile
	imported[0].hasAutoFilter = true;
	imported[0].autoFilterRange = range(1, 1, 16, 701); // A1:P701
	imported[0].hasTabColor = true;
	imported[0].tabColor = kDarkGray;

	// Colonne del dataset reale (verificate a mano sullo XML sorgente):
	// A=Segment B=Country C=Product D=Discount Band E=Units Sold
	// F=Manufacturing Price G=Sale Price H=Gross Sales I=Discounts
	// J=Sales K=COGS L=Profit M=Date N=Month Number O=Month Name P=Year
	//
	// Elenchi di categorie FISSI (le uniche 5/5/6 che il dataset reale
	// contiene, lette dallo XML sorgente -- non un raggruppamento
	// dinamico: senza un pivot "vivo" nel motore, un riepilogo per
	// categoria in Atomo123 oggi si scrive cosi', un SUMIF per
	// categoria nota).
	std::vector<BString> segments;
	segments.push_back("Small Business");
	segments.push_back("Midmarket");
	segments.push_back("Enterprise");
	segments.push_back("Government");
	segments.push_back("Channel Partners");

	std::vector<BString> countries;
	countries.push_back("Canada");
	countries.push_back("United States of America");
	countries.push_back("France");
	countries.push_back("Germany");
	countries.push_back("Mexico");

	std::vector<BString> products;
	products.push_back("Carretera");
	products.push_back("Montana");
	products.push_back("Paseo");
	products.push_back("Velo");
	products.push_back("VTT");
	products.push_back("Amarilla");

	// ==================== Foglio "Pivot" ====================
	CContainer* pivot = new CContainer(NULL, NULL);

	TryToParseString("Analisi per categoria - Financial Sample (700 record, formule dal vivo)",
		cell(1, 1), pivot, true);
	pivot->AddMergedRange(range(1, 1, 8, 1));
	Style(pivot, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kBlue; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});

	struct CategoryBlock {
		const char* label; int destCol; const std::vector<BString>* categories;
		char critCol, valCol; bool currency, integer;
	};
	CategoryBlock blocks[] = {
		{ "Per segmento (somma vendite)", 1, &segments, 'A', 'J', true, false },
		{ "Per paese (somma profitto)", 4, &countries, 'B', 'L', true, false },
		{ "Per prodotto (somma unita' vendute)", 7, &products, 'C', 'E', false, true },
	};

	for (int b = 0; b < 3; b++)
	{
		cell labelCell(blocks[b].destCol, 2);
		TryToParseString(blocks[b].label, labelCell, pivot, true);
		pivot->AddMergedRange(range(blocks[b].destCol, 2, blocks[b].destCol + 1, 2));
		Style(pivot, labelCell, [&](CellStyle& cs) {
			cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
		});

		WriteLiveCategoryTable(pivot, blocks[b].destCol, *blocks[b].categories,
			blocks[b].critCol, blocks[b].valCol, blocks[b].currency, blocks[b].integer);
	}

	// Tre grafici incorporati (barre, torta, barre): dati letti dal
	// vivo dalle formule appena scritte sopra -- il grafico si
	// aggiorna da solo ogni volta che il documento viene ricalcolato,
	// non serve rigenerare il file.
	ChartObject chartSeg;
	chartSeg.type = eBarChart;
	chartSeg.title = "Vendite per segmento";
	chartSeg.dataRange = range(1, 4, 2, 3 + (int)segments.size());
	chartSeg.frame = BRect(20, 230, 380, 430);

	ChartObject chartCountry;
	chartCountry.type = ePieChart;
	chartCountry.title = "Profitto per paese";
	chartCountry.dataRange = range(4, 4, 5, 3 + (int)countries.size());
	chartCountry.frame = BRect(400, 230, 700, 430);

	ChartObject chartProduct;
	chartProduct.type = eBarChart;
	chartProduct.title = "Unita' vendute per prodotto";
	chartProduct.dataRange = range(7, 4, 8, 3 + (int)products.size());
	chartProduct.frame = BRect(20, 450, 500, 650);

	// ==================== Foglio "Riunione CdA" ====================
	CContainer* cda = new CContainer(NULL, NULL);

	TryToParseString("Riunione CdA - Sintesi Vendite", cell(1, 1), cda, true);
	cda->AddMergedRange(range(1, 1, 4, 1));
	Style(cda, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kGreen; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});

	TryToParseString(
		"Dati: Financial Sample - 700 record, tutte le formule si aggiornano da sole",
		cell(1, 2), cda, true);
	cda->AddMergedRange(range(1, 2, 4, 2));
	Style(cda, cell(1, 2), [&](CellStyle& cs) {
		cs.fHighColor = kDarkGray; cs.fAlignment = eAlignCenter;
	});

	// Riga KPI: intestazioni (riga 4) + formule dal vivo sul foglio
	// "Dati" (riga 5) -- lasciate deliberatamente NON calcolate qui,
	// stesso principio di generate_demo.cpp: senza un vero
	// ISheetResolver (nessuna MainWindow in questo generatore
	// headless) restano "vive" ma non risolvibili, si calcolano da
	// sole alla riapertura in Atomo123.
	const char* kpiHeaders[] = { "Vendite Totali", "Profitto Totale", "Margine %", "Unita' Vendute" };
	for (int col = 1; col <= 4; col++)
	{
		TryToParseString(kpiHeaders[col - 1], cell(col, 4), cda, true);
		Style(cda, cell(col, 4), [&](CellStyle& cs) {
			cs.fLowColor = kGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
		});
		Border(cda, cell(col, 4));
	}

	TryToParseString("=SUM(Dati!J2:J701)", cell(1, 5), cda, true);
	TryToParseString("=SUM(Dati!L2:L701)", cell(2, 5), cda, true);
	TryToParseString("=B5/A5", cell(3, 5), cda, true);
	TryToParseString("=SUM(Dati!E2:E701)", cell(4, 5), cda, true);
	Currency(cda, cell(1, 5));
	Currency(cda, cell(2, 5));
	Percent(cda, cell(3, 5));
	Integer(cda, cell(4, 5));
	for (int col = 1; col <= 4; col++)
	{
		Border(cda, cell(col, 5));
		Style(cda, cell(col, 5), [&](CellStyle& cs) { cs.fLowColor = kYellow; });
	}
	cda->SetComment(cell(1, 5),
		"Formula dal vivo (somma della colonna Sales del foglio Dati): si aggiorna da sola "
		"se i dati importati cambiano, non un numero congelato.");

	// Sezione "vendite per segmento", classificata per valore
	// decrescente SENZA congelare nessun numero: LARGE(...) prende il
	// k-esimo valore piu' alto dalla tabella del foglio Pivot (dal vivo,
	// formule SUMIF, mai un'istantanea), INDEX/MATCH ne risale il nome
	// del segmento -- esattamente come si farebbe in Excel per una
	// "classifica" che si aggiorna da sola quando i dati sorgente
	// cambiano.
	TryToParseString("Vendite per segmento (classifica, formule dal vivo)", cell(1, 7), cda, true);
	cda->AddMergedRange(range(1, 7, 3, 7));
	Style(cda, cell(1, 7), [&](CellStyle& cs) {
		cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});

	const char* rankHeaders[] = { "Segmento", "Vendite", "Quota %" };
	for (int col = 1; col <= 3; col++)
	{
		TryToParseString(rankHeaders[col - 1], cell(col, 8), cda, true);
		Style(cda, cell(col, 8), [&](CellStyle& cs) { cs.fLowColor = kGray; cs.fAlignment = eAlignCenter; });
		Border(cda, cell(col, 8));
	}

	for (size_t i = 0; i < segments.size(); i++)
	{
		int row = 9 + (int)i;
		int rank = (int)i + 1;

		char valueFormula[96];
		snprintf(valueFormula, sizeof(valueFormula), "=LARGE(Pivot!$B$4:$B$%d;%d)",
			3 + (int)segments.size(), rank);
		TryToParseString(valueFormula, cell(2, row), cda, true);

		char nameFormula[128];
		snprintf(nameFormula, sizeof(nameFormula),
			"=INDEX(Pivot!$A$4:$A$%d;MATCH(B%d;Pivot!$B$4:$B$%d;0))",
			3 + (int)segments.size(), row, 3 + (int)segments.size());
		TryToParseString(nameFormula, cell(1, row), cda, true);

		char shareFormula[32];
		snprintf(shareFormula, sizeof(shareFormula), "=B%d/$A$5", row);
		TryToParseString(shareFormula, cell(3, row), cda, true);

		Currency(cda, cell(2, row));
		Percent(cda, cell(3, row));
		for (int col = 1; col <= 3; col++)
			Border(cda, cell(col, row));
	}

	int lastRankRow = 8 + (int)segments.size();
	float chartTop = 20 + (lastRankRow + 1) * 20;
	float chartBottom = chartTop + 220;

	ChartObject chartRank;
	chartRank.type = eBarChart;
	chartRank.title = "Vendite per segmento";
	chartRank.dataRange = range(1, 9, 2, lastRankRow);
	chartRank.frame = BRect(20, chartTop, 460, chartBottom);

	// Riga di nota sotto al grafico, con un margine di sicurezza (20px,
	// una riga) per non sovrapporsi al bordo inferiore del grafico:
	// SheetView::kRowHeight = 20px, riga 1 (titolo) alta 30px invece di
	// 20 (vedi rowHeights piu' sotto), quindi la riga N inizia a
	// 10 + (N-1)*20 pixel, non semplicemente N*20.
	int noteRow = (int)((chartBottom - 10) / 20) + 2;
	TryToParseString(
		"Generato automaticamente da Atomo123 a partire da Financial Sample.xlsx (dataset "
		"dimostrativo pubblico Microsoft). Ogni numero di questo file e' una formula dal vivo: "
		"modificando i dati nel foglio Dati, tutto il resto si aggiorna da solo al ricalcolo.",
		cell(1, noteRow), cda, true);
	cda->AddMergedRange(range(1, noteRow, 4, noteRow));
	Style(cda, cell(1, noteRow), [&](CellStyle& cs) { cs.fHighColor = kDarkGray; cs.fWrapText = true; });

	// ==================== Cartella di lavoro ====================
	AscdSheet cdaSheet;
	cdaSheet.name = "Riunione CdA";
	cdaSheet.doc = cda;
	cdaSheet.charts.push_back(chartRank);
	cdaSheet.colWidths.push_back(std::make_pair(1, 140.0f));
	cdaSheet.colWidths.push_back(std::make_pair(2, 130.0f));
	cdaSheet.colWidths.push_back(std::make_pair(3, 110.0f));
	cdaSheet.colWidths.push_back(std::make_pair(4, 130.0f));
	cdaSheet.rowHeights.push_back(std::make_pair(1, 30.0f));
	cdaSheet.hasTabColor = true;
	cdaSheet.tabColor = kGreen;

	AscdSheet pivotSheet;
	pivotSheet.name = "Pivot";
	pivotSheet.doc = pivot;
	pivotSheet.charts.push_back(chartSeg);
	pivotSheet.charts.push_back(chartCountry);
	pivotSheet.charts.push_back(chartProduct);
	pivotSheet.colWidths.push_back(std::make_pair(1, 130.0f));
	pivotSheet.colWidths.push_back(std::make_pair(4, 130.0f));
	pivotSheet.colWidths.push_back(std::make_pair(7, 130.0f));
	pivotSheet.rowHeights.push_back(std::make_pair(1, 30.0f));
	pivotSheet.hasTabColor = true;
	pivotSheet.tabColor = kOrange;

	std::vector<AscdSheet> sheets;
	sheets.push_back(cdaSheet);   // foglio attivo all'apertura
	sheets.push_back(pivotSheet);
	sheets.push_back(imported[0]); // "Dati", con i dati reali importati

	const char* outPath = "/boot/home/Desktop/Financial_Sample_CdA.ascd";
	BFile outFile(outPath, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (outFile.InitCheck() != B_OK)
	{
		fprintf(stderr, "Impossibile creare %s\n", outPath);
		return 1;
	}

	status_t err = SaveASCDBook(sheets, &outFile);

	// Senza questo, un doppio clic su questo stesso file in Tracker non
	// apre Atomo123 (bug reale scoperto proprio su un file generato
	// cosi', vedi il commento gemello su kAtomoNativeMimeType in
	// MainWindow.cpp).
	if (err == B_OK)
	{
		BNodeInfo nodeInfo(&outFile);
		if (nodeInfo.InitCheck() == B_OK)
			nodeInfo.SetType("application/x-vnd.atomo-sheet-data");
	}
	outFile.Unset();

	cda->Release();
	pivot->Release();
	dati->Release();

	if (err != B_OK)
	{
		fprintf(stderr, "SaveASCDBook fallita (errore %d)\n", (int)err);
		return 1;
	}

	printf("Creato %s con %zu fogli (tutte le tabelle sono formule dal vivo).\n",
		outPath, sheets.size());
	return 0;
}
