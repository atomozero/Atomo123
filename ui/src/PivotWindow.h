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

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgPivotRequest = 'pvrq';

class BMenuField;
class BTextControl;

class PivotWindow : public BWindow {
public:
	PivotWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fSourceField;
	BTextControl* fDestField;
	BMenuField* fAggField;
	BMessenger fTarget;
};

#endif
