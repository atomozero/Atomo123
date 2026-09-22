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
	Value.c

	Copyright 1997, Hekkelman Programmatuur
	Portions Copyright 2026 Andrea Bernardi

	Part of Sum-It for the BeBox version 1.1.

*/

#ifndef   CELL_H
#include "Cell.h"
#endif

#ifndef   RANGE_H
#include "Range.h"
#endif

#ifndef   FORMULA_H
#include "Formula.h"
#endif

#ifndef   VALUE_H
#include "Value.h"
#endif

#ifndef   UTILS_H
#include "Utils.h"
#endif

#ifndef   MYERROR_H
#include "MyError.h"
#endif

#ifndef   GLOBALS_H
#include "Globals.h"
#endif

// Confronto range-scalare/range-range (Tier 2 di "Path to full Excel
// parity", vedi CompareRangeAware sotto): serve la definizione VERA di
// CContainer per rileggere le celle di un range, non solo il puntatore
// dichiarato in avanti gia' presente in Value.h.
#include "Container.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

Value::Value()
{
	fType = eNoData;
	fRangeContainer = NULL;
	fArrayCount = 0;
}

Value::Value(double d)
{
	fType = eNumData;
	fDouble = d;
	fRangeContainer = NULL;
	fArrayCount = 0;
}

Value::Value(time_t t)
{
	fType = eTimeData;
	fTime = t;
	fRangeContainer = NULL;
	fArrayCount = 0;
}

Value::Value(const char *inString, bool inCopy)
{
	fType = eTextData;
	fRangeContainer = NULL;
	fArrayCount = 0;
	if ((fTextIsCopy = inCopy) == true)
	{
		fText = STRDUP(inString);
		FailNil(fText);
	}
	else
		fText = const_cast<char *>(inString) ;
}

Value::Value(bool b)
{
	fType = eBoolData;
	fBool = b;
	fRangeContainer = NULL;
	fArrayCount = 0;
}

Value::Value(CellData& cd)
{
	fType = eNoData;
	*this = cd;
}

// Fase 29: vedi il commento su questo costruttore in Value.h -- copia
// PROFONDA (STRDUP) per un testo, esattamente come operator=(const
// Value&) sotto, ma senza il suo Clear() iniziale, che presuppone un
// oggetto GIA' costruito (qui "this" non lo e' ancora).
Value::Value(const Value& other)
{
	fType = other.fType;
	if (fType == eTextData)
	{
		fText = other.fText ? STRDUP(other.fText) : NULL;
		fTextIsCopy = true;
	}
	else if (fType == eBoolArrayData)
	{
		fArrayCount = other.fArrayCount;
		fBoolArray = other.fBoolArray ? new bool[fArrayCount] : NULL;
		if (fBoolArray)
			std::copy(other.fBoolArray, other.fBoolArray + fArrayCount, fBoolArray);
	}
	else
		fDouble = other.fDouble;
	fRangeContainer = other.fRangeContainer;
	if (fType != eBoolArrayData)
		fArrayCount = 0;
}

Value::~Value()
{
	if (fType == eTextData && fText && fTextIsCopy)
		FREE(fText);
	else if (fType == eBoolArrayData && fBoolArray)
		delete[] fBoolArray;
}

void Value::operator+=(Value &v)
{
	if (fType == v.fType)
		switch (fType)
		{
			case eNumData:
				fDouble += v.fDouble;
				break;
			
			case eTextData:
			{
				char *t = fText;
				int tlen = strlen(fText) + strlen(v.fText);
				fText = (char *)MALLOC(tlen + 1);
				FailNil(fText);
				strcpy(fText, t);
				strcat(fText, v.fText);
				if (fTextIsCopy)
					FREE(t);
				fTextIsCopy = true;
				break;
			}
			case eNoData:
				break;
			case eTimeData:
				if (abs((long)v.fTime) < (24 * 60 * 60))
					fTime += v.fTime;
				else
					*this = gValueNan;
				break;
			default:
				*this = gValueNan;
		}
	else if (fType == eNoData)
		this->operator=(v);
	else if (fType == eTimeData && v.fType == eNumData)
		fTime += static_cast<time_t>( v.fDouble ) ;
	else if (v.fType != eNoData)
		*this = gValueNan;
}

