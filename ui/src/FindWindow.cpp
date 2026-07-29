/*
	FindWindow.cpp

	Vedi FindWindow.h.
*/

#include "FindWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <TextControl.h>

static const uint32 kMsgFindNextLocal = 'fnlc';

FindWindow::FindWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 430, 210), "Trova", B_FLOATING_WINDOW_LOOK,
		B_FLOATING_APP_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE
			| B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fSearchField = new BTextControl("search", "Cerca:", "",
		new BMessage(kMsgFindNextLocal));
	fSearchField->SetTarget(this);
	fSearchField->MakeFocus(true);

	BButton* findButton = new BButton("find", "Trova successivo",
		new BMessage(kMsgFindNextLocal));
	findButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fSearchField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(findButton)
		.End();
}

void FindWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgFindNextLocal)
	{
		BMessage forward(kMsgFindNext);
		forward.AddString("text", fSearchField->Text());
		fTarget.SendMessage(&forward);
		return;
	}

	BWindow::MessageReceived(message);
}

bool FindWindow::QuitRequested()
{
	// Non si distrugge mai da sola: resta nascosta e pronta a essere
	// riaperta dal menu Trova di MainWindow (che tiene un unico
	// puntatore a questa finestra per tutta la vita dell'app).
	Hide();
	return false;
}
