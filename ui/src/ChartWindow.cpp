/*
	ChartWindow.cpp

	Vedi ChartWindow.h.
*/

#include "ChartWindow.h"
#include "ChartView.h"
#include "Chart.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <String.h>
#include <TextControl.h>

static const uint32 kMsgDrawLocal = 'drlc';
static const uint32 kMsgInsertLocal = 'inlc';

ChartWindow::ChartWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 580, 500), "Grafico a barre",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fRangeField = new BTextControl("range", "Intervallo (due colonne: etichette, valori):",
		"A1:B5", new BMessage(kMsgDrawLocal));
	fRangeField->SetTarget(this);
	fRangeField->MakeFocus(true);

	BButton* drawButton = new BButton("draw", "Disegna", new BMessage(kMsgDrawLocal));
	drawButton->SetTarget(this);

	fChartView = new ChartView();

	fDestField = new BTextControl("dest", "Cella di destinazione nel foglio:",
		"D1", NULL);

	BButton* insertButton = new BButton("insert", "Inserisci nel foglio",
		new BMessage(kMsgInsertLocal));
	insertButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.AddGroup(B_HORIZONTAL)
			.Add(fRangeField)
			.Add(drawButton)
		.End()
		.Add(fChartView)
		.AddGroup(B_HORIZONTAL)
			.Add(fDestField)
			.AddGlue()
			.Add(insertButton)
		.End();
}

void ChartWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgDrawLocal:
		{
			BMessage request(kMsgChartRequest);
			request.AddString("range", fRangeField->Text());
			fTarget.SendMessage(&request);
			return;
		}

		case kMsgInsertLocal:
		{
			BMessage request(kMsgChartInsert);
			request.AddString("range", fRangeField->Text());
			request.AddString("dest", fDestField->Text());
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
	}

	BWindow::MessageReceived(message);
}

bool ChartWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
