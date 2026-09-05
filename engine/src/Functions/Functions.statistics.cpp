/*
	Copyright 1996, 1997, 1998, 2000
	        Hekkelman Programmatuur B.V.  All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.
	3. All advertising materials mentioning features or use of this software
	   must display the following acknowledgement:
	   
	    This product includes software developed by Hekkelman Programmatuur B.V.
	
	4. The name of Hekkelman Programmatuur B.V. may not be used to endorse or
	   promote products derived from this software without specific prior
	   written permission.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
	AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
	OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/
/*
	Functions.statistics.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include "Container.h"
#include "CellIterator.h"
#include "Formula.h"
#include "FunctionUtils.h"
#include "Functions.h"
#include "Globals.h"
#include "Set.h"

#include <algorithm>
#include <vector>

void COUNTAFunction(Value *stack, int argCnt, CContainer *cells)
{
	int i;
	long countedCells;
	range cRange;
	cell c;
	Value val;
	double d;
	bool b;
	time_t date;
	char s[256];

	countedCells = 0;

	for (i = 1; i <= argCnt; i++)
	{
		if (GetBooleanArgument(stack, argCnt, i, &b))
		{
			countedCells++;
		}
		else if (GetDoubleArgument(stack, argCnt, i, &d))
		{
			countedCells++;
		}
		else if (GetTimeArgument(stack, argCnt, i, &date))
		{
			countedCells++;
		}
		else if (GetTextArgument(stack, argCnt, i, s))
		{
			countedCells++;
		}		
		else if (GetRangeArgument(stack, argCnt, i, &cRange))
		{
			if (!cRange.IsValid())
			{
				stack[0] = gRefNan;
				return;
			}
			
			CContainer *rangeCells = GetRangeContainer(stack, i, cells);
			CCellIterator iter(rangeCells, &cRange);
			while (iter.NextExisting(c))
			{
				rangeCells->GetValue(c, val);
				if (val.fType != eNoData)
					countedCells++;
			}
		}
	}

	stack[0] = (double)countedCells;
}

void STDDEVFunction(Value *stack, int argCnt, CContainer *cells)
{
	VARIANCEFunction(stack, argCnt, cells);
//	if (stack[0].fType == eNumData && !isnan(stack[0].fDouble))
// deze if is overbodig, stack[0] is altijd een nummer, desnoods een nan
// en de sqrt van een nan is die nan zelf weer...
		stack[0].fDouble = sqrt(stack[0].fDouble);
}

void VARIANCEFunction(Value *stack, int argCnt, CContainer *cells)
{
	long countedCells = 0;
	range cRange;
	int i;
	cell c;
	double theResult, tmp, average;
	Value val;

	theResult = 0.0;
	average = 0.0;

	for (i = 1; i <= argCnt; i++)
	{

		if (GetDoubleArgument(stack, argCnt, i, &tmp))
		{
			average += tmp;
			countedCells++;
		}
		else if (GetRangeArgument(stack, argCnt, i, &cRange))
		{
			if (!cRange.IsValid())
			{
				theResult = gRefNan;
				break;
			}
		
			CContainer *rangeCells = GetRangeContainer(stack, i, cells);
			CCellIterator iter(rangeCells, &cRange);
			while (iter.NextExisting(c))
			{
				rangeCells->GetValue(c, val);
				if (val.fType == eNumData)
				{
					average += val.fDouble;
					countedCells++;
				}
			}
		}
	}

	if (countedCells)
	{
		average /= countedCells;
		
		for (i = 1; i <= argCnt; i++)
		{
			if (GetDoubleArgument(stack, argCnt, i, &tmp))
				theResult += (tmp - average) * (tmp - average);
			else if (GetRangeArgument(stack, argCnt, i, &cRange))
			{
				if (!cRange.IsValid())
				{
					theResult = gRefNan;
					break;
				}
				CContainer *rangeCells = GetRangeContainer(stack, i, cells);
				CCellIterator iter(rangeCells, &cRange);
				while (iter.NextExisting(c))
				{
					rangeCells->GetValue(c, val);
					if (val.fType == eNumData)
						theResult += (val.fDouble - average) * (val.fDouble - average);
				}
			}
		}
		
		theResult /= countedCells - 1;
	}

	stack[0] = theResult;
}

// MEDIAN/MODE (Fase 13): a differenza di AVG/VARIANCE sopra, che
// accumulano solo somme durante lo scorrimento, qui serve l'elenco
// completo dei valori (per ordinarli/confrontarli), quindi si
// raccolgono prima in un std::vector -- gia' usato altrove nel motore
// (Container.cpp), non un'introduzione nuova per questo file.

static void CollectNumbers(Value *stack, int argCnt, CContainer *cells,
	std::vector<double> &values, bool &invalidRange)
{
	range cRange;
	cell c;
	double tmp;
	Value val;

	invalidRange = false;
	for (int i = 1; i <= argCnt; i++)
	{
		if (GetDoubleArgument(stack, argCnt, i, &tmp))
			values.push_back(tmp);
		else if (GetRangeArgument(stack, argCnt, i, &cRange))
		{
			if (!cRange.IsValid())
			{
				invalidRange = true;
				return;
			}

			CContainer *rangeCells = GetRangeContainer(stack, i, cells);
			CCellIterator iter(rangeCells, &cRange);
			while (iter.NextExisting(c))
			{
				rangeCells->GetValue(c, val);
				if (val.fType == eNumData)
					values.push_back(val.fDouble);
			}
		}
	}
}

void MEDIANFunction(Value *stack, int argCnt, CContainer *cells)
{
	std::vector<double> values;
	bool invalidRange;

	CollectNumbers(stack, argCnt, cells, values, invalidRange);
	if (invalidRange)
	{
		stack[0] = gRefNan;
		return;
	}
	if (values.empty())
	{
		stack[0] = gValueNan;
		return;
	}

	std::sort(values.begin(), values.end());
	size_t n = values.size();
	stack[0] = (n % 2 == 0)
		? (values[n / 2 - 1] + values[n / 2]) / 2.0
		: values[n / 2];
}

void MODEFunction(Value *stack, int argCnt, CContainer *cells)
{
	std::vector<double> values;
	bool invalidRange;

	CollectNumbers(stack, argCnt, cells, values, invalidRange);
	if (invalidRange)
	{
		stack[0] = gRefNan;
		return;
	}
	if (values.empty())
	{
		stack[0] = gValueNan;
		return;
	}

	std::sort(values.begin(), values.end());

	double bestValue = values[0];
	int bestCount = 1, currentCount = 1;
	for (size_t i = 1; i < values.size(); i++)
	{
		currentCount = (values[i] == values[i - 1]) ? currentCount + 1 : 1;
		if (currentCount > bestCount)
		{
			bestCount = currentCount;
			bestValue = values[i];
		}
	}

	// Come in Excel: se nessun valore si ripete, MODE non ha una moda
	// da restituire.
	stack[0] = (bestCount >= 2) ? bestValue : gValueNan;
}

// RANK/LARGE/SMALL/AVERAGEIFS/MAXIFS/MINIFS/SUBTOTAL (Fase 26, vedi
// ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni originali
// di Sum-It, mancanti confrontando la tabella con l'elenco standard di
// Excel.

void RANKFunction(Value *stack, int argCnt, CContainer *cells)
{
	double number, orderArg = 0;
	range refRange;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetDoubleArgument(stack, argCnt, 1, &number)
		|| !GetRangeArgument(stack, argCnt, 2, &refRange) || !refRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	bool ascending = argCnt >= 3 && GetDoubleArgument(stack, argCnt, 3, &orderArg) && orderArg != 0;
	CContainer *refCells = GetRangeContainer(stack, 2, cells);

	bool found = false;
	int rank = 1;
	CCellIterator iter(refCells, &refRange);
	cell c;
	while (iter.NextExisting(c))
	{
		Value val;
		refCells->GetValue(c, val);
		if (val.fType != eNumData)
			continue;
		if (val.fDouble == number)
			found = true;
		if (ascending ? (val.fDouble < number) : (val.fDouble > number))
			rank++;
	}

	stack[0] = found ? (double)rank : gValueNan;
}

// LARGE/SMALL(intervallo,k): k-esimo valore piu' grande/piccolo --
// "intervallo" e' un SOLO argomento (a differenza di MEDIAN/MODE sopra
// che ne accettano quanti se ne vuole tramite CollectNumbers): "k"
// stesso e' un numero semplice, non parte dei dati da ordinare, quindi
// CollectNumbers (che tratterebbe anche "k" come un altro valore da
// raccogliere) non e' la funzione giusta qui.
void LARGEFunction(Value *stack, int argCnt, CContainer *cells)
{
	range dataRange;
	double kArg;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 1, &dataRange) || !dataRange.IsValid()
		|| !GetDoubleArgument(stack, argCnt, 2, &kArg))
	{
		stack[0] = gValueNan;
		return;
	}

	int k = static_cast<int>(rint(kArg));
	if (k < 1)
	{
		stack[0] = gValueNan;
		return;
	}

	CContainer *dataCells = GetRangeContainer(stack, 1, cells);
	std::vector<double> values;
	CCellIterator iter(dataCells, &dataRange);
	cell c;
	while (iter.NextExisting(c))
	{
		Value val;
		dataCells->GetValue(c, val);
		if (val.fType == eNumData)
			values.push_back(val.fDouble);
	}

	if (k > (int)values.size())
	{
		stack[0] = gValueNan;
		return;
	}

	std::sort(values.begin(), values.end(), std::greater<double>());
	stack[0] = values[k - 1];
}

void SMALLFunction(Value *stack, int argCnt, CContainer *cells)
{
	range dataRange;
	double kArg;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 1, &dataRange) || !dataRange.IsValid()
		|| !GetDoubleArgument(stack, argCnt, 2, &kArg))
	{
		stack[0] = gValueNan;
		return;
	}

	int k = static_cast<int>(rint(kArg));
	if (k < 1)
	{
		stack[0] = gValueNan;
		return;
	}

	CContainer *dataCells = GetRangeContainer(stack, 1, cells);
	std::vector<double> values;
	CCellIterator iter(dataCells, &dataRange);
	cell c;
	while (iter.NextExisting(c))
	{
		Value val;
		dataCells->GetValue(c, val);
		if (val.fType == eNumData)
			values.push_back(val.fDouble);
	}

	if (k > (int)values.size())
	{
		stack[0] = gValueNan;
		return;
	}

	std::sort(values.begin(), values.end());
	stack[0] = values[k - 1];
}

// AVERAGEIFS/MAXIFS/MINIFS(intervallo_valori,intervallo_criterio1,
// criterio1,...): a differenza di AVERAGEIF/SUMIF/COUNTIF/COUNTIFS in
// Functions.math.cpp (dove l'intervallo dati e' l'ULTIMO argomento,
// opzionale), qui l'intervallo dati e' sempre il PRIMO -- stessa
// differenza gia' presente nella vera Excel fra le forme "IF" (Fase
// 13/14) e le piu' recenti "IFS" a piu' criteri. MatchesCriteria e'
// dichiarata "static" in Functions.math.cpp (non visibile qui): una
// copia identica locale, nessun modo pulito di condividerla fra i due
// file senza un header apposta solo per questo, fuori scope qui.
static bool MatchesCriteriaLocal(const Value &val, const Value &criteria)
{
	if (criteria.fType == eNumData)
		return val.fType == eNumData && val.fDouble == criteria.fDouble;

	if (criteria.fType != eTextData)
		return false;

	const char *crit = criteria.fText;
	enum { eEQ, eNE, eGE, eLE, eGT, eLT } op = eEQ;
	const char *rest = crit;

	if (strncmp(crit, ">=", 2) == 0) { op = eGE; rest = crit + 2; }
	else if (strncmp(crit, "<=", 2) == 0) { op = eLE; rest = crit + 2; }
	else if (strncmp(crit, "<>", 2) == 0) { op = eNE; rest = crit + 2; }
	else if (crit[0] == '>') { op = eGT; rest = crit + 1; }
	else if (crit[0] == '<') { op = eLT; rest = crit + 1; }
	else if (crit[0] == '=') { op = eEQ; rest = crit + 1; }

	if (rest != crit)
	{
		char *end = NULL;
		double num = strtod(rest, &end);
		if (end == rest || val.fType != eNumData)
			return false;

		switch (op)
		{
			case eGE: return val.fDouble >= num;
			case eLE: return val.fDouble <= num;
			case eGT: return val.fDouble > num;
			case eLT: return val.fDouble < num;
			case eNE: return val.fDouble != num;
			default: return val.fDouble == num;
		}
	}

	char *end = NULL;
	double critNum = strtod(crit, &end);
	if (end != crit && *end == 0)
		return val.fType == eNumData && val.fDouble == critNum;

	return val.fType == eTextData && strcasecmp(val.fText, crit) == 0;
}

// Comune a AVERAGEIFS/MAXIFS/MINIFS sotto: risolve l'intervallo valori
// (arg 1) e le coppie intervallo/criterio (arg 2+), poi chiama
// "visit(valueVal)" per ogni posizione dove TUTTE le coppie
// corrispondono -- ogni funzione decide da sola cosa farne (media,
// massimo, minimo).
template <typename Visitor>
static bool VisitIfsMatches(Value *stack, int argCnt, CContainer *cells, Visitor visit)
{
	const int kMaxPairs = 12; // vedi kMaxStackHeight in Formula.h

	if (argCnt < 3 || (argCnt - 1) % 2 != 0 || (argCnt - 1) / 2 > kMaxPairs)
		return false;

	range valueRange;
	if (!GetRangeArgument(stack, argCnt, 1, &valueRange) || !valueRange.IsValid())
		return false;
	CContainer *valueCells = GetRangeContainer(stack, 1, cells);

	int pairCount = (argCnt - 1) / 2;
	range ranges[kMaxPairs];
	CContainer *rangeCells[kMaxPairs];
	for (int p = 0; p < pairCount; p++)
	{
		int rangeArgNr = 2 + 2 * p;
		if (!GetRangeArgument(stack, argCnt, rangeArgNr, &ranges[p]) || !ranges[p].IsValid())
			return false;
		rangeCells[p] = GetRangeContainer(stack, rangeArgNr, cells);
	}

	CCellIterator iter(rangeCells[0], &ranges[0]);
	cell c;
	while (iter.NextExisting(c))
	{
		bool allMatch = true;
		for (int p = 0; p < pairCount && allMatch; p++)
		{
			cell target(ranges[p].left + (c.h - ranges[0].left),
				ranges[p].top + (c.v - ranges[0].top));
			Value val;
			rangeCells[p]->GetValue(target, val);
			int critArgNr = 3 + 2 * p;
			if (!MatchesCriteriaLocal(val, stack[critArgNr - 1]))
				allMatch = false;
		}
		if (!allMatch)
			continue;

		cell target(valueRange.left + (c.h - ranges[0].left),
			valueRange.top + (c.v - ranges[0].top));
		Value valueVal;
		valueCells->GetValue(target, valueVal);
		if (valueVal.fType == eNumData)
			visit(valueVal.fDouble);
	}

	return true;
}

// AVERAGEIFS (10 caratteri) non entra nel campo funcName[10] a
// lunghezza fissa della risorsa 'Func': registrata internamente come
// "AVGIFS" (vedi funcs_by_nr.r), alias in GetFunctionNr (Utils.cpp)
// verso lo stesso funcNr, identico principio di
// SUBSTITUTE/NETWORKDAYS/CEILING.MATH/CONCATENATE/LOG10.
void AVERAGEIFSFunction(Value *stack, int argCnt, CContainer *cells)
{
	double sum = 0.0;
	long count = 0;
	bool ok = VisitIfsMatches(stack, argCnt, cells, [&](double v) { sum += v; count++; });
	stack[0] = (ok && count > 0) ? sum / count : gRefNan;
}

void MAXIFSFunction(Value *stack, int argCnt, CContainer *cells)
{
	double best = 0.0;
	bool found = false;
	bool ok = VisitIfsMatches(stack, argCnt, cells, [&](double v) {
		if (!found || v > best) { best = v; found = true; }
	});
	stack[0] = (ok && found) ? best : (ok ? 0.0 : gRefNan);
}

void MINIFSFunction(Value *stack, int argCnt, CContainer *cells)
{
	double best = 0.0;
	bool found = false;
	bool ok = VisitIfsMatches(stack, argCnt, cells, [&](double v) {
		if (!found || v < best) { best = v; found = true; }
	});
	stack[0] = (ok && found) ? best : (ok ? 0.0 : gRefNan);
}

// SUBTOTAL(numero_funzione,intervallo1,...): solo le sei aggregazioni
// piu' comuni (AVERAGE/COUNT/COUNTA/MAX/MIN/SUM, codici 1-5 e 9, o
// 101-105/109 per la variante "ignora le righe nascoste" -- questo
// motore non tiene traccia di quali righe siano nascoste a livello di
// CContainer/funzione, solo SheetView lo sa, quindi la variante 100+
// si comporta esattamente come quella base, non ignora nulla in piu').
// PRODUCT/STDEV/STDEVP/VAR/VARP (6,7,8,10,11) non sono supportate.
// Excludes cells whose OWN formula is itself a SUBTOTAL call (real
// Excel behavior, to avoid double-counting a subtotal that's already
// included in another total over the same column) -- found by
// analyzing a real XLSX file (agile-kanban-board.xlsx, Vertex42): 5
// chained SUBTOTAL cells (H11/H15/H18/H22/H25) where the last one sums
// the whole H11:H24, including the 4 earlier SUBTOTAL cells on top of
// the raw data those same cells already total -- without this
// exclusion the final total was doubled (16 instead of 8), halving
// any percentage computed from it (12.5% instead of the correct 25%).
static bool IsNestedSubtotal(CContainer *rangeCells, cell c)
{
	void *formula = rangeCells->GetCellFormula(c);
	if (!formula)
		return false;
	CFormula form(formula);
	CSet funcs;
	form.CollectFunctionNrs(funcs);
	return funcs[kSUBTOTALFuncNr];
}

void SUBTOTALFunction(Value *stack, int argCnt, CContainer *cells)
{
	double funcNumArg;

	if (argCnt < 2 || !GetDoubleArgument(stack, argCnt, 1, &funcNumArg))
	{
		stack[0] = gValueNan;
		return;
	}

	int funcNum = static_cast<int>(rint(funcNumArg));
	if (funcNum > 100)
		funcNum -= 100;

	double sum = 0.0, maxVal = 0.0, minVal = 0.0;
	long count = 0, countA = 0;
	bool foundFirst = false;

	for (int i = 2; i <= argCnt; i++)
	{
		range r;
		if (!GetRangeArgument(stack, argCnt, i, &r) || !r.IsValid())
			continue;
		CContainer *rangeCells = GetRangeContainer(stack, i, cells);
		CCellIterator iter(rangeCells, &r);
		cell c;
		while (iter.NextExisting(c))
		{
			if (IsNestedSubtotal(rangeCells, c))
				continue;
			Value val;
			rangeCells->GetValue(c, val);
			if (val.fType != eNoData)
				countA++;
			if (val.fType == eNumData)
			{
				sum += val.fDouble;
				count++;
				if (!foundFirst || val.fDouble > maxVal)
					maxVal = val.fDouble;
				if (!foundFirst || val.fDouble < minVal)
					minVal = val.fDouble;
				foundFirst = true;
			}
		}
	}

	switch (funcNum)
	{
		case 1: stack[0] = count > 0 ? sum / count : gValueNan; break; // AVERAGE
		case 2: stack[0] = (double)count; break;                       // COUNT
		case 3: stack[0] = (double)countA; break;                      // COUNTA
		case 4: stack[0] = foundFirst ? maxVal : 0.0; break;           // MAX
		case 5: stack[0] = foundFirst ? minVal : 0.0; break;           // MIN
		case 9: stack[0] = sum; break;                                 // SUM
		default: stack[0] = gValueNan; break; // aggregazione non supportata
	}
}

