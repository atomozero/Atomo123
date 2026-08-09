/*
	PasteSpecialWindow.cpp

	Vedi PasteSpecialWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PasteSpecialWindow.h"

#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>

static const uint32 kMsgPasteLocal = 'pstl';

PasteSpecialWindow::PasteSpecialWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 460, 300), "Incolla speciale",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	BPopUpMenu* contentMenu = new BPopUpMenu("content");
	contentMenu->AddItem(new BMenuItem("Tutto (come Incolla)", NULL));
	contentMenu->AddItem(new BMenuItem("Solo valori", NULL));
	contentMenu->ItemAt(0)->SetMarked(true);
	fContentField = new BMenuField("contentField", "Cosa incollare:", contentMenu);

	BPopUpMenu* opMenu = new BPopUpMenu("operation");
	opMenu->AddItem(new BMenuItem("Sovrascrivi", NULL));
	opMenu->AddItem(new BMenuItem("Somma", NULL));
	opMenu->AddItem(new BMenuItem("Sottrai", NULL));
	opMenu->AddItem(new BMenuItem("Moltiplica", NULL));
	opMenu->AddItem(new BMenuItem("Dividi", NULL));
	opMenu->ItemAt(0)->SetMarked(true);
	fOperationField = new BMenuField("operationField",
		"Operazione con la cella di destinazione:", opMenu);

	fTransposeBox = new BCheckBox("transpose", "Trasponi righe e colonne", NULL);

	BButton* pasteButton = new BButton("paste", "Incolla", new BMessage(kMsgPasteLocal));
	pasteButton->SetTarget(this);
	pasteButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fContentField)
		.Add(fOperationField)
		.Add(fTransposeBox)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(pasteButton)
		.End();
}

void PasteSpecialWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgPasteLocal)
	{
		int32 content = 0;
		BMenuItem* marked = fContentField->Menu()->FindMarked();
		if (marked)
			content = fContentField->Menu()->IndexOf(marked);

		int32 operation = 0;
		marked = fOperationField->Menu()->FindMarked();
		if (marked)
			operation = fOperationField->Menu()->IndexOf(marked);

		BMessage request(kMsgPasteSpecialRequest);
		request.AddInt32("content", content);
		request.AddInt32("operation", operation);
		request.AddBool("transpose", fTransposeBox->Value() == B_CONTROL_ON);
		fTarget.SendMessage(&request);
		return;
	}

	BWindow::MessageReceived(message);
}

bool PasteSpecialWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
