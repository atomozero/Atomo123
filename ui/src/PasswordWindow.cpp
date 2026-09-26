/*
	PasswordWindow.cpp

	Vedi PasswordWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PasswordWindow.h"

#include <Alert.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PasswordWindow"

static const uint32 kMsgPasswordOKLocal = 'pwol';
static const uint32 kMsgPasswordCancelLocal = 'pwcl';

PasswordWindow::PasswordWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 420, 260), B_TRANSLATE("Password"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fSetMode(true)
{
	fPrompt = new BStringView("prompt", "");

	fPasswordField = new BTextControl("password", B_TRANSLATE("Password:"), "",
		new BMessage(kMsgPasswordOKLocal));
	fPasswordField->TextView()->HideTyping(true);

	fConfirmField = new BTextControl("confirm", B_TRANSLATE("Conferma password:"), "",
		new BMessage(kMsgPasswordOKLocal));
	fConfirmField->TextView()->HideTyping(true);

	BButton* cancelButton = new BButton("cancel", B_TRANSLATE("Annulla"),
		new BMessage(kMsgPasswordCancelLocal));
	cancelButton->SetTarget(this);

	BButton* okButton = new BButton("ok", B_TRANSLATE("OK"),
		new BMessage(kMsgPasswordOKLocal));
	okButton->SetTarget(this);
	okButton->MakeDefault(true);

	fPasswordField->SetTarget(this);
	fConfirmField->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fPrompt)
		.Add(fPasswordField)
		.Add(fConfirmField)
		.AddGroup(B_HORIZONTAL)
			.Add(cancelButton)
			.AddGlue()
			.Add(okButton)
		.End();
}

void PasswordWindow::PrepareForSet()
{
	fSetMode = true;
	fPrompt->SetText(B_TRANSLATE(
		"Password facoltativa per questo foglio (vuota = proteggi senza password):"));
	fPasswordField->SetText("");
	fConfirmField->SetText("");
	if (fConfirmField->IsHidden())
		fConfirmField->Show();
	fPasswordField->MakeFocus(true);
}

void PasswordWindow::PrepareForVerify()
{
	fSetMode = false;
	fPrompt->SetText(B_TRANSLATE("Questo foglio e' protetto da password:"));
	fPasswordField->SetText("");
	fConfirmField->SetText("");
	if (!fConfirmField->IsHidden())
		fConfirmField->Hide();
	fPasswordField->MakeFocus(true);
}

void PasswordWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgPasswordOKLocal:
		{
			if (fSetMode)
			{
				BString pw = fPasswordField->Text();
				BString confirm = fConfirmField->Text();
				if (pw != confirm)
				{
					// Stesso principio di GuardProtectedEdit altrove in
					// questo programma: un vero BAlert bloccante, la
					// finestra resta aperta cosi' l'utente puo' correggere
					// senza dover riaprire tutto da capo.
					BAlert* alert = new BAlert(B_TRANSLATE("Password"),
						B_TRANSLATE("Le due password non corrispondono."),
						B_TRANSLATE("OK"));
					alert->Go();
					return;
				}
			}

			BMessage request(kMsgPasswordCommit);
			request.AddBool("setMode", fSetMode);
			request.AddString("password", fPasswordField->Text());
			fTarget.SendMessage(&request);
			Hide();
			return;
		}

		case kMsgPasswordCancelLocal:
			// Niente messaggio inviato: vedi il commento in cima a
			// PasswordWindow.h sul perche' "annullare" non ha bisogno di
			// un contratto a parte.
			Hide();
			return;
	}

	BWindow::MessageReceived(message);
}

bool PasswordWindow::QuitRequested()
{
	// Stessa regola di RenameSheetWindow/GoToWindow: resta nascosta e
	// riusabile.
	Hide();
	return false;
}
