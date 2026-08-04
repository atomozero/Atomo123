/*
	SheetTabView.cpp

	Vedi SheetTabView.h.
*/

#include "SheetTabView.h"

#include <Font.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <Region.h>
#include <Window.h>

static const float kTabPadding = 14;
static const float kMinTabWidth = 50;
static const float kArrowWidth = 18;
static const float kTabHeight = 22;

SheetTabView::SheetTabView(const char* name, uint32 switchWhat, BHandler* target)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS),
	fActiveIndex(0),
	fFirstVisible(0),
	fScrolling(false),
	fSwitchWhat(switchWhat),
	fTarget(target)
{
	SetExplicitMinSize(BSize(20, kTabHeight));
	SetExplicitPreferredSize(BSize(B_SIZE_UNSET, kTabHeight));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, kTabHeight));
}

SheetTabView::~SheetTabView()
{
}

BSize SheetTabView::MinSize()
{
	return BSize(20, kTabHeight);
}

BSize SheetTabView::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, kTabHeight);
}

BSize SheetTabView::PreferredSize()
{
	return BSize(200, kTabHeight);
}

void SheetTabView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	Invalidate();
}

void SheetTabView::SetSheets(const std::vector<BString>& names, int activeIndex,
	const std::vector<bool>* hasColor, const std::vector<rgb_color>* colors)
{
	fTabs.clear();
	BFont font;
	GetFont(&font);
	for (size_t i = 0; i < names.size(); i++)
	{
		TabInfo t;
		t.name = names[i];
		t.width = font.StringWidth(t.name.String()) + kTabPadding * 2;
		if (t.width < kMinTabWidth)
			t.width = kMinTabWidth;
		t.hasColor = hasColor && i < hasColor->size() && (*hasColor)[i];
		t.color = { 0, 0, 0, 255 };
		if (t.hasColor && colors && i < colors->size())
			t.color = (*colors)[i];
		fTabs.push_back(t);
	}
	fActiveIndex = activeIndex;

	Layout();
	if (activeIndex >= 0 && !IsIndexVisible(activeIndex))
	{
		fFirstVisible = activeIndex;
		Layout();
	}

	Invalidate();
}

bool SheetTabView::IsIndexVisible(int index) const
{
	for (size_t i = 0; i < fVisible.size(); i++)
	{
		if (fVisible[i].index == index)
			return true;
	}
	return false;
}

BRect SheetTabView::TabRectFor(int index) const
{
	for (size_t i = 0; i < fVisible.size(); i++)
	{
		if (fVisible[i].index == index)
			return fVisible[i].rect;
	}
	return BRect(); // non visibile con l'attuale fFirstVisible: rettangolo non valido
}

BRect SheetTabView::LeftArrowRect() const
{
	BRect b = Bounds();
	return BRect(b.left, b.top, b.left + kArrowWidth, b.bottom);
}

BRect SheetTabView::RightArrowRect() const
{
	BRect b = Bounds();
	return BRect(b.left + kArrowWidth, b.top, b.left + kArrowWidth * 2, b.bottom);
}

void SheetTabView::Layout()
{
	fVisible.clear();

	if (fTabs.empty())
	{
		fScrolling = false;
		fFirstVisible = 0;
		return;
	}

	BRect b = Bounds();
	float total = 0;
	for (size_t i = 0; i < fTabs.size(); i++)
		total += fTabs[i].width;

	fScrolling = total > b.Width();

	if (!fScrolling)
		fFirstVisible = 0;
	else
	{
		if (fFirstVisible < 0)
			fFirstVisible = 0;
		if (fFirstVisible >= (int)fTabs.size())
			fFirstVisible = (int)fTabs.size() - 1;
	}

	float x = fScrolling ? kArrowWidth * 2 : 0;
	float maxX = b.right;

	for (int i = fFirstVisible; i < (int)fTabs.size(); i++)
	{
		float w = fTabs[i].width;
		// La prima scheda si disegna comunque anche se non ci sta
		// tutta (meglio tagliata che nessuna scheda visibile).
		if (x + w > maxX && !fVisible.empty())
			break;
		VisibleTab v;
		v.index = i;
		v.rect = BRect(x, b.top, x + w, b.bottom);
		fVisible.push_back(v);
		x += w;
	}
}

