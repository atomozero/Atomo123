/*
	GoToWindow.cpp

	Vedi GoToWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "GoToWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <TextControl.h>

static const uint32 kMsgGoToLocal = 'gtol';

GoToWindow::GoToWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 380, 210), "Vai a",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fRangeField = new BTextControl("range", "Cella o intervallo:", "",
		new BMessage(kMsgGoToLocal));
	fRangeField->SetTarget(this);
	fRangeField->MakeFocus(true);

	BButton* goButton = new BButton("go", "Vai a", new BMessage(kMsgGoToLocal));
	goButton->SetTarget(this);
	goButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fRangeField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(goButton)
		.End();
}

void GoToWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgGoToLocal)
	{
		BMessage request(kMsgGoToRequest);
		request.AddString("range", fRangeField->Text());
		fTarget.SendMessage(&request);
		return;
	}

	BWindow::MessageReceived(message);
}

bool GoToWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
