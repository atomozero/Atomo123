/*
	WatchWindow.cpp

	Vedi WatchWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "WatchWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "WatchWindow"

static const uint32 kMsgRemoveRowLocal = 'wtrl';

WatchWindow::WatchWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 520, 380), B_TRANSLATE("Finestra di controllo"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fList = new BListView("watchedCells");
	BScrollView* listScroll = new BScrollView("watchedCellsScroll", fList,
		0, false, true);

	BButton* removeButton = new BButton("remove", B_TRANSLATE("Rimuovi"),
		new BMessage(kMsgRemoveRowLocal));
	removeButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(listScroll)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(removeButton)
		.End();
}

void WatchWindow::SetRows(const std::vector<BString>& rows)
{
	fList->MakeEmpty();
	for (size_t i = 0; i < rows.size(); i++)
		fList->AddItem(new BStringItem(rows[i].String()));
}

void WatchWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgRemoveRowLocal:
		{
			int32 index = fList->CurrentSelection();
			if (index >= 0)
			{
				BMessage forward(kMsgWatchRemoveRow);
				forward.AddInt32("row", index);
				fTarget.SendMessage(&forward);
			}
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool WatchWindow::QuitRequested()
{
	// Non si distrugge mai da sola, stessa regola di NameWindow/
	// FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
