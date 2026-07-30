/*
	ChartView.cpp

	Vedi ChartView.h.
*/

#include "ChartView.h"

ChartView::ChartView()
	:
	BView(BRect(0, 0, 380, 260), "ChartView", B_FOLLOW_ALL, B_WILL_DRAW)
{
	SetViewColor(255, 255, 255);
}

void ChartView::SetData(const std::vector<ChartSeries>& data)
{
	fData = data;
	Invalidate();
}

void ChartView::Draw(BRect updateRect)
{
	// Il disegno vero e proprio (assi/barre/etichette) e' condiviso
	// con SheetView (grafico incorporato nel foglio, vedi Chart.h) --
	// qui i dati arrivano gia' pronti via SetData (ricevuti da
	// MainWindow con un BMessage, vedi ChartWindow.cpp), non letti
	// direttamente dal documento.
	DrawBarChart(this, Bounds(), fData);
}
