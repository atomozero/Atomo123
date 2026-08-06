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
