/*
	ValidationWindow.h

	Finestra "Convalida dati" (Fase 13): sceglie fra due tipi di
	regola (elenco di valori separati da virgola, o intervallo
	numerico min/max) per la cella attiva -- stesso schema di
	CommentWindow/HyperlinkWindow (un vero BWindow, non tocca mai
	CContainer direttamente, inoltra solo una richiesta a MainWindow
	via BMessage).
*/

#ifndef VALIDATION_WINDOW_H
#define VALIDATION_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgValidationCommit = 'vldc';
const uint32 kMsgValidationRemove = 'vldr';

class BMenuField;
class BTextControl;

class ValidationWindow : public BWindow {
public:
								ValidationWindow(BMessenger target);

			void				SetCell(int row, int col, int type,
									const char* list, double min, double max);

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			BMenuField*			fTypeField;
			BTextControl*		fListField;
			BTextControl*		fMinField;
			BTextControl*		fMaxField;
			BMessenger			fTarget;
			int					fRow, fCol;

			int					SelectedType() const;
};

#endif
