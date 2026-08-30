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
	// Grafico a dispersione (Fase 35): percorso a parte da SetData/
	// SetMultiData, i dati non condividono la stessa forma (coppie
	// (x, y), non etichetta+valore) -- vedi ScatterPoint in Chart.h.
	void SetScatterData(const std::vector<ScatterPoint>& data);
	// Fase 17 (serie multiple): percorso alternativo a SetData, usato
	// quando l'intervallo richiesto ha piu' di due colonne -- vedi il
	// commento su MultiChartData in Chart.h. fIsMulti sceglie quale dei
	// due disegnare in Draw().
	void SetMultiData(const MultiChartData& data);
	// Accende/spegne l'etichetta del valore numerico di UNA sola serie
	// (Fase 19, una checkbox per serie in ChartWindow) senza dover
	// rifare la richiesta dati a MainWindow -- i valori non cambiano,
	// solo se disegnarne l'etichetta o no. Ininfluente se non c'e'
	// ancora un grafico a serie multiple caricato (indice fuori dai
	// limiti, vedi SeriesShowsValues in Chart.cpp).
	void SetSeriesShowValues(int index, bool show);
	void SetChartType(ChartType type);
	void SetTitle(const BString& title);

	virtual void Draw(BRect updateRect);

private:
	std::vector<ChartSeries> fData;
	MultiChartData fMultiData;
	std::vector<ScatterPoint> fScatterData;
	bool fIsMulti;
	ChartType fType;
	BString fTitle;
};

#endif
