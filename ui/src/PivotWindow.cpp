/*
	PivotWindow.cpp

	Vedi PivotWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PivotWindow.h"
#include "Pivot.h"

#include <map>

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PivotWindow"

static const uint32 kMsgCreateLocal = 'pvlc';
static const uint32 kMsgDetectLocal = 'pvdl';

// Stessi 5 elementi/ordine di fAggField, riusato per il menu di
// aggregazione di ogni riga di misura -- l'ordine DEVE combaciare con la
// mappatura indice->PivotAggFunc in AggFuncFromMenuField sotto.
static BPopUpMenu* NewAggMenu(const char* name)
{
	BPopUpMenu* menu = new BPopUpMenu(name);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Somma"), NULL));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Conteggio"), NULL));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Media"), NULL));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Minimo"), NULL));
	menu->AddItem(new BMenuItem(B_TRANSLATE("Massimo"), NULL));
	menu->ItemAt(0)->SetMarked(true);
	return menu;
}

static PivotAggFunc AggFuncFromMenuField(BMenuField* field)
{
	BMenuItem* marked = field->Menu()->FindMarked();
	if (!marked)
		return ePivotSum;
	switch (field->Menu()->IndexOf(marked))
	{
		case 1: return ePivotCount;
		case 2: return ePivotAverage;
		case 3: return ePivotMin;
		case 4: return ePivotMax;
		default: return ePivotSum;
	}
}

PivotWindow::PivotWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 560, 420), B_TRANSLATE("Tabella pivot"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	// Stesso messaggio su Invio + pulsante dedicato di fRangeField in
	// ChartWindow: digitare l'intervallo e premere Invio (o "Rileva
	// colonne") rileva subito le colonne disponibili per il campo
	// Colonne/le misure sotto.
	fSourceField = new BTextControl("source",
		B_TRANSLATE("Intervallo dati (una o piu' colonne di categoria, poi il valore):"),
		"A1:B10", new BMessage(kMsgDetectLocal));
	fSourceField->SetTarget(this);

	fDestField = new BTextControl("dest", B_TRANSLATE("Cella di destinazione:"), "D1", NULL);

	fAggField = new BMenuField("aggField", B_TRANSLATE("Aggregazione:"), NewAggMenu("agg"));

	fDetectButton = new BButton("detect", B_TRANSLATE("Rileva colonne"),
		new BMessage(kMsgDetectLocal));
	fDetectButton->SetTarget(this);

	// Riquadro "Campo Colonne" (Fase 2D): scelta ESCLUSIVA fra "(nessuna)"
	// (percorso 1D di sempre) e una colonna rilevata -- popolato solo
	// dopo il primo "Rileva colonne"/Invio, vedi RebuildColumnPickers.
	BPopUpMenu* columnFieldMenu = new BPopUpMenu("columnField");
	columnFieldMenu->AddItem(new BMenuItem(B_TRANSLATE("(nessuna)"), NULL));
	columnFieldMenu->ItemAt(0)->SetMarked(true);
	fColumnFieldMenu = new BMenuField("columnFieldField", B_TRANSLATE("Campo Colonne:"),
		columnFieldMenu);

	// Riquadro "Misure" (Fase 2D): una riga checkbox+aggregazione per
	// ogni colonna rilevata -- stesso pattern di fSeriesCheckboxBox/
	// fSeriesCheckboxRow in ChartWindow.cpp, ma ogni "riga" qui e' una
	// coppia di controlli invece di una sola checkbox.
	fMeasureBox = new BBox("measureBox");
	fMeasureBox->SetLabel(B_TRANSLATE("Misure"));

	fMeasureRows = new BView("measureRows", 0);
	fMeasureRows->SetLayout(new BGroupLayout(B_VERTICAL, 4));

	BLayoutBuilder::Group<>(fMeasureBox, B_VERTICAL, 4)
		.SetInsets(8, fMeasureBox->TopBorderOffset() + 6, 8, 6)
		.Add(fMeasureRows);
	// Nascosto finche' "Rileva colonne" non trova almeno una colonna,
	// stesso principio di fSeriesCheckboxBox in ChartWindow.
	fMeasureBox->Hide();

	BButton* createButton = new BButton("create", B_TRANSLATE("Crea"), new BMessage(kMsgCreateLocal));
	createButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fSourceField)
		.AddGroup(B_HORIZONTAL)
			.Add(fDetectButton)
			.AddGlue()
		.End()
		.Add(fDestField)
		.Add(fAggField)
		.Add(fColumnFieldMenu)
		.Add(fMeasureBox)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(createButton)
		.End();
}

void PivotWindow::RequestDetectColumns()
{
	BMessage request(kMsgPivotDetectColumns);
	request.AddString("source", fSourceField->Text());
	fTarget.SendMessage(&request);
}

void PivotWindow::ClearColumnPickers()
{
	while (fMeasureRows->CountChildren() > 0)
	{
		BView* child = fMeasureRows->ChildAt(0);
		fMeasureRows->RemoveChild(child);
		delete child;
	}
	fMeasureCheckboxes.clear();
	fMeasureAggFields.clear();
	fMeasureCols.clear();
	if (!fMeasureBox->IsHidden())
		fMeasureBox->Hide();

	BMenu* menu = fColumnFieldMenu->Menu();
	for (int32 i = menu->CountItems() - 1; i >= 1; i--)
		delete menu->RemoveItem(i);
}

void PivotWindow::RebuildColumnPickers(BMessage* colInfo)
{
	// Preserva lo stato scelto in precedenza per colonna assoluta (non
	// per posizione: un nuovo "Rileva colonne" dopo aver modificato
	// l'intervallo puo' cambiare quali colonne esistono) -- stesso
	// principio di preservazione per nome di RebuildSeriesCheckboxes in
	// ChartWindow.cpp.
	std::map<int32, bool> previousChecked;
	std::map<int32, int32> previousAggIndex;
	for (size_t i = 0; i < fMeasureCheckboxes.size(); i++)
	{
		previousChecked[fMeasureCols[i]] = fMeasureCheckboxes[i]->Value() != 0;
		BMenuItem* marked = fMeasureAggFields[i]->Menu()->FindMarked();
		if (marked)
			previousAggIndex[fMeasureCols[i]] = fMeasureAggFields[i]->Menu()->IndexOf(marked);
	}
	int32 previousColumnField = -1;
	BMenuItem* markedColField = fColumnFieldMenu->Menu()->FindMarked();
	if (markedColField && markedColField->Message())
		markedColField->Message()->FindInt32("col", &previousColumnField);

	ClearColumnPickers();

	int32 col;
	BString label;
	std::vector<int32> cols;
	std::vector<BString> labels;
	for (int32 i = 0; colInfo->FindInt32("col", i, &col) == B_OK
			&& colInfo->FindString("label", i, &label) == B_OK; i++)
	{
		cols.push_back(col);
		labels.push_back(label);

		BMessage* itemMsg = new BMessage();
		itemMsg->AddInt32("col", col);
		BMenuItem* item = new BMenuItem(label.String(), itemMsg);
		fColumnFieldMenu->Menu()->AddItem(item);
		if (col == previousColumnField)
			item->SetMarked(true);
	}
	if (previousColumnField == -1 || cols.empty())
		fColumnFieldMenu->Menu()->ItemAt(0)->SetMarked(true);

	for (size_t i = 0; i < cols.size(); i++)
	{
		// Default quando non c'e' stato precedente: solo l'ULTIMA
		// colonna rilevata spuntata come misura -- riproduce esattamente
		// il vecchio comportamento implicito a una sola misura (ultima
		// colonna dell'intervallo) quando l'utente non tocca altro.
		bool checked = (i + 1 == cols.size());
		// Aggregazione di default = quella gia' scelta in fAggField
		// (l'aggregazione della misura implicita nel vecchio percorso
		// 1D), non sempre "Somma": un utente che ha gia' cambiato
		// fAggField prima di rilevare le colonne non deve vedersi
		// tornare silenziosamente a Somma per l'unica misura di default.
		int32 aggIndex = 0;
		BMenuItem* markedAgg = fAggField->Menu()->FindMarked();
		if (markedAgg)
			aggIndex = fAggField->Menu()->IndexOf(markedAgg);
		std::map<int32, bool>::iterator cit = previousChecked.find(cols[i]);
		if (cit != previousChecked.end())
			checked = cit->second;
		std::map<int32, int32>::iterator ait = previousAggIndex.find(cols[i]);
		if (ait != previousAggIndex.end())
			aggIndex = ait->second;

		BView* row = new BView("measureRow", 0);
		row->SetLayout(new BGroupLayout(B_HORIZONTAL, 8));

		BCheckBox* cb = new BCheckBox("measureCheck", labels[i].String(), NULL);
		cb->SetValue(checked ? B_CONTROL_ON : B_CONTROL_OFF);

		BMenuField* aggField = new BMenuField("measureAgg", NULL, NewAggMenu("measureAgg"));
		if (aggIndex >= 0 && aggIndex < aggField->Menu()->CountItems())
			aggField->Menu()->ItemAt(aggIndex)->SetMarked(true);

		BLayoutBuilder::Group<>(row, B_HORIZONTAL, 8)
			.Add(cb)
			.Add(aggField);
		fMeasureRows->AddChild(row);

		fMeasureCheckboxes.push_back(cb);
		fMeasureAggFields.push_back(aggField);
		fMeasureCols.push_back(cols[i]);
	}

	if (!cols.empty() && fMeasureBox->IsHidden())
		fMeasureBox->Show();
}

void PivotWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgDetectLocal)
	{
		RequestDetectColumns();
		return;
	}

	if (message->what == kMsgPivotColumnsInfo)
	{
		RebuildColumnPickers(message);
		return;
	}

	if (message->what == kMsgCreateLocal)
	{
		BMessage request(kMsgPivotRequest);
		request.AddString("source", fSourceField->Text());
		request.AddString("dest", fDestField->Text());
		request.AddInt32("agg", (int32)AggFuncFromMenuField(fAggField));

		int32 columnFieldCol = -1;
		BMenuItem* markedColField = fColumnFieldMenu->Menu()->FindMarked();
		if (markedColField && markedColField->Message())
			markedColField->Message()->FindInt32("col", &columnFieldCol);

		std::vector<int32> measureCols;
		std::vector<int32> measureAggs;
		std::vector<BString> measureLabels;
		for (size_t i = 0; i < fMeasureCheckboxes.size(); i++)
		{
			if (fMeasureCheckboxes[i]->Value() == 0)
				continue;
			measureCols.push_back(fMeasureCols[i]);
			measureAggs.push_back((int32)AggFuncFromMenuField(fMeasureAggFields[i]));
			measureLabels.push_back(fMeasureCheckboxes[i]->Label());
		}

		// Nulla di nuovo toccato (nessun campo Colonne, nessuna misura
		// esplicita spuntata): manda esattamente il vecchio messaggio 1D
		// -- MainWindow::HandlePivotRequest tratta l'assenza di questi
		// campi come il percorso di sempre, ma evitare del tutto di
		// aggiungerli qui riproduce byte per byte il comportamento
		// precedente a questa estensione per un utente che ignora la UI
		// nuova.
		if (columnFieldCol != -1 || !measureCols.empty())
		{
			request.AddInt32("columnField", columnFieldCol);
			for (size_t i = 0; i < measureCols.size(); i++)
			{
				request.AddInt32("measureCol", measureCols[i]);
				request.AddInt32("measureAgg", measureAggs[i]);
				request.AddString("measureLabel", measureLabels[i]);
			}
		}

		fTarget.SendMessage(&request);
		return;
	}

	BWindow::MessageReceived(message);
}

bool PivotWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
