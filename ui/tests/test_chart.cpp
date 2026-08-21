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

	// ComputeLineLayout (Fase 13): stesso principio di ComputeBarLayout
	// sopra, un punto invece di una barra.
	std::vector<LinePoint> points;
	ComputeLineLayout(layoutData, bounds, points);

	Check(points.size() == 2, "ComputeLineLayout produce un punto per voce");
	if (points.size() == 2)
	{
		Check(points[0].point.x < points[1].point.x,
			"i punti sono ordinati da sinistra a destra come i dati");
		Check(points[1].point.y < points[0].point.y,
			"il punto col valore maggiore (B=20) sta piu' in alto (y minore) di quello con A=10");
	}

	std::vector<LinePoint> emptyPoints;
	std::vector<ChartSeries> noData;
	ComputeLineLayout(noData, bounds, emptyPoints);
	Check(emptyPoints.empty(), "ComputeLineLayout su una serie vuota non produce punti");

	// ComputePieLayout (Fase 13): angoli proporzionali al peso di ogni
	// valore sul totale, non alla posizione nell'elenco.
	std::vector<ChartSeries> pieData;
	ChartSeries p1; p1.label = "P1"; p1.value = 25;
	ChartSeries p2; p2.label = "P2"; p2.value = 75;
	pieData.push_back(p1);
	pieData.push_back(p2);

	std::vector<PieSlice> slices;
	ComputePieLayout(pieData, slices);

	Check(slices.size() == 2, "ComputePieLayout produce uno spicchio per voce");
	if (slices.size() == 2)
	{
		Check(slices[0].startAngle == 0.0f,
			"il primo spicchio parte da 0 gradi");
		Check(slices[0].sweepAngle == 90.0f,
			"il primo spicchio (25 su 100) occupa 90 gradi (un quarto del cerchio)");
		Check(slices[1].startAngle == 90.0f,
			"il secondo spicchio inizia dove finisce il primo");
		Check(slices[1].sweepAngle == 270.0f,
			"il secondo spicchio (75 su 100) occupa 270 gradi (tre quarti)");
	}

	std::vector<PieSlice> emptySlices;
	ComputePieLayout(noData, emptySlices);
	Check(emptySlices.empty(), "ComputePieLayout su una serie vuota non produce spicchi");

	// Una serie con solo valori non positivi non ha nessuno spicchio
	// disegnabile (una torta non ha senso senza un totale positivo).
	std::vector<ChartSeries> negativeData;
	ChartSeries neg; neg.label = "Neg"; neg.value = -5;
	negativeData.push_back(neg);
	std::vector<PieSlice> negativeSlices;
	ComputePieLayout(negativeData, negativeSlices);
	Check(negativeSlices.empty(),
		"ComputePieLayout su una serie con solo valori non positivi non produce spicchi");

	// ComputeYAxisTicks: 5 tacche equidistanti da minValue a maxValue,
	// con coordinate Y decrescenti (minValue in basso, maxValue in
	// alto) dentro plotArea -- stesso principio geometrico di
	// ComputeBarLayout sopra.
	std::vector<AxisTick> ticks;
	BRect plotArea(0, 0, 100, 40);
	ComputeYAxisTicks(0.0, 20.0, plotArea, ticks);

	Check(ticks.size() == 5, "ComputeYAxisTicks produce 5 tacche (0, 1/4, 2/4, 3/4, max)");
	if (ticks.size() == 5)
	{
		Check(ticks[0].label == "0", "la prima tacca vale 0");
		Check(ticks[4].label == "20", "l'ultima tacca vale il massimo");
		Check(ticks[0].y == plotArea.bottom,
			"la tacca 0 sta sul bordo inferiore di plotArea");
		Check(ticks[4].y == plotArea.top,
			"la tacca del massimo sta sul bordo superiore di plotArea");
		Check(ticks[2].label == "10", "la tacca di mezzo vale la meta' del massimo");
		Check(ticks[0].y > ticks[2].y && ticks[2].y > ticks[4].y,
			"le coordinate Y decrescono salendo verso il massimo");
	}

	// Un intervallo degenere (minValue == maxValue, es. serie tutta a
	// zero o vuota) non deve dividere per zero: allarga maxValue di 1
	// di riserva, stesso principio di ChartValueRange in
	// ComputeBarLayout/ComputeLineLayout.
	std::vector<AxisTick> zeroTicks;
	ComputeYAxisTicks(0.0, 0.0, plotArea, zeroTicks);
	Check(zeroTicks.size() == 5,
		"ComputeYAxisTicks con minValue == maxValue non va in errore, produce comunque 5 tacche");

	// ComputeYAxisTicks con un intervallo che scende sotto zero (serie
	// con valori negativi): la prima tacca deve valere il minimo
	// negativo, non 0 -- l'asse deve mostrare anche la parte negativa
	// della scala, non tagliarla via.
	std::vector<AxisTick> negativeTicks;
	ComputeYAxisTicks(-10.0, 10.0, plotArea, negativeTicks);
	Check(negativeTicks.size() == 5, "ComputeYAxisTicks produce 5 tacche anche con un minimo negativo");
	if (negativeTicks.size() == 5)
	{
		Check(negativeTicks[0].label == "-10", "la prima tacca vale il minimo negativo, non 0");
		Check(negativeTicks[2].label == "0", "la tacca di mezzo vale 0 quando l'intervallo e' simmetrico");
		Check(negativeTicks[4].label == "10", "l'ultima tacca vale il massimo");
	}

	// ComputeBarLayout con valori sia positivi che negativi: la barra
	// negativa deve scendere SOTTO la linea di zero (bottom > zeroY),
	// non finire fuori dall'area disegnabile come prima di questo fix.
	std::vector<ChartSeries> mixedData;
	ChartSeries pos; pos.label = "Pos"; pos.value = 10;
	ChartSeries neg2; neg2.label = "Neg"; neg2.value = -10;
	mixedData.push_back(pos);
	mixedData.push_back(neg2);

	std::vector<BarLayout> mixedBars;
	BRect mixedBounds(0, 0, 100, 40);
	ComputeBarLayout(mixedData, mixedBounds, mixedBars);

	Check(mixedBars.size() == 2, "ComputeBarLayout produce una barra per voce anche con valori misti");
	if (mixedBars.size() == 2)
	{
		Check(mixedBars[0].bar.top < mixedBars[0].bar.bottom,
			"la barra positiva e' un rettangolo valido (top sopra bottom)");
		Check(mixedBars[1].bar.top < mixedBars[1].bar.bottom,
			"la barra negativa e' un rettangolo valido (top sopra bottom), non invertito");
		Check(mixedBars[0].bar.bottom == mixedBars[1].bar.top,
			"le due barre (valori opposti e simmetrici) si toccano esattamente sulla linea di zero");
		Check(mixedBars[0].bar.top == mixedBounds.top,
			"la barra positiva massima tocca il bordo superiore dell'area");
		Check(mixedBars[1].bar.bottom == mixedBounds.bottom,
			"la barra negativa minima tocca il bordo inferiore dell'area");
	}

	// ComputeLineLayout con lo stesso caso misto: il punto negativo
	// deve scendere sotto il punto a valore zero (se ci fosse), non
	// finire fuori da bounds.
	std::vector<LinePoint> mixedPoints;
	ComputeLineLayout(mixedData, mixedBounds, mixedPoints);
	Check(mixedPoints.size() == 2, "ComputeLineLayout produce un punto per voce anche con valori misti");
	if (mixedPoints.size() == 2)
	{
		Check(mixedPoints[0].point.y == mixedBounds.top,
			"il punto col valore positivo massimo tocca il bordo superiore dell'area");
		Check(mixedPoints[1].point.y == mixedBounds.bottom,
			"il punto col valore negativo minimo tocca il bordo inferiore dell'area");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
