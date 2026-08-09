/*
	ToolbarView.cpp

	Vedi ToolbarView.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ToolbarView.h"

#include <Button.h>
#include <SeparatorView.h>
#include <Window.h>

static const float kInset = 4;
static const float kSpacing = 4;

ToolbarView::ToolbarView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS),
	fRowCount(1),
	fRowHeight(28),
	fNaturalWidth(kInset * 2)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}

ToolbarView::~ToolbarView()
{
}

void ToolbarView::AttachedToWindow()
{
	BView::AttachedToWindow();
	Layout();
}

void ToolbarView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	Layout();
}

BSize ToolbarView::MinSize()
{
	// Non ha senso restringersi sotto la larghezza del gruppo piu'
	// largo: un gruppo non si spezza mai fra due righe (vedi
	// ToolbarView.h), quindi quella e' la vera larghezza minima utile,
	// non un valore arbitrario piu' piccolo.
	float widest = 0;
	for (size_t g = 0; g < fGroups.size(); g++)
	{
		float w = _GroupWidth(fGroups[g]);
		if (w > widest)
			widest = w;
	}
	return BSize(widest + kInset * 2, fRowHeight);
}

BSize ToolbarView::MaxSize()
{
	// L'altezza non ha un tetto fisso: con una finestra molto stretta e
	// molti gruppi, ogni gruppo puo' finire sulla propria riga.
	return BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED);
}

BSize ToolbarView::PreferredSize()
{
	// La dimensione "ideale" resta quella di una sola riga con tutti i
	// gruppi affiancati -- GetHeightForWidth sotto e' quello che decide
	// davvero quante righe servono per la larghezza che il layout le
	// assegna per davvero.
	return BSize(fNaturalWidth, fRowHeight);
}

bool ToolbarView::HasHeightForWidth()
{
	return true;
}

void ToolbarView::GetHeightForWidth(float width, float* min, float* max, float* preferred)
{
	int rows = _PackGroups(width, NULL);
	float height = rows * fRowHeight;
	if (min)
		*min = height;
	if (max)
		*max = height;
	if (preferred)
		*preferred = height;
}

void ToolbarView::AddButton(BButton* button, const char* label)
{
	AddChild(button);

	if (fGroups.empty())
	{
		Group g;
		g.separator = NULL;
		fGroups.push_back(g);
	}

	ButtonItem item;
	item.view = button;
	item.label = label;
	fGroups.back().buttons.push_back(item);

	float w, h;
	button->GetPreferredSize(&w, &h);
	if (h + kInset * 2 > fRowHeight)
		fRowHeight = h + kInset * 2;

	fNaturalWidth = kInset;
	for (size_t g = 0; g < fGroups.size(); g++)
	{
		if (g > 0)
			fNaturalWidth += kSpacing;
		fNaturalWidth += _GroupWidth(fGroups[g]);
	}
	fNaturalWidth += kInset;
}

void ToolbarView::AddSeparator()
{
	Group g;
	g.separator = new BSeparatorView(B_VERTICAL);
	AddChild(g.separator);
	fGroups.push_back(g);
}

int ToolbarView::ButtonCount() const
{
	int count = 0;
	for (size_t g = 0; g < fGroups.size(); g++)
		count += (int)fGroups[g].buttons.size();
	return count;
}

float ToolbarView::_GroupWidth(const Group& group) const
{
	float w = 0;
	for (size_t i = 0; i < group.buttons.size(); i++)
	{
		if (i > 0)
			w += kSpacing;
		float bw, bh;
		group.buttons[i].view->GetPreferredSize(&bw, &bh);
		w += bw;
	}
	return w;
}

int ToolbarView::_PackGroups(float availWidth, std::vector<int>* outRows) const
{
	if (outRows)
	{
		outRows->clear();
		outRows->resize(fGroups.size(), 0);
	}

	if (fGroups.empty())
		return 1;

	float sepWidth = 0, sepHeight = 0;
	for (size_t g = 0; g < fGroups.size(); g++)
	{
		if (fGroups[g].separator)
		{
			fGroups[g].separator->GetPreferredSize(&sepWidth, &sepHeight);
			break;
		}
	}

	int row = 0;
	float x = kInset;
	bool firstInRow = true;
	float limit = availWidth - kInset;

	for (size_t g = 0; g < fGroups.size(); g++)
	{
		float w = _GroupWidth(fGroups[g]);
		bool hasSep = fGroups[g].separator != NULL;
		float widthAsNonFirst = kSpacing + (hasSep ? sepWidth + kSpacing : 0) + w;

		if (!firstInRow && x + widthAsNonFirst > limit)
		{
			row++;
			x = kInset;
			firstInRow = true;
		}

		if (outRows)
			(*outRows)[g] = row;

		x += firstInRow ? w : widthAsNonFirst;
		firstInRow = false;
	}

	return row + 1;
}

void ToolbarView::Layout()
{
	float availWidth = Bounds().Width();
	if (availWidth <= 0 || fGroups.empty())
		return;

	std::vector<int> rows;
	fRowCount = _PackGroups(availWidth, &rows);

	float sepWidth = 0, sepHeight = 0;
	for (size_t g = 0; g < fGroups.size(); g++)
	{
		if (fGroups[g].separator)
		{
			fGroups[g].separator->GetPreferredSize(&sepWidth, &sepHeight);
			break;
		}
	}

	float x = kInset;
	int currentRow = -1;
	for (size_t g = 0; g < fGroups.size(); g++)
	{
		Group& group = fGroups[g];
		bool firstInRow = (rows[g] != currentRow);
		if (firstInRow)
		{
			currentRow = rows[g];
			x = kInset;
		}

		float y = kInset + currentRow * fRowHeight;

		if (group.separator)
		{
			if (firstInRow)
			{
				if (!group.separator->IsHidden())
					group.separator->Hide();
			}
			else
			{
				x += kSpacing;
				group.separator->MoveTo(x, y);
				group.separator->ResizeTo(sepWidth, sepHeight);
				if (group.separator->IsHidden())
					group.separator->Show();
				x += sepWidth + kSpacing;
			}
		}

		for (size_t i = 0; i < group.buttons.size(); i++)
		{
			BButton* button = group.buttons[i].view;
			float w, h;
			button->GetPreferredSize(&w, &h);
			button->MoveTo(x, y);
			button->ResizeTo(w, h);
			if (button->IsHidden())
				button->Show();
			x += w + kSpacing;
		}
	}
}