void SheetTabView::Draw(BRect updateRect)
{
	Layout();

	BRect b = Bounds();
	SetHighColor(216, 216, 216);
	FillRect(b);

	if (fScrolling)
	{
		BRect leftRect = LeftArrowRect();
		BRect rightRect = RightArrowRect();
		bool canLeft = fFirstVisible > 0;
		bool canRight = !fVisible.empty()
			&& fVisible.back().index < (int)fTabs.size() - 1;

		SetHighColor(canLeft ? 90 : 190, canLeft ? 90 : 190, canLeft ? 90 : 190);
		FillTriangle(
			BPoint(leftRect.right - 6, leftRect.top + 5),
			BPoint(leftRect.right - 6, leftRect.bottom - 5),
			BPoint(leftRect.left + 4, (leftRect.top + leftRect.bottom) / 2));

		SetHighColor(canRight ? 90 : 190, canRight ? 90 : 190, canRight ? 90 : 190);
		FillTriangle(
			BPoint(rightRect.left + 6, rightRect.top + 5),
			BPoint(rightRect.left + 6, rightRect.bottom - 5),
			BPoint(rightRect.right - 4, (rightRect.top + rightRect.bottom) / 2));
	}

	BFont font;
	GetFont(&font);
	font_height fh;
	font.GetHeight(&fh);

	for (size_t v = 0; v < fVisible.size(); v++)
	{
		const VisibleTab& tab = fVisible[v];
		const TabInfo& info = fTabs[tab.index];
		bool active = tab.index == fActiveIndex;

		// Scheda colorata (import XLSX, <sheetPr><tabColor>): stesso
		// linguaggio visivo di Excel -- la scheda NON attiva mostra il
		// colore a tutta area (si nota anche senza guardarla da vicino,
		// esattamente come l'originale), quella attiva resta bianca
		// (deve confondersi con il foglio sopra, non spiccare) con solo
		// una barra colorata in basso a ricordare la scelta.
		if (active)
			SetHighColor(255, 255, 255);
		else if (info.hasColor)
			SetHighColor(info.color);
		else
			SetHighColor(220, 220, 220);
		FillRect(tab.rect);

		if (active && info.hasColor)
		{
			BRect bar(tab.rect.left, tab.rect.bottom - 3, tab.rect.right, tab.rect.bottom);
			SetHighColor(info.color);
			FillRect(bar);
		}

		SetHighColor(140, 140, 140);
		StrokeRect(tab.rect);

		if (active)
			SetHighColor(0, 0, 0);
		else
			SetHighColor(90, 90, 90);

		float textY = tab.rect.top
			+ (tab.rect.Height() - (fh.ascent + fh.descent)) / 2 + fh.ascent;
		BRect textClip = tab.rect.InsetByCopy(kTabPadding - 4, 0);
		ConstrainClippingRegion(NULL);
		BRegion clipRegion(textClip);
		ConstrainClippingRegion(&clipRegion);
		DrawString(fTabs[tab.index].name.String(),
			BPoint(tab.rect.left + kTabPadding, textY));
		ConstrainClippingRegion(NULL);
	}
}

void SheetTabView::MouseDown(BPoint where)
{
	if (fScrolling)
	{
		if (LeftArrowRect().Contains(where))
		{
			if (fFirstVisible > 0)
			{
				fFirstVisible--;
				Invalidate();
			}
			return;
		}
		if (RightArrowRect().Contains(where))
		{
			if (!fVisible.empty() && fVisible.back().index < (int)fTabs.size() - 1)
			{
				fFirstVisible++;
				Invalidate();
			}
			return;
		}
	}

	for (size_t i = 0; i < fVisible.size(); i++)
	{
		if (fVisible[i].rect.Contains(where))
		{
			if (fTarget)
			{
				BMessage msg(fSwitchWhat);
				msg.AddInt32("index", fVisible[i].index);
				BMessenger(fTarget).SendMessage(&msg);
			}
			break;
		}
	}
}
