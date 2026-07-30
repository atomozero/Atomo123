/*
	Pivot.cpp

	Vedi Pivot.h.
*/

#include "Pivot.h"

#include <algorithm>
#include <map>

#include "Cell.h"
#include "Container.h"
#include "Range.h"
#include "Value.h"

static bool RowLess(const PivotRow& a, const PivotRow& b)
{
	return a.category < b.category;
}

bool BuildPivotTable(CContainer* doc, const range& source,
	std::vector<PivotRow>& out)
{
	out.clear();
	if (!doc || source.right - source.left != 1)
		return false;

	// aggregate accumula la somma, count il numero di righe valide per
	// ogni categoria -- WritePivotTable sceglie poi quale mostrare
	// (o ne fa la media) in base all'aggregazione scelta dall'utente.
	std::map<BString, PivotRow> groups;

	for (int row = source.top; row <= source.bottom; row++)
	{
		cell categoryCell(source.left, row);
		cell valueCell(source.left + 1, row);

		Value cv, vv;
		doc->GetValue(categoryCell, cv);
		doc->GetValue(valueCell, vv);

		if (cv.fType != eTextData || vv.fType != eNumData)
			continue;

		BString key((const char*)cv);
		std::map<BString, PivotRow>::iterator it = groups.find(key);
		if (it == groups.end())
		{
			PivotRow r;
			r.category = key;
			r.aggregate = 0;
			r.count = 0;
			it = groups.insert(std::make_pair(key, r)).first;
		}
		it->second.aggregate += (double)vv;
		it->second.count++;
	}

	for (std::map<BString, PivotRow>::iterator it = groups.begin();
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
			return "Conteggio";
		case ePivotAverage:
			return "Media";
		default:
			return "Somma";
	}
}

void WritePivotTable(CContainer* doc, const cell& dest,
	const std::vector<PivotRow>& rows, PivotAggFunc fn)
{
	if (!doc)
		return;

	cell headerCat(dest.h, dest.v);
	cell headerVal(dest.h + 1, dest.v);
	doc->NewCell(headerCat, Value("Categoria"), NULL);
	doc->NewCell(headerVal, Value(AggLabel(fn)), NULL);

	for (size_t i = 0; i < rows.size(); i++)
	{
		double shown = rows[i].aggregate;
		if (fn == ePivotCount)
			shown = rows[i].count;
		else if (fn == ePivotAverage && rows[i].count > 0)
			shown = rows[i].aggregate / rows[i].count;

		cell catCell(dest.h, dest.v + 1 + i);
		cell valCell(dest.h + 1, dest.v + 1 + i);
		doc->NewCell(catCell, Value(rows[i].category.String()), NULL);
		doc->NewCell(valCell, Value(shown), NULL);
	}
}
