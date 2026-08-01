/*
	ColorWindow.cpp

	Vedi ColorWindow.h.
*/

#include "ColorWindow.h"

#include <Button.h>
#include <ColorControl.h>
#include <LayoutBuilder.h>

static const uint32 kMsgApplyLocal = 'aply';

ColorWindow::ColorWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 460, 340), "Colore",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fBackground(false)
{
	fColorControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 8, "colorControl");

	BButton* applyButton = new BButton("apply", "Applica", new BMessage(kMsgApplyLocal));
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

void ColorWindow::SetMode(bool background, rgb_color initial)
{
	fBackground = background;
	SetTitle(background ? "Colore sfondo" : "Colore testo");
	fColorControl->SetValue(initial);
}

void ColorWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgApplyLocal)
	{
		rgb_color color = fColorControl->ValueAsColor();
		BMessage request(kMsgColorRequest);
		request.AddData("color", B_RGB_COLOR_TYPE, &color, sizeof(rgb_color));
		request.AddBool("background", fBackground);
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
