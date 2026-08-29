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
	Container.styles.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include <algorithm>

#include <support/Debug.h>

#include "Cell.h"
#include "Formula.h"
#include "Value.h"
#include "Formula.h"
#include "CellData.h"
#include "Container.h"
#include "EngineViewStub.h"
#include "MyError.h"
#include "Formatter.h"
#include "Utils.h"
#include "StringTable.h"
#include "CellIterator.h"
#include "CellStyle.h"
#include "FontMetrics.h"

#if DEBUG
void WarnForUnlockedContainer(int lineNr);
#define CHECKLOCK	if (!fWriteLocker.IsLocked() && fInView) WarnForUnlockedContainer(__LINE__);

//THROW((errContainerNotLocked));
#else
#define CHECKLOCK	
#endif

void CContainer::GetCellStyle(const cell& inLoc, CellStyle& outStyle)
{	//CHECKLOCK
	outStyle = gStyleTable[GetCellStyleNr(inLoc)];
} /* GetCellStyle */

void CContainer::SetCellStyle(const cell& inLoc, CellStyle& inStyle)
{	CHECKLOCK
	SetCellStyleNr(inLoc, gStyleTable.GetStyleID(inStyle));
} /* SetCellStyle */

int CContainer::GetCellStyleNr(const cell& inLoc)
{	//CHECKLOCK
	cellmap::iterator i;
	
	if ((i = fCellData.find(inLoc)) != fCellData.end())
		return (*i).second.mStyle;
	else
		return GetColumnStyleNr(inLoc.h);
} /* GetCellStyleNr */

void CContainer::SetCellStyleNr(const cell& inLoc, int inStyle)
{	CHECKLOCK
	cellmap::iterator i;
	
	if ((i = fCellData.find(inLoc)) == fCellData.end())
	{
		if (inStyle != fDefaultCellStyle)
		{
			Value v;
			NewCell(inLoc, v, NULL);
			fCellData[inLoc].mStyle = inStyle;
		}
		else
			return;
	}	
	else
		(*i).second.mStyle = inStyle;
} /* SetCellStyleNr */

void CContainer::GetColumnStyle(int colNr, CellStyle& outStyle)
{	//CHECKLOCK
	outStyle = gStyleTable[GetColumnStyleNr(colNr)];
} /* GetColumnStyle */

void CContainer::SetColumnStyle(int colNr, CellStyle& inStyle)
{	CHECKLOCK
	SetColumnStyleNr(colNr, gStyleTable.GetStyleID(inStyle));
} /* SetColumnStyle */

int CContainer::GetColumnStyleNr(int colNr)
{	//CHECKLOCK
	if (fColumnStyles[colNr] >= 0)
		return fColumnStyles[colNr];
	else
		return fDefaultCellStyle;
} /* GetColumnStyleNr */

void CContainer::SetColumnStyleNr(int colNr, int inStyle)
{	CHECKLOCK
	fColumnStyles.SetValue(colNr, inStyle);
} /* SetColumnStyleNr */

void CContainer::GetDefaultCellStyle(CellStyle& outStyle)
{
	outStyle = gStyleTable[fDefaultCellStyle];
} /* GetDefaultCellStyle */

void CContainer::SetDefaultCellStyle(CellStyle& inStyle)
{
	fDefaultCellStyle = gStyleTable.GetStyleID(inStyle);
} /* CContainer::SetDefaultCellStyle */

int CContainer::CollectStyles(int *styleList)
{
	int result = 0;
	cellmap::iterator i;
	
	for (i = fCellData.begin(); i != fCellData.end(); i++)
	{
		int styleNr = (*i).second.mStyle;
		bool isNew = true;
		
		for (int i = 0; i < result && isNew; i++)
			if (styleNr == styleList[i])
			{
				isNew = false;
				break;
			}
		
		if (isNew)
			styleList[result++] = styleNr;
	}
	
	for (int i = 1; i < kColCount; i++)
	{
		int styleNr = fColumnStyles[i];
		bool isNew = true;

		if (styleNr < 0) continue;
		
		for (int i = 0; i < result && isNew; i++)
			if (styleNr == styleList[i])
			{
				isNew = false;
				break;
			}
			
		if (isNew)
			styleList[result++] = styleNr;
	}
	
	return result;
} // CContainer::CollectStyles

// Risolve un ColorScalePoint (Fase 33) in una soglia numerica vera,
// dati il minimo/massimo REALI trovati fra le celle numeriche
// dell'intervallo e i loro valori ordinati (serve solo per
// "percentile", interpolazione lineare fra i due valori piu' vicini
// al rango richiesto -- stesso metodo comune di numpy/Excel).
// "percent" e' diverso da "percentile": e' una posizione fra min e
// max (es. 25% = un quarto della strada da min a max), non un
// percentile della distribuzione reale dei dati.
static double ResolveColorScaleThreshold(const ColorScalePoint& point,
	double rangeMin, double rangeMax, const std::vector<double>& sortedValues)
{
	if (point.cfvoType == "max")
		return rangeMax;
	if (point.cfvoType == "num")
		return point.cfvoValue;
	if (point.cfvoType == "percent")
		return rangeMin + (point.cfvoValue / 100.0) * (rangeMax - rangeMin);
	if (point.cfvoType == "percentile" && !sortedValues.empty())
	{
		double rank = (point.cfvoValue / 100.0) * (double)(sortedValues.size() - 1);
		if (rank < 0) rank = 0;
		size_t lo = (size_t)rank;
		size_t hi = (lo + 1 < sortedValues.size()) ? lo + 1 : lo;
		double frac = rank - (double)lo;
		return sortedValues[lo] + frac * (sortedValues[hi] - sortedValues[lo]);
	}
	return rangeMin; // "min", o un cfvoType sconosciuto: ripiego sicuro
}