void Value::operator-=(Value &v)
{
	if (fType == eNumData && v.fType == eNumData)
		fDouble -= v.fDouble;
	else if (fType == eNoData && v.fType == eNumData)
	{
		fType = eNumData;
		fDouble = -v.fDouble;
	}
	else if (fType == eTimeData && v.fType == eTimeData)
	{
		fType = eNumData;
		fDouble = fTime - v.fTime;
	}
	else if (fType == eTimeData && v.fType == eNumData)
	{
		fTime -= static_cast<time_t>( v.fDouble ) ;
	}
	else if (v.fType != eNoData)
		*this = gValueNan;
}

void Value::operator*=(Value &v)
{
	if (fType == eNumData && v.fType == eNumData)
		this->operator=(fDouble * v.fDouble);
	else if (fType == eNoData || v.fType == eNoData)
		*this = 0.0;
	else
		*this = gValueNan;
}

void Value::operator/=(Value &v)
{
	if (fType == eNumData && v.fType == eNumData)
	{
		if (v.fDouble != 0.0)
			fDouble /= v.fDouble;
		else
			*this = gDivNan;
	}
	else if (fType == eNoData && v.fType == eNumData)
		*this = 0.0;
	else if (fType == eNumData && v.fType == eNoData)
		*this = gDivNan;
	else
		*this = gValueNan;
}

void Value::operator^=(Value &v)
{
	if (fType == eNumData || v.fType == eNumData)
		this->operator=(pow(fDouble, v.fDouble));
	else
		*this = gValueNan;
}

void Value::operator&=(Value &v)
{
	if (fType == eBoolData && v.fType == eBoolData)
		fBool = fBool && v.fBool;
	else
		*this = gValueNan;
}

void Value::operator|=(Value &v)
{
	if (fType == eBoolData && v.fType == eBoolData)
		fBool = fBool || v.fBool;
	else
		*this = gValueNan;
}

//Value& Value::Error(int err)
//{
//	Value result(Nan(err));
//	return result;
//}

static const char kCompareTypes[7][7] = 
{
	{ 0, 1, 1, 1, 1, 1, 1 },
	{ -1, 0, -1, -1, -1, -1, -1 },
	{ -1, 1, 0, -1, -1, -1, -1 },
	{ -1, 1, 1, 0, -1, -1, -1 },
	{ -1, 1, 1, 1, 0, -1, -1 },
	{ -1, 1, 1, 1, 1, 0, -1 },
	{ -1, 1, 1, 1, 1, 1, 0 }
};

// Una cella VUOTA (eNoData) confrontata con "=" o "<>" contro un
// numero/testo/booleano equivale al valore predefinito di quel tipo
// (0, "", FALSO), esattamente come in Excel -- bug reale scoperto
// analizzando file XLSX veri: il pattern comunissimo
// "=IF(cella=0;"";calcolo)" per nascondere una riga finche' la cella
// non e' compilata prendeva sempre il ramo sbagliato, perche' prima
// di questo fix kCompareTypes trattava eNoData come "diverso da
// chiunque" (in realta' pensato per un altro scopo: ordinare le
// celle vuote sempre per ultime in un Ordina, comportamento voluto e
// NON toccato qui -- vedi ScalarCompare sotto, invariato per </<=/>/>=).
// Solo "="/"<>" hanno un significato pratico per il confronto con un
// valore predefinito; "<"/">" restano ordinamento puro.
static bool NoDataEqualsDefault(Value &v)
{
	switch (v.fType)
	{
		case eNumData:  return v.fDouble == 0.0;
		case eBoolData: return v.fBool == false;
		case eTextData: return v.fText == NULL || v.fText[0] == 0;
		default:        return false;
	}
}

enum { kCmpLT, kCmpLE, kCmpGT, kCmpGE, kCmpEQ, kCmpNE };

