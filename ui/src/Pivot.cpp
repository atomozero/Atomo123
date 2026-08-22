/*
	Pivot.cpp

	Vedi Pivot.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "Pivot.h"

#include <algorithm>
#include <map>

#include <Catalog.h>

#include "Cell.h"
#include "Container.h"
#include "Range.h"
#include "Value.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Pivot"

static bool RowLess(const PivotRow& a, const PivotRow& b)
{
	// Confronto lessicografico sull'intero vettore (std::vector<BString>
	// eredita operator< da BString elemento per elemento): un
	// raggruppamento a due livelli ordina prima per il primo livello,
	// poi per il secondo a parita' del primo -- lo stesso ordine che
	// Excel userebbe per righe annidate.
	return a.categories < b.categories;
}

bool BuildPivotTable(CContainer* doc, const range& source,
	std::vector<PivotRow>& out)
{
	out.clear();
	// Almeno due colonne: una di valore (l'ultima) piu' almeno una di
	// categoria. source.right - source.left e' il numero di colonne
	// meno 1, quindi >= 1 significa "almeno due colonne".
	if (!doc || source.right - source.left < 1)
		return false;

	int valueCol = source.right;

	// aggregate accumula la somma, count il numero di righe valide per
	// ogni combinazione di categorie -- WritePivotTable sceglie poi
	// quale mostrare (o ne fa la media/minimo/massimo) in base
	// all'aggregazione scelta dall'utente.
	std::map<std::vector<BString>, PivotRow> groups;

	for (int row = source.top; row <= source.bottom; row++)
	{
		std::vector<BString> keys;
		bool validKeys = true;
		for (int col = source.left; col < valueCol; col++)
		{
			Value cv;
			doc->GetValue(cell(col, row), cv);
			if (cv.fType != eTextData)
			{
				validKeys = false;
				break;
			}
			keys.push_back(BString((const char*)cv));
		}
		if (!validKeys)
			continue;

		Value vv;
		doc->GetValue(cell(valueCol, row), vv);
		if (vv.fType != eNumData)
			continue;

		std::map<std::vector<BString>, PivotRow>::iterator it = groups.find(keys);
		if (it == groups.end())
		{
			PivotRow r;
			r.categories = keys;
			r.aggregate = 0;
			r.count = 0;
			r.minVal = 0;
			r.maxVal = 0;
			it = groups.insert(std::make_pair(keys, r)).first;
		}

		double v = (double)vv;
		if (it->second.count == 0)
		{
			it->second.minVal = v;
			it->second.maxVal = v;
		}
		else
		{
			if (v < it->second.minVal)
				it->second.minVal = v;
			if (v > it->second.maxVal)
				it->second.maxVal = v;
		}
		it->second.aggregate += v;
		it->second.count++;
	}

	for (std::map<std::vector<BString>, PivotRow>::iterator it = groups.begin();
			it != groups.end(); ++it)
		out.push_back(it->second);

	std::sort(out.begin(), out.end(), RowLess);
	return !out.empty();
}

static const char* AggLabel(PivotAggFunc fn)
{
	switch (fn)
	{
		case ePivotCount:
			return B_TRANSLATE("Conteggio");
		case ePivotAverage:
			return B_TRANSLATE("Media");
		case ePivotMin:
			return B_TRANSLATE("Minimo");
		case ePivotMax:
			return B_TRANSLATE("Massimo");
		default:
			return B_TRANSLATE("Somma");
	}
}

void WritePivotTable(CContainer* doc, const cell& dest,
	const std::vector<PivotRow>& rows, PivotAggFunc fn)
{
	if (!doc || rows.empty())
		return;

	// Tutte le righe hanno lo stesso numero di livelli (lo stesso
	// intervallo sorgente per costruzione), quindi basta guardare la
	// prima per sapere quante colonne di intestazione servono.
	int numKeyCols = (int)rows[0].categories.size();

	for (int k = 0; k < numKeyCols; k++)
	{
		cell headerCell(dest.h + k, dest.v);
		BString label(B_TRANSLATE("Categoria"));
		// Un solo livello (il caso comune, invariato dalla versione
		// precedente): resta "Categoria" senza numero, per non
		// cambiare l'intestazione di ogni pivot gia' esistente a un
		// solo livello.
		if (numKeyCols > 1)
			label << " " << (k + 1);
		doc->NewCell(headerCell, Value(label.String()), NULL);
	}

	cell headerVal(dest.h + numKeyCols, dest.v);
	doc->NewCell(headerVal, Value(AggLabel(fn)), NULL);

	for (size_t i = 0; i < rows.size(); i++)
	{
		double shown = rows[i].aggregate;
		if (fn == ePivotCount)
			shown = rows[i].count;
		else if (fn == ePivotAverage && rows[i].count > 0)
			shown = rows[i].aggregate / rows[i].count;
		else if (fn == ePivotMin)
			shown = rows[i].minVal;
		else if (fn == ePivotMax)
			shown = rows[i].maxVal;

		for (int k = 0; k < numKeyCols; k++)
		{
			cell catCell(dest.h + k, dest.v + 1 + i);
			doc->NewCell(catCell, Value(rows[i].categories[k].String()), NULL);
		}
		cell valCell(dest.h + numKeyCols, dest.v + 1 + i);
		doc->NewCell(valCell, Value(shown), NULL);
	}
}
