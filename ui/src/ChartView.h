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
	// Colore personalizzato della serie "index" per un grafico a serie
	// multiple (Fase colori) -- aggiorna fMultiData.seriesColors e
	// ridisegna subito, senza dover rifare la richiesta dati a
	// MainWindow. Simmetrico a SetSeriesShowValues sopra.
	void SetSeriesColor(int index, rgb_color color);
	// Gemella di SetSeriesColor, ma per un grafico a SINGOLA serie
	// (barre/linee/area/barre orizzontali/dispersione): un solo colore
	// per l'intero grafico, non uno per categoria. Ininfluente per la
	// torta (mantiene sempre la sua tavolozza per fetta).
	void SetChartColor(rgb_color color);
	void SetChartType(ChartType type);
	void SetTitle(const BString& title);

	virtual void Draw(BRect updateRect);

private:
	std::vector<ChartSeries> fData;
	MultiChartData fMultiData;
	std::vector<ScatterPoint> fScatterData;
	// Colore personalizzato per un grafico a SINGOLA serie (vedi
	// SetChartColor sopra): solo l'indice 0 e' mai usato, stesso
	// principio di ChartObject::seriesColors in Chart.h -- un
	// std::vector (non un semplice rgb_color) solo per passarlo diretto
	// a SeriesColor()/alle funzioni Draw*Chart senza conversioni.
	std::vector<rgb_color> fSingleColors;
	bool fIsMulti;
	ChartType fType;
	BString fTitle;
};

#endif
