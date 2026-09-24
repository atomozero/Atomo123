/*
	PivotWindow.h

	Finestra "Tabella pivot": intervallo dati sorgente (una o piu'
	colonne di categoria, poi il valore -- raggruppamento multi-livello
	se piu' di una, Fase 29), cella di destinazione e scelta
	dell'aggregazione (Somma/Conteggio/Media/Minimo/Massimo). Stessa
	regola sui thread di FindWindow: manda una richiesta a MainWindow via
	BMessage, che legge/scrive il documento sul proprio thread e
	aggiorna la griglia -- questa finestra non tocca mai il documento
	direttamente e non riceve dati indietro (il risultato va nel
	foglio, non in una vista propria di questa finestra).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PIVOT_WINDOW_H
#define PIVOT_WINDOW_H

#include <vector>

#include <Messenger.h>
#include <String.h>
#include <Window.h>

const uint32 kMsgPivotRequest = 'pvrq';

// Round trip campo Colonne/misure (Fase 2D): PivotWindow non ha piu' solo
// due campi di testo, ha bisogno di conoscere le colonne dell'intervallo
// sorgente PRIMA che l'utente prema "Crea" -- stesso principio round
// trip di kMsgChartRequest/kMsgChartDataMulti in ChartWindow, mai servito
// finora perche' il vecchio dialogo non aveva controlli dinamici da
// popolare.
const uint32 kMsgPivotDetectColumns = 'pvdc'; // PivotWindow -> MainWindow
const uint32 kMsgPivotColumnsInfo   = 'pvci'; // MainWindow -> PivotWindow

class BBox;
class BButton;
class BCheckBox;
class BMenuField;
class BTextControl;
class BView;

class PivotWindow : public BWindow {
public:
	PivotWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	void RebuildColumnPickers(BMessage* colInfo);
	void ClearColumnPickers();
	void RequestDetectColumns();

	BTextControl* fSourceField;
	BTextControl* fDestField;
	BMenuField* fAggField; // aggregazione della sola misura implicita (percorso 1D)
	BButton* fDetectButton;
	BMessenger fTarget;

	// Scelta ESCLUSIVA del campo Colonne: "(nessuna)" (indice 0, ==
	// percorso 1D) piu' un elemento per colonna rilevata, ognuno con un
	// BMessage che porta "col" (colonna assoluta).
	BMenuField* fColumnFieldMenu;

	// Una riga CHECKBOX + BMenuField (aggregazione) per ogni colonna
	// rilevata -- stesso pattern di fSeriesCheckboxBox/
	// fSeriesCheckboxRow/fSeriesCheckboxes in ChartWindow.h.
	BBox* fMeasureBox;
	BView* fMeasureRows;
	std::vector<BCheckBox*> fMeasureCheckboxes;
	std::vector<BMenuField*> fMeasureAggFields;
	std::vector<int32> fMeasureCols; // colonna assoluta di ogni riga sopra, stesso indice
};

#endif
