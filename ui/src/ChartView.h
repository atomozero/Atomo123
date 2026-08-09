/*
	ChartView.h

	Disegna il grafico a barre (vedi Chart.h per il calcolo del
	layout) dentro ChartWindow. Non tocca mai il documento: riceve i
	dati gia' pronti (etichette + valori) da ChartWindow, che a sua
	volta li riceve da MainWindow via BMessage (vedi ChartWindow.cpp
	e la nota sui thread in FindWindow.h).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef CHART_VIEW_H
#define CHART_VIEW_H

#include <vector>

#include <View.h>

#include "Chart.h"

class ChartView : public BView {
public:
	ChartView();

	void SetData(const std::vector<ChartSeries>& data);
	void SetChartType(ChartType type);

	virtual void Draw(BRect updateRect);

private:
	std::vector<ChartSeries> fData;
	ChartType fType;
};

#endif
