/*
	FindWindow.cpp

	Vedi FindWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "FindWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <TextControl.h>

static const uint32 kMsgFindNextLocal = 'fnlc';
static const uint32 kMsgReplaceCurrentLocal = 'rclc';
static const uint32 kMsgReplaceAllLocal = 'ralc';

FindWindow::FindWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 430, 260), "Trova e sostituisci",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fSearchField = new BTextControl("search", "Cerca:", "",
		new BMessage(kMsgFindNextLocal));
	fSearchField->SetTarget(this);
	fSearchField->MakeFocus(true);

	fReplaceField = new BTextControl("replace", "Sostituisci con:", "", NULL);

	BButton* findButton = new BButton("find", "Trova successivo",
		new BMessage(kMsgFindNextLocal));
	findButton->SetTarget(this);

	BButton* replaceButton = new BButton("replace", "Sostituisci",
		new BMessage(kMsgReplaceCurrentLocal));
	replaceButton->SetTarget(this);

	BButton* replaceAllButton = new BButton("replaceAll", "Sostituisci tutto",
		new BMessage(kMsgReplaceAllLocal));
	replaceAllButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fSearchField)
		.Add(fReplaceField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(replaceAllButton)
			.Add(replaceButton)
			.Add(findButton)
		.End();
}

void FindWindow::SendSearchMessage(uint32 what)
{
	BMessage forward(what);
	forward.AddString("text", fSearchField->Text());
	forward.AddString("replace", fReplaceField->Text());
	fTarget.SendMessage(&forward);
}

void FindWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgFindNextLocal:
			SendSearchMessage(kMsgFindNext);
			return;
		case kMsgReplaceCurrentLocal:
			SendSearchMessage(kMsgReplaceCurrent);
			return;
		case kMsgReplaceAllLocal:
			SendSearchMessage(kMsgReplaceAll);
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
