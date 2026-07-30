/*
	PivotWindow.h

	Finestra "Tabella pivot": intervallo dati sorgente (due colonne:
	categoria, valore), cella di destinazione e scelta
	dell'aggregazione (Somma/Conteggio/Media). Stessa regola sui
	thread di FindWindow: manda una richiesta a MainWindow via
	BMessage, che legge/scrive il documento sul proprio thread e
	aggiorna la griglia -- questa finestra non tocca mai il documento
	direttamente e non riceve dati indietro (il risultato va nel
	foglio, non in una vista propria di questa finestra).
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
