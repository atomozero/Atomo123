/*
	ConditionalFormatWindow.cpp

	Vedi ConditionalFormatWindow.h.
*/

#include "ConditionalFormatWindow.h"

#include <Button.h>
#include <ColorControl.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TextControl.h>

static const uint32 kMsgApplyLocal = 'cfal';
static const uint32 kMsgRemoveAllLocal = 'cfrl';

// Corrispondenza posizionale con CondFormatRuleType in Container.h
// (0=eCondCellIsEqual, 1=eCondDuplicateValues) -- stesso principio gia'
// usato per ChartType/ChartWindow e ValidationType/ValidationWindow.
ConditionalFormatWindow::ConditionalFormatWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 480, 340), "Formattazione condizionale",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	BPopUpMenu* typeMenu = new BPopUpMenu("typeMenu");
	typeMenu->AddItem(new BMenuItem("Se il valore e' uguale a", NULL));
	typeMenu->AddItem(new BMenuItem("Valori duplicati nella selezione", NULL));
	typeMenu->ItemAt(0)->SetMarked(true);
	fTypeField = new BMenuField("type", "Tipo:", typeMenu);

	fValueField = new BTextControl("value", "Valore (solo per \"uguale a\"):", "", NULL);

	fColorControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 8, "colorControl");
	rgb_color initial = { 255, 199, 206, 255 }; // FFC7CE, lo stesso "rosso Excel" predefinito
	fColorControl->SetValue(initial);

	BButton* removeButton = new BButton("removeAll", "Rimuovi tutte le regole",
		new BMessage(kMsgRemoveAllLocal));
	removeButton->SetTarget(this);

	BButton* applyButton = new BButton("apply", "Applica alla selezione",
		new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fTypeField)
		.Add(fValueField)
		.Add(fColorControl)
		.AddGroup(B_HORIZONTAL)
			.Add(removeButton)
			.AddGlue()
			.Add(applyButton)
		.End();

	fValueField->MakeFocus(true);
}

int ConditionalFormatWindow::SelectedType() const
{
	return fTypeField->Menu()->IndexOf(fTypeField->Menu()->FindMarked());
}

void ConditionalFormatWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgApplyLocal:
		{
			rgb_color color = fColorControl->ValueAsColor();
			BMessage request(kMsgCondFormatCommit);
			request.AddInt32("type", SelectedType());
			request.AddString("value", fValueField->Text());
			request.AddData("color", B_RGB_COLOR_TYPE, &color, sizeof(rgb_color));
			fTarget.SendMessage(&request);
			Hide();
			return;
		}

		case kMsgRemoveAllLocal:
		{
			BMessage request(kMsgCondFormatRemoveAll);
			fTarget.SendMessage(&request);
			Hide();
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool ConditionalFormatWindow::QuitRequested()
{
	// Stessa regola di GoToWindow/ValidationWindow: resta nascosta e
	// riusabile.
	Hide();
	return false;
}
