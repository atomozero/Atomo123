/*
	ChartWindow.cpp

	Vedi ChartWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ChartWindow.h"
#include "ChartView.h"
#include "Chart.h"

#include <map>
#include <string>

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChartWindow"

static const uint32 kMsgDrawLocal = 'drlc';
static const uint32 kMsgInsertLocal = 'inlc';
static const uint32 kMsgTypeChangedLocal = 'tpcl';
// Una checkbox per serie (Fase 19): tutte le checkbox della riga
// condividono lo stesso "what", l'indice della serie viaggia nel
// campo "index" del BMessage di ognuna (vedi RebuildSeriesCheckboxes).
static const uint32 kMsgSeriesToggleLocal = 'stvl';

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
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Area"), new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Dispersione (XY)"), new BMessage(kMsgTypeChangedLocal)));
	typeMenu->ItemAt(0)->SetMarked(true);
	fTypeField = new BMenuField("type", B_TRANSLATE("Tipo:"), typeMenu);
	fTypeField->Menu()->SetTargetForItems(this);

	fChartView = new ChartView();

	// Riquadro "Mostra valori per serie" (Fase 19): titolo del BBox
	// (stessa convenzione di raggruppamento di PreferencesWindow/
	// ConditionalFormatWindow) + un suggerimento in corpo minore che
	// spiega cosa fanno davvero le checkbox -- senza, la riga di
	// checkbox appariva senza contesto e si poteva credere che togliere
	// la spunta nascondesse l'intera serie invece della sola etichetta
	// numerica. Titolo e suggerimento sono creati una sola volta qui;
	// solo fSeriesCheckboxRow al suo interno viene svuotato/ripopolato
	// da RebuildSeriesCheckboxes/ClearSeriesCheckboxes a ogni richiesta.
	fSeriesCheckboxBox = new BBox("seriesCheckboxBox");
	fSeriesCheckboxBox->SetLabel(B_TRANSLATE("Mostra valori per serie"));

	BStringView* seriesHint = new BStringView("seriesHint",
		B_TRANSLATE("Deseleziona una serie per nascondere solo le sue etichette numeriche "
			"nel grafico; la serie resta comunque visibile."));
	BFont hintFont(be_plain_font);
	hintFont.SetSize(be_plain_font->Size() - 1);
	seriesHint->SetFont(&hintFont);
	seriesHint->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.7));

	// BGroupLayout invece di BLayoutBuilder per la riga vera e propria
	// perche' le checkbox al suo interno vanno aggiunte/rimosse
	// dinamicamente dopo la costruzione, non solo una volta come il
	// resto della finestra.
	fSeriesCheckboxRow = new BView("seriesCheckboxes", 0);
	fSeriesCheckboxRow->SetLayout(new BGroupLayout(B_HORIZONTAL, 8));

	BLayoutBuilder::Group<>(fSeriesCheckboxBox, B_VERTICAL, 4)
		.SetInsets(8, fSeriesCheckboxBox->TopBorderOffset() + 6, 8, 6)
		.Add(seriesHint)
		.Add(fSeriesCheckboxRow);
	// Nascosto finche' non arriva un grafico a serie multiple (vedi
	// ClearSeriesCheckboxes/RebuildSeriesCheckboxes): un riquadro con
	// titolo ma senza nemmeno una checkbox sarebbe fuorviante quanto la
	// vecchia riga vuota.
	fSeriesCheckboxBox->Hide();

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
		.Add(fSeriesCheckboxBox)
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
	// BPopUpMenu sopra (0=Barre, 1=Linee, 2=Torta, 3=Area,
	// 4=Dispersione) e con i valori dell'enum ChartType in Chart.h --
	// niente da cercare per nome.
	int32 index = fTypeField->Menu()->IndexOf(fTypeField->Menu()->FindMarked());
	switch (index)
	{
		case 1: return eLineChart;
		case 2: return ePieChart;
		case 3: return eAreaChart;
		case 4: return eScatterChart;
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
	// Il tipo scelto viaggia con la richiesta (Fase 35): un grafico a
	// dispersione (Chart.h, ScatterPoint) legge l'intervallo in un modo
	// completamente diverso (entrambe le colonne numeriche, niente
	// etichetta) da MainWindow::HandleChartRequest, che altrimenti non
	// avrebbe modo di saperlo dalla sola forma dell'intervallo (due
	// colonne e' anche la forma normale di un grafico a barre/linee).
	request.AddInt32("type", (int32)SelectedType());
	fTarget.SendMessage(&request);
}

void ChartWindow::ClearSeriesCheckboxes()
{
	while (fSeriesCheckboxRow->CountChildren() > 0)
	{
		BView* child = fSeriesCheckboxRow->ChildAt(0);
		fSeriesCheckboxRow->RemoveChild(child);
		delete child;
	}
	fSeriesCheckboxes.clear();
	// Nessuna serie da elencare: il riquadro intero sparisce invece di
	// restare visibile ma vuoto (vedi il commento nel costruttore).
	if (!fSeriesCheckboxBox->IsHidden())
		fSeriesCheckboxBox->Hide();
}

void ChartWindow::RebuildSeriesCheckboxes(MultiChartData* data)
{
	// Preserva lo stato "spuntata/non spuntata" per nome di serie:
	// senza questo, ridisegnare lo stesso intervallo (es. premendo di
	// nuovo Invio nel campo Intervallo) resetterebbe ogni volta le
	// checkbox a "tutte spuntate", cancellando una scelta gia' fatta
	// dall'utente.
	std::map<std::string, bool> previousState;
	for (size_t i = 0; i < fSeriesCheckboxes.size(); i++)
		previousState[fSeriesCheckboxes[i]->Label()] = fSeriesCheckboxes[i]->Value() != 0;

	ClearSeriesCheckboxes();

	data->showValues.resize(data->seriesNames.size());
	for (size_t s = 0; s < data->seriesNames.size(); s++)
	{
		bool checked = true;
		std::map<std::string, bool>::iterator it =
			previousState.find(data->seriesNames[s].String());
		if (it != previousState.end())
			checked = it->second;
		data->showValues[s] = checked;

		BMessage* msg = new BMessage(kMsgSeriesToggleLocal);
		msg->AddInt32("index", (int32)s);
		BCheckBox* cb = new BCheckBox(data->seriesNames[s].String(),
			data->seriesNames[s].String(), msg);
		cb->SetTarget(this);
		cb->SetValue(checked ? B_CONTROL_ON : B_CONTROL_OFF);
		fSeriesCheckboxRow->AddChild(cb);
		fSeriesCheckboxes.push_back(cb);
	}

	// Almeno una serie da elencare: mostra (o tieni visibile) il
	// riquadro col titolo/suggerimento -- vedi ClearSeriesCheckboxes
	// per il caso opposto.
	if (!data->seriesNames.empty() && fSeriesCheckboxBox->IsHidden())
		fSeriesCheckboxBox->Show();
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
			// Nessuna checkbox per un grafico a singola serie: niente
			// da scegliere, il valore e' sempre mostrato (come da
			// sempre).
			ClearSeriesCheckboxes();

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

		case kMsgChartDataScatter:
		{
			// Nessuna checkbox qui neanche: un grafico a dispersione non
			// ha serie multiple, stesso principio di kMsgChartData sopra.
			ClearSeriesCheckboxes();

			std::vector<ScatterPoint> data;
			double x, y;
			for (int32 i = 0; message->FindDouble("x", i, &x) == B_OK
					&& message->FindDouble("y", i, &y) == B_OK; i++)
			{
				ScatterPoint p;
				p.x = x;
				p.y = y;
				data.push_back(p);
			}
			fChartView->SetScatterData(data);
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
			// Ricostruisce le checkbox PRIMA di passare i dati a
			// ChartView: popola anche data.showValues (preservando lo
			// stato di prima per nome di serie), cosi' il grafico si
			// disegna gia' con le visibilita' corrette al primo giro.
			RebuildSeriesCheckboxes(&data);
			fChartView->SetMultiData(data);
			return;
		}

		case kMsgSeriesToggleLocal:
		{
			int32 index;
			int32 value = 0;
			if (message->FindInt32("index", &index) == B_OK)
			{
				// "be:value" e' aggiunto automaticamente da
				// BControl::Invoke() col nuovo stato della checkbox
				// che ha generato il messaggio.
				message->FindInt32("be:value", &value);
				fChartView->SetSeriesShowValues(index, value != 0);
			}
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
