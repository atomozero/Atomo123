/*
	PivotWindow.cpp

	Vedi PivotWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PivotWindow.h"
#include "Pivot.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TextControl.h>

static const uint32 kMsgCreateLocal = 'pvlc';

PivotWindow::PivotWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 520, 300), "Tabella pivot",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fSourceField = new BTextControl("source",
		"Intervallo dati (due colonne: categoria, valore):", "A1:B10", NULL);

	fDestField = new BTextControl("dest", "Cella di destinazione:", "D1", NULL);

	BPopUpMenu* aggMenu = new BPopUpMenu("agg");
	aggMenu->AddItem(new BMenuItem("Somma", NULL));
	aggMenu->AddItem(new BMenuItem("Conteggio", NULL));
	aggMenu->AddItem(new BMenuItem("Media", NULL));
	aggMenu->ItemAt(0)->SetMarked(true);
	fAggField = new BMenuField("aggField", "Aggregazione:", aggMenu);

	BButton* createButton = new BButton("create", "Crea", new BMessage(kMsgCreateLocal));
	createButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fSourceField)
		.Add(fDestField)
		.Add(fAggField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(createButton)
		.End();
}

void PivotWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgCreateLocal)
	{
		PivotAggFunc fn = ePivotSum;
		BMenuItem* marked = fAggField->Menu()->FindMarked();
		if (marked)
		{
			int32 index = fAggField->Menu()->IndexOf(marked);
			if (index == 1)
				fn = ePivotCount;
			else if (index == 2)
				fn = ePivotAverage;
		}

		BMessage request(kMsgPivotRequest);
		request.AddString("source", fSourceField->Text());
		request.AddString("dest", fDestField->Text());
		request.AddInt32("agg", (int32)fn);
		fTarget.SendMessage(&request);
		return;
	}

	BWindow::MessageReceived(message);
}

bool PivotWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
