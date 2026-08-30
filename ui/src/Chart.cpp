/*
	Chart.cpp

	Vedi Chart.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "Chart.h"

#include <algorithm>
#include <cstdio>

#include <Catalog.h>
#include <Font.h>
#include <View.h>

#include "Cell.h"
#include "Container.h"
#include "Value.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Chart"

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

// Disegna "text" andando a capo per stare entro "maxWidth", fino a
// "maxLines" righe -- BView::DrawString da solo non tronca ne' va a
// capo, quindi un'etichetta piu' larga dello slot/della striscia a
// disposizione (es. un nome di categoria lungo come "United States of
// America", o "Channel Partners" nella legenda) sconfinava fuori dal
// grafico (bug segnalato dall'utente). Suddivisione "greedy" classica
// per parole (spazio come separatore): non serve altro per etichette
// corte come nomi di categoria/serie. Se il testo non entra comunque
// in "maxLines" righe, le parole in eccesso si accorpano nell'ultima
// riga, troncata con l'ellissi (BFont::TruncateString) -- l'altezza
// occupata resta cosi' sempre prevedibile per chi riserva lo spazio
// sotto/accanto al grafico. "centered"=true centra ogni riga su
// "anchor.x" (etichette di categoria, sotto una barra/un punto);
// false allinea a sinistra (voci di legenda, dopo il quadratino di
// colore). Ritorna l'altezza totale disegnata, cosi' i chiamanti con
// piu' voci in colonna (la legenda) possono spaziarle senza
// sovrapporsi quando una voce va a capo.
static float DrawWrappedLabel(BView* view, const char* text, BPoint anchor,
	float maxWidth, int maxLines, bool centered)
{
	if (!text || !text[0] || maxWidth < 4 || maxLines < 1)
		return 0;

	font_height fh;
	view->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;

	std::vector<BString> words;
	BString word;
	for (int32 i = 0; text[i]; i++)
	{
		if (text[i] == ' ')
		{
			if (word.Length() > 0)
			{
				words.push_back(word);
				word = "";
			}
		}
		else
			word << text[i];
	}
	if (word.Length() > 0)
		words.push_back(word);
	if (words.empty())
		return 0;

	std::vector<BString> lines;
	BString current = words[0];
	for (size_t i = 1; i < words.size(); i++)
	{
		BString candidate = current;
		candidate << " " << words[i];
		if (view->StringWidth(candidate.String()) <= maxWidth)
			current = candidate;
		else
		{
			lines.push_back(current);
			current = words[i];
		}
	}
	lines.push_back(current);

	BFont font;
	view->GetFont(&font);

	// Piu' righe di quante concesse: le eccedenti si accorpano
	// nell'ultima, troncata con l'ellissi -- mai piu' di "maxLines"
	// righe disegnate.
	if ((int)lines.size() > maxLines)
	{
		BString merged = lines[maxLines - 1];
		for (size_t i = maxLines; i < lines.size(); i++)
			merged << " " << lines[i];
		lines.resize(maxLines);
		font.TruncateString(&merged, B_TRUNCATE_END, maxWidth);
		lines[maxLines - 1] = merged;
	}
	// Anche una singola riga puo' restare piu' larga di "maxWidth" (una
	// sola parola lunghissima, senza spazi su cui spezzare): troncarla
	// comunque, altrimenti sconfinerebbe lo stesso.
	for (size_t i = 0; i < lines.size(); i++)
	{
		if (view->StringWidth(lines[i].String()) > maxWidth)
			font.TruncateString(&lines[i], B_TRUNCATE_END, maxWidth);
	}

	for (size_t i = 0; i < lines.size(); i++)
	{
		float x = centered ? anchor.x - view->StringWidth(lines[i].String()) / 2 : anchor.x;
		view->DrawString(lines[i].String(), BPoint(x, anchor.y + i * lineHeight));
	}

	return lines.size() * lineHeight;
}

// Titolo centrato in grassetto sopra "frame" -- condiviso da barre/
// linee/torta. Una stringa vuota non disegna nulla: i chiamanti
// riservano lo spazio in piu' solo quando c'e' davvero un titolo, cosi'
// un ChartObject senza titolo (il caso comune, e ogni file .ascd
// scritto prima di questo campo) si disegna esattamente come prima.
static void DrawChartTitle(BView* view, BRect frame, const BString& title)
{
	if (title.IsEmpty())
		return;

	BFont font;
	view->GetFont(&font);
	font.SetFace(B_BOLD_FACE);
	view->SetFont(&font, B_FONT_FACE);

	view->SetHighColor(0, 0, 0);
	// Troncato con l'ellissi se piu' largo del frame (meno un margine):
	// un titolo lunghissimo sconfinerebbe altrimenti a sinistra/destra,
	// stesso principio delle etichette di categoria/legenda sotto (vedi
	// DrawWrappedLabel) ma su una sola riga, il titolo resta sempre
	// centrato su una riga sola.
	BString truncated = title;
	font.TruncateString(&truncated, B_TRUNCATE_END, frame.Width() - 20);
	float width = view->StringWidth(truncated.String());
	view->DrawString(truncated.String(), BPoint(frame.left + (frame.Width() - width) / 2, frame.top + 14));

	font.SetFace(B_REGULAR_FACE);
	view->SetFont(&font, B_FONT_FACE);
}

// Righe massime per un'etichetta che va a capo (DrawWrappedLabel):
// due per le categorie sotto barre/punti e per le voci di legenda --
// abbastanza per un nome ragionevolmente lungo senza far crescere
// troppo lo spazio riservato sotto/accanto al grafico.
static const int kCategoryLabelMaxLines = 2;
static const int kLegendLabelMaxLines = 2;

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

// Stesso schema di BuildChartSeries sopra, ma ENTRAMBE le colonne
// devono essere numeriche riga per riga -- niente colonna etichetta,
// vedi il commento su ScatterPoint in Chart.h.
bool BuildScatterSeries(CContainer* doc, const range& r, std::vector<ScatterPoint>& out)
{
	out.clear();
	if (!doc || r.right - r.left != 1)
		return false;

	for (int row = r.top; row <= r.bottom; row++)
	{
		Value xv, yv;
		doc->GetValue(cell(r.left, row), xv);
		doc->GetValue(cell(r.left + 1, row), yv);

		if (xv.fType != eNumData || yv.fType != eNumData)
			continue;

		ScatterPoint p;
		p.x = (double)xv;
		p.y = (double)yv;
		out.push_back(p);
	}

	return !out.empty();
}

// Intervallo di valori della serie, sempre comprensivo dello zero (la
// linea di base convenzionale di un grafico a barre/linee): con dati
// tutti positivi (il caso comune) e' equivalente al vecchio
// "ChartMaxValue" di sola andata (0..max). Con valori negativi, minValue
// scende sotto zero cosi' ComputeBarLayout/ComputeLineLayout possono
// disegnare quella parte della serie SOTTO la linea di zero invece di
// fuori dall'area disegnabile (bug corretto qui). Una serie piatta o
// vuota (minValue == maxValue, es. tutti zero) allarga maxValue di 1
// cosi' i chiamanti possono dividere per (maxValue - minValue) senza
// controllare separatamente il caso degenere.
static void ChartValueRange(const std::vector<ChartSeries>& data, double* outMin, double* outMax)
{
	double minValue = 0, maxValue = 0;
	for (size_t i = 0; i < data.size(); i++)
	{
		if (data[i].value < minValue)
			minValue = data[i].value;
		if (data[i].value > maxValue)
			maxValue = data[i].value;
	}
	if (minValue == maxValue)
		maxValue = minValue + 1;
	*outMin = minValue;
	*outMax = maxValue;
}

// Coordinata Y di "value" dentro "bounds", scalata sull'intervallo
// [minValue, maxValue] -- minValue cade su bounds.bottom, maxValue su
// bounds.top. Condivisa da ComputeBarLayout/ComputeLineLayout/
// ComputeYAxisTicks cosi' i tre usano sempre la stessa proiezione.
static float ChartValueToY(double value, double minValue, double maxValue, BRect bounds)
{
	return bounds.bottom - bounds.Height() * (float)((value - minValue) / (maxValue - minValue));
}

void ComputeYAxisTicks(double minValue, double maxValue, BRect plotArea, std::vector<AxisTick>& out)
{
	out.clear();
	if (maxValue <= minValue)
		maxValue = minValue + 1;

	// 5 tacche equidistanti da minValue a maxValue: abbastanza per
	// leggere la scala senza affollare l'asse su un grafico piccolo
	// come quello di ChartView/un grafico incorporato nel foglio.
	const int kDivisions = 4;
	for (int i = 0; i <= kDivisions; i++)
	{
		double value = minValue + (maxValue - minValue) * i / kDivisions;
		float y = ChartValueToY(value, minValue, maxValue, plotArea);

		AxisTick tick;
		tick.y = y;
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", value);
		tick.label = buf;
		out.push_back(tick);
	}
}

void DrawYAxisGrid(BView* view, BRect plotArea, double minValue, double maxValue)
{
	std::vector<AxisTick> ticks;
	ComputeYAxisTicks(minValue, maxValue, plotArea, ticks);

	view->SetHighColor(225, 225, 225);
	for (size_t i = 0; i < ticks.size(); i++)
		view->StrokeLine(BPoint(plotArea.left, ticks[i].y), BPoint(plotArea.right, ticks[i].y));

	view->SetHighColor(90, 90, 90);
	for (size_t i = 0; i < ticks.size(); i++)
	{
		float width = view->StringWidth(ticks[i].label.String());
		view->DrawString(ticks[i].label.String(), BPoint(plotArea.left - width - 6, ticks[i].y + 4));
	}
}

void ComputeBarLayout(const std::vector<ChartSeries>& data, BRect bounds,
	std::vector<BarLayout>& out)
{
	out.clear();
	if (data.empty())
		return;

	double minValue, maxValue;
	ChartValueRange(data, &minValue, &maxValue);

	float slotWidth = bounds.Width() / data.size();
	float gap = slotWidth * 0.2f;
	if (gap > 10)
		gap = 10;

	float zeroY = ChartValueToY(0.0, minValue, maxValue, bounds);

	for (size_t i = 0; i < data.size(); i++)
	{
		float left = bounds.left + i * slotWidth + gap / 2;
		float right = left + slotWidth - gap;
		float valueY = ChartValueToY(data[i].value, minValue, maxValue, bounds);

		BarLayout bl;
		// La barra va sempre dalla linea di zero al valore, qualunque
		// sia il segno: min/max invece di "top fisso, bottom fisso"
		// cosi' un valore negativo scende sotto zero invece di
		// produrre un rettangolo con top > bottom (invisibile/storto).
		bl.bar.Set(left, std::min(valueY, zeroY), right, std::max(valueY, zeroY));
		out.push_back(bl);
	}
}

void DrawBarChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea = frame;
	plotArea.InsetBy(10, 10);
	plotArea.top += 14;	// spazio per l'etichetta del valore sopra le barre
	if (!title.IsEmpty())
		plotArea.top += 18;	// spazio per il titolo, vedi DrawChartTitle

	double minValue, maxValue;
	ChartValueRange(data, &minValue, &maxValue);

	// Spazio per le etichette di categoria sotto: fino a
	// kCategoryLabelMaxLines righe (vedi DrawWrappedLabel), non piu'
	// una sola come prima di questo fix -- un'etichetta lunga (es. un
	// nome di categoria) puo' ora andare a capo invece di sconfinare
	// fuori dal grafico (bug segnalato dall'utente).
	font_height fh;
	view->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;
	plotArea.bottom -= 12 + lineHeight * kCategoryLabelMaxLines + 4;
	// La riga delle etichette di categoria resta SEMPRE a questa
	// posizione fissa (calcolata PRIMA dell'eventuale restrizione
	// aggiuntiva qui sotto), anche quando la serie ha valori negativi:
	// solo il "pavimento" delle barre sale, non la riga di categoria.
	float categoryLabelY = plotArea.bottom + 12;
	if (minValue < 0)
		// Spazio aggiuntivo perche' l'etichetta valore di una barra
		// negativa (disegnata subito sotto la barra) non finisca a
		// sovrapporsi alla riga di categoria fissa sopra -- prima di
		// questo fix le due etichette cadevano sulla STESSA
		// coordinata Y quando una barra toccava il minimo della
		// serie (bug segnalato dall'utente).
		plotArea.bottom -= 14;

	// Riserva a sinistra lo spazio per le etichette dell'asse Y,
	// misurando la piu' larga con il font corrente -- le coordinate Y
	// delle tacche non dipendono da plotArea.left/right, quindi vanno
	// bene anche calcolate prima di restringere plotArea qui sotto.
	std::vector<AxisTick> ticks;
	ComputeYAxisTicks(minValue, maxValue, plotArea, ticks);
	float axisLabelWidth = 0;
	for (size_t i = 0; i < ticks.size(); i++)
	{
		float width = view->StringWidth(ticks[i].label.String());
		if (width > axisLabelWidth)
			axisLabelWidth = width;
	}
	plotArea.left += axisLabelWidth + 6;

	std::vector<BarLayout> bars;
	ComputeBarLayout(data, plotArea, bars);

	DrawYAxisGrid(view, plotArea, minValue, maxValue);

	view->SetHighColor(70, 110, 190);
	for (size_t i = 0; i < bars.size(); i++)
		view->FillRect(bars[i].bar);

	// Valore numerico accanto a ogni barra, centrato -- sopra per un
	// valore positivo (plotArea.top riservato apposta qui sopra),
	// sotto per uno negativo (la barra scende sotto la linea di zero,
	// vedi ComputeBarLayout: mettere l'etichetta sopra la barra la
	// piazzerebbe vicino alla linea di zero, lontano dalla barra
	// vera). +4 in entrambi i casi (non +12 sotto): la riga di
	// categoria fissa sotto ha gia' il suo spazio riservato apposta
	// sopra, vedi "categoryLabelY".
	view->SetHighColor(40, 40, 40);
	for (size_t i = 0; i < bars.size() && i < data.size(); i++)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", data[i].value);
		float width = view->StringWidth(buf);
		float x = bars[i].bar.left + bars[i].bar.Width() / 2 - width / 2;
		float y = (data[i].value >= 0) ? bars[i].bar.top - 4 : bars[i].bar.bottom + 4;
		view->DrawString(buf, BPoint(x, y));
	}

	view->SetHighColor(0, 0, 0);
	view->StrokeRect(frame);
	// Linea di zero: sul fondo di plotArea con soli valori positivi
	// (comportamento di sempre), ma sale a meta' se la serie ha anche
	// valori negativi -- e' la vera linea di base delle barre, non il
	// bordo del grafico.
	float zeroY = ChartValueToY(0.0, minValue, maxValue, plotArea);
	view->StrokeLine(BPoint(plotArea.left, zeroY), BPoint(plotArea.right, zeroY));

	// Etichetta centrata sotto la barra e avvolta su piu' righe se
	// serve (vedi DrawWrappedLabel), invece di un unico DrawString a
	// sinistra che sconfinava fuori dallo slot con un nome lungo.
	// slotWidth qui e' la stessa identica formula di ComputeBarLayout
	// (non esposta al chiamante), riusata solo per il centraggio.
	float slotWidth = plotArea.Width() / data.size();
	for (size_t i = 0; i < bars.size() && i < data.size(); i++)
	{
		float centerX = bars[i].bar.left + bars[i].bar.Width() / 2;
		DrawWrappedLabel(view, data[i].label.String(), BPoint(centerX, categoryLabelY),
			slotWidth - 2, kCategoryLabelMaxLines, true);
	}
}

void ComputeLineLayout(const std::vector<ChartSeries>& data, BRect bounds,
	std::vector<LinePoint>& out)
{
	out.clear();
	if (data.empty())
		return;

	double minValue, maxValue;
	ChartValueRange(data, &minValue, &maxValue);

	// Stessa larghezza di slot di ComputeBarLayout, cosi' un punto
	// della linea cade nello stesso centro orizzontale dell'etichetta
	// sotto -- coerenza visiva fra i due tipi di grafico che usano lo
	// stesso asse a categorie.
	float slotWidth = bounds.Width() / data.size();

	for (size_t i = 0; i < data.size(); i++)
	{
		float x = bounds.left + i * slotWidth + slotWidth / 2;
		float y = ChartValueToY(data[i].value, minValue, maxValue, bounds);

		LinePoint lp;
		lp.point.Set(x, y);
		out.push_back(lp);
	}
}

void DrawLineChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea = frame;
	plotArea.InsetBy(10, 10);
	plotArea.top += 14;	// spazio per l'etichetta del valore sopra i punti
	if (!title.IsEmpty())
		plotArea.top += 18;	// spazio per il titolo, vedi DrawChartTitle

	double minValue, maxValue;
	ChartValueRange(data, &minValue, &maxValue);

	// Spazio per le etichette di categoria sotto, fino a
	// kCategoryLabelMaxLines righe -- vedi il commento gemello in
	// DrawBarChart.
	font_height fh;
	view->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;
	plotArea.bottom -= 12 + lineHeight * kCategoryLabelMaxLines + 4;
	// Stessa correzione di DrawBarChart (vedi il commento gemello li'):
	// la riga di categoria resta fissa qui, solo il "pavimento" dei
	// punti sale quando la serie ha valori negativi, cosi' l'etichetta
	// valore di un punto negativo non ci si sovrappone piu'.
	float categoryLabelY = plotArea.bottom + 12;
	if (minValue < 0)
		plotArea.bottom -= 14;

	// Stesso margine sinistro per l'asse Y di DrawBarChart -- vedi il
	// commento li' sopra per il perche' delle tacche calcolate prima
	// di restringere plotArea.
	std::vector<AxisTick> ticks;
	ComputeYAxisTicks(minValue, maxValue, plotArea, ticks);
	float axisLabelWidth = 0;
	for (size_t i = 0; i < ticks.size(); i++)
	{
		float width = view->StringWidth(ticks[i].label.String());
		if (width > axisLabelWidth)
			axisLabelWidth = width;
	}
	plotArea.left += axisLabelWidth + 6;

	std::vector<LinePoint> points;
	ComputeLineLayout(data, plotArea, points);

	DrawYAxisGrid(view, plotArea, minValue, maxValue);

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

	// Valore numerico accanto a ogni punto, centrato -- sopra per un
	// valore positivo (plotArea.top riservato apposta qui sopra), sotto
	// per uno negativo, stesso principio delle barre in DrawBarChart.
	// +8 sotto (non di piu'): il pallino ha gia' raggio 3, la riga di
	// categoria fissa sotto ha il suo spazio riservato a parte, vedi
	// "categoryLabelY".
	view->SetHighColor(40, 40, 40);
	for (size_t i = 0; i < points.size() && i < data.size(); i++)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", data[i].value);
		float width = view->StringWidth(buf);
		float y = (data[i].value >= 0) ? points[i].point.y - 8 : points[i].point.y + 8;
		view->DrawString(buf, BPoint(points[i].point.x - width / 2, y));
	}

	view->SetHighColor(0, 0, 0);
	view->StrokeRect(frame);
	// Linea di zero (vedi il commento gemello in DrawBarChart): sale a
	// meta' di plotArea se la serie ha anche valori negativi, invece di
	// restare sempre sul fondo.
	float zeroY = ChartValueToY(0.0, minValue, maxValue, plotArea);
	view->StrokeLine(BPoint(plotArea.left, zeroY), BPoint(plotArea.right, zeroY));

	// Stessa larghezza di slot di ComputeLineLayout, per centrare
	// l'etichetta sotto il punto corrispondente e avvolgerla su piu'
	// righe se serve (vedi DrawWrappedLabel), invece di un unico
	// DrawString a sinistra che sconfinava fuori dallo slot.
	float slotWidth = plotArea.Width() / data.size();
	for (size_t i = 0; i < points.size() && i < data.size(); i++)
	{
		float centerX = plotArea.left + i * slotWidth + slotWidth / 2;
		DrawWrappedLabel(view, data[i].label.String(), BPoint(centerX, categoryLabelY),
			slotWidth - 2, kCategoryLabelMaxLines, true);
	}
}

void DrawAreaChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea = frame;
	plotArea.InsetBy(10, 10);
	plotArea.top += 14;
	if (!title.IsEmpty())
		plotArea.top += 18;

	double minValue, maxValue;
	ChartValueRange(data, &minValue, &maxValue);

	font_height fh;
	view->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;
	plotArea.bottom -= 12 + lineHeight * kCategoryLabelMaxLines + 4;
	float categoryLabelY = plotArea.bottom + 12;
	if (minValue < 0)
		plotArea.bottom -= 14;

	std::vector<AxisTick> ticks;
	ComputeYAxisTicks(minValue, maxValue, plotArea, ticks);
	float axisLabelWidth = 0;
	for (size_t i = 0; i < ticks.size(); i++)
	{
		float width = view->StringWidth(ticks[i].label.String());
		if (width > axisLabelWidth)
			axisLabelWidth = width;
	}
	plotArea.left += axisLabelWidth + 6;

	// Stessa geometria di DrawLineChart (ComputeLineLayout, riusata
	// senza modifiche): l'unica differenza reale di un grafico ad area
	// e' che la zona fra la spezzata e la linea dello zero viene
	// riempita di colore, non solo tracciata.
	std::vector<LinePoint> points;
	ComputeLineLayout(data, plotArea, points);

	DrawYAxisGrid(view, plotArea, minValue, maxValue);

	float zeroY = ChartValueToY(0.0, minValue, maxValue, plotArea);

	if (points.size() >= 2)
	{
		// Poligono chiuso: ogni punto della spezzata, poi lo stesso
		// percorso all'indietro lungo la linea dello zero -- un
		// trapezio/poligono irregolare che segue il profilo dei dati
		// sopra e sotto lo zero a seconda del segno di ogni valore.
		std::vector<BPoint> polygon;
		polygon.reserve(points.size() + 2);
		for (size_t i = 0; i < points.size(); i++)
			polygon.push_back(points[i].point);
		polygon.push_back(BPoint(points.back().point.x, zeroY));
		polygon.push_back(BPoint(points.front().point.x, zeroY));

		// B_OP_ALPHA/B_PIXEL_ALPHA (stesso principio della tinta di
		// selezione in SheetView::Draw): un riempimento pieno
		// nasconderebbe del tutto la griglia dell'asse Y appena
		// disegnata sotto.
		view->SetDrawingMode(B_OP_ALPHA);
		view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		view->SetHighColor(70, 110, 190, 90);
		view->FillPolygon(&polygon[0], (int32)polygon.size());
		view->SetDrawingMode(B_OP_COPY);
	}

	view->SetHighColor(70, 110, 190);
	for (size_t i = 1; i < points.size(); i++)
		view->StrokeLine(points[i - 1].point, points[i].point);
	for (size_t i = 0; i < points.size(); i++)
	{
		BRect dot(points[i].point.x - 3, points[i].point.y - 3,
			points[i].point.x + 3, points[i].point.y + 3);
		view->FillEllipse(dot);
	}

	view->SetHighColor(40, 40, 40);
	for (size_t i = 0; i < points.size() && i < data.size(); i++)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", data[i].value);
		float width = view->StringWidth(buf);
		float y = (data[i].value >= 0) ? points[i].point.y - 8 : points[i].point.y + 8;
		view->DrawString(buf, BPoint(points[i].point.x - width / 2, y));
	}

	view->SetHighColor(0, 0, 0);
	view->StrokeRect(frame);
	view->StrokeLine(BPoint(plotArea.left, zeroY), BPoint(plotArea.right, zeroY));

	float slotWidth = plotArea.Width() / data.size();
	for (size_t i = 0; i < points.size() && i < data.size(); i++)
	{
		float centerX = plotArea.left + i * slotWidth + slotWidth / 2;
		DrawWrappedLabel(view, data[i].label.String(), BPoint(centerX, categoryLabelY),
			slotWidth - 2, kCategoryLabelMaxLines, true);
	}
}

// Coordinata X di "value" dentro "bounds", scalata sull'intervallo
// [minValue, maxValue] -- minValue cade su bounds.left, maxValue su
// bounds.right. Stesso principio di ChartValueToY sopra, ma per l'asse
// orizzontale: nessun altro tipo di grafico in questo file ne ha
// bisogno (barre/linee/aree hanno sempre una categoria discreta sulle
// ascisse, mai un valore continuo), quindi resta locale a questo
// blocco invece di un parametro in piu' su ChartValueToY.
static float ChartValueToX(double value, double minValue, double maxValue, BRect bounds)
{
	return bounds.left + bounds.Width() * (float)((value - minValue) / (maxValue - minValue));
}

// Intervallo VERO di X e Y (a differenza di ChartValueRange sopra, qui
// NON si include mai lo zero forzatamente): un grafico a dispersione
// tipico ha valori lontani da zero su entrambi gli assi (es.
// temperatura vs pressione), includere lo zero sprecherebbe la
// maggior parte dell'area disegnabile comprimendo i punti veri in un
// angolo. Il chiamante deve garantire "data" non vuoto.
static void ScatterValueRange(const std::vector<ScatterPoint>& data,
	double* outMinX, double* outMaxX, double* outMinY, double* outMaxY)
{
	double minX = data[0].x, maxX = data[0].x;
	double minY = data[0].y, maxY = data[0].y;
	for (size_t i = 1; i < data.size(); i++)
	{
		if (data[i].x < minX) minX = data[i].x;
		if (data[i].x > maxX) maxX = data[i].x;
		if (data[i].y < minY) minY = data[i].y;
		if (data[i].y > maxY) maxY = data[i].y;
	}
	// Un solo punto, o piu' punti tutti allineati su una sola X/Y:
	// stesso principio di ChartValueRange sopra, allarga di 1 per non
	// dividere per zero.
	if (minX == maxX) { minX -= 1; maxX += 1; }
	if (minY == maxY) { minY -= 1; maxY += 1; }
	*outMinX = minX; *outMaxX = maxX; *outMinY = minY; *outMaxY = maxY;
}

void ComputeScatterLayout(const std::vector<ScatterPoint>& data, BRect bounds,
	std::vector<BPoint>& out)
{
	out.clear();
	if (data.empty())
		return;

	double minX, maxX, minY, maxY;
	ScatterValueRange(data, &minX, &maxX, &minY, &maxY);

	for (size_t i = 0; i < data.size(); i++)
	{
		float x = ChartValueToX(data[i].x, minX, maxX, bounds);
		float y = ChartValueToY(data[i].y, minY, maxY, bounds);
		out.push_back(BPoint(x, y));
	}
}

void DrawScatterChart(BView* view, BRect frame, const std::vector<ScatterPoint>& data,
	const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea = frame;
	plotArea.InsetBy(10, 10);
	if (!title.IsEmpty())
		plotArea.top += 18;

	double minX, maxX, minY, maxY;
	ScatterValueRange(data, &minX, &maxX, &minY, &maxY);

	// Spazio per le etichette numeriche di entrambi gli assi, calcolate
	// qui apposta invece che con ComputeYAxisTicks/DrawYAxisGrid
	// (condivisi con barre/linee/aree, pensati per una scala che
	// include sempre lo zero -- non adatta a un grafico a dispersione,
	// vedi il commento su ScatterValueRange sopra).
	font_height fh;
	view->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;
	plotArea.bottom -= lineHeight + 6;

	const int kDivisions = 4;
	float yLabelWidth = 0;
	for (int i = 0; i <= kDivisions; i++)
	{
		double value = minY + (maxY - minY) * i / kDivisions;
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", value);
		float width = view->StringWidth(buf);
		if (width > yLabelWidth)
			yLabelWidth = width;
	}
	plotArea.left += yLabelWidth + 6;

	// Griglia orizzontale (asse Y) + etichette a sinistra.
	view->SetHighColor(225, 225, 225);
	for (int i = 0; i <= kDivisions; i++)
	{
		double value = minY + (maxY - minY) * i / kDivisions;
		float y = ChartValueToY(value, minY, maxY, plotArea);
		view->StrokeLine(BPoint(plotArea.left, y), BPoint(plotArea.right, y));
	}
	view->SetHighColor(90, 90, 90);
	for (int i = 0; i <= kDivisions; i++)
	{
		double value = minY + (maxY - minY) * i / kDivisions;
		float y = ChartValueToY(value, minY, maxY, plotArea);
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", value);
		float width = view->StringWidth(buf);
		view->DrawString(buf, BPoint(plotArea.left - width - 6, y + 4));
	}

	// Griglia verticale (asse X) + etichette sotto.
	view->SetHighColor(225, 225, 225);
	for (int i = 0; i <= kDivisions; i++)
	{
		double value = minX + (maxX - minX) * i / kDivisions;
		float x = ChartValueToX(value, minX, maxX, plotArea);
		view->StrokeLine(BPoint(x, plotArea.top), BPoint(x, plotArea.bottom));
	}
	view->SetHighColor(90, 90, 90);
	for (int i = 0; i <= kDivisions; i++)
	{
		double value = minX + (maxX - minX) * i / kDivisions;
		float x = ChartValueToX(value, minX, maxX, plotArea);
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", value);
		float width = view->StringWidth(buf);
		view->DrawString(buf, BPoint(x - width / 2, plotArea.bottom + lineHeight));
	}

	view->SetHighColor(0, 0, 0);
	view->StrokeRect(plotArea);

	// Soli punti, MAI una linea di collegamento -- il vero grafico
	// "Dispersione" di Excel (non "Dispersione con linee dritte",
	// variante rara nell'uso reale, non implementata).
	std::vector<BPoint> points;
	ComputeScatterLayout(data, plotArea, points);
	view->SetHighColor(70, 110, 190);
	for (size_t i = 0; i < points.size(); i++)
	{
		BRect dot(points[i].x - 3, points[i].y - 3, points[i].x + 3, points[i].y + 3);
		view->FillEllipse(dot);
	}

	view->SetHighColor(0, 0, 0);
	view->StrokeRect(frame);
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

void DrawPieChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	std::vector<PieSlice> slices;
	ComputePieLayout(data, slices);

	if (slices.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun valore positivo da mostrare."), frame.LeftTop() + BPoint(10, 20));
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
	if (!title.IsEmpty())
		pieArea.top += 18;	// spazio per il titolo, vedi DrawChartTitle
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

	// Larghezza di testo disponibile nella striscia di legenda
	// (legendWidth sopra, 110px): meno il quadratino di colore/il suo
	// scarto (16px) e un margine destro (8px) prima del bordo del
	// frame -- un'etichetta lunga (es. "United States of America (25%)")
	// vi va ora a capo su piu' righe (DrawWrappedLabel) invece di
	// sconfinare fuori dal grafico (bug segnalato dall'utente).
	float legendTextWidth = legendWidth - 16 - 8;
	float legendX = pieArea.right + 16;
	float legendY = pieArea.top + 4;
	for (size_t i = 0; i < data.size() && i < slices.size(); i++)
	{
		BRect swatch(legendX, legendY - 8, legendX + 10, legendY + 2);
		view->SetHighColor(kPieColors[i % kPieColorCount]);
		view->FillRect(swatch);
		view->SetHighColor(0, 0, 0);

		// Percentuale sul totale, ricavata dalla stessa ampiezza
		// d'angolo gia' calcolata da ComputePieLayout (sweepAngle e'
		// proporzionale al peso del valore, vedi il commento li').
		// Parte della STESSA stringa avvolta sotto, non disegnata a
		// parte: il testo a capo la sposta naturalmente su qualunque
		// riga ci sia spazio, senza bisogno di un caso speciale.
		BString entry = data[i].label;
		char pct[16];
		snprintf(pct, sizeof(pct), " (%.0f%%)", slices[i].sweepAngle / 360.0 * 100.0);
		entry << pct;

		float used = DrawWrappedLabel(view, entry.String(), BPoint(legendX + 16, legendY),
			legendTextWidth, kLegendLabelMaxLines, false);
		legendY += std::max(16.0f, used + 4);
	}
}

bool BuildMultiChartSeries(CContainer* doc, const range& r, MultiChartData& out)
{
	out.categories.clear();
	out.seriesNames.clear();
	out.values.clear();
	if (!doc || r.right - r.left < 1)
		return false;

	int seriesCount = r.right - r.left;
	out.values.resize(seriesCount);

	// Riga di intestazione (Fase 18): se la prima riga dell'intervallo
	// ha un valore testuale in ALMENO una colonna serie, la si assume
	// una riga di intestazione con i nomi delle serie -- stessa
	// convenzione di Excel, selezionare A1:C5 con "Vendite"/"Costi" in
	// B1/C1 nomina le serie senza bisogno di un controllo dedicato.
	// Senza questo controllo quella riga verrebbe comunque scartata
	// come dato (un valore testuale in una colonna serie la rende
	// "rowOk = false" piu' sotto), quindi leggerla come intestazione
	// invece di limitarsi a saltarla e' un puro miglioramento, non un
	// cambio di comportamento per un intervallo senza intestazione.
	bool hasHeader = false;
	for (int s = 0; s < seriesCount; s++)
	{
		cell headerCell(r.left + 1 + s, r.top);
		Value hv;
		doc->GetValue(headerCell, hv);
		if (hv.fType == eTextData && BString((const char*)hv).Length() > 0)
		{
			hasHeader = true;
			break;
		}
	}

	for (int s = 0; s < seriesCount; s++)
	{
		BString name;
		if (hasHeader)
		{
			cell headerCell(r.left + 1 + s, r.top);
			Value hv;
			doc->GetValue(headerCell, hv);
			if (hv.fType == eTextData)
				name = (const char*)hv;
		}
		// Una colonna senza intestazione testuale propria (vuota o
		// numerica) resta "Serie N", anche se le altre colonne ne
		// hanno una -- non tutte le serie devono avere per forza un
		// nome esplicito.
		if (name.IsEmpty())
		{
			name = B_TRANSLATE("Serie");
			name << " " << (int32)(s + 1);
		}
		out.seriesNames.push_back(name);
	}

	int firstDataRow = hasHeader ? r.top + 1 : r.top;
	for (int row = firstDataRow; row <= r.bottom; row++)
	{
		// Una riga con un valore non numerico in QUALSIASI colonna
		// serie viene saltata per intero (non solo per quella serie):
		// le serie condividono lo stesso elenco di categorie, quindi
		// un valore mancante in una sola serie romperebbe
		// l'allineamento values[s][c] <-> categories[c] per tutte le
		// altre se la riga venisse tenuta con un buco.
		std::vector<double> rowValues(seriesCount);
		bool rowOk = true;
		for (int s = 0; s < seriesCount; s++)
		{
			cell valueCell(r.left + 1 + s, row);
			Value vv;
			doc->GetValue(valueCell, vv);
			if (vv.fType != eNumData)
			{
				rowOk = false;
				break;
			}
			rowValues[s] = (double)vv;
		}
		if (!rowOk)
			continue;

		cell labelCell(r.left, row);
		Value lv;
		doc->GetValue(labelCell, lv);
		BString label;
		ValueToLabel(lv, label);

		out.categories.push_back(label);
		for (int s = 0; s < seriesCount; s++)
			out.values[s].push_back(rowValues[s]);
	}

	return !out.categories.empty();
}

// Intervallo di valori su TUTTE le serie insieme (non una per serie):
// le barre/linee di serie diverse devono restare sullo stesso asse per
// essere confrontabili, stesso principio di ChartValueRange ma esteso
// a piu' vettori di valori.
static void MultiChartValueRange(const MultiChartData& data, double* outMin, double* outMax)
{
	double minValue = 0, maxValue = 0;
	for (size_t s = 0; s < data.values.size(); s++)
	{
		for (size_t c = 0; c < data.values[s].size(); c++)
		{
			if (data.values[s][c] < minValue)
				minValue = data.values[s][c];
			if (data.values[s][c] > maxValue)
				maxValue = data.values[s][c];
		}
	}
	if (minValue == maxValue)
		maxValue = minValue + 1;
	*outMin = minValue;
	*outMax = maxValue;
}

void ComputeGroupedBarLayout(const MultiChartData& data, BRect bounds, GroupedBarLayout& out)
{
	out.bars.clear();
	size_t seriesCount = data.seriesNames.size();
	size_t catCount = data.categories.size();
	if (seriesCount == 0 || catCount == 0)
		return;

	double minValue, maxValue;
	MultiChartValueRange(data, &minValue, &maxValue);
	float zeroY = ChartValueToY(0.0, minValue, maxValue, bounds);

	float slotWidth = bounds.Width() / catCount;
	float slotGap = slotWidth * 0.15f;
	if (slotGap > 10)
		slotGap = 10;
	float groupWidth = slotWidth - slotGap;
	float barWidth = groupWidth / seriesCount;

	out.bars.resize(seriesCount);
	for (size_t s = 0; s < seriesCount; s++)
	{
		out.bars[s].resize(catCount);
		for (size_t c = 0; c < catCount; c++)
		{
			float groupLeft = bounds.left + c * slotWidth + slotGap / 2;
			float left = groupLeft + s * barWidth;
			float right = left + barWidth;
			float valueY = ChartValueToY(data.values[s][c], minValue, maxValue, bounds);
			out.bars[s][c].Set(left, std::min(valueY, zeroY), right, std::max(valueY, zeroY));
		}
	}
}

void ComputeMultiLineLayout(const MultiChartData& data, BRect bounds, MultiLinePoint& out)
{
	out.points.clear();
	size_t seriesCount = data.seriesNames.size();
	size_t catCount = data.categories.size();
	if (seriesCount == 0 || catCount == 0)
		return;

	double minValue, maxValue;
	MultiChartValueRange(data, &minValue, &maxValue);
	float slotWidth = bounds.Width() / catCount;

	out.points.resize(seriesCount);
	for (size_t s = 0; s < seriesCount; s++)
	{
		out.points[s].resize(catCount);
		for (size_t c = 0; c < catCount; c++)
		{
			float x = bounds.left + c * slotWidth + slotWidth / 2;
			float y = ChartValueToY(data.values[s][c], minValue, maxValue, bounds);
			out.points[s][c].Set(x, y);
		}
	}
}

// Riserva il margine sinistro (etichette asse Y) e la striscia destra
// (legenda per serie) di plotArea -- condivisa da DrawGroupedBarChart/
// DrawMultiLineChart, che hanno la stessa struttura (asse Y comune +
// legenda) a differenza delle loro controparti a singola serie (che
// non hanno bisogno di una legenda, una sola serie non ha nulla da
// distinguere).
// Vera per l'indice "s" se quella serie deve mostrare l'etichetta del
// valore numerico -- showValues VUOTO (il caso di un grafico
// incorporato nel foglio, che non passa mai da ChartWindow/le sue
// checkbox) o un indice fuori dai limiti significano "visibile",
// stesso principio permissivo del resto dell'app (vedi il commento su
// MultiChartData::showValues in Chart.h).
static bool SeriesShowsValues(const MultiChartData& data, size_t s)
{
	return s >= data.showValues.size() || data.showValues[s];
}

static void PrepareMultiSeriesPlotArea(BView* view, BRect frame, const MultiChartData& data,
	const BString& title, BRect* outPlotArea, double* outMinValue, double* outMaxValue,
	float* outCategoryLabelY)
{
	float legendWidth = 110;
	BRect plotArea = frame;
	plotArea.InsetBy(10, 10);
	plotArea.top += 14;	// spazio per l'etichetta del valore sopra barre/punti (Fase 19, opzionale per serie)
	if (!title.IsEmpty())
		plotArea.top += 18;
	plotArea.right -= legendWidth;

	double minValue, maxValue;
	MultiChartValueRange(data, &minValue, &maxValue);

	// Spazio per le etichette di categoria sotto, fino a
	// kCategoryLabelMaxLines righe -- vedi il commento gemello in
	// DrawBarChart.
	font_height fh;
	view->GetFontHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;
	plotArea.bottom -= 12 + lineHeight * kCategoryLabelMaxLines + 4;
	// La riga di categoria resta fissa a questa posizione (calcolata
	// PRIMA di ogni restrizione aggiuntiva sotto), stessa correzione
	// di DrawBarChart/DrawLineChart -- vedi il commento gemello li'
	// per il bug che questo evita (etichetta valore negativo
	// sovrapposta alla riga di categoria).
	float categoryLabelY = plotArea.bottom + 12;
	if (minValue < 0)
		plotArea.bottom -= 14;

	std::vector<AxisTick> ticks;
	ComputeYAxisTicks(minValue, maxValue, plotArea, ticks);
	float axisLabelWidth = 0;
	for (size_t i = 0; i < ticks.size(); i++)
	{
		float width = view->StringWidth(ticks[i].label.String());
		if (width > axisLabelWidth)
			axisLabelWidth = width;
	}
	plotArea.left += axisLabelWidth + 6;

	*outPlotArea = plotArea;
	*outMinValue = minValue;
	*outMaxValue = maxValue;
	*outCategoryLabelY = categoryLabelY;
}

// Bordo, linea di zero, etichette di categoria e legenda per serie --
// condivisa da DrawGroupedBarChart/DrawMultiLineChart (stesso schema
// del commento gemello su PrepareMultiSeriesPlotArea sopra).
static void DrawMultiSeriesFooter(BView* view, BRect frame, BRect plotArea,
	const MultiChartData& data, double minValue, double maxValue, float categoryLabelY)
{
	view->SetHighColor(0, 0, 0);
	view->StrokeRect(frame);
	float zeroY = ChartValueToY(0.0, minValue, maxValue, plotArea);
	view->StrokeLine(BPoint(plotArea.left, zeroY), BPoint(plotArea.right, zeroY));

	// Etichetta centrata sotto lo slot e avvolta su piu' righe se serve
	// (vedi DrawWrappedLabel), invece di un unico DrawString a sinistra
	// che sconfinava fuori dallo slot con un nome di categoria lungo.
	float slotWidth = plotArea.Width() / data.categories.size();
	for (size_t c = 0; c < data.categories.size(); c++)
	{
		float centerX = plotArea.left + c * slotWidth + slotWidth / 2;
		DrawWrappedLabel(view, data.categories[c].String(), BPoint(centerX, categoryLabelY),
			slotWidth - 2, kCategoryLabelMaxLines, true);
	}

	// Striscia di legenda: stesso "legendWidth" di
	// PrepareMultiSeriesPlotArea (110px), meno il quadratino di colore
	// e un margine -- vedi il commento su legendTextWidth in
	// DrawPieChart per lo stesso identico calcolo. legendY avanza
	// dell'altezza VERA occupata (il valore di ritorno di
	// DrawWrappedLabel), non piu' un passo fisso: una voce che va a
	// capo non si sovrappone piu' alla successiva.
	float legendX = plotArea.right + 16;
	float legendY = plotArea.top + 4;
	float legendTextWidth = 110 - 16 - 8;
	for (size_t s = 0; s < data.seriesNames.size(); s++)
	{
		BRect swatch(legendX, legendY - 8, legendX + 10, legendY + 2);
		view->SetHighColor(kPieColors[s % kPieColorCount]);
		view->FillRect(swatch);
		view->SetHighColor(0, 0, 0);
		float used = DrawWrappedLabel(view, data.seriesNames[s].String(), BPoint(legendX + 16, legendY),
			legendTextWidth, kLegendLabelMaxLines, false);
		legendY += std::max(16.0f, used + 4);
	}
}

void DrawGroupedBarChart(BView* view, BRect frame, const MultiChartData& data, const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.categories.empty() || data.seriesNames.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea;
	double minValue, maxValue;
	float categoryLabelY;
	PrepareMultiSeriesPlotArea(view, frame, data, title, &plotArea, &minValue, &maxValue, &categoryLabelY);

	GroupedBarLayout layout;
	ComputeGroupedBarLayout(data, plotArea, layout);

	DrawYAxisGrid(view, plotArea, minValue, maxValue);

	for (size_t s = 0; s < layout.bars.size(); s++)
	{
		view->SetHighColor(kPieColors[s % kPieColorCount]);
		for (size_t c = 0; c < layout.bars[s].size(); c++)
			view->FillRect(layout.bars[s][c]);
	}

	// Valore numerico sopra/sotto ogni barra, solo per le serie con la
	// visibilita' attivata (checkbox in ChartWindow) -- con piu' serie
	// affiancate i valori di TUTTE le barre affollerebbero il
	// grafico, per questo restano opzionali per serie (a differenza
	// del grafico a singola serie, dove sono sempre visibili). Colore
	// dell'etichetta uguale a quello della barra/serie, per restare
	// leggibile a colpo d'occhio quale valore appartiene a quale
	// serie.
	for (size_t s = 0; s < layout.bars.size(); s++)
	{
		if (!SeriesShowsValues(data, s))
			continue;
		view->SetHighColor(kPieColors[s % kPieColorCount]);
		for (size_t c = 0; c < layout.bars[s].size(); c++)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%g", data.values[s][c]);
			float width = view->StringWidth(buf);
			BRect bar = layout.bars[s][c];
			float x = bar.left + bar.Width() / 2 - width / 2;
			float y = (data.values[s][c] >= 0) ? bar.top - 4 : bar.bottom + 4;
			view->DrawString(buf, BPoint(x, y));
		}
	}

	DrawMultiSeriesFooter(view, frame, plotArea, data, minValue, maxValue, categoryLabelY);
}

void DrawMultiLineChart(BView* view, BRect frame, const MultiChartData& data, const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.categories.empty() || data.seriesNames.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea;
	double minValue, maxValue;
	float categoryLabelY;
	PrepareMultiSeriesPlotArea(view, frame, data, title, &plotArea, &minValue, &maxValue, &categoryLabelY);

	MultiLinePoint layout;
	ComputeMultiLineLayout(data, plotArea, layout);

	DrawYAxisGrid(view, plotArea, minValue, maxValue);

	for (size_t s = 0; s < layout.points.size(); s++)
	{
		view->SetHighColor(kPieColors[s % kPieColorCount]);
		for (size_t c = 1; c < layout.points[s].size(); c++)
			view->StrokeLine(layout.points[s][c - 1], layout.points[s][c]);
		for (size_t c = 0; c < layout.points[s].size(); c++)
		{
			BPoint p = layout.points[s][c];
			view->FillEllipse(BRect(p.x - 3, p.y - 3, p.x + 3, p.y + 3));
		}
	}

	// Valore numerico accanto a ogni punto, solo per le serie con la
	// visibilita' attivata -- stesso principio di DrawGroupedBarChart
	// sopra.
	for (size_t s = 0; s < layout.points.size(); s++)
	{
		if (!SeriesShowsValues(data, s))
			continue;
		view->SetHighColor(kPieColors[s % kPieColorCount]);
		for (size_t c = 0; c < layout.points[s].size(); c++)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%g", data.values[s][c]);
			float width = view->StringWidth(buf);
			BPoint p = layout.points[s][c];
			float y = (data.values[s][c] >= 0) ? p.y - 8 : p.y + 8;
			view->DrawString(buf, BPoint(p.x - width / 2, y));
		}
	}

	DrawMultiSeriesFooter(view, frame, plotArea, data, minValue, maxValue, categoryLabelY);
}

void DrawMultiAreaChart(BView* view, BRect frame, const MultiChartData& data, const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.categories.empty() || data.seriesNames.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea;
	double minValue, maxValue;
	float categoryLabelY;
	PrepareMultiSeriesPlotArea(view, frame, data, title, &plotArea, &minValue, &maxValue, &categoryLabelY);

	MultiLinePoint layout;
	ComputeMultiLineLayout(data, plotArea, layout);

	DrawYAxisGrid(view, plotArea, minValue, maxValue);

	float zeroY = ChartValueToY(0.0, minValue, maxValue, plotArea);

	// Ogni serie riempita verso lo zero PRIMA di disegnarne la linea/i
	// pallini (stesso ordine di DrawAreaChart sopra): una serie
	// disegnata dopo copre visivamente quella precedente dove si
	// sovrappongono, esattamente come il grafico ad area "normale" (non
	// impilato) di Excel -- niente somma fra serie, ognuna resta la
	// propria altezza vera.
	for (size_t s = 0; s < layout.points.size(); s++)
	{
		if (layout.points[s].size() < 2)
			continue;
		std::vector<BPoint> polygon;
		polygon.reserve(layout.points[s].size() + 2);
		for (size_t c = 0; c < layout.points[s].size(); c++)
			polygon.push_back(layout.points[s][c]);
		polygon.push_back(BPoint(layout.points[s].back().x, zeroY));
		polygon.push_back(BPoint(layout.points[s].front().x, zeroY));

		view->SetDrawingMode(B_OP_ALPHA);
		view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		rgb_color fill = kPieColors[s % kPieColorCount];
		fill.alpha = 90;
		view->SetHighColor(fill);
		view->FillPolygon(&polygon[0], (int32)polygon.size());
		view->SetDrawingMode(B_OP_COPY);
	}

	for (size_t s = 0; s < layout.points.size(); s++)
	{
		view->SetHighColor(kPieColors[s % kPieColorCount]);
		for (size_t c = 1; c < layout.points[s].size(); c++)
			view->StrokeLine(layout.points[s][c - 1], layout.points[s][c]);
		for (size_t c = 0; c < layout.points[s].size(); c++)
		{
			BPoint p = layout.points[s][c];
			view->FillEllipse(BRect(p.x - 3, p.y - 3, p.x + 3, p.y + 3));
		}
	}

	for (size_t s = 0; s < layout.points.size(); s++)
	{
		if (!SeriesShowsValues(data, s))
			continue;
		view->SetHighColor(kPieColors[s % kPieColorCount]);
		for (size_t c = 0; c < layout.points[s].size(); c++)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%g", data.values[s][c]);
			float width = view->StringWidth(buf);
			BPoint p = layout.points[s][c];
			float y = (data.values[s][c] >= 0) ? p.y - 8 : p.y + 8;
			view->DrawString(buf, BPoint(p.x - width / 2, y));
		}
	}

	DrawMultiSeriesFooter(view, frame, plotArea, data, minValue, maxValue, categoryLabelY);
}

void ComputeComboLayout(const MultiChartData& data, BRect bounds, ComboLayout& out)
{
	out.bars.clear();
	out.lines.clear();
	size_t seriesCount = data.seriesNames.size();
	size_t catCount = data.categories.size();
	if (seriesCount == 0 || catCount == 0)
		return;

	double minValue, maxValue;
	MultiChartValueRange(data, &minValue, &maxValue);
	float zeroY = ChartValueToY(0.0, minValue, maxValue, bounds);
	float slotWidth = bounds.Width() / catCount;
	// Barra piu' stretta dello slot (60%), stesso principio di
	// ComputeBarLayout a singola serie -- qui non c'e' bisogno di
	// dividere lo slot per serie come ComputeGroupedBarLayout, la barra
	// e' sempre una sola per categoria (solo la serie 0).
	float barWidth = slotWidth * 0.6f;

	out.bars.resize(catCount);
	for (size_t c = 0; c < catCount; c++)
	{
		float centerX = bounds.left + c * slotWidth + slotWidth / 2;
		float valueY = ChartValueToY(data.values[0][c], minValue, maxValue, bounds);
		out.bars[c].Set(centerX - barWidth / 2, std::min(valueY, zeroY),
			centerX + barWidth / 2, std::max(valueY, zeroY));
	}

	if (seriesCount <= 1)
		return;

	// Le linee condividono la STESSA X di centro-slot delle barre
	// (identica formula di ComputeMultiLineLayout), cosi' un punto e la
	// barra della stessa categoria restano allineati verticalmente.
	out.lines.resize(seriesCount - 1);
	for (size_t s = 1; s < seriesCount; s++)
	{
		out.lines[s - 1].resize(catCount);
		for (size_t c = 0; c < catCount; c++)
		{
			float x = bounds.left + c * slotWidth + slotWidth / 2;
			float y = ChartValueToY(data.values[s][c], minValue, maxValue, bounds);
			out.lines[s - 1][c].Set(x, y);
		}
	}
}

void DrawComboChart(BView* view, BRect frame, const MultiChartData& data, const BString& title)
{
	view->SetHighColor(255, 255, 255);
	view->FillRect(frame);
	DrawChartTitle(view, frame, title);

	if (data.categories.empty() || data.seriesNames.empty())
	{
		view->SetHighColor(120, 120, 120);
		view->DrawString(B_TRANSLATE("Nessun dato da mostrare."), frame.LeftTop() + BPoint(10, 20));
		return;
	}

	BRect plotArea;
	double minValue, maxValue;
	float categoryLabelY;
	PrepareMultiSeriesPlotArea(view, frame, data, title, &plotArea, &minValue, &maxValue, &categoryLabelY);

	ComboLayout layout;
	ComputeComboLayout(data, plotArea, layout);

	DrawYAxisGrid(view, plotArea, minValue, maxValue);

	// Serie 0: barre. Stesso colore (kPieColors[0]) che DrawMultiSeriesFooter
	// assegna alla legenda della serie 0, cosi' barra e legenda combaciano.
	view->SetHighColor(kPieColors[0]);
	for (size_t c = 0; c < layout.bars.size(); c++)
		view->FillRect(layout.bars[c]);

	if (SeriesShowsValues(data, 0))
	{
		view->SetHighColor(kPieColors[0]);
		for (size_t c = 0; c < layout.bars.size(); c++)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%g", data.values[0][c]);
			float width = view->StringWidth(buf);
			BRect bar = layout.bars[c];
			float x = bar.left + bar.Width() / 2 - width / 2;
			float y = (data.values[0][c] >= 0) ? bar.top - 4 : bar.bottom + 4;
			view->DrawString(buf, BPoint(x, y));
		}
	}

	// Serie 1..N-1: linee. layout.lines[s] corrisponde alla serie reale
	// (s + 1), quindi il colore/indice showValues usano sempre "s + 1"
	// per restare allineati alla stessa numerazione della legenda.
	for (size_t s = 0; s < layout.lines.size(); s++)
	{
		view->SetHighColor(kPieColors[(s + 1) % kPieColorCount]);
		for (size_t c = 1; c < layout.lines[s].size(); c++)
			view->StrokeLine(layout.lines[s][c - 1], layout.lines[s][c]);
		for (size_t c = 0; c < layout.lines[s].size(); c++)
		{
			BPoint p = layout.lines[s][c];
			view->FillEllipse(BRect(p.x - 3, p.y - 3, p.x + 3, p.y + 3));
		}
	}

	for (size_t s = 0; s < layout.lines.size(); s++)
	{
		if (!SeriesShowsValues(data, s + 1))
			continue;
		view->SetHighColor(kPieColors[(s + 1) % kPieColorCount]);
		for (size_t c = 0; c < layout.lines[s].size(); c++)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%g", data.values[s + 1][c]);
			float width = view->StringWidth(buf);
			BPoint p = layout.lines[s][c];
			float y = (data.values[s + 1][c] >= 0) ? p.y - 8 : p.y + 8;
			view->DrawString(buf, BPoint(p.x - width / 2, y));
		}
	}

	DrawMultiSeriesFooter(view, frame, plotArea, data, minValue, maxValue, categoryLabelY);
}

void DrawChart(BView* view, BRect frame, const std::vector<ChartSeries>& data,
	ChartType type, const BString& title)
{
	switch (type)
	{
		case eAreaChart:
			DrawAreaChart(view, frame, data, title);
			return;
		case eLineChart:
			DrawLineChart(view, frame, data, title);
			return;
		case ePieChart:
			DrawPieChart(view, frame, data, title);
			return;
		case eBarChart:
		default:
			DrawBarChart(view, frame, data, title);
			return;
	}
}
