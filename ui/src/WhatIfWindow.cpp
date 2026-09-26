/*
	WhatIfWindow.cpp

	Vedi WhatIfWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "WhatIfWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "WhatIfWindow"

static const uint32 kMsgCreateLocal = 'wifL';

WhatIfWindow::WhatIfWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 480, 330), B_TRANSLATE("Tabella dati"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	// Stesso principio dei brevi suggerimenti a riga singola gia' usati
	// da altre finestre (es. ChartWindow::seriesHint) -- BStringView non
	// va a capo da solo, quindi il testo resta corto apposta.
	BStringView* help = new BStringView("help",
		B_TRANSLATE("Cella formula in alto a sinistra, valori a destra/sotto:"));

	fRowInputField = new BTextControl("rowInput",
		B_TRANSLATE("Cella input riga (opzionale):"), "", NULL);
	fColInputField = new BTextControl("colInput",
		B_TRANSLATE("Cella input colonna (opzionale):"), "", NULL);

	BButton* createButton = new BButton("create", B_TRANSLATE("Crea"),
		new BMessage(kMsgCreateLocal));
	createButton->SetTarget(this);
	createButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(help)
		.Add(fRowInputField)
		.Add(fColInputField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(createButton)
		.End();

	fRowInputField->MakeFocus(true);
}

void WhatIfWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgCreateLocal:
		{
			BMessage request(kMsgWhatIfCommit);
			request.AddString("rowInput", fRowInputField->Text());
			request.AddString("colInput", fColInputField->Text());
			fTarget.SendMessage(&request);
			Hide();
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool WhatIfWindow::QuitRequested()
{
	// Stessa regola di GoToWindow/ValidationWindow: resta nascosta e
	// riusabile.
	Hide();
	return false;
}
