/*
	MacroListWindow.cpp

	Vedi MacroListWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "MacroListWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MacroListWindow"

static const uint32 kMsgRunLocal = 'vbrl';

MacroListWindow::MacroListWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 460, 420), B_TRANSLATE("Esegui macro"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	BStringView* help = new BStringView("help", B_TRANSLATE("Scegli una macro da eseguire:"));

	fMacroList = new BListView("macroList");
	fMacroList->SetInvocationMessage(new BMessage(kMsgRunLocal));
	BScrollView* listScroll = new BScrollView("macroListScroll", fMacroList, 0, false, true);

	BButton* runButton = new BButton("run", B_TRANSLATE("Esegui"), new BMessage(kMsgRunLocal));
	runButton->SetTarget(this);
	runButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(help)
		.Add(listScroll)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(runButton)
		.End();
}

void MacroListWindow::SetMacroNames(const std::vector<std::string>& names)
{
	fMacroList->MakeEmpty();
	for (size_t i = 0; i < names.size(); i++)
		fMacroList->AddItem(new BStringItem(names[i].c_str()));
	if (names.size() > 0)
		fMacroList->Select(0);
}

void MacroListWindow::RunSelected()
{
	int32 index = fMacroList->CurrentSelection();
	if (index < 0)
		return;
	BStringItem* item = (BStringItem*)fMacroList->ItemAt(index);
	if (!item)
		return;

	BMessage request(kMsgRunVbaMacro);
	request.AddString("macroName", item->Text());
	fTarget.SendMessage(&request);
}

void MacroListWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgRunLocal:
			RunSelected();
			return;
	}

	BWindow::MessageReceived(message);
}

bool MacroListWindow::QuitRequested()
{
	// Stessa regola di NameWindow/GoalSeekWindow: resta nascosta e
	// riusabile, MainWindow tiene un unico puntatore per tutta la vita
	// dell'app.
	Hide();
	return false;
}
