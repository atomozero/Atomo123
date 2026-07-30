/*
	test_chart.cpp

	Verifica la logica del grafico a barre (Chart.h/.cpp) senza
	nessuna sessione grafica: costruisce un documento headless (come
	engine/tests/smoke_test.cpp), inserisce dati a due colonne e
	controlla che BuildChartSeries li estragga correttamente, poi
	verifica ComputeBarLayout (puro calcolo geometrico, nessun
	BView/Draw) con dei BRect di prova.
*/

#include <cstdio>

#include "Cell.h"
#include "Container.h"
#include "Range.h"
#include "Value.h"
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
	CContainer& doc = *new CContainer(NULL, NULL);

	doc.NewCell(cell(1, 1), Value("Gen"), NULL);
	doc.NewCell(cell(2, 1), Value(10.0), NULL);
	doc.NewCell(cell(1, 2), Value("Feb"), NULL);
	doc.NewCell(cell(2, 2), Value(20.0), NULL);
	doc.NewCell(cell(1, 3), Value("Mar"), NULL);
	doc.NewCell(cell(2, 3), Value(30.0), NULL);

	std::vector<ChartSeries> series;
	range twoColumns(1, 1, 2, 3);
	bool ok = BuildChartSeries(&doc, twoColumns, series);

	Check(ok, "BuildChartSeries riesce su un intervallo A1:B3 valido");
	Check(series.size() == 3, "estrae le 3 righe");
	if (series.size() == 3)
	{
		Check(series[0].label == "Gen" && series[0].value == 10.0,
			"prima riga: Gen/10");
		Check(series[1].label == "Feb" && series[1].value == 20.0,
			"seconda riga: Feb/20");
		Check(series[2].label == "Mar" && series[2].value == 30.0,
			"terza riga: Mar/30");
	}

	// Una riga con valore non numerico (o assente) viene saltata, non
	// causa un errore ne' un valore spazzatura.
	doc.NewCell(cell(1, 4), Value("Apr"), NULL);
	std::vector<ChartSeries> withGap;
	range fourRows(1, 1, 2, 4);
	BuildChartSeries(&doc, fourRows, withGap);
	Check(withGap.size() == 3,
		"una riga con valore mancante viene saltata, non genera una quarta voce");

	// Intervallo con tre colonne: non e' il formato atteso (etichetta,
	// valore), deve fallire esplicitamente invece di leggere dati a
	// caso.
	std::vector<ChartSeries> badShape;
	range threeColumns(1, 1, 3, 3);
	Check(!BuildChartSeries(&doc, threeColumns, badShape),
		"un intervallo con tre colonne viene rifiutato");

	// ComputeBarLayout: puro calcolo geometrico, verificabile senza
	// nessuna vista/finestra reale.
	std::vector<ChartSeries> layoutData;
	ChartSeries a; a.label = "A"; a.value = 10;
	ChartSeries b; b.label = "B"; b.value = 20;
	layoutData.push_back(a);
	layoutData.push_back(b);

	std::vector<BarLayout> bars;
	BRect bounds(0, 0, 100, 50);
	ComputeBarLayout(layoutData, bounds, bars);

	Check(bars.size() == 2, "ComputeBarLayout produce una barra per voce");
	if (bars.size() == 2)
	{
		Check(bars[0].bar.left < bars[1].bar.left,
			"le barre sono ordinate da sinistra a destra come i dati");
		Check(bars[1].bar.Height() > bars[0].bar.Height(),
			"la barra col valore maggiore (B=20) e' piu' alta di quella con A=10");
		Check(bars[0].bar.bottom == bounds.bottom && bars[1].bar.bottom == bounds.bottom,
			"tutte le barre poggiano sulla stessa base");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
