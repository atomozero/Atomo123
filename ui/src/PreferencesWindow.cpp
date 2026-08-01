/*
	PreferencesWindow.cpp

	Vedi PreferencesWindow.h.
*/

#include "PreferencesWindow.h"

#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>

static const uint32 kMsgApplyLocal = 'aplp';

PreferencesWindow::PreferencesWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 460, 320), "Preferenze",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fShowGridBox = new BCheckBox("showGrid", "Mostra griglia", NULL);

	BPopUpMenu* decimalMenu = new BPopUpMenu("decimal");
	decimalMenu->AddItem(new BMenuItem("Punto (.)", NULL));
	decimalMenu->AddItem(new BMenuItem("Virgola (,)", NULL));
	decimalMenu->ItemAt(0)->SetMarked(true);
	fDecimalField = new BMenuField("decimalField", "Separatore decimale:", decimalMenu);

	BPopUpMenu* listMenu = new BPopUpMenu("list");
	listMenu->AddItem(new BMenuItem("Punto e virgola (;)", NULL));
	listMenu->AddItem(new BMenuItem("Virgola (,)", NULL));
	listMenu->ItemAt(0)->SetMarked(true);
	fListField = new BMenuField("listField", "Separatore di elenco:", listMenu);

	BButton* applyButton = new BButton("apply", "Applica", new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fShowGridBox)
		.Add(fDecimalField)
		.Add(fListField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(applyButton)
		.End();
}

void PreferencesWindow::SetValues(bool showGrid, char decimalSep, char listSep)
{
	fShowGridBox->SetValue(showGrid ? B_CONTROL_ON : B_CONTROL_OFF);
	fDecimalField->Menu()->ItemAt(decimalSep == ',' ? 1 : 0)->SetMarked(true);
	fListField->Menu()->ItemAt(listSep == ',' ? 1 : 0)->SetMarked(true);
}

void PreferencesWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgApplyLocal)
	{
		BMenuItem* markedDecimal = fDecimalField->Menu()->FindMarked();
		char decimalSep = (markedDecimal
			&& fDecimalField->Menu()->IndexOf(markedDecimal) == 1) ? ',' : '.';

		BMenuItem* markedList = fListField->Menu()->FindMarked();
		char listSep = (markedList
			&& fListField->Menu()->IndexOf(markedList) == 1) ? ',' : ';';

		BMessage request(kMsgPreferencesRequest);
		request.AddBool("showGrid", fShowGridBox->Value() == B_CONTROL_ON);
		request.AddInt8("decimalSeparator", decimalSep);
		request.AddInt8("listSeparator", listSep);
		fTarget.SendMessage(&request);
		return;
	}

	BWindow::MessageReceived(message);
}

bool PreferencesWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
