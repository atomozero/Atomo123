/*
	Chart.h

	Logica del grafico a barre, separata dal disegno (ChartView) e
	dalla finestra (ChartWindow) cosi' da poter essere testata senza
	una sessione grafica -- stesso principio gia' seguito per
	SheetView::ScrollToShowSelection (vedi ui/tests/test_scroll.cpp)
	e per le funzioni con nome (engine/tests/named_functions_test.cpp).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
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
	ePieChart = 2,
	// Fase 35, "Path to full Excel parity" Tier 2, "More chart types":
	// appesi in coda, mai in mezzo -- lo stesso principio di eBarChart=0
	// sopra, un file .ascd scritto prima di un nuovo tipo non deve mai
	// vedere un valore esistente cambiare significato.
	eAreaChart = 3,
	eScatterChart = 4
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
	// Opzionale (Fase 17): un file .ascd scritto prima di questo campo
	// non ce l'ha, resta vuoto (nessun titolo disegnato) -- vedi la
	// sezione dedicata, EOF-tollerante, in AscdIO.cpp.
	BString title;
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

struct AxisTick {
	float y;		// coordinata Y dentro plotArea
	BString label;	// valore gia' formattato (stesso formato "%g" di ValueToLabel)
};

// Tacche dell'asse Y: valori equidistanti da minValue a maxValue,
// convertiti in coordinate Y dentro plotArea. Funzione pura (nessun
// BView/Draw), usata sia per disegnare la griglia sia per misurare la
// larghezza delle etichette prima di riservare il margine sinistro del
// grafico -- vedi DrawBarChart/DrawLineChart. minValue e' quasi sempre
// 0, tranne quando la serie ha valori negativi (vedi ChartValueRange
// in Chart.cpp): le tacche/etichette scendono allora sotto zero invece
// di ignorare la parte negativa della scala.
void ComputeYAxisTicks(double minValue, double maxValue, BRect plotArea,
	std::vector<AxisTick>& out);

// Disegna la griglia orizzontale (linee chiare) e le etichette numeriche
// a sinistra di plotArea -- condivisa da barre e linee, che condividono
// lo stesso asse a valori (a differenza della torta, che non ne ha uno).
void DrawYAxisGrid(BView* view, BRect plotArea, double minValue, double maxValue);

// Calcola il rettangolo di ogni barra dentro "bounds", scalato
// all'intervallo di valori della serie (vedi ComputeYAxisTicks sopra):
// un valore negativo produce una barra che scende sotto la linea dello
// zero invece che sopra, non una barra fuori dall'area disegnabile.
// Funzione pura (nessun BView/Draw), cosi' e' verificabile con un test
// headless.
void ComputeBarLayout(const std::vector<ChartSeries>& data, BRect bounds,
	std::vector<BarLayout>& out);

// Disegna il grafico dentro "frame" su "view" (assi, barre, etichette)
// a partire da dati gia' estratti -- mai un CContainer, cosi' la
// stessa funzione serve sia a ChartView (dati ricevuti via BMessage
// da un'altra finestra, vedi ChartWindow.h) sia a SheetView (dati
// letti dal vivo sul proprio thread, che possiede il documento).
// "title" e' opzionale (vedi ChartObject::title): una stringa vuota non
// disegna nulla e non riserva spazio in piu' rispetto a prima.
void DrawBarChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	const BString& title = BString());

struct LinePoint {
	BPoint point;
};

// Calcola il punto di ogni valore della serie dentro "bounds", scalato
// come ComputeBarLayout -- stessa funzione pura verificabile senza
// BView/Draw.
void ComputeLineLayout(const std::vector<ChartSeries>& data, BRect bounds,
	std::vector<LinePoint>& out);

void DrawLineChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	const BString& title = BString());

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

void DrawPieChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	const BString& title = BString());

// Grafico ad area (Fase 35): la stessa spezzata di DrawLineChart (vedi
// ComputeLineLayout, riusata cosi' com'e', nessuna nuova geometria pura
// da calcolare), ma con la zona fra la linea e l'asse zero riempita di
// colore -- stesso asse Y/stessa scala di barre e linee. Un valore
// negativo riempie sotto lo zero fino al punto, mai sopra: coerente con
// come ComputeLineLayout gia' posiziona un punto negativo.
void DrawAreaChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	const BString& title = BString());

// Grafico a dispersione/XY (Fase 35): a differenza di TUTTI i tipi
// sopra (una categoria testuale + un valore), qui ENTRAMBE le colonne
// dell'intervallo sono numeriche -- niente asse a categorie, un vero
// asse X a valori come quello Y. Non condivide ChartSeries/
// BuildChartSeries per questo motivo: un punto e' una coppia (x, y),
// non un'etichetta con un valore. Percorso di disegno completamente a
// parte in DrawChart e nei chiamanti (ChartView/SheetView), mai
// mescolato con gli altri tipi.
struct ScatterPoint {
	double x, y;
};

// L'intervallo deve avere esattamente due colonne, ENTRAMBE numeriche
// riga per riga (a differenza di BuildChartSeries, dove la prima e'
// sempre un'etichetta testuale) -- una riga con un valore non
// numerico in una delle due colonne viene saltata, stesso principio
// permissivo di BuildChartSeries. Restituisce false se l'intervallo
// non ha due colonne o se non risulta nessuna riga valida.
bool BuildScatterSeries(CContainer* doc, const range& r, std::vector<ScatterPoint>& out);

// Calcola la posizione pixel di ogni punto dentro "bounds", scalando
// X e Y ciascuno sul proprio intervallo minimo/massimo VERO (a
// differenza di ChartValueRange usato da barre/linee/aree, qui NON si
// forza lo zero nella scala: un grafico a dispersione tipico ha
// valori lontani da zero su entrambi gli assi, includerlo sprecherebbe
// la maggior parte dell'area disegnabile). Funzione pura, verificabile
// senza BView/Draw.
void ComputeScatterLayout(const std::vector<ScatterPoint>& data, BRect bounds,
	std::vector<BPoint>& out);

// Disegna soli punti (pallini), MAI una linea di collegamento fra loro
// -- il vero grafico "Dispersione" di Excel, distinto da "Dispersione
// con linee dritte" (non implementato, variante rara nell'uso reale).
// Asse X e asse Y entrambi con griglia/etichette numeriche, calcolate
// qui apposta (non tramite ComputeYAxisTicks/DrawYAxisGrid, condivisi
// con barre/linee/aree e pensati per una scala che include sempre lo
// zero).
void DrawScatterChart(BView* view, BRect frame, const std::vector<ScatterPoint>& data,
	const BString& title = BString());

// Smista verso DrawBarChart/DrawLineChart/DrawPieChart secondo "type"
// -- unico punto di ingresso condiviso da ChartView e SheetView (vedi
// i commenti su DrawBarChart sopra), cosi' aggiungere un futuro nuovo
// tipo di grafico tocca un solo punto di dispatch.
void DrawChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	ChartType type, const BString& title = BString());

// Dati di un grafico a PIU' serie (Fase 17): a differenza di
// ChartSeries/BuildChartSeries sopra (un solo valore per categoria),
// qui ogni categoria ha una lista di valori, uno per serie -- non un
// tipo di dato nuovo per ogni serie, le serie condividono le STESSE
// etichette di categoria (stesso principio del grafico a barre
// raggruppate di Excel). values[s][c] = valore della serie s alla
// categoria c, quindi values[s].size() == categories.size() per ogni
// s. Deliberatamente NON usato dalla torta (una torta e' per natura
// una singola serie, vedi DrawPieChart) ne' da BuildChartSeries/
// DrawBarChart/DrawLineChart sopra, che restano invariati per il caso
// (comune) a singola serie -- chi legge un intervallo decide quale dei
// due percorsi usare in base al numero di colonne, vedi
// MainWindow::HandleChartRequest/HandleChartInsert e SheetView::Draw.
struct MultiChartData {
	std::vector<BString> categories;
	std::vector<BString> seriesNames;
	std::vector<std::vector<double> > values;
	// Visibilita' dell'etichetta del valore numerico per serie (Fase
	// 19, una checkbox per serie in ChartWindow): VUOTO significa
	// "mostra tutte" (comportamento di sempre per un grafico
	// incorporato nel foglio, che non passa mai da questo campo --
	// solo l'anteprima di ChartWindow lo popola esplicitamente), non
	// serie tutte nascoste. Se presente, deve avere la stessa
	// lunghezza di seriesNames; un indice fuori dai limiti (vettore
	// piu' corto di seriesNames) e' trattato come "visibile", stesso
	// principio permissivo del resto dell'app.
	std::vector<bool> showValues;
};

// L'intervallo deve avere almeno due colonne: la prima con le
// etichette di categoria, le successive una per serie di valori
// numerici. Riga di intestazione facoltativa: se la prima riga ha un
// valore testuale in una colonna serie, quel testo diventa il nome
// della serie (stessa convenzione di Excel) e i dati partono dalla
// riga successiva; una colonna serie senza intestazione testuale (o
// senza riga di intestazione affatto) si chiama "Serie 1", "Serie 2",
// ... nell'ordine delle colonne. Una riga (di dati) con un valore non
// numerico in QUALSIASI colonna serie viene saltata per intero (stesso
// principio di BuildChartSeries, esteso per restare allineata su tutte
// le serie). Restituisce false se l'intervallo ha meno di due colonne
// o se non risulta nessuna riga di dati valida.
bool BuildMultiChartSeries(CContainer* doc, const range& r, MultiChartData& out);

struct GroupedBarLayout {
	std::vector<std::vector<BRect> > bars;	// bars[serie][categoria]
};

// Calcola il rettangolo di ogni barra, raggruppate per categoria (una
// barra affiancata per serie dentro lo stesso "slot" di categoria) --
// stessa scala di valori (intervallo min/max comune a tutte le serie)
// di ComputeBarLayout, cosi' le barre di serie diverse restano
// confrontabili sullo stesso asse. Funzione pura, verificabile senza
// BView/Draw.
void ComputeGroupedBarLayout(const MultiChartData& data, BRect bounds,
	GroupedBarLayout& out);

// Disegna il grafico a barre raggruppate: un colore per serie (stessa
// tavolozza della torta, kPieColors in Chart.cpp) e una legenda a
// destra invece del valore sopra ogni barra (con piu' serie affiancate
// diventerebbe illeggibile).
void DrawGroupedBarChart(BView* view, BRect frame, const MultiChartData& data,
	const BString& title = BString());

struct MultiLinePoint {
	std::vector<std::vector<BPoint> > points;	// points[serie][categoria]
};

// Calcola il punto di ogni valore, una spezzata per serie -- stessa
// scala comune di ComputeGroupedBarLayout. Funzione pura.
void ComputeMultiLineLayout(const MultiChartData& data, BRect bounds,
	MultiLinePoint& out);

// Disegna una spezzata per serie (stessi colori/legenda di
// DrawGroupedBarChart).
void DrawMultiLineChart(BView* view, BRect frame, const MultiChartData& data,
	const BString& title = BString());

// Grafico ad area a piu' serie (Fase 35): stessa spezzata di
// DrawMultiLineChart per serie (ComputeMultiLineLayout, riusata cosi'
// com'e'), ognuna riempita verso l'asse zero con lo stesso colore
// (semitrasparente) della sua linea -- le serie disegnate PRIMA
// restano visibili sotto quelle disegnate dopo dove si sovrappongono,
// come il grafico ad area "normale" (non impilato) di Excel.
void DrawMultiAreaChart(BView* view, BRect frame, const MultiChartData& data,
	const BString& title = BString());

#endif