// Nucleo del confronto fra due Value SEMPRE scalari (mai eRangeData/
// eBoolArrayData -- quei due casi sono intercettati prima, vedi
// CompareRangeAware sotto): stessa identica logica che stava prima in
// ciascuno dei sei "operator<"/"<="/">"/">="/"=="/"!=", solo
// fattorizzata qui per essere riusata cella per cella dal confronto
// range-aware, invece di duplicarla in un ciclo per ognuno dei sei.
static bool ScalarCompare(Value &a, Value &b, int op)
{
	switch (op)
	{
		case kCmpLT:
			if (a.fType == eNumData && b.fType == eNumData) return a.fDouble < b.fDouble;
			if (a.fType == eTextData && b.fType == eTextData) return strcasecmp(a.fText, b.fText) < 0;
			if (a.fType == eTimeData && b.fType == eTimeData) return a.fTime < b.fTime;
			if (a.fType == eBoolData && b.fType == eBoolData) return a.fBool < b.fBool;
			return kCompareTypes[a.fType][b.fType] < 0;

		case kCmpLE:
			if (a.fType == eNumData && b.fType == eNumData) return a.fDouble <= b.fDouble;
			if (a.fType == eTextData && b.fType == eTextData) return strcasecmp(a.fText, b.fText) <= 0;
			if (a.fType == eTimeData && b.fType == eTimeData) return a.fTime <= b.fTime;
			if (a.fType == eBoolData && b.fType == eBoolData) return a.fBool <= b.fBool;
			return kCompareTypes[a.fType][b.fType] <= 0;

		case kCmpGT:
			if (a.fType == eNumData && b.fType == eNumData) return a.fDouble > b.fDouble;
			if (a.fType == eTextData && b.fType == eTextData) return strcasecmp(a.fText, b.fText) > 0;
			if (a.fType == eTimeData && b.fType == eTimeData) return a.fTime > b.fTime;
			if (a.fType == eBoolData && b.fType == eBoolData) return a.fBool > b.fBool;
			return kCompareTypes[a.fType][b.fType] > 0;

		case kCmpGE:
			if (a.fType == eNumData && b.fType == eNumData) return a.fDouble >= b.fDouble;
			if (a.fType == eTextData && b.fType == eTextData) return strcasecmp(a.fText, b.fText) >= 0;
			if (a.fType == eTimeData && b.fType == eTimeData) return a.fTime >= b.fTime;
			if (a.fType == eBoolData && b.fType == eBoolData) return a.fBool >= b.fBool;
			return kCompareTypes[a.fType][b.fType] >= 0;

		case kCmpEQ:
			if (a.fType == eNumData && b.fType == eNumData) return a.fDouble == b.fDouble;
			if (a.fType == eTextData && b.fType == eTextData) return strcasecmp(a.fText, b.fText) == 0;
			if (a.fType == eTimeData && b.fType == eTimeData) return a.fTime == b.fTime;
			if (a.fType == eBoolData && b.fType == eBoolData) return a.fBool == b.fBool;
			if (a.fType == eNoData && b.fType == eNoData) return true;
			if (a.fType == eNoData) return NoDataEqualsDefault(b);
			if (b.fType == eNoData) return NoDataEqualsDefault(a);
			return kCompareTypes[a.fType][b.fType] == 0;

		default: // kCmpNE
			if (a.fType == eNumData && b.fType == eNumData) return a.fDouble != b.fDouble;
			if (a.fType == eTextData && b.fType == eTextData) return strcasecmp(a.fText, b.fText) != 0;
			if (a.fType == eTimeData && b.fType == eTimeData) return a.fTime != b.fTime;
			if (a.fType == eBoolData && b.fType == eBoolData) return a.fBool != b.fBool;
			if (a.fType == eNoData && b.fType == eNoData) return false;
			if (a.fType == eNoData) return !NoDataEqualsDefault(b);
			if (b.fType == eNoData) return !NoDataEqualsDefault(a);
			return kCompareTypes[a.fType][b.fType] != 0;
	}
}

