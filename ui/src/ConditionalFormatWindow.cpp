/*
	ConditionalFormatWindow.cpp

	Vedi ConditionalFormatWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ConditionalFormatWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <ColorControl.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ConditionalFormatWindow"

static const uint32 kMsgApplyLocal = 'cfal';
static const uint32 kMsgRemoveAllLocal = 'cfrl';
static const uint32 kMsgTypeChangedLocal = 'cftc';

// Corrispondenza posizionale con CondFormatRuleType in Container.h
// (0=eCondCellIsEqual, 1=eCondDuplicateValues, 2=eCondColorScale) --
// stesso principio gia' usato per ChartType/ChartWindow e
// ValidationType/ValidationWindow. La scala di colori qui e' sempre a
// due punti (min->max): copre il caso Excel piu' comune senza dover
// costruire un editor per un numero arbitrario di soglie.
ConditionalFormatWindow::ConditionalFormatWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 480, 400), B_TRANSLATE("Formattazione condizionale"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	BPopUpMenu* typeMenu = new BPopUpMenu("typeMenu");
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Se il valore e' uguale a"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Valori duplicati nella selezione"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Scala di colori (minimo -> massimo)"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->ItemAt(0)->SetMarked(true);
	typeMenu->SetTargetForItems(this);
	fTypeField = new BMenuField("type", B_TRANSLATE("Tipo:"), typeMenu);

	fValueField = new BTextControl("value", B_TRANSLATE("Valore (solo per \"uguale a\"):"), "", NULL);

	fColorControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 8, "colorControl");
	rgb_color initial = { 255, 199, 206, 255 }; // FFC7CE, lo stesso "rosso Excel" predefinito
	fColorControl->SetValue(initial);

	fMaxColorControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 8, "maxColorControl");
	rgb_color maxInitial = { 99, 190, 123, 255 }; // 63BE7B, il verde predefinito di Excel per il massimo
	fMaxColorControl->SetValue(maxInitial);
	fMaxColorControl->Hide();

	BButton* removeButton = new BButton("removeAll", B_TRANSLATE("Rimuovi tutte le regole"),
		new BMessage(kMsgRemoveAllLocal));
	removeButton->SetTarget(this);

	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica alla selezione"),
		new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fTypeField)
		.Add(fValueField)
		.Add(fColorControl)
		.Add(fMaxColorControl)
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

void ConditionalFormatWindow::UpdateFieldsForType()
{
	bool isColorScale = (SelectedType() == 2);
	fValueField->SetEnabled(!isColorScale);
	if (isColorScale)
		fMaxColorControl->Show();
	else if (!fMaxColorControl->IsHidden())
		fMaxColorControl->Hide();
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
			if (SelectedType() == 2)
			{
				rgb_color maxColor = fMaxColorControl->ValueAsColor();
				request.AddData("maxColor", B_RGB_COLOR_TYPE, &maxColor, sizeof(rgb_color));
			}
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

		case kMsgTypeChangedLocal:
			UpdateFieldsForType();
			return;
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
