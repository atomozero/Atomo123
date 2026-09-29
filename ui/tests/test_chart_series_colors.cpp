/*
	test_chart_series_colors.cpp

	Verifica i colori personalizzati per serie nell'editor dei grafici
	(ultima delle 4 migliorie "editor piu' professionale" richieste
	dall'utente, dopo le icone del tipo e l'interruttore righe/colonne).

	Tre parti, come da piano:
	1) SeriesColor() pura, senza MainWindow -- la logica del segnaposto
	   alpha 0 ("nessun colore scelto per questa posizione", vedi il
	   commento su SeriesColor() in Chart.h).
	2) MainWindow vera, HandleChartInsert/HandleChartUpdate chiamati
	   direttamente con seriesColors esplicito (stesso schema di
	   test_chart_row_orientation.cpp) -- compreso un giro annulla/
	   ripristina che verifica ancora una volta che il "prima" catturato
	   da SaveChartEditUndoState sia l'INTERO ChartObject, quindi nessun
	   codice nuovo per l'undo era necessario per questo campo.
	3) AscdIO: un giro salva->ricarica in memoria/su file conferma che i
	   colori sopravvivono (RGBA per intero, non solo RGB: l'alpha e'
	   significativo qui, a differenza del colore di scheda foglio). Poi,
	   per provare CONCRETAMENTE (non solo a parole) che la sezione e'
	   EOF-tollerante: tronca gli ultimi byte del file (esattamente quelli
	   della nuova sezione, l'ultima cosa scritta da SaveASCD) per
	   simulare un file scritto PRIMA di questa modifica, e verifica che
	   si carichi comunque con seriesColors vuoto invece di fallire.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <File.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "AscdIO.h"
#include "Chart.h"

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
	BApplication app("application/x-vnd.Atomo-TestChartSeriesColors");

	// --- Parte 1: SeriesColor() pura -----------------------------------

	std::vector<rgb_color> empty;
	rgb_color fallback0 = SeriesColor(empty, 0);
	rgb_color fallback1 = SeriesColor(empty, 1);
	Check(!(fallback0 == fallback1), "senza sovrascritture, serie diverse ricevono colori diversi dalla tavolozza");

	std::vector<rgb_color> overrides;
	overrides.push_back(rgb_color{ 200, 30, 30, 255 });
	Check(SeriesColor(overrides, 0) == rgb_color{ 200, 30, 30, 255 },
		"un colore scelto (alpha 255) viene restituito cosi' com'e'");
	Check(SeriesColor(overrides, 1) == SeriesColor(empty, 1),
		"un indice oltre l'ultima sovrascrittura ricade sulla tavolozza predefinita");

	// Segnaposto alpha 0: una posizione "riempita" solo per fare spazio a
	// un indice successivo (es. l'utente ha scelto il colore della serie
	// 2 prima di quello della serie 0) NON deve essere confusa con un
	// vero colore scelto -- deve ricadere comunque sulla tavolozza.
	std::vector<rgb_color> withGap;
	withGap.push_back(rgb_color{ 0, 0, 0, 0 });
	withGap.push_back(rgb_color{ 10, 20, 30, 0 });
	withGap.push_back(rgb_color{ 90, 90, 90, 255 });
	Check(SeriesColor(withGap, 0) == SeriesColor(empty, 0),
		"un segnaposto alpha 0 in posizione 0 ricade sulla tavolozza, non resta nero trasparente");
	Check(SeriesColor(withGap, 1) == SeriesColor(empty, 1),
		"un segnaposto alpha 0 in posizione 1 ricade sulla tavolozza");
	Check(SeriesColor(withGap, 2) == rgb_color{ 90, 90, 90, 255 },
		"il vero colore scelto in posizione 2 (dopo i segnaposto) e' restituito cosi' com'e'");

	// --- Parte 2: MainWindow vera, insert/update/undo/redo --------------

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer* doc = win->GetSheetView()->Document();

	TryToParseString("Gen", cell(1, 1), doc, true);
	TryToParseString("10", cell(2, 1), doc, true);
	TryToParseString("20", cell(3, 1), doc, true);
	TryToParseString("Feb", cell(1, 2), doc, true);
	TryToParseString("30", cell(2, 2), doc, true);
	TryToParseString("40", cell(3, 2), doc, true);
	TryToParseString("Mar", cell(1, 3), doc, true);
	TryToParseString("50", cell(2, 3), doc, true);
	TryToParseString("60", cell(3, 3), doc, true);

	std::vector<rgb_color> chosenColors;
	chosenColors.push_back(rgb_color{ 220, 40, 40, 255 });
	chosenColors.push_back(rgb_color{ 40, 140, 220, 255 });

	win->HandleChartInsert("A1:C3", "E1", eBarChart, "Con colori", /*rowOriented=*/false, chosenColors);
	Check(win->Charts().size() == 1, "HandleChartInsert con seriesColors espliciti aggiunge un grafico");
	if (win->Charts().size() == 1)
	{
		const ChartObject& obj = win->Charts()[0];
		Check(obj.seriesColors.size() == 2, "il grafico ricorda i 2 colori scelti");
		if (obj.seriesColors.size() == 2)
		{
			Check(obj.seriesColors[0] == chosenColors[0], "il colore della prima serie e' quello scelto");
			Check(obj.seriesColors[1] == chosenColors[1], "il colore della seconda serie e' quello scelto");
		}
	}

	// Un aggiornamento con un vettore diverso sostituisce interamente i
	// colori precedenti (indicizzati per POSIZIONE di serie, non per
	// colonna del foglio -- niente logica di "conserva se l'intervallo
	// non cambia" qui, a differenza di valueColumns).
	std::vector<rgb_color> newColors;
	newColors.push_back(rgb_color{ 10, 200, 10, 255 });
	win->HandleChartUpdate(0, "A1:C3", eLineChart, "Colori aggiornati", /*rowOriented=*/false, newColors);
	Check(win->Charts()[0].seriesColors.size() == 1,
		"HandleChartUpdate sostituisce interamente il vettore di colori");
	if (win->Charts()[0].seriesColors.size() == 1)
		Check(win->Charts()[0].seriesColors[0] == newColors[0], "il nuovo colore e' quello inviato");

	// Annullabile SENZA nessun codice nuovo per l'undo (stesso principio
	// gia' verificato per rowOriented in test_chart_row_orientation.cpp):
	// il "prima" catturato e' l'intero ChartObject.
	win->GetSheetView()->Undo();
	Check(win->Charts()[0].seriesColors.size() == 2,
		"Annulla ripristina i 2 colori precedenti (stato prima dell'ultimo aggiornamento)");

	win->GetSheetView()->Redo();
	Check(win->Charts()[0].seriesColors.size() == 1,
		"Ripristina riapplica l'aggiornamento a 1 colore appena annullato");

	// Un normale HandleChartInsert senza seriesColors (default vuoto)
	// continua a funzionare esattamente come prima di questa modifica.
	win->HandleChartInsert("A1:C3", "E10", eBarChart, "Grafico normale");
	Check(win->Charts().size() == 2, "un HandleChartInsert senza seriesColors (default) continua a funzionare");
	if (win->Charts().size() == 2)
		Check(win->Charts()[1].seriesColors.empty(),
			"il valore predefinito di seriesColors resta vuoto quando non specificato (tavolozza di sempre)");

	win->Unlock();

	win->Lock();
	win->Quit();

	// --- Parte 3: round-trip AscdIO --------------------------------------

	{
		CContainer& chartDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), &chartDoc, true);

		std::vector<ChartObject> saved;
		ChartObject obj;
		obj.dataRange.Set(1, 1, 2, 5);
		obj.frame.Set(100, 200, 400, 380);
		obj.seriesColors.push_back(rgb_color{ 250, 5, 5, 255 });
		// Un segnaposto alpha 0 in mezzo (mai scelto dall'utente, solo
		// riempimento per far spazio all'indice 2) deve sopravvivere
		// come alpha 0, non essere silenziosamente promosso ad alpha 255
		// -- questo e' esattamente il bug catturato durante l'impianto di
		// questa sezione (vedi il commento in AscdIO.cpp).
		obj.seriesColors.push_back(rgb_color{ 0, 0, 0, 0 });
		obj.seriesColors.push_back(rgb_color{ 5, 5, 250, 255 });
		saved.push_back(obj);

		// Un secondo grafico senza nessun colore personalizzato: deve
		// restare vuoto dopo il giro, non ereditare quelli del primo.
		ChartObject obj2;
		obj2.dataRange.Set(1, 1, 2, 3);
		obj2.frame.Set(50, 50, 200, 150);
		saved.push_back(obj2);

		BFile chartFile("tests/roundtrip_chart_series_colors.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCD(&chartDoc, &chartFile, &saved);
		Check(err == B_OK, "SaveASCD con colori per serie riesce");
		chartDoc.Release();

		BFile chartReopened("tests/roundtrip_chart_series_colors.ascd", B_READ_ONLY);
		CContainer& chartReloaded = *new CContainer(NULL, NULL);
		std::vector<ChartObject> loaded;
		err = LoadASCD(&chartReopened, &chartReloaded, &loaded);
		Check(err == B_OK, "LoadASCD con colori per serie riesce");
		Check(loaded.size() == 2, "entrambi i grafici sopravvivono al giro salva->ricarica");
		if (loaded.size() == 2)
		{
			Check(loaded[0].seriesColors.size() == 3,
				"i 3 colori (compreso il segnaposto) del primo grafico sopravvivono");
			if (loaded[0].seriesColors.size() == 3)
			{
				Check(loaded[0].seriesColors[0] == rgb_color{ 250, 5, 5, 255 },
					"il primo colore (alpha 255) e' preservato per intero");
				Check(loaded[0].seriesColors[1].alpha == 0,
					"il segnaposto alpha 0 resta alpha 0 dopo il giro (NON viene promosso a 255)");
				Check(loaded[0].seriesColors[2] == rgb_color{ 5, 5, 250, 255 },
					"il terzo colore (dopo il segnaposto) e' preservato per intero");
			}
			Check(loaded[1].seriesColors.empty(),
				"il secondo grafico (senza colori personalizzati) resta vuoto, non eredita quelli del primo");
		}
		chartReloaded.Release();

		// --- Compatibilita' con file scritti PRIMA di questa modifica ---
		//
		// La sezione seriesColors NON e' piu' l'ultima cosa scritta da
		// SaveASCD (lo era quando questo test fu scritto): fasi
		// successive hanno appeso, in ordine, la sezione scenari, quella
		// degli ID di stile con nome per cella, il byte hasStyles/hasTheme
		// (Tier 4, "Named cell styles + live theme palette"), e infine la
		// sezione opzioni per serie di grafico (Fase 7, "asse secondario /
		// trendline / barre d'errore") -- stessa lezione appresa (e
		// documentata) in test_scenario_manager.cpp quando gli si e'
		// aggiunta la stessa sezione in coda. Per un file con un solo
		// grafico SENZA colori personalizzati, senza scenari, senza celle
		// con stile con nome, senza tema e senza opzioni di serie, la coda
		// del file e' quindi: 4+4 byte (contatore grafici/colori di
		// QUESTA sezione) + 4 byte (contatore scenari, sempre 0 qui) + 4
		// byte (contatore celle con stile con nome, sempre 0 qui) + 1
		// byte (hasStyles, 0) + 1 byte (hasTheme, 0) + 4 byte (contatore
		// grafici della sezione opzioni per serie, 1 qui: il vettore
		// "charts" passato a SaveASCD non e' mai NULL in questo test) + 4
		// byte (optionCount di quell'unico grafico, 0 qui: nessuna
		// opzione impostata) = 26 byte in tutto. Troncare questi 26 byte
		// finali equivale quindi a un file scritto da una build PRIMA che
		// la sezione seriesColors esistesse -- prova CONCRETA (non solo
		// un ragionamento sul codice) che LoadASCD resta EOF-tollerante
		// con un file vecchio. NOTA per la prossima fase che aggiunge una
		// sezione in coda: questo conteggio andra' aggiornato di nuovo.
		CContainer& oldDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), &oldDoc, true);
		std::vector<ChartObject> oldSaved;
		ChartObject oldObj;
		oldObj.dataRange.Set(1, 1, 2, 5);
		oldObj.frame.Set(10, 10, 100, 100);
		oldSaved.push_back(oldObj);

		BFile oldFile("tests/roundtrip_chart_series_colors_old.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		err = SaveASCD(&oldDoc, &oldFile, &oldSaved);
		Check(err == B_OK, "SaveASCD di riferimento (un grafico, nessun colore) riesce");
		oldDoc.Release();

		off_t size = 0;
		oldFile.GetSize(&size);
		Check(size > 26, "il file di riferimento e' abbastanza grande da poter troncare 26 byte");
		oldFile.SetSize(size - 26);

		BFile oldReopened("tests/roundtrip_chart_series_colors_old.ascd", B_READ_ONLY);
		CContainer& oldReloaded = *new CContainer(NULL, NULL);
		std::vector<ChartObject> oldLoaded;
		err = LoadASCD(&oldReopened, &oldReloaded, &oldLoaded);
		Check(err == B_OK, "un file troncato (che simula una build precedente a questa sezione) si carica comunque");
		Check(oldLoaded.size() == 1, "il grafico del file troncato sopravvive comunque");
		if (oldLoaded.size() == 1)
			Check(oldLoaded[0].seriesColors.empty(),
				"un file senza questa sezione (simulato) restituisce seriesColors vuoto, non un errore");
		oldReloaded.Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
