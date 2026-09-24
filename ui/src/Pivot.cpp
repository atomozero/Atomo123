/*
	Pivot.cpp

	Vedi Pivot.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "Pivot.h"

#include <algorithm>
#include <map>
#include <set>

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

static bool RowLess2D(const PivotRow2D& a, const PivotRow2D& b)
{
	return a.categories < b.categories;
}

bool BuildPivotTable2D(CContainer* doc, const range& source, int columnFieldCol,
	const std::vector<PivotMeasure>& measures,
	std::vector<BString>* outColumnValues, std::vector<PivotRow2D>* outRows)
{
	outColumnValues->clear();
	outRows->clear();
	if (!doc || measures.empty())
		return false;

	// Colonne chiave di riga: ogni colonna di "source" tranne
	// columnFieldCol e tranne ogni measures[i].sourceCol -- insieme
	// escluso esplicito, il resto implicito, stesso principio del
	// vettore ChartObject::valueColumns in Chart.h.
	std::vector<int> rowKeyCols;
	for (int col = source.left; col <= source.right; col++)
	{
		if (col == columnFieldCol)
			continue;
		bool isMeasureCol = false;
		for (size_t m = 0; m < measures.size(); m++)
		{
			if (measures[m].sourceCol == col)
			{
				isMeasureCol = true;
				break;
			}
		}
		if (isMeasureCol)
			continue;
		rowKeyCols.push_back(col);
	}
	if (rowKeyCols.empty())
		return false;

	// Prima passata: valori distinti del campo Colonne (se presente),
	// ordinati lessicograficamente -- std::set<BString> li ordina da
	// solo (BString ha operator< gia' usato altrove in questo file).
	std::map<BString, int> columnValueIndex;
	if (columnFieldCol >= 0)
	{
		std::set<BString> distinctVals;
		for (int row = source.top; row <= source.bottom; row++)
		{
			Value cv;
			doc->GetValue(cell(columnFieldCol, row), cv);
			if (cv.fType != eTextData)
				continue;
			distinctVals.insert(BString((const char*)cv));
		}
		for (std::set<BString>::iterator it = distinctVals.begin(); it != distinctVals.end(); ++it)
		{
			columnValueIndex[*it] = (int)outColumnValues->size();
			outColumnValues->push_back(*it);
		}
		if (outColumnValues->empty())
			return false; // campo Colonne scelto ma nessun valore testuale trovato
	}
	int numColSlots = columnFieldCol >= 0 ? (int)outColumnValues->size() : 1;

	// Seconda passata: raggruppamento per chiave di riga, aggregazione
	// per (valore di colonna, misura) dentro ogni gruppo.
	std::map<std::vector<BString>, PivotRow2D> groups;
	for (int row = source.top; row <= source.bottom; row++)
	{
		std::vector<BString> rowKey;
		bool validRowKey = true;
		for (size_t k = 0; k < rowKeyCols.size(); k++)
		{
			Value cv;
			doc->GetValue(cell(rowKeyCols[k], row), cv);
			if (cv.fType != eTextData)
			{
				validRowKey = false;
				break;
			}
			rowKey.push_back(BString((const char*)cv));
		}
		if (!validRowKey)
			continue;

		int colIndex = 0;
		if (columnFieldCol >= 0)
		{
			Value cv;
			doc->GetValue(cell(columnFieldCol, row), cv);
			if (cv.fType != eTextData)
				continue;
			std::map<BString, int>::iterator cit = columnValueIndex.find(BString((const char*)cv));
			if (cit == columnValueIndex.end())
				continue; // difensivo, non dovrebbe capitare: stessa sorgente della prima passata
			colIndex = cit->second;
		}

		std::map<std::vector<BString>, PivotRow2D>::iterator git = groups.find(rowKey);
		if (git == groups.end())
		{
			PivotRow2D r;
			r.categories = rowKey;
			r.cells.resize(numColSlots);
			for (int c = 0; c < numColSlots; c++)
				r.cells[c].resize(measures.size());
			git = groups.insert(std::make_pair(rowKey, r)).first;
		}

		// Ogni misura e' valutata indipendentemente: un valore non
		// numerico esclude SOLO quella misura per questa riga, le altre
		// misure della stessa riga sorgente restano valide (a
		// differenza di BuildPivotTable sopra, che scarta l'intera riga
		// se l'unica misura non e' numerica).
		for (size_t m = 0; m < measures.size(); m++)
		{
			Value vv;
			doc->GetValue(cell(measures[m].sourceCol, row), vv);
			if (vv.fType != eNumData)
				continue;

			double v = (double)vv;
			PivotCellAgg& agg = git->second.cells[colIndex][m];
			if (agg.count == 0)
			{
				agg.minVal = v;
				agg.maxVal = v;
			}
			else
			{
				if (v < agg.minVal)
					agg.minVal = v;
				if (v > agg.maxVal)
					agg.maxVal = v;
			}
			agg.aggregate += v;
			agg.count++;
		}
	}

	for (std::map<std::vector<BString>, PivotRow2D>::iterator it = groups.begin();
			it != groups.end(); ++it)
		outRows->push_back(it->second);

	std::sort(outRows->begin(), outRows->end(), RowLess2D);
	return !outRows->empty();
}

void WritePivotTable2D(CContainer* doc, const cell& dest,
	const std::vector<BString>& columnValues, const std::vector<PivotMeasure>& measures,
	const std::vector<PivotRow2D>& rows)
{
	if (!doc || rows.empty() || measures.empty())
		return;

	int numKeyCols = (int)rows[0].categories.size();
	int numColSlots = columnValues.empty() ? 1 : (int)columnValues.size();
	int numMeasures = (int)measures.size();

	// Riga di intestazione 1: valore del campo Colonne, ripetuto su
	// tutte le colonne misura di quel valore -- vuoto sotto le colonne
	// chiave di riga (mai scritte qui) e vuoto ovunque quando non c'e'
	// campo Colonne (columnValues vuoto, stesso schema "due righe
	// sempre" per non introdurre un caso a parte in questa funzione).
	if (!columnValues.empty())
	{
		for (int c = 0; c < numColSlots; c++)
		{
			for (int m = 0; m < numMeasures; m++)
			{
				cell headerCell(dest.h + numKeyCols + c * numMeasures + m, dest.v);
				doc->NewCell(headerCell, Value(columnValues[c].String()), NULL);
			}
		}
	}

	// Riga di intestazione 2: etichette di categoria (colonne chiave di
	// riga) e di misura (colonne dati).
	for (int k = 0; k < numKeyCols; k++)
	{
		cell headerCell(dest.h + k, dest.v + 1);
		BString label(B_TRANSLATE("Categoria"));
		if (numKeyCols > 1)
			label << " " << (k + 1);
		doc->NewCell(headerCell, Value(label.String()), NULL);
	}
	for (int c = 0; c < numColSlots; c++)
	{
		for (int m = 0; m < numMeasures; m++)
		{
			cell headerCell(dest.h + numKeyCols + c * numMeasures + m, dest.v + 1);
			BString label = measures[m].label;
			if (label.IsEmpty())
				label = AggLabel(measures[m].aggFunc);
			doc->NewCell(headerCell, Value(label.String()), NULL);
		}
	}

	for (size_t i = 0; i < rows.size(); i++)
	{
		int rowY = dest.v + 2 + (int)i;
		for (int k = 0; k < numKeyCols; k++)
		{
			cell catCell(dest.h + k, rowY);
			doc->NewCell(catCell, Value(rows[i].categories[k].String()), NULL);
		}
		for (int c = 0; c < numColSlots; c++)
		{
			for (int m = 0; m < numMeasures; m++)
			{
				const PivotCellAgg& agg = rows[i].cells[c][m];
				if (agg.count == 0)
					continue; // combinazione mai vista nei dati: cella vuota

				double shown = agg.aggregate;
				PivotAggFunc fn = measures[m].aggFunc;
				if (fn == ePivotCount)
					shown = agg.count;
				else if (fn == ePivotAverage)
					shown = agg.aggregate / agg.count;
				else if (fn == ePivotMin)
					shown = agg.minVal;
				else if (fn == ePivotMax)
					shown = agg.maxVal;

				cell valCell(dest.h + numKeyCols + c * numMeasures + m, rowY);
				doc->NewCell(valCell, Value(shown), NULL);
			}
		}
	}
}

range PivotTable2DDestRange(const cell& dest, int numRowKeyCols,
	const std::vector<BString>& columnValues, const std::vector<PivotMeasure>& measures,
	size_t rowCount)
{
	int numColSlots = columnValues.empty() ? 1 : (int)columnValues.size();
	int numMeasures = (int)measures.size();
	int width = numRowKeyCols + numColSlots * numMeasures;
	int height = 2 + (int)rowCount; // due righe di intestazione + una per gruppo
	return range(dest.h, dest.v, dest.h + width - 1, dest.v + height - 1);
}
