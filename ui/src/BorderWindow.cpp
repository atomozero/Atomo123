/*
	BorderWindow.cpp

	Vedi BorderWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "BorderWindow.h"

#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <ColorControl.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <View.h>

static const uint32 kMsgApplyLocal = 'bapl';
static const uint32 kMsgPreviewUpdate = 'bprv';

// Disegna la cella cosi' come verra' -- un quadrato con i quattro lati
// on/off, allo spessore e nel colore correnti dei controlli, invece di
// dover immaginare il risultato di quattro voci di menu applicate in
// sequenza. Nessuna interazione propria (i checkbox restano l'unico
// modo di scegliere i lati, piu' affidabile e testabile di un'area
// cliccabile): si limita a rispecchiare lo stato di BorderWindow,
// aggiornata da _UpdatePreview() a ogni cambiamento di un controllo.
class BorderPreviewView : public BView {
public:
	BorderPreviewView()
		:
		BView("borderPreview", B_WILL_DRAW),
		fTop(false), fLeft(false), fBottom(false), fRight(false),
		fThickness(1)
	{
		fColor = (rgb_color){0, 0, 0, 255};
		SetViewColor(255, 255, 255);
		SetExplicitMinSize(BSize(100, 100));
		SetExplicitMaxSize(BSize(100, 100));
		SetExplicitPreferredSize(BSize(100, 100));
	}

	void SetState(bool top, bool left, bool bottom, bool right,
		int thickness, rgb_color color)
	{
		fTop = top;
		fLeft = left;
		fBottom = bottom;
		fRight = right;
		fThickness = thickness;
		fColor = color;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		BRect b = Bounds();
		SetHighColor(180, 180, 180);
		StrokeRect(b.InsetByCopy(0.5f, 0.5f));

		SetHighColor(fColor);
		// Spessore in pixel sullo schermo: 1/3/5, proporzionato ma
		// sempre distinguibile a occhio anche il "sottile" contro lo
		// sfondo bianco.
		float penSize = 1.0f + (float)(fThickness - 1) * 2.0f;
		if (penSize < 1.0f)
			penSize = 1.0f;
		SetPenSize(penSize);

		BRect inner = b.InsetByCopy(penSize * 0.5f, penSize * 0.5f);
		if (fTop)
			StrokeLine(inner.LeftTop(), inner.RightTop());
		if (fBottom)
			StrokeLine(inner.LeftBottom(), inner.RightBottom());
		if (fLeft)
			StrokeLine(inner.LeftTop(), inner.LeftBottom());
		if (fRight)
			StrokeLine(inner.RightTop(), inner.RightBottom());
	}

private:
	bool fTop, fLeft, fBottom, fRight;
	int fThickness;
	rgb_color fColor;
};

BorderWindow::BorderWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 520, 400), "Bordo cella",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	BMessage* topMsg = new BMessage(kMsgPreviewUpdate);
	fTopBox = new BCheckBox("borderTop", "Superiore", topMsg);
	fTopBox->SetTarget(this);
	BMessage* leftMsg = new BMessage(kMsgPreviewUpdate);
	fLeftBox = new BCheckBox("borderLeft", "Sinistro", leftMsg);
	fLeftBox->SetTarget(this);
	BMessage* bottomMsg = new BMessage(kMsgPreviewUpdate);
	fBottomBox = new BCheckBox("borderBottom", "Inferiore", bottomMsg);
	fBottomBox->SetTarget(this);
	BMessage* rightMsg = new BMessage(kMsgPreviewUpdate);
	fRightBox = new BCheckBox("borderRight", "Destro", rightMsg);
	fRightBox->SetTarget(this);

	BPopUpMenu* thicknessMenu = new BPopUpMenu("thickness");
	BMessage* thinMsg = new BMessage(kMsgPreviewUpdate);
	thicknessMenu->AddItem(new BMenuItem("Sottile", thinMsg));
	BMessage* mediumMsg = new BMessage(kMsgPreviewUpdate);
	thicknessMenu->AddItem(new BMenuItem("Medio", mediumMsg));
	BMessage* thickMsg = new BMessage(kMsgPreviewUpdate);
	thicknessMenu->AddItem(new BMenuItem("Spesso", thickMsg));
	thicknessMenu->ItemAt(0)->SetMarked(true);
	thicknessMenu->SetTargetForItems(this);
	fThicknessField = new BMenuField("thicknessField", "Spessore:", thicknessMenu);

	fColorControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 8, "borderColorControl",
		new BMessage(kMsgPreviewUpdate));
	fColorControl->SetTarget(this);
	fColorControl->SetValue((rgb_color){0, 0, 0, 255});

	fPreview = new BorderPreviewView();

	BBox* sidesBox = new BBox("sidesBox");
	sidesBox->SetLabel("Lati");
	BLayoutBuilder::Group<>(sidesBox, B_VERTICAL, 4)
		.SetInsets(8, sidesBox->TopBorderOffset() + 8, 8, 8)
		.Add(fTopBox)
		.Add(fLeftBox)
		.Add(fRightBox)
		.Add(fBottomBox)
		.Add(fThicknessField);

	BButton* applyButton = new BButton("apply", "Applica", new BMessage(kMsgApplyLocal));
	applyButton->SetTarget(this);
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.AddGroup(B_HORIZONTAL, 12)
			.Add(sidesBox)
			.AddGroup(B_VERTICAL, 4)
				.Add(fPreview)
				.AddGlue()
			.End()
		.End()
		.Add(fColorControl)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(applyButton)
		.End();

	_UpdatePreview();
}

int BorderWindow::_SelectedThickness() const
{
	BMenuItem* marked = fThicknessField->Menu()->FindMarked();
	int32 index = marked ? fThicknessField->Menu()->IndexOf(marked) : 0;
	return (int)index + 1; // 0..2 -> 1..3, stesso significato di CellStyle::fTBorderColor ecc.
}

void BorderWindow::_UpdatePreview()
{
	fPreview->SetState(
		fTopBox->Value() == B_CONTROL_ON,
		fLeftBox->Value() == B_CONTROL_ON,
		fBottomBox->Value() == B_CONTROL_ON,
		fRightBox->Value() == B_CONTROL_ON,
		_SelectedThickness(),
		fColorControl->ValueAsColor());
}

void BorderWindow::SetInitial(bool top, bool left, bool bottom, bool right,
	int thickness, rgb_color color)
{
	fTopBox->SetValue(top ? B_CONTROL_ON : B_CONTROL_OFF);
	fLeftBox->SetValue(left ? B_CONTROL_ON : B_CONTROL_OFF);
	fBottomBox->SetValue(bottom ? B_CONTROL_ON : B_CONTROL_OFF);
	fRightBox->SetValue(right ? B_CONTROL_ON : B_CONTROL_OFF);

	if (thickness < 1)
		thickness = 1;
	if (thickness > 3)
		thickness = 3;
	fThicknessField->Menu()->ItemAt(thickness - 1)->SetMarked(true);

	fColorControl->SetValue(color);

	_UpdatePreview();
}

void BorderWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgPreviewUpdate:
			_UpdatePreview();
			return;

		case kMsgApplyLocal:
		{
			BMessage request(kMsgBorderFormatRequest);
			request.AddBool("top", fTopBox->Value() == B_CONTROL_ON);
			request.AddBool("left", fLeftBox->Value() == B_CONTROL_ON);
			request.AddBool("bottom", fBottomBox->Value() == B_CONTROL_ON);
			request.AddBool("right", fRightBox->Value() == B_CONTROL_ON);
			request.AddInt32("thickness", _SelectedThickness());
			rgb_color color = fColorControl->ValueAsColor();
			request.AddData("color", B_RGB_COLOR_TYPE, &color, sizeof(rgb_color));
			fTarget.SendMessage(&request);
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool BorderWindow::QuitRequested()
{
	// Stessa regola di ColorWindow/FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
