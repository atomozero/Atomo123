/*
	HyperlinkWindow.h

	Finestra per editare il collegamento ipertestuale di una cella
	(Fase 13): stesso schema esatto di CommentWindow, ma con un
	BTextControl a una riga per l'URL invece di un BTextView
	multiriga, piu' un pulsante "Apri" per lanciare subito il
	collegamento corrente senza dover prima chiudere la finestra.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef HYPERLINK_WINDOW_H
#define HYPERLINK_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgHyperlinkCommit = 'hlkc';
const uint32 kMsgHyperlinkRemove = 'hlkr';

class BTextControl;

class HyperlinkWindow : public BWindow {
public:
								HyperlinkWindow(BMessenger target);

			void				SetCell(int row, int col, const char* currentUrl);

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			BTextControl*		fUrlControl;
			BMessenger			fTarget;
			int					fRow, fCol;
};

#endif
