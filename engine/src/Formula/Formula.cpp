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
	Formula.c

	Copyright 1997, Hekkelman Programmatuur
	Portions Copyright 2026 Andrea Bernardi

	Part of Sum-It for the BeBox version 1.1.

*/

#include <algorithm>
#include <cstdio>

#ifndef   FORMULA_H
#include "Formula.h"
#endif

#ifndef   CONTAINER_H
#include "Container.h"
#endif

#ifndef   FUNCTIONUTILS_H
#include "FunctionUtils.h"
#endif

#ifndef   FORMATTER_H
#include "Formatter.h"
#endif

//#ifndef   MYRESOURCES_H
//#include "MyResources.h"
//#endif

#ifndef   MYERROR_H
#include "MyError.h"
#endif

#include <support/Debug.h>
#ifndef   NAMETABLE_H
#include "NameTable.h"
#endif

#ifndef   SET_H
#include "Set.h"
#endif

#ifndef   CELLVIEW_H
#endif

#ifndef   GLOBALS_H
#include "Globals.h"
#endif

#ifndef   STRINGTABLE_H
#include "StringTable.h"
#endif

#ifndef   CONFIG_H
#include "Config.h"
#endif


const size_t kBuildBufferSize = 1000;

// globals
bool gWithEqualSign;

enum Precedence {
	eAddition, eRoot, eMultiplication, eRaising, eNumber,
	eLogic, eAnd, eOr, eNot
};

CFormula::CFormula()
{
	fString = NULL;
} /* CFormula::CFormula */

CFormula::CFormula(void *inString)
{
	fString = (int32 *)inString;
} /* CFormula::CFormula */

void CFormula::AddToken(PFToken inToken, const void *inData, int& ioOffset)
{
	if (!fString)
	{
		ioOffset = 0;
		fString = (int32 *)MALLOC(sizeof(int32) * kBuildBufferSize);
		FailNil(fString);
	}
	
	int indx, toAdd;
	indx = ioOffset / kPFWordSize;
	
	switch (inToken)
	{
		case opFunc:
			toAdd = sizeof(FuncCallData);
			break;
		case valBool:
			toAdd = sizeof(bool);
			break;
		case valNum:
		case valPerc:
			toAdd = sizeof(double);
			break;
		case valTime:
			toAdd = sizeof(time_t);
			break;
		case valName:
		case valStr:
			toAdd = strlen((const char *)inData) + 1;
			break;
		case valCell:
			toAdd = sizeof(cell);
			break;
		case valRange:
		case valRefRange:
			toAdd = sizeof(range);
			break;
		// Nome del foglio (NUL-terminato, come valName/valStr sopra)
		// seguito dalla cell/range vera e propria: strlen si ferma
		// correttamente al NUL del nome, i byte della cell/range dopo
		// non vengono mai considerati parte della stringa.
		case valXRef:
			toAdd = strlen((const char *)inData) + 1 + sizeof(cell);
			break;
		case valXRange:
			toAdd = strlen((const char *)inData) + 1 + sizeof(range);
			break;
		default:
			toAdd = 0;
			break;
	}

	if (toAdd & kPFAlignBits) /* We houden alles 'word aligned'*/
		toAdd = (toAdd & ~kPFAlignBits) + kPFWordSize;

	ioOffset += toAdd + kPFWordSize;

	fString[indx] = inToken;
	indx++;

	if (toAdd && (inData != NULL) )
		memcpy(fString + indx, inData, toAdd);
	else if (toAdd)
		memset(fString + indx, 0, toAdd);
} /* CFormula::AddToken */

// Same number formatting as CONCATFunction (Functions.text.cpp) --
// plain snprintf("%.10g"), not the graphical formatter/ftoa, since
// Calculate() must stay usable without a real BApplication (headless
// engine tests, translators): ftoa calls BFont::StringWidth, which
// needs an app_server connection and hangs forever without one.
static void ValueToConcatText(const Value &v, char *out, size_t outSize)
{
	switch (v.fType)
	{
		case eTextData:
			strlcpy(out, v.fText, outSize);
			break;
		case eNumData:
			snprintf(out, outSize, "%.10g", v.fDouble);
			break;
		case eBoolData:
			strlcpy(out, v.fBool ? "TRUE" : "FALSE", outSize);
			break;
		default:
			out[0] = 0;
			break;
	}
}

