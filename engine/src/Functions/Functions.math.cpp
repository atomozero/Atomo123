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
	Functions.math.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Container.h"
#include "CellIterator.h"
#include "FunctionUtils.h"
#include "Functions.h"
#include "Globals.h"


void ABSFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d))
	{
		stack[0] = fabs(d);
	}
	else
	{
		stack[0] = gValueNan;
	}
}

void SUMFunction(Value *stack, int argCnt, CContainer *cells)
{
	double theResult = 0.0, tmp;
	int i;
	range cRange;
	cell c;
	Value val;

	for (i = 1; i <= argCnt; i++)
	{
		if (GetDoubleArgument(stack, argCnt, i, &tmp))
		{
			theResult += tmp;
		}
		else if (GetRangeArgument(stack, argCnt, i, &cRange))
		{
			if (!cRange.IsValid())
			{
				theResult = gRefNan;
				break;
			}
			
			// Fase 16: se l'intervallo e' stato risolto su un
			// altro foglio (es. "SUM(Foglio!A:A)"),
			// GetRangeContainer restituisce quel documento invece
			// di "cells" -- vedi il commento su
			// Value::fRangeContainer in Value.h.
			CContainer *rangeCells = GetRangeContainer(stack, i, cells);
			CCellIterator iter(rangeCells, &cRange);
			while (iter.NextExisting(c))
			{
				rangeCells->GetValue(c, val);
				if (val.fType == eNumData)
					theResult += val.fDouble;
			}
		}
	}

	stack[0] = theResult;
}

void ACOSFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = acos(d);
	else
		stack[0] = gRefNan;
}

void ASINFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d))
		stack[0] = asin(d);
	else
		stack[0] = gRefNan;
}

void ATANFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = atan(d);
	else
		stack[0] = gRefNan;
}

void ATAN2Function(Value *stack, int argCnt, CContainer *cells)
{
	double x, y;
	
	if (GetDoubleArgument(stack, argCnt, 1, &x) &&
		GetDoubleArgument(stack, argCnt, 2, &y))
	{
		if (x==0 && y==0)
			stack[0] = gDivNan;
		else
			stack[0] = atan2(y,x);
	}		
	else
		stack[0] = gValueNan;
}

void COSFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = cos(d);
	else
		stack[0] = gRefNan;
}

void COTFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = 1 / tan(d);
	else
		stack[0] = gRefNan;
}

void EXPFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = exp(d);
	else
		stack[0] = gRefNan;
}

void FRACFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d, h;

	if (GetDoubleArgument(stack, argCnt, 1, &d))
		stack[0] = modf(d, &h);
	else
		stack[0] = gRefNan;
}

void INTFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = rint(d);
	else
		stack[0] = gRefNan;
}

void LNFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = log(d);
	else
		stack[0] = gRefNan;
}

void LOGFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = log10(d);
	else
		stack[0] = gRefNan;
}

void MODFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d1, d2;

	if (GetDoubleArgument(stack, argCnt, 1, &d1) &&
		GetDoubleArgument(stack, argCnt, 2, &d2))
		stack[0] = fmod(d1, d2);
	else
		stack[0] = gRefNan;
}

void PIFunction(Value *stack, int, CContainer *)
{
//	stack[0] = PI; //3.141592653589793239;
	stack[0] = 3.141592653589793239;
}

void POWERFunction(Value *stack, int argCnt, CContainer *cells)
{
	double number;
	double power;

	if (GetDoubleArgument(stack, argCnt, 1, &number) &&
		GetDoubleArgument(stack, argCnt, 2, &power))
		stack[0] = pow(number, power);
	else
		stack[0] = gRefNan;
}

void RANDOMFunction(Value *stack, int, CContainer *)
{
	stack[0] = (double)rand();
}

void SIGNFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d))
	{
		if (isnan(d))
			stack[0] = d;
		else if (d < 0)
			stack[0] = -1.0;
		else if (d > 0)
			stack[0] = 1.0;
		else
			stack[0] = 0.0;
	}
	else
		stack[0] = gRefNan;
}

void SINFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d))
		stack[0] = sin(d);
	else
		stack[0] = gRefNan;
}

void SQRTFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d))
		stack[0] = sqrt(d);
	else
		stack[0] = gRefNan;
}

void TANFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d)) 
		stack[0] = tan(d);
	else
		stack[0] = gRefNan;
}

