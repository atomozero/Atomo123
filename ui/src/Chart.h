/*
	Chart.h

	Logica del grafico a barre, separata dal disegno (ChartView) e
	dalla finestra (ChartWindow) cosi' da poter essere testata senza
	una sessione grafica -- stesso principio gia' seguito per
	SheetView::ScrollToShowSelection (vedi ui/tests/test_scroll.cpp)
	e per le funzioni con nome (engine/tests/named_functions_test.cpp).
*/

#ifndef CHART_H
#define CHART_H

#include <vector>

#include <Rect.h>
#include <String.h>

#include "Range.h"

class BView;
class CContainer;

struct ChartSeries {
	BString label;
	double value;
};

// Tipo di grafico (Fase 13): eBarChart resta il predefinito numerico
// (0) apposta -- un ChartObject letto da un file .ascd scritto PRIMA
// di questa modifica (che aveva solo grafici a barre, nessun campo
// tipo nel formato) deve continuare a disegnarsi come una barra senza
// bisogno di nessuna sezione nel file, vedi AscdIO.cpp.
enum ChartType {
	eBarChart = 0,
	eLineChart = 1,
	ePieChart = 2
};

// Un grafico incorporato nel foglio (vedi SheetView::Draw): posizione
// fissa in pixel nello stesso sistema di coordinate delle celle
// (CellRect), dati letti dal vivo da "dataRange" a ogni ridisegno
// (non un'istantanea statica come nella sola finestra di anteprima
// ChartWindow) -- cosi' modificando i dati sorgente il grafico
// incorporato si aggiorna da solo.
struct ChartObject {
	ChartObject() : type(eBarChart) {}

	range dataRange;
	BRect frame;
	ChartType type;
};

// L'intervallo deve avere esattamente due colonne: la prima con le
// etichette (categoria), la seconda con i valori numerici -- una
// riga alla volta. Righe con un valore non numerico nella seconda
// colonna vengono saltate. Restituisce false se l'intervallo non ha
// due colonne o se non risulta nessuna riga valida.
bool BuildChartSeries(CContainer* doc, const range& r,
	std::vector<ChartSeries>& out);

struct BarLayout {
	BRect bar;
};

// Calcola il rettangolo di ogni barra dentro "bounds", scalato al
// valore massimo della serie. Funzione pura (nessun BView/Draw),
// cosi' e' verificabile con un test headless.
void ComputeBarLayout(const std::vector<ChartSeries>& data, BRect bounds,
	std::vector<BarLayout>& out);

// Disegna il grafico dentro "frame" su "view" (assi, barre, etichette)
// a partire da dati gia' estratti -- mai un CContainer, cosi' la
// stessa funzione serve sia a ChartView (dati ricevuti via BMessage
// da un'altra finestra, vedi ChartWindow.h) sia a SheetView (dati
// letti dal vivo sul proprio thread, che possiede il documento).
void DrawBarChart(BView* view, BRect frame, const std::vector<ChartSeries>& data);

struct LinePoint {
	BPoint point;
};

// Calcola il punto di ogni valore della serie dentro "bounds", scalato
// come ComputeBarLayout -- stessa funzione pura verificabile senza
// BView/Draw.
void ComputeLineLayout(const std::vector<ChartSeries>& data, BRect bounds,
	std::vector<LinePoint>& out);

void DrawLineChart(BView* view, BRect frame, const std::vector<ChartSeries>& data);

struct PieSlice {
	float startAngle;	// gradi, 0 = ore 3, senso antiorario (convenzione BView::FillArc)
	float sweepAngle;	// gradi, sempre positivo
};

// Calcola l'angolo di partenza e l'ampiezza di ogni spicchio,
// proporzionale al peso di ogni valore sul totale della serie --
// stessa funzione pura verificabile senza BView/Draw. Una serie con
// somma <= 0 (nessun valore positivo) produce un vettore vuoto: una
// torta non ha senso con valori negativi o tutti nulli.
void ComputePieLayout(const std::vector<ChartSeries>& data,
	std::vector<PieSlice>& out);

void DrawPieChart(BView* view, BRect frame, const std::vector<ChartSeries>& data);

// Smista verso DrawBarChart/DrawLineChart/DrawPieChart secondo "type"
// -- unico punto di ingresso condiviso da ChartView e SheetView (vedi
// i commenti su DrawBarChart sopra), cosi' aggiungere un futuro nuovo
// tipo di grafico tocca un solo punto di dispatch.
void DrawChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	ChartType type);

#endif
