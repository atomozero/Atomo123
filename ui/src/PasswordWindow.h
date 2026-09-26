/*
	PasswordWindow.h

	Finestra di utilita' per una password VERA di protezione foglio (Path
	to full Excel parity -- gestione completa delle password): due modi,
	scelti con PrepareForSet/PrepareForVerify prima di ogni Show(), stesso
	principio di riuso di RenameSheetWindow::SetSheet. "Set" (proteggendo
	un foglio non ancora protetto): password + conferma, campo vuoto in
	entrambi vuol dire "proteggi senza password" (comportamento di sempre,
	rimasto valido). "Verify" (sbloccando un foglio protetto DA password):
	un solo campo, il testo digitato torna al chiamante per il confronto
	con l'hash vero -- questa finestra non sa nulla di hash/crittografia,
	resta un contenitore di stato per l'input, stesso principio di
	ConditionalFormatWindow.

	Annullare (Cancella) non invia MAI kMsgPasswordCommit: il chiamante
	(MainWindow) non cambia nulla se non riceve il messaggio, niente stato
	"annullato" da gestire a parte.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PASSWORD_WINDOW_H
#define PASSWORD_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgPasswordCommit = 'pwdc';

class BStringView;
class BTextControl;

class PasswordWindow : public BWindow {
public:
								PasswordWindow(BMessenger target);

			void				PrepareForSet();
			void				PrepareForVerify();

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			BStringView*		fPrompt;
			BTextControl*		fPasswordField;
			BTextControl*		fConfirmField;
			BMessenger			fTarget;
			bool				fSetMode;
};

#endif
