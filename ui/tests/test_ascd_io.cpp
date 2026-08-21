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
#include <File.h>

#include "AscdIO.h"
#include "Cell.h"
#include "CellStyle.h"
#include "Chart.h"
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
