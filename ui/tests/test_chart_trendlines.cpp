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
*/

#include <cstdio>

#include <Application.h>

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

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