void AVGFunction(Value *stack, int argCnt, CContainer *cells)
{
	long countedCells = 0;
	range cRange;
	int i;
	cell c;
	double theResult, tmp;
	Value val;

	theResult = 0.0;

	for (i = 1; i <= argCnt; i++)
	{

		if (GetDoubleArgument(stack, argCnt, i, &tmp))
		{
			theResult += tmp;
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
					theResult += val.fDouble;
					countedCells++;
				}
// 				else if (val.fType != eNoData)
// 				{
// 					theResult = gValueNan;
// 					break;
// 				}
			}
		}
	}

	if (countedCells)
		theResult /= countedCells;
	stack[0] = theResult;
}

void CEILINGFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;
	
	if (GetDoubleArgument(stack, argCnt, 1, &d))
		stack[0] = ceil(d);
	else
		stack[0] = gValueNan;
}

void COUNTFunction(Value *stack, int argCnt, CContainer *cells)
{
	int i;
	long countedCells;
	range cRange;
	cell c;
	Value val;

	countedCells = 0;

	for (i = 1; i <= argCnt; i++)
	{

		if (GetDoubleArgument(stack, argCnt, i, &val.fDouble))
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
				if (val.fType == eNumData)
					countedCells++;
			}
		}
	}

	stack[0] = (double)countedCells;
}

void FLOORFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;
	
	if (GetDoubleArgument(stack, argCnt, 1, &d))
		stack[0] = floor(d);
	else
		stack[0] = gValueNan;
}

void MAXFunction(Value *stack, int argCnt, CContainer *cells)
{
	double theResult = 0.0;
	int i;
	range cRange;
	cell c;
	Value val;
	bool foundFirst = false;

	for (i = 1; i <= argCnt; i++)
	{

		if (GetDoubleArgument(stack, argCnt, i, &val.fDouble))
		{
			if (!foundFirst || theResult < val.fDouble)
			{
				theResult = val.fDouble;
				foundFirst = true;
			}
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
					if (!foundFirst || theResult < val.fDouble)
					{
						theResult = val.fDouble;
						foundFirst = true;
					}
			}
		}
	}

	stack[0] = theResult;
}

void MINFunction(Value *stack, int argCnt, CContainer *cells)
{
	double theResult = 0.0;
	int i;
	range cRange;
	cell c;
	Value val;
	bool foundFirst = false;

	for (i = 1; i <= argCnt; i++)
	{

		if (GetDoubleArgument(stack, argCnt, i, &val.fDouble))
		{
			if (!foundFirst || theResult > val.fDouble)
			{
				theResult = val.fDouble;
				foundFirst = true;
			}
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
					if (!foundFirst || theResult > val.fDouble)
					{
						theResult = val.fDouble;
						foundFirst = true;
					}
			}
		}
	}

	stack[0] = theResult;
}

void ROUNDFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d, n;
	
	if (GetDoubleArgument(stack, argCnt, 1, &d) &&
		GetDoubleArgument(stack, argCnt, 2, &n))
	{
		if (isnan(d))
			;
		else if (isnan(n))
			d = n;
		else if (n > 15 || n < -15)
			d = gValueNan;
		else
		{
			double deel, heel, factor;
			
			factor = pow(10.0, n);
			
			d *= factor;
			deel = modf(d, &heel);
			d = heel / factor;
			if (deel > 0.5 && fmod(heel, 2.0) == 1.0)
				d += 1.0;
		}
		stack[0] = d;
	}
	else
		stack[0] = gValueNan;
}

// ROUNDUP/ROUNDDOWN (Fase 14): a differenza di ROUND sopra (arrotonda
// al piu' vicino, met-a'-pari va all'intero pari, cosiddetto "banker's
// rounding"), qui non importa la cifra scartata -- ROUNDUP arrotonda
// SEMPRE lontano dallo zero, ROUNDDOWN SEMPRE verso lo zero (tronca).
// Stessa validazione di ROUND (num_digits fra -15 e 15, NaN propagato).
void ROUNDUPFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d, n;

	if (GetDoubleArgument(stack, argCnt, 1, &d) &&
		GetDoubleArgument(stack, argCnt, 2, &n))
	{
		if (isnan(d))
			;
		else if (isnan(n))
			d = n;
		else if (n > 15 || n < -15)
			d = gValueNan;
		else
		{
			double factor = pow(10.0, n);
			d = (d >= 0) ? ceil(d * factor) / factor : floor(d * factor) / factor;
		}
		stack[0] = d;
	}
	else
		stack[0] = gValueNan;
}

void ROUNDDOWNFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d, n;

	if (GetDoubleArgument(stack, argCnt, 1, &d) &&
		GetDoubleArgument(stack, argCnt, 2, &n))
	{
		if (isnan(d))
			;
		else if (isnan(n))
			d = n;
		else if (n > 15 || n < -15)
			d = gValueNan;
		else
		{
			double factor = pow(10.0, n);
			d = (d >= 0) ? floor(d * factor) / factor : ceil(d * factor) / factor;
		}
		stack[0] = d;
	}
	else
		stack[0] = gValueNan;
}

