/*
	FindWindow.h

	Piccola finestra di utilita' per "Trova"/"Sostituisci": due campi
	di testo (cerca/sostituisci con) e tre pulsanti. Non esegue la
	ricerca/sostituzione da se' (le celle appartengono al documento di
	MainWindow, su un altro thread/BLooper): inoltra il testo con un
	BMessage a un BMessenger passato dal chiamante, invece di chiamare
	direttamente un metodo della finestra proprietaria -- la stessa
	regola del bug di thread BApplication/BWindow gia' trovato e
	corretto in Fase 4 (vedi docs/UI_ARCHITECTURE.md).

	Non si chiude mai davvero alla pressione del pulsante di chiusura:
	resta nascosta e riusabile, per evitare che MainWindow debba
	gestire un puntatore che potrebbe diventare invalido.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef FIND_WINDOW_H
#define FIND_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgFindNext = 'fndn';
const uint32 kMsgReplaceCurrent = 'rplc';
const uint32 kMsgReplaceAll = 'rpla';

class BTextControl;

class FindWindow : public BWindow {
public:
	FindWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fSearchField;
	BTextControl* fReplaceField;
	BMessenger fTarget;

	void SendSearchMessage(uint32 what);
};

#endif
