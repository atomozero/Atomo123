/*
	Chart.cpp

	Vedi Chart.h.
*/

#include "Chart.h"

#include <cstdio>

#include <View.h>

#include "Cell.h"
#include "Container.h"
#include "Value.h"

static void ValueToLabel(const Value& v, BString& out)
{
	if (v.fType == eTextData)
		out = (const char*)v;
	else if (v.fType == eNumData)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", (double)v);
		out = buf;
	}
	else
		out = "";
}

bool BuildChartSeries(CContainer* doc, const range& r,
	std::vector<ChartSeries>& out)
{
	out.clear();
	if (!doc || r.right - r.left != 1)
		return false;

	for (int row = r.top; row <= r.bottom; row++)
	{
		cell labelCell(r.left, row);
		cell valueCell(r.left + 1, row);

		Value lv, vv;
		doc->GetValue(labelCell, lv);
		doc->GetValue(valueCell, vv);

		if (vv.fType != eNumData)
			continue;

		ChartSeries s;
		ValueToLabel(lv, s.label);
		s.value = (double)vv;
		out.push_back(s);
	}

	return !out.empty();
}

void ComputeBarLayout(const std::vector<ChartSeries>& data, BRect bounds,
	std::vector<BarLayout>& out)
{
	out.clear();
	if (data.empty())
		return;

	double maxValue = 0;
	for (size_t i = 0; i < data.size(); i++)
		if (data[i].value > maxValue)
			maxValue = data[i].value;
	if (maxValue <= 0)
		maxValue = 1;

	float slotWidth = bounds.Width() / data.size();
	float gap = slotWidth * 0.2f;
	if (gap > 10)
		gap = 10;

	for (size_t i = 0; i < data.size(); i++)
	{
		float left = bounds.left + i * slotWidth + gap / 2;
		float right = left + slotWidth - gap;
		float barHeight = bounds.Height() * (float)(data[i].value / maxValue);
		float top = bounds.bottom - barHeight;

		BarLayout bl;
		bl.bar.Set(left, top, right, bounds.bottom);
		out.push_back(bl);
	}
}

void DrawBarChart(BView* view, BRect frame, const std::vector<ChartSeries>& data)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);

	if (data.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString("Nessun dato da mostrare.", frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea = frame;
	plotArea.InsetBy(10, 10);
	plotArea.bottom -= 16;	// spazio per le etichette sotto le barre

	std::vector<BarLayout> bars;
	ComputeBarLayout(data, plotArea, bars);

	view->SetHighColor(70, 110, 190);
	for (size_t i = 0; i < bars.size(); i++)
		view->FillRect(bars[i].bar);

	view->SetHighColor(0, 0, 0);
	view->StrokeRect(frame);
	view->StrokeLine(BPoint(plotArea.left, plotArea.bottom),
		BPoint(plotArea.right, plotArea.bottom));

	for (size_t i = 0; i < bars.size() && i < data.size(); i++)
	{
		BPoint labelPos(bars[i].bar.left, plotArea.bottom + 12);
		view->DrawString(data[i].label.String(), labelPos);
	}
}