// TEXT(numero, formato) (Fase 14): solo il caso comune visto in file
// reali finora (TEXT(L36,"000"), zero-riempimento a N cifre) -- un
// formato fatto SOLO di placeholder di cifra ('0'/'#'), al massimo un
// punto decimale. Formati con separatore delle migliaia, percentuale,
// valuta o data (tutt'altra sintassi in Excel) non sono riconosciuti:
// restituiscono il numero cosi' com'e' senza applicare alcun formato
// (comportamento sicuro, non un errore bloccante) invece di un
// crash o un risultato silenziosamente sbagliato.
void TEXTFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;
	char fmt[256];

	if (!GetDoubleArgument(stack, argCnt, 1, &d) || !GetTextArgument(stack, argCnt, 2, fmt))
	{
		stack[0] = gValueNan;
		return;
	}
	if (isnan(d))
	{
		stack[0] = d;
		return;
	}

	int intDigits = 0, decDigits = 0;
	bool afterDot = false;
	bool recognized = true;
	for (const char *p = fmt; *p; p++)
	{
		if (*p == '.' && !afterDot)
			afterDot = true;
		else if (*p == '0' || *p == '#')
		{
			if (afterDot)
				decDigits++;
			else
				intDigits++;
		}
		else
		{
			recognized = false;
			break;
		}
	}

	char out[64];
	if (!recognized || (intDigits == 0 && decDigits == 0))
		snprintf(out, sizeof(out), "%.10g", d);
	else
	{
		int width = intDigits + (decDigits > 0 ? decDigits + 1 : 0);
		snprintf(out, sizeof(out), "%0*.*f", width, decDigits, d);
	}
	stack[0] = out;
}

// SUMIF/COUNTIF/AVERAGEIF: assenti dalle funzioni originali di
// Sum-It (le 86 della risorsa 'Func' storica), aggiunte perche'
// mancava proprio l'aggregazione condizionata, fra le piu' usate in
// un foglio di calcolo moderno. "criteria" accetta un numero (o del
// testo numerico) per un confronto esatto, del testo per un
// confronto letterale (senza distinguere maiuscole/minuscole, come
// Excel), oppure un operatore di confronto stile Excel in testa
// (">10", "<=5", "<>0", "=10") seguito da un numero.
static bool MatchesCriteria(const Value &val, const Value &criteria)
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
		// Un operatore e' stato riconosciuto: il resto deve essere un
		// numero valido, altrimenti il criterio non seleziona nessuna
		// cella (comportamento sicuro, non un errore bloccante).
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

	// Nessun operatore: se il criterio si legge per intero come
	// numero, confronto numerico esatto (accetta sia SUMIF(A:A,10,..)
	// che SUMIF(A:A,"10",..), come Excel); altrimenti confronto
	// testuale esatto.
	char *end = NULL;
	double critNum = strtod(crit, &end);
	if (end != crit && *end == 0)
		return val.fType == eNumData && val.fDouble == critNum;

	return val.fType == eTextData && strcasecmp(val.fText, crit) == 0;
} /* MatchesCriteria */

