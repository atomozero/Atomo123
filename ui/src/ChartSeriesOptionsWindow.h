/*
	ChartSeriesOptionsWindow.h

	Finestra "Opzioni serie" (Fase 7, "asse secondario / trendline /
	barre d'errore"): un pop-up PER SERIE, proprieta' di ChartWindow
	(stesso principio esatto di ColorWindow -- vedi
	ChartWindow::fSeriesColorWindow/ShowSeriesColorPicker), aperto dal
	pulsante "Opzioni..." accanto a ogni riga di serie nell'editor
	grafico. L'asse secondario resta una semplice checkbox INLINE nella
	riga di serie di ChartWindow (Fase 7d) -- non c'entra con questa
	finestra, che si occupa solo delle due configurazioni troppo
	ingombranti per stare in riga: linea di tendenza e barre d'errore.

	Tiene un ChartSeriesOptions COMPLETO in memoria (fOptions): la fase
	7b ha aggiunto i controlli per la linea di tendenza, questa fase
	(7c) aggiunge quelli per le barre d'errore SENZA riscrivere la
	finestra -- Applica manda sempre l'intero struct indietro.

	Stessa regola sui thread di FindWindow (vedi FindWindow.h): non
	tocca mai il documento direttamente, manda la richiesta a
	ChartWindow via BMessage.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef CHART_SERIES_OPTIONS_WINDOW_H
#define CHART_SERIES_OPTIONS_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include "Chart.h"

// Risposta al bersaglio (sempre ChartWindow, mai MainWindow -- stesso
// principio di kMsgColorRequest/ColorWindow): porta "index" (la serie)
// e l'intero ChartSeriesOptions aggiornato ("options", B_RAW_TYPE,
// stesso trucco di round-trip gia' usato per kMsgChartInsert/
// kMsgChartUpdate in ChartWindow.cpp, non essendoci un tipo BMessage
// registrato per questo struct).
const uint32 kMsgSeriesOptionsRequest = 'sopr';

class BMenuField;
class BTextControl;

class ChartSeriesOptionsWindow : public BWindow {
public:
	ChartSeriesOptionsWindow(BMessenger target);

	// Va chiamata PRIMA di Show() per ogni nuovo utilizzo, come
	// ColorWindow::SetSeriesIndex.
	void SetSeriesIndex(int index) { fSeriesIndex = index; }
	// Precompila i controlli con le opzioni attuali della serie.
	void SetOptions(const ChartSeriesOptions& options);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	// Il campo "punti" ha senso solo per la media mobile -- disabilitato
	// (non nascosto: la finestra non deve "saltare" dimensione a ogni
	// cambio di tipo) altrimenti.
	void UpdatePeriodEnabled();
	// Il campo valore ha senso solo con una modalita' scelta (non
	// "Nessuna") -- stessa disabilitazione di UpdatePeriodEnabled, e
	// l'etichetta cambia fra "Valore:" e "Percentuale (%):" a seconda
	// della modalita' (nessun precedente di etichetta dinamica in questo
	// progetto: una semplice SetLabel() qui basta, non serve una nuova
	// astrazione).
	void UpdateErrorBarValueField();

	BMessenger fTarget;
	int fSeriesIndex;
	ChartSeriesOptions fOptions;

	BMenuField* fTrendlineTypeField;
	BTextControl* fTrendlinePeriodField;
	BMenuField* fErrorBarModeField;
	BTextControl* fErrorBarValueField;
};

#endif
