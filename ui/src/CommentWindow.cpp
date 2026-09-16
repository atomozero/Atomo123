/*
	CommentWindow.cpp

	Vedi CommentWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "CommentWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <TextView.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "CommentWindow"

static const uint32 kMsgCommentLocal = 'cmsl';
static const uint32 kMsgCommentRemoveLocal = 'cmrl';

CommentWindow::CommentWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 470, 350), B_TRANSLATE("Commento cella"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fRow(-1),
	fCol(-1)
{
	fTextView = new BTextView("commentText");
	fTextView->SetWordWrap(true);
	// Esplicito invece di fidarsi dei valori predefiniti: modificabile e
	// selezionabile (dovrebbero gia' esserlo di default, ma qui e' l'unica
	// BTextView "nuda" di tutto il progetto -- ogni altro punto che
	// serve testo modificabile usa BTextControl, che gestisce questi
	// dettagli da se').
	fTextView->MakeEditable(true);
	fTextView->MakeSelectable(true);
	// Colori documento invece di bianco fisso: seguono il tema
	// chiaro/scuro della GUI (prima restava sempre bianco anche con
	// Dark mode). Senza queste due righe lo sfondo resta quello
	// ereditato di default (grigio pannello): sembra un riquadro
	// spento invece di un campo modificabile.
	fTextView->SetViewColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
	fTextView->SetLowColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
	fTextView->SetHighColor(ui_color(B_DOCUMENT_TEXT_COLOR));
	// BTextView vuota ha preferred height di una sola riga: senza
	// min-size esplicita il BScrollView collassa a "riga bianca" e non
	// riempie la finestra. 0 come flags (come NameWindow/WatchWindow),
	// non B_FOLLOW_ALL: con BLayoutBuilder i FOLLOW rompono il layout.
	BScrollView* scroll = new BScrollView("scroll", fTextView,
		0, false, true, B_FANCY_BORDER);
	scroll->SetExplicitMinSize(BSize(280, 140));

	BButton* removeButton = new BButton("remove", B_TRANSLATE("Rimuovi commento"),
		new BMessage(kMsgCommentRemoveLocal));
	removeButton->SetTarget(this);

	BButton* saveButton = new BButton("save", B_TRANSLATE("Salva"),
		new BMessage(kMsgCommentLocal));
	saveButton->SetTarget(this);
	saveButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(scroll)
		.AddGroup(B_HORIZONTAL)
			.Add(removeButton)
			.AddGlue()
			.Add(saveButton)
		.End();

	fTextView->MakeFocus(true);
}

void CommentWindow::SetCell(int row, int col, const char* currentComment)
{
	fRow = row;
	fCol = col;
	fTextView->SetText(currentComment ? currentComment : "");
	fTextView->SelectAll();
	// Richiamato di nuovo qui (oltre che nel costruttore), ora che
	// MainWindow chiama SetCell() DOPO Show()/Activate() invece che
	// prima -- vedi il commento in MainWindow::MessageReceived,
	// kMsgShowCommentWindow. MakeFocus() chiamato mentre la finestra non
	// era ancora mai stata mostrata non bastava a far arrivare gli
	// eventi tastiera per davvero.
	fTextView->MakeFocus(true);
}

void CommentWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgCommentLocal:
			if (fRow >= 0 && fCol >= 0)
			{
				BMessage request(kMsgCommentCommit);
				request.AddInt32("row", fRow);
				request.AddInt32("col", fCol);
				request.AddString("text", fTextView->Text());
				fTarget.SendMessage(&request);
			}
			Hide();
			return;

		case kMsgCommentRemoveLocal:
			if (fRow >= 0 && fCol >= 0)
			{
				BMessage request(kMsgCommentRemove);
				request.AddInt32("row", fRow);
				request.AddInt32("col", fCol);
				fTarget.SendMessage(&request);
			}
			Hide();
			return;
	}

	BWindow::MessageReceived(message);
}

bool CommentWindow::QuitRequested()
{
	// Stessa regola di GoToWindow/FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}

void CommentWindow::WindowActivated(bool active)
{
	BWindow::WindowActivated(active);
	if (active)
		fTextView->MakeFocus(true);
}
