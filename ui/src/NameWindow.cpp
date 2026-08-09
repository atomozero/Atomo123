/*
	NameWindow.cpp

	Vedi NameWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "NameWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <TextControl.h>

static const uint32 kMsgDefineNameLocal = 'dfnl';
static const uint32 kMsgDeleteNameLocal = 'dlnl';
static const uint32 kMsgGoToNameLocal = 'gtnl';
static const uint32 kMsgSelectNameLocal = 'slnl';

NameWindow::NameWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 460, 400), "Intervalli con nome",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fNameList = new BListView("names");
	fNameList->SetSelectionMessage(new BMessage(kMsgSelectNameLocal));
	fNameList->SetTarget(this);
	BScrollView* listScroll = new BScrollView("namesScroll", fNameList,
		0, false, true);

	fNameField = new BTextControl("name", "Nome:", "", NULL);
	fRangeField = new BTextControl("range", "Intervallo:", "", NULL);

	BButton* defineButton = new BButton("define", "Aggiungi/Aggiorna",
		new BMessage(kMsgDefineNameLocal));
	defineButton->SetTarget(this);

	BButton* deleteButton = new BButton("delete", "Elimina",
		new BMessage(kMsgDeleteNameLocal));
	deleteButton->SetTarget(this);

	BButton* goToButton = new BButton("goto", "Vai a",
		new BMessage(kMsgGoToNameLocal));
	goToButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(listScroll)
		.Add(fNameField)
		.Add(fRangeField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(goToButton)
			.Add(deleteButton)
			.Add(defineButton)
		.End();
}

void NameWindow::SetNames(const std::vector<BString>& names,
	const std::vector<BString>& ranges)
{
	fNameList->MakeEmpty();
	fRanges = ranges;

	for (size_t i = 0; i < names.size(); i++)
		fNameList->AddItem(new BStringItem(names[i].String()));
}

void NameWindow::SendNameMessage(uint32 what)
{
	BMessage forward(what);
	forward.AddString("name", fNameField->Text());
	forward.AddString("range", fRangeField->Text());
	fTarget.SendMessage(&forward);
}

void NameWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgSelectNameLocal:
		{
			int32 index = fNameList->CurrentSelection();
			if (index >= 0 && (size_t)index < fRanges.size())
			{
				BStringItem* item = (BStringItem*)fNameList->ItemAt(index);
				fNameField->SetText(item->Text());
				fRangeField->SetText(fRanges[index].String());
			}
			return;
		}

		case kMsgDefineNameLocal:
			SendNameMessage(kMsgDefineName);
			return;

		case kMsgDeleteNameLocal:
			SendNameMessage(kMsgDeleteName);
			return;

		case kMsgGoToNameLocal:
			SendNameMessage(kMsgGoToName);
			return;
	}

	BWindow::MessageReceived(message);
}

bool NameWindow::QuitRequested()
{
	// Non si distrugge mai da sola, stessa regola di FindWindow: resta
	// nascosta e riusabile, MainWindow tiene un unico puntatore per
	// tutta la vita dell'app.
	Hide();
	return false;
}