// Confronto range-scalare o range-range dentro un argomento di
// funzione (es. "FILTER(A2:A8;B2:B8>=20)", Tier 2 di "Path to full
// Excel parity"): costruisce un eBoolArrayData con un booleano PER
// CELLA invece del singolo booleano sbagliato che ScalarCompare da
// solo produrrebbe per un eRangeData (via kCompareTypes, che non sa
// nulla di range). Ambito deliberatamente ristretto (vedi il commento
// in cima al file): solo range 1D (una riga o una colonna), stesso
// numero di celle quando ENTRAMBI gli operandi sono range; un operando
// gia' eBoolArrayData (un confronto su un confronto gia' calcolato)
// degrada a un singolo FALSO invece di un secondo giro di broadcast,
// per non dover estendere kCompareTypes oltre le sue 7 righe/colonne
// attuali. "inContainer" e' il ripiego SOLO quando l'operando range non
// ha un fRangeContainer proprio (stesso principio di
// FunctionUtils::GetRangeContainer).
static Value CompareRangeAware(Value &a, Value &b, CContainer *inContainer, int op)
{
	if (a.fType == eBoolArrayData || b.fType == eBoolArrayData)
		return Value(false);

	bool aIsRange = a.fType == eRangeData, bIsRange = b.fType == eRangeData;

	int count;
	if (aIsRange && bIsRange)
	{
		bool a1D = (a.fRange.left == a.fRange.right) || (a.fRange.top == a.fRange.bottom);
		bool b1D = (b.fRange.left == b.fRange.right) || (b.fRange.top == b.fRange.bottom);
		int countA = (a.fRange.right - a.fRange.left + 1) * (a.fRange.bottom - a.fRange.top + 1);
		int countB = (b.fRange.right - b.fRange.left + 1) * (b.fRange.bottom - b.fRange.top + 1);
		if (!a1D || !b1D || countA != countB)
			return Value(false);
		count = countA;
	}
	else
	{
		_range &r = aIsRange ? a.fRange : b.fRange;
		bool r1D = (r.left == r.right) || (r.top == r.bottom);
		if (!r1D)
			return Value(false);
		count = (r.right - r.left + 1) * (r.bottom - r.top + 1);
	}

	CContainer *aCells = a.fRangeContainer ? a.fRangeContainer : inContainer;
	CContainer *bCells = b.fRangeContainer ? b.fRangeContainer : inContainer;

	bool *resultArray = new bool[count];
	for (int i = 0; i < count; i++)
	{
		Value av, bv;
		if (aIsRange)
		{
			cell c = (a.fRange.left == a.fRange.right)
				? cell(a.fRange.left, a.fRange.top + i) : cell(a.fRange.left + i, a.fRange.top);
			if (aCells) aCells->GetValue(c, av);
		}
		else
			av = a;

		if (bIsRange)
		{
			cell c = (b.fRange.left == b.fRange.right)
				? cell(b.fRange.left, b.fRange.top + i) : cell(b.fRange.left + i, b.fRange.top);
			if (bCells) bCells->GetValue(c, bv);
		}
		else
			bv = b;

		resultArray[i] = ScalarCompare(av, bv, op);
	}

	Value out;
	out.fType = eBoolArrayData;
	out.fArrayCount = count;
	out.fBoolArray = resultArray;
	return out;
}

Value Value::CompareLT(Value &v, CContainer *inContainer)
{
	if (fType == eRangeData || v.fType == eRangeData || fType == eBoolArrayData || v.fType == eBoolArrayData)
		return CompareRangeAware(*this, v, inContainer, kCmpLT);
	return Value(ScalarCompare(*this, v, kCmpLT));
}

Value Value::CompareLE(Value &v, CContainer *inContainer)
{
	if (fType == eRangeData || v.fType == eRangeData || fType == eBoolArrayData || v.fType == eBoolArrayData)
		return CompareRangeAware(*this, v, inContainer, kCmpLE);
	return Value(ScalarCompare(*this, v, kCmpLE));
}

Value Value::CompareGT(Value &v, CContainer *inContainer)
{
	if (fType == eRangeData || v.fType == eRangeData || fType == eBoolArrayData || v.fType == eBoolArrayData)
		return CompareRangeAware(*this, v, inContainer, kCmpGT);
	return Value(ScalarCompare(*this, v, kCmpGT));
}

Value Value::CompareGE(Value &v, CContainer *inContainer)
{
	if (fType == eRangeData || v.fType == eRangeData || fType == eBoolArrayData || v.fType == eBoolArrayData)
		return CompareRangeAware(*this, v, inContainer, kCmpGE);
	return Value(ScalarCompare(*this, v, kCmpGE));
}

