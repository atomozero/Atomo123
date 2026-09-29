/*
	ScenarioWindow.cpp

	Vedi ScenarioWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ScenarioWindow.h"
#include "RangeRef.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ScenarioWindow"

static const uint32 kMsgDefineScenarioLocal = 'dfsl';
static const uint32 kMsgDeleteScenarioLocal = 'dlsl';
static const uint32 kMsgShowScenarioLocal = 'shsl';
static const uint32 kMsgSelectScenarioLocal = 'slsl';
// Pulsante "..." (vedi il commento su fRangePickButton in
// ScenarioWindow.h): stesso principio esatto di kMsgRangePickButtonLocal
// in ChartWindow.cpp.
static const uint32 kMsgRangePickButtonLocal = 'rpbl';

ScenarioWindow::ScenarioWindow(BMessenger target)
	:
	BWindow(BRect(150, 150, 480, 520), B_TRANSLATE("Gestione scenari"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fPickingRange(false),
	fTarget(target)
{
	fScenarioList = new BListView("scenarios");
	fScenarioList->SetSelectionMessage(new BMessage(kMsgSelectScenarioLocal));
	fScenarioList->SetTarget(this);
	BScrollView* listScroll = new BScrollView("scenariosScroll", fScenarioList,
		0, false, true);

	fNameField = new BTextControl("name", B_TRANSLATE("Nome:"), "", NULL);

	fRangeField = new BTextControl("range",
		B_TRANSLATE("Celle variabili (un solo intervallo contiguo):"), "", NULL);

	BButton* rangePickButton = new BButton("rangePick", "...",
		new BMessage(kMsgRangePickButtonLocal));
	rangePickButton->SetTarget(this);
	fRangePickButton = rangePickButton;

	BStringView* valuesLabel = new BStringView("valuesLabel",
		B_TRANSLATE("Valori sostitutivi (uno per riga, stesso ordine dell'intervallo):"));

	fValuesField = new BTextView("valuesText");
	fValuesField->SetWordWrap(false);
	fValuesField->MakeEditable(true);
	fValuesField->MakeSelectable(true);
	fValuesField->SetViewColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
	fValuesField->SetLowColor(ui_color(B_DOCUMENT_BACKGROUND_COLOR));
	fValuesField->SetHighColor(ui_color(B_DOCUMENT_TEXT_COLOR));
	BScrollView* valuesScroll = new BScrollView("valuesScroll", fValuesField,
		0, false, true, B_FANCY_BORDER);
	valuesScroll->SetExplicitMinSize(BSize(260, 100));

	fCommentField = new BTextControl("comment", B_TRANSLATE("Commento (facoltativo):"),
		"", NULL);

	BButton* defineButton = new BButton("define", B_TRANSLATE("Aggiungi/Aggiorna"),
		new BMessage(kMsgDefineScenarioLocal));
	defineButton->SetTarget(this);

	BButton* deleteButton = new BButton("delete", B_TRANSLATE("Elimina"),
		new BMessage(kMsgDeleteScenarioLocal));
	deleteButton->SetTarget(this);

	BButton* showButton = new BButton("show", B_TRANSLATE("Mostra"),
		new BMessage(kMsgShowScenarioLocal));
	showButton->SetTarget(this);
	showButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(listScroll)
		.Add(fNameField)
		.AddGroup(B_HORIZONTAL)
			.Add(fRangeField)
			.Add(fRangePickButton)
		.End()
		.Add(valuesLabel)
		.Add(valuesScroll)
		.Add(fCommentField)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(showButton)
			.Add(deleteButton)
			.Add(defineButton)
		.End();
}

void ScenarioWindow::SetScenarios(const std::vector<Scenario>& scenarios)
{
	fScenarioList->MakeEmpty();
	fScenarios = scenarios;

	for (size_t i = 0; i < scenarios.size(); i++)
		fScenarioList->AddItem(new BStringItem(scenarios[i].name.String()));
}

void ScenarioWindow::SendScenarioMessage(uint32 what)
{
	BMessage forward(what);
	forward.AddString("name", fNameField->Text());
	forward.AddString("range", fRangeField->Text());
	forward.AddString("values", fValuesField->Text());
	forward.AddString("comment", fCommentField->Text());
	fTarget.SendMessage(&forward);
}

void ScenarioWindow::CancelPickingIfArmed()
{
	if (!fPickingRange)
		return;
	fPickingRange = false;
	fRangePickButton->SetLabel("...");
	BMessage request(kMsgScenarioRangePickRequest);
	request.AddBool("start", false);
	request.AddMessenger("replyTo", BMessenger(this));
	fTarget.SendMessage(&request);
}

void ScenarioWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgSelectScenarioLocal:
		{
			int32 index = fScenarioList->CurrentSelection();
			if (index >= 0 && (size_t)index < fScenarios.size())
			{
				const Scenario& s = fScenarios[index];
				fNameField->SetText(s.name.String());
				char rangeText[64];
				FormatRangeRef(s.changingCells, rangeText, sizeof(rangeText));
				fRangeField->SetText(rangeText);

				BString joined;
				for (size_t v = 0; v < s.values.size(); v++)
				{
					if (v > 0)
						joined << "\n";
					joined << s.values[v];
				}
				fValuesField->SetText(joined.String());
				fCommentField->SetText(s.comment.String());
			}
			return;
		}

		case kMsgDefineScenarioLocal:
			SendScenarioMessage(kMsgDefineScenario);
			return;

		case kMsgDeleteScenarioLocal:
			SendScenarioMessage(kMsgDeleteScenario);
			return;

		case kMsgShowScenarioLocal:
			SendScenarioMessage(kMsgShowScenario);
			return;

		case kMsgRangePickButtonLocal:
		{
			// Un solo pulsante arma/disarma, stesso principio esatto di
			// ChartWindow::kMsgRangePickButtonLocal.
			if (fPickingRange)
			{
				CancelPickingIfArmed();
				return;
			}
			fPickingRange = true;
			fRangePickButton->SetLabel(B_TRANSLATE("Seleziona..."));
			BMessage request(kMsgScenarioRangePickRequest);
			request.AddBool("start", true);
			request.AddMessenger("replyTo", BMessenger(this));
			fTarget.SendMessage(&request);
			return;
		}

		case kMsgScenarioRangePicked:
		{
			fPickingRange = false;
			fRangePickButton->SetLabel("...");
			BString rangeText;
			if (message->FindString("range", &rangeText) == B_OK)
				fRangeField->SetText(rangeText.String());
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool ScenarioWindow::QuitRequested()
{
	// Se il selettore di intervallo era armato, disarmalo -- stesso
	// principio esatto di ChartWindow::QuitRequested.
	CancelPickingIfArmed();

	// Stessa regola di FindWindow/NameWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
