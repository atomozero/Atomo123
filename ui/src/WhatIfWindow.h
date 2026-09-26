/*
	WhatIfWindow.h

	Finestra "Tabella dati" (Tier 4, "Path to full Excel parity" -- What-
	if Data Tables a una o due variabili): stesso schema di
	ValidationWindow/CommentWindow (un vero BWindow, non tocca mai
	CContainer direttamente, inoltra solo una richiesta a MainWindow via
	BMessage). Opera sulla selezione CORRENTE del foglio al momento
	dell'invio (non la cattura all'apertura): l'utente puo' aprire la
	finestra, poi selezionare/cambiare l'intervallo sul foglio sotto,
	esattamente come ValidationWindow gia' fa per Convalida dati.

	Convenzione di questa app (NON identica a quella di Excel, che usa
	un angolo diverso per ciascuno dei tre casi -- vedi il commento piu'
	lungo su MainWindow::ApplyWhatIfDataTable): la cella dell'intervallo
	selezionato in alto a sinistra e' SEMPRE la cella formula/risultato,
	in tutti e tre i casi (riga sola, colonna sola, o entrambe). Quale
	campo qui e' compilato (riga, colonna, o entrambi) decide da solo
	quale dei tre casi si applica -- non serve dedurlo dalla sola forma
	dell'intervallo.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef WHAT_IF_WINDOW_H
#define WHAT_IF_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgWhatIfCommit = 'wifc';

class BTextControl;

class WhatIfWindow : public BWindow {
public:
	WhatIfWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fRowInputField;
	BTextControl* fColInputField;
	BMessenger fTarget;
};

#endif
