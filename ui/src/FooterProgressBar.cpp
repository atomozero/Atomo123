/*
	FooterProgressBar.cpp

	Vedi FooterProgressBar.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "FooterProgressBar.h"

#include <cmath>

#include <InterfaceDefs.h>

FooterProgressBar::FooterProgressBar(const char* name)
	:
	BView(name, B_WILL_DRAW),
	fFraction(0.0f)
{
}

void FooterProgressBar::SetFraction(float fraction)
{
	if (fraction < 0.0f)
		fraction = 0.0f;
	if (fraction > 1.0f)
		fraction = 1.0f;
	if (fraction == fFraction)
		return;
	fFraction = fraction;
	Invalidate();
}

void FooterProgressBar::Draw(BRect updateRect)
{
	BRect bounds = Bounds();

	SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_2_TINT));
	StrokeRect(bounds);

	bounds.InsetBy(1, 1);
	if (fFraction > 0.0f)
	{
		BRect fill = bounds;
		fill.right = fill.left + fill.Width() * fFraction;
		// Stesso blu della selezione nella griglia (vedi SheetView.cpp),
		// per coerenza visiva con il resto dell'app.
		SetHighColor(30, 100, 200);
		FillRect(fill);
		bounds.left = fill.right + 1;
	}
	if (bounds.left <= bounds.right)
	{
		SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
		FillRect(bounds);
	}
}

void FooterProgressBar::GetPreferredSize(float* width, float* height)
{
	font_height fh;
	GetFontHeight(&fh);
	if (width)
		*width = 100.0f;
	if (height)
		*height = ceilf(fh.ascent + fh.descent + fh.leading) - 2.0f;
}

BSize FooterProgressBar::MinSize()
{
	float w, h;
	GetPreferredSize(&w, &h);
	return BSize(20.0f, h);
}

BSize FooterProgressBar::MaxSize()
{
	float w, h;
	GetPreferredSize(&w, &h);
	return BSize(B_SIZE_UNLIMITED, h);
}

BSize FooterProgressBar::PreferredSize()
{
	float w, h;
	GetPreferredSize(&w, &h);
	return BSize(w, h);
}
