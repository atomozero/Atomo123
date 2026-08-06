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

private:
	BTextView* fTextView;
	BMessenger fTarget;
	int fRow, fCol;
};

#endif
