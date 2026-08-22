/*
	PageSetupWindow.cpp

	Vedi PageSetupWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PageSetupWindow.h"

#include <cstdlib>

#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <RadioButton.h>
#include <StringView.h>
#include <TextControl.h>

#include "PrintPreviewView.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PageSetupWindow"

static const uint32 kMsgApplyLocal = 'apps';
static const uint32 kMsgPrintLocal = 'pspr';
// I quattro BRadioButton di scala si "accorgono" a vicenda solo perche'
// condividono lo stesso genitore (comportamento standard di
// BRadioButton) -- questo messaggio abilita/disabilita il campo
// percentuale in base a quale e' selezionato E rigenera l'anteprima
// (kMsgFieldChanged sotto fa lo stesso per i campi di testo).
static const uint32 kMsgScaleModeChanged = 'scmd';
static const uint32 kMsgFieldChanged = 'fldc';
static const uint32 kMsgPrevPage = 'prvp';
static const uint32 kMsgNextPage = 'nxtp';

PageSetupWindow::PageSetupWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 780, 620), B_TRANSLATE("Imposta pagina"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fMarginTopField = new BTextControl("marginTop", B_TRANSLATE("Superiore:"), "2.0",
		new BMessage(kMsgFieldChanged));
	fMarginBottomField = new BTextControl("marginBottom", B_TRANSLATE("Inferiore:"), "2.0",
		new BMessage(kMsgFieldChanged));
	fMarginLeftField = new BTextControl("marginLeft", B_TRANSLATE("Sinistro:"), "2.0",
		new BMessage(kMsgFieldChanged));
	fMarginRightField = new BTextControl("marginRight", B_TRANSLATE("Destro:"), "2.0",
		new BMessage(kMsgFieldChanged));

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

	fScalePercentField = new BTextControl("scalePercent", B_TRANSLATE("Percentuale:"), "100",
		new BMessage(kMsgFieldChanged));

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

	fPreviewView = new PrintPreviewView();

	fPageLabel = new BStringView("pageLabel", B_TRANSLATE("Nessuna anteprima"));
	fPrevPageButton = new BButton("prevPage", B_TRANSLATE("‹"), new BMessage(kMsgPrevPage));
	fNextPageButton = new BButton("nextPage", B_TRANSLATE("›"), new BMessage(kMsgNextPage));
	fPrevPageButton->SetEnabled(false);
	fNextPageButton->SetEnabled(false);

	BButton* printButton = new BButton("print", B_TRANSLATE("Stampa" B_UTF8_ELLIPSIS),
		new BMessage(kMsgPrintLocal));
	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica"), new BMessage(kMsgApplyLocal));
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.AddGroup(B_HORIZONTAL, 8)
			.AddGroup(B_VERTICAL, 4)
				.Add(fPreviewView)
				.AddGroup(B_HORIZONTAL)
					.AddGlue()
					.Add(fPrevPageButton)
					.Add(fPageLabel)
					.Add(fNextPageButton)
					.AddGlue()
				.End()
			.End()
			.AddGroup(B_VERTICAL, 8)
				.Add(marginsBox)
				.Add(scaleBox)
				.AddGlue()
			.End()
		.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(printButton)
			.Add(applyButton)
		.End();

	fMarginTopField->SetTarget(this);
	fMarginBottomField->SetTarget(this);
	fMarginLeftField->SetTarget(this);
	fMarginRightField->SetTarget(this);
	fScalePercentField->SetTarget(this);
	fScalePercentRadio->SetTarget(this);
	fScaleFitWidthRadio->SetTarget(this);
	fScaleFitHeightRadio->SetTarget(this);
	fScaleFitBothRadio->SetTarget(this);
	fPrevPageButton->SetTarget(this);
	fNextPageButton->SetTarget(this);
	printButton->SetTarget(this);
	applyButton->SetTarget(this);
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

void PageSetupWindow::SetPreviewPages(std::vector<BBitmap*> pages)
{
	fPreviewView->SetPages(pages);
	_UpdatePageNavControls();
}

void PageSetupWindow::_UpdatePageNavControls()
{
	int count = fPreviewView->PageCount();
	if (count == 0)
	{
		fPageLabel->SetText(B_TRANSLATE("Nessuna anteprima"));
		fPrevPageButton->SetEnabled(false);
		fNextPageButton->SetEnabled(false);
		return;
	}

	int index = fPreviewView->PageIndex();
	BString label;
	label << B_TRANSLATE("Pagina") << " " << (index + 1) << " " << B_TRANSLATE("di") << " "
		<< count;
	fPageLabel->SetText(label.String());
	fPrevPageButton->SetEnabled(index > 0);
	fNextPageButton->SetEnabled(index < count - 1);
}

BMessage PageSetupWindow::_BuildSettingsMessage(uint32 what) const
{
	double marginTop = atof(fMarginTopField->Text());
	double marginBottom = atof(fMarginBottomField->Text());
	double marginLeft = atof(fMarginLeftField->Text());
	double marginRight = atof(fMarginRightField->Text());
	// Margini negativi non hanno senso (l'area stampabile diventerebbe
	// piu' grande della pagina stessa, non solo "senza bordo") --
	// azzerati invece di rifiutare silenziosamente la richiesta.
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
	// Una percentuale fuori da un intervallo sensato (Excel stesso
	// limita a 10-400%) ricade sul 100% predefinito, stesso principio
	// gia' seguito per l'intervallo di salvataggio automatico in
	// PreferencesWindow.
	if (scalePercent < 10 || scalePercent > 400)
		scalePercent = 100;

	BMessage request(what);
	request.AddDouble("marginTop", marginTop);
	request.AddDouble("marginBottom", marginBottom);
	request.AddDouble("marginLeft", marginLeft);
	request.AddDouble("marginRight", marginRight);
	request.AddInt32("scaleMode", scaleMode);
	request.AddDouble("scalePercent", scalePercent);
	return request;
}

void PageSetupWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgScaleModeChanged:
		{
			// Il campo percentuale ha senso solo in modalita' "Adatta
			// al: NN%" -- disabilitato (non nascosto, la finestra non
			// e' ridimensionabile) nelle altre tre, che calcolano la
			// scala da sole al momento della stampa/anteprima.
			fScalePercentField->SetEnabled(fScalePercentRadio->Value() == B_CONTROL_ON);
			BMessage preview = _BuildSettingsMessage(kMsgPageSetupPreviewRequest);
			fTarget.SendMessage(&preview);
			break;
		}

		case kMsgFieldChanged:
		{
			BMessage preview = _BuildSettingsMessage(kMsgPageSetupPreviewRequest);
			fTarget.SendMessage(&preview);
			break;
		}

		case kMsgApplyLocal:
		{
			BMessage request = _BuildSettingsMessage(kMsgPageSetupRequest);
			fTarget.SendMessage(&request);
			break;
		}

		case kMsgPrintLocal:
		{
			BMessage request = _BuildSettingsMessage(kMsgPageSetupPrintRequest);
			fTarget.SendMessage(&request);
			break;
		}

		case kMsgPrevPage:
			fPreviewView->SetPageIndex(fPreviewView->PageIndex() - 1);
			_UpdatePageNavControls();
			break;

		case kMsgNextPage:
			fPreviewView->SetPageIndex(fPreviewView->PageIndex() + 1);
			_UpdatePageNavControls();
			break;

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