void SUMIFFunction(Value *stack, int argCnt, CContainer *cells)
{
	range criteriaRange, sumRange;

	if (argCnt < 2 || !GetRangeArgument(stack, argCnt, 1, &criteriaRange)
		|| !criteriaRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	// L'intervallo da sommare e' il terzo argomento, se presente;
	// altrimenti lo stesso intervallo dei criteri (come in Excel).
	if (argCnt >= 3)
	{
		if (!GetRangeArgument(stack, argCnt, 3, &sumRange) || !sumRange.IsValid())
		{
			stack[0] = gRefNan;
			return;
		}
	}
	else
		sumRange = criteriaRange;

	// Fase 16: i due intervalli possono vivere su fogli diversi
	// (es. "SUMIF(Foglio1!A:A,">5",Foglio2!B:B)"), ciascuno risolto
	// per conto proprio -- vedi il commento su Value::fRangeContainer
	// in Value.h.
	CContainer *criteriaCells = GetRangeContainer(stack, 1, cells);
	CContainer *sumCells = (argCnt >= 3) ? GetRangeContainer(stack, 3, cells) : criteriaCells;

	double result = 0.0;
	CCellIterator iter(criteriaCells, &criteriaRange);
	cell c;
	while (iter.NextExisting(c))
	{
		Value val;
		criteriaCells->GetValue(c, val);
		if (!MatchesCriteria(val, stack[1]))
			continue;

		// stessa posizione relativa dentro l'intervallo da sommare
		cell target(sumRange.left + (c.h - criteriaRange.left),
			sumRange.top + (c.v - criteriaRange.top));
		Value sumVal;
		sumCells->GetValue(target, sumVal);
		if (sumVal.fType == eNumData)
			result += sumVal.fDouble;
	}

	stack[0] = result;
} /* SUMIFFunction */

void COUNTIFFunction(Value *stack, int argCnt, CContainer *cells)
{
	range criteriaRange;

	if (argCnt < 2 || !GetRangeArgument(stack, argCnt, 1, &criteriaRange)
		|| !criteriaRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	CContainer *criteriaCells = GetRangeContainer(stack, 1, cells);

	long count = 0;
	CCellIterator iter(criteriaCells, &criteriaRange);
	cell c;
	while (iter.NextExisting(c))
	{
		Value val;
		criteriaCells->GetValue(c, val);
		if (MatchesCriteria(val, stack[1]))
			count++;
	}

	stack[0] = (double)count;
} /* COUNTIFFunction */

// COUNTIFS(intervallo1, criterio1, [intervallo2, criterio2], ...): a
// differenza di COUNTIF sopra (un solo intervallo/criterio), conta le
// posizioni dove OGNI coppia intervallo/criterio corrisponde -- un AND
// fra tutte le coppie, non una somma separata. Stesso MatchesCriteria
// gia' usato da SUMIF/COUNTIF/AVERAGEIF, applicato a ogni intervallo
// alla STESSA posizione relativa del primo (Excel richiede che tutti
// gli intervalli abbiano la stessa forma -- non verificato
// esplicitamente qui: una forma diversa semplicemente non trova mai
// nulla alla posizione corrispondente, comportamento sicuro invece di
// un errore bloccante).
void COUNTIFSFunction(Value *stack, int argCnt, CContainer *cells)
{
	const int kMaxPairs = 12; // vedi kMaxStackHeight in Formula.h: mai piu' di ~12 coppie in una formula reale
	if (argCnt < 2 || argCnt % 2 != 0 || argCnt / 2 > kMaxPairs)
	{
		stack[0] = gRefNan;
		return;
	}

	int pairCount = argCnt / 2;
	range ranges[kMaxPairs];
	CContainer *rangeCells[kMaxPairs];
	for (int p = 0; p < pairCount; p++)
	{
		if (!GetRangeArgument(stack, argCnt, 2 * p + 1, &ranges[p]) || !ranges[p].IsValid())
		{
			stack[0] = gRefNan;
			return;
		}
		// Fase 16: ogni coppia intervallo/criterio puo' vivere su un
		// foglio diverso dalle altre, risolta per conto proprio.
		rangeCells[p] = GetRangeContainer(stack, 2 * p + 1, cells);
	}

	long count = 0;
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
			if (!MatchesCriteria(val, stack[2 * p + 1]))
				allMatch = false;
		}
		if (allMatch)
			count++;
	}

	stack[0] = (double)count;
} /* COUNTIFSFunction */

void AVERAGEIFFunction(Value *stack, int argCnt, CContainer *cells)
{
	range criteriaRange, sumRange;

	if (argCnt < 2 || !GetRangeArgument(stack, argCnt, 1, &criteriaRange)
		|| !criteriaRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	if (argCnt >= 3)
	{
		if (!GetRangeArgument(stack, argCnt, 3, &sumRange) || !sumRange.IsValid())
		{
			stack[0] = gRefNan;
			return;
		}
	}
	else
		sumRange = criteriaRange;

	CContainer *criteriaCells = GetRangeContainer(stack, 1, cells);
	CContainer *sumCells = (argCnt >= 3) ? GetRangeContainer(stack, 3, cells) : criteriaCells;

	double sum = 0.0;
	long count = 0;
	CCellIterator iter(criteriaCells, &criteriaRange);
	cell c;
	while (iter.NextExisting(c))
	{
		Value val;
		criteriaCells->GetValue(c, val);
		if (!MatchesCriteria(val, stack[1]))
			continue;

		cell target(sumRange.left + (c.h - criteriaRange.left),
			sumRange.top + (c.v - criteriaRange.top));
		Value sumVal;
		sumCells->GetValue(target, sumVal);
		if (sumVal.fType == eNumData)
		{
			sum += sumVal.fDouble;
			count++;
		}
	}

	stack[0] = count > 0 ? sum / count : gRefNan;
} /* AVERAGEIFFunction */

