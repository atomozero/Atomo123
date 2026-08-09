/*
	CommentWindow.h

	Piccola finestra di utilita' "Commento cella" (Fase 13): un'area di
	testo multi-riga (a differenza di GoToWindow/RenameSheetWindow, un
	commento puo' essere lungo piu' di una riga, come in Excel/
	LibreOffice Calc) precompilata col commento attuale della cella
	(se gia' presente), un pulsante "Salva" e un pulsante "Rimuovi
	commento". Stessa regola sui thread di GoToWindow: non tocca mai
	CContainer::SetComment direttamente, inoltra solo un BMessage a
	MainWindow, che possiede fDoc e lo puo' toccare sul proprio thread.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef COMMENT_WINDOW_H
#define COMMENT_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgCommentCommit = 'cmsc';
const uint32 kMsgCommentRemove = 'cmsr';

class BTextView;

class CommentWindow : public BWindow {
public:
	CommentWindow(BMessenger target);

	// row/col individuano la cella (coordinate dirette, non un tipo
	// "cell" dell'engine: questa finestra non linka contro l'engine,
	// stesso principio di GoToWindow che passa il testo grezzo
	// dell'intervallo invece di un "range" gia' risolto).
	void SetCell(int row, int col, const char* currentComment);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();
	// MakeFocus(true) chiamato una sola volta nel costruttore (prima di
	// qualunque Show()) imposta il fuoco solo lato BView -- il caret
	// lampeggia (BTextView::Draw guarda IsFocus(), puramente locale),
	// ma l'app_server non ha ancora un motivo per instradarci davvero
	// gli eventi tastiera finche' la finestra non diventa attiva per
	// davvero. Bug reale segnalato dall'utente: cursore visibile, ma
	// digitare non scriveva nulla. Riaffermare il fuoco qui, che scatta
	// a ogni vera attivazione (anche le successive, non solo la prima
	// Show()), risolve sia il primo utilizzo sia i riusi seguenti.
	virtual void WindowActivated(bool active);

private:
	BTextView* fTextView;
	BMessenger fTarget;
	int fRow, fCol;
};

#endif
