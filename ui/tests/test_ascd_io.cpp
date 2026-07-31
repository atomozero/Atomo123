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
	saved.push_back(obj);

	BFile chartFile("tests/roundtrip_charts.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	err = SaveASCD(&chartDoc, &chartFile, &saved);
	Check(err == B_OK, "SaveASCD con un grafico incorporato riesce");
	chartDoc.Release();

	BFile chartReopened("tests/roundtrip_charts.ascd", B_READ_ONLY);
	CContainer& chartReloaded = *new CContainer(NULL, NULL);
	std::vector<ChartObject> loaded;
	err = LoadASCD(&chartReopened, &chartReloaded, &loaded);
	Check(err == B_OK, "LoadASCD con un grafico incorporato riesce");
	Check(loaded.size() == 1, "il grafico sopravvive al giro salva->ricarica");
	if (loaded.size() == 1)
	{
		Check(loaded[0].dataRange.left == 1 && loaded[0].dataRange.top == 1
				&& loaded[0].dataRange.right == 2 && loaded[0].dataRange.bottom == 5,
			"l'intervallo dati del grafico e' preservato");
		Check(loaded[0].frame == BRect(100, 200, 400, 380),
			"la posizione del grafico e' preservata");
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

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
