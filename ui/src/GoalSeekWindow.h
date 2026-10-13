/*
	GoalSeekWindow.h

	Finestra "Ricerca obiettivo" (Tier 3, "Path to full Excel parity" --
	Goal Seek): stesso schema di WhatIfWindow (un vero BWindow, non
	tocca mai CContainer direttamente, inoltra solo una richiesta a
	MainWindow via BMessage) ma con tre campi fissi invece di una forma
	variabile, visto che Goal Seek e' sempre lo stesso identico caso
	(una cella formula, un valore obiettivo, una cella da variare) --
	non serve dedurre quale variante si applica come fa invece
	WhatIfWindow per le tre forme di Tabella dati.

	La risoluzione vera (metodo delle secanti, stessa tecnica gia' usata
	da RATEFunction/IRRFunction nel motore) vive in
	MainWindow::ApplyGoalSeek, pubblico e testabile senza questa
	finestra -- vedi il commento li' per i dettagli dell'algoritmo e
	per cosa succede se non converge.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef GOAL_SEEK_WINDOW_H
#define GOAL_SEEK_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgGoalSeekCommit = 'gskc';

class BTextControl;

class GoalSeekWindow : public BWindow {
public:
	GoalSeekWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fSetCellField;
	BTextControl* fToValueField;
	BTextControl* fChangingCellField;
	BMessenger fTarget;
};

#endif
