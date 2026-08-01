/*
	ClickableStringView.cpp

	Vedi ClickableStringView.h.
*/

#include "ClickableStringView.h"

#include <Cursor.h>
#include <Message.h>
#include <Window.h>

ClickableStringView::ClickableStringView(const char* name, const char* text)
	:
	BStringView(name, text),
	fClickMessage(NULL)
{
}

ClickableStringView::~ClickableStringView()
{
	delete fClickMessage;
}

void ClickableStringView::AttachedToWindow()
{
	BStringView::AttachedToWindow();
	// Colore da collegamento ipertestuale, solo se c'e' davvero un
	// messaggio da inviare al click.
	if (fClickMessage != NULL)
		SetHighColor(40, 80, 200);
}

void ClickableStringView::SetClickMessage(BMessage* message)
{
	delete fClickMessage;
	fClickMessage = message;
}

void ClickableStringView::MouseDown(BPoint /*where*/)
{
	if (fClickMessage == NULL || Window() == NULL)
		return;
	Window()->PostMessage(fClickMessage);
}

void ClickableStringView::MouseMoved(BPoint /*where*/, uint32 /*code*/,
	const BMessage* /*message*/)
{
	if (fClickMessage == NULL)
		return;
	BCursor link(B_CURSOR_ID_FOLLOW_LINK);
	SetViewCursor(&link);
}
