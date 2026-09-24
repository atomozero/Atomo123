/*
	ConditionalFormatWindow.cpp

	Vedi ConditionalFormatWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ConditionalFormatWindow.h"

#include <cstdlib>

#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <ColorControl.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ConditionalFormatWindow"

static const uint32 kMsgApplyLocal = 'cfal';
static const uint32 kMsgRemoveAllLocal = 'cfrl';
static const uint32 kMsgTypeChangedLocal = 'cftc';
static const uint32 kMsgOperatorChangedLocal = 'cfoc';

// Corrispondenza posizionale del menu "Tipo" (NON lo stesso ordine di
// CondFormatRuleType in Container.h -- MainWindow::MessageReceived
// smista questi indici sul vero enum, vedi il commento li'):
// 0=uguale a/diverso da/maggiore/minore/tra (cellIs, l'operatore vero
// e' nel menu "Operatore" sotto), 1=valori duplicati, 2=scala di
// colori, 3=barra dei dati, 4=icon set, 5=contiene testo,
// 6=non contiene testo, 7=inizia con, 8=finisce con, 9=celle vuote,
// 10=celle non vuote, 11=errori, 12=non errori, 13=primi/ultimi N
// valori, 14=sopra/sotto la media. Stesso principio gia' usato per
// ChartType/ChartWindow e ValidationType/ValidationWindow.
enum {
	kTypeCellIs = 0, kTypeDuplicate, kTypeColorScale, kTypeDataBar, kTypeIconSet,
	kTypeContainsText, kTypeNotContainsText, kTypeBeginsWith, kTypeEndsWith,
	kTypeContainsBlanks, kTypeNotContainsBlanks, kTypeContainsErrors, kTypeNotContainsErrors,
	kTypeTop10, kTypeAboveAverage
};

// Corrispondenza posizionale del menu "Operatore" (solo per
// kTypeCellIs) con ConditionalFormatRule::ruleOperator in Container.h:
// 0=equal (il solo comportamento che esisteva prima di questa
// versione) .. 7=notBetween.
enum {
	kOpEqual = 0, kOpNotEqual, kOpGreaterThan, kOpLessThan,
	kOpGreaterOrEqual, kOpLessOrEqual, kOpBetween, kOpNotBetween
};

// La scala di colori qui e' sempre a due punti (min->max): copre il
// caso Excel piu' comune senza dover costruire un editor per un numero
// arbitrario di soglie. La barra dei dati riusa fColorControl (un solo
// colore, come il vero Excel) invece di aggiungere un terzo
// BColorControl. L'icon set non ha NESSUN colore scelto dall'utente
// (il colore di ogni icona e' fisso per livello, vedi
// SheetView::IconColorForTier) e crea sempre un set a 3 livelli (il
// caso Excel piu' comune, "3 semafori") -- nessun editor per il numero
// di icone o lo stile, a differenza dell'importazione XLSX che li
// accetta entrambi cosi' come il file li descrive. Le famiglie
// contiene-testo/celle-vuote/errori/top10/sopra-la-media (Path to full
// Excel parity, Tier 3) sono tutte "per cella" o "per soglia" come
// uguale-a/duplicati, stesso colore di sfondo unico. equalAverage non
// e' esposto qui (solo above/below la media, vedi UpdateFieldsForType)
// -- stesso principio gia' scelto per iconSet: la UI nativa espone un
// sottoinsieme piu' stretto di quanto l'importazione XLSX accetta.
ConditionalFormatWindow::ConditionalFormatWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 480, 460), B_TRANSLATE("Formattazione condizionale"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	BPopUpMenu* typeMenu = new BPopUpMenu("typeMenu");
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Confronto numerico (vedi Operatore)"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Valori duplicati nella selezione"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Scala di colori (minimo -> massimo)"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Barra dei dati (minimo -> massimo)"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Icon set (3 livelli)"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Contiene testo"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Non contiene testo"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Inizia con"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Finisce con"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Contiene celle vuote"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Non contiene celle vuote"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Contiene errori"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Non contiene errori"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Primi/ultimi N valori"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->AddItem(new BMenuItem(B_TRANSLATE("Sopra/sotto la media"),
		new BMessage(kMsgTypeChangedLocal)));
	typeMenu->ItemAt(0)->SetMarked(true);
	typeMenu->SetTargetForItems(this);
	fTypeField = new BMenuField("type", B_TRANSLATE("Tipo:"), typeMenu);

	BPopUpMenu* opMenu = new BPopUpMenu("opMenu");
	opMenu->AddItem(new BMenuItem(B_TRANSLATE("Uguale a"), new BMessage(kMsgOperatorChangedLocal)));
	opMenu->AddItem(new BMenuItem(B_TRANSLATE("Diverso da"), new BMessage(kMsgOperatorChangedLocal)));
	opMenu->AddItem(new BMenuItem(B_TRANSLATE("Maggiore di"), new BMessage(kMsgOperatorChangedLocal)));
	opMenu->AddItem(new BMenuItem(B_TRANSLATE("Minore di"), new BMessage(kMsgOperatorChangedLocal)));
	opMenu->AddItem(new BMenuItem(B_TRANSLATE("Maggiore o uguale a"), new BMessage(kMsgOperatorChangedLocal)));
	opMenu->AddItem(new BMenuItem(B_TRANSLATE("Minore o uguale a"), new BMessage(kMsgOperatorChangedLocal)));
	opMenu->AddItem(new BMenuItem(B_TRANSLATE("Tra"), new BMessage(kMsgOperatorChangedLocal)));
	opMenu->AddItem(new BMenuItem(B_TRANSLATE("Non tra"), new BMessage(kMsgOperatorChangedLocal)));
	opMenu->ItemAt(0)->SetMarked(true);
	opMenu->SetTargetForItems(this);
	fOperatorField = new BMenuField("operator", B_TRANSLATE("Operatore:"), opMenu);

	fValueField = new BTextControl("value", B_TRANSLATE("Valore:"), "", NULL);
	fValueField2 = new BTextControl("value2", B_TRANSLATE("E:"), "", NULL);
	fValueField2->Hide();

	fColorControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 8, "colorControl");
	rgb_color initial = { 255, 199, 206, 255 }; // FFC7CE, lo stesso "rosso Excel" predefinito
	fColorControl->SetValue(initial);

	fMaxColorControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 8, "maxColorControl");
	rgb_color maxInitial = { 99, 190, 123, 255 }; // 63BE7B, il verde predefinito di Excel per il massimo
	fMaxColorControl->SetValue(maxInitial);
	fMaxColorControl->Hide();

	fRankField = new BTextControl("rank", B_TRANSLATE("Quanti valori:"), "10", NULL);
	fRankField->Hide();
	fPercentCheckBox = new BCheckBox("percent", B_TRANSLATE("Percentuale"), NULL);
	fPercentCheckBox->Hide();
	fBottomCheckBox = new BCheckBox("bottom", B_TRANSLATE("Ultimi (invece di primi)"), NULL);
	fBottomCheckBox->Hide();

	fBelowAverageCheckBox = new BCheckBox("belowAverage", B_TRANSLATE("Sotto la media (invece di sopra)"), NULL);
	fBelowAverageCheckBox->Hide();

	BButton* removeButton = new BButton("removeAll", B_TRANSLATE("Rimuovi tutte le regole"),
		new BMessage(kMsgRemoveAllLocal));
	removeButton->SetTarget(this);

	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica alla selezione"),
		new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fTypeField)
		.Add(fOperatorField)
		.Add(fValueField)
		.Add(fValueField2)
		.Add(fColorControl)
		.Add(fMaxColorControl)
		.Add(fRankField)
		.Add(fPercentCheckBox)
		.Add(fBottomCheckBox)
		.Add(fBelowAverageCheckBox)
		.AddGroup(B_HORIZONTAL)
			.Add(removeButton)
			.AddGlue()
			.Add(applyButton)
		.End();

	UpdateFieldsForType();
	fValueField->MakeFocus(true);
}

int ConditionalFormatWindow::SelectedType() const
{
	return fTypeField->Menu()->IndexOf(fTypeField->Menu()->FindMarked());
}

int ConditionalFormatWindow::SelectedOperator() const
{
	return fOperatorField->Menu()->IndexOf(fOperatorField->Menu()->FindMarked());
}

void ConditionalFormatWindow::UpdateFieldsForType()
{
	int type = SelectedType();
	bool isCellIs = (type == kTypeCellIs);
	bool isColorScale = (type == kTypeColorScale);
	bool isIconSet = (type == kTypeIconSet);
	bool isTextFamily = (type >= kTypeContainsText && type <= kTypeEndsWith);
	bool isBlankErrorFamily = (type >= kTypeContainsBlanks && type <= kTypeNotContainsErrors);
	bool isTop10 = (type == kTypeTop10);
	bool isAboveAverage = (type == kTypeAboveAverage);
	int op = isCellIs ? SelectedOperator() : -1;
	bool isBetween = isCellIs && (op == kOpBetween || op == kOpNotBetween);

	if (isCellIs && fOperatorField->IsHidden())
		fOperatorField->Show();
	else if (!isCellIs && !fOperatorField->IsHidden())
		fOperatorField->Hide();

	// Il valore di confronto ha senso per cellIs (letterale o
	// riferimento) e per la famiglia contiene-testo (il testo
	// cercato) -- niente per gli altri tipi, che derivano tutto
	// dall'intervallo o non hanno bisogno di nessun valore.
	bool needsValue = isCellIs || isTextFamily;
	fValueField->SetEnabled(needsValue);
	if (needsValue && fValueField->IsHidden())
		fValueField->Show();
	else if (!needsValue && !fValueField->IsHidden())
		fValueField->Hide();

	if (isBetween && fValueField2->IsHidden())
		fValueField2->Show();
	else if (!isBetween && !fValueField2->IsHidden())
		fValueField2->Hide();

	if (isColorScale)
	{
		if (fMaxColorControl->IsHidden())
			fMaxColorControl->Show();
	}
	else if (!fMaxColorControl->IsHidden())
		fMaxColorControl->Hide();

	// L'icon set non ha nessun colore scelto dall'utente (vedi il
	// commento sul costruttore sopra): nasconde anche fColorControl,
	// non solo fMaxColorControl.
	if (isIconSet && !fColorControl->IsHidden())
		fColorControl->Hide();
	else if (!isIconSet && fColorControl->IsHidden())
		fColorControl->Show();

	if (isTop10)
	{
		if (fRankField->IsHidden()) fRankField->Show();
		if (fPercentCheckBox->IsHidden()) fPercentCheckBox->Show();
		if (fBottomCheckBox->IsHidden()) fBottomCheckBox->Show();
	}
	else
	{
		if (!fRankField->IsHidden()) fRankField->Hide();
		if (!fPercentCheckBox->IsHidden()) fPercentCheckBox->Hide();
		if (!fBottomCheckBox->IsHidden()) fBottomCheckBox->Hide();
	}

	if (isAboveAverage && fBelowAverageCheckBox->IsHidden())
		fBelowAverageCheckBox->Show();
	else if (!isAboveAverage && !fBelowAverageCheckBox->IsHidden())
		fBelowAverageCheckBox->Hide();

	(void)isBlankErrorFamily; // niente controlli dedicati: nessun valore/opzione da mostrare.
}

void ConditionalFormatWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgApplyLocal:
		{
			int type = SelectedType();
			BMessage request(kMsgCondFormatCommit);
			request.AddInt32("type", type);
			request.AddString("value", fValueField->Text());
			request.AddString("value2", fValueField2->Text());
			request.AddInt32("operator", SelectedOperator());
			int32 rank = atoi(fRankField->Text());
			if (rank < 1)
				rank = 10;
			request.AddInt32("rank", rank);
			request.AddBool("percent", fPercentCheckBox->Value() == B_CONTROL_ON);
			request.AddBool("bottom", fBottomCheckBox->Value() == B_CONTROL_ON);
			request.AddBool("belowAverage", fBelowAverageCheckBox->Value() == B_CONTROL_ON);
			// L'icon set non porta nessun colore (vedi il commento sul
			// costruttore sopra): "color" resta assente dal messaggio,
			// MainWindow lo smista PRIMA di cercare quel campo.
			if (type != kTypeIconSet)
			{
				rgb_color color = fColorControl->ValueAsColor();
				request.AddData("color", B_RGB_COLOR_TYPE, &color, sizeof(rgb_color));
				if (type == kTypeColorScale)
				{
					rgb_color maxColor = fMaxColorControl->ValueAsColor();
					request.AddData("maxColor", B_RGB_COLOR_TYPE, &maxColor, sizeof(rgb_color));
				}
			}
			fTarget.SendMessage(&request);
			Hide();
			return;
		}

		case kMsgRemoveAllLocal:
		{
			BMessage request(kMsgCondFormatRemoveAll);
			fTarget.SendMessage(&request);
			Hide();
			return;
		}

		case kMsgTypeChangedLocal:
		case kMsgOperatorChangedLocal:
			UpdateFieldsForType();
			return;
	}

	BWindow::MessageReceived(message);
}

bool ConditionalFormatWindow::QuitRequested()
{
	// Stessa regola di GoToWindow/ValidationWindow: resta nascosta e
	// riusabile.
	Hide();
	return false;
}
