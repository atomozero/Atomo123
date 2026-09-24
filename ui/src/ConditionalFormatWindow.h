/*
	ConditionalFormatWindow.h

	Finestra "Formattazione condizionale" (Fase 13, poi Tier 1/Tier 3 di
	"Path to 100% XLSX standard compatibility", poi "Path to full Excel
	parity" Tier 3 -- cellIs con tutti gli operatori ECMA-376 piu' le
	famiglie containsText/containsBlanks/containsErrors, top10 e
	aboveAverage): sceglie fra i tipi di regola gestiti e un colore,
	applicata a tutta la selezione corrente -- come il vero "Convalida
	dati", non solo alla cella attiva (a differenza di CommentWindow/
	HyperlinkWindow), stesso principio di
	MainWindow::ApplyValidationToSelection. Nessun editing per singola
	regola gia' esistente: "Rimuovi tutte le regole" toglie l'intero
	elenco in un colpo solo, stessa semplicita' di scope gia' scelta
	per il resto di questo punto.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef CONDITIONAL_FORMAT_WINDOW_H
#define CONDITIONAL_FORMAT_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgCondFormatCommit = 'cfmc';
const uint32 kMsgCondFormatRemoveAll = 'cfmr';

class BCheckBox;
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
			BMenuField*			fOperatorField;
			BTextControl*		fValueField;
			BTextControl*		fValueField2;
			BColorControl*		fColorControl;
			BColorControl*		fMaxColorControl;
			BTextControl*		fRankField;
			BCheckBox*			fPercentCheckBox;
			BCheckBox*			fBottomCheckBox;
			BCheckBox*			fBelowAverageCheckBox;
			BMessenger			fTarget;

			int					SelectedType() const;
			int					SelectedOperator() const;
			void				UpdateFieldsForType();
};

#endif
