/*
	ConditionalFormatWindow.h

	Finestra "Formattazione condizionale" (Fase 13): sceglie fra i due
	tipi di regola gestiti (valore uguale a un letterale, o valori
	duplicati) e un colore di sfondo, applicata a tutta la selezione
	corrente -- come il vero "Convalida dati", non solo alla cella
	attiva (a differenza di CommentWindow/HyperlinkWindow), stesso
	principio di MainWindow::ApplyValidationToSelection. Nessun editing
	per singola regola gia' esistente: "Rimuovi tutte le regole"
	toglie l'intero elenco in un colpo solo, stessa semplicita' di
	scope gia' scelta per il resto di questo punto.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef CONDITIONAL_FORMAT_WINDOW_H
#define CONDITIONAL_FORMAT_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgCondFormatCommit = 'cfmc';
const uint32 kMsgCondFormatRemoveAll = 'cfmr';

class BColorControl;
class BMenuField;
class BTextControl;

class ConditionalFormatWindow : public BWindow {
public:
								ConditionalFormatWindow(BMessenger target);

	virtual	void				MessageReceived(BMessage* message);
	virtual	bool				QuitRequested();

private:
			BMenuField*			fTypeField;
			BTextControl*		fValueField;
			BColorControl*		fColorControl;
			BColorControl*		fMaxColorControl;
			BMessenger			fTarget;

			int					SelectedType() const;
			void				UpdateFieldsForType();
};

#endif
