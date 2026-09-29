/*
	test_chart_trendlines.cpp

	Verifica "Fase 7" (asse secondario / trendline / barre d'errore),
	l'ultimo elemento non pianificato della lista grafici in
	ROADMAP.md, "Path to full Excel parity" Tier 4. Cresce nel tempo
	insieme alle sotto-fasi 7a-7d (vedi il piano):

	1) Sotto-fase 7a (solo plumbing, nessuna funzionalita' visibile
	   ancora): SeriesOptions() pura, senza MainWindow -- stesso
	   principio permissivo di SeriesColor() (vedi
	   test_chart_series_colors.cpp), ma qui "non impostato" e'
	   semplicemente un ChartSeriesOptions() di default, non un
	   segnaposto alpha.
	2) Sotto-fase 7b (trendline lineare/media mobile): ComputeLinearTrendline/
	   ComputeMovingAverageTrendline pure (nessun BView/Draw), poi
	   MainWindow vera (HandleChartInsert/HandleChartUpdate/undo/redo,
	   stesso schema di test_chart_series_colors.cpp Parte 2) e infine un
	   giro AscdIO (salva->ricarica + troncamento EOF-tollerante, stesso
	   schema della Parte 3 li').
*/

#include <cstdio>
#include <cmath>

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

static bool IsDefault(const ChartSeriesOptions& o)
{
	return o.secondaryAxis == false && o.trendlineType == eNoTrendline
		&& o.trendlinePeriod == 2 && o.errorBarMode == eNoErrorBars
		&& o.errorBarValue == 0.0;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestChartTrendlines");

	// --- Parte 1: SeriesOptions() pura -----------------------------------

	std::vector<ChartSeriesOptions> empty;
	Check(IsDefault(SeriesOptions(empty, 0)),
		"un vettore vuoto restituisce opzioni di default per qualunque indice");
	Check(IsDefault(SeriesOptions(empty, 5)),
		"un vettore vuoto restituisce opzioni di default anche per un indice alto");

	std::vector<ChartSeriesOptions> shortVec;
	shortVec.push_back(ChartSeriesOptions());
	Check(IsDefault(SeriesOptions(shortVec, 3)),
		"un indice oltre la fine di un vettore piu' corto ricade sulle opzioni di default");

	ChartSeriesOptions custom;
	custom.secondaryAxis = true;
	custom.trendlineType = eMovingAverageTrendline;
	custom.trendlinePeriod = 5;
	custom.errorBarMode = ePercentErrorBars;
	custom.errorBarValue = 12.5;

	std::vector<ChartSeriesOptions> populated;
	populated.push_back(ChartSeriesOptions());
	populated.push_back(custom);

	ChartSeriesOptions result0 = SeriesOptions(populated, 0);
	Check(IsDefault(result0), "l'indice 0 (mai toccato) resta di default in un vettore popolato");

	ChartSeriesOptions result1 = SeriesOptions(populated, 1);
	Check(result1.secondaryAxis == true, "secondaryAxis viene restituito cosi' com'e'");
	Check(result1.trendlineType == eMovingAverageTrendline, "trendlineType viene restituito cosi' com'e'");
	Check(result1.trendlinePeriod == 5, "trendlinePeriod viene restituito cosi' com'e'");
	Check(result1.errorBarMode == ePercentErrorBars, "errorBarMode viene restituito cosi' com'e'");
	Check(result1.errorBarValue == 12.5, "errorBarValue viene restituito cosi' com'e'");

	// --- Parte 2: ComputeLinearTrendline/ComputeMovingAverageTrendline --

	{
		// y = 2x + 1 esattamente: la regressione ai minimi quadrati su
		// punti gia' perfettamente allineati deve ritrovare la retta
		// esatta, non solo un'approssimazione grezza.
		std::vector<double> xs, ys;
		for (int i = 0; i < 5; i++)
		{
			xs.push_back((double)i);
			ys.push_back(2.0 * i + 1.0);
		}
		double slope = 0, intercept = 0;
		bool ok = ComputeLinearTrendline(xs, ys, &slope, &intercept);
		Check(ok, "ComputeLinearTrendline riesce su un dataset lineare esatto");
		Check(fabs(slope - 2.0) < 1e-9, "la pendenza ritrovata e' esattamente 2");
		Check(fabs(intercept - 1.0) < 1e-9, "l'intercetta ritrovata e' esattamente 1");

		std::vector<double> oneX(1, 5.0), oneY(1, 10.0);
		Check(!ComputeLinearTrendline(oneX, oneY, &slope, &intercept),
			"un solo punto (n<2) non produce una retta");

		std::vector<double> sameX(3, 7.0);
		std::vector<double> anyY; anyY.push_back(1); anyY.push_back(2); anyY.push_back(3);
		Check(!ComputeLinearTrendline(sameX, anyY, &slope, &intercept),
			"ogni x coincidente (denominatore zero) non produce una retta");
	}

	{
		std::vector<double> ys;
		ys.push_back(10); ys.push_back(20); ys.push_back(30); ys.push_back(40);
		std::vector<MovingAveragePoint> avg;
		ComputeMovingAverageTrendline(ys, 2, avg);
		Check(avg.size() == 4, "ComputeMovingAverageTrendline restituisce sempre out.size() == ys.size()");
		if (avg.size() == 4)
		{
			Check(!avg[0].valid, "il primo punto (period-1 = 1 punto mancante) non ha un valore valido");
			Check(avg[1].valid && fabs(avg[1].value - 15.0) < 1e-9, "media di (10,20) = 15");
			Check(avg[2].valid && fabs(avg[2].value - 25.0) < 1e-9, "media di (20,30) = 25");
			Check(avg[3].valid && fabs(avg[3].value - 35.0) < 1e-9, "media di (30,40) = 35");
		}
	}

	// --- Parte 3: MainWindow vera, insert/update/undo/redo --------------

	{
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

		std::vector<ChartSeriesOptions> chosenOptions(2);
		chosenOptions[1].trendlineType = eLinearTrendline;

		win->HandleChartInsert("A1:C3", "E1", eBarChart, "Con trendline", /*rowOriented=*/false,
			std::vector<rgb_color>(), chosenOptions);
		Check(win->Charts().size() == 1, "HandleChartInsert con seriesOptions espliciti aggiunge un grafico");
		if (win->Charts().size() == 1)
		{
			const ChartObject& obj = win->Charts()[0];
			Check(obj.seriesOptions.size() == 2, "il grafico ricorda le 2 opzioni scelte");
			if (obj.seriesOptions.size() == 2)
			{
				Check(obj.seriesOptions[0].trendlineType == eNoTrendline,
					"la prima serie non ha trendline (come inviato)");
				Check(obj.seriesOptions[1].trendlineType == eLinearTrendline,
					"la seconda serie ha trendline lineare (come inviato)");
			}
		}

		std::vector<ChartSeriesOptions> newOptions(1);
		newOptions[0].trendlineType = eMovingAverageTrendline;
		newOptions[0].trendlinePeriod = 3;
		win->HandleChartUpdate(0, "A1:C3", eLineChart, "Trendline aggiornata", /*rowOriented=*/false,
			std::vector<rgb_color>(), newOptions);
		Check(win->Charts()[0].seriesOptions.size() == 1,
			"HandleChartUpdate sostituisce interamente il vettore di opzioni");
		if (win->Charts()[0].seriesOptions.size() == 1)
		{
			Check(win->Charts()[0].seriesOptions[0].trendlineType == eMovingAverageTrendline,
				"la nuova opzione e' quella inviata (media mobile)");
			Check(win->Charts()[0].seriesOptions[0].trendlinePeriod == 3,
				"il periodo della media mobile e' quello inviato");
		}

		// Annullabile SENZA nessun codice nuovo per l'undo (stesso
		// principio gia' verificato per seriesColors in
		// test_chart_series_colors.cpp): il "prima" catturato e' l'intero
		// ChartObject.
		win->GetSheetView()->Undo();
		Check(win->Charts()[0].seriesOptions.size() == 2,
			"Annulla ripristina le 2 opzioni precedenti (stato prima dell'ultimo aggiornamento)");

		win->GetSheetView()->Redo();
		Check(win->Charts()[0].seriesOptions.size() == 1,
			"Ripristina riapplica l'aggiornamento a 1 opzione appena annullato");

		// Un normale HandleChartInsert senza seriesOptions (default vuoto)
		// continua a funzionare esattamente come prima di questa modifica.
		win->HandleChartInsert("A1:C3", "E10", eBarChart, "Grafico normale");
		Check(win->Charts().size() == 2, "un HandleChartInsert senza seriesOptions (default) continua a funzionare");
		if (win->Charts().size() == 2)
			Check(win->Charts()[1].seriesOptions.empty(),
				"il valore predefinito di seriesOptions resta vuoto quando non specificato");

		win->Unlock();

		win->Lock();
		win->Quit();
	}

	// --- Parte 4: round-trip AscdIO --------------------------------------

	{
		CContainer& chartDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), &chartDoc, true);

		std::vector<ChartObject> saved;
		ChartObject obj;
		obj.dataRange.Set(1, 1, 2, 5);
		obj.frame.Set(100, 200, 400, 380);
		ChartSeriesOptions opt0; // di default, nessuna opzione
		ChartSeriesOptions opt1;
		opt1.trendlineType = eLinearTrendline;
		ChartSeriesOptions opt2;
		opt2.trendlineType = eMovingAverageTrendline;
		opt2.trendlinePeriod = 4;
		obj.seriesOptions.push_back(opt0);
		obj.seriesOptions.push_back(opt1);
		obj.seriesOptions.push_back(opt2);
		saved.push_back(obj);

		// Un secondo grafico senza nessuna opzione: deve restare vuoto
		// dopo il giro, non ereditare quelle del primo.
		ChartObject obj2;
		obj2.dataRange.Set(1, 1, 2, 3);
		obj2.frame.Set(50, 50, 200, 150);
		saved.push_back(obj2);

		BFile chartFile("tests/roundtrip_chart_trendlines.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCD(&chartDoc, &chartFile, &saved);
		Check(err == B_OK, "SaveASCD con opzioni per serie riesce");
		chartDoc.Release();

		BFile chartReopened("tests/roundtrip_chart_trendlines.ascd", B_READ_ONLY);
		CContainer& chartReloaded = *new CContainer(NULL, NULL);
		std::vector<ChartObject> loaded;
		err = LoadASCD(&chartReopened, &chartReloaded, &loaded);
		Check(err == B_OK, "LoadASCD con opzioni per serie riesce");
		Check(loaded.size() == 2, "entrambi i grafici sopravvivono al giro salva->ricarica");
		if (loaded.size() == 2)
		{
			Check(loaded[0].seriesOptions.size() == 3,
				"le 3 opzioni del primo grafico sopravvivono");
			if (loaded[0].seriesOptions.size() == 3)
			{
				Check(loaded[0].seriesOptions[0].trendlineType == eNoTrendline,
					"la prima opzione (di default) resta senza trendline");
				Check(loaded[0].seriesOptions[1].trendlineType == eLinearTrendline,
					"la seconda opzione (lineare) e' preservata per intero");
				Check(loaded[0].seriesOptions[2].trendlineType == eMovingAverageTrendline
						&& loaded[0].seriesOptions[2].trendlinePeriod == 4,
					"la terza opzione (media mobile, periodo 4) e' preservata per intero");
			}
			Check(loaded[1].seriesOptions.empty(),
				"il secondo grafico (senza opzioni) resta vuoto, non eredita quelle del primo");
		}
		chartReloaded.Release();

		// --- Compatibilita' con file scritti PRIMA di questa modifica ---
		//
		// La sezione opzioni per serie di grafico e' l'ULTIMA cosa
		// scritta da SaveASCD ad oggi (subito dopo il byte hasStyles/
		// hasTheme della Fase "Named cell styles + live theme palette").
		// Per un file con un solo grafico SENZA opzioni, senza scenari,
		// senza celle con stile con nome e senza tema, la coda del file
		// e': 4 byte (conteggio scenari, 0) + 4 byte (conteggio celle con
		// stile con nome, 0) + 1 byte (hasStyles, 0) + 1 byte (hasTheme,
		// 0) + 4 byte (conteggio grafici di QUESTA sezione, 1: "charts"
		// non e' NULL in questa chiamata) + 4 byte (optionCount di
		// quell'unico grafico, 0: nessuna opzione impostata) = 18 byte in
		// tutto. Troncare questi 18 byte finali equivale quindi a un file
		// scritto da una build PRIMA che questa sezione esistesse -- prova
		// CONCRETA (non solo un ragionamento sul codice) che LoadASCD
		// resta EOF-tollerante con un file vecchio. NOTA per la prossima
		// fase (7c/7d) che aggiunge una sezione in coda: questo conteggio
		// andra' aggiornato di nuovo -- salvo che 7c/7d riusino i campi
		// gia' riservati in QUESTA sezione (vedi il commento in
		// AscdIO.cpp), nel qual caso resta invariato.
		CContainer& oldDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), &oldDoc, true);
		std::vector<ChartObject> oldSaved;
		ChartObject oldObj;
		oldObj.dataRange.Set(1, 1, 2, 5);
		oldObj.frame.Set(10, 10, 100, 100);
		oldSaved.push_back(oldObj);

		BFile oldFile("tests/roundtrip_chart_trendlines_old.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		err = SaveASCD(&oldDoc, &oldFile, &oldSaved);
		Check(err == B_OK, "SaveASCD di riferimento (un grafico, nessuna opzione) riesce");
		oldDoc.Release();

		off_t size = 0;
		oldFile.GetSize(&size);
		Check(size > 18, "il file di riferimento e' abbastanza grande da poter troncare 18 byte");
		oldFile.SetSize(size - 18);

		BFile oldReopened("tests/roundtrip_chart_trendlines_old.ascd", B_READ_ONLY);
		CContainer& oldReloaded = *new CContainer(NULL, NULL);
		std::vector<ChartObject> oldLoaded;
		err = LoadASCD(&oldReopened, &oldReloaded, &oldLoaded);
		Check(err == B_OK, "un file troncato (che simula una build precedente a questa sezione) si carica comunque");
		Check(oldLoaded.size() == 1, "il grafico del file troncato sopravvive comunque");
		if (oldLoaded.size() == 1)
			Check(oldLoaded[0].seriesOptions.empty(),
				"un file senza questa sezione (simulato) restituisce seriesOptions vuoto, non un errore");
		oldReloaded.Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
