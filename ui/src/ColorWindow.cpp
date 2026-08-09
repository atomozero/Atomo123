/*
	ColorWindow.cpp

	Vedi ColorWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ColorWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <ColorControl.h>
#include <LayoutBuilder.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ColorWindow"

static const uint32 kMsgApplyLocal = 'aply';

ColorWindow::ColorWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 460, 340), B_TRANSLATE("Colore"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fColorTarget(eTextColor)
{
	fColorControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 8, "colorControl");

	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica"), new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fColorControl)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(applyButton)
		.End();
}

void ColorWindow::SetMode(ColorTarget target, rgb_color initial)
{
	fColorTarget = target;
	switch (target)
	{
		case eBackgroundColor: SetTitle(B_TRANSLATE("Colore sfondo")); break;
		case eBorderColor: SetTitle(B_TRANSLATE("Colore bordo")); break;
		default: SetTitle(B_TRANSLATE("Colore testo")); break;
	}
	fColorControl->SetValue(initial);
}

void ColorWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgApplyLocal)
	{
		rgb_color color = fColorControl->ValueAsColor();
		BMessage request(kMsgColorRequest);
		request.AddData("color", B_RGB_COLOR_TYPE, &color, sizeof(rgb_color));
		request.AddInt32("target", (int32)fColorTarget);
		fTarget.SendMessage(&request);
		return;
	}

	BWindow::MessageReceived(message);
}

bool ColorWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
