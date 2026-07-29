/*
	FindWindow.h

	Piccola finestra di utilita' per "Trova": un campo di testo e un
	pulsante "Trova successivo". Non esegue la ricerca da se' (le
	celle appartengono al documento di MainWindow, su un altro
	thread/BLooper): inoltra il testo cercato con un BMessage a un
	BMessenger passato dal chiamante, invece di chiamare direttamente
	un metodo della finestra proprietaria -- la stessa regola del bug
	di thread BApplication/BWindow gia' trovato e corretto in Fase 4
	(vedi docs/UI_ARCHITECTURE.md).

	Non si chiude mai davvero alla pressione del pulsante di chiusura:
	resta nascosta e riusabile, per evitare che MainWindow debba
	gestire un puntatore che potrebbe diventare invalido.
*/

#ifndef FIND_WINDOW_H
#define FIND_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgFindNext = 'fndn';

class BTextControl;

class FindWindow : public BWindow {
public:
	FindWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fSearchField;
	BMessenger fTarget;
};

#endif
