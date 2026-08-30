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

	// BuildMultiChartSeries (Fase 17, serie multiple): un intervallo con
	// PIU' di due colonne, la prima le etichette di categoria, le
	// successive una per serie -- stesso principio di BuildChartSeries
	// ma esteso a N colonne di valori invece di una sola. Colonne 5-7
	// (non ancora usate sopra in questo documento) per non sporcare i
	// dati A1:B4 dei test precedenti.
	doc.NewCell(cell(5, 1), Value("Gen"), NULL);
	doc.NewCell(cell(6, 1), Value(10.0), NULL);
	doc.NewCell(cell(7, 1), Value(15.0), NULL);
	doc.NewCell(cell(5, 2), Value("Feb"), NULL);
	doc.NewCell(cell(6, 2), Value(20.0), NULL);
	doc.NewCell(cell(7, 2), Value(25.0), NULL);
	doc.NewCell(cell(5, 3), Value("Mar"), NULL);
	doc.NewCell(cell(6, 3), Value(30.0), NULL);
	doc.NewCell(cell(7, 3), Value(10.0), NULL);

	MultiChartData multi;
	range threeColRange(5, 1, 7, 3);
	bool multiOk = BuildMultiChartSeries(&doc, threeColRange, multi);
	Check(multiOk, "BuildMultiChartSeries riesce su un intervallo a tre colonne (etichette + 2 serie)");
	Check(multi.categories.size() == 3, "estrae le 3 categorie");
	Check(multi.seriesNames.size() == 2, "estrae le 2 serie (una per colonna valori)");
	if (multi.categories.size() == 3 && multi.seriesNames.size() == 2)
	{
		Check(multi.categories[0] == "Gen" && multi.categories[1] == "Feb" && multi.categories[2] == "Mar",
			"le categorie sono nell'ordine delle righe");
		Check(multi.seriesNames[0] == "Serie 1" && multi.seriesNames[1] == "Serie 2",
			"le serie senza nome esplicito si chiamano Serie 1, Serie 2, ... in ordine di colonna");
		Check(multi.values[0][0] == 10.0 && multi.values[0][1] == 20.0 && multi.values[0][2] == 30.0,
			"i valori della prima serie sono nell'ordine delle righe");
		Check(multi.values[1][0] == 15.0 && multi.values[1][1] == 25.0 && multi.values[1][2] == 10.0,
			"i valori della seconda serie sono nell'ordine delle righe, non mescolati con la prima");
	}

	// Una riga con un valore non numerico in una QUALSIASI colonna
	// serie viene saltata per intero, non solo per quella serie --
	// altrimenti l'indice categoria/valore si disallineerebbe tra le
	// serie.
	doc.NewCell(cell(5, 4), Value("Apr"), NULL);
	doc.NewCell(cell(6, 4), Value(40.0), NULL);
	// col 7 riga 4 lasciata vuota (nessun valore) apposta
	MultiChartData multiWithGap;
	range fourRowRange(5, 1, 7, 4);
	BuildMultiChartSeries(&doc, fourRowRange, multiWithGap);
	Check(multiWithGap.categories.size() == 3,
		"una riga con un valore mancante in una sola serie viene saltata per intero, non genera una quarta categoria");

	// Riga di intestazione (Fase 18): se la prima riga dell'intervallo
	// ha un testo in una colonna serie, quel testo diventa il nome
	// della serie e i dati partono dalla riga successiva -- stessa
	// convenzione di Excel, colonne 10-12 per non toccare i dati usati
	// sopra.
	doc.NewCell(cell(11, 1), Value("Vendite"), NULL);
	doc.NewCell(cell(12, 1), Value("Costi"), NULL);
	doc.NewCell(cell(10, 2), Value("Gen"), NULL);
	doc.NewCell(cell(11, 2), Value(100.0), NULL);
	doc.NewCell(cell(12, 2), Value(40.0), NULL);
	doc.NewCell(cell(10, 3), Value("Feb"), NULL);
	doc.NewCell(cell(11, 3), Value(120.0), NULL);
	doc.NewCell(cell(12, 3), Value(50.0), NULL);

	MultiChartData multiHeader;
	range headerRange(10, 1, 12, 3);
	bool headerOk = BuildMultiChartSeries(&doc, headerRange, multiHeader);
	Check(headerOk, "BuildMultiChartSeries riesce su un intervallo con riga di intestazione");
	Check(multiHeader.categories.size() == 2,
		"la riga di intestazione non viene contata come categoria (solo Gen/Feb, non 3 righe)");
	if (multiHeader.categories.size() == 2 && multiHeader.seriesNames.size() == 2)
	{
		Check(multiHeader.categories[0] == "Gen" && multiHeader.categories[1] == "Feb",
			"le categorie partono dalla riga DOPO l'intestazione");
		Check(multiHeader.seriesNames[0] == "Vendite" && multiHeader.seriesNames[1] == "Costi",
			"i nomi delle serie sono presi dal testo nella riga di intestazione, non \"Serie 1\"/\"Serie 2\"");
		Check(multiHeader.values[0][0] == 100.0 && multiHeader.values[0][1] == 120.0,
			"i valori della serie \"Vendite\" sono corretti (l'intestazione non e' stata letta come dato)");
		Check(multiHeader.values[1][0] == 40.0 && multiHeader.values[1][1] == 50.0,
			"i valori della serie \"Costi\" sono corretti");
	}

	// Un'intestazione parziale (una sola colonna serie con testo, non
	// tutte) nomina solo quella colonna, l'altra resta "Serie N" --
	// non tutte le serie devono avere per forza un nome esplicito.
	doc.NewCell(cell(14, 1), Value("Vendite"), NULL);
	// col 15 riga 1 lasciata numerica/vuota apposta (nessuna intestazione)
	doc.NewCell(cell(13, 2), Value("Gen"), NULL);
	doc.NewCell(cell(14, 2), Value(100.0), NULL);
	doc.NewCell(cell(15, 2), Value(40.0), NULL);

	MultiChartData multiPartialHeader;
	range partialHeaderRange(13, 1, 15, 2);
	BuildMultiChartSeries(&doc, partialHeaderRange, multiPartialHeader);
	if (multiPartialHeader.seriesNames.size() == 2)
	{
		Check(multiPartialHeader.seriesNames[0] == "Vendite",
			"la colonna con intestazione testuale usa quel nome");
		Check(multiPartialHeader.seriesNames[1] == "Serie 2",
			"la colonna senza intestazione testuale propria resta \"Serie 2\", non eredita quella della prima");
	}

	// Un intervallo di una sola colonna (nessuna colonna serie) viene
	// rifiutato esplicitamente.
	MultiChartData multiBadShape;
	range oneColumn(5, 1, 5, 3);
	Check(!BuildMultiChartSeries(&doc, oneColumn, multiBadShape),
		"un intervallo di una sola colonna (nessuna serie) viene rifiutato");

	// ComputeGroupedBarLayout: due serie, tre categorie -- una barra
	// per (serie, categoria), ordinate correttamente.
	GroupedBarLayout grouped;
	BRect groupedBounds(0, 0, 120, 40);
	ComputeGroupedBarLayout(multi, groupedBounds, grouped);
	Check(grouped.bars.size() == 2, "ComputeGroupedBarLayout produce un vettore di barre per serie");
	if (grouped.bars.size() == 2)
	{
		Check(grouped.bars[0].size() == 3 && grouped.bars[1].size() == 3,
			"ogni serie ha una barra per categoria");
		Check(grouped.bars[0][0].left < grouped.bars[1][0].left,
			"dentro lo stesso gruppo di categoria, la barra della serie 0 sta a sinistra di quella della serie 1");
		Check(grouped.bars[0][0].right <= grouped.bars[0][1].left,
			"le barre di categorie diverse (stessa serie) non si sovrappongono");
		// La serie 0 a Mar (30) e' il valore assoluto piu' alto di
		// tutto il grafico: deve risultare piu' alta della serie 1
		// alla stessa categoria (10), sulla stessa scala comune.
		Check(grouped.bars[0][2].Height() > grouped.bars[1][2].Height(),
			"la barra col valore maggiore (serie 0, Mar=30) e' piu' alta di quella con valore minore nello stesso gruppo (serie 1, Mar=10)");
	}

	// ComputeMultiLineLayout: stesso principio, un punto per (serie,
	// categoria) invece di una barra.
	MultiLinePoint multiLine;
	ComputeMultiLineLayout(multi, groupedBounds, multiLine);
	Check(multiLine.points.size() == 2, "ComputeMultiLineLayout produce un vettore di punti per serie");
	if (multiLine.points.size() == 2)
	{
		Check(multiLine.points[0].size() == 3 && multiLine.points[1].size() == 3,
			"ogni serie ha un punto per categoria");
		Check(multiLine.points[0][0].x == multiLine.points[1][0].x,
			"lo stesso indice di categoria cade sulla stessa X per tutte le serie (stesso asse a categorie)");
	}

	// BuildScatterSeries (dispersione/XY): esattamente due colonne
	// NUMERICHE, X e Y sulla stessa riga, colonne 17-18 per non
	// toccare i dati usati sopra.
	doc.NewCell(cell(17, 1), Value(1.0), NULL);
	doc.NewCell(cell(18, 1), Value(10.0), NULL);
	doc.NewCell(cell(17, 2), Value(2.0), NULL);
	doc.NewCell(cell(18, 2), Value(30.0), NULL);
	doc.NewCell(cell(17, 3), Value(3.0), NULL);
	doc.NewCell(cell(18, 3), Value(20.0), NULL);

	std::vector<ScatterPoint> scatter;
	range scatterRange(17, 1, 18, 3);
	bool scatterOk = BuildScatterSeries(&doc, scatterRange, scatter);
	Check(scatterOk, "BuildScatterSeries riesce su un intervallo A1:B3 con due colonne numeriche");
	Check(scatter.size() == 3, "estrae le 3 coppie (x,y)");
	if (scatter.size() == 3)
	{
		Check(scatter[0].x == 1.0 && scatter[0].y == 10.0, "prima coppia: (1,10)");
		Check(scatter[1].x == 2.0 && scatter[1].y == 30.0, "seconda coppia: (2,30)");
		Check(scatter[2].x == 3.0 && scatter[2].y == 20.0, "terza coppia: (3,20)");
	}

	// Una riga con X o Y non numerico viene saltata, non causa un
	// errore ne' una coppia spazzatura -- stessa convenzione di
	// BuildChartSeries.
	doc.NewCell(cell(17, 4), Value("testo"), NULL);
	doc.NewCell(cell(18, 4), Value(99.0), NULL);
	std::vector<ScatterPoint> scatterWithGap;
	range scatterFourRows(17, 1, 18, 4);
	BuildScatterSeries(&doc, scatterFourRows, scatterWithGap);
	Check(scatterWithGap.size() == 3,
		"una riga con X non numerico viene saltata, non genera una quarta coppia");

	// Un intervallo con una sola colonna o con tre colonne non e' il
	// formato atteso (X, Y) e viene rifiutato esplicitamente.
	std::vector<ScatterPoint> scatterBadShape;
	range scatterOneColumn(17, 1, 17, 3);
	Check(!BuildScatterSeries(&doc, scatterOneColumn, scatterBadShape),
		"un intervallo di una sola colonna viene rifiutato (serve X e Y)");
	Check(!BuildScatterSeries(&doc, threeColumns, scatterBadShape),
		"un intervallo con tre colonne viene rifiutato (dispersione vuole esattamente X, Y)");

	// ComputeScatterLayout: puro calcolo geometrico, i punti mantengono
	// l'ordine dei dati (non vengono riordinati per X o Y).
	std::vector<BPoint> scatterPoints;
	BRect scatterBounds(0, 0, 100, 50);
	ComputeScatterLayout(scatter, scatterBounds, scatterPoints);

	Check(scatterPoints.size() == 3, "ComputeScatterLayout produce un punto per coppia");
	if (scatterPoints.size() == 3)
	{
		Check(scatterPoints[0].x < scatterPoints[1].x && scatterPoints[1].x < scatterPoints[2].x,
			"le coordinate X crescono con il valore X dei dati (1 < 2 < 3), l'ordine non e' quello dei dati ma quello dei valori");
		Check(scatterPoints[1].y < scatterPoints[0].y && scatterPoints[1].y < scatterPoints[2].y,
			"il punto col valore Y maggiore (30, il secondo) sta piu' in alto (y minore) degli altri due (10 e 20)");
	}

	std::vector<BPoint> emptyScatterPoints;
	std::vector<ScatterPoint> noScatterData;
	ComputeScatterLayout(noScatterData, scatterBounds, emptyScatterPoints);
	Check(emptyScatterPoints.empty(), "ComputeScatterLayout su dati vuoti non produce punti");

	// Un solo punto (X e Y entrambi degeneri, min == max su entrambi
	// gli assi): non deve dividere per zero, il punto risultante deve
	// restare dentro bounds.
	std::vector<ScatterPoint> singleScatter;
	ScatterPoint singlePoint; singlePoint.x = 5.0; singlePoint.y = 5.0;
	singleScatter.push_back(singlePoint);
	std::vector<BPoint> singleScatterLayout;
	ComputeScatterLayout(singleScatter, scatterBounds, singleScatterLayout);
	Check(singleScatterLayout.size() == 1, "ComputeScatterLayout con un solo punto non va in errore");
	if (singleScatterLayout.size() == 1)
	{
		Check(scatterBounds.Contains(singleScatterLayout[0]),
			"un solo punto (intervallo degenere su entrambi gli assi) resta dentro bounds");
	}

	// ComputeComboLayout (barre+linee): la serie 0 diventa barre, le
	// successive linee -- riusa il "multi" a due serie/tre categorie
	// gia' costruito sopra per ComputeGroupedBarLayout/
	// ComputeMultiLineLayout.
	ComboLayout combo;
	ComputeComboLayout(multi, groupedBounds, combo);
	Check(combo.bars.size() == 3, "ComputeComboLayout produce una barra per categoria (solo serie 0)");
	Check(combo.lines.size() == 1, "ComputeComboLayout produce una linea per le serie RESTANTI (2 serie totali - 1 a barre = 1 linea)");
	if (combo.lines.size() == 1)
		Check(combo.lines[0].size() == 3, "la linea (serie 1) ha un punto per categoria");
	if (combo.bars.size() == 3 && combo.lines.size() == 1 && combo.lines[0].size() == 3)
	{
		Check(combo.bars[0].left < combo.bars[1].left && combo.bars[1].left < combo.bars[2].left,
			"le barre (serie 0) sono ordinate da sinistra a destra come le categorie");
		Check(combo.lines[0][0].x < combo.lines[0][1].x && combo.lines[0][1].x < combo.lines[0][2].x,
			"i punti della linea (serie 1) sono ordinati da sinistra a destra come le categorie");
		// La serie 0 a Mar vale 30 (il massimo assoluto di tutto il
		// grafico, stessa scala comune di ComputeGroupedBarLayout sopra):
		// la barra deve toccare il bordo superiore dell'area.
		Check(combo.bars[2].top == groupedBounds.top,
			"la barra col valore massimo di tutto il grafico (serie 0, Mar=30) tocca il bordo superiore dell'area, stessa scala comune della linea");
		// Stessa X di centro-categoria condivisa tra barre e linee, cosi'
		// restano allineate verticalmente sullo stesso asse.
		float barCenterX = (combo.bars[0].left + combo.bars[0].right) / 2;
		Check(barCenterX == combo.lines[0][0].x,
			"la barra e il punto della linea alla STESSA categoria condividono la stessa X di centro-slot");
	}

	// Una sola serie (nessuna colonna aggiuntiva oltre le etichette):
	// nessuna linea da disegnare, degenera in un grafico a sole barre
	// -- non deve andare in errore ne' produrre un vettore lines vuoto
	// ma di dimensione sbagliata.
	MultiChartData singleSeries;
	singleSeries.categories = multi.categories;
	singleSeries.seriesNames.push_back(multi.seriesNames[0]);
	singleSeries.values.push_back(multi.values[0]);
	ComboLayout comboSingle;
	ComputeComboLayout(singleSeries, groupedBounds, comboSingle);
	Check(comboSingle.bars.size() == 3, "ComputeComboLayout con una sola serie produce comunque le barre");
	Check(comboSingle.lines.empty(), "ComputeComboLayout con una sola serie non produce nessuna linea");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
