/*
	ChartView.cpp

	Vedi ChartView.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ChartView.h"

ChartView::ChartView()
	:
	BView(BRect(0, 0, 380, 260), "ChartView", B_FOLLOW_ALL, B_WILL_DRAW),
	fType(eBarChart)
{
	SetViewColor(255, 255, 255);
	// Senza una dimensione minima esplicita, BLayoutBuilder puo'
	// schiacciare l'anteprima a quasi nulla se la finestra che la
	// contiene e' troppo piccola per il resto dei controlli.
	SetExplicitMinSize(BSize(380, 260));
}

void ChartView::SetData(const std::vector<ChartSeries>& data)
{
	fData = data;
	Invalidate();
}

void ChartView::SetChartType(ChartType type)
{
	fType = type;
	Invalidate();
}

void ChartView::Draw(BRect updateRect)
{
	// Il disegno vero e proprio (assi/barre/linee/spicchi/etichette) e'
	// condiviso con SheetView (grafico incorporato nel foglio, vedi
	// Chart.h) -- qui i dati arrivano gia' pronti via SetData
	// (ricevuti da MainWindow con un BMessage, vedi ChartWindow.cpp),
	// non letti direttamente dal documento.
	DrawChart(this, Bounds(), fData, fType);
}