static rgb_color InterpolateColor(const rgb_color& a, const rgb_color& b, double t)
{
	rgb_color result;
	result.red = (uint8)(a.red + t * ((double)b.red - (double)a.red));
	result.green = (uint8)(a.green + t * ((double)b.green - (double)a.green));
	result.blue = (uint8)(a.blue + t * ((double)b.blue - (double)a.blue));
	result.alpha = 255;
	return result;
}

// Formattazione condizionale viva (Fase 13): stessa identica logica di
// valutazione gia' scritta per l'importazione XLSX (Fase 12,
// ApplyConditionalFormatting/XlsxTranslator.cpp) -- solo il risultato
// cambia, un colore per cella restituito invece di scritto in
// CellStyle, cosi' chi chiama (SheetView::Draw) puo' rivalutarlo a
// ogni ridisegno senza corrompere lo stile "vero" della cella.
std::map<cell, rgb_color> CContainer::EvaluateConditionalFormatting()
{
	std::map<cell, rgb_color> result;

	for (size_t i = 0; i < fCondFormatRules.size(); i++)
	{
		const ConditionalFormatRule& rule = fCondFormatRules[i];

		if (rule.type == eCondCellIsEqual)
		{
			for (size_t r = 0; r < rule.ranges.size(); r++)
			{
				const range& rg = rule.ranges[r];
				for (int row = rg.top; row <= rg.bottom; row++)
				{
					for (int col = rg.left; col <= rg.right; col++)
					{
						cell c(col, row);
						char text[4096];
						GetCellResult(c, text, sizeof(text), true);
						if (rule.compareValue == text)
							result[c] = rule.bgColor;
					}
				}
			}
		}
		else if (rule.type == eCondDuplicateValues)
		{
			for (size_t r = 0; r < rule.ranges.size(); r++)
			{
				const range& rg = rule.ranges[r];
				std::map<std::string, int> counts;
				for (int row = rg.top; row <= rg.bottom; row++)
				{
					for (int col = rg.left; col <= rg.right; col++)
					{
						char text[4096];
						GetCellResult(cell(col, row), text, sizeof(text), true);
						if (text[0] != 0)
							counts[text]++;
					}
				}
				for (int row = rg.top; row <= rg.bottom; row++)
				{
					for (int col = rg.left; col <= rg.right; col++)
					{
						cell c(col, row);
						char text[4096];
						GetCellResult(c, text, sizeof(text), true);
						if (text[0] != 0 && counts[text] > 1)
							result[c] = rule.bgColor;
					}
				}
			}
		}
		else if (rule.type == eCondColorScale && rule.colorScalePoints.size() >= 2)
		{
			for (size_t r = 0; r < rule.ranges.size(); r++)
			{
				const range& rg = rule.ranges[r];

				// Prima passata: raccoglie i valori numerici di TUTTO
				// l'intervallo -- servono min/max (o percentile) PRIMA
				// di poter colorare qualsiasi singola cella, a
				// differenza dei due tipi sopra dove ogni cella si
				// valuta da sola.
				std::vector<double> values;
				for (int row = rg.top; row <= rg.bottom; row++)
				{
					for (int col = rg.left; col <= rg.right; col++)
					{
						Value v;
						GetValue(cell(col, row), v);
						if (v.fType == eNumData && !v.IsNan())
							values.push_back((double)v);
					}
				}
				if (values.empty())
					continue;

				std::vector<double> sortedValues = values;
				std::sort(sortedValues.begin(), sortedValues.end());
				double rangeMin = sortedValues.front();
				double rangeMax = sortedValues.back();

				std::vector<double> thresholds(rule.colorScalePoints.size());
				for (size_t p = 0; p < rule.colorScalePoints.size(); p++)
					thresholds[p] = ResolveColorScaleThreshold(rule.colorScalePoints[p],
						rangeMin, rangeMax, sortedValues);

				// Seconda passata: interpola il colore di ogni cella
				// numerica in base al segmento (fra due soglie
				// consecutive) in cui cade il suo valore.
				for (int row = rg.top; row <= rg.bottom; row++)
				{
					for (int col = rg.left; col <= rg.right; col++)
					{
						cell c(col, row);
						Value v;
						GetValue(c, v);
						if (v.fType != eNumData || v.IsNan())
							continue;

						double val = (double)v;
						size_t segment = 0;
						while (segment + 1 < thresholds.size() && val > thresholds[segment + 1])
							segment++;
						size_t nextSegment = (segment + 1 < thresholds.size())
							? segment + 1 : segment;

						double lo = thresholds[segment], hi = thresholds[nextSegment];
						double t = (hi > lo) ? (val - lo) / (hi - lo) : 0.0;
						if (t < 0) t = 0;
						if (t > 1) t = 1;

						result[c] = InterpolateColor(rule.colorScalePoints[segment].color,
							rule.colorScalePoints[nextSegment].color, t);
					}
				}
			}
		}
	}

	return result;
} // CContainer::EvaluateConditionalFormatting

