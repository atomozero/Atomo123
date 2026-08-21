/*
	ChartWindow.h

	Finestra "Grafico": un campo di testo per l'intervallo dati (es.
	"A1:B5", due colonne: etichette e valori), un selettore del tipo
	(Barre/Linee/Torta), un'anteprima (ChartView, "Disegna") e un
	secondo campo con la cella di destinazione per incorporare davvero
	il grafico nel foglio ("Inserisci nel foglio", vedi ChartObject in
	Chart.h). Stessa regola sui thread di FindWindow (vedi
	FindWindow.h): non tocca mai il documento direttamente, manda le
	richieste a MainWindow via BMessage e riceve indietro solo dati
	gia' estratti (mai un puntatore al documento), sempre via
	BMessage.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef CHART_WINDOW_H
#define CHART_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include "Chart.h"

const uint32 kMsgChartRequest = 'chrq';
const uint32 kMsgChartData = 'chdt';
const uint32 kMsgChartInsert = 'chin';
// Risposta di HandleChartRequest quando l'intervallo ha piu' di due
// colonne (serie multiple, Fase 17) -- vedi il commento su
// MultiChartData in Chart.h e il gestore in ChartWindow.cpp.
const uint32 kMsgChartDataMulti = 'chdm';

class BMenuField;
class BTextControl;
class ChartView;

class ChartWindow : public BWindow {
public:
	ChartWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fTitleField;
	BTextControl* fRangeField;
	BMenuField* fTypeField;
	BTextControl* fDestField;
	ChartView* fChartView;
	BMessenger fTarget;

	ChartType SelectedType() const;
};

#endif
