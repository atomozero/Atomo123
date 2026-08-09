/*
	PreferencesWindow.cpp

	Vedi PreferencesWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PreferencesWindow.h"

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PreferencesWindow"

static const uint32 kMsgApplyLocal = 'aplp';

// Scelte proposte per "numero di file recenti": stessi valori
// dell'equivalente in altre app Haiku (Tracker, WebPositive), piu' un
// valore alto per chi tiene molti documenti aperti in rotazione.
// MainWindow::kMaxRecentFilesLimit (15) e' il tetto degli slot
// riservati in gPrefs -- deve restare >= all'ultimo valore qui.
static const int kRecentChoices[] = { 3, 5, 10, 15 };

PreferencesWindow::PreferencesWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 460, 360), B_TRANSLATE("Preferenze"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fShowGridBox = new BCheckBox("showGrid", B_TRANSLATE("Mostra griglia"), NULL);

	BPopUpMenu* decimalMenu = new BPopUpMenu("decimal");
	decimalMenu->AddItem(new BMenuItem(B_TRANSLATE("Punto (.)"), NULL));
	decimalMenu->AddItem(new BMenuItem(B_TRANSLATE("Virgola (,)"), NULL));
	decimalMenu->ItemAt(0)->SetMarked(true);
	fDecimalField = new BMenuField("decimalField", B_TRANSLATE("Separatore decimale:"), decimalMenu);

	BPopUpMenu* listMenu = new BPopUpMenu("list");
	listMenu->AddItem(new BMenuItem(B_TRANSLATE("Punto e virgola (;)"), NULL));
	listMenu->AddItem(new BMenuItem(B_TRANSLATE("Virgola (,)"), NULL));
	listMenu->ItemAt(0)->SetMarked(true);
	fListField = new BMenuField("listField", B_TRANSLATE("Separatore di elenco:"), listMenu);

	// Separatore delle migliaia/simbolo di valuta: usati da
	// CContainer::GetCellResult (export CSV, elenco valori univoci del
	// filtro automatico -- vedi il commento in App::App()), non dalla
	// griglia a schermo (quella segue sempre il Locale Kit di Haiku,
	// indipendente da questa preferenza).
	BPopUpMenu* thousandMenu = new BPopUpMenu("thousand");
	thousandMenu->AddItem(new BMenuItem(B_TRANSLATE("Virgola (,)"), NULL));
	thousandMenu->AddItem(new BMenuItem(B_TRANSLATE("Punto (.)"), NULL));
	thousandMenu->AddItem(new BMenuItem(B_TRANSLATE("Spazio ( )"), NULL));
	thousandMenu->ItemAt(0)->SetMarked(true);
	fThousandField = new BMenuField("thousandField", B_TRANSLATE("Separatore delle migliaia:"),
		thousandMenu);

	fCurrencyField = new BTextControl("currencyField", B_TRANSLATE("Simbolo valuta:"), "$", NULL);

	// Generale: vista del foglio e interpretazione dei numeri digitati
	// nelle formule -- le tre preferenze originali di Fase 7, piu' le
	// due di esportazione/valuta aggiunte dopo.
	BBox* generalBox = new BBox("generalBox");
	generalBox->SetLabel(B_TRANSLATE("Generale"));
	BLayoutBuilder::Group<>(generalBox, B_VERTICAL, 6)
		.SetInsets(8, generalBox->TopBorderOffset() + 8, 8, 8)
		.Add(fShowGridBox)
		.Add(fDecimalField)
		.Add(fListField)
		.Add(fThousandField)
		.Add(fCurrencyField);

	BPopUpMenu* recentMenu = new BPopUpMenu("recent");
	for (size_t i = 0; i < sizeof(kRecentChoices) / sizeof(kRecentChoices[0]); i++)
	{
		BString label;
		label << kRecentChoices[i];
		recentMenu->AddItem(new BMenuItem(label.String(), NULL));
	}
	recentMenu->ItemAt(1)->SetMarked(true); // 5, il valore predefinito
	fRecentField = new BMenuField("recentField", B_TRANSLATE("File recenti da ricordare:"), recentMenu);

	// File: comportamento del menu File > "Apri recenti" -- prima un
	// numero fisso nel codice (MainWindow::kMaxRecentFiles = 5), ora
	// scelto qui (vedi MainWindow::fMaxRecentFiles).
	BBox* fileBox = new BBox("fileBox");
	fileBox->SetLabel(B_TRANSLATE("File"));
	BLayoutBuilder::Group<>(fileBox, B_VERTICAL, 6)
		.SetInsets(8, fileBox->TopBorderOffset() + 8, 8, 8)
		.Add(fRecentField);

	// Avvio: solo lo splash screen per ora -- letta direttamente da
	// App::ReadyToRun (non riguarda un documento gia' aperto, a
	// differenza delle altre preferenze qui sopra, quindi non serve
	// nessun MainWindow::fXxx a specchio, solo gPrefs).
	fShowSplashBox = new BCheckBox("showSplash", B_TRANSLATE("Mostra lo splash screen all'avvio"), NULL);
	BBox* startupBox = new BBox("startupBox");
	startupBox->SetLabel(B_TRANSLATE("Avvio"));
	BLayoutBuilder::Group<>(startupBox, B_VERTICAL, 6)
		.SetInsets(8, startupBox->TopBorderOffset() + 8, 8, 8)
		.Add(fShowSplashBox);

	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica"), new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(generalBox)
		.Add(fileBox)
		.Add(startupBox)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(applyButton)
		.End();
}

void PreferencesWindow::SetValues(bool showGrid, char decimalSep, char listSep,
	int maxRecentFiles, bool showSplash, char thousandSep, const char* currencySymbol)
{
	fShowGridBox->SetValue(showGrid ? B_CONTROL_ON : B_CONTROL_OFF);
	fShowSplashBox->SetValue(showSplash ? B_CONTROL_ON : B_CONTROL_OFF);
	fDecimalField->Menu()->ItemAt(decimalSep == ',' ? 1 : 0)->SetMarked(true);
	fListField->Menu()->ItemAt(listSep == ',' ? 1 : 0)->SetMarked(true);
	int thousandIndex = thousandSep == '.' ? 1 : (thousandSep == ' ' ? 2 : 0);
	fThousandField->Menu()->ItemAt(thousandIndex)->SetMarked(true);
	fCurrencyField->SetText(currencySymbol);

	// Segna la scelta piu' vicina fra quelle proposte: se gPrefs
	// contiene un valore non elencato (es. modificato a mano nel file
	// di preferenze), non deve lasciare il menu senza nessuna voce
	// marcata.
	size_t count = sizeof(kRecentChoices) / sizeof(kRecentChoices[0]);
	size_t closest = 0;
	int bestDiff = -1;
	for (size_t i = 0; i < count; i++)
	{
		int diff = maxRecentFiles - kRecentChoices[i];
		if (diff < 0)
			diff = -diff;
		if (bestDiff < 0 || diff < bestDiff)
		{
			bestDiff = diff;
			closest = i;
		}
	}
	fRecentField->Menu()->ItemAt(closest)->SetMarked(true);
}

void PreferencesWindow::MessageReceived(BMessage* message)
{
	if (message->what == kMsgApplyLocal)
	{
		BMenuItem* markedDecimal = fDecimalField->Menu()->FindMarked();
		char decimalSep = (markedDecimal
			&& fDecimalField->Menu()->IndexOf(markedDecimal) == 1) ? ',' : '.';

		BMenuItem* markedList = fListField->Menu()->FindMarked();
		char listSep = (markedList
			&& fListField->Menu()->IndexOf(markedList) == 1) ? ',' : ';';

		BMenuItem* markedRecent = fRecentField->Menu()->FindMarked();
		int32 recentIndex = markedRecent
			? fRecentField->Menu()->IndexOf(markedRecent) : 1;
		int maxRecentFiles = kRecentChoices[1];
		if (recentIndex >= 0
			&& (size_t)recentIndex < sizeof(kRecentChoices) / sizeof(kRecentChoices[0]))
			maxRecentFiles = kRecentChoices[recentIndex];

		BMenuItem* markedThousand = fThousandField->Menu()->FindMarked();
		int32 thousandIndex = markedThousand
			? fThousandField->Menu()->IndexOf(markedThousand) : 0;
		char thousandSep = thousandIndex == 1 ? '.' : (thousandIndex == 2 ? ' ' : ',');

		BMessage request(kMsgPreferencesRequest);
		request.AddBool("showGrid", fShowGridBox->Value() == B_CONTROL_ON);
		request.AddInt8("decimalSeparator", decimalSep);
		request.AddInt8("listSeparator", listSep);
		request.AddInt32("maxRecentFiles", maxRecentFiles);
		request.AddBool("showSplash", fShowSplashBox->Value() == B_CONTROL_ON);
		request.AddInt8("thousandSeparator", thousandSep);
		request.AddString("currencySymbol", fCurrencyField->Text());
		fTarget.SendMessage(&request);
		return;
	}

	BWindow::MessageReceived(message);
}

bool PreferencesWindow::QuitRequested()
{
	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
