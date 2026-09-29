/*
	ChartSeriesOptionsWindow.cpp

	Vedi ChartSeriesOptionsWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ChartSeriesOptionsWindow.h"

#include <stdlib.h>

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <String.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChartSeriesOptionsWindow"

static const uint32 kMsgApplyLocal = 'sapl';
static const uint32 kMsgTrendlineTypeChangedLocal = 'strl';
static const uint32 kMsgErrorBarModeChangedLocal = 'sebl';

ChartSeriesOptionsWindow::ChartSeriesOptionsWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 440, 300), B_TRANSLATE("Opzioni serie"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fSeriesIndex(-1)
{
	BPopUpMenu* trendlineMenu = new BPopUpMenu("trendlineType");
	trendlineMenu->AddItem(new BMenuItem(B_TRANSLATE("Nessuna"),
		new BMessage(kMsgTrendlineTypeChangedLocal)));
	trendlineMenu->AddItem(new BMenuItem(B_TRANSLATE("Lineare"),
		new BMessage(kMsgTrendlineTypeChangedLocal)));
	trendlineMenu->AddItem(new BMenuItem(B_TRANSLATE("Media mobile"),
		new BMessage(kMsgTrendlineTypeChangedLocal)));
	trendlineMenu->ItemAt(0)->SetMarked(true);
	trendlineMenu->SetTargetForItems(this);
	fTrendlineTypeField = new BMenuField("trendlineType",
		B_TRANSLATE("Linea di tendenza:"), trendlineMenu);

	// Abilitato solo per la media mobile (vedi UpdatePeriodEnabled) --
	// "2" e' il default di Excel, stesso di ChartSeriesOptions().
	fTrendlinePeriodField = new BTextControl("trendlinePeriod",
		B_TRANSLATE("Punti media mobile:"), "2", NULL);

	BPopUpMenu* errorBarMenu = new BPopUpMenu("errorBarMode");
	errorBarMenu->AddItem(new BMenuItem(B_TRANSLATE("Nessuna"),
		new BMessage(kMsgErrorBarModeChangedLocal)));
	errorBarMenu->AddItem(new BMenuItem(B_TRANSLATE("Valore fisso"),
		new BMessage(kMsgErrorBarModeChangedLocal)));
	errorBarMenu->AddItem(new BMenuItem(B_TRANSLATE("Percentuale"),
		new BMessage(kMsgErrorBarModeChangedLocal)));
	errorBarMenu->ItemAt(0)->SetMarked(true);
	errorBarMenu->SetTargetForItems(this);
	fErrorBarModeField = new BMenuField("errorBarMode",
		B_TRANSLATE("Barre d'errore:"), errorBarMenu);

	// Etichetta/contenuto iniziale sovrascritti da UpdateErrorBarValueField
	// sotto (chiamata a fine costruttore) -- "0" e' il valore di default
	// di ChartSeriesOptions().
	fErrorBarValueField = new BTextControl("errorBarValue", B_TRANSLATE("Valore:"), "0", NULL);

	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica"),
		new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fTrendlineTypeField)
		.Add(fTrendlinePeriodField)
		.Add(fErrorBarModeField)
		.Add(fErrorBarValueField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(applyButton)
		.End();

	UpdatePeriodEnabled();
	UpdateErrorBarValueField();
}

void ChartSeriesOptionsWindow::SetOptions(const ChartSeriesOptions& options)
{
	fOptions = options;
	BMenuItem* item = fTrendlineTypeField->Menu()->ItemAt((int32)fOptions.trendlineType);
	if (item)
		item->SetMarked(true);
	BString periodText;
	periodText << fOptions.trendlinePeriod;
	fTrendlinePeriodField->SetText(periodText.String());
	UpdatePeriodEnabled();

	BMenuItem* errorItem = fErrorBarModeField->Menu()->ItemAt((int32)fOptions.errorBarMode);
	if (errorItem)
		errorItem->SetMarked(true);
	BString valueText;
	valueText << fOptions.errorBarValue;
	fErrorBarValueField->SetText(valueText.String());
	UpdateErrorBarValueField();
}

void ChartSeriesOptionsWindow::UpdatePeriodEnabled()
{
	fTrendlinePeriodField->SetEnabled(fOptions.trendlineType == eMovingAverageTrendline);
}

void ChartSeriesOptionsWindow::UpdateErrorBarValueField()
{
	bool enabled = fOptions.errorBarMode != eNoErrorBars;
	fErrorBarValueField->SetEnabled(enabled);
	fErrorBarValueField->SetLabel(fOptions.errorBarMode == ePercentErrorBars
		? B_TRANSLATE("Percentuale (%):") : B_TRANSLATE("Valore:"));
}

void ChartSeriesOptionsWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgTrendlineTypeChangedLocal:
		{
			int32 index = fTrendlineTypeField->Menu()->IndexOf(
				fTrendlineTypeField->Menu()->FindMarked());
			if (index >= 0)
				fOptions.trendlineType = (TrendlineType)index;
			UpdatePeriodEnabled();
			return;
		}

		case kMsgErrorBarModeChangedLocal:
		{
			int32 index = fErrorBarModeField->Menu()->IndexOf(
				fErrorBarModeField->Menu()->FindMarked());
			if (index >= 0)
				fOptions.errorBarMode = (ErrorBarMode)index;
			UpdateErrorBarValueField();
			return;
		}

		case kMsgApplyLocal:
		{
			int period = atoi(fTrendlinePeriodField->Text());
			if (period < 1)
				period = 1;
			fOptions.trendlinePeriod = period;
			fOptions.errorBarValue = atof(fErrorBarValueField->Text());

			BMessage request(kMsgSeriesOptionsRequest);
			request.AddInt32("index", fSeriesIndex);
			request.AddData("options", B_RAW_TYPE, &fOptions, sizeof(ChartSeriesOptions));
			fTarget.SendMessage(&request);
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool ChartSeriesOptionsWindow::QuitRequested()
{
	// Stessa regola di ColorWindow/FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
