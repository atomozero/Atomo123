/*
	ColorWindow.h

	Finestra "Colore testo"/"Colore sfondo" (Fase 7): un BColorControl
	piu' un pulsante Applica. Un'unica finestra riusata per entrambe le
	scelte (SetMode cambia titolo e colore iniziale) invece di due
	classi quasi identiche. Stessa regola sui thread di FindWindow: non
	tocca mai CellStyle direttamente, inoltra solo una richiesta a
	MainWindow via BMessage.
*/

#ifndef COLOR_WINDOW_H
#define COLOR_WINDOW_H

#include <GraphicsDefs.h>
#include <Messenger.h>
#include <Window.h>

const uint32 kMsgColorRequest = 'colr';

class BColorControl;

class ColorWindow : public BWindow {
public:
	ColorWindow(BMessenger target);

	// background = false per il colore del testo, true per quello di
	// sfondo -- cambia titolo e messaggio inviato da Applica, initial
	// e' il colore da mostrare gia' selezionato (letto dalla cella
	// attiva da MainWindow prima di mostrare la finestra).
	void SetMode(bool background, rgb_color initial);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BColorControl* fColorControl;
	BMessenger fTarget;
	bool fBackground;
};

#endif
