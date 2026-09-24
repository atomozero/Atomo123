/*
	test_ascd_io.cpp

	Test di round-trip di AscdIO (SaveASCD/LoadASCD), la logica usata
	da MainWindow per "Salva con nome"/apertura di file .ascd nativi.
	Non passa dalla vera finestra (BFilePanel, menu) -- verifica solo
	che le funzioni di lettura/scrittura siano l'una l'inversa
	dell'altra, con formule, numeri e testo.

	Serve comunque un BApplication (senza mostrare nessuna finestra):
	GetCellFormula su una formula con una costante numerica passa da
	CFormatter::FormatValue, che per l'allineamento decimale chiede la
	larghezza in pixel del testo al font (BFont::StringWidth) -- una
	chiamata che senza un BApplication registrato resta bloccata in
	attesa di una risposta dall'app_server che non arrivera' mai. Bug
	scoperto durante lo sviluppo dell'export ODS: un test con una
	formula tipo "=A1+10" restava appeso qui, non nella logica di
	salvataggio/ricalcolo che si stava effettivamente verificando.
*/

#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

#include <Application.h>
#include <DataIO.h>
#include <File.h>

#include "AscdIO.h"
#include "Cell.h"
#include "CellStyle.h"
#include "Chart.h"
#include "Pivot.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"

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

