/*
	generate_cda_report.cpp

	Genera una cartella di lavoro Atomo123 a CINQUE fogli a partire da
	un vero file XLSX (il dataset pubblico "Financial Sample" di
	Microsoft, 700 righe reali) -- pensata come dimostrazione pratica
	di QUASI OGNI feature reale di Atomo123, su dati veri, non un
	elenco astratto di funzionalita'. Aggiornata a ogni nuova feature
	rilevante (vedi la memoria di progetto "feedback_showcase_file_
	update"), non solo alla creazione iniziale. Tutto il contenuto
	VISIBILE (nomi foglio, intestazioni, titoli grafico, commenti nel
	file generato) e' in INGLESE (richiesta esplicita dell'utente,
	2026-09-26) -- solo i commenti di QUESTO sorgente C++ restano in
	italiano, come da convenzione del progetto:

	- "Board Meeting": KPI e classifica dal vivo (con un bordo colorato
	  sul primo classificato), grafico, area di stampa/margini/scala
	  per davvero (Fase 29, salvati per foglio) e adattati a una
	  pagina sola, un collegamento ipertestuale reale (al repository
	  GitHub del progetto) e un'immagine incorporata (un piccolo logo
	  generato al volo, vedi MakeLogoPng).
	- "Pivot": QUATTRO raggruppamenti dal vivo per categoria (SUMIF su
	  elenchi noti: segmento/paese/prodotto/mese), tutti e tre i tipi
	  di grafico (barre/torta/linee), PIU' una vera tabella pivot
	  statica (BuildPivotTable/WritePivotTable, Inserisci -> Tabella
	  Pivot, raggruppamento a due livelli Prodotto+Fascia sconto) a
	  fianco di quelle dal vivo, per mostrare la differenza reale fra
	  i due approcci, PIU' una vera tabella pivot 2D (v0.3.0,
	  BuildPivotTable2D/WritePivotTable2D: campo Colonne + due misure
	  con aggregazioni diverse, Somma e Media, sullo stesso intervallo
	  sorgente del pivot 1D appena sopra).
	- "Functions": un catalogo di circa 50 funzioni con nome (RATE
	  compresa) applicate ai dati reali, una formula a blocco
	  (SEQUENCE), una tabella strutturata ("SalesTable[Column]"), una
	  ricerca interattiva con un INTERVALLO CON NOME al posto di un
	  indirizzo grezzo (Convalida dati a elenco + SUMIF), una seconda
	  convalida dati a intervallo numerico, un piccolo esempio di
	  formattazione condizionale a valori duplicati, e l'intero foglio
	  PROTETTO tranne le due celle interattive (Fase 32, "Proteggi
	  foglio").
	- "Data": le 700 righe importate integralmente, blocca riquadri
	  per riga E per colonna, con quattro regole di formattazione
	  condizionale dal vivo (Discount Band = "High", scala di colori
	  a due punti sulla colonna Profit, primi 10% per Sales e sopra
	  la media per Units Sold -- questi ultimi due dal completamento
	  di "Path to full Excel parity" Tier 3).
	- "Languages": nuovo (richiesta esplicita dell'utente, 2026-09-26),
	  una tabella con nome/saluto/frase campione/numero di parlanti
	  nativi (approssimato) in otto lingue non inglesi (giapponese,
	  coreano, cinese, arabo, greco, russo, indonesiano, friulano) piu'
	  un grafico a barre sulla stessa tabella -- verifica pratica che
	  il motore/il renderer gestiscano davvero testo Unicode non-Latino
	  (CJK, arabo RTL, cirillico, greco) in celle E in un grafico, non
	  solo negli accenti latini gia' usati altrove in questo file.

	Deliberatamente NON rappresentate in questo file: le tre "Formula
	auditing views" (Mostra formule, Traccia precedenti/dipendenti,
	Finestra di controllo) sono interruttori di sola visualizzazione
	dell'APPLICAZIONE (per-finestra o via BMessage), non dati -- non
	c'e' nessun campo di CContainer/AscdSheet che ne registri lo stato
	da salvare in un file .ascd, quindi non c'e' nulla che questo
	generatore possa scrivere per "mostrarle": si verificano aprendo
	QUALUNQUE file (questo compreso) nell'app vera e usando il nuovo
	menu "Formule".

	A differenza di una prima versione di questo generatore, QUI TUTTO
	(tabelle raggruppate comprese, non solo i KPI) e' scritto come
	FORMULA dal vivo sul foglio "Data", non come valore gia' calcolato:
	l'utente ha chiesto esplicitamente un file che si aggiorni da solo
	se cambia qualcosa nei 700 record importati. Le tabelle pivot vere
	dell'app (Inserisci -> Tabella Pivot, vedi Pivot.h) sono invece
	deliberatamente statiche per design (un'istantanea, non ricalcolata
	da sola) -- il foglio "Pivot" mostra ORA entrambi gli approcci
	fianco a fianco: i quattro raggruppamenti dal vivo con SUMIF su un
	elenco FISSO di categorie note (verificate a mano sullo XML sorgente
	di Financial Sample.xlsx: 5 segmenti, 5 paesi, 6 prodotti, 12 mesi),
	esattamente come un utente reale costruirebbe un riepilogo del
	genere in Atomo123 oggi, PIU' una vera tabella pivot statica per
	mostrare la feature reale dell'app.

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
#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <File.h>
#include <Font.h>
#include <Message.h>
#include <NodeInfo.h>
#include <Path.h>
#include <Roster.h>
#include <TranslatorRoster.h>
#include <View.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "TableStyles.h"
#include "Chart.h"
#include "EmbeddedImage.h"
#include "FontMetrics.h"
#include "Formatter.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "NameTable.h"
#include "Pivot.h"
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

// Bordo con un colore VERO (non il nero predefinito di Border() sopra,
// Fase 13): fBorderColor e' condiviso da tutti e quattro i lati, vedi
// il commento su CellStyle in engine/src/Cell/CellStyle.h.
static void BorderColor(CContainer* doc, cell c, rgb_color color)
{
	Style(doc, c, [&](CellStyle& cs) {
		cs.fTBorderColor = cs.fLBorderColor = cs.fBBorderColor = cs.fRBorderColor = 2; // medio
		cs.fBorderColor = color;
	});
}

// Font in grassetto (Fase 12): CellStyle::fFont e' un indice nella
// tabella globale gFontSizeTable, non un flag booleano -- serve
// registrare (famiglia, stile, dimensione) e riusare l'indice
// restituito. sReservedRegular riserva PRIMA un font "Regular" (mai ne'
// grassetto ne' corsivo): se il primo indice mai assegnato da questo
// processo fosse un font in grassetto, finirebbe per caso proprio
// all'indice 0, lo stesso valore che CellStyle usa come sentinella
// "nessun font esplicito" (memset a zero nel costruttore) -- bug reale
// gia' scoperto una volta scrivendo il test dell'importazione XLSX
// (vedi il commento gemello in XlsxTranslator.cpp, ResolveCellStyles).
static bool sReservedRegularFont = false;

static void Bold(CContainer* doc, cell c)
{
	font_family family;
	font_style style;
	be_plain_font->GetFamilyAndStyle(&family, &style);
	float size = be_plain_font->Size();

	if (!sReservedRegularFont)
	{
		gFontSizeTable.GetFontID(family, style, size);
		sReservedRegularFont = true;
	}

	int fontID = (int)gFontSizeTable.GetFontID(family, "Bold", size);
	Style(doc, c, [&](CellStyle& cs) { cs.fFont = fontID; });
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

// Genera un vero PNG (non un blob finto) per l'immagine incorporata
// dimostrativa piu' sotto -- stessa tecnica di ui/tests/test_image_
// alpha.cpp: disegna su una BBitmap offscreen ("accetta viste") con una
// BView vera, poi la codifica con lo stesso Translation Kit che
// SheetView usa per DECODIFICARE le immagini incorporate. Un piccolo
// logo a blocchi colorati con le iniziali "A123", non un file esterno:
// questo generatore non deve dipendere da nessun asset grafico fuori
// dal proprio codice.
static bool MakeLogoPng(std::vector<uint8>& out)
{
	BRect bounds(0, 0, 63, 63);
	BBitmap* bitmap = new BBitmap(bounds, B_RGBA32, true); // true = accetta viste
	BView* view = new BView(bounds, "logo", B_FOLLOW_NONE, B_WILL_DRAW);
	bitmap->AddChild(view);

	bitmap->Lock();
	view->SetHighColor(0, 120, 215, 255); // stesso blu del titolo "Board Meeting"
	view->FillRoundRect(bounds, 10, 10);
	view->SetHighColor(255, 255, 255, 255);
	view->SetFontSize(22);
	BFont font;
	view->GetFont(&font);
	font.SetFace(B_BOLD_FACE);
	view->SetFont(&font);
	view->DrawString("123", BPoint(10, 40));
	view->Sync();
	bitmap->Unlock();

	BBitmapStream stream(bitmap); // adotta 'bitmap', lo cancella lei
	BMallocIO dest;
	status_t err = BTranslatorRoster::Default()->Translate(&stream, NULL, NULL,
		&dest, B_PNG_FORMAT);
	if (err != B_OK)
		return false;

	out.assign((const uint8*)dest.Buffer(), (const uint8*)dest.Buffer() + dest.BufferLength());
	return true;
}

// NewCell diretto, MAI TryToParseString, per ogni etichetta di puro
// testo (titoli, intestazioni, nomi) di questo generatore: un bug
// reale del motore (corretto in engine/src/Formula/parser.cpp,
// scoperto proprio scrivendo questo file) faceva sì che un'etichetta
// nuda corrispondente per caso al nome di una funzione ("TODAY",
// "CONCAT"...) venisse silenziosamente CALCOLATA invece di restare
// testo -- gia' corretto alla radice, ma un'etichetta che contiene
// operatori veri ("INDEX+MATCH", "Table[Col] + SUM") resta comunque
// un'espressione valida per il parser, per design (non un bug: "+" e
// "[" hanno un significato reale in una formula). Passare sempre da
// qui, mai da TryToParseString, elimina l'ambiguita' alla radice per
// QUALUNQUE testo, invece di dover verificare ogni etichetta caso per
// caso.
static void WriteLabel(CContainer* doc, cell c, const char* text)
{
	doc->NewCell(c, Value(text), NULL);
}

// Scrive una tabella "Category/Sum" a partire da (destCol, 3) con
// una riga di formula SUMIF per categoria (dal vivo sul foglio "Data",
// mai un valore congelato) -- stesso principio di SUMIF(A1:A10;
// "Roma";B1:B10) gia' documentato in docs/USER_GUIDE.md, qui applicato
// a un elenco di categorie note invece che a un raggruppamento
// calcolato una tantum come farebbe Inserisci -> Tabella Pivot.
static void WriteLiveCategoryTable(CContainer* doc, int destCol,
	const std::vector<BString>& categories, char critCol, char valCol, bool currency, bool integer)
{
	WriteLabel(doc, cell(destCol, 3), "Category");
	WriteLabel(doc, cell(destCol + 1, 3), "Sum");
	Style(doc, cell(destCol, 3), [](CellStyle& cs) { cs.fLowColor = (rgb_color){ 217, 217, 217, 255 }; });
	Style(doc, cell(destCol + 1, 3), [](CellStyle& cs) { cs.fLowColor = (rgb_color){ 217, 217, 217, 255 }; });
	Border(doc, cell(destCol, 3));
	Border(doc, cell(destCol + 1, 3));

	for (size_t i = 0; i < categories.size(); i++)
	{
		int row = 4 + (int)i;
		WriteLabel(doc, cell(destCol, row), categories[i].String());

		char formula[128];
		snprintf(formula, sizeof(formula), "=SUMIF(Data!%c2:%c701;\"%s\";Data!%c2:%c701)",
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
	imported[0].name = "Data";
	imported[0].frozenRows = 1; // intestazione (riga 1) sempre visibile
	imported[0].hasAutoFilter = true;
	imported[0].autoFilterRange = range(1, 1, 16, 701); // A1:P701
	// Tier 4 "AutoFilter persistence" (prerequisito per gli Slicer):
	// esclude "None" dalla colonna D (Discount Band, indice 4) -- riaprire
	// questo file mostra solo le righe con uno sconto REALE gia' applicato
	// (Low/Medium/High), non solo il risultato (righe nascoste), ma anche
	// il menu a tendina dell'AutoFilter che "ricorda" quale valore era
	// escluso, esattamente come farebbe Excel.
	imported[0].filterHiddenValues[4].push_back(BString("None"));
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

	// Ordine calendario, non alfabetico: serve per il grafico a linee
	// piu' sotto, dove l'ordine dei punti conta (un andamento nel
	// tempo), a differenza delle altre tre categorie sopra dove
	// l'ordine e' solo estetico.
	std::vector<BString> months;
	months.push_back("January"); months.push_back("February"); months.push_back("March");
	months.push_back("April"); months.push_back("May"); months.push_back("June");
	months.push_back("July"); months.push_back("August"); months.push_back("September");
	months.push_back("October"); months.push_back("November"); months.push_back("December");

	// Tabella strutturata "SalesTable" (Fase 14, "Tabella12[Colonna]"):
	// registrata sull'intero intervallo importato, cosi' il foglio
	// "Functions" piu' sotto puo' scrivere formule come "SalesTable[Sales]"
	// invece di "Data!J2:J701" -- stessa identica sintassi che un vero
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
		// Tier 4 "named table styles": stesso stile che una vera
		// <tableStyleInfo name="TableStyleMedium2"> produrrebbe
		// all'importazione XLSX -- ApplyTableStyleBanding chiamata a
		// mano qui per lo stesso motivo di AddTable sopra (questo file
		// nasce da un generatore, non da un vero import).
		table.tableStyleName = "TableStyleMedium2";
		table.showBandedRows = true;
		dati->AddTable("SalesTable", table);
		ApplyTableStyleBanding(dati, range(1, 1, 16, 701), 0, table.tableStyleName);
	}

	// Formattazione condizionale VIVA (Fase 13) sul foglio "Data": ogni
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

	// Primi 10% per Vendite (Path to full Excel parity, Tier 3, appena
	// completato): a differenza della scala di colori sopra (un colore
	// diverso per OGNI cella), qui solo le celle nella fascia piu' alta
	// prendono un colore, tutte uguale -- lo stesso "Top 10%" di Excel,
	// sulla colonna Sales (J, 700 righe).
	{
		ConditionalFormatRule rule;
		rule.type = eCondTop10;
		rule.top10Rank = 10;
		rule.top10Percent = true;
		rule.bgColor = (rgb_color){ 198, 239, 206, 255 }; // C6EFCE, verde Excel per "Top 10%"
		rule.ranges.push_back(range(10, 2, 10, 701)); // J2:J701 (Sales)
		dati->AddConditionalFormatRule(rule);
	}

	// Sopra la media per Units Sold (stesso gruppo di regole appena
	// completato): la soglia e' la MEDIA aritmetica dell'intervallo,
	// ricalcolata a ogni valutazione -- non un valore fisso come la
	// regola "High" in cima a questo blocco.
	{
		ConditionalFormatRule rule;
		rule.type = eCondAboveAverage;
		rule.bgColor = (rgb_color){ 255, 235, 156, 255 }; // FFEB9C, giallo Excel
		rule.ranges.push_back(range(5, 2, 5, 701)); // E2:E701 (Units Sold)
		dati->AddConditionalFormatRule(rule);
	}

	// ==================== Foglio "Pivot" ====================
	CContainer* pivot = new CContainer(NULL, NULL);

	WriteLabel(pivot, cell(1, 1), "Category analysis - Financial Sample (700 records, live formulas)");
	pivot->AddMergedRange(range(1, 1, 11, 1));
	Style(pivot, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kBlue; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});
	Bold(pivot, cell(1, 1));

	struct CategoryBlock {
		const char* label; int destCol; const std::vector<BString>* categories;
		char critCol, valCol; bool currency, integer;
	};
	CategoryBlock blocks[] = {
		{ "By segment (sum of sales)", 1, &segments, 'A', 'J', true, false },
		{ "By country (sum of profit)", 4, &countries, 'B', 'L', true, false },
		{ "By product (sum of units sold)", 7, &products, 'C', 'E', false, true },
		// Per mese (colonna O, "Month Name"): in ordine calendario, non
		// alfabetico (vedi il vettore "months" sopra) -- serve al
		// grafico a LINEE piu' sotto, l'unico tipo di grafico ancora
		// mai usato in questo file (barre e torta gia' presenti).
		{ "By month (sum of sales)", 10, &months, 'O', 'J', true, false },
	};

	for (int b = 0; b < 4; b++)
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

	// Dati per il grafico COMBINATO (Fase 35): stesse categorie del
	// blocco "By segment" sopra (colonna 1-2), ma con una SECONDA
	// colonna serie (Profitto) accanto -- serve una tabella con due
	// colonne di valori NUMERICHE oltre alle etichette per un grafico
	// combinato barre+linee, che WriteLiveCategoryTable (una sola
	// colonna valore) non produce da sola. Colonne 13-15, lontane da
	// tutte le tabelle sopra apposta per non serve nessuna colonna di
	// margine extra.
	int comboLabelRow = 3;
	WriteLabel(pivot, cell(13, comboLabelRow), "Segment");
	WriteLabel(pivot, cell(14, comboLabelRow), "Sales");
	WriteLabel(pivot, cell(15, comboLabelRow), "Profit");
	for (size_t i = 0; i < segments.size(); i++)
	{
		int row = comboLabelRow + 1 + (int)i;
		WriteLabel(pivot, cell(13, row), segments[i].String());

		char salesFormula[128];
		snprintf(salesFormula, sizeof(salesFormula), "=SUMIF(Data!A2:A701;\"%s\";Data!J2:J701)",
			segments[i].String());
		TryToParseString(salesFormula, cell(14, row), pivot, true);
		Currency(pivot, cell(14, row));

		char profitFormula[128];
		snprintf(profitFormula, sizeof(profitFormula), "=SUMIF(Data!A2:A701;\"%s\";Data!L2:L701)",
			segments[i].String());
		TryToParseString(profitFormula, cell(15, row), pivot, true);
		Currency(pivot, cell(15, row));
	}

	// Dati per il grafico a DISPERSIONE (Fase 35): un grafico XY vuole
	// due colonne NUMERICHE senza etichette -- qui X e Y sono entrambe
	// derivate dal vivo dal dataset reale (Unita' vendute e Profitto,
	// sommate per prodotto), non valori inventati, per mostrare una
	// correlazione vera. Colonne 17-18, stesso principio di isolamento
	// del blocco combinato sopra.
	int scatterFirstRow = 3;
	for (size_t i = 0; i < products.size(); i++)
	{
		int row = scatterFirstRow + (int)i;

		char unitsFormula[128];
		snprintf(unitsFormula, sizeof(unitsFormula), "=SUMIF(Data!C2:C701;\"%s\";Data!E2:E701)",
			products[i].String());
		TryToParseString(unitsFormula, cell(17, row), pivot, true);
		Integer(pivot, cell(17, row));

		char profitFormula[128];
		snprintf(profitFormula, sizeof(profitFormula), "=SUMIF(Data!C2:C701;\"%s\";Data!L2:L701)",
			products[i].String());
		TryToParseString(profitFormula, cell(18, row), pivot, true);
		Currency(pivot, cell(18, row));
	}

	// Sette grafici incorporati (barre, torta, barre, linee, area,
	// combinato, dispersione -- tutti i tipi disponibili): dati letti
	// dal vivo dalle formule appena scritte sopra -- il grafico si
	// aggiorna da solo ogni volta che il documento viene ricalcolato,
	// non serve rigenerare il file. ChartObject::frame condivide lo
	// STESSO spazio di coordinate delle celle (SheetView::DrawCharts lo
	// disegna senza nessun offset in piu'), quindi "left" DEVE essere
	// >= SheetView::kHeaderWidth (30px, la larghezza della colonna dei
	// numeri di riga) -- un frame con left < 30 finisce visivamente
	// sopra quella colonna invece che a fianco. Bug reale (2026-09-26,
	// segnalato dall'utente aprendo il file vero): ogni grafico qui
	// sotto partiva da left=20, dentro la colonna dei numeri di riga.
	ChartObject chartSeg;
	chartSeg.type = eBarChart;
	chartSeg.title = "Sales by segment";
	chartSeg.dataRange = range(1, 4, 2, 3 + (int)segments.size());
	chartSeg.frame = BRect(40, 230, 380, 430);

	ChartObject chartCountry;
	chartCountry.type = ePieChart;
	chartCountry.title = "Profit by country";
	chartCountry.dataRange = range(4, 4, 5, 3 + (int)countries.size());
	chartCountry.frame = BRect(400, 230, 700, 430);

	ChartObject chartProduct;
	chartProduct.type = eBarChart;
	chartProduct.title = "Units sold by product";
	chartProduct.dataRange = range(7, 4, 8, 3 + (int)products.size());
	chartProduct.frame = BRect(40, 450, 500, 650);

	// Grafico a LINEE (eLineChart, l'unico dei tre tipi di grafico
	// ancora mai usato in questo file): l'andamento mensile si presta
	// meglio a una linea che a barre separate, esattamente come si
	// sceglierebbe in Excel per un trend nel tempo.
	ChartObject chartMonth;
	chartMonth.type = eLineChart;
	chartMonth.title = "Sales trend by month";
	chartMonth.dataRange = range(10, 4, 11, 3 + (int)months.size());
	chartMonth.frame = BRect(520, 450, 900, 650);

	// Grafico ad AREA (Fase 35): stessi dati mensili del grafico a
	// linee sopra, riuso deliberato -- il tipo Area e' pensato proprio
	// per lo stesso caso d'uso (un andamento nel tempo), la differenza
	// e' solo visiva (area riempita sotto la linea).
	// top=340 (non 230 come chartSeg/chartCountry sulla stessa riga di
	// grafici): a differenza di quei due blocchi (5 righe dati, finiscono
	// entro la riga 8), il blocco "Per mese" qui accanto (colonne 10-11)
	// ha 12 righe dati (fino alla riga 15, dicembre) -- un top=230
	// (giusto per le tabelle piu' corte) copriva davvero Settembre-
	// Dicembre. Bug reale, pre-esistente (mai toccato in questa sessione
	// prima d'ora), screenshot dell'utente, 2026-09-26. bottom=440 (non
	// 430) per restare dentro lo spazio libero fra la riga 15 (fine
	// tabella, y=310) e la riga di grafici sotto (chartProduct/chartMonth/
	// ecc., y=450) -- un'altezza piu' bassa degli altri grafici (100px
	// invece di 200), ma l'unico spazio davvero libero senza spostare
	// anche tutto il resto della griglia di grafici qui sotto.
	ChartObject chartMonthArea;
	chartMonthArea.type = eAreaChart;
	chartMonthArea.title = "Sales trend by month (area)";
	chartMonthArea.dataRange = chartMonth.dataRange;
	chartMonthArea.frame = BRect(920, 340, 1300, 440);

	// Grafico COMBINATO (Fase 35): Vendite come barre, Profitto come
	// linea, stessa scala e stesso asse categorie (vedi il commento su
	// DrawComboChart in Chart.cpp) -- un caso reale in cui vedere
	// entrambe le grandezze insieme, per segmento, ha senso (un
	// segmento puo' vendere molto ma avere un profitto basso).
	ChartObject chartSegCombo;
	chartSegCombo.type = eComboChart;
	chartSegCombo.title = "Sales (bars) and profit (line) by segment";
	chartSegCombo.dataRange = range(13, comboLabelRow, 15, comboLabelRow + (int)segments.size());
	chartSegCombo.frame = BRect(1320, 230, 1700, 430);

	// Grafico a DISPERSIONE (Fase 35): la stessa correlazione unita'
	// vendute/profitto per prodotto usata sopra per costruire i dati,
	// qui visualizzata come punti invece che come due barre separate --
	// esattamente il caso d'uso di un grafico XY (cercare una relazione
	// fra due grandezze), non un trend nel tempo ne' un confronto per
	// categoria.
	ChartObject chartProductScatter;
	chartProductScatter.type = eScatterChart;
	chartProductScatter.title = "Units sold vs profit by product";
	chartProductScatter.dataRange = range(17, scatterFirstRow, 18, scatterFirstRow + (int)products.size() - 1);
	chartProductScatter.frame = BRect(920, 450, 1300, 650);

	// Grafico a BARRE ORIZZONTALI (eHBarChart): stessi dati di
	// chartProduct sopra, riuso deliberato -- la differenza e' solo
	// l'orientamento (categorie sull'asse verticale, barre che si
	// estendono da sinistra a destra), il vero "Bar" di Excel a
	// differenza di "Column" (eBarChart).
	ChartObject chartProductHBar;
	chartProductHBar.type = eHBarChart;
	chartProductHBar.title = "Units sold by product (horizontal bars)";
	chartProductHBar.dataRange = chartProduct.dataRange;
	chartProductHBar.frame = BRect(1320, 450, 1700, 650);

	// Tabella pivot VERA (Inserisci -> Tabella Pivot, Fase 29 per il
	// raggruppamento a piu' livelli): a differenza dei quattro blocchi
	// SUMIF sopra (dal vivo, MAI congelati), questa e' deliberatamente
	// un'ISTANTANEA statica, per design -- vedi il commento in cima al
	// file. Prodotto+Fascia sconto sono le uniche due colonne di
	// categoria ADIACENTI nel dataset reale seguite da una colonna
	// numerica altrettanto adiacente (Unita' Vendute): C:E, l'unica
	// combinazione a due livelli che BuildPivotTable puo' leggere da
	// un intervallo unico senza dover prima riordinare le colonne.
	int realPivotLabelRow = 36;
	WriteLabel(pivot, cell(1, realPivotLabelRow),
		"REAL pivot table (Insert -> Pivot Table): a static snapshot, not live "
		"like the tables above -- two-level grouping, Product then Discount Band");
	pivot->AddMergedRange(range(1, realPivotLabelRow, 11, realPivotLabelRow));
	Style(pivot, cell(1, realPivotLabelRow), [&](CellStyle& cs) {
		cs.fLowColor = kOrange; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter; cs.fWrapText = true;
	});
	Bold(pivot, cell(1, realPivotLabelRow));

	int realPivotDataRow = realPivotLabelRow + 1;
	{
		std::vector<PivotRow> rows;
		if (BuildPivotTable(dati, range(3, 2, 5, 701), rows))
			WritePivotTable(pivot, cell(1, realPivotDataRow), rows, ePivotSum);
	}

	// Tabella pivot 2D VERA (campo Colonne + misure multiple -- v0.3.0,
	// vedi CHANGELOG.md): stesso intervallo sorgente adiacente C:F del
	// blocco sopra (Prodotto, Fascia sconto, Unita' vendute, Prezzo di
	// produzione), ma qui la Fascia sconto diventa il campo COLONNE
	// (i suoi valori si spargono su nuove colonne) e vengono aggregate
	// DUE misure fianco a fianco con aggregazioni diverse (Somma per le
	// unita', Media per il prezzo) -- le due capacita' nuove insieme,
	// non separate, sullo stesso dataset gia' usato per il confronto
	// col pivot 1D appena sopra.
	int realPivot2DLabelRow = realPivotLabelRow + 40;
	WriteLabel(pivot, cell(1, realPivot2DLabelRow),
		"REAL 2D pivot table (v0.3.0): Columns field (Discount Band) + two measures "
		"(Sum of units, Average manufacturing price), rows by Product");
	pivot->AddMergedRange(range(1, realPivot2DLabelRow, 11, realPivot2DLabelRow));
	Style(pivot, cell(1, realPivot2DLabelRow), [&](CellStyle& cs) {
		cs.fLowColor = kOrange; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter; cs.fWrapText = true;
	});
	Bold(pivot, cell(1, realPivot2DLabelRow));

	int realPivot2DDataRow = realPivot2DLabelRow + 1;
	{
		std::vector<PivotMeasure> measures(2);
		measures[0].sourceCol = 5; measures[0].aggFunc = ePivotSum; measures[0].label = "Units";
		measures[1].sourceCol = 6; measures[1].aggFunc = ePivotAverage; measures[1].label = "Average price";

		std::vector<BString> columnValues;
		std::vector<PivotRow2D> rows2D;
		if (BuildPivotTable2D(dati, range(3, 2, 6, 701), 4 /* Fascia sconto */, measures,
				&columnValues, &rows2D))
			WritePivotTable2D(pivot, cell(1, realPivot2DDataRow), columnValues, measures, rows2D);
	}

	// ==================== Foglio "Board Meeting" ====================
	CContainer* cda = new CContainer(NULL, NULL);

	WriteLabel(cda, cell(1, 1), "Board Meeting - Sales Summary");
	cda->AddMergedRange(range(1, 1, 4, 1));
	Style(cda, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kGreen; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});
	Bold(cda, cell(1, 1));

	WriteLabel(cda, cell(1, 2),
		"Data: Financial Sample - 700 records, every formula updates itself");
	cda->AddMergedRange(range(1, 2, 4, 2));
	Style(cda, cell(1, 2), [&](CellStyle& cs) {
		cs.fHighColor = kDarkGray; cs.fAlignment = eAlignCenter;
	});

	// Collegamento ipertestuale (Fase 13, 100% XLSX standard
	// compatibility Tier 2): un vero URL esterno, verificato -- lo
	// stesso repository che ha ricevuto ogni commit di questa sessione,
	// non un indirizzo inventato per l'occasione.
	WriteLabel(cda, cell(1, 3), "Atomo123 source code on GitHub");
	cda->AddMergedRange(range(1, 3, 4, 3));
	Style(cda, cell(1, 3), [&](CellStyle& cs) {
		cs.fHighColor = kBlue; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});
	cda->SetHyperlink(cell(1, 3), "https://github.com/atomozero/Atomo123");

	// Riga KPI: intestazioni (riga 4) + formule dal vivo sul foglio
	// "Data" (riga 5) -- lasciate deliberatamente NON calcolate qui,
	// stesso principio di generate_demo.cpp: senza un vero
	// ISheetResolver (nessuna MainWindow in questo generatore
	// headless) restano "vive" ma non risolvibili, si calcolano da
	// sole alla riapertura in Atomo123.
	const char* kpiHeaders[] = { "Total Sales", "Total Profit", "Margin %", "Units Sold" };
	for (int col = 1; col <= 4; col++)
	{
		WriteLabel(cda, cell(col, 4), kpiHeaders[col - 1]);
		Style(cda, cell(col, 4), [&](CellStyle& cs) {
			cs.fLowColor = kGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
		});
		Border(cda, cell(col, 4));
	}

	TryToParseString("=SUM(Data!J2:J701)", cell(1, 5), cda, true);
	TryToParseString("=SUM(Data!L2:L701)", cell(2, 5), cda, true);
	TryToParseString("=B5/A5", cell(3, 5), cda, true);
	TryToParseString("=SUM(Data!E2:E701)", cell(4, 5), cda, true);
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
		"Live formula (sum of the Sales column on the Data sheet): updates itself "
		"if the imported data changes, not a frozen number.");

	// Sezione "vendite per segmento", classificata per valore
	// decrescente SENZA congelare nessun numero: LARGE(...) prende il
	// k-esimo valore piu' alto dalla tabella del foglio Pivot (dal vivo,
	// formule SUMIF, mai un'istantanea), INDEX/MATCH ne risale il nome
	// del segmento -- esattamente come si farebbe in Excel per una
	// "classifica" che si aggiorna da sola quando i dati sorgente
	// cambiano.
	WriteLabel(cda, cell(1, 7), "Sales by segment (ranking, live formulas)");
	cda->AddMergedRange(range(1, 7, 3, 7));
	Style(cda, cell(1, 7), [&](CellStyle& cs) {
		cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});

	const char* rankHeaders[] = { "Segment", "Sales", "Share %" };
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
		{
			// Il primo classificato in classifica prende un bordo
			// colorato invece del solito nero (BorderColor, non Border):
			// un tocco di evidenza in piu' rispetto al semplice bordo
			// gia' usato ovunque altrove in questo file.
			if (rank == 1)
				BorderColor(cda, cell(col, row), kOrange);
			else
				Border(cda, cell(col, row));
		}
	}

	int lastRankRow = 8 + (int)segments.size();
	float chartTop = 20 + (lastRankRow + 1) * 20;
	float chartBottom = chartTop + 220;

	ChartObject chartRank;
	chartRank.type = eBarChart;
	chartRank.title = "Sales by segment";
	chartRank.dataRange = range(1, 9, 2, lastRankRow);
	// left=40, non 20: vedi il commento su SheetView::kHeaderWidth
	// (30px) accanto al primo grafico del foglio "Pivot".
	chartRank.frame = BRect(40, chartTop, 460, chartBottom);

	// Riga di nota sotto al grafico, con un margine di sicurezza (20px,
	// una riga) per non sovrapporsi al bordo inferiore del grafico:
	// SheetView::kRowHeight = 20px, riga 1 (titolo) alta 30px invece di
	// 20 (vedi rowHeights piu' sotto), quindi la riga N inizia a
	// 10 + (N-1)*20 pixel, non semplicemente N*20.
	int noteRow = (int)((chartBottom - 10) / 20) + 2;
	WriteLabel(cda, cell(1, noteRow),
		"Automatically generated by Atomo123 from Financial Sample.xlsx (Microsoft's public "
		"demo dataset). Every number in this file is a live formula: change the data on the "
		"Data sheet, and everything else updates itself on recalculation.");
	cda->AddMergedRange(range(1, noteRow, 4, noteRow));
	Style(cda, cell(1, noteRow), [&](CellStyle& cs) { cs.fHighColor = kDarkGray; cs.fWrapText = true; });

	// ==================== Foglio "Functions" ====================
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

	WriteLabel(funcs, cell(1, 1), "Function catalog - applied to real data");
	funcs->AddMergedRange(range(1, 1, 4, 1));
	Style(funcs, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kBlue; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});
	WriteLabel(funcs, cell(1, 2),
		"Every row below is a LIVE formula (Result column): filter with AutoFilter by category.");
	funcs->AddMergedRange(range(1, 2, 4, 2));
	Style(funcs, cell(1, 2), [&](CellStyle& cs) { cs.fHighColor = kDarkGray; cs.fAlignment = eAlignCenter; });

	const char* catalogHeaders[] = { "Category", "Function", "Formula", "Result" };
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
		// -- Text --
		{ "Text", "UPPER", "=UPPER(\"lowercase\")" },
		{ "Text", "LOWER", "=LOWER(\"UPPERCASE\")" },
		{ "Text", "PROPER", "=PROPER(\"first last\")" },
		{ "Text", "TRIM", "=TRIM(\"  too many spaces  \")" },
		{ "Text", "CONCAT", "=CONCAT(Data!A2;\" / \";Data!B2)" },
		{ "Text", "TEXTJOIN", "=TEXTJOIN(\", \";TRUE;Data!A2;Data!B2;Data!C2)" },
		{ "Text", "SUBSTITUTE", "=SUBSTITUTE(\"Small Business\";\"Business\";\"Biz\")" },
		{ "Text", "REPLACE", "=REPLACE(\"Atomo123\";1;5;\"New\")" },
		{ "Text", "REPT", "=REPT(\"=\";10)" },
		{ "Text", "EXACT", "=EXACT(Data!A2;Data!A3)" },
		{ "Text", "VALUE", "=VALUE(\"1234.5\")" },
		{ "Text", "TEXT", "=TEXT(Data!J2;\"0.00\")" },
		// -- Date --
		{ "Date", "TODAY", "=TODAY()" },
		{ "Date", "YEAR", "=YEAR(Data!M2)" },
		{ "Date", "MONTH", "=MONTH(Data!M2)" },
		{ "Date", "DAY", "=DAY(Data!M2)" },
		{ "Date", "EDATE", "=EDATE(Data!M2;3)" },
		{ "Date", "EOMONTH", "=EOMONTH(Data!M2;0)" },
		{ "Date", "NETWORKDAYS", "=NETWORKDAYS(DATE(2013;1;1);DATE(2013;12;31))" },
		{ "Date", "WORKDAY", "=WORKDAY(DATE(2026;1;1);10)" },
		{ "Date", "DATEDIF", "=DATEDIF(Data!M2;TODAY();\"Y\")" },
		// -- Logic --
		{ "Logic", "IF", "=IF(Data!J2>100000;\"High\";\"Low\")" },
		{ "Logic", "AND", "=AND(Data!E2>0;Data!J2>0)" },
		{ "Logic", "OR", "=OR(Data!D2=\"High\";Data!D2=\"Low\")" },
		{ "Logic", "NOT", "=NOT(Data!D2=\"None\")" },
		{ "Logic", "XOR", "=XOR(Data!E2>1000;Data!J2>500000)" },
		{ "Logic", "SWITCH", "=SWITCH(Data!D2;\"None\";\"N\";\"Low\";\"L\";\"Medium\";\"M\";\"High\";\"H\")" },
		{ "Logic", "IFERROR", "=IFERROR(1/0;\"Error avoided\")" },
		{ "Logic", "IFNA", "=IFNA(XMATCH(\"DoesNotExist\";Data!A2:A10);\"Not found\")" },
		{ "Logic", "ISBLANK", "=ISBLANK(Q1)" },
		{ "Logic", "ISFORMULA", "=ISFORMULA(Data!J2:J3)" }, // intervallo di ALMENO due celle: vedi il limite noto in Functions.logical.cpp
		// -- Lookup --
		{ "Lookup", "VLOOKUP", "=VLOOKUP(\"Government\";Data!A2:J701;10;0)" },
		{ "Lookup", "INDEX+MATCH", "=INDEX(Data!J2:J701;MATCH(\"Government\";Data!A2:A701;0))" },
		{ "Lookup", "XLOOKUP", "=XLOOKUP(\"Government\";Data!A2:A701;Data!J2:J701)" },
		{ "Lookup", "XMATCH", "=XMATCH(\"Government\";Data!A2:A701)" },
		{ "Lookup", "INDIRECT", "=INDIRECT(\"Data!J2\")" },
		{ "Lookup", "ADDRESS", "=ADDRESS(2;10)" },
		{ "Lookup", "Table[Col] + SUM", "=SUM(SalesTable[Sales])" },
		{ "Lookup", "Table[Col] + INDEX/MATCH", "=INDEX(SalesTable[Profit];MATCH(\"Government\";SalesTable[Segment];0))" },
		// -- Math and statistics --
		{ "Math", "SUMPRODUCT", "=SUMPRODUCT(Data!E2:E11;Data!G2:G11)" },
		{ "Math", "AVERAGEIFS", "=AVERAGEIFS(Data!J2:J701;Data!A2:A701;\"Government\";Data!B2:B701;\"Canada\")" },
		{ "Math", "MAXIFS", "=MAXIFS(Data!J2:J701;Data!A2:A701;\"Government\")" },
		{ "Math", "MINIFS", "=MINIFS(Data!J2:J701;Data!A2:A701;\"Government\")" },
		{ "Math", "RANK", "=RANK(Data!J2;Data!J2:J701)" },
		{ "Math", "LARGE", "=LARGE(Data!J2:J701;1)" },
		{ "Math", "SMALL", "=SMALL(Data!J2:J701;1)" },
		{ "Math", "SUBTOTAL", "=SUBTOTAL(9;Data!J2:J701)" },
		{ "Math", "MEDIAN", "=MEDIAN(Data!J2:J701)" },
		{ "Math", "STDDEV", "=STDDEV(Data!J2:J701)" },
		{ "Math", "ROUND", "=ROUND(Data!J2;0)" },
		{ "Math", "COUNTIF", "=COUNTIF(Data!A2:A701;\"Government\")" },
		// -- Finance --
		{ "Finance", "RATE", "=RATE(8;-150;1000)" }, // tasso periodico di un prestito di 1000 restituito in 8 rate da 150
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
	funcs->NewCell(cell(1, sequenceLabelRow), Value("SEQUENCE (block/\"spill\" formula)"), NULL);
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
	WriteLabel(funcs, cell(1, interactiveLabelRow), "Interactive lookup (Data validation + SUMIF)");
	funcs->AddMergedRange(range(1, interactiveLabelRow, 4, interactiveLabelRow));
	Style(funcs, cell(1, interactiveLabelRow), [&](CellStyle& cs) {
		cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});

	int pickerRow = interactiveLabelRow + 1;
	WriteLabel(funcs, cell(1, pickerRow), "Choose a segment:");
	WriteLabel(funcs, cell(2, pickerRow), "Government"); // valore iniziale della cella con l'elenco
	Style(funcs, cell(2, pickerRow), [&](CellStyle& cs) { cs.fLowColor = kYellow; });
	Border(funcs, cell(2, pickerRow));
	{
		ValidationRule rule;
		rule.type = eListValidation;
		rule.list = "Small Business,Midmarket,Enterprise,Government,Channel Partners";
		funcs->SetValidation(cell(2, pickerRow), rule);
	}

	// Intervallo con nome (Fase 7, "Formule -> Definisci nome"): la
	// stessa cella di sopra, usata SOTTO al posto di un riferimento
	// grezzo ("B12") -- una formula con un nome al posto di un
	// indirizzo resta leggibile anche se la riga si sposta piu' avanti
	// nel file (com'e' gia' successo piu' volte scrivendo questo
	// generatore). I nomi in questo motore sono per FOGLIO (vedi
	// CContainer::GetOrCreateNameTable), non per l'intera cartella:
	// valido solo dentro "funcs", esattamente come qui sotto.
	(*funcs->GetOrCreateNameTable())["SelectedSegment"] = range(2, pickerRow, 2, pickerRow);

	int resultRow = pickerRow + 1;
	WriteLabel(funcs, cell(1, resultRow), "Sales for that segment:");
	TryToParseString("=SUMIF(Data!A2:A701;SelectedSegment;Data!J2:J701)", cell(2, resultRow), funcs, true);
	Currency(funcs, cell(2, resultRow));
	Border(funcs, cell(2, resultRow));
	funcs->SetComment(cell(2, pickerRow),
		"Data validation: right-click the cell to see the dropdown list "
		"(Small Business/Midmarket/Enterprise/Government/Channel Partners). Changing the choice "
		"makes the formula below (which uses the name \"SelectedSegment\", not the address B12) "
		"recalculate itself.");

	// Convalida dati a intervallo numerico (Fase 13, il secondo dei due
	// tipi che questo motore modella, l'altro e' l'elenco sopra): un
	// valore fuori da [0, 100] viene rifiutato dalla UI al momento
	// dell'inserimento, non solo segnalato dopo.
	int rangeValidationRow = resultRow + 2;
	WriteLabel(funcs, cell(1, rangeValidationRow), "Hypothetical discount (0-100):");
	TryToParseString("15", cell(2, rangeValidationRow), funcs, true);
	Style(funcs, cell(2, rangeValidationRow), [&](CellStyle& cs) { cs.fLowColor = kYellow; });
	Border(funcs, cell(2, rangeValidationRow));
	{
		ValidationRule rule;
		rule.type = eNumberRangeValidation;
		rule.min = 0;
		rule.max = 100;
		funcs->SetValidation(cell(2, rangeValidationRow), rule);
	}
	funcs->SetComment(cell(2, rangeValidationRow),
		"Numeric range data validation (0-100): a value outside this range is rejected "
		"immediately, not just flagged afterward.");

	// Formattazione condizionale: valori duplicati (Fase 13, il tipo
	// che manca ancora in questo file -- "Discount Band = High" sul
	// foglio Data usa eCondCellIsEqual, la scala di colori sulla
	// colonna Profit usa eCondColorScale). Un piccolo elenco costruito
	// apposta (non i 700 record reali, dove quasi ogni valore
	// categoriale si ripete comunque) rende il confronto leggibile a
	// colpo d'occhio: due codici identici, quattro diversi.
	int dupLabelRow = rangeValidationRow + 3;
	WriteLabel(funcs, cell(1, dupLabelRow), "Conditional formatting: duplicate values (example)");
	funcs->AddMergedRange(range(1, dupLabelRow, 4, dupLabelRow));
	Style(funcs, cell(1, dupLabelRow), [&](CellStyle& cs) {
		cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});
	int dupFirstRow = dupLabelRow + 1;
	const char* dupCodes[] = { "ATC-001", "ATC-002", "ATC-003", "ATC-002", "ATC-004", "ATC-005" };
	for (size_t i = 0; i < sizeof(dupCodes) / sizeof(dupCodes[0]); i++)
	{
		WriteLabel(funcs, cell(1, dupFirstRow + (int)i), dupCodes[i]);
		Border(funcs, cell(1, dupFirstRow + (int)i));
	}
	int dupLastRow = dupFirstRow + (int)(sizeof(dupCodes) / sizeof(dupCodes[0])) - 1;
	{
		ConditionalFormatRule rule;
		rule.type = eCondDuplicateValues;
		rule.bgColor = (rgb_color){ 255, 235, 156, 255 }; // FFEB9C, stesso giallo di Excel
		rule.ranges.push_back(range(1, dupFirstRow, 1, dupLastRow));
		funcs->AddConditionalFormatRule(rule);
	}

	// Array dinamici oltre SEQUENCE (Fase 34, "Path to full Excel
	// parity" Tier 2): UNIQUE sui dati reali (i 5 segmenti distinti
	// della colonna Segment, 700 righe, formula dal vivo -- si aggiorna
	// da solo se cambia il dataset importato), SORT/SORTBY/FILTER su
	// una piccola tabella costruita apposta (una tabella reale a 700
	// righe spillerebbe altrettante righe nel foglio, inutile da
	// mostrare qui, stesso principio dell'esempio duplicati sopra).
	int arrayLabelRow = dupLastRow + 3;
	WriteLabel(funcs, cell(1, arrayLabelRow), "Dynamic arrays (UNIQUE, SORT, SORTBY, FILTER)");
	funcs->AddMergedRange(range(1, arrayLabelRow, 4, arrayLabelRow));
	Style(funcs, cell(1, arrayLabelRow), [&](CellStyle& cs) {
		cs.fLowColor = kLightGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
	});

	int uniqueRow = arrayLabelRow + 1;
	WriteLabel(funcs, cell(1, uniqueRow), "=UNIQUE(Data!A2:A701)");
	TryToParseString("=UNIQUE(Data!A2:A701)", cell(2, uniqueRow), funcs, true);
	funcs->SetComment(cell(2, uniqueRow),
		"UNIQUE on the 700 real records (Segment column): spills the 5 distinct segments, "
		"updates itself if the imported dataset changes.");

	int arrayTableLabelRow = uniqueRow + 7; // sotto ai 5 segmenti spillati, piu' margine
	WriteLabel(funcs, cell(1, arrayTableLabelRow),
		"SORT/SORTBY/FILTER on a small sample table (Name, Region, Sales)");
	funcs->AddMergedRange(range(1, arrayTableLabelRow, 4, arrayTableLabelRow));
	Style(funcs, cell(1, arrayTableLabelRow), [&](CellStyle& cs) { cs.fHighColor = kDarkGray; });

	int sampleFirstRow = arrayTableLabelRow + 1;
	struct SampleRow { const char* name; const char* region; double sales; };
	const SampleRow sampleRows[] = {
		{ "Anna", "North", 300 },
		{ "Bruce", "South", 100 },
		{ "Clara", "North", 200 },
		{ "David", "Central", 400 },
		{ "Ellen", "South", 150 },
	};
	for (size_t i = 0; i < sizeof(sampleRows) / sizeof(sampleRows[0]); i++)
	{
		int row = sampleFirstRow + (int)i;
		WriteLabel(funcs, cell(1, row), sampleRows[i].name);
		WriteLabel(funcs, cell(2, row), sampleRows[i].region);
		TryToParseString(BString() << sampleRows[i].sales, cell(3, row), funcs, true);
		Integer(funcs, cell(3, row));
		for (int col = 1; col <= 3; col++)
			Border(funcs, cell(col, row));
	}
	int sampleLastRow = sampleFirstRow + (int)(sizeof(sampleRows) / sizeof(sampleRows[0])) - 1;

	char sortFormula[64];
	snprintf(sortFormula, sizeof(sortFormula), "=SORT(A%d:C%d;3;-1)", sampleFirstRow, sampleLastRow);
	WriteLabel(funcs, cell(5, sampleFirstRow - 1), "SORT (by Sales, descending)");
	TryToParseString(sortFormula, cell(5, sampleFirstRow), funcs, true);

	char sortByFormula[64];
	snprintf(sortByFormula, sizeof(sortByFormula), "=SORTBY(A%d:A%d;C%d:C%d;-1)",
		sampleFirstRow, sampleLastRow, sampleFirstRow, sampleLastRow);
	WriteLabel(funcs, cell(8, sampleFirstRow - 1), "SORTBY (names by Sales)");
	TryToParseString(sortByFormula, cell(8, sampleFirstRow), funcs, true);

	char filterFormula[80];
	snprintf(filterFormula, sizeof(filterFormula), "=FILTER(A%d:C%d;B%d:B%d=\"North\";\"none\")",
		sampleFirstRow, sampleLastRow, sampleFirstRow, sampleLastRow);
	WriteLabel(funcs, cell(10, sampleFirstRow - 1), "FILTER (only Region=\"North\")");
	TryToParseString(filterFormula, cell(10, sampleFirstRow), funcs, true);
	funcs->SetComment(cell(10, sampleFirstRow),
		"FILTER with a live-calculated condition (B:B=\"North\"), the most common real case: "
		"change the Region of a row above, and the filtered result updates itself.");

	// ==================== Foglio "Languages" ====================
	// Nuovo (richiesta esplicita dell'utente, 2026-09-26): una tabella +
	// un grafico in otto lingue non inglesi (giapponese, coreano,
	// cinese, arabo, greco, russo, indonesiano, friulano) -- verifica
	// pratica che celle E grafici gestiscano davvero testo Unicode non
	// latino (CJK, arabo RTL, cirillico, greco), non solo gli accenti
	// latini gia' usati altrove in questo file (es. "e'" scritto per
	// esteso invece di un apostrofo, per compatibilita' con vecchi
	// strumenti -- qui invece testo Unicode vero, un carattere per
	// codepoint, dato che il motore/il Translation Kit di Haiku
	// lavorano in UTF-8 nativamente). NewCell/WriteLabel diretto (mai
	// TryToParseString), stesso motivo di ogni altra etichetta di solo
	// testo in questo file.
	CContainer* languages = new CContainer(NULL, NULL);

	WriteLabel(languages, cell(1, 1), "Multilingual showcase - text and charts in non-English languages");
	languages->AddMergedRange(range(1, 1, 5, 1));
	Style(languages, cell(1, 1), [&](CellStyle& cs) {
		cs.fLowColor = kBlue; cs.fHighColor = kWhite; cs.fAlignment = eAlignCenter;
	});
	Bold(languages, cell(1, 1));

	WriteLabel(languages, cell(1, 2),
		"Native name, greeting, and welcome message in 8 languages (speakers = approximate, millions)");
	languages->AddMergedRange(range(1, 2, 5, 2));
	Style(languages, cell(1, 2), [&](CellStyle& cs) { cs.fHighColor = kDarkGray; cs.fAlignment = eAlignCenter; });

	// Colonne 4-5 (Nome nativo, Parlanti) ADIACENTI apposta: un grafico
	// vuole etichetta+valore in due colonne una accanto all'altra,
	// stesso principio di ogni altro ChartObject::dataRange in questo
	// file -- Nome nativo qui, non Language (colonna 1, sempre inglese),
	// e' la scelta deliberata: e' l'asse categorie del grafico piu'
	// sotto che deve mostrare i veri glifi non latini, non una comoda
	// traslitterazione.
	const char* langHeaders[] = { "Language", "Greeting", "Welcome message", "Native name", "Speakers (M)" };
	int langHeaderRow = 4;
	for (int col = 1; col <= 5; col++)
	{
		WriteLabel(languages, cell(col, langHeaderRow), langHeaders[col - 1]);
		Style(languages, cell(col, langHeaderRow), [&](CellStyle& cs) {
			cs.fLowColor = kGray; cs.fAlignment = eAlignCenter; cs.fUnderline = true;
		});
		Border(languages, cell(col, langHeaderRow));
	}

	// Saluto = "hello"/"hi", messaggio di benvenuto = "Welcome to
	// Atomo123" tradotto -- traduzioni di buon senso per una frase
	// standard, non verificate da un madrelingua per ognuna delle otto
	// lingue: corrette quanto basta per una dimostrazione, non un testo
	// destinato alla pubblicazione. Parlanti nativi: stime approssimate
	// arrotondate, solo per dare al grafico piu' sotto numeri reali (non
	// inventati) invece che valori a caso -- non una cifra rigorosamente
	// documentata (vedi la nota nel foglio stesso).
	struct LanguageRow {
		const char* language; const char* greeting; const char* welcome;
		const char* nativeName; double speakersMillions;
	};
	const LanguageRow langRows[] = {
		{ "Japanese", "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf",
			"Atomo123\xe3\x81\xb8\xe3\x82\x88\xe3\x81\x86\xe3\x81\x93\xe3\x81\x9d",
			"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", 123 },
		{ "Korean", "\xec\x95\x88\xeb\x85\x95\xed\x95\x98\xec\x84\xb8\xec\x9a\x94",
			"Atomo123\xec\x97\x90 \xec\x98\xa4\xec\x8b\xa0 \xea\xb2\x83\xec\x9d\x84 "
			"\xed\x99\x98\xec\x98\x81\xed\x95\xa9\xeb\x8b\x88\xeb\x8b\xa4",
			"\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4", 82 },
		{ "Chinese", "\xe4\xbd\xa0\xe5\xa5\xbd", "\xe6\xac\xa2\xe8\xbf\x8e\xe4\xbd\xbf\xe7\x94\xa8 Atomo123",
			"\xe4\xb8\xad\xe6\x96\x87", 920 },
		{ "Arabic", "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7",
			"\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd9\x8b\xd8\xa7 \xd8\xa8\xd9\x83 \xd9\x81\xd9\x8a Atomo123",
			"\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9", 274 },
		{ "Greek", "\xce\x93\xce\xb5\xce\xb9\xce\xac \xcf\x83\xce\xbf\xcf\x85",
			"\xce\x9a\xce\xb1\xce\xbb\xcf\x8e\xcf\x82 \xce\xae\xcf\x81\xce\xb8\xce\xb1\xcf\x84\xce\xb5 "
			"\xcf\x83\xcf\x84\xce\xbf Atomo123",
			"\xce\x95\xce\xbb\xce\xbb\xce\xb7\xce\xbd\xce\xb9\xce\xba\xce\xac", 13 },
		{ "Russian", "\xd0\x97\xd0\xb4\xd1\x80\xd0\xb0\xd0\xb2\xd1\x81\xd1\x82\xd0\xb2\xd1\x83\xd0\xb9\xd1\x82\xd0\xb5",
			"\xd0\x94\xd0\xbe\xd0\xb1\xd1\x80\xd0\xbe \xd0\xbf\xd0\xbe\xd0\xb6\xd0\xb0\xd0\xbb\xd0\xbe\xd0\xb2\xd0\xb0\xd1\x82\xd1\x8c "
			"\xd0\xb2 Atomo123",
			"\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9", 150 },
		{ "Indonesian", "Halo", "Selamat datang di Atomo123", "Bahasa Indonesia", 43 },
		{ "Friulian", "Mandi", "Benvign\xc3\xbbts in Atomo123", "Furlan", 0.6 },
	};
	int langDataFirstRow = langHeaderRow + 1;
	for (size_t i = 0; i < sizeof(langRows) / sizeof(langRows[0]); i++)
	{
		int row = langDataFirstRow + (int)i;
		WriteLabel(languages, cell(1, row), langRows[i].language);
		WriteLabel(languages, cell(2, row), langRows[i].greeting);
		WriteLabel(languages, cell(3, row), langRows[i].welcome);
		WriteLabel(languages, cell(4, row), langRows[i].nativeName);
		TryToParseString(BString() << langRows[i].speakersMillions, cell(5, row), languages, true);
		for (int col = 1; col <= 5; col++)
			Border(languages, cell(col, row));
	}
	int langDataLastRow = langDataFirstRow + (int)(sizeof(langRows) / sizeof(langRows[0])) - 1;

	// Nota RTL sulla riga araba (Fase "Languages"): l'arabo si scrive da
	// destra a sinistra -- se questa cella viene VISUALIZZATA davvero
	// da destra a sinistra dipende dal supporto RTL del Locale/testo di
	// Haiku sottostante, non da questo generatore (che scrive solo byte
	// UTF-8 in ordine logico, come qualunque altra stringa di questo
	// file).
	{
		int arabicRow = langDataFirstRow + 3; // quarta lingua nell'elenco sopra
		languages->SetComment(cell(4, arabicRow),
			"Arabic is written right-to-left. Whether this cell actually DISPLAYS "
			"right-to-left depends on the underlying Haiku locale/text-rendering support, "
			"not on this generator (which only writes UTF-8 bytes in logical order, like "
			"every other string in this file).");
	}

	ChartObject chartLanguages;
	chartLanguages.type = eBarChart;
	chartLanguages.title = "Approximate native speakers by language (millions)";
	chartLanguages.dataRange = range(4, langDataFirstRow, 5, langDataLastRow);
	// STESSA identica formula di "chartTop"/"lastRankRow" sul foglio
	// "Board Meeting" sopra (mai "10 + (N-1)*20" da solo: quella versione
	// e' stata provata qui e produceva un grafico che iniziava DENTRO
	// l'ultima riga dati invece che sotto -- bug reale, screenshot
	// dell'utente, 2026-09-26 -- il -10 mancava del margine di
	// sicurezza che la versione "Board Meeting" include apposta).
	float langChartTop = 20 + (langDataLastRow + 1) * 20;
	float langChartBottom = langChartTop + 220;
	// left=40, non 20: vedi il commento su SheetView::kHeaderWidth
	// (30px) accanto al primo grafico del foglio "Pivot".
	chartLanguages.frame = BRect(40, langChartTop, 500, langChartBottom);

	int langNoteRow = (int)((langChartBottom - 10) / 20) + 2;
	WriteLabel(languages, cell(1, langNoteRow),
		"Native-speaker counts are rough, rounded estimates for demonstration purposes only -- "
		"not a rigorously sourced figure. Greetings/welcome messages are best-effort standard "
		"phrases, not reviewed by a native speaker for each of the 8 languages.");
	languages->AddMergedRange(range(1, langNoteRow, 5, langNoteRow));
	Style(languages, cell(1, langNoteRow), [&](CellStyle& cs) { cs.fHighColor = kDarkGray; cs.fWrapText = true; });

	// ==================== Cartella di lavoro ====================
	AscdSheet cdaSheet;
	cdaSheet.name = "Board Meeting";
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

	// Immagine incorporata (Fase 12): un piccolo logo generato al volo
	// (vedi MakeLogoPng sopra), ancorato subito a destra del titolo con
	// uno scarto in pixel -- segue la cella se righe/colonne cambiano
	// dimensione, esattamente come un logo importato da un vero file
	// XLSX (xl/drawings+xl/media).
	{
		EmbeddedImage logo;
		std::vector<uint8> pngBytes;
		if (MakeLogoPng(pngBytes))
		{
			logo.anchor = cell(4, 1);
			logo.offsetX = 40; logo.offsetY = -5;
			logo.width = 40; logo.height = 40;
			logo.pngData = pngBytes;
			cdaSheet.images.push_back(logo);
		}
	}

	AscdSheet pivotSheet;
	pivotSheet.name = "Pivot";
	pivotSheet.doc = pivot;
	pivotSheet.charts.push_back(chartSeg);
	pivotSheet.charts.push_back(chartCountry);
	pivotSheet.charts.push_back(chartProduct);
	pivotSheet.charts.push_back(chartMonth);
	pivotSheet.charts.push_back(chartMonthArea);
	pivotSheet.charts.push_back(chartSegCombo);
	pivotSheet.charts.push_back(chartProductScatter);
	pivotSheet.charts.push_back(chartProductHBar);
	pivotSheet.colWidths.push_back(std::make_pair(1, 130.0f));
	pivotSheet.colWidths.push_back(std::make_pair(4, 130.0f));
	pivotSheet.colWidths.push_back(std::make_pair(7, 130.0f));
	pivotSheet.colWidths.push_back(std::make_pair(10, 130.0f));
	pivotSheet.rowHeights.push_back(std::make_pair(1, 30.0f));
	pivotSheet.hasTabColor = true;
	pivotSheet.tabColor = kOrange;

	AscdSheet funcsSheet;
	funcsSheet.name = "Functions";
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
	// Protezione foglio (Fase 32, "Proteggi foglio"): OGNI cella e'
	// bloccata per default (vedi il costruttore di CellStyle) finche'
	// non la si sblocca esplicitamente -- le due celle interattive
	// (l'elenco a discesa e l'input a intervallo numerico) restano
	// modificabili anche a foglio protetto, esattamente come un utente
	// vero farebbe per un pannello di controllo che il resto del foglio
	// non deve permettere di alterare per sbaglio.
	funcsSheet.isProtected = true;
	Style(funcs, cell(2, pickerRow), [](CellStyle& cs) { cs.fLocked = false; });
	Style(funcs, cell(2, rangeValidationRow), [](CellStyle& cs) { cs.fLocked = false; });

	// Blocca riquadri per RIGA (Functions, sopra) e per COLONNA (Data,
	// qui): la prima colonna (Segment) resta visibile scorrendo verso
	// destra fra le 16 colonne del dataset reale, non solo l'intestazione
	// scorrendo verso il basso.
	imported[0].frozenCols = 1;

	AscdSheet languagesSheet;
	languagesSheet.name = "Languages";
	languagesSheet.doc = languages;
	languagesSheet.charts.push_back(chartLanguages);
	languagesSheet.colWidths.push_back(std::make_pair(1, 100.0f));
	languagesSheet.colWidths.push_back(std::make_pair(2, 160.0f));
	languagesSheet.colWidths.push_back(std::make_pair(3, 260.0f));
	languagesSheet.colWidths.push_back(std::make_pair(4, 130.0f));
	languagesSheet.colWidths.push_back(std::make_pair(5, 100.0f));
	languagesSheet.rowHeights.push_back(std::make_pair(1, 30.0f));
	languagesSheet.hasTabColor = true;
	languagesSheet.tabColor = (rgb_color){ 0, 150, 150, 255 };

	std::vector<AscdSheet> sheets;
	sheets.push_back(cdaSheet);   // foglio attivo all'apertura
	sheets.push_back(pivotSheet);
	sheets.push_back(funcsSheet);
	sheets.push_back(imported[0]); // "Data", con i dati reali importati
	sheets.push_back(languagesSheet);

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
	languages->Release();

	if (err != B_OK)
	{
		fprintf(stderr, "SaveASCDBook fallita (errore %d)\n", (int)err);
		return 1;
	}

	printf("Creato %s con %zu fogli (tutte le tabelle sono formule dal vivo).\n",
		outPath, sheets.size());
	return 0;
}