void CFormula::Calculate(cell inLocation, Value& outResult, CContainer *inContainer) const
{
	PFToken nextOpcode;
	Value stack[kMaxStackHeight];
	int stackIndx, indx, sLen;
	cell theCell;
	range theRange;
	FuncCallData theFuncData;

	if (!fString)
	{
		outResult.fType = eNoData;
		return;
	}

	stackIndx = -1;
	indx = 0;

	while ((nextOpcode = (PFToken)fString[indx++]) != opEnd)
	{
		switch(nextOpcode)
		{
			case opLT:
				if (stack[stackIndx - 1].IsNan())	{}
				else if (stack[stackIndx].IsNan())	stack[stackIndx - 1] = stack[stackIndx];
				else	stack[stackIndx - 1] = stack[stackIndx - 1] < stack[stackIndx];
				stackIndx--;
				break;

			case opLE:
				if (stack[stackIndx - 1].IsNan())	{}
				else if (stack[stackIndx].IsNan())	stack[stackIndx - 1] = stack[stackIndx];
				else	stack[stackIndx - 1] = stack[stackIndx - 1] <= stack[stackIndx];
				stackIndx--;
				break;

			case opEQ:
				if (stack[stackIndx - 1].IsNan())	{}
				else if (stack[stackIndx].IsNan())	stack[stackIndx - 1] = stack[stackIndx];
				else	stack[stackIndx - 1] = stack[stackIndx - 1] == stack[stackIndx];
				stackIndx--;
				break;

			case opGE:
				if (stack[stackIndx - 1].IsNan())	{}
				else if (stack[stackIndx].IsNan())	stack[stackIndx - 1] = stack[stackIndx];
				else	stack[stackIndx - 1] = stack[stackIndx - 1] >= stack[stackIndx];
				stackIndx--;
				break;

			case opGT:
				if (stack[stackIndx - 1].IsNan())	{}
				else if (stack[stackIndx].IsNan())	stack[stackIndx - 1] = stack[stackIndx];
				else	stack[stackIndx - 1] = stack[stackIndx - 1] > stack[stackIndx];
				stackIndx--;
				break;

			case opNE:
				if (stack[stackIndx - 1].IsNan())	{}
				else if (stack[stackIndx].IsNan())	stack[stackIndx - 1] = stack[stackIndx];
				else	stack[stackIndx - 1] = stack[stackIndx - 1] != stack[stackIndx];
				stackIndx--;
				break;

			case opRaise:
				stack[stackIndx - 1] ^= stack[stackIndx];
				stackIndx--;
				break;

			case opMul:
				stack[stackIndx - 1] *= stack[stackIndx];
				stackIndx--;
				break;

			case opDiv:
				stack[stackIndx - 1] /= stack[stackIndx];
				stackIndx--;
				break;

			case opPlus:
				stack[stackIndx - 1] += stack[stackIndx];
				stackIndx--;
				break;

			case opMinus:
				stack[stackIndx - 1] -= stack[stackIndx];
				stackIndx--;
				break;

			case opNegate:
				if (stack[stackIndx].fType == eNumData)
					stack[stackIndx] = -stack[stackIndx].fDouble;
				else
					stack[stackIndx] = gRefNan;
				break;

			// Operatore percentuale postfisso di Excel (es. "F37%",
			// diverso dal FORMATO valuta/percentuale di una cella --
			// qui e' l'operatore usato dentro una formula, come "*" o
			// "/"): divide per 100 il valore in cima allo stack. Il
			// ptg BIFF corrispondente (ptgPercent) era un case vuoto
			// in Excel.formula.cpp, ereditato incompleto dal codice
			// BIFF5 originale -- bug reale scoperto confrontando con
			// Excel vero una formula reale ("=D37*F37%" impaginata
			// come "D37*F37" nella fattura di prova): il risultato
			// era 100 volte troppo grande (5250 invece di 52,5)
			// perche' l'operatore veniva silenziosamente ignorato.
			case opPercent:
				if (stack[stackIndx].fType == eNumData)
					stack[stackIndx] = stack[stackIndx].fDouble / 100.0;
				else
					stack[stackIndx] = gRefNan;
				break;

			case opAND:
				stack[stackIndx - 1] &= stack[stackIndx];
				stackIndx--;
				break;

			case opOR:
				stack[stackIndx - 1] |= stack[stackIndx];
				stackIndx--;
				break;

			// Excel's "&", text concatenation (see the comment on
			// opConcat in Formula.h). ValueToConcatText mirrors
			// CONCATFunction's own number formatting (Functions.text.cpp)
			// so "1"&"2" and CONCAT(1,2) produce the same text. An error
			// (modeled in this engine as a NaN eNumData, there's no
			// separate error type) propagates instead of stringifying
			// to the literal text "nan" -- matches real Excel, where
			// concatenating an error value with anything just gives
			// back that same error. Caught by an existing table-refs
			// test using the common "X&\"\"" idiom to coerce a NaN/
			// unresolved reference to an empty string.
			case opConcat:
				if (stack[stackIndx - 1].IsNan() || stack[stackIndx].IsNan())
					stack[stackIndx - 1] = gValueNan;
				else
				{
					char buf1[2048], buf2[2048], result[4096];
					ValueToConcatText(stack[stackIndx - 1], buf1, sizeof(buf1));
					ValueToConcatText(stack[stackIndx], buf2, sizeof(buf2));
					strlcpy(result, buf1, sizeof(result));
					strlcat(result, buf2, sizeof(result));
					stack[stackIndx - 1] = result;
				}
				stackIndx--;
				break;

			case opNOT:
				if (stack[stackIndx].fType == eBoolData)
					stack[stackIndx] = (bool)!stack[stackIndx].fBool;
				else
					stack[stackIndx] = gRefNan;
				break;

			case opFunc:
				theFuncData = *((FuncCallData *)(fString + indx));
				indx += sizeof(FuncCallData) / kPFWordSize;

				stackIndx -= theFuncData.argCnt - 1;
				if (stackIndx < 0)
				{
					char s[32];
					inLocation.GetName(s);
/**/					strcat(s, "(f)");
					THROW((errIllPFString, s));
				}
				if (theFuncData.funcNr >= 0 &&
					theFuncData.funcNr < gFuncCount &&
					gFuncs[theFuncData.funcNr])
					try
					{
						(gFuncs[theFuncData.funcNr])
							(stack + stackIndx, theFuncData.argCnt, inContainer);
					}
					catch(...)
					{
						CATCHED;
						stack[stackIndx] = gFuncNan;
					}
				else
					stack[stackIndx] = gFuncNan;
				break;

			case valNum:
			case valPerc:
				stackIndx++;
				stack[stackIndx] = *((double *)(fString + indx));
				indx += sizeof(double) / kPFWordSize;
				break;
			
			case valTime:
				stackIndx++;
				stack[stackIndx] = *((time_t *)(fString + indx));
				indx += sizeof(time_t) / kPFWordSize;
				break;

			case valStr:
				stackIndx++;
				stack[stackIndx] = (char *)(fString + indx);
				sLen = strlen(stack[stackIndx].fText) + 1;
				if (sLen & kPFAlignBits)
					sLen = (sLen & ~kPFAlignBits) + kPFWordSize;
				indx += sLen / kPFWordSize;
				break;

			case valCell:
				stackIndx++;
				theCell = *((cell *)(fString + indx));
				indx += sizeof(cell) / kPFWordSize;
				inContainer->GetValue(theCell.GetFlatCell(inLocation), stack[stackIndx]);
				break;

			case valRange:
				stackIndx++;
				theRange = *((range *)(fString + indx));

				if (theRange.TopLeft() == theRange.BotRight())
					// Bug reale (Fase 16, scoperto sistemando gli
					// argomenti-intervallo fra fogli): TopLeft() qui
					// e' ancora l'offset relativo grezzo del
					// bytecode, non una cella assoluta -- serve
					// GetFlatCell(inLocation), esattamente come
					// valCell sopra, altrimenti un intervallo
					// "degenere" a una sola cella (es. "A1:A1", che
					// puo' capitare scritto direttamente o prodotto
					// da un'altra funzione) legge una cella
					// completamente sbagliata invece di quella vera.
					inContainer->GetValue(theRange.TopLeft().GetFlatCell(inLocation), stack[stackIndx]);
				else
				{
					stack[stackIndx].fType = eRangeData;
					stack[stackIndx] = theRange.GetFlatRange(inLocation);
				}
				indx += sizeof(range) / kPFWordSize;
				break;

			// Intervalli dinamici: vedi il commento su valRefRange/
			// opRangeOp in Formula.h. A differenza di valRange sopra,
			// NON collassa mai al valore della cella per un intervallo
			// di una sola cella -- e' l'unico modo di rappresentare
			// oggi "un riferimento", non "un valore", nel bytecode.
			case valRefRange:
				stackIndx++;
				theRange = *((range *)(fString + indx));
				stack[stackIndx].fType = eRangeData;
				stack[stackIndx] = theRange.GetFlatRange(inLocation);
				indx += sizeof(range) / kPFWordSize;
				break;

			// Operatore ":" applicato a due espressioni qualunque che
			// producono un riferimento (non solo due celle letterali,
			// gia' risolto a tempo di parsing come valRange sopra) --
			// es. "OFFSET(H11,1,0):OFFSET(H15,-1,0)". Il risultato e' il
			// rettangolo che contiene ENTRAMBI gli operandi (lo stesso
			// calcolo del vero operatore ":" di Excel, non una
			// concatenazione letterale). Se uno dei due operandi non e'
			// davvero un riferimento (es. "=SUM(1:2)", un errore
			// dell'utente), il risultato e' l'errore sentinella gia'
			// usato ovunque in questo file per un input non valido, non
			// un crash ne' una lettura di memoria a caso.
			case opRangeOp:
				if (stack[stackIndx - 1].fType != eRangeData || stack[stackIndx].fType != eRangeData)
					stack[stackIndx - 1] = gRefNan;
				else
				{
					range left = stack[stackIndx - 1];
					range right = stack[stackIndx];
					range result;
					result.left = std::min(left.left, right.left);
					result.top = std::min(left.top, right.top);
					result.right = std::max(left.right, right.right);
					result.bottom = std::max(left.bottom, right.bottom);
					stack[stackIndx - 1] = result;
				}
				stackIndx--;
				break;

			case valBool:
				stackIndx++;
				stack[stackIndx] = *((bool *)(fString + indx));
				indx++;
				break;
			
			case valName:
			{
				stackIndx++;
				char *s = (char *)(fString + indx);
				sLen = strlen(s) + 1;
				if (sLen & kPFAlignBits)
					sLen = (sLen & ~kPFAlignBits) + kPFWordSize;
				indx += sLen / kPFWordSize;
				
				try
				{
					FailNil(inContainer);
					CContainer *owner = inContainer;
					stack[stackIndx] = inContainer->ResolveName(s, inLocation, &owner);
					range r = stack[stackIndx];
					if (r.TopLeft() == r.BotRight())
						owner->GetValue(r.TopLeft(), stack[stackIndx]);
					else if (owner != inContainer)
						// Riferimento a tabella strutturata risolto su un
						// ALTRO foglio (Fase 15, vedi CContainer::
						// ResolveName): un intervallo multi-cella vero
						// (colonna di piu' righe, consumato da una
						// funzione come XLOOKUP/VLOOKUP/MATCH/INDEX, vedi
						// FunctionUtils::GetRangeContainer) deve restare
						// legato al documento che possiede DAVVERO quelle
						// celle, non a quello in cui vive questa formula.
						stack[stackIndx].fRangeContainer = owner;
				}
				catch(CErr& e)
				{
					CATCHED;
					stack[stackIndx] = gNameNan;
				}
				break;
			}
			
			case valNil:
				stackIndx++;
				stack[stackIndx].fType = eNoData;
				break;

			// Fase 9: "NomeFoglio!Cella"/"NomeFoglio!Cella:Cella" -- il
			// nome del foglio (non un indice, vedi Formula.h) si
			// risolve QUI, in fase di calcolo, mai in fase di parsing:
			// GetSheetResolver() e' NULL per un documento a un solo
			// foglio (mai collegato a una cartella multi-foglio), o il
			// nome non corrisponde a nessun foglio (piu') esistente --
			// in entrambi i casi il riferimento resta semplicemente
			// non risolto (eNoData), mai un crash, esattamente come un
			// CalcCell che referenzia una cella vuota.
			case valXRef:
			{
				stackIndx++;
				const char *sheetName = (const char *)(fString + indx);
				size_t nameLen = strlen(sheetName) + 1;
				cell target;
				memcpy(&target, sheetName + nameLen, sizeof(cell));
				size_t totalBytes = nameLen + sizeof(cell);
				if (totalBytes & kPFAlignBits)
					totalBytes = (totalBytes & ~kPFAlignBits) + kPFWordSize;
				indx += totalBytes / kPFWordSize;

				CContainer *sheet = inContainer && inContainer->GetSheetResolver()
					? inContainer->GetSheetResolver()->ResolveSheetByName(sheetName) : NULL;

				if (sheet)
					sheet->GetValue(target.GetFlatCell(inLocation), stack[stackIndx]);
				else
					stack[stackIndx].fType = eNoData;
				break;
			}

			case valXRange:
			{
				stackIndx++;
				const char *sheetName = (const char *)(fString + indx);
				size_t nameLen = strlen(sheetName) + 1;
				range target;
				memcpy(&target, sheetName + nameLen, sizeof(range));
				size_t totalBytes = nameLen + sizeof(range);
				if (totalBytes & kPFAlignBits)
					totalBytes = (totalBytes & ~kPFAlignBits) + kPFWordSize;
				indx += totalBytes / kPFWordSize;

				CContainer *sheet = inContainer && inContainer->GetSheetResolver()
					? inContainer->GetSheetResolver()->ResolveSheetByName(sheetName) : NULL;

				if (!sheet)
					stack[stackIndx].fType = eNoData;
				else if (target.TopLeft() == target.BotRight())
					// Un solo punto (es. "Foglio!A1:A1", che Excel
					// normalizza comunque a un riferimento singolo):
					// stesso trattamento di valXRef sopra --
					// GetFlatCell(inLocation) e' necessario, vedi il
					// commento sul bug analogo nel caso valRange sopra.
					sheet->GetValue(target.TopLeft().GetFlatCell(inLocation), stack[stackIndx]);
				else
				{
					// Un vero intervallo multi-cella fra fogli (Fase
					// 16, es. "SUM(Foglio!A1:A3)" o
					// "MATCH(x,Foglio!A:A,0)"): stesso meccanismo gia'
					// in uso per un riferimento a tabella strutturata
					// risolto su un altro foglio (vedi il caso
					// valName sopra e il commento su
					// Value::fRangeContainer in Value.h) -- le
					// funzioni che sanno gia' leggere fRangeContainer
					// (FunctionUtils::GetRangeContainer, usato da
					// XLOOKUP/VLOOKUP/HLOOKUP/MATCH/INDEX/SUM/AVG/
					// COUNT/...) leggono le celle vere dal foglio
					// giusto invece che da quello in cui vive questa
					// formula. Prima di questo cambiamento restava
					// sempre eNoData (bug reale scoperto sistemando i
					// riferimenti a colonna intera fra fogli, vedi
					// memoria project_xlsx_formula_gaps_20260808).
					stack[stackIndx] = target.GetFlatRange(inLocation);
					stack[stackIndx].fRangeContainer = sheet;
				}
				break;
			}

			case opParen:
				break;

			default:
			{
				char s[32];
				inLocation.GetName(s);
				strcat(s, "(t)");
				THROW((errIllPFString, s));
			}
		}
	}

	if (stackIndx != 0)
	{
		char s[32];
		inLocation.GetName(s);
		strcat(s, "(i)");
		THROW((errIllPFString, s));
	}

	outResult = stack[0];
} /* CFormula::Calculate */