Value Value::CompareEQ(Value &v, CContainer *inContainer)
{
	if (fType == eRangeData || v.fType == eRangeData || fType == eBoolArrayData || v.fType == eBoolArrayData)
		return CompareRangeAware(*this, v, inContainer, kCmpEQ);
	return Value(ScalarCompare(*this, v, kCmpEQ));
}

Value Value::CompareNE(Value &v, CContainer *inContainer)
{
	if (fType == eRangeData || v.fType == eRangeData || fType == eBoolArrayData || v.fType == eBoolArrayData)
		return CompareRangeAware(*this, v, inContainer, kCmpNE);
	return Value(ScalarCompare(*this, v, kCmpNE));
}

void Value::operator=(const char *inString)
{
	Clear();
	fType = eTextData;
	fText = STRDUP(inString);
	fTextIsCopy = true;
	fRangeContainer = NULL;
}

void Value::operator=(const Value &inValue)
{
	Clear();
	if ((fType = inValue.fType) == eTextData)
	{
		fText = STRDUP(inValue.fText);
		fTextIsCopy = true;
	}
	else if (fType == eBoolArrayData)
	{
		fArrayCount = inValue.fArrayCount;
		fBoolArray = inValue.fBoolArray ? new bool[fArrayCount] : NULL;
		if (fBoolArray)
			std::copy(inValue.fBoolArray, inValue.fBoolArray + fArrayCount, fBoolArray);
	}
	else
		fDouble = inValue.fDouble;
	// Fase 15: va copiato esplicitamente, non fa parte dell'union sopra
	// (fDouble/fRange condividono la stessa memoria, fRangeContainer no)
	// -- senza questo, un range risolto su un altro foglio (vedi
	// Formula.cpp, caso valName) perdeva quell'informazione ogni volta
	// che veniva copiato da uno slot all'altro dello stack di calcolo
	// (es. "stack[0] = stack[i];" in IFS/IFERROR).
	fRangeContainer = inValue.fRangeContainer;
}

void Value::operator=(const double d)
{
	Clear();
	fType = eNumData;
	fDouble = d;
	fRangeContainer = NULL;
}

void Value::operator=(const bool b)
{
	Clear();
	fType = eBoolData;
	fBool = b;
	fRangeContainer = NULL;
}

void Value::operator=(const time_t& t)
{
	Clear();
	fType = eTimeData;
	fTime = t;
	fRangeContainer = NULL;
}

void Value::operator=(const range& r)
{
	Clear();
	fType = eRangeData;
	fRange = r;
	// Locale di default (lo stesso CContainer che il chiamante gia' usa
	// per leggere le celle) -- chi risolve un riferimento a tabella su
	// un ALTRO foglio (Formula.cpp, caso valName) lo imposta esplicitamente
	// SUBITO DOPO questa assegnazione, vedi il commento su fRangeContainer
	// in Value.h.
	fRangeContainer = NULL;
}

void Value::Clear()
{
	// fType andava resettato anche lui, non solo fText: senza questo,
	// una Value gia' a eTextData (es. il risultato di una funzione
	// annidata, come CONCAT dentro IFERROR) restava "fType==eTextData"
	// con "fText==NULL" dopo Clear() -- non "nessun valore" come il
	// nome del metodo promette, ma un valore TESTO fasullo con un
	// puntatore nullo. CellData::operator=(Value&) (Container.cpp)
	// prende quel fType alla lettera e chiama STRDUP(NULL), che
	// restituisce NULL: FailNil() lancia errInsufficientMemory subito
	// dopo -- un crash che sembrava scollegato dalla causa reale,
	// scoperto su un file XLSX reale con "=IFERROR(CONCAT(...);"")"
	// (IFFunction/IFERRFunction chiamano proprio Clear() quando nessun
	// ramo corrisponde, vedi Functions.spreadsheet.cpp).
	if (fType == eTextData && fText && fTextIsCopy)
	{
		FREE(fText);
		fText = NULL;
	}
	else if (fType == eBoolArrayData && fBoolArray)
	{
		delete[] fBoolArray;
		fBoolArray = NULL;
	}
	fType = eNoData;
	fTextIsCopy = false;
	fRangeContainer = NULL;
	fArrayCount = 0;
}
