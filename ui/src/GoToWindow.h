/*
	GoToWindow.h

	Piccola finestra di utilita' "Vai a" (Fase 7): un campo di testo
	(cella o intervallo, es. "C15" o "A1:B5" -- stessa sintassi di
	RangeRef::ParseRangeRef, gia' condivisa da grafico e tabella
	pivot) e un pulsante. Stessa regola sui thread di FindWindow: non
	tocca mai la selezione del foglio direttamente, inoltra solo un
	BMessage a MainWindow, che possiede la SheetView attiva e la puo'
	toccare sul proprio thread.
*/

#ifndef GO_TO_WINDOW_H
#define GO_TO_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgGoToRequest = 'gtor';

class BTextControl;

class GoToWindow : public BWindow {
public:
	GoToWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fRangeField;
	BMessenger fTarget;
};

#endif
