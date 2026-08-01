/*
	AboutWindow.h

	Finestra "Informazioni su Atomo123": banner con sfondo sfumato,
	icona dell'app su un riquadro blu arrotondato, titolo/versione,
	una breve descrizione, autore, link cliccabile al progetto e
	crediti -- stesso stile (e stessi componenti) della finestra
	Informazioni di Brube2000, altro progetto nativo Haiku dello stesso
	autore, riproposto qui su richiesta esplicita per coerenza visiva
	fra le due app.
*/

#ifndef ABOUT_WINDOW_H
#define ABOUT_WINDOW_H

#include <Window.h>

class AboutWindow : public BWindow {
public:
	AboutWindow();

	virtual void MessageReceived(BMessage* message);
};

#endif
