/*
	RenameSheetWindow.cpp

	Vedi RenameSheetWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "RenameSheetWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "RenameSheetWindow"

static const uint32 kMsgRenameSheetLocal = 'rnsl';

RenameSheetWindow::RenameSheetWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 380, 210), B_TRANSLATE("Rinomina foglio"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fIndex(-1)
{
	fNameField = new BTextControl("name", B_TRANSLATE("Nome del foglio:"), "",
		new BMessage(kMsgRenameSheetLocal));
	fNameField->SetTarget(this);
	fNameField->MakeFocus(true);

	BButton* renameButton = new BButton("rename", B_TRANSLATE("Rinomina"),
		new BMessage(kMsgRenameSheetLocal));
	renameButton->SetTarget(this);
	renameButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fNameField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(renameButton)
		.End();
}

void RenameSheetWindow::SetSheet(int index, const char* currentName)
{
	fIndex = index;
	fNameField->SetText(currentName);
	fNameField->TextView()->SelectAll();
}

void RenameSheetWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgRenameSheetLocal)
	{
		if (fIndex >= 0)
		{
			BMessage request(kMsgRenameSheetCommit);
			request.AddInt32("index", fIndex);
			request.AddString("name", fNameField->Text());
			fTarget.SendMessage(&request);
		}
		Hide();
		return;
	}

	BWindow::MessageReceived(message);
}

bool RenameSheetWindow::QuitRequested()
{
	// Stessa regola di GoToWindow/FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
