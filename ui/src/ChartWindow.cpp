/*
	ChartWindow.cpp

	Vedi ChartWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ChartWindow.h"
#include "ChartView.h"
#include "Chart.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <String.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChartWindow"

static const uint32 kMsgDrawLocal = 'drlc';
static const uint32 kMsgInsertLocal = 'inlc';
static const uint32 kMsgTypeChangedLocal = 'tpcl';

ChartWindow::ChartWindow(BMessenger target)
	:
	BWindow(BRect(140, 120, 780, 680), B_TRANSLATE("Grafico"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	// Stesso messaggio di fRangeField (kMsgDrawLocal): digitare un
	// titolo e premere Invio (o "Disegna") lo applica subito
	// all'anteprima, senza bisogno di un pulsante/campo separato.
	fTitleField = new BTextControl("title", B_TRANSLATE("Titolo (facoltativo):"),
		"", new BMessage(kMsgDrawLocal));
	fTitleField->SetTarget(this);

	fRangeField = new BTextControl("range", B_TRANSLATE("Intervallo (due colonne: etichette, valori):"),
		"A1:B5", new BMessage(kMsgDrawLocal));
	fRangeField->SetTarget(this);
	fRangeField->MakeFocus(true);

	BButton* drawButton = new BButton("draw", B_TRANSLATE("Disegna"), new BMessage(kMsgDrawLocal));
	drawButton->SetTarget(this);

	// Barre come voce predefinita (indice 0), stesso ordine dei valori
	// dell'enum ChartType in Chart.h -- SelectedType() sotto si basa
	// su questa corrispondenza posizionale.
	BPopUpMenu* typeMenu = new BPopUpMenu("typeMenu");
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Barre"), new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Linee"), new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Torta"), new BMessage(kMsgTypeChangedLocal)));
	typeMenu->ItemAt(0)->SetMarked(true);
	fTypeField = new BMenuField("type", B_TRANSLATE("Tipo:"), typeMenu);
	fTypeField->Menu()->SetTargetForItems(this);

	fChartView = new ChartView();

	fDestField = new BTextControl("dest", B_TRANSLATE("Cella di destinazione nel foglio:"),
		"D1", NULL);

	BButton* insertButton = new BButton("insert", B_TRANSLATE("Inserisci nel foglio"),
		new BMessage(kMsgInsertLocal));
	insertButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fTitleField)
		.AddGroup(B_HORIZONTAL)
			.Add(fRangeField)
			.Add(fTypeField)
			.Add(drawButton)
		.End()
		.Add(fChartView)
		.AddGroup(B_HORIZONTAL)
			.Add(fDestField)
			.AddGlue()
			.Add(insertButton)
		.End();
}

ChartType ChartWindow::SelectedType() const
{
	// Corrispondenza posizionale con l'ordine di inserimento in
	// BPopUpMenu sopra (0=Barre, 1=Linee, 2=Torta) e con i valori
	// dell'enum ChartType in Chart.h -- niente da cercare per nome.
	int32 index = fTypeField->Menu()->IndexOf(fTypeField->Menu()->FindMarked());
	switch (index)
	{
		case 1: return eLineChart;
		case 2: return ePieChart;
		default: return eBarChart;
	}
}

void ChartWindow::LoadRange(const char* rangeText)
{
	fRangeField->SetText(rangeText);
	RequestDraw();
}

void ChartWindow::RequestDraw()
{
	fChartView->SetTitle(fTitleField->Text());
	BMessage request(kMsgChartRequest);
	request.AddString("range", fRangeField->Text());
	fTarget.SendMessage(&request);
}

void ChartWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgDrawLocal:
			RequestDraw();
			return;

		case kMsgTypeChangedLocal:
			fChartView->SetChartType(SelectedType());
			return;

		case kMsgInsertLocal:
		{
			BMessage request(kMsgChartInsert);
			request.AddString("range", fRangeField->Text());
			request.AddString("dest", fDestField->Text());
			request.AddInt32("type", (int32)SelectedType());
			request.AddString("title", fTitleField->Text());
			fTarget.SendMessage(&request);
			return;
		}

		case kMsgChartData:
		{
			std::vector<ChartSeries> data;
			BString label;
			double value;
			for (int32 i = 0; message->FindString("label", i, &label) == B_OK
					&& message->FindDouble("value", i, &value) == B_OK; i++)
			{
				ChartSeries s;
				s.label = label;
				s.value = value;
				data.push_back(s);
			}
			fChartView->SetData(data);
			return;
		}

		case kMsgChartDataMulti:
		{
			// Codifica piatta dello stesso MultiChartData di Chart.h:
			// tutte le categorie, poi tutti i nomi di serie, poi tutti
			// i valori in ordine "serie-maggiore" (prima tutte le
			// categorie della serie 0, poi tutte quelle della serie 1,
			// ...) -- ricostruito qui nello stesso ordine, vedi
			// MainWindow::HandleChartRequest dove viene scritto.
			MultiChartData data;
			BString category;
			for (int32 i = 0; message->FindString("category", i, &category) == B_OK; i++)
				data.categories.push_back(category);

			BString seriesName;
			for (int32 s = 0; message->FindString("seriesName", s, &seriesName) == B_OK; s++)
				data.seriesNames.push_back(seriesName);

			data.values.resize(data.seriesNames.size());
			int32 k = 0;
			for (size_t s = 0; s < data.seriesNames.size(); s++)
			{
				data.values[s].resize(data.categories.size());
				for (size_t c = 0; c < data.categories.size(); c++)
				{
					double v = 0;
					message->FindDouble("value", k++, &v);
					data.values[s][c] = v;
				}
			}
			fChartView->SetMultiData(data);
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool ChartWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