int main()
{
	BApplication app("application/x-vnd.Atomo-TestAscdIO");

	CContainer& doc = *new CContainer(NULL, NULL);

	cell a1(1, 1), b1(2, 1), c1(3, 1), d1(4, 1);
	TryToParseString("10", a1, &doc, true);
	TryToParseString("20", b1, &doc, true);
	TryToParseString("=A1+B1", c1, &doc, true);
	TryToParseString("Ciao Atomo123", d1, &doc, true);

	// Allineamento verticale su B1 (in basso): deve sopravvivere al giro
	// salva/ricarica sotto, come ogni altra sezione di stile.
	{
		CellStyle cs;
		doc.GetCellStyle(b1, cs);
		cs.fVerticalAlignment = eVAlignBottom;
		doc.SetCellStyle(b1, cs);
	}
	doc.CalcCell(c1);
	Value beforeSave;
	doc.GetValue(c1, beforeSave);
	Check((double)beforeSave == 30.0, "la formula C1 calcola 30 prima del salvataggio");

	BFile file("tests/roundtrip.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	Check(file.InitCheck() == B_OK, "creazione del file di test riuscita");

	status_t err = SaveASCD(&doc, &file);
	Check(err == B_OK, "SaveASCD riesce");
	doc.Release();

	BFile reopened("tests/roundtrip.ascd", B_READ_ONLY);
	Check(reopened.InitCheck() == B_OK, "riapertura del file salvato riuscita");

	CContainer& reloaded = *new CContainer(NULL, NULL);
	err = LoadASCD(&reopened, &reloaded);
	Check(err == B_OK, "LoadASCD riesce");

	char text[512];
	reloaded.GetCellFormula(a1, text, sizeof(text), false);
	Check(strcmp(text, "10") == 0, "A1 e' 10 dopo il giro completo");

	reloaded.GetCellFormula(b1, text, sizeof(text), false);
	Check(strcmp(text, "20") == 0, "B1 e' 20 dopo il giro completo");

	reloaded.GetCellFormula(c1, text, sizeof(text), false);
	Check(strstr(text, "A1") != NULL && strstr(text, "B1") != NULL,
		"C1 mantiene la formula (non il valore gia' calcolato) dopo il giro completo");

	reloaded.GetCellFormula(d1, text, sizeof(text), false);
	Check(strcmp(text, "Ciao Atomo123") == 0, "D1 mantiene il testo dopo il giro completo");

	CellStyle rcs;
	reloaded.GetCellStyle(b1, rcs);
	Check(rcs.fVerticalAlignment == eVAlignBottom,
		"B1 mantiene allineamento verticale in basso dopo il giro completo");
	reloaded.GetCellStyle(a1, rcs);
	Check(rcs.fVerticalAlignment == eVAlignTop,
		"A1 (mai toccata) resta con allineamento verticale in alto dopo il giro completo");

	// LoadASCD deve aver gia' ricalcolato da solo (RecalculateAll): a
	// differenza di TryToParseString, che imposta solo la formula
	// senza calcolarla, il valore deve essere gia' corretto qui,
	// PRIMA di qualunque CalcCell esplicito -- altrimenti la griglia
	// mostrerebbe celle vuote finche' l'utente non le tocca a mano.
	Value afterLoad;
	reloaded.GetValue(c1, afterLoad);
	Check((double)afterLoad == 30.0,
		"LoadASCD ricalcola gia' da solo C1 a 30, senza bisogno di un CalcCell esplicito");

	reloaded.Release();

	// Bug reale scoperto aprendo una tabella di codici ATECO (colonna
	// di codici come "01.11.10", tre gruppi separati da punti): un
	// testo che assomiglia abbastanza a un numero/data da superare
	// l'analisi grammaticale del parser ma poi fallisce a ridursi a un
	// valore fa lanciare un'eccezione da TryToParseString quando
	// chiamata con inWarnIfError=true (come faceva LoadASCD prima di
	// questo fix) -- il testo originale va perso del tutto, la cella
	// resta vuota, invece di ripiegare sul testo cosi' com'e' (stesso
	// comportamento di sicurezza gia' in uso dai tre translator XLSX/
	// ODS/CSV quando importano testo da un formato esterno).
	{
		CContainer& ambigDoc = *new CContainer(NULL, NULL);
		cell f1(1, 1), f2(1, 2), f3(1, 3);
		TryToParseString("01.11.10", f1, &ambigDoc, false);
		TryToParseString("01.12.00", f2, &ambigDoc, false);
		TryToParseString("CODICE", f3, &ambigDoc, false);

		BFile ambigFile("tests/roundtrip_ambiguous_text.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(&ambigDoc, &ambigFile) == B_OK,
			"SaveASCD riesce con un testo ambiguo tipo codice ATECO");
		ambigDoc.Release();

		BFile ambigReopened("tests/roundtrip_ambiguous_text.ascd", B_READ_ONLY);
		CContainer& ambigReloaded = *new CContainer(NULL, NULL);
		Check(LoadASCD(&ambigReopened, &ambigReloaded) == B_OK,
			"LoadASCD riesce con un testo ambiguo tipo codice ATECO");

		Value v;
		ambigReloaded.GetValue(f1, v);
		Check(v.fType == eTextData && strcmp((const char*)v, "01.11.10") == 0,
			"\"01.11.10\" sopravvive al giro salva->ricarica come testo, non sparisce");

		ambigReloaded.GetValue(f2, v);
		Check(v.fType == eTextData && strcmp((const char*)v, "01.12.00") == 0,
			"\"01.12.00\" sopravvive al giro salva->ricarica come testo, non sparisce");

		ambigReloaded.GetValue(f3, v);
		Check(v.fType == eTextData && strcmp((const char*)v, "CODICE") == 0,
			"un testo non ambiguo continua a sopravvivere come prima");

		ambigReloaded.Release();
	}

	// Caso limite scoperto durante lo sviluppo dell'export ODS: una
	// formula che e' anche la cella piu' a destra/in basso del foglio
	// (nessun'altra cella "reale" oltre di lei) deve comunque essere
	// ricalcolata da RecalculateAll. GetBounds esclude le celle con
	// mType eNoData -- lo stato di una formula appena analizzata da
	// TryToParseString e non ancora calcolata -- quindi calcolare i
	// limiti del foglio PRIMA di ricalcolare escludeva proprio quella
	// cella dall'iterazione, lasciandola vuota per sempre (bug fisso
	// in RecalculateAll: ora itera l'intero range del foglio invece
	// dei limiti di GetBounds).
	{
		CContainer& edgeDoc = *new CContainer(NULL, NULL);
		cell e1(1, 1), e2(2, 1);
		TryToParseString("5", e1, &edgeDoc, true);
		TryToParseString("=A1+10", e2, &edgeDoc, true); // B1: cella piu' a destra del foglio

		BFile edgeFile("tests/edge.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		err = SaveASCD(&edgeDoc, &edgeFile);
		Check(err == B_OK, "SaveASCD riesce con una formula come cella piu' a destra del foglio");
		edgeDoc.Release();

		BFile edgeReopened("tests/edge.ascd", B_READ_ONLY);
		CContainer& edgeReloaded = *new CContainer(NULL, NULL);
		err = LoadASCD(&edgeReopened, &edgeReloaded);
		Check(err == B_OK, "LoadASCD riesce con una formula come cella piu' a destra del foglio");

		Value edgeValue;
		edgeReloaded.GetValue(e2, edgeValue);
		Check(edgeValue.fType == eNumData && (double)edgeValue == 15.0,
			"la formula nell'angolo del foglio viene ricalcolata (non resta vuota)");

		edgeReloaded.Release();
	}

	// Sezione grafici incorporati (Chart.h): un file scritto senza
	// (SaveASCD/LoadASCD sopra, chiamati senza il parametro "charts")
	// deve restare leggibile -- verificato implicitamente sopra, dove
	// LoadASCD ha gia' avuto successo senza quel parametro. Qui si
	// verifica invece che uno o piu' grafici sopravvivano a un giro
	// completo salva->ricarica, e che "charts" resti vuoto (non un
	// errore) se il file non ne conteneva nessuno.
	CContainer& chartDoc = *new CContainer(NULL, NULL);
	TryToParseString("10", cell(1, 1), &chartDoc, true);

	std::vector<ChartObject> saved;
	ChartObject obj;
	obj.dataRange.Set(1, 1, 2, 5);
	obj.frame.Set(100, 200, 400, 380);
	// Titolo (Fase 17): sezione separata in coda come il tipo, stesso
	// motivo -- verifica che sopravviva insieme al resto.
	obj.title = "Vendite 2026";
	saved.push_back(obj);

	// Un secondo grafico di tipo diverso (Fase 13, linee/torta): la
	// sezione dei tipi vive in una sezione SEPARATA in coda al
	// formato (vedi il commento in AscdIO.cpp), non dentro il record
	// fisso di ogni grafico -- verifica che l'ordine resti allineato
	// con due grafici, non solo uno.
	ChartObject obj2;
	obj2.dataRange.Set(1, 1, 2, 3);
	obj2.frame.Set(50, 50, 200, 150);
	obj2.type = ePieChart;
	saved.push_back(obj2);

	BFile chartFile("tests/roundtrip_charts.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	err = SaveASCD(&chartDoc, &chartFile, &saved);
	Check(err == B_OK, "SaveASCD con un grafico incorporato riesce");
	chartDoc.Release();

	BFile chartReopened("tests/roundtrip_charts.ascd", B_READ_ONLY);
	CContainer& chartReloaded = *new CContainer(NULL, NULL);
	std::vector<ChartObject> loaded;
	err = LoadASCD(&chartReopened, &chartReloaded, &loaded);
	Check(err == B_OK, "LoadASCD con un grafico incorporato riesce");
	Check(loaded.size() == 2, "entrambi i grafici sopravvivono al giro salva->ricarica");
	if (loaded.size() == 2)
	{
		Check(loaded[0].dataRange.left == 1 && loaded[0].dataRange.top == 1
				&& loaded[0].dataRange.right == 2 && loaded[0].dataRange.bottom == 5,
			"l'intervallo dati del primo grafico e' preservato");
		Check(loaded[0].frame == BRect(100, 200, 400, 380),
			"la posizione del primo grafico e' preservata");
		Check(loaded[0].type == eBarChart,
			"il primo grafico resta a barre (tipo predefinito)");
		Check(loaded[1].type == ePieChart,
			"il tipo del secondo grafico (torta) e' preservato, allineato al grafico giusto");
		Check(loaded[0].title == "Vendite 2026",
			"il titolo del primo grafico e' preservato");
		Check(loaded[1].title.IsEmpty(),
			"il secondo grafico (senza titolo assegnato) resta vuoto, non eredita quello del primo");
	}
	chartReloaded.Release();

	// Un file scritto SENZA sezione grafici (il primo di questo test,
	// scritto senza passare "charts" a SaveASCD) deve restituire un
	// vettore vuoto quando riletto CON "charts" richiesto -- non un
	// errore: e' la compatibilita' all'indietro che rende sicuro
	// aprire un .ascd salvato prima che questa sezione esistesse.
	BFile oldFormat("tests/roundtrip.ascd", B_READ_ONLY);
	CContainer& oldDoc = *new CContainer(NULL, NULL);
	std::vector<ChartObject> noCharts;
	err = LoadASCD(&oldFormat, &oldDoc, &noCharts);
	Check(err == B_OK && noCharts.empty(),
		"un file senza sezione grafici si rilegge senza errori e senza grafici");
	oldDoc.Release();

	// Sezione larghezze di colonna personalizzate: stesso principio dei
	// grafici sopra, verificata qui separatamente perche' e' un
	// parametro indipendente di SaveASCD/LoadASCD (un file puo' avere
	// grafici senza colonne ridimensionate, o viceversa).
	CContainer& widthDoc = *new CContainer(NULL, NULL);
	TryToParseString("10", cell(1, 1), &widthDoc, true);

	std::vector<std::pair<int, float> > savedWidths;
	savedWidths.push_back(std::make_pair(1, 120.0f));
	savedWidths.push_back(std::make_pair(3, 45.0f));

	BFile widthFile("tests/roundtrip_widths.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	err = SaveASCD(&widthDoc, &widthFile, NULL, &savedWidths);
	Check(err == B_OK, "SaveASCD con larghezze di colonna personalizzate riesce");
	widthDoc.Release();

	BFile widthReopened("tests/roundtrip_widths.ascd", B_READ_ONLY);
	CContainer& widthReloaded = *new CContainer(NULL, NULL);
	std::vector<std::pair<int, float> > loadedWidths;
	err = LoadASCD(&widthReopened, &widthReloaded, NULL, &loadedWidths);
	Check(err == B_OK, "LoadASCD con larghezze di colonna personalizzate riesce");
	Check(loadedWidths.size() == 2, "le due larghezze personalizzate sopravvivono al giro salva->ricarica");
	if (loadedWidths.size() == 2)
	{
		Check(loadedWidths[0].first == 1 && loadedWidths[0].second == 120.0f,
			"la larghezza della colonna 1 e' preservata");
		Check(loadedWidths[1].first == 3 && loadedWidths[1].second == 45.0f,
			"la larghezza della colonna 3 e' preservata");
	}
	widthReloaded.Release();

	// Un file scritto senza larghezze personalizzate deve restituire un
	// vettore vuoto quando riletto CON colWidths richiesto -- stessa
	// compatibilita' all'indietro dei grafici sopra.
	BFile oldFormat2("tests/roundtrip.ascd", B_READ_ONLY);
	CContainer& oldDoc2 = *new CContainer(NULL, NULL);
	std::vector<std::pair<int, float> > noWidths;
	err = LoadASCD(&oldFormat2, &oldDoc2, NULL, &noWidths);
	Check(err == B_OK && noWidths.empty(),
		"un file senza sezione larghezze si rilegge senza errori e senza larghezze personalizzate");
	oldDoc2.Release();

	// Colori di sfondo/testo (CellStyle::fLowColor/fHighColor): a
	// differenza di grafici/larghezze sopra, non passano da un
	// parametro dedicato -- vivono gia' dentro CContainer tramite
	// GetCellStyle/SetCellStyle/GetColumnStyle/SetColumnStyle (le
	// stesse gia' usate per il formato numerico), quindi SaveASCD li
	// legge direttamente da "doc" e LoadASCD li scrive direttamente li'.
	CContainer& colorDoc = *new CContainer(NULL, NULL);
	TryToParseString("Intestazione", cell(1, 1), &colorDoc, true); // A1: colore di cella

	CellStyle a1Style;
	a1Style.fLowColor.red = 255; a1Style.fLowColor.green = 255; a1Style.fLowColor.blue = 0;
	a1Style.fLowColor.alpha = 255; // sfondo giallo
	a1Style.fHighColor.red = 200; a1Style.fHighColor.green = 0; a1Style.fHighColor.blue = 0;
	a1Style.fHighColor.alpha = 255; // testo rosso
	colorDoc.SetCellStyle(cell(1, 1), a1Style);

	CellStyle col2Style;
	col2Style.fLowColor.red = 0; col2Style.fLowColor.green = 200; col2Style.fLowColor.blue = 0;
	col2Style.fLowColor.alpha = 255; // sfondo verde per tutta la colonna 2
	col2Style.fHighColor.alpha = 255; // testo nero (predefinito, dal costruttore)
	colorDoc.SetColumnStyle(2, col2Style);

	BFile colorFile("tests/roundtrip_colors.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	err = SaveASCD(&colorDoc, &colorFile);
	Check(err == B_OK, "SaveASCD con colori di cella/colonna personalizzati riesce");
	colorDoc.Release();

	BFile colorReopened("tests/roundtrip_colors.ascd", B_READ_ONLY);
	CContainer& colorReloaded = *new CContainer(NULL, NULL);
	err = LoadASCD(&colorReopened, &colorReloaded);
	Check(err == B_OK, "LoadASCD con colori di cella/colonna personalizzati riesce");

	CellStyle reloadedA1;
	colorReloaded.GetCellStyle(cell(1, 1), reloadedA1);
	Check(reloadedA1.fLowColor.red == 255 && reloadedA1.fLowColor.green == 255
			&& reloadedA1.fLowColor.blue == 0,
		"il colore di sfondo (giallo) di A1 sopravvive al giro salva->ricarica");
	Check(reloadedA1.fHighColor.red == 200 && reloadedA1.fHighColor.green == 0
			&& reloadedA1.fHighColor.blue == 0,
		"il colore di testo (rosso) di A1 sopravvive al giro salva->ricarica");

	// B5 e' del tutto vuota (mai toccata): CContainer::GetCellStyleNr
	// ricade sul colore di colonna SOLO per le celle che non esistono
	// affatto in fCellData -- una cella con un contenuto proprio ma
	// senza uno stile esplicito (mai il caso qui: SetCellStyle non e'
	// mai stata chiamata su B5) avrebbe comunque un proprio mStyle
	// predefinito e non erediterebbe il colore della colonna, stesso
	// meccanismo gia' esistente per il formato numerico.
	CellStyle reloadedB5;
	colorReloaded.GetCellStyle(cell(2, 5), reloadedB5);
	Check(reloadedB5.fLowColor.red == 0 && reloadedB5.fLowColor.green == 200
			&& reloadedB5.fLowColor.blue == 0,
		"B5, vuota, eredita il colore di sfondo (verde) della colonna 2");

	CellStyle reloadedC1;
	colorReloaded.GetCellStyle(cell(3, 1), reloadedC1);
	Check(reloadedC1.fLowColor.red == 255 && reloadedC1.fLowColor.green == 255
			&& reloadedC1.fLowColor.blue == 255,
		"una cella al di fuori della colonna colorata (C1) resta bianca");
	colorReloaded.Release();

	// Un file scritto senza colori personalizzati (il primo di questo
	// test) si rilegge senza errori, con ogni cella al colore
	// predefinito -- stessa compatibilita' all'indietro delle sezioni
	// sopra.
	BFile oldFormat3("tests/roundtrip.ascd", B_READ_ONLY);
	CContainer& oldDoc3 = *new CContainer(NULL, NULL);
	err = LoadASCD(&oldFormat3, &oldDoc3);
	Check(err == B_OK, "un file senza sezione colori si rilegge senza errori");
	CellStyle oldA1;
	oldDoc3.GetCellStyle(a1, oldA1);
	Check(oldA1.fLowColor.red == 255 && oldA1.fLowColor.green == 255 && oldA1.fLowColor.blue == 255
			&& oldA1.fHighColor.red == 0 && oldA1.fHighColor.green == 0 && oldA1.fHighColor.blue == 0,
		"...e ogni cella resta al colore predefinito (bianco/nero)");
	oldDoc3.Release();

	// Colore del bordo (Fase 13): CellStyle::fBorderColor, sezione
	// SEPARATA da quella di presenza/spessore del bordo (vedi il
	// commento in AscdIO.cpp) -- verificata qui a parte proprio perche'
	// e' una sezione indipendente, non un'estensione del formato colori
	// sopra.
	CContainer& borderColorDoc = *new CContainer(NULL, NULL);
	CellStyle a1Border;
	a1Border.fTBorderColor = 3; // spesso
	a1Border.fBorderColor.red = 220; a1Border.fBorderColor.green = 40;
	a1Border.fBorderColor.blue = 40; a1Border.fBorderColor.alpha = 255; // rosso
	borderColorDoc.SetCellStyle(cell(1, 1), a1Border);

	BFile borderColorFile("tests/roundtrip_border_color.ascd",
		B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	err = SaveASCD(&borderColorDoc, &borderColorFile);
	Check(err == B_OK, "SaveASCD con un colore di bordo personalizzato riesce");
	borderColorDoc.Release();

	BFile borderColorReopened("tests/roundtrip_border_color.ascd", B_READ_ONLY);
	CContainer& borderColorReloaded = *new CContainer(NULL, NULL);
	err = LoadASCD(&borderColorReopened, &borderColorReloaded);
	Check(err == B_OK, "LoadASCD con un colore di bordo personalizzato riesce");

	CellStyle reloadedBorder;
	borderColorReloaded.GetCellStyle(cell(1, 1), reloadedBorder);
	Check(reloadedBorder.fTBorderColor == 3,
		"lo spessore del bordo (spesso) sopravvive al giro salva->ricarica");
	Check(reloadedBorder.fBorderColor.red == 220 && reloadedBorder.fBorderColor.green == 40
			&& reloadedBorder.fBorderColor.blue == 40,
		"il colore del bordo (rosso) sopravvive al giro salva->ricarica, sezione separata dallo spessore");
	borderColorReloaded.Release();

	// Un file scritto senza colori di bordo personalizzati si rilegge
	// senza errori, con ogni bordo nero -- stessa compatibilita'
	// all'indietro delle altre sezioni opzionali.
	BFile oldFormat4("tests/roundtrip.ascd", B_READ_ONLY);
	CContainer& oldDoc4 = *new CContainer(NULL, NULL);
	err = LoadASCD(&oldFormat4, &oldDoc4);
	Check(err == B_OK, "un file senza sezione colore di bordo si rilegge senza errori");
	CellStyle oldBorderStyle;
	oldDoc4.GetCellStyle(a1, oldBorderStyle);
	Check(oldBorderStyle.fBorderColor.red == 0 && oldBorderStyle.fBorderColor.green == 0
			&& oldBorderStyle.fBorderColor.blue == 0,
		"...e ogni bordo resta al colore predefinito (nero)");
	oldDoc4.Release();

	// Formattazione condizionale VIVA (Fase 13): a differenza di tutte
	// le sezioni sopra (dato per cella), qui il dato e' la REGOLA
	// stessa -- il colore che ne risulta si ricalcola a ogni chiamata
	// di CContainer::EvaluateConditionalFormatting, mai memorizzato.
	{
		CContainer& condDoc = *new CContainer(NULL, NULL);
		TryToParseString("Mancante", cell(1, 1), &condDoc, true); // A1
		TryToParseString("OK", cell(1, 2), &condDoc, true);       // A2
		TryToParseString("X", cell(2, 1), &condDoc, true);        // B1
		TryToParseString("Y", cell(2, 2), &condDoc, true);        // B2
		TryToParseString("X", cell(2, 3), &condDoc, true);        // B3

		ConditionalFormatRule cellIsRule;
		cellIsRule.type = eCondCellIsEqual;
		cellIsRule.compareValue = "Mancante";
		cellIsRule.bgColor.red = 255; cellIsRule.bgColor.green = 199;
		cellIsRule.bgColor.blue = 206; cellIsRule.bgColor.alpha = 255; // FFC7CE
		cellIsRule.ranges.push_back(range(1, 1, 1, 2)); // A1:A2
		condDoc.AddConditionalFormatRule(cellIsRule);

		ConditionalFormatRule dupRule;
		dupRule.type = eCondDuplicateValues;
		dupRule.bgColor.red = 255; dupRule.bgColor.green = 235;
		dupRule.bgColor.blue = 156; dupRule.bgColor.alpha = 255; // FFEB9C
		dupRule.ranges.push_back(range(2, 1, 2, 3)); // B1:B3
		condDoc.AddConditionalFormatRule(dupRule);

		Check(condDoc.GetConditionalFormatRules().size() == 2,
			"AddConditionalFormatRule aggiunge davvero entrambe le regole");

		// --- Valutazione VIVA: i colori corrispondono ai valori
		// CORRENTI, senza che nessuno li abbia mai scritti in
		// CellStyle. ---
		std::map<cell, rgb_color> colors = condDoc.EvaluateConditionalFormatting();
		Check(colors.find(cell(1, 1)) != colors.end() && colors[cell(1, 1)].red == 255
				&& colors[cell(1, 1)].green == 199 && colors[cell(1, 1)].blue == 206,
			"A1 (\"Mancante\") ottiene il colore della regola cellIs/equal");
		Check(colors.find(cell(1, 2)) == colors.end(),
			"A2 (\"OK\", nessuna corrispondenza) non ottiene nessun colore");
		Check(colors.find(cell(2, 1)) != colors.end() && colors[cell(2, 1)].red == 255
				&& colors[cell(2, 1)].green == 235 && colors[cell(2, 1)].blue == 156,
			"B1 (\"X\", duplicata con B3) ottiene il colore della regola duplicateValues");
		Check(colors.find(cell(2, 3)) != colors.end(),
			"B3 (\"X\", duplicata con B1) ottiene anch'essa il colore");
		Check(colors.find(cell(2, 2)) == colors.end(),
			"B2 (\"Y\", valore unico nell'intervallo) non ottiene nessun colore");

		// La prova decisiva del "viva": si cambia il valore di una
		// cella DOPO aver gia' valutato le regole una volta, senza mai
		// toccare ne' le regole ne' CellStyle -- una nuova chiamata a
		// EvaluateConditionalFormatting deve riflettere il valore
		// NUOVO, non quello di prima.
		TryToParseString("Mancante", cell(1, 2), &condDoc, true); // A2 ora e' anch'essa "Mancante"
		std::map<cell, rgb_color> colorsAfterEdit = condDoc.EvaluateConditionalFormatting();
		Check(colorsAfterEdit.find(cell(1, 2)) != colorsAfterEdit.end(),
			"cambiando A2 in \"Mancante\" DOPO la prima valutazione, la rivalutazione la colora "
			"da sola, senza nessuna scrittura esplicita in CellStyle (e' questo che la rende viva)");

		condDoc.Release();
	}

	// Round-trip nel formato nativo: le regole (non un colore
	// congelato) sopravvivono a salvataggio/ricarica.
	{
		CContainer& condSaveDoc = *new CContainer(NULL, NULL);
		ConditionalFormatRule rule;
		rule.type = eCondCellIsEqual;
		rule.compareValue = "Mancante";
		rule.bgColor.red = 255; rule.bgColor.green = 199;
		rule.bgColor.blue = 206; rule.bgColor.alpha = 255;
		rule.ranges.push_back(range(1, 1, 1, 3));
		rule.ranges.push_back(range(5, 5, 6, 6)); // due intervalli sulla stessa regola
		condSaveDoc.AddConditionalFormatRule(rule);

		BFile condFile("tests/roundtrip_condformat.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(&condSaveDoc, &condFile) == B_OK,
			"SaveASCD con una regola di formattazione condizionale riesce");
		condSaveDoc.Release();

		BFile condReopened("tests/roundtrip_condformat.ascd", B_READ_ONLY);
		CContainer& condReloaded = *new CContainer(NULL, NULL);
		Check(LoadASCD(&condReopened, &condReloaded) == B_OK,
			"LoadASCD con una regola di formattazione condizionale riesce");

		const std::vector<ConditionalFormatRule>& reloadedRules
			= condReloaded.GetConditionalFormatRules();
		Check(reloadedRules.size() == 1, "la regola sopravvive al giro salva->ricarica");
		if (reloadedRules.size() == 1)
		{
			Check(reloadedRules[0].type == eCondCellIsEqual
					&& reloadedRules[0].compareValue == "Mancante",
				"il tipo e il valore di confronto sopravvivono al giro salva->ricarica");
			Check(reloadedRules[0].bgColor.red == 255 && reloadedRules[0].bgColor.green == 199
					&& reloadedRules[0].bgColor.blue == 206,
				"il colore sopravvive al giro salva->ricarica");
			Check(reloadedRules[0].ranges.size() == 2,
				"entrambi gli intervalli della stessa regola sopravvivono al giro salva->ricarica");
		}
		condReloaded.Release();
	}

	// Un file scritto senza regole di formattazione condizionale si
	// rilegge senza errori, con nessuna regola -- stessa
	// compatibilita' all'indietro delle altre sezioni opzionali.
	{
		BFile oldFormat5("tests/roundtrip.ascd", B_READ_ONLY);
		CContainer& oldDoc5 = *new CContainer(NULL, NULL);
		status_t err5 = LoadASCD(&oldFormat5, &oldDoc5);
		Check(err5 == B_OK && oldDoc5.GetConditionalFormatRules().empty(),
			"un file senza sezione formattazione condizionale si rilegge senza errori e senza regole");
		oldDoc5.Release();
	}

	// Round-trip di una regola a scala di colori (Fase 33/A punto 4,
	// versione 3 del formato): a differenza delle due regole sopra, qui
	// il dato non e' un valore/colore singolo ma un elenco di
	// ColorScalePoint, ognuno con un cfvoType, un cfvoValue e un
	// colore proprio.
	{
		CContainer& scaleSaveDoc = *new CContainer(NULL, NULL);
		ConditionalFormatRule scaleRule;
		scaleRule.type = eCondColorScale;
		scaleRule.ranges.push_back(range(1, 1, 1, 10));

		ColorScalePoint minPoint;
		minPoint.cfvoType = "min";
		minPoint.color.red = 255; minPoint.color.green = 0;
		minPoint.color.blue = 0; minPoint.color.alpha = 255;
		scaleRule.colorScalePoints.push_back(minPoint);

		ColorScalePoint midPoint;
		midPoint.cfvoType = "percentile";
		midPoint.cfvoValue = 50;
		midPoint.color.red = 255; midPoint.color.green = 255;
		midPoint.color.blue = 0; midPoint.color.alpha = 255;
		scaleRule.colorScalePoints.push_back(midPoint);

		ColorScalePoint maxPoint;
		maxPoint.cfvoType = "max";
		maxPoint.color.red = 0; maxPoint.color.green = 255;
		maxPoint.color.blue = 0; maxPoint.color.alpha = 255;
		scaleRule.colorScalePoints.push_back(maxPoint);

		scaleSaveDoc.AddConditionalFormatRule(scaleRule);

		BFile scaleFile("tests/roundtrip_colorscale.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(&scaleSaveDoc, &scaleFile) == B_OK,
			"SaveASCD con una regola a scala di colori (3 punti) riesce");
		scaleSaveDoc.Release();

		BFile scaleReopened("tests/roundtrip_colorscale.ascd", B_READ_ONLY);
		CContainer& scaleReloaded = *new CContainer(NULL, NULL);
		Check(LoadASCD(&scaleReopened, &scaleReloaded) == B_OK,
			"LoadASCD con una regola a scala di colori riesce");

		const std::vector<ConditionalFormatRule>& scaleReloadedRules
			= scaleReloaded.GetConditionalFormatRules();
		Check(scaleReloadedRules.size() == 1, "la regola sopravvive al giro salva->ricarica");
		if (scaleReloadedRules.size() == 1)
		{
			const ConditionalFormatRule& r = scaleReloadedRules[0];
			Check(r.type == eCondColorScale, "il tipo scala di colori sopravvive al giro");
			Check(r.colorScalePoints.size() == 3,
				"tutti e tre i punti di controllo sopravvivono al giro");
			if (r.colorScalePoints.size() == 3)
			{
				Check(r.colorScalePoints[0].cfvoType == "min"
						&& r.colorScalePoints[0].color.red == 255
						&& r.colorScalePoints[0].color.green == 0,
					"il punto minimo (tipo e colore) sopravvive al giro");
				Check(r.colorScalePoints[1].cfvoType == "percentile"
						&& r.colorScalePoints[1].cfvoValue == 50
						&& r.colorScalePoints[1].color.green == 255,
					"il punto percentile (tipo, valore e colore) sopravvive al giro");
				Check(r.colorScalePoints[2].cfvoType == "max"
						&& r.colorScalePoints[2].color.green == 255
						&& r.colorScalePoints[2].color.blue == 0,
					"il punto massimo (tipo e colore) sopravvive al giro");
			}
		}
		scaleReloaded.Release();
	}

	// Round-trip di una regola a barra dei dati (Tier 3, Fase B,
	// versione 6 del formato): il campo nuovo e' dataBarColor, in coda
	// al record della regola -- stesso schema "EOF tollerante ma
	// versionato" gia' verificato sopra per i punti di scala di colori.
	{
		CContainer& barSaveDoc = *new CContainer(NULL, NULL);
		ConditionalFormatRule barRule;
		barRule.type = eCondDataBar;
		barRule.ranges.push_back(range(2, 1, 2, 10));
		barRule.dataBarColor.red = 99; barRule.dataBarColor.green = 142;
		barRule.dataBarColor.blue = 198; barRule.dataBarColor.alpha = 255;

		ColorScalePoint barMinPoint;
		barMinPoint.cfvoType = "min";
		barRule.colorScalePoints.push_back(barMinPoint);
		ColorScalePoint barMaxPoint;
		barMaxPoint.cfvoType = "max";
		barRule.colorScalePoints.push_back(barMaxPoint);

		barSaveDoc.AddConditionalFormatRule(barRule);

		BFile barFile("tests/roundtrip_databar.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(&barSaveDoc, &barFile) == B_OK,
			"SaveASCD con una regola a barra dei dati riesce");
		barSaveDoc.Release();

		BFile barReopened("tests/roundtrip_databar.ascd", B_READ_ONLY);
		CContainer& barReloaded = *new CContainer(NULL, NULL);
		Check(LoadASCD(&barReopened, &barReloaded) == B_OK,
			"LoadASCD con una regola a barra dei dati riesce");

		const std::vector<ConditionalFormatRule>& barReloadedRules
			= barReloaded.GetConditionalFormatRules();
		Check(barReloadedRules.size() == 1, "la regola sopravvive al giro salva->ricarica");
		if (barReloadedRules.size() == 1)
		{
			const ConditionalFormatRule& r = barReloadedRules[0];
			Check(r.type == eCondDataBar, "il tipo barra dei dati sopravvive al giro");
			Check(r.dataBarColor.red == 99 && r.dataBarColor.green == 142
					&& r.dataBarColor.blue == 198,
				"il colore della barra sopravvive al giro");
			Check(r.colorScalePoints.size() == 2
					&& r.colorScalePoints[0].cfvoType == "min"
					&& r.colorScalePoints[1].cfvoType == "max",
				"le due soglie min/max sopravvivono al giro");
			Check(r.ranges.size() == 1 && r.ranges[0].left == 2 && r.ranges[0].right == 2
					&& r.ranges[0].top == 1 && r.ranges[0].bottom == 10,
				"l'intervallo della regola sopravvive al giro");
		}
		barReloaded.Release();
	}

	// Round-trip di una regola a icon set (Tier 3, Fase C, versione 7
	// del formato): il campo nuovo e' iconSetStyle (una stringa), in
	// coda al record della regola -- stesso schema di dataBarColor
	// sopra.
	{
		CContainer& iconSaveDoc = *new CContainer(NULL, NULL);
		ConditionalFormatRule iconRule;
		iconRule.type = eCondIconSet;
		iconRule.ranges.push_back(range(3, 1, 3, 10));
		iconRule.iconSetStyle = "3TrafficLights1";

		const double kPercents[3] = { 0, 33, 67 };
		for (int p = 0; p < 3; p++)
		{
			ColorScalePoint point;
			point.cfvoType = "percent";
			point.cfvoValue = kPercents[p];
			iconRule.colorScalePoints.push_back(point);
		}

		iconSaveDoc.AddConditionalFormatRule(iconRule);

		BFile iconFile("tests/roundtrip_iconset.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(&iconSaveDoc, &iconFile) == B_OK,
			"SaveASCD con una regola a icon set riesce");
		iconSaveDoc.Release();

		BFile iconReopened("tests/roundtrip_iconset.ascd", B_READ_ONLY);
		CContainer& iconReloaded = *new CContainer(NULL, NULL);
		Check(LoadASCD(&iconReopened, &iconReloaded) == B_OK,
			"LoadASCD con una regola a icon set riesce");

		const std::vector<ConditionalFormatRule>& iconReloadedRules
			= iconReloaded.GetConditionalFormatRules();
		Check(iconReloadedRules.size() == 1, "la regola sopravvive al giro salva->ricarica");
		if (iconReloadedRules.size() == 1)
		{
			const ConditionalFormatRule& r = iconReloadedRules[0];
			Check(r.type == eCondIconSet, "il tipo icon set sopravvive al giro");
			Check(r.iconSetStyle == "3TrafficLights1",
				"il nome dello stile sopravvive al giro");
			Check(r.colorScalePoints.size() == 3
					&& r.colorScalePoints[0].cfvoValue == 0
					&& r.colorScalePoints[1].cfvoValue == 33
					&& r.colorScalePoints[2].cfvoValue == 67,
				"le tre soglie percentuali sopravvivono al giro");
			Check(r.ranges.size() == 1 && r.ranges[0].left == 3 && r.ranges[0].right == 3
					&& r.ranges[0].top == 1 && r.ranges[0].bottom == 10,
				"l'intervallo della regola sopravvive al giro");
		}
		iconReloaded.Release();
	}

	// Round-trip dei campi versione 8 (Path to full Excel parity, Tier
	// 3 -- cellIs con operatori oltre "equal", top10, aboveAverage):
	// stesso schema "EOF tollerante ma versionato" gia' verificato
	// sopra per i campi versione 3/6/7. Due regole in una volta, una
	// cellIs "between" e una top10 "ultimi 20%", per coprire sia
	// compareValue2 sia i tre campi di eCondTop10 in un solo giro.
	{
		CContainer& v8SaveDoc = *new CContainer(NULL, NULL);
		ConditionalFormatRule betweenRule;
		betweenRule.type = eCondCellIsEqual;
		betweenRule.ruleOperator = 6; // between
		betweenRule.compareValue = "10";
		betweenRule.compareValue2 = "20";
		betweenRule.ranges.push_back(range(1, 1, 1, 10));
		v8SaveDoc.AddConditionalFormatRule(betweenRule);

		ConditionalFormatRule top10Rule;
		top10Rule.type = eCondTop10;
		top10Rule.top10Bottom = true;
		top10Rule.top10Percent = true;
		top10Rule.top10Rank = 20;
		top10Rule.ranges.push_back(range(2, 1, 2, 10));
		v8SaveDoc.AddConditionalFormatRule(top10Rule);

		BFile v8File("tests/roundtrip_condformat_v8.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(&v8SaveDoc, &v8File) == B_OK,
			"SaveASCD con regole cellIs \"between\" e top10 (campi versione 8) riesce");
		v8SaveDoc.Release();

		BFile v8Reopened("tests/roundtrip_condformat_v8.ascd", B_READ_ONLY);
		CContainer& v8Reloaded = *new CContainer(NULL, NULL);
		Check(LoadASCD(&v8Reopened, &v8Reloaded) == B_OK,
			"LoadASCD con regole cellIs \"between\" e top10 riesce");

		const std::vector<ConditionalFormatRule>& v8ReloadedRules
			= v8Reloaded.GetConditionalFormatRules();
		Check(v8ReloadedRules.size() == 2, "entrambe le regole sopravvivono al giro salva->ricarica");
		if (v8ReloadedRules.size() == 2)
		{
			const ConditionalFormatRule& br = v8ReloadedRules[0];
			Check(br.type == eCondCellIsEqual && br.ruleOperator == 6,
				"la regola cellIs mantiene il tipo e l'operatore \"between\" dopo il giro");
			Check(br.compareValue == "10" && br.compareValue2 == "20",
				"entrambi i limiti (compareValue/compareValue2) sopravvivono al giro");

			const ConditionalFormatRule& tr = v8ReloadedRules[1];
			Check(tr.type == eCondTop10, "la regola top10 mantiene il tipo dopo il giro");
			Check(tr.top10Bottom && tr.top10Percent && tr.top10Rank == 20,
				"top10Bottom/top10Percent/top10Rank sopravvivono tutti e tre al giro");
		}
		v8Reloaded.Release();

		// A differenza del pivot 2D legacy sotto (dove la sezione era
		// davvero l'ULTIMA scritta da SaveASCD al momento in cui fu
		// aggiunta, rendendo sicuro un troncamento diretto del
		// buffer), la formattazione condizionale e' seguita da molte
		// altre sezioni (tabelle strutturate, titolo/colonne di
		// grafico, area di stampa, impostazioni di stampa, VBA,
		// protezione...): troncare un numero fisso di byte in coda al
		// buffer intero rischia di tagliare dentro una di QUELLE
		// sezioni invece che esattamente ai campi versione 8 di questa
		// regola, un test fragile e non rappresentativo. Il principio
		// "un file piu' vecchio resta leggibile" per questa sezione e'
		// gia' verificato sopra ("un file senza sezione formattazione
		// condizionale si rilegge senza errori e senza regole", stesso
		// principio EOF-tollerante) e il gate "if (version >= 8)" in
		// LoadASCD segue lo stesso identico schema gia' in uso per le
		// versioni 4/5/6/7 -- nessuna delle quali ha un test di
		// troncamento dedicato in questo file.
	}

	// Round-trip di una tabella pivot persistita (oggetto + cache, non
	// solo le celle che WritePivotTable scrive): vedi PivotTableObject
	// in Container.h.
	{
		CContainer& pivotSaveDoc = *new CContainer(NULL, NULL);
		pivotSaveDoc.NewCell(cell(1, 1), Value("Mela"), NULL);
		pivotSaveDoc.NewCell(cell(2, 1), Value(10.0), NULL);
		pivotSaveDoc.NewCell(cell(1, 2), Value("Pera"), NULL);
		pivotSaveDoc.NewCell(cell(2, 2), Value(5.0), NULL);

		std::vector<PivotRow> pivotRows;
		range pivotSource(1, 1, 2, 2);
		BuildPivotTable(&pivotSaveDoc, pivotSource, pivotRows);
		WritePivotTable(&pivotSaveDoc, cell(4, 1), pivotRows, ePivotSum);

		PivotTableObject pivot;
		pivot.sourceRange = pivotSource;
		pivot.destAnchor = cell(4, 1);
		pivot.aggFunc = ePivotSum;
		pivot.cachedRows = pivotRows;
		pivotSaveDoc.AddPivotTable(pivot);

		BFile pivotFile("tests/roundtrip_pivot.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(&pivotSaveDoc, &pivotFile) == B_OK,
			"SaveASCD con una tabella pivot persistita riesce");
		pivotSaveDoc.Release();

		BFile pivotReopened("tests/roundtrip_pivot.ascd", B_READ_ONLY);
		CContainer& pivotReloaded = *new CContainer(NULL, NULL);
		Check(LoadASCD(&pivotReopened, &pivotReloaded) == B_OK,
			"LoadASCD con una tabella pivot persistita riesce");

		const std::vector<PivotTableObject>& reloadedPivots = pivotReloaded.GetPivotTables();
		Check(reloadedPivots.size() == 1, "la tabella pivot sopravvive al giro salva->ricarica");
		if (reloadedPivots.size() == 1)
		{
			Check(reloadedPivots[0].sourceRange.left == 1 && reloadedPivots[0].sourceRange.right == 2,
				"sourceRange sopravvive al giro salva->ricarica");
			Check(reloadedPivots[0].destAnchor.h == 4 && reloadedPivots[0].destAnchor.v == 1,
				"destAnchor sopravvive al giro salva->ricarica");
			Check(reloadedPivots[0].aggFunc == ePivotSum, "aggFunc sopravvive al giro salva->ricarica");
			Check(reloadedPivots[0].cachedRows.size() == pivotRows.size(),
				"cachedRows sopravvive al giro salva->ricarica");
		}

		// Le celle scritte da WritePivotTable sono anch'esse nel file,
		// caricate dalla normale sezione celle -- NON ricalcolate da
		// LoadASCD (nessuna chiamata a BuildPivotTable qui): verifica
		// che il valore rimasto sia quello scritto al salvataggio.
		Value reloadedHeader;
		pivotReloaded.GetValue(cell(4, 1), reloadedHeader);
		Check(BString((const char*)reloadedHeader) == "Categoria",
			"le celle scritte da WritePivotTable sopravvivono anch'esse (LoadASCD non ricalcola)");

		pivotReloaded.Release();
	}

	// Round-trip di un pivot 2D (campo Colonne + misure multiple, la
	// NUOVISSIMA ultima sezione del formato): stesso principio del
	// blocco 1D sopra, ma confrontando anche columnFieldCol/measures/
	// columnValues/cachedRows2D campo per campo.
	{
		CContainer& pivot2DSaveDoc = *new CContainer(NULL, NULL);

		PivotTableObject pivot2D;
		pivot2D.sourceRange = range(1, 1, 4, 4);
		pivot2D.destAnchor = cell(6, 1);
		pivot2D.columnFieldCol = 2;
		{
			PivotMeasure m;
			m.sourceCol = 3; m.aggFunc = ePivotSum; m.label = "TotA";
			pivot2D.measures.push_back(m);
		}
		{
			PivotMeasure m;
			m.sourceCol = 4; m.aggFunc = ePivotAverage; m.label = "TotB";
			pivot2D.measures.push_back(m);
		}
		pivot2D.columnValues.push_back(BString("Est"));
		pivot2D.columnValues.push_back(BString("Ovest"));
		{
			PivotRow2D r;
			r.categories.push_back(BString("Nord"));
			r.cells.resize(2);
			r.cells[0].resize(2); r.cells[1].resize(2);
			r.cells[0][0].aggregate = 110; r.cells[0][0].count = 2;
			r.cells[0][0].minVal = 10; r.cells[0][0].maxVal = 100;
			r.cells[0][1].aggregate = 11; r.cells[0][1].count = 2;
			r.cells[0][1].minVal = 1; r.cells[0][1].maxVal = 10;
			r.cells[1][0].aggregate = 200; r.cells[1][0].count = 1;
			r.cells[1][0].minVal = 200; r.cells[1][0].maxVal = 200;
			r.cells[1][1].aggregate = 20; r.cells[1][1].count = 1;
			r.cells[1][1].minVal = 20; r.cells[1][1].maxVal = 20;
			pivot2D.cachedRows2D.push_back(r);
		}
		{
			PivotRow2D r;
			r.categories.push_back(BString("Sud"));
			r.cells.resize(2);
			r.cells[0].resize(2); r.cells[1].resize(2);
			r.cells[0][0].aggregate = 50; r.cells[0][0].count = 1;
			r.cells[0][0].minVal = 50; r.cells[0][0].maxVal = 50;
			r.cells[0][1].aggregate = 5; r.cells[0][1].count = 1;
			r.cells[0][1].minVal = 5; r.cells[0][1].maxVal = 5;
			r.cells[1][0].aggregate = 100; r.cells[1][0].count = 2;
			r.cells[1][0].minVal = 20; r.cells[1][0].maxVal = 80;
			r.cells[1][1].aggregate = 10; r.cells[1][1].count = 2;
			r.cells[1][1].minVal = 2; r.cells[1][1].maxVal = 8;
			pivot2D.cachedRows2D.push_back(r);
		}
		pivot2DSaveDoc.AddPivotTable(pivot2D);

		BFile pivot2DFile("tests/roundtrip_pivot2d.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(&pivot2DSaveDoc, &pivot2DFile) == B_OK,
			"SaveASCD con un pivot 2D persistito riesce");
		pivot2DSaveDoc.Release();

		BFile pivot2DReopened("tests/roundtrip_pivot2d.ascd", B_READ_ONLY);
		CContainer& pivot2DReloaded = *new CContainer(NULL, NULL);
		Check(LoadASCD(&pivot2DReopened, &pivot2DReloaded) == B_OK,
			"LoadASCD con un pivot 2D persistito riesce");

		const std::vector<PivotTableObject>& reloadedPivots2D = pivot2DReloaded.GetPivotTables();
		Check(reloadedPivots2D.size() == 1, "il pivot 2D sopravvive al giro salva->ricarica");
		if (reloadedPivots2D.size() == 1)
		{
			const PivotTableObject& p = reloadedPivots2D[0];
			Check(p.columnFieldCol == 2, "columnFieldCol sopravvive al giro salva->ricarica");
			Check(p.measures.size() == 2
					&& p.measures[0].sourceCol == 3 && p.measures[0].aggFunc == ePivotSum
					&& p.measures[0].label == "TotA"
					&& p.measures[1].sourceCol == 4 && p.measures[1].aggFunc == ePivotAverage
					&& p.measures[1].label == "TotB",
				"measures (colonna/aggregazione/etichetta) sopravvivono al giro salva->ricarica");
			Check(p.columnValues.size() == 2 && p.columnValues[0] == "Est" && p.columnValues[1] == "Ovest",
				"columnValues sopravvivono al giro salva->ricarica");
			Check(p.cachedRows2D.size() == 2,
				"cachedRows2D sopravvive al giro salva->ricarica (due gruppi)");
			if (p.cachedRows2D.size() == 2)
			{
				Check(p.cachedRows2D[0].categories[0] == "Nord"
						&& p.cachedRows2D[0].cells[0][0].aggregate == 110
						&& p.cachedRows2D[0].cells[0][0].count == 2
						&& p.cachedRows2D[0].cells[1][1].aggregate == 20,
					"il grigliato di Nord sopravvive campo per campo");
				Check(p.cachedRows2D[1].categories[0] == "Sud"
						&& p.cachedRows2D[1].cells[1][0].aggregate == 100
						&& p.cachedRows2D[1].cells[1][0].count == 2,
					"il grigliato di Sud sopravvive campo per campo");
			}
		}
		pivot2DReloaded.Release();

		// Compatibilita' con un file scritto PRIMA di questa sezione:
		// tronca il buffer esattamente alla fine della sezione
		// orientamento riga di grafico (l'ultima sezione PRIMA di questa,
		// vedi il commento in SaveASCD) -- per un documento con UN solo
		// pivot interamente 1D (columnFieldCol=-1, measures/columnValues/
		// cachedRows2D tutti vuoti), la sezione pivot 2D e' sempre
		// ESATTAMENTE 18 byte (int32 count=1, poi int16+int32+int32+int32
		// tutti "vuoti" per quell'unico pivot) -- troncare quei 18 byte
		// equivale esattamente a un file scritto prima che questa
		// sezione esistesse.
		{
			CContainer& legacyDoc = *new CContainer(NULL, NULL);
			PivotTableObject legacyPivot;
			legacyPivot.sourceRange = range(1, 1, 2, 2);
			legacyPivot.destAnchor = cell(4, 1);
			legacyPivot.aggFunc = ePivotSum;
			legacyDoc.AddPivotTable(legacyPivot);

			BMallocIO legacyBuf;
			Check(SaveASCD(&legacyDoc, &legacyBuf) == B_OK,
				"SaveASCD con un pivot puramente 1D (per il test di compatibilita') riesce");
			legacyDoc.Release();

			size_t legacyFullLen = legacyBuf.BufferLength();
			Check(legacyFullLen > 18, "il buffer di riferimento e' abbastanza lungo da poter troncare 18 byte");
			if (legacyFullLen > 18)
			{
				BFile legacyFile("tests/roundtrip_pivot2d_legacy.ascd",
					B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
				legacyFile.Write(legacyBuf.Buffer(), legacyFullLen - 18);
				legacyFile.Unset();

				BFile legacyReopened("tests/roundtrip_pivot2d_legacy.ascd", B_READ_ONLY);
				CContainer& legacyReloaded = *new CContainer(NULL, NULL);
				status_t legacyErr = LoadASCD(&legacyReopened, &legacyReloaded);
				Check(legacyErr == B_OK,
					"un file .ascd senza la sezione pivot 2D (scritto prima che esistesse) si carica comunque");

				const std::vector<PivotTableObject>& legacyPivots = legacyReloaded.GetPivotTables();
				Check(legacyPivots.size() == 1
						&& legacyPivots[0].columnFieldCol == -1
						&& legacyPivots[0].measures.empty()
						&& legacyPivots[0].columnValues.empty()
						&& legacyPivots[0].cachedRows2D.empty(),
					"senza la sezione, il pivot resta al suo default 1D legacy "
					"(columnFieldCol=-1, measures/columnValues/cachedRows2D vuoti)");
				legacyReloaded.Release();

				// Un solo byte in meno (17 troncati invece di 18): l'intero
				// record del pivot (pivot2DCount, columnFieldCol,
				// measureCount, columnValueCount) si legge per intero, ma
				// l'ultimo byte di rowCount2D manca -- deve fallire con
				// B_BAD_DATA, non leggere oltre i limiti del buffer.
				BFile shortFile("tests/roundtrip_pivot2d_short.ascd",
					B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
				shortFile.Write(legacyBuf.Buffer(), legacyFullLen - 1);
				shortFile.Unset();

				BFile shortReopened("tests/roundtrip_pivot2d_short.ascd", B_READ_ONLY);
				CContainer& shortReloaded = *new CContainer(NULL, NULL);
				status_t shortErr = LoadASCD(&shortReopened, &shortReloaded);
				Check(shortErr == B_BAD_DATA,
					"una sezione pivot 2D troncata a meta' record viene rifiutata con B_BAD_DATA");
				shortReloaded.Release();
			}
		}
	}

	// --- Un file .ascd con la colonna di un commento manomessa (fuori
	// dall'intervallo valido) viene rifiutato con B_BAD_DATA invece di
	// causare una lettura fuori dai limiti. Bug reale trovato durante
	// un audit: "col"/"row" letti grezzi dal file finivano dritti in
	// cell/GetColumnStyleNr (fColumnStyles[col] ecc.) senza nessun
	// controllo in una build di rilascio -- ASSERT esiste solo in
	// debug. Qui si manomette un file scritto da SaveASCD invece di
	// costruirne uno a mano, cosi' il resto del formato resta valido e
	// l'unica differenza e' il campo sotto test. ---
	{
		CContainer* corruptDoc = new CContainer(NULL, NULL);
		corruptDoc->SetComment(cell(3, 5), "prova"); // colonna 3, riga 5

		BFile corruptFile("tests/roundtrip.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(corruptDoc, &corruptFile) == B_OK,
			"SaveASCD con un commento (di prova, per la manomissione sotto) riesce");
		corruptDoc->Release();
		corruptFile.Unset();

		BFile readBack("tests/roundtrip.ascd", B_READ_ONLY);
		off_t size = 0;
		readBack.GetSize(&size);
		std::vector<char> bytes((size_t)size);
		readBack.Read(&bytes[0], (size_t)size);
		readBack.Unset();

		// Cerca la sequenza riga(int16)=5, colonna(int16)=3,
		// lunghezza(int32)=5 (la lunghezza di "prova"): la combinazione
		// degli otto byte insieme e' praticamente unica nel file,
		// molto meno probabile di una singola coppia riga/colonna da
		// sola a comparire per caso altrove nei dati binari.
		int16 targetRow = 5, targetCol = 3;
		int32 targetLen = 5;
		int32 found = -1;
		for (size_t i = 0; i + 8 <= bytes.size(); i++)
		{
			int16 r, c;
			int32 len;
			memcpy(&r, &bytes[i], 2);
			memcpy(&c, &bytes[i + 2], 2);
			memcpy(&len, &bytes[i + 4], 4);
			if (r == targetRow && c == targetCol && len == targetLen)
			{
				found = (int32)i;
				break;
			}
		}
		Check(found >= 0,
			"i byte riga/colonna/lunghezza del commento di prova si trovano nel file salvato");

		if (found >= 0)
		{
			int16 badCol = 30000; // ben oltre kColCount
			memcpy(&bytes[found + 2], &badCol, 2);

			BFile tampered("tests/roundtrip.ascd", B_WRITE_ONLY | B_ERASE_FILE);
			tampered.Write(&bytes[0], bytes.size());
			tampered.Unset();

			BFile reopenTampered("tests/roundtrip.ascd", B_READ_ONLY);
			CContainer* reloadedTampered = new CContainer(NULL, NULL);
			status_t err = LoadASCD(&reopenTampered, reloadedTampered);
			Check(err == B_BAD_DATA,
				"un file con la colonna del commento manomessa (30000, fuori dall'intervallo valido) "
				"viene rifiutato con B_BAD_DATA, non causa una lettura fuori dai limiti");
			reloadedTampered->Release();
		}
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
