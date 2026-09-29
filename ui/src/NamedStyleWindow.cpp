/*
	NamedStyleWindow.cpp

	Vedi NamedStyleWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "NamedStyleWindow.h"
#include "CellStyle.h"
#include "ColorWindow.h"

#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "NamedStyleWindow"

static const uint32 kMsgSelectStyleLocal = 'slns';
static const uint32 kMsgApplyLocal = 'apnl';
static const uint32 kMsgNewFromSelectionLocal = 'crnl';
static const uint32 kMsgRedefineLocal = 'rdnl';
static const uint32 kMsgDeleteLocal = 'dlnl';
static const uint32 kMsgRoleChangedLocal = 'rlcl';
// Un riquadretto letterale (sfondo/testo) o uno degli 8 del tema, tutti
// mandano questo con "kind" (vedi ShowColorPicker in NamedStyleWindow.h
// per il significato dei valori).
static const uint32 kMsgSwatchClickedLocal = 'swcl';

// -1 = "Colore personalizzato" (letterale), altrimenti un ThemeColorRole
// (vedi NamedStyle.h) -- stessa convenzione di fColorPickKind ma senza
// l'offset di 2 usato li' per distinguere sfondo/testo letterali dai
// ruoli tema.
static const int32 kRoleLiteral = -1;

// Piccolo riquadro colorato cliccabile (sfondo/testo letterale, o un
// ruolo della tavolozza tema) -- stessa identica forma di
// ChartWindow.cpp::ChartColorSwatch, duplicata qui apposta invece di
// condivisa: entrambe restano piccole classi locali al proprio file,
// stesso principio gia' seguito la prima volta che questo pattern e'
// servito altrove in questo progetto.
class SwatchView : public BView {
public:
	SwatchView(int kind, BMessenger target, uint32 what)
		:
		BView(BRect(0, 0, 17, 17), "colorSwatch", B_FOLLOW_NONE, B_WILL_DRAW),
		fKind(kind),
		fTarget(target),
		fWhat(what),
		fColor(ui_color(B_PANEL_BACKGROUND_COLOR))
	{
		SetExplicitMinSize(BSize(17, 17));
		SetExplicitMaxSize(BSize(17, 17));
	}

	void SetColor(rgb_color color)
	{
		fColor = color;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		SetHighColor(fColor);
		FillRect(Bounds());
		SetHighColor(120, 120, 120);
		StrokeRect(Bounds());
	}

	virtual void MouseDown(BPoint where)
	{
		BMessage msg(fWhat);
		msg.AddInt32("kind", fKind);
		fTarget.SendMessage(&msg);
	}

private:
	int fKind;
	BMessenger fTarget;
	uint32 fWhat;
	rgb_color fColor;
};

// Un solo menu popolato allo stesso modo per sfondo e testo: "Colore
// personalizzato" (kRoleLiteral) seguito dagli 8 ruoli tema in ordine.
static BMenuField* BuildRoleField(const char* label, uint32 what)
{
	BPopUpMenu* menu = new BPopUpMenu("roleMenu");

	BMessage* literalMsg = new BMessage(what);
	literalMsg->AddInt32("role", kRoleLiteral);
	menu->AddItem(new BMenuItem(B_TRANSLATE("Colore personalizzato"), literalMsg));
	menu->AddSeparatorItem();

	static const char* kRoleNames[kThemeColorRoleCount] = {
		"Testo tema", "Sfondo tema",
		"Accento 1", "Accento 2", "Accento 3", "Accento 4", "Accento 5", "Accento 6"
	};
	for (int i = 0; i < kThemeColorRoleCount; i++)
	{
		BMessage* roleMsg = new BMessage(what);
		roleMsg->AddInt32("role", i);
		menu->AddItem(new BMenuItem(B_TRANSLATE(kRoleNames[i]), roleMsg));
	}

	menu->ItemAt(0)->SetMarked(true);
	BMenuField* field = new BMenuField("roleField", label, menu);
	return field;
}

static int32 CurrentRole(BMenuField* field)
{
	BMenuItem* marked = field->Menu()->FindMarked();
	int32 role = kRoleLiteral;
	if (marked)
		marked->Message()->FindInt32("role", &role);
	return role;
}

static void SetCurrentRole(BMenuField* field, int32 role)
{
	for (int32 i = 0; i < field->Menu()->CountItems(); i++)
	{
		BMenuItem* item = field->Menu()->ItemAt(i);
		int32 itemRole = kRoleLiteral;
		if (item->Message() && item->Message()->FindInt32("role", &itemRole) == B_OK
				&& itemRole == role)
		{
			item->SetMarked(true);
			return;
		}
	}
}

NamedStyleWindow::NamedStyleWindow(BMessenger target)
	:
	BWindow(BRect(160, 140, 560, 640), B_TRANSLATE("Gestione stili cella"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fBackgroundLiteral(rgb_color{ 255, 255, 255, 255 }),
	fTextLiteral(rgb_color{ 0, 0, 0, 255 }),
	fColorWindow(NULL),
	fColorPickKind(-1),
	fTarget(target)
{
	fStyleList = new BListView("styles");
	fStyleList->SetSelectionMessage(new BMessage(kMsgSelectStyleLocal));
	fStyleList->SetTarget(this);
	BScrollView* listScroll = new BScrollView("stylesScroll", fStyleList, 0, false, true);

	fNameField = new BTextControl("name",
		B_TRANSLATE("Nome (per \"Nuovo dalla selezione\"):"), "", NULL);

	fBackgroundRoleField = BuildRoleField(B_TRANSLATE("Sfondo:"), kMsgRoleChangedLocal);
	fBackgroundRoleField->Menu()->SetTargetForItems(this);
	fBackgroundSwatch = new SwatchView(0, BMessenger(this), kMsgSwatchClickedLocal);

	fTextRoleField = BuildRoleField(B_TRANSLATE("Testo:"), kMsgRoleChangedLocal);
	fTextRoleField->Menu()->SetTargetForItems(this);
	fTextSwatch = new SwatchView(1, BMessenger(this), kMsgSwatchClickedLocal);

	fBoldCheckbox = new BCheckBox("bold", B_TRANSLATE("Grassetto"), NULL);
	fItalicCheckbox = new BCheckBox("italic", B_TRANSLATE("Corsivo"), NULL);
	fUnderlineCheckbox = new BCheckBox("underline", B_TRANSLATE("Sottolineato"), NULL);

	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica"), new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);
	BButton* newButton = new BButton("new", B_TRANSLATE("Nuovo dalla selezione"),
		new BMessage(kMsgNewFromSelectionLocal));
	newButton->SetTarget(this);
	BButton* updateButton = new BButton("update", B_TRANSLATE("Aggiorna"),
		new BMessage(kMsgRedefineLocal));
	updateButton->SetTarget(this);
	BButton* deleteButton = new BButton("delete", B_TRANSLATE("Elimina"),
		new BMessage(kMsgDeleteLocal));
	deleteButton->SetTarget(this);

	// Striscia degli 8 colori tema: un riquadretto per ruolo, kind =
	// 2 + ruolo (vedi il commento su fColorPickKind in
	// NamedStyleWindow.h) cosi' ShowColorPicker distingue questi dai
	// due riquadretti letterali sopra (kind 0/1).
	BBox* themeBox = new BBox("themeBox");
	themeBox->SetLabel(B_TRANSLATE("Colori tema"));
	BView* themeRow = new BView("themeRow", 0);
	themeRow->SetLayout(new BGroupLayout(B_HORIZONTAL, 6));
	static const char* kRoleShortNames[kThemeColorRoleCount] = {
		"T", "S", "A1", "A2", "A3", "A4", "A5", "A6"
	};
	for (int i = 0; i < kThemeColorRoleCount; i++)
	{
		BView* cell = new BView("themeCell", 0);
		cell->SetLayout(new BGroupLayout(B_VERTICAL, 2));
		SwatchView* swatch = new SwatchView(2 + i, BMessenger(this), kMsgSwatchClickedLocal);
		fThemeSwatches.push_back(swatch);
		BStringView* label = new BStringView("themeLabel", kRoleShortNames[i]);
		BFont smallFont(be_plain_font);
		smallFont.SetSize(be_plain_font->Size() - 2);
		label->SetFont(&smallFont);
		label->SetExplicitAlignment(BAlignment(B_ALIGN_CENTER, B_ALIGN_MIDDLE));
		cell->AddChild(swatch);
		cell->AddChild(label);
		themeRow->AddChild(cell);
	}
	BLayoutBuilder::Group<>(themeBox, B_VERTICAL, 4)
		.SetInsets(8, themeBox->TopBorderOffset() + 6, 8, 6)
		.Add(themeRow);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(listScroll)
		.Add(fNameField)
		.AddGroup(B_HORIZONTAL)
			.Add(fBackgroundRoleField)
			.Add(fBackgroundSwatch)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fTextRoleField)
			.Add(fTextSwatch)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(fBoldCheckbox)
			.Add(fItalicCheckbox)
			.Add(fUnderlineCheckbox)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(applyButton)
			.Add(newButton)
			.Add(updateButton)
			.Add(deleteButton)
		.End()
		.Add(themeBox);

	RefreshPreview();
}

void NamedStyleWindow::RebuildStyleList()
{
	fStyleList->MakeEmpty();
	fListIDs.clear();
	for (int i = 0; i < fStyles.Count(); i++)
	{
		int styleID = fStyles.IDAtIndex(i);
		const NamedStyleDef* def = fStyles.Get(styleID);
		if (!def)
			continue;
		fListIDs.push_back(styleID);
		fStyleList->AddItem(new BStringItem(def->name.c_str()));
	}
}

void NamedStyleWindow::SetStyles(const NamedStyleTable& styles)
{
	fStyles = styles;
	RebuildStyleList();
}

void NamedStyleWindow::SetTheme(const ThemePalette& theme)
{
	fTheme = theme;
	for (int i = 0; i < kThemeColorRoleCount; i++)
		fThemeSwatches[i]->SetColor(fTheme.colors[i]);
	RefreshPreview();
}

void NamedStyleWindow::LoadStyleIntoControls(int styleID)
{
	const NamedStyleDef* def = fStyles.Get(styleID);
	if (!def)
		return;

	fNameField->SetText(def->name.c_str());
	SetCurrentRole(fBackgroundRoleField, def->useThemeBackground ? (int32)def->backgroundRole : kRoleLiteral);
	fBackgroundLiteral = def->backgroundColor;
	SetCurrentRole(fTextRoleField, def->useThemeText ? (int32)def->textRole : kRoleLiteral);
	fTextLiteral = def->textColor;
	fBoldCheckbox->SetValue(def->bold ? B_CONTROL_ON : B_CONTROL_OFF);
	fItalicCheckbox->SetValue(def->italic ? B_CONTROL_ON : B_CONTROL_OFF);
	fUnderlineCheckbox->SetValue(def->underline ? B_CONTROL_ON : B_CONTROL_OFF);

	RefreshPreview();
}

void NamedStyleWindow::RefreshPreview()
{
	NamedStyleDef def;
	int32 bgRole = CurrentRole(fBackgroundRoleField);
	def.useThemeBackground = bgRole != kRoleLiteral;
	def.backgroundRole = (ThemeColorRole)(bgRole != kRoleLiteral ? bgRole : eThemeBackground);
	def.backgroundColor = fBackgroundLiteral;

	int32 textRole = CurrentRole(fTextRoleField);
	def.useThemeText = textRole != kRoleLiteral;
	def.textRole = (ThemeColorRole)(textRole != kRoleLiteral ? textRole : eThemeText);
	def.textColor = fTextLiteral;

	CellStyle base;
	CellStyle resolved = def.Resolve(base, fTheme);
	fBackgroundSwatch->SetColor(resolved.fLowColor);
	fTextSwatch->SetColor(resolved.fHighColor);
}

void NamedStyleWindow::ShowColorPicker(int kind)
{
	if (!fColorWindow)
		fColorWindow = new ColorWindow(BMessenger(this));

	rgb_color initial;
	if (kind == 0)
		initial = fBackgroundLiteral;
	else if (kind == 1)
		initial = fTextLiteral;
	else
		initial = fTheme.colors[kind - 2];

	fColorPickKind = kind;
	if (fColorWindow->Lock())
	{
		fColorWindow->SetTarget(BMessenger(this));
		fColorWindow->SetSeriesIndex(-1);
		fColorWindow->SetThemeRole(kind >= 2 ? kind - 2 : -1);
		fColorWindow->SetMode(kind >= 2 ? eThemeColor : (kind == 0 ? eBackgroundColor : eTextColor), initial);
		fColorWindow->Unlock();
	}
	if (fColorWindow->IsHidden())
		fColorWindow->Show();
	fColorWindow->Activate();
}

void NamedStyleWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgSelectStyleLocal:
		{
			int32 index = fStyleList->CurrentSelection();
			if (index >= 0 && (size_t)index < fListIDs.size())
				LoadStyleIntoControls(fListIDs[index]);
			return;
		}

		case kMsgRoleChangedLocal:
			RefreshPreview();
			return;

		case kMsgSwatchClickedLocal:
		{
			int32 kind = -1;
			message->FindInt32("kind", &kind);
			ShowColorPicker(kind);
			return;
		}

		case kMsgColorRequest:
		{
			rgb_color color;
			const void* colorData;
			ssize_t size;
			if (message->FindData("color", B_RGB_COLOR_TYPE, &colorData, &size) != B_OK
					|| size != (ssize_t)sizeof(rgb_color))
				return;
			color = *(const rgb_color*)colorData;

			if (fColorPickKind == 0)
			{
				fBackgroundLiteral = color;
				RefreshPreview();
			}
			else if (fColorPickKind == 1)
			{
				fTextLiteral = color;
				RefreshPreview();
			}
			else if (fColorPickKind >= 2)
			{
				// Colore tema: DAL VIVO subito, non aspetta "Aggiorna" --
				// stesso principio del vero Excel, cambiare il tema si
				// vede immediatamente su ogni stile che lo referenzia.
				int role = fColorPickKind - 2;
				fTheme.colors[role] = color;
				fThemeSwatches[role]->SetColor(color);
				RefreshPreview();

				BMessage request(kMsgSetThemeColor);
				request.AddInt32("role", role);
				request.AddData("color", B_RGB_COLOR_TYPE, &color, sizeof(rgb_color));
				fTarget.SendMessage(&request);
			}
			return;
		}

		case kMsgApplyLocal:
		{
			int32 index = fStyleList->CurrentSelection();
			if (index < 0 || (size_t)index >= fListIDs.size())
				return;
			BMessage request(kMsgApplyNamedStyle);
			request.AddInt32("styleID", fListIDs[index]);
			fTarget.SendMessage(&request);
			return;
		}

		case kMsgNewFromSelectionLocal:
		{
			BMessage request(kMsgCreateNamedStyleFromSelection);
			request.AddString("name", fNameField->Text());
			fTarget.SendMessage(&request);
			return;
		}

		case kMsgRedefineLocal:
		{
			int32 index = fStyleList->CurrentSelection();
			if (index < 0 || (size_t)index >= fListIDs.size())
				return;

			BMessage request(kMsgRedefineNamedStyle);
			request.AddInt32("styleID", fListIDs[index]);
			int32 bgRole = CurrentRole(fBackgroundRoleField);
			request.AddBool("useThemeBackground", bgRole != kRoleLiteral);
			request.AddInt32("backgroundRole", bgRole != kRoleLiteral ? bgRole : (int32)eThemeBackground);
			request.AddData("backgroundColor", B_RGB_COLOR_TYPE, &fBackgroundLiteral, sizeof(rgb_color));
			int32 textRole = CurrentRole(fTextRoleField);
			request.AddBool("useThemeText", textRole != kRoleLiteral);
			request.AddInt32("textRole", textRole != kRoleLiteral ? textRole : (int32)eThemeText);
			request.AddData("textColor", B_RGB_COLOR_TYPE, &fTextLiteral, sizeof(rgb_color));
			request.AddBool("bold", fBoldCheckbox->Value() == B_CONTROL_ON);
			request.AddBool("italic", fItalicCheckbox->Value() == B_CONTROL_ON);
			request.AddBool("underline", fUnderlineCheckbox->Value() == B_CONTROL_ON);
			fTarget.SendMessage(&request);
			return;
		}

		case kMsgDeleteLocal:
		{
			int32 index = fStyleList->CurrentSelection();
			if (index < 0 || (size_t)index >= fListIDs.size())
				return;
			BMessage request(kMsgDeleteNamedStyle);
			request.AddInt32("styleID", fListIDs[index]);
			fTarget.SendMessage(&request);
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool NamedStyleWindow::QuitRequested()
{
	// Stessa regola di ogni altra finestra di questo progetto: resta
	// nascosta e riusabile.
	Hide();
	return false;
}
