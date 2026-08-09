/*
	Chart.cpp

	Vedi Chart.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "Chart.h"

#include <algorithm>
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

void ComputeLineLayout(const std::vector<ChartSeries>& data, BRect bounds,
	std::vector<LinePoint>& out)
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

	// Stessa larghezza di slot di ComputeBarLayout, cosi' un punto
	// della linea cade nello stesso centro orizzontale dell'etichetta
	// sotto -- coerenza visiva fra i due tipi di grafico che usano lo
	// stesso asse a categorie.
	float slotWidth = bounds.Width() / data.size();

	for (size_t i = 0; i < data.size(); i++)
	{
		float x = bounds.left + i * slotWidth + slotWidth / 2;
		float y = bounds.bottom - bounds.Height() * (float)(data[i].value / maxValue);

		LinePoint lp;
		lp.point.Set(x, y);
		out.push_back(lp);
	}
}

void DrawLineChart(BView* view, BRect frame, const std::vector<ChartSeries>& data)
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
	plotArea.bottom -= 16;	// spazio per le etichette sotto i punti

	std::vector<LinePoint> points;
	ComputeLineLayout(data, plotArea, points);

	view->SetHighColor(70, 110, 190);
	for (size_t i = 1; i < points.size(); i++)
		view->StrokeLine(points[i - 1].point, points[i].point);
	// Un pallino su ogni punto, non solo la spezzata: rende visibile
	// anche una serie con un solo valore (nessun segmento da
	// disegnare) e segna con chiarezza dove cade ogni valore reale.
	for (size_t i = 0; i < points.size(); i++)
	{
		BRect dot(points[i].point.x - 3, points[i].point.y - 3,
			points[i].point.x + 3, points[i].point.y + 3);
		view->FillEllipse(dot);
	}

	view->SetHighColor(0, 0, 0);
	view->StrokeRect(frame);
	view->StrokeLine(BPoint(plotArea.left, plotArea.bottom),
		BPoint(plotArea.right, plotArea.bottom));

	// Stessa larghezza di slot di ComputeLineLayout, per allineare
	// l'etichetta sotto il punto corrispondente (DrawString parte da
	// sinistra, non centrata: uno scarto fisso approssima il centro).
	float slotWidth = plotArea.Width() / data.size();
	for (size_t i = 0; i < points.size() && i < data.size(); i++)
	{
		BPoint labelPos(plotArea.left + i * slotWidth + 2, plotArea.bottom + 12);
		view->DrawString(data[i].label.String(), labelPos);
	}
}

// Tavolozza fissa per gli spicchi della torta: a differenza di barre/
// linee (una sola serie, un solo colore ha senso), ogni spicchio
// rappresenta una categoria diversa nello STESSO grafico e deve
// restare distinguibile dai vicini -- ciclica se le categorie sono
// piu' dei colori disponibili.
static const rgb_color kPieColors[] = {
	{ 70, 110, 190, 255 },
	{ 220, 120, 60, 255 },
	{ 90, 170, 90, 255 },
	{ 200, 90, 140, 255 },
	{ 210, 180, 60, 255 },
	{ 130, 100, 190, 255 },
	{ 80, 170, 170, 255 },
	{ 190, 90, 90, 255 },
};
static const int kPieColorCount = sizeof(kPieColors) / sizeof(kPieColors[0]);

void ComputePieLayout(const std::vector<ChartSeries>& data, std::vector<PieSlice>& out)
{
	out.clear();
	if (data.empty())
		return;

	double total = 0;
	for (size_t i = 0; i < data.size(); i++)
		if (data[i].value > 0)
			total += data[i].value;
	if (total <= 0)
		return;

	float angle = 0;
	for (size_t i = 0; i < data.size(); i++)
	{
		// Un valore non positivo non occupa nessuno spicchio (ampiezza
		// zero), ma resta al suo posto nell'elenco -- l'indice deve
		// continuare a combaciare con "data" per il colore/l'etichetta
		// nella legenda in DrawPieChart sotto.
		float sweep = (data[i].value > 0)
			? (float)(data[i].value / total * 360.0) : 0.0f;

		PieSlice slice;
		slice.startAngle = angle;
		slice.sweepAngle = sweep;
		out.push_back(slice);

		angle += sweep;
	}
}

void DrawPieChart(BView* view, BRect frame, const std::vector<ChartSeries>& data)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);

	if (data.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString("Nessun dato da mostrare.", frame.LeftTop() + BPoint(10, 20));
		return;
	}

	std::vector<PieSlice> slices;
	ComputePieLayout(data, slices);

	if (slices.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString("Nessun valore positivo da mostrare.", frame.LeftTop() + BPoint(10, 20));
		return;
	}

	// La legenda (un quadratino di colore + etichetta per riga) occupa
	// una striscia fissa a destra, la torta vera e propria il
	// quadrato piu' grande possibile a sinistra -- a differenza di
	// barre/linee, qui le etichette non stanno sotto ogni spicchio
	// (spicchi piccoli non hanno spazio per il testo).
	float legendWidth = 110;
	BRect pieArea = frame;
	pieArea.InsetBy(10, 10);
	pieArea.right -= legendWidth;

	float diameter = std::min(pieArea.Width(), pieArea.Height());
	BPoint center(pieArea.left + diameter / 2, pieArea.top + diameter / 2);
	float radius = diameter / 2;

	for (size_t i = 0; i < slices.size(); i++)
	{
		if (slices[i].sweepAngle <= 0)
			continue;
		view->SetHighColor(kPieColors[i % kPieColorCount]);
		view->FillArc(center, radius, radius, slices[i].startAngle, slices[i].sweepAngle);
	}

	view->SetHighColor(0, 0, 0);
	view->StrokeEllipse(center, radius, radius);
	view->StrokeRect(frame);

	float legendX = pieArea.right + 16;
	float legendY = frame.top + 14;
	for (size_t i = 0; i < data.size() && i < slices.size(); i++)
	{
		BRect swatch(legendX, legendY - 8, legendX + 10, legendY + 2);
		view->SetHighColor(kPieColors[i % kPieColorCount]);
		view->FillRect(swatch);
		view->SetHighColor(0, 0, 0);
		view->DrawString(data[i].label.String(), BPoint(legendX + 16, legendY));
		legendY += 16;
	}
}

void DrawChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	ChartType type)
{
	switch (type)
	{
		case eLineChart:
			DrawLineChart(view, frame, data);
			return;
		case ePieChart:
			DrawPieChart(view, frame, data);
			return;
		case eBarChart:
		default:
			DrawBarChart(view, frame, data);
			return;
	}
}
