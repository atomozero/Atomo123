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

void ChartView::SetSeriesColor(int index, rgb_color color)
{
	if (index < 0)
		return;
	// Slot intermedi creati da resize restano alpha 0 ("non impostato",
	// vedi SeriesColor() in Chart.cpp) finche' l'utente non li tocca a
	// loro volta -- solo l'indice richiesto qui diventa un vero override.
	if (index >= (int)fMultiData.seriesColors.size())
		fMultiData.seriesColors.resize(index + 1, rgb_color{0, 0, 0, 0});
	fMultiData.seriesColors[index] = color;
	Invalidate();
}

void ChartView::SetChartColor(rgb_color color)
{
	fSingleColors.resize(1);
	fSingleColors[0] = color;
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
		DrawChartAntialiased(this, Bounds(), [&](BView* v, BRect f) {
			DrawScatterChart(v, f, fScatterData, fTitle, fSingleColors);
		});
		return;
	}

	if (!fIsMulti)
	{
		DrawChartAntialiased(this, Bounds(), [&](BView* v, BRect f) {
			DrawChart(v, f, fData, fType, fTitle, fSingleColors);
		});
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
		DrawChartAntialiased(this, Bounds(), [&](BView* v, BRect f) {
			DrawPieChart(v, f, single, fTitle);
		});
		return;
	}

	DrawChartAntialiased(this, Bounds(), [&](BView* v, BRect f) {
		if (fType == eLineChart)
			DrawMultiLineChart(v, f, fMultiData, fTitle);
		else if (fType == eAreaChart)
			DrawMultiAreaChart(v, f, fMultiData, fTitle);
		else if (fType == eComboChart)
			DrawComboChart(v, f, fMultiData, fTitle);
		else if (fType == eHBarChart)
			DrawGroupedHBarChart(v, f, fMultiData, fTitle);
		else
			DrawGroupedBarChart(v, f, fMultiData, fTitle);
	});
}
