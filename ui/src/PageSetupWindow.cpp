/*
	PageSetupWindow.cpp

	Vedi PageSetupWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PageSetupWindow.h"

#include <cstdlib>

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <RadioButton.h>
#include <StringView.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PageSetupWindow"

static const uint32 kMsgApplyLocal = 'apps';
// I quattro BRadioButton di scala si "accorgono" a vicenda solo perche'
// condividono lo stesso genitore (comportamento standard di
// BRadioButton) -- questo messaggio serve solo ad abilitare/disabilitare
// il campo percentuale in base a quale e' selezionato, non a scegliere
// la scala stessa (letta di nuovo da zero al momento di "Applica").
static const uint32 kMsgScaleModeChanged = 'scmd';

PageSetupWindow::PageSetupWindow(BMessenger target)
	:
	BWindow(BRect(200, 200, 460, 420), B_TRANSLATE("Imposta pagina"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fMarginTopField = new BTextControl("marginTop", B_TRANSLATE("Superiore:"), "2.0", NULL);
	fMarginBottomField = new BTextControl("marginBottom", B_TRANSLATE("Inferiore:"), "2.0", NULL);
	fMarginLeftField = new BTextControl("marginLeft", B_TRANSLATE("Sinistro:"), "2.0", NULL);
	fMarginRightField = new BTextControl("marginRight", B_TRANSLATE("Destro:"), "2.0", NULL);

	BStringView* marginUnitHint = new BStringView("marginUnitHint",
		B_TRANSLATE("Valori in centimetri"));
	marginUnitHint->SetFont(be_plain_font);

	BBox* marginsBox = new BBox("marginsBox");
	marginsBox->SetLabel(B_TRANSLATE("Margini"));
	BLayoutBuilder::Group<>(marginsBox, B_VERTICAL, 6)
		.SetInsets(8, marginsBox->TopBorderOffset() + 8, 8, 8)
		.Add(fMarginTopField)
		.Add(fMarginBottomField)
		.Add(fMarginLeftField)
		.Add(fMarginRightField)
		.Add(marginUnitHint);

	fScalePercentField = new BTextControl("scalePercent", B_TRANSLATE("Percentuale:"), "100", NULL);

	fScalePercentRadio = new BRadioButton("scalePercentRadio",
		B_TRANSLATE("Adatta al:"), new BMessage(kMsgScaleModeChanged));
	fScaleFitWidthRadio = new BRadioButton("scaleFitWidthRadio",
		B_TRANSLATE("Larghezza di una pagina"), new BMessage(kMsgScaleModeChanged));
	fScaleFitHeightRadio = new BRadioButton("scaleFitHeightRadio",
		B_TRANSLATE("Altezza di una pagina"), new BMessage(kMsgScaleModeChanged));
	fScaleFitBothRadio = new BRadioButton("scaleFitBothRadio",
		B_TRANSLATE("Una sola pagina"), new BMessage(kMsgScaleModeChanged));
	fScalePercentRadio->SetValue(B_CONTROL_ON);

	BBox* scaleBox = new BBox("scaleBox");
	scaleBox->SetLabel(B_TRANSLATE("Scala"));
	BLayoutBuilder::Group<>(scaleBox, B_VERTICAL, 6)
		.SetInsets(8, scaleBox->TopBorderOffset() + 8, 8, 8)
		.AddGroup(B_HORIZONTAL)
			.Add(fScalePercentRadio)
			.Add(fScalePercentField)
			.AddGlue()
		.End()
		.Add(fScaleFitWidthRadio)
		.Add(fScaleFitHeightRadio)
		.Add(fScaleFitBothRadio);

	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica"), new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(marginsBox)
		.Add(scaleBox)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(applyButton)
		.End();

	fScalePercentRadio->SetTarget(this);
	fScaleFitWidthRadio->SetTarget(this);
	fScaleFitHeightRadio->SetTarget(this);
	fScaleFitBothRadio->SetTarget(this);
}

void PageSetupWindow::SetValues(double marginTop, double marginBottom, double marginLeft,
	double marginRight, int scaleMode, double scalePercent)
{
	BString s;
	s << marginTop;
	fMarginTopField->SetText(s.String());
	s = "";
	s << marginBottom;
	fMarginBottomField->SetText(s.String());
	s = "";
	s << marginLeft;
	fMarginLeftField->SetText(s.String());
	s = "";
	s << marginRight;
	fMarginRightField->SetText(s.String());

	s = "";
	s << scalePercent;
	fScalePercentField->SetText(s.String());

	fScalePercentRadio->SetValue(scaleMode == 0 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScaleFitWidthRadio->SetValue(scaleMode == 1 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScaleFitHeightRadio->SetValue(scaleMode == 2 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScaleFitBothRadio->SetValue(scaleMode == 3 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScalePercentField->SetEnabled(scaleMode == 0);
}

void PageSetupWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgScaleModeChanged:
			// Il campo percentuale ha senso solo in modalita' "Adatta
			// al: NN%" -- disabilitato (non nascosto, la finestra non
			// e' ridimensionabile) nelle altre tre, che calcolano la
			// scala da sole al momento della stampa.
			fScalePercentField->SetEnabled(fScalePercentRadio->Value() == B_CONTROL_ON);
			break;

		case kMsgApplyLocal:
		{
			double marginTop = atof(fMarginTopField->Text());
			double marginBottom = atof(fMarginBottomField->Text());
			double marginLeft = atof(fMarginLeftField->Text());
			double marginRight = atof(fMarginRightField->Text());
			// Margini negativi non hanno senso (l'area stampabile
			// diventerebbe piu' grande della pagina stessa, non solo
			// "senza bordo") -- azzerati invece di rifiutare
			// silenziosamente la richiesta.
			if (marginTop < 0) marginTop = 0;
			if (marginBottom < 0) marginBottom = 0;
			if (marginLeft < 0) marginLeft = 0;
			if (marginRight < 0) marginRight = 0;

			int scaleMode = 0;
			if (fScaleFitWidthRadio->Value() == B_CONTROL_ON)
				scaleMode = 1;
			else if (fScaleFitHeightRadio->Value() == B_CONTROL_ON)
				scaleMode = 2;
			else if (fScaleFitBothRadio->Value() == B_CONTROL_ON)
				scaleMode = 3;

			double scalePercent = atof(fScalePercentField->Text());
			// Una percentuale fuori da un intervallo sensato (Excel
			// stesso limita a 10-400%) ricade sul 100% predefinito,
			// stesso principio gia' seguito per l'intervallo di
			// salvataggio automatico in PreferencesWindow.
			if (scalePercent < 10 || scalePercent > 400)
				scalePercent = 100;

			BMessage request(kMsgPageSetupRequest);
			request.AddDouble("marginTop", marginTop);
			request.AddDouble("marginBottom", marginBottom);
			request.AddDouble("marginLeft", marginLeft);
			request.AddDouble("marginRight", marginRight);
			request.AddInt32("scaleMode", scaleMode);
			request.AddDouble("scalePercent", scalePercent);
			fTarget.SendMessage(&request);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

bool PageSetupWindow::QuitRequested()
{
	// Stessa regola di PreferencesWindow/FindWindow: resta nascosta e
	// riusabile, non viene mai davvero distrutta chiudendola con la X.
	Hide();
	return false;
}
