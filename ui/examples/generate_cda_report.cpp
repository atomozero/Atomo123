/*
	generate_cda_report.cpp

	Genera una cartella di lavoro Atomo123 a QUATTRO fogli a partire da
	un vero file XLSX (il dataset pubblico "Financial Sample" di
	Microsoft, 700 righe reali) -- pensata per la prossima release
	come dimostrazione pratica di gran parte di quello che Atomo123 sa
	fare, su dati veri, non un elenco astratto di funzionalita':

	- "Riunione CdA": KPI e classifica dal vivo, grafico, area di
	  stampa/margini/scala impostati per davvero (Fase 29, ora salvati
	  per foglio) e adattati a una pagina sola.
	- "Pivot": raggruppamento per categoria (SUMIF su elenchi noti) e
	  tre tipi di grafico (barre/torta/barre).
	- "Funzioni": un catalogo di circa 50 funzioni con nome applicate
	  ai dati reali (testo, data, logica, ricerca, matematica/
	  statistica), piu' una formula a blocco (SEQUENCE), una tabella
	  strutturata ("Vendite[Colonna]") e una piccola ricerca
	  interattiva (Convalida dati + SUMIF).
	- "Dati": le 700 righe importate integralmente, con una regola di
	  formattazione condizionale viva (Discount Band = "High").

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
#include "PrintLayout.h"
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

// NewCell diretto, MAI TryToParseString, per ogni etichetta di puro
// testo (titoli, intestazioni, nomi) di questo generatore: un bug
// reale del motore (corretto in engine/src/Formula/parser.cpp,
// scoperto proprio scrivendo questo file) faceva sì che un'etichetta
// nuda corrispondente per caso al nome di una funzione ("TODAY",
// "CONCAT"...) venisse silenziosamente CALCOLATA invece di restare
// testo -- gia' corretto alla radice, ma un'etichetta che contiene
// operatori veri ("INDEX+MATCH", "Tabella[Col] + SUM") resta comunque
// un'espressione valida per il parser, per design (non un bug: "+" e
// "[" hanno un significato reale in una formula). Passare sempre da
// qui, mai da TryToParseString, elimina l'ambiguita' alla radice per
// QUALUNQUE testo, invece di dover verificare ogni etichetta caso per
// caso.
static void WriteLabel(CContainer* doc, cell c, const char* text)
{
	doc->NewCell(c, Value(text), NULL);
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
	WriteLabel(doc, cell(destCol, 3), "Categoria");
	WriteLabel(doc, cell(destCol + 1, 3), "Somma");
	Style(doc, cell(destCol, 3), [](CellStyle& cs) { cs.fLowColor = (rgb_color){ 217, 217, 217, 255 }; });
	Style(doc, cell(destCol + 1, 3), [](CellStyle& cs) { cs.fLowColor = (rgb_color){ 217, 217, 217, 255 }; });
	Border(doc, cell(destCol, 3));
	Border(doc, cell(destCol + 1, 3));

	for (size_t i = 0; i < categories.size(); i++)
	{
		int row = 4 + (int)i;
		WriteLabel(doc, cell(destCol, row), categories[i].String());

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

	// Tabella strutturata "Vendite" (Fase 14, "Tabella12[Colonna]"):
	// registrata sull'intero intervallo importato, cosi' il foglio
	// "Funzioni" piu' sotto puo' scrivere formule come "Vendite[Sales]"
	// invece di "Dati!J2:J701" -- stessa identica sintassi che un vero
	// file XLSX con una Tabella Excel produce all'importazione (vedi
	// Excel.cpp), qui costruita a mano perche' questo file nasce da un
	// generatore, non da un vero file con una Tabella gia' definita.
	// dataRange ESCLUDE la riga di intestazione (riga 1), stesso
	// principio di CTableDef in ogni test/importazione esistente.
	{
		CTableDef table;
		table.dataRange = range(1, 2, 16, 701); // A2:P701
		const char* columnNames[] = {
			"Segment", "Country", "Product", "Discount Band", "Units Sold",
			"Manufacturing Price", "Sale Price", "Gross Sales", "Discounts",
			"Sales", "COGS", "Profit", "Date", "Month Number", "Month Name", "Year"
		};
		for (size_t i = 0; i < sizeof(columnNames) / sizeof(columnNames[0]); i++)
			table.columnNames.push_back(columnNames[i]);
		dati->AddTable("Vendite", table);
	}

	// Formattazione condizionale VIVA (Fase 13) sul foglio "Dati": ogni
	// riga con Discount Band = "High" prende uno sfondo evidenziato --
	// un vero rischio di margine, utile da vedere a colpo d'occhio
	// scorrendo 700 righe. eCondCellIsEqual confronta con un valore
	// FISSO (non un "e' il piu' alto della colonna", che questo motore
	// non supporta ancora, vedi ROADMAP.md): "High" resta valido
	// qualunque cosa cambi nei dati, non e' un'istantanea come i valori
	// del vecchio generatore.
	{
		ConditionalFormatRule rule;
		rule.type = eCondCellIsEqual;
		rule.compareValue = "High";
		rule.bgColor = (rgb_color){ 255, 205, 205, 255 };
		rule.ranges.push_back(range(4, 2, 4, 701)); // D2:D701 (Discount Band)
		dati->AddConditionalFormatRule(rule);
	}

	// Scala di colori a due punti (Fase 33/A, appena aggiunta): a
	// differenza della regola sopra (un confronto per-cella con un
	// valore fisso), qui il colore di OGNI cella numerica dipende dal
	// minimo/massimo di TUTTO l'intervallo -- rosso il profitto piu'
	// basso della colonna, verde il piu' alto, interpolato in mezzo.
	// Sulla colonna Profit (L, 700 righe) rende visibile a colpo
	// d'occhio quali vendite sono state le piu'/meno redditizie, senza
	// bisogno di ordinare o filtrare nulla.
	{
		ConditionalFormatRule rule;
		rule.type = eCondColorScale;
		rule.ranges.push_back(range(12, 2, 12, 701)); // L2:L701 (Profit)

		ColorScalePoint minPoint;
		minPoint.cfvoType = "min";
		minPoint.color = (rgb_color){ 248, 105, 107, 255 }; // F8696B, rosso Excel
		rule.colorScalePoints.push_back(minPoint);

		ColorScalePoint maxPoint;
		maxPoint.cfvoType = "max";
		maxPoint.color = (rgb_color){ 99, 190, 123, 255 }; // 63BE7B, verde Excel
		rule.colorScalePoints.push_back(maxPoint);

		dati->AddConditionalFormatRule(rule);
	}

	// ==================== Foglio "Pivot" ====================
	CContainer* pivot = new CContainer(NULL, NULL);

	WriteLabel(pivot, cell(1, 1), "Analisi per categoria - Financial Sample (700 record, formule dal vivo)");
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
		WriteLabel(pivot, labelCell, blocks[b].label);
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

	WriteLabel(cda, cell(1, 1), "Riunione CdA - Sintesi Vendite");
	cda->AddMergedRange(range(1, 1, 4, 1));
	Style(cda, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kGreen; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});

	WriteLabel(cda, cell(1, 2),
		"Dati: Financial Sample - 700 record, tutte le formule si aggiornano da sole");
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
		WriteLabel(cda, cell(col, 4), kpiHeaders[col - 1]);
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
	WriteLabel(cda, cell(1, 7), "Vendite per segmento (classifica, formule dal vivo)");
	cda->AddMergedRange(range(1, 7, 3, 7));
	Style(cda, cell(1, 7), [&](CellStyle& cs) {
		cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});

	const char* rankHeaders[] = { "Segmento", "Vendite", "Quota %" };
	for (int col = 1; col <= 3; col++)
	{
		WriteLabel(cda, cell(col, 8), rankHeaders[col - 1]);
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
	WriteLabel(cda, cell(1, noteRow),
		"Generato automaticamente da Atomo123 a partire da Financial Sample.xlsx (dataset "
		"dimostrativo pubblico Microsoft). Ogni numero di questo file e' una formula dal vivo: "
		"modificando i dati nel foglio Dati, tutto il resto si aggiorna da solo al ricalcolo.");
	cda->AddMergedRange(range(1, noteRow, 4, noteRow));
	Style(cda, cell(1, noteRow), [&](CellStyle& cs) { cs.fHighColor = kDarkGray; cs.fWrapText = true; });

	// ==================== Foglio "Funzioni" ====================
	// Catalogo dal vivo: una riga per funzione, con la formula vera
	// (colonna "Formula", testo letterale per poterla leggere) e il
	// suo risultato calcolato (colonna "Risultato", la STESSA formula
	// scritta come formula vera) -- applicate ai dati reali importati
	// dove ha senso (es. VLOOKUP/XLOOKUP su "Government"), o ad
	// argomenti letterali sicuri dove il contenuto dei dati non conta
	// per il funzionamento della funzione (es. UPPER/TRIM). Non e'
	// l'elenco COMPLETO di ogni funzione del motore (alcune sono
	// residui storici di Sum-It poco rilevanti oggi, es. ANNUITY/DB/
	// SOYD per l'ammortamento, o CELL/PAGE/NUMPAGES legate alla
	// stampa) -- una rassegna ampia e rappresentativa delle funzioni
	// che un utente userebbe davvero, non un test esaustivo.
	CContainer* funcs = new CContainer(NULL, NULL);

	WriteLabel(funcs, cell(1, 1), "Catalogo delle funzioni - applicate ai dati reali");
	funcs->AddMergedRange(range(1, 1, 4, 1));
	Style(funcs, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kBlue; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});
	WriteLabel(funcs, cell(1, 2),
		"Ogni riga sotto e' una formula VIVA (colonna Risultato): filtra con AutoFilter per categoria.");
	funcs->AddMergedRange(range(1, 2, 4, 2));
	Style(funcs, cell(1, 2), [&](CellStyle& cs) { cs.fHighColor = kDarkGray; cs.fAlignment = eAlignCenter; });

	const char* catalogHeaders[] = { "Categoria", "Funzione", "Formula", "Risultato" };
	for (int col = 1; col <= 4; col++)
	{
		WriteLabel(funcs, cell(col, 4), catalogHeaders[col - 1]);
		Style(funcs, cell(col, 4), [&](CellStyle& cs) {
			cs.fLowColor = kGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
		});
		Border(funcs, cell(col, 4));
	}

	struct FuncDemo { const char* category; const char* name; const char* formula; };
	const FuncDemo demos[] = {
		// -- Testo --
		{ "Testo", "UPPER", "=UPPER(\"maiuscolo\")" },
		{ "Testo", "LOWER", "=LOWER(\"MINUSCOLO\")" },
		{ "Testo", "PROPER", "=PROPER(\"nome cognome\")" },
		{ "Testo", "TRIM", "=TRIM(\"  spazi di troppo  \")" },
		{ "Testo", "CONCAT", "=CONCAT(Dati!A2;\" / \";Dati!B2)" },
		{ "Testo", "TEXTJOIN", "=TEXTJOIN(\", \";TRUE;Dati!A2;Dati!B2;Dati!C2)" },
		{ "Testo", "SUBSTITUTE", "=SUBSTITUTE(\"Small Business\";\"Business\";\"Biz\")" },
		{ "Testo", "REPLACE", "=REPLACE(\"Atomo123\";1;5;\"Nuovo\")" },
		{ "Testo", "REPT", "=REPT(\"=\";10)" },
		{ "Testo", "EXACT", "=EXACT(Dati!A2;Dati!A3)" },
		{ "Testo", "VALUE", "=VALUE(\"1234.5\")" },
		{ "Testo", "TEXT", "=TEXT(Dati!J2;\"0.00\")" },
		// -- Data --
		{ "Data", "TODAY", "=TODAY()" },
		{ "Data", "YEAR", "=YEAR(Dati!M2)" },
		{ "Data", "MONTH", "=MONTH(Dati!M2)" },
		{ "Data", "DAY", "=DAY(Dati!M2)" },
		{ "Data", "EDATE", "=EDATE(Dati!M2;3)" },
		{ "Data", "EOMONTH", "=EOMONTH(Dati!M2;0)" },
		{ "Data", "NETWORKDAYS", "=NETWORKDAYS(DATE(2013;1;1);DATE(2013;12;31))" },
		{ "Data", "WORKDAY", "=WORKDAY(DATE(2026;1;1);10)" },
		{ "Data", "DATEDIF", "=DATEDIF(Dati!M2;TODAY();\"Y\")" },
		// -- Logica --
		{ "Logica", "IF", "=IF(Dati!J2>100000;\"Alta\";\"Bassa\")" },
		{ "Logica", "AND", "=AND(Dati!E2>0;Dati!J2>0)" },
		{ "Logica", "OR", "=OR(Dati!D2=\"High\";Dati!D2=\"Low\")" },
		{ "Logica", "NOT", "=NOT(Dati!D2=\"None\")" },
		{ "Logica", "XOR", "=XOR(Dati!E2>1000;Dati!J2>500000)" },
		{ "Logica", "SWITCH", "=SWITCH(Dati!D2;\"None\";\"N\";\"Low\";\"L\";\"Medium\";\"M\";\"High\";\"H\")" },
		{ "Logica", "IFERROR", "=IFERROR(1/0;\"Errore evitato\")" },
		{ "Logica", "IFNA", "=IFNA(XMATCH(\"NonEsiste\";Dati!A2:A10);\"Non trovato\")" },
		{ "Logica", "ISBLANK", "=ISBLANK(Q1)" },
		{ "Logica", "ISFORMULA", "=ISFORMULA(Dati!J2:J3)" }, // intervallo di ALMENO due celle: vedi il limite noto in Functions.logical.cpp
		// -- Ricerca --
		{ "Ricerca", "VLOOKUP", "=VLOOKUP(\"Government\";Dati!A2:J701;10;0)" },
		{ "Ricerca", "INDEX+MATCH", "=INDEX(Dati!J2:J701;MATCH(\"Government\";Dati!A2:A701;0))" },
		{ "Ricerca", "XLOOKUP", "=XLOOKUP(\"Government\";Dati!A2:A701;Dati!J2:J701)" },
		{ "Ricerca", "XMATCH", "=XMATCH(\"Government\";Dati!A2:A701)" },
		{ "Ricerca", "INDIRECT", "=INDIRECT(\"Dati!J2\")" },
		{ "Ricerca", "ADDRESS", "=ADDRESS(2;10)" },
		{ "Ricerca", "Tabella[Col] + SUM", "=SUM(Vendite[Sales])" },
		{ "Ricerca", "Tabella[Col] + INDEX/MATCH", "=INDEX(Vendite[Profit];MATCH(\"Government\";Vendite[Segment];0))" },
		// -- Matematica e statistica --
		{ "Matematica", "SUMPRODUCT", "=SUMPRODUCT(Dati!E2:E11;Dati!G2:G11)" },
		{ "Matematica", "AVERAGEIFS", "=AVERAGEIFS(Dati!J2:J701;Dati!A2:A701;\"Government\";Dati!B2:B701;\"Canada\")" },
		{ "Matematica", "MAXIFS", "=MAXIFS(Dati!J2:J701;Dati!A2:A701;\"Government\")" },
		{ "Matematica", "MINIFS", "=MINIFS(Dati!J2:J701;Dati!A2:A701;\"Government\")" },
		{ "Matematica", "RANK", "=RANK(Dati!J2;Dati!J2:J701)" },
		{ "Matematica", "LARGE", "=LARGE(Dati!J2:J701;1)" },
		{ "Matematica", "SMALL", "=SMALL(Dati!J2:J701;1)" },
		{ "Matematica", "SUBTOTAL", "=SUBTOTAL(9;Dati!J2:J701)" },
		{ "Matematica", "MEDIAN", "=MEDIAN(Dati!J2:J701)" },
		{ "Matematica", "STDDEV", "=STDDEV(Dati!J2:J701)" },
		{ "Matematica", "ROUND", "=ROUND(Dati!J2;0)" },
		{ "Matematica", "COUNTIF", "=COUNTIF(Dati!A2:A701;\"Government\")" },
	};
	const int demoCount = sizeof(demos) / sizeof(demos[0]);

	int catalogFirstRow = 5;
	for (int i = 0; i < demoCount; i++)
	{
		int row = catalogFirstRow + i;
		WriteLabel(funcs, cell(1, row), demos[i].category);
		WriteLabel(funcs, cell(2, row), demos[i].name);
		WriteLabel(funcs, cell(3, row), demos[i].formula); // testo letterale, non una formula
		TryToParseString(demos[i].formula, cell(4, row), funcs, true); // la STESSA, ma viva
		for (int col = 1; col <= 4; col++)
			Border(funcs, cell(col, row));
	}
	int catalogLastRow = catalogFirstRow + demoCount - 1;

	// SEQUENCE (Fase 28, formula a blocco/"spill"): una sola formula
	// riempie un blocco di celle, l'unica di questo catalogo che non
	// sta in una riga sola -- vedi il paragrafo dedicato in
	// docs/USER_GUIDE.md.
	int sequenceLabelRow = catalogLastRow + 3;
	// NewCell diretto, non TryToParseString: le virgolette incorporate
	// in questa etichetta ("spill") fanno tentare al parser una lettura
	// come espressione (stesso principio del bug del testo ambiguo
	// "P-EL-a" gia' corretto altrove), che qui fallisce e lancia una
	// CParseErr invece di ricadere silenziosamente su testo letterale
	// -- bug reale scoperto generando proprio questo file.
	funcs->NewCell(cell(1, sequenceLabelRow), Value("SEQUENCE (formula a blocco/\"spill\")"), NULL);
	funcs->AddMergedRange(range(1, sequenceLabelRow, 4, sequenceLabelRow));
	Style(funcs, cell(1, sequenceLabelRow), [&](CellStyle& cs) {
		cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});
	int sequenceDataRow = sequenceLabelRow + 1;
	TryToParseString("=SEQUENCE(5;3;1;1)", cell(1, sequenceDataRow), funcs, true);

	// Ricerca interattiva: Convalida dati (Fase 13, elenco a discesa)
	// piu' SUMIF dal vivo -- cambiando la scelta nella cella qui sotto,
	// la vendita totale del segmento scelto si ricalcola da sola,
	// stessa combinazione che un utente reale userebbe per un piccolo
	// "pannello di controllo" del foglio.
	int interactiveLabelRow = sequenceDataRow + 7; // sotto al blocco SEQUENCE (5 righe) piu' margine
	WriteLabel(funcs, cell(1, interactiveLabelRow), "Ricerca interattiva (Convalida dati + SUMIF)");
	funcs->AddMergedRange(range(1, interactiveLabelRow, 4, interactiveLabelRow));
	Style(funcs, cell(1, interactiveLabelRow), [&](CellStyle& cs) {
		cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});

	int pickerRow = interactiveLabelRow + 1;
	WriteLabel(funcs, cell(1, pickerRow), "Scegli un segmento:");
	WriteLabel(funcs, cell(2, pickerRow), "Government"); // valore iniziale della cella con l'elenco
	Style(funcs, cell(2, pickerRow), [&](CellStyle& cs) { cs.fLowColor = kYellow; });
	Border(funcs, cell(2, pickerRow));
	{
		ValidationRule rule;
		rule.type = eListValidation;
		rule.list = "Small Business,Midmarket,Enterprise,Government,Channel Partners";
		funcs->SetValidation(cell(2, pickerRow), rule);
	}

	int resultRow = pickerRow + 1;
	WriteLabel(funcs, cell(1, resultRow), "Vendite di quel segmento:");
	char pickerFormula[80];
	snprintf(pickerFormula, sizeof(pickerFormula), "=SUMIF(Dati!A2:A701;B%d;Dati!J2:J701)", pickerRow);
	TryToParseString(pickerFormula, cell(2, resultRow), funcs, true);
	Currency(funcs, cell(2, resultRow));
	Border(funcs, cell(2, resultRow));
	funcs->SetComment(cell(2, pickerRow),
		"Convalida dati: clic con il tasto destro sulla cella per vedere l'elenco a discesa "
		"(Small Business/Midmarket/Enterprise/Government/Channel Partners). Cambiando la scelta, "
		"la formula sotto si ricalcola da sola.");

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
	// Area di stampa e margini/scala (Fase 29, ora per foglio -- vedi
	// AscdPrintSettings in AscdIO.h): esattamente il contenuto di
	// questo foglio, adattato a UNA pagina sola (kPrintFitBoth) --
	// il vero motivo per cui questo foglio esiste, "pronto per la
	// stampa" non e' piu' solo un commento nella guida utente ma
	// un'impostazione salvata davvero nel file.
	cdaSheet.hasPrintArea = true;
	cdaSheet.printArea = range(1, 1, 4, noteRow);
	cdaSheet.printSettings.hasSettings = true;
	cdaSheet.printSettings.marginTopCm = 1.5;
	cdaSheet.printSettings.marginBottomCm = 1.5;
	cdaSheet.printSettings.marginLeftCm = 1.5;
	cdaSheet.printSettings.marginRightCm = 1.5;
	cdaSheet.printSettings.scaleMode = kPrintFitBoth;

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

	AscdSheet funcsSheet;
	funcsSheet.name = "Funzioni";
	funcsSheet.doc = funcs;
	funcsSheet.colWidths.push_back(std::make_pair(1, 110.0f));
	funcsSheet.colWidths.push_back(std::make_pair(2, 130.0f));
	funcsSheet.colWidths.push_back(std::make_pair(3, 320.0f));
	funcsSheet.colWidths.push_back(std::make_pair(4, 160.0f));
	funcsSheet.rowHeights.push_back(std::make_pair(1, 30.0f));
	funcsSheet.frozenRows = 4; // titolo+sottotitolo+intestazione restano visibili scorrendo il catalogo
	funcsSheet.hasAutoFilter = true;
	funcsSheet.autoFilterRange = range(1, 4, 4, catalogLastRow);
	funcsSheet.hasTabColor = true;
	funcsSheet.tabColor = (rgb_color){ 130, 100, 190, 255 };

	std::vector<AscdSheet> sheets;
	sheets.push_back(cdaSheet);   // foglio attivo all'apertura
	sheets.push_back(pivotSheet);
	sheets.push_back(funcsSheet);
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
	funcs->Release();
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
