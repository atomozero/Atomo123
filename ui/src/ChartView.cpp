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
	fIsMulti(false),
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
	fIsMulti = false;
	Invalidate();
}

void ChartView::SetMultiData(const MultiChartData& data)
{
	fMultiData = data;
	fIsMulti = true;
	Invalidate();
}

void ChartView::SetScatterData(const std::vector<ScatterPoint>& data)
{
	fScatterData = data;
	fIsMulti = false;
	Invalidate();
}

void ChartView::SetSeriesShowValues(int index, bool show)
{
	if (index < 0 || index >= (int)fMultiData.showValues.size())
		return;
	fMultiData.showValues[index] = show;
	Invalidate();
}

void ChartView::SetChartType(ChartType type)
{
	fType = type;
	Invalidate();
}

void ChartView::SetTitle(const BString& title)
{
	fTitle = title;
	Invalidate();
}

void ChartView::Draw(BRect updateRect)
{
	// Il disegno vero e proprio (assi/barre/linee/spicchi/etichette) e'
	// condiviso con SheetView (grafico incorporato nel foglio, vedi
	// Chart.h) -- qui i dati arrivano gia' pronti via SetData/
	// SetMultiData (ricevuti da MainWindow con un BMessage, vedi
	// ChartWindow.cpp), non letti direttamente dal documento.
	if (fType == eScatterChart)
	{
		// Percorso completamente a parte (fScatterData, mai fData/
		// fMultiData): niente serie multiple ne' torta per un grafico a
		// dispersione, vedi il commento su ScatterPoint in Chart.h.
		DrawScatterChart(this, Bounds(), fScatterData, fTitle);
		return;
	}

	if (!fIsMulti)
	{
		DrawChart(this, Bounds(), fData, fType, fTitle);
		return;
	}

	if (fType == ePieChart)
	{
		// La torta non supporta piu' serie (vedi il commento su
		// MultiChartData in Chart.h): si disegna con la sola prima
		// serie, invece di rifiutarsi o mostrare un errore -- stesso
		// principio permissivo del resto dell'app (un dato "non del
		// tutto nella forma attesa" si adatta il piu' possibile invece
		// di bloccarsi).
		std::vector<ChartSeries> single;
		if (!fMultiData.values.empty())
		{
			for (size_t c = 0; c < fMultiData.categories.size(); c++)
			{
				ChartSeries s;
				s.label = fMultiData.categories[c];
				s.value = fMultiData.values[0][c];
				single.push_back(s);
			}
		}
		DrawPieChart(this, Bounds(), single, fTitle);
		return;
	}

	if (fType == eLineChart)
		DrawMultiLineChart(this, Bounds(), fMultiData, fTitle);
	else if (fType == eAreaChart)
		DrawMultiAreaChart(this, Bounds(), fMultiData, fTitle);
	else if (fType == eComboChart)
		DrawComboChart(this, Bounds(), fMultiData, fTitle);
	else
		DrawGroupedBarChart(this, Bounds(), fMultiData, fTitle);
}
