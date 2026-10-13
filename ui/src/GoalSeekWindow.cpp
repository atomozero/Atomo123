/*
	GoalSeekWindow.cpp

	Vedi GoalSeekWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "GoalSeekWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "GoalSeekWindow"

static const uint32 kMsgRunLocal = 'gskr';

GoalSeekWindow::GoalSeekWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 480, 340), B_TRANSLATE("Ricerca obiettivo"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	// Stesso principio del breve suggerimento a riga singola gia' usato
	// da WhatIfWindow::help -- BStringView non va a capo da solo.
	BStringView* help = new BStringView("help",
		B_TRANSLATE("Trova il valore della cella da cambiare che porta la cella formula al valore voluto:"));

	fSetCellField = new BTextControl("setCell",
		B_TRANSLATE("Imposta cella (deve contenere una formula):"), "", NULL);
	fToValueField = new BTextControl("toValue",
		B_TRANSLATE("Al valore:"), "", NULL);
	fChangingCellField = new BTextControl("changingCell",
		B_TRANSLATE("Cambiando la cella:"), "", NULL);

	BButton* runButton = new BButton("run", B_TRANSLATE("Esegui"),
		new BMessage(kMsgRunLocal));
	runButton->SetTarget(this);
	runButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(help)
		.Add(fSetCellField)
		.Add(fToValueField)
		.Add(fChangingCellField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(runButton)
		.End();

	fSetCellField->MakeFocus(true);
}

void GoalSeekWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgRunLocal:
		{
			BMessage request(kMsgGoalSeekCommit);
			request.AddString("setCell", fSetCellField->Text());
			request.AddString("toValue", fToValueField->Text());
			request.AddString("changingCell", fChangingCellField->Text());
			fTarget.SendMessage(&request);
			Hide();
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool GoalSeekWindow::QuitRequested()
{
	// Stessa regola di WhatIfWindow/ValidationWindow: resta nascosta e
	// riusabile.
	Hide();
	return false;
}
