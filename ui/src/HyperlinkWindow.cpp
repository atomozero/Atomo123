/*
	HyperlinkWindow.cpp

	Vedi HyperlinkWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "HyperlinkWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <TextControl.h>
#include <Url.h>

static const uint32 kMsgHyperlinkLocal = 'hlkl';
static const uint32 kMsgHyperlinkRemoveLocal = 'hlrl';
static const uint32 kMsgHyperlinkOpenLocal = 'hlol';

HyperlinkWindow::HyperlinkWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 470, 230), "Collegamento ipertestuale",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fRow(-1),
	fCol(-1)
{
	fUrlControl = new BTextControl("url", "URL:", "", NULL);

	BButton* removeButton = new BButton("remove", "Rimuovi collegamento",
		new BMessage(kMsgHyperlinkRemoveLocal));
	removeButton->SetTarget(this);

	BButton* openButton = new BButton("open", "Apri",
		new BMessage(kMsgHyperlinkOpenLocal));
	openButton->SetTarget(this);

	BButton* saveButton = new BButton("save", "Salva",
		new BMessage(kMsgHyperlinkLocal));
	saveButton->SetTarget(this);
	saveButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fUrlControl)
		.AddGroup(B_HORIZONTAL)
			.Add(removeButton)
			.AddGlue()
			.Add(openButton)
			.Add(saveButton)
		.End();

	fUrlControl->MakeFocus(true);
}

void HyperlinkWindow::SetCell(int row, int col, const char* currentUrl)
{
	fRow = row;
	fCol = col;
	fUrlControl->SetText(currentUrl ? currentUrl : "");
}

void HyperlinkWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgHyperlinkLocal:
			if (fRow >= 0 && fCol >= 0)
			{
				BMessage request(kMsgHyperlinkCommit);
				request.AddInt32("row", fRow);
				request.AddInt32("col", fCol);
				request.AddString("url", fUrlControl->Text());
				fTarget.SendMessage(&request);
			}
			Hide();
			return;

		case kMsgHyperlinkRemoveLocal:
			if (fRow >= 0 && fCol >= 0)
			{
				BMessage request(kMsgHyperlinkRemove);
				request.AddInt32("row", fRow);
				request.AddInt32("col", fCol);
				fTarget.SendMessage(&request);
			}
			Hide();
			return;

		case kMsgHyperlinkOpenLocal:
		{
			// Apre subito l'URL corrente nel campo, senza aspettare
			// "Salva" -- utile per verificare il collegamento prima di
			// confermarlo. Nessuna scrittura su fDoc qui: solo lettura
			// del campo di testo e apertura, stesso identico
			// comportamento di MainWindow::OpenCellHyperlink.
			BUrl url(fUrlControl->Text(), false);
			url.OpenWithPreferredApplication(false);
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool HyperlinkWindow::QuitRequested()
{
	// Stessa regola di GoToWindow/CommentWindow: resta nascosta e
	// riusabile.
	Hide();
	return false;
}
