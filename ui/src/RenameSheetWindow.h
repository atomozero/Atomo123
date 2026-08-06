/*
	RenameSheetWindow.h

	Piccola finestra di utilita' "Rinomina foglio" (Fase 13): un campo
	di testo pre-riempito con il nome corrente del foglio e un
	pulsante. Stesso schema esatto di GoToWindow (stessa regola sui
	thread: non tocca mai fSheets direttamente, inoltra solo un
	BMessage a MainWindow, che possiede fSheets e lo puo' toccare sul
	proprio thread), con l'aggiunta dell'indice del foglio da
	rinominare (SetSheet, chiamato da MainWindow subito prima di
	Show()/Activate() -- stesso principio di ColorWindow::SetMode).
*/

#ifndef RENAME_SHEET_WINDOW_H
#define RENAME_SHEET_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgRenameSheetCommit = 'rnsc';

class BTextControl;

class RenameSheetWindow : public BWindow {
public:
	RenameSheetWindow(BMessenger target);

	// index e' il foglio da rinominare, currentName il suo nome
	// attuale (precompila il campo, cosi' l'utente puo' modificare
	// solo la parte che vuole invece di riscrivere tutto da capo).
	void SetSheet(int index, const char* currentName);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fNameField;
	BMessenger fTarget;
	int fIndex;
};

#endif