void CFormula::UnMangle(char *outString, cell inLocation, CContainer *inContainer, bool rcStyle,
	char decSepOverride, char listSepOverride, bool odfRefs) const
{
	char *stack[kMaxStackHeight];
	int stackIndx, indx, sLen, i;
	PFToken nextOpcode;
	double d;
	cell theCell;
	range theRange;
	FuncCallData theFuncData;

	void *p = MALLOC(kMaxStackHeight * kMaxStringLength);
	FailNil(p);

	for (int i = 0; i < kMaxStackHeight; i++)
		stack[i] = (char *)p + i * kMaxStringLength;

	stackIndx = -1;
	indx = 0;

	try
	{
		while ((nextOpcode = (PFToken)fString[indx++]) != opEnd)
		{
			switch(nextOpcode)
			{
				case opLT:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "<", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opLE:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "<=", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opEQ:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "=", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opGE:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, ">=", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opGT:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, ">", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opNE:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "<>", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
				
				case opNOT:
					strlcpy(outString, "!", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					break;
				
				// Reprinted as AND(a,b)/OR(a,b), not infix "a&b"/"a|b" as
				// before: "&"/"|" now mean text concatenation/opConcat
				// (see Formula.h) for any NEW formula text, so an old
				// opAND/opOR formula's calculation stays exactly as it
				// was (this case is unchanged), but its reprinted text
				// must no longer use the symbol that now means something
				// else -- otherwise editing and reparsing it would
				// silently flip it into a concatenation.
				case opAND:
				{
					char s[2];
					if (listSepOverride) s[0] = listSepOverride;
					else if (rcStyle) s[0] = ',';
					else s[0] = gListSeparator;
					s[1] = 0;
					strlcpy(outString, "AND(", kMaxStringLength);
					strlcat(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, s, kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcat(outString, ")", kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
				}

				case opOR:
				{
					char s[2];
					if (listSepOverride) s[0] = listSepOverride;
					else if (rcStyle) s[0] = ',';
					else s[0] = gListSeparator;
					s[1] = 0;
					strlcpy(outString, "OR(", kMaxStringLength);
					strlcat(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, s, kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcat(outString, ")", kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
				}

				case opConcat:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "&", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opRaise:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "^", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opMul:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "*", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opDiv:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "/", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opPlus:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "+", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;

				// "OFFSET(H11,1,0):OFFSET(H15,-1,0)": stesso schema di
				// opPlus sopra, ":" al posto di "+".
				case opRangeOp:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, ":", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;

				case opMinus:
					strlcpy(outString, stack[stackIndx - 1], kMaxStringLength);
					strlcat(outString, "-", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx - 1], outString, kMaxStringLength);
					stackIndx--;
					break;
	
				case opNegate:
					strlcpy(outString, "-", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					break;

				case opPercent:
					strlcpy(outString, stack[stackIndx], kMaxStringLength);
					strlcat(outString, "%", kMaxStringLength);
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					break;

				case opRoot:
					strlcpy(outString, "√", kMaxStringLength);
					strlcat(outString, stack[stackIndx], kMaxStringLength);
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					break;
	
				case opParen:
					sLen = strlen(stack[stackIndx]);
					memmove(stack[stackIndx]+1, stack[stackIndx], sLen);
					stack[stackIndx][0] = '(';
					stack[stackIndx][sLen+1] = ')';
					stack[stackIndx][sLen+2] = 0;
					break;
				
				case opFunc:
				{
					theFuncData = *((FuncCallData *)(fString + indx));
					indx += sizeof(FuncCallData) / kPFWordSize;

					if (theFuncData.funcNr >= 0 &&
						theFuncData.funcNr < gFuncCount)
						strlcpy(outString, gFuncArrayByNr[theFuncData.funcNr].funcName, kMaxStringLength);
					else
						snprintf(outString, kMaxStringLength, GetIndString(1, 19), theFuncData.funcNr);

					stackIndx -= theFuncData.argCnt - 1;

						// cannot guarantee that theFuncData.argCnt is always correct anymore
						// since we now build formula's from Excel formula's and those, yeah, well.. Microsoft huh?
					if (stackIndx < 0)
					{
						char name[32];
						inLocation.GetName(name);
						THROW((errIllPFString, name));
					}

					strlcat(outString, "(", kMaxStringLength);
					for(i = 0; i < theFuncData.argCnt; i++)
					{
						if (i)
						{
							char s[2];
							if (listSepOverride)
								s[0] = listSepOverride;
							else if (rcStyle)
								s[0] = ',';
							else
								s[0] = gListSeparator;
							s[1] = 0;
							strlcat(outString, s, kMaxStringLength);
						}
						strlcat(outString, stack[stackIndx + i], kMaxStringLength);
					}
					strlcat(outString, ")", kMaxStringLength);

					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					break;
				}
	
				case valNum:
				case valPerc:
					d = *((double *)(fString + indx));
					indx += sizeof(double) / kPFWordSize;

					if (nextOpcode == valPerc)
						d *= 100;

					ftoa(d, outString);

					{
						char targetDecimal = decSepOverride ? decSepOverride : (rcStyle ? '.' : 0);
						if (targetDecimal && targetDecimal != gDecimalPoint)
						{
							char *dp = strchr(outString, gDecimalPoint);
							if (dp) *dp = targetDecimal;
						}
					}
	
					if (nextOpcode == valPerc)
						strlcat(outString, "%", kMaxStringLength);
	
					stackIndx++;
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					break;
	
				case valTime:
				{
					Value t = *((time_t *)(fString + indx));
					indx += sizeof(time_t) / kPFWordSize;
					
					stackIndx++;
					gFormatTable.FormatValue(0, t, stack[stackIndx]);
					break;
				}
	
				case valStr:
					stackIndx++;
					strlcpy(stack[stackIndx], "\"", kMaxStringLength);
					strlcat(stack[stackIndx], (char *)(fString + indx), kMaxStringLength);
					strlcat(stack[stackIndx], "\"", kMaxStringLength);
					sLen = 1 + strlen((char *)(fString + indx));
					if (sLen & kPFAlignBits)
						sLen = (sLen & ~kPFAlignBits) + kPFWordSize;
					indx += sLen / kPFWordSize;
					break;
	
				case valCell:
					theCell = *((cell *)(fString + indx));
					if (rcStyle)
						theCell.GetRCName(outString);
					else
					{
						theCell.GetFormulaName(outString, inLocation);
						if (odfRefs)
						{
							char wrapped[kMaxStringLength];
							snprintf(wrapped, kMaxStringLength, "[.%s]", outString);
							strlcpy(outString, wrapped, kMaxStringLength);
						}
					}
					stackIndx++;
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					indx += sizeof(cell) / kPFWordSize;
					break;

				// valRefRange stampa esattamente come valRange (la sola
				// differenza fra i due token e' a tempo di CALCOLO, vedi
				// Calculate sopra -- il testo di una formula non
				// distingue affatto i due casi, "H11" resta "H11" che
				// sia un riferimento normale o l'argomento di OFFSET).
				case valRange:
				case valRefRange:
					theRange = *((range *)(fString + indx));
					if (rcStyle)
						theRange.GetRCName(outString);
					else
					{
						theRange.GetFormulaName(outString, inLocation);
						if (decSepOverride)
						{
							// Normalizza il separatore di intervallo per
							// l'export verso formati esterni (Excel/ODF
							// vogliono ":", il nostro ".." storico e'
							// pensato solo per il giro testuale interno
							// ASCD/barra formule -- vedi il commento sul
							// token RANGE nel lexer, che accetta entrambi
							// in ingresso). L'uscita e' sempre uguale o
							// piu' corta dell'ingresso (".." di 2
							// caratteri diventa ":" di 1), quindi riscrivere
							// nello stesso buffer scorrendolo una volta
							// sola e' sicuro.
							char *src = outString, *dst = outString;
							while (*src)
							{
								if (src[0] == '.' && src[1] == '.')
								{
									*dst++ = ':';
									src += 2;
								}
								else
									*dst++ = *src++;
							}
							*dst = 0;
						}
						if (odfRefs)
						{
							char wrapped[kMaxStringLength];
							snprintf(wrapped, kMaxStringLength, "[.%s]", outString);
							strlcpy(outString, wrapped, kMaxStringLength);
						}
					}
					stackIndx++;
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					indx += sizeof(range) / kPFWordSize;
					break;

				case valName:
					stackIndx++;
					strlcpy(stack[stackIndx], (char *)(fString + indx), kMaxStringLength);
					sLen = 1 + strlen((char *)(fString + indx));
					if (sLen & kPFAlignBits)
						sLen = (sLen & ~kPFAlignBits) + kPFWordSize;
					indx += sLen / kPFWordSize;
					break;

				case valXRef:
				{
					const char *sheetName = (const char *)(fString + indx);
					size_t nameLen = strlen(sheetName) + 1;
					cell target;
					memcpy(&target, sheetName + nameLen, sizeof(cell));
					size_t totalBytes = nameLen + sizeof(cell);
					if (totalBytes & kPFAlignBits)
						totalBytes = (totalBytes & ~kPFAlignBits) + kPFWordSize;
					indx += totalBytes / kPFWordSize;

					char cellName[64];
					if (rcStyle)
						target.GetRCName(cellName);
					else
						target.GetFormulaName(cellName, inLocation);

					// Sempre fra apici (anche se il nome non le richiederebbe,
					// es. senza spazi): il bytecode non registra se era stato
					// scritto con o senza, e senza le virgolette un nome con
					// spazi/trattini non si rianalizzerebbe correttamente
					// (vedi QIDENT in parser.h/.cpp) -- fondamentale per il
					// giro testuale di AscdIO::SaveASCD/LoadASCD (UnMangle poi
					// TryToParseString), non solo per la barra formule.
					snprintf(outString, kMaxStringLength, "'%s'!%s", sheetName, cellName);
					stackIndx++;
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					break;
				}

				case valXRange:
				{
					const char *sheetName = (const char *)(fString + indx);
					size_t nameLen = strlen(sheetName) + 1;
					range target;
					memcpy(&target, sheetName + nameLen, sizeof(range));
					size_t totalBytes = nameLen + sizeof(range);
					if (totalBytes & kPFAlignBits)
						totalBytes = (totalBytes & ~kPFAlignBits) + kPFWordSize;
					indx += totalBytes / kPFWordSize;

					char rangeName[128];
					if (rcStyle)
						target.GetRCName(rangeName);
					else
						target.GetFormulaName(rangeName, inLocation);

					snprintf(outString, kMaxStringLength, "'%s'!%s", sheetName, rangeName);
					stackIndx++;
					strlcpy(stack[stackIndx], outString, kMaxStringLength);
					break;
				}

				case valNil:
					stackIndx++;
					stack[stackIndx][0] = 0;
					break;
				
				case valBool:
					stackIndx++;
					if (*(bool *)(fString + indx))
						strcpy(stack[stackIndx], gTrueString);
					else
						strcpy(stack[stackIndx], gFalseString);
					indx++;
					break;
	
				default:
				{
					char name[32];
					inLocation.GetName(name);
					THROW((errIllPFString, name));
				}
			}
		}
	
		if (stackIndx != 0)
		{
			char name[32];
			inLocation.GetName(name);
			THROW((errIllPFString, name));
		}
	
		if (gWithEqualSign && !rcStyle)
		{
			strlcpy(outString, "=", kMaxStringLength);
			strlcat(outString, stack[0], kMaxStringLength);
		}
		else
			strlcpy(outString, stack[0], kMaxStringLength);
	}
	catch (CErr& e)
	{
		strlcpy(outString, e, kMaxStringLength);
	}
	
	FREE(p);
} /* CFormula::UnMangle */

bool CFormula::ReferencesOtherSheet() const
{
	// Scansione del bytecode simile a CFormulaIterator::Next (vedi
	// Formula.iter.cpp), ma senza bisogno di ricostruire nessun
	// riferimento: basta sapere se un valXRef/valXRange compare da
	// qualche parte, per questo appena trovato si esce subito senza
	// nemmeno saltarne il payload.
	if (!fString)
		return false;

	int indx = 0;
	int l;
	PFToken opcode;

	while ((opcode = (PFToken)fString[indx++]) != opEnd)
	{
		switch (opcode)
		{
			case valXRef:
			case valXRange:
				return true;

			case opFunc:
				indx += sizeof(FuncCallData) / kPFWordSize;
				break;

			case valNum:
			case valPerc:
				indx += sizeof(double) / kPFWordSize;
				break;

			case valTime:
				indx += sizeof(time_t) / kPFWordSize;
				break;

			case valBool:
				indx++;
				break;

			case valStr:
			case valName:
				l = 1 + strlen((char *)(fString + indx));
				if (l & kPFAlignBits)
					l = (l & ~kPFAlignBits) + kPFWordSize;
				indx += l / kPFWordSize;
				break;

			case valCell:
				indx += sizeof(cell) / kPFWordSize;
				break;

			case valRange:
			case valRefRange:
				indx += sizeof(range) / kPFWordSize;
				break;

			default:
				break;
		}
	}
	return false;
} /* CFormula::ReferencesOtherSheet */

bool CFormula::IsConstant() const
{
	if (!fString)
		return true;

	PFToken theOpcode;
	int indx = 0, l;
	
	do {
		theOpcode = (PFToken)fString[indx];
		indx++;

		switch(theOpcode) {
			case opFunc:	return false;
			case valCell:	return false;
			case valRange:return false;
			case valRefRange:return false;
			case valXRef:	return false;
			case valXRange:	return false;
			// Fase 7: un nome, come un riferimento a cella, non e'
			// mai davvero "costante" -- il suo valore puo' cambiare
			// ridefinendolo (finestra Intervalli con nome), quindi la
			// formula deve restare viva (Calculate() a ogni
			// ricalcolo) invece di "congelarsi" al valore risolto la
			// prima volta che viene analizzata. Bug preesistente ma
			// mai raggiungibile prima di questo lavoro: valName
			// veniva emesso dal parser solo se GetOwner()->
			// IsNamedRange() era vero, sempre falso nella UI reale
			// (CCellView/EngineViewStub, mai collegata) -- quindi
			// nessuna formula reale conteneva mai un valName da
			// verificare qui.
			case valName:	return false;
			case valNum:
			case valPerc:
				indx += sizeof(double) / kPFWordSize;
				break;
			case valTime:
				indx += sizeof(time_t) / kPFWordSize;
				break;
			case valBool:
				indx++;
				break;
			case valStr:
				l = 1 + strlen((char *)(fString + indx));
				if (l & kPFAlignBits)
					l = (l & ~kPFAlignBits) + kPFWordSize;
				indx += l / kPFWordSize;
				break;

			default:
				// there was a warning about not all enum values handled in
				// switch statement.
				break;
		}
	}
	while (theOpcode != opEnd);

	return true;
} /* CFormula::IsConstant */

long CFormula::StringLength() const
{
	if (!fString)
		return 0;

	PFToken theOpcode;
	int indx = 0, l;
	
	do {
		theOpcode = (PFToken)fString[indx];
		indx++;

		switch(theOpcode) {
			case opFunc:
				indx += sizeof(FuncCallData) / kPFWordSize;
				break;
			case valNum:
			case valPerc:
				indx += sizeof(double) / kPFWordSize;
				break;
			case valTime:
				indx += sizeof(time_t) / kPFWordSize;
				break;
			case valStr:
			case valName:
				l = 1 + strlen((char *)(fString + indx));
				if (l & kPFAlignBits)
					l = (l & ~kPFAlignBits) + kPFWordSize;
				indx += l / kPFWordSize;
				break;
			case valBool:
				indx++;
				break;
			case valCell:
				indx += sizeof(cell) / kPFWordSize;
				break;
			case valRange:
			case valRefRange:
				indx += sizeof(range) / kPFWordSize;
				break;
			case valXRef:
				l = strlen((char *)(fString + indx)) + 1 + sizeof(cell);
				if (l & kPFAlignBits)
					l = (l & ~kPFAlignBits) + kPFWordSize;
				indx += l / kPFWordSize;
				break;
			case valXRange:
				l = strlen((char *)(fString + indx)) + 1 + sizeof(range);
				if (l & kPFAlignBits)
					l = (l & ~kPFAlignBits) + kPFWordSize;
				indx += l / kPFWordSize;
				break;
			default:
				// there was a warning about not all enum values handled in
				// switch statement.
				break;
		}
	}
	while (theOpcode != opEnd);

	return indx * sizeof(int32);
} /* CFormula::StringLength */

void* CFormula::CopyString() const
{
	long l = StringLength();
	void *t = MALLOC(l);
	FailNil(t);
	memcpy(t, fString, l);
	return t;
} /* CFormula::CopyString */

void CFormula::Clear()
{
	if (fString)
	{
		FREE(fString);
		fString = NULL;
	}
} /* CFormula::Clear */

void* CFormula::DetachString()
{
	void* result = fString;
	fString = NULL;
	return result;
} /* CFormula::DetachString */

void CFormula::operator=(void *inString)
{
	if (fString)
		FREE(fString);
	fString = (int32 *)inString;
} /* CFormula::operator= */

bool CFormula::ReduceToValue(Value& val, CContainer *inContainer) const
{
	if (!fString)
	{
		val = Value();
		return true;
	}
	
	int i;
	
	switch (fString[0])
	{
		case valNum:
		case valPerc:
			i = 1 + sizeof(double) / kPFWordSize;
			if (fString[i] == opNegate)
			{
				val = -(*(double *)(fString + 1));
				i++;
			}
			else
				val = *(double *)(fString + 1);
			
			return (fString[i] == opEnd);
		case valTime:
			if (fString[1 + sizeof(time_t) / kPFWordSize] == opEnd)
			{
				val = *(time_t *)(fString + 1);
				return true;
			}
			break;
		case valBool:
			if (fString[2] == opEnd)
			{
				val = *(bool *)(fString + 1);
				return true;
			}
			break;
		case valStr:
			i = strlen((char *)(fString + 1)) + 1;
			if (i & kPFAlignBits)
				i = (i & ~kPFAlignBits) + kPFWordSize;
			i /= kPFWordSize;
			if (fString[i + 1] == opEnd)
			{
				val = (char *)(fString + 1);
				return true;
			}
			break;
		case valName:
			i = strlen((char *)(fString + 1)) + 1;
			if (i & kPFAlignBits)
				i = (i & ~kPFAlignBits) + kPFWordSize;
			i /= kPFWordSize;
			if (fString[i + 1] == opEnd)
			{
				if (!inContainer)
				{
					val = (char *)(fString + 1);
					return true;
				}

				// "Tabella[#Col]" (Fase 16, riga della formula stessa,
				// vedi CParser::ParseTableReference/CContainer::
				// ResolveName): questa funzione non riceve MAI la
				// posizione della formula (ReduceToValue e' pensata per
				// valori indipendenti dalla cella, come un intervallo
				// con nome), quindi risolverebbe sempre con una
				// posizione fittizia (riga 0) e fallirebbe SEMPRE, anche
				// per un riferimento perfettamente valido -- degradando
				// una formula viva a testo morto. Si salta la verifica
				// (si assume risolvibile, come una tabella/colonna
				// normale gia' trovata) e si lascia a CalcCell la
				// verifica vera, con la posizione reale.
				if (strstr((char *)(fString + 1), "[#") != NULL)
					return false;

				try
				{
					inContainer->ResolveName((char *)(fString + 1));
					return false;
				}
				catch (CErr& e)
				{
					val = (char *)(fString + 1);
					return true;
				}
			}
			break;
	}
	
	return false;
} /* CFormula::ReduceToValue */
