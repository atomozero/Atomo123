/*
	CommentWindow.cpp

	Vedi CommentWindow.h.
*/

#include "CommentWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <TextView.h>

static const uint32 kMsgCommentLocal = 'cmsl';
static const uint32 kMsgCommentRemoveLocal = 'cmrl';

CommentWindow::CommentWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 470, 350), "Commento cella",
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
	// Senza queste due righe il colore di sfondo resta quello ereditato
	// di default (grigio pannello, non bianco): sembra un riquadro
	// spento invece di un campo di testo modificabile -- bug reale
	// segnalato dall'utente insieme al fuoco tastiera qui sotto.
	fTextView->SetViewColor(255, 255, 255);
	fTextView->SetLowColor(255, 255, 255);
	BScrollView* scroll = new BScrollView("scroll", fTextView,
		B_FOLLOW_ALL, 0, false, true, B_FANCY_BORDER);

	BButton* removeButton = new BButton("remove", "Rimuovi commento",
		new BMessage(kMsgCommentRemoveLocal));
	removeButton->SetTarget(this);

	BButton* saveButton = new BButton("save", "Salva",
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
