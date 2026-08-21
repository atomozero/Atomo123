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

#include <vector>

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

class BBox;
class BCheckBox;
class BMenuField;
class BTextControl;
class ChartView;

class ChartWindow : public BWindow {
public:
	ChartWindow(BMessenger target);

	// Precompila il campo Intervallo e disegna subito l'anteprima --
	// usata da MainWindow::ShowChartWindow quando il foglio ha gia'
	// una selezione di piu' di una cella al momento dell'apertura,
	// cosi' l'utente non deve ridigitare come testo un intervallo gia'
	// selezionato sul foglio (Fase 18).
	void LoadRange(const char* rangeText);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fTitleField;
	BTextControl* fRangeField;
	BMenuField* fTypeField;
	BTextControl* fDestField;
	ChartView* fChartView;
	BMessenger fTarget;
	// Riquadro (con titolo + suggerimento) che contiene la riga di
	// checkbox "mostra i valori", una voce per serie (Fase 19). Creato
	// una sola volta nel costruttore -- solo fSeriesCheckboxRow al suo
	// interno viene svuotato/ripopolato a ogni richiesta, cosi' titolo
	// e suggerimento non vengono mai distrutti e ricreati. Nascosto per
	// intero (Hide/Show) quando l'ultimo grafico richiesto e' a singola
	// serie, cosi' non resta un riquadro vuoto che sembra un controllo
	// "sparito" (vedi ClearSeriesCheckboxes/RebuildSeriesCheckboxes).
	BBox* fSeriesCheckboxBox;
	BView* fSeriesCheckboxRow;
	std::vector<BCheckBox*> fSeriesCheckboxes;

	ChartType SelectedType() const;
	// Corpo comune di kMsgDrawLocal e LoadRange sopra: applica il
	// titolo corrente all'anteprima e richiede a MainWindow i dati
	// dell'intervallo attualmente in fRangeField.
	void RequestDraw();
	// Ricostruisce fSeriesCheckboxRow per i dati appena ricevuti,
	// preservando lo stato di una checkbox il cui nome di serie
	// combacia con quello di prima (stessa serie tra una richiesta e
	// l'altra) e imposta data->showValues di conseguenza -- chiamata
	// PRIMA di ChartView::SetMultiData, cosi' il grafico si disegna
	// gia' con le visibilita' corrette al primo giro, non solo dopo
	// che l'utente tocca una checkbox.
	void RebuildSeriesCheckboxes(MultiChartData* data);
	// Nessuna checkbox da mostrare per un grafico a singola serie.
	void ClearSeriesCheckboxes();
};

#endif
