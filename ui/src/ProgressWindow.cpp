/*
	ProgressWindow.cpp

	Vedi ProgressWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ProgressWindow.h"

#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StatusBar.h>
#include <StringView.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ProgressWindow"

static const uint32 kMsgProgressUpdate = 'prgu';

ProgressWindow::ProgressWindow()
	:
	BWindow(BRect(0, 0, 380, 90), B_TRANSLATE("Apertura file"),
		B_MODAL_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_NOT_CLOSABLE
			| B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fSelf(this)
{
	fStatusBar = new BStatusBar("progress", B_TRANSLATE("Apertura del file in corso..."));
	fStatusBar->SetMaxValue(100.0f);

	fDetailView = new BStringView("detail", "");
	fDetailView->SetFont(be_plain_font);
	float size = be_plain_font->Size() * 0.9f;
	BFont detailFont(be_plain_font);
	detailFont.SetSize(size);
	fDetailView->SetFont(&detailFont);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(12, 12, 12, 12)
		.Add(fStatusBar)
		.Add(fDetailView);

	CenterOnScreen();
}

void ProgressWindow::Update(float fraction, const char* phaseText, const char* detail)
{
	BMessage msg(kMsgProgressUpdate);
	msg.AddFloat("fraction", fraction);
	if (phaseText)
		msg.AddString("phase", phaseText);
	if (detail)
		msg.AddString("detail", detail);
	fSelf.SendMessage(&msg);
}

void ProgressWindow::Finish()
{
	fSelf.SendMessage(B_QUIT_REQUESTED);
}

void ProgressWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgProgressUpdate)
	{
		float fraction = 0.0f;
		message->FindFloat("fraction", &fraction);
		if (fraction < 0.0f)
			fraction = 0.0f;
		if (fraction > 1.0f)
			fraction = 1.0f;

		BString phase;
		if (message->FindString("phase", &phase) != B_OK)
			phase = fStatusBar->Label();

		fStatusBar->Reset(phase);
		fStatusBar->SetTo(fraction * 100.0f);

		BString detail;
		if (message->FindString("detail", &detail) == B_OK)
			fDetailView->SetText(detail);
		else
			fDetailView->SetText("");
		return;
	}

	BWindow::MessageReceived(message);
}

bool ProgressWindow::QuitRequested()
{
	// Stessa regola di FindWindow/GoToWindow: resta nascosta e
	// riusabile per l'apertura successiva, invece di distruggersi.
	Hide();
	return false;
}
