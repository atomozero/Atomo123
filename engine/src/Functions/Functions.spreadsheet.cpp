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
	Functions.spreadsheet.c

	Copyright 1995 - 1997, Hekkelman Programmatuur
	Portions Copyright 2026 Andrea Bernardi

	Part of Sum-It for the BeBox version 1.1.

*/

#ifndef   CONTAINER_H
#include "Container.h"
#endif

#ifndef   FORMULA_H
#include "Formula.h"
#endif

#ifndef   CELLVIEW_H
#include "EngineViewStub.h"
#endif

#ifndef   FUNCTIONUTILS_H
#include "FunctionUtils.h"
#endif

#ifndef   FUNCTIONS_H
#include "Functions.h"
#endif

#ifndef   UTILS_H
#include "Utils.h"
#endif

#ifndef   GLOBALS_H
#include "Globals.h"
#endif


#include <Window.h>

#include <algorithm>
#include <vector>

void CHOOSEFunction(Value *stack, int argCnt, CContainer *cells)
{
	Value result;
	double d;
	int indx;

	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetDoubleArgument(stack, argCnt, 1, &d))
		indx = static_cast<int>(rint(d));
	else
		indx = argCnt;	// levert dus een fout op...
	
	if (indx <= argCnt - 1 && indx > 0)
		stack[0] = stack[indx];
	else
		stack[0] = gRefNan;
}

void ERRFunction(Value *stack, int , CContainer *)
{
	stack[0] = gErrorNan;
}

void ERRORFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d))
	{
		if (isnan(d))
			stack[0] = d;
		else
		{
//			char n[32];
//			sprintf(n, "%d", (int)rint(d));
			stack[0] = Nan( static_cast<int>(rint(d)) );
		}
	}
	else
		stack[0] = gRefNan;
}

void FALSEFunction(Value *stack, int, CContainer *)
{
	stack[0] = (bool)false;
}

void TRUEFunction(Value *stack, int, CContainer *)
{
	stack[0] = (bool)true;
}

void IFFunction(Value *stack, int argCnt, CContainer *cells)
{
	bool b;
	double d;

	if (GetBooleanArgument(stack, argCnt, 1, &b))
	{
		if (b && (argCnt >= 2))
			stack[0] = stack[1];
		else if (!b && (argCnt >= 3))
			stack[0] = stack[2];
		// altrimenti: stack[0] resta la condizione originale (il
		// booleano stesso), NON va azzerato -- stesso principio
		// difensivo di IFERRFunction sotto (dove "Clear()" qui perdeva
		// davvero un valore valido, bug reale scoperto su un file
		// XLSX reale). Per IF questo ramo non e' raggiungibile
		// dall'analisi grammaticale vera (argCnt e' fissato
		// esattamente a 3 nella risorsa 'Func', vedi funcs_by_nr.r:
		// "IF(condizione)" con un solo argomento non passa nemmeno il
		// parsing) -- corretto comunque per coerenza con IFERRFunction
		// e come difesa in caso IF diventasse mai a argomenti
		// variabili.
	}
	else if (GetDoubleArgument(stack, argCnt, 1, &d) && isnan(d))
		stack[0] = d;
	else
		stack[0] = gRefNan;
}

void IFERRFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d) && isnan(d) &&
	   (argCnt >= 2))
		stack[0] = stack[1];
	else if (argCnt >= 3)
		stack[0] = stack[2];
	// altrimenti (Excel: IFERROR(valore, valore_se_errore) a due
	// argomenti, senza errore): stack[0] resta il valore originale
	// cosi' com'e', NON va azzerato -- bug reale scoperto su un file
	// XLSX reale con "=IFERROR(CONCAT(...);"")": "valore" (il
	// risultato di CONCAT, testo) non e' mai un numero NaN
	// controllabile da isnan(), quindi finiva sempre in questo ramo, e
	// "Clear()" perdeva il risultato buono di CONCAT invece di
	// lasciarlo passare -- IFERROR "senza errore" restituisce sempre
	// "valore" stesso in Excel vero, qualunque sia il suo tipo.
}

void ISNULLFunction(Value *stack, int, CContainer *)
{
	stack[0] = (bool)(stack[0].fType == eNoData);
}

void ISNUMFunction(Value *stack, int, CContainer *)
{
	stack[0] = (bool)(stack[0].fType == eNumData && !isnan(stack[0].fDouble));
}

void ISTEXTFunction(Value *stack, int, CContainer *)
{
	stack[0] = (bool)(stack[0].fType == eTextData);
}

void NAFunction(Value *stack, int, CContainer *)
{
	stack[0] = gNANan;
}

void CELLFunction(Value *stack, int argCnt, CContainer *cells)
{
	double row = 0, column = 0;
	cell c;
	
	if ( GetDoubleArgument(stack, argCnt, 1, &column)
	  && !isnan(column)
	  && (c.h = static_cast<short>( rint(column) )) > 0
	  && c.h <= kColCount
	  && GetDoubleArgument(stack, argCnt, 2, &row)
	  && !isnan(row)
	  && ( c.v = static_cast<short>(rint(row)) ) > 0 && c.v <= kRowCount)
	{
		cell c;
		Value val;
		
		c.h = static_cast<short>(rint(column)) ;
		c.v = static_cast<short>(rint(row)) ;
		
		cells->GetValue(c, val);
		
		stack[0] = val;
	}
	else if (isnan(column))
		stack[0] = column;
	else if (isnan(row))
		stack[0] = row;
	else
		stack[0] = gRefNan;
}

void COLUMNFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;
	double d;
	
	if (GetRangeArgument(stack, argCnt, 1, &cRange) && cRange.IsValid())
		stack[0] = (double)cRange.left;
	else if (GetDoubleArgument(stack, argCnt, 1, &d) && isnan(d))
		stack[0] = d;
	else
		stack[0] = gRefNan;
}

void HINDEXFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;

	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetRangeArgument(stack, argCnt, 2, &cRange) && cRange.IsValid())
	{
		CContainer *rangeCells = GetRangeContainer(stack, 2, cells);
		char keyS[256];
		double key;
		time_t keyD;
		int v = cRange.top;
		cell c;
		Value val;
		bool stop = false;

		if (GetDoubleArgument(stack, argCnt, 1, &key) && !isnan(key))
		{
			c.h = cRange.left - 1; c.v = v;
			do
			{
				c.h++;
				rangeCells->GetValue(c, val);
				if (val.fType == eNumData)
					stop = (key <= val.fDouble);
			}
			while (c.h < cRange.right && !stop);

			stack[0] = (double)(c.h - cRange.left + 1);
			if (!stop)
				stack[0].fDouble += 1;
		}
		else if (GetTextArgument(stack, argCnt, 1, keyS))
		{
			c.h = cRange.left - 1; c.v = v;
			do
			{
				c.h++;
				rangeCells->GetValue(c, val);
				if (val.fType == eTextData)
					stop = (strcmp(keyS, val.fText) <= 0);
			}
			while (c.h <= cRange.right && !stop);

			stack[0] = (double)(c.h - cRange.left + 1);
			if (!stop)
				stack[0].fDouble += 1;
		}
		else if (GetTimeArgument(stack, argCnt, 1, &keyD))
		{
			c.h = cRange.left - 1; c.v = v;
			do
			{
				c.h++;
				rangeCells->GetValue(c, val);
				if (val.fType == eTimeData)
//					stop = memcmp(&keyD, &val.dTime, sizeof(LongDateTime)) <= 0;
					stop = (keyD <= val.fTime);
			}
			while (c.h <= cRange.right && !stop);

			stack[0] = (double)(c.h - cRange.left + 1);
			if (!stop)
				stack[0].fDouble += 1;
		}
	}
	else
		stack[0] = gRefNan;
}

void HLOOKUPFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;
	double offset;
	bool handled = false;
	Value val;

	if (CheckForNanParameters(stack, argCnt))
		return;

	// Corrispondenza esatta (Fase 13): quarto argomento opzionale
	// (FALSE/0), come nel vero HLOOKUP di Excel -- vedi il commento
	// gemello su VLOOKUPFunction sotto per il motivo del fix.
	bool exactMatch = false;
	double rangeLookupNum;
	bool rangeLookupBool;
	if (GetDoubleArgument(stack, argCnt, 4, &rangeLookupNum))
		exactMatch = (rangeLookupNum == 0);
	else if (GetBooleanArgument(stack, argCnt, 4, &rangeLookupBool))
		exactMatch = !rangeLookupBool;

	if (GetRangeArgument(stack, argCnt, 2, &cRange) &&
		GetDoubleArgument(stack, argCnt, 3, &offset) &&
		cRange.IsValid())
	{
		// Fase 15: vedi lo stesso commento su VLOOKUPFunction sotto.
		CContainer *rangeCells = GetRangeContainer(stack, 2, cells);
		char keyS[256];
		double key;
		time_t keyD;
		int v = cRange.top;
		cell c;
		bool stop = false;

		if (GetDoubleArgument(stack, argCnt, 1, &key) && !isnan(key))
		{
			c.h = cRange.left - 1; c.v = v;
			do
			{
				c.h++;
				rangeCells->GetValue(c, val);
				if (val.fType == eNumData)
					stop = exactMatch ? (key == val.fDouble) : (key <= val.fDouble);
			}
			while (c.h <= cRange.right && !stop);

			if (stop)
			{
				// "- 1": c.v e' gia' fermo sulla riga di intestazione
				// (cRange.top, la PRIMA riga dell'intervallo, indice 1
				// per la convenzione di Excel row_index_num), quindi
				// sommare offset senza togliere 1 sbaglia sempre di una
				// riga (offset=1, "la riga stessa", finiva sulla riga
				// SUCCESSIVA). Bug reale pre-esistente scoperto
				// verificando il fix della corrispondenza esatta sopra
				// con valori noti -- mai notato prima perche' nessun
				// test aveva mai controllato il valore VERO restituito,
				// solo che HLOOKUP/VLOOKUP non andassero in crash.
				c.v += static_cast<short>(rint(offset)) - 1;
				rangeCells->GetValue(c, stack[0]);
				handled = true;
			}
		}
		else if (GetTextArgument(stack, argCnt, 1, keyS))
		{
			c.h = cRange.left - 1; c.v = v;
			do
			{
				c.h++;
				rangeCells->GetValue(c, val);
				if (val.fType == eTextData)
					stop = exactMatch ? (strcmp(keyS, val.fText) == 0) : (strcmp(keyS, val.fText) <= 0);
			}
			while (c.h <= cRange.right && !stop);

			if (stop)
			{
				c.v += static_cast<short>(rint(offset)) - 1; // vedi il commento sopra
				rangeCells->GetValue(c, stack[0]);
				handled = true;
			}
		}
		else if (GetTimeArgument(stack, argCnt, 1, &keyD))
		{
			c.h = cRange.left - 1; c.v = v;
			do
			{
				c.h++;
				rangeCells->GetValue(c, val);
				if (val.fType == eTimeData)
					stop = exactMatch ? (keyD == val.fTime) : (keyD <= val.fTime);
			}
			while (c.h <= cRange.right && !stop);

			if (stop)
			{
				c.v += static_cast<short>(rint(offset)) - 1; // vedi il commento sopra
				rangeCells->GetValue(c, stack[0]);
				handled = true;
			}
		}
	}

	if (!handled)
		stack[0] = gRefNan;
}

// INDEX(intervallo, riga [, colonna]) (Fase 13): a differenza di
// HINDEX/VINDEX sopra (che nonostante il nome fanno una ricerca in
// stile MATCH approssimato, non "valore alla posizione N" -- retaggio
// storico di Sum-It, nomi fuorvianti rispetto a Excel), questa e'
// l'INDEX vero e proprio di Excel: restituisce il valore alla
// posizione (riga, colonna) DENTRO l'intervallo, 1-based.
//
// Riga o colonna omessa: se l'intervallo e' largo una sola colonna
// (o alto una sola riga), l'unico argomento numerico indica la
// posizione lungo quella dimensione -- stesso comportamento "opzionale"
// del vero INDEX quando l'intervallo ha una sola riga/colonna. Se
// l'intervallo e' davvero bidimensionale e uno dei due indici e' 0 (o
// del tutto omesso), il risultato e' l'intera riga/colonna
// corrispondente come INTERVALLO (non un valore singolo) -- stesso
// principio di OFFSET sopra: un Value di tipo range che una funzione
// che aggrega (es. SUM) puo' consumare direttamente, es.
// =SUM(INDEX(A1:C10,0,2)) somma tutta la seconda colonna.
void INDEXFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 1, &cRange) || !cRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	int numRows = cRange.bottom - cRange.top + 1;
	int numCols = cRange.right - cRange.left + 1;

	double arg2 = 0, arg3 = 0;
	bool hasArg2 = GetDoubleArgument(stack, argCnt, 2, &arg2);
	bool hasArg3 = GetDoubleArgument(stack, argCnt, 3, &arg3);

	int rowNum, colNum;
	if (numRows == 1 && numCols != 1 && hasArg2 && !hasArg3)
	{
		// Intervallo a una riga sola, un unico argomento numerico:
		// seleziona la colonna, non la riga (che puo' essere solo 1).
		rowNum = 1;
		colNum = static_cast<int>(rint(arg2));
	}
	else
	{
		rowNum = hasArg2 ? static_cast<int>(rint(arg2)) : 1;
		colNum = hasArg3 ? static_cast<int>(rint(arg3)) : (numCols == 1 ? 1 : 0);
	}

	if (rowNum < 0 || rowNum > numRows || colNum < 0 || colNum > numCols
		|| (rowNum == 0 && colNum == 0))
	{
		stack[0] = gRefNan;
		return;
	}

	// Fase 15: la tabella di partenza (arg 1) puo' vivere su un altro
	// foglio -- il range/la cella restituiti restano sempre dentro
	// QUELLA STESSA tabella, mai in "cells" (il documento della formula
	// che chiama INDEX, che potrebbe essere un foglio diverso).
	CContainer *rangeCells = GetRangeContainer(stack, 1, cells);

	if (rowNum == 0)
	{
		// L'intera colonna colNum.
		int col = cRange.left + colNum - 1;
		stack[0] = range(col, cRange.top, col, cRange.bottom);
		stack[0].fRangeContainer = rangeCells;
		return;
	}
	if (colNum == 0)
	{
		// L'intera riga rowNum.
		int row = cRange.top + rowNum - 1;
		stack[0] = range(cRange.left, row, cRange.right, row);
		stack[0].fRangeContainer = rangeCells;
		return;
	}

	cell c(cRange.left + colNum - 1, cRange.top + rowNum - 1);
	rangeCells->GetValue(c, stack[0]);
}

// MATCH(valore, intervallo [, tipo]) (Fase 13): restituisce la
// POSIZIONE relativa (1-based) di "valore" dentro "intervallo", non il
// valore stesso -- l'intervallo deve essere largo una sola riga o una
// sola colonna, come nel vero MATCH di Excel. tipo=1 (predefinito,
// come Excel): ultimo valore <= chiave, intervallo assunto crescente
// (nessun controllo esplicito dell'ordinamento, stessa assunzione gia'
// fatta da VLOOKUP/HLOOKUP sopra in modalita' approssimata). tipo=0:
// corrispondenza esatta, intervallo non necessariamente ordinato.
// tipo=-1: primo valore >= chiave, intervallo assunto decrescente.
void MATCHFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 2, &cRange) || !cRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	int numRows = cRange.bottom - cRange.top + 1;
	int numCols = cRange.right - cRange.left + 1;
	if (numRows != 1 && numCols != 1)
	{
		stack[0] = gRefNan;
		return;
	}
	bool horizontal = (numRows == 1 && numCols > 1);
	int count = horizontal ? numCols : numRows;

	double matchTypeArg;
	int matchType = GetDoubleArgument(stack, argCnt, 3, &matchTypeArg)
		? static_cast<int>(rint(matchTypeArg)) : 1;

	char keyS[256];
	double key = 0;
	time_t keyD = 0;
	enum { kNumKey, kTextKey, kTimeKey } keyKind;

	if (GetDoubleArgument(stack, argCnt, 1, &key) && !isnan(key))
		keyKind = kNumKey;
	else if (GetTextArgument(stack, argCnt, 1, keyS))
		keyKind = kTextKey;
	else if (GetTimeArgument(stack, argCnt, 1, &keyD))
		keyKind = kTimeKey;
	else
	{
		stack[0] = gRefNan;
		return;
	}

	CContainer *rangeCells = GetRangeContainer(stack, 2, cells);

	int foundPos = -1;
	Value val;
	for (int i = 0; i < count; i++)
	{
		cell c = horizontal ? cell(cRange.left + i, cRange.top) : cell(cRange.left, cRange.top + i);
		rangeCells->GetValue(c, val);

		if (matchType == 0)
		{
			bool eq = (keyKind == kNumKey && val.fType == eNumData && key == val.fDouble)
				|| (keyKind == kTextKey && val.fType == eTextData && strcmp(keyS, val.fText) == 0)
				|| (keyKind == kTimeKey && val.fType == eTimeData && keyD == val.fTime);
			if (eq)
			{
				foundPos = i + 1;
				break;
			}
		}
		else if (matchType > 0)
		{
			bool le = (keyKind == kNumKey && val.fType == eNumData && val.fDouble <= key)
				|| (keyKind == kTextKey && val.fType == eTextData && strcmp(val.fText, keyS) <= 0)
				|| (keyKind == kTimeKey && val.fType == eTimeData && val.fTime <= keyD);
			if (!le)
				break;
			foundPos = i + 1;
		}
		else
		{
			bool ge = (keyKind == kNumKey && val.fType == eNumData && val.fDouble >= key)
				|| (keyKind == kTextKey && val.fType == eTextData && strcmp(val.fText, keyS) >= 0)
				|| (keyKind == kTimeKey && val.fType == eTimeData && val.fTime >= keyD);
			if (!ge)
				break;
			foundPos = i + 1;
		}
	}

	if (foundPos > 0)
		stack[0] = (double)foundPos;
	else
		// #N/A ("non trovato"), non #REF! ("riferimento non valido"):
		// stesso principio del vero Excel, dove IFNA(MATCH(...),
		// ripiego) e' un pattern comune -- bug reale scoperto scrivendo
		// il catalogo di esempio di generate_cda_report.cpp, IFNA non
		// intercettava mai un MATCH senza corrispondenza perche' l'unico
		// controllo di IFNAFunction e' sul numero di errore taggato
		// dentro il NaN (GetNanNr), non su isnan() da solo.
		stack[0] = gNANan;
}

// INDIRECT/ADDRESS/XMATCH (Fase 26, vedi ROADMAP.md "v3.0
// Consolidation"): assenti dalle funzioni originali di Sum-It,
// mancanti confrontando la tabella con l'elenco standard di Excel.

// INDIRECT(testo_rif,[stile_a1]): converte un TESTO in un riferimento
// vero -- a differenza di un riferimento scritto direttamente in una
// formula (risolto una volta sola dal parser in bytecode valRange/
// valCell, vedi Formula.cpp), qui il testo va analizzato e risolto A
// RUNTIME, ogni volta che la formula ricalcola. Solo lo stile A1 e'
// riconosciuto ("Foglio1!A1:B5", "A1", "$A$1" -- il "$" e' tollerato
// ma ignorato, il risultato e' comunque sempre un riferimento
// assoluto); lo stile R1C1 (stile_a1=FALSO) non e' supportato, il
// secondo argomento e' accettato ma ignorato.
void INDIRECTFunction(Value *stack, int argCnt, CContainer *cells)
{
	char refText[256];

	if (!GetTextArgument(stack, argCnt, 1, refText))
	{
		stack[0] = gRefNan;
		return;
	}

	CContainer *targetCells = cells;
	char *cellsPart = refText;

	char *bang = strchr(refText, '!');
	if (bang)
	{
		*bang = 0;
		char *sheetName = refText;
		// Nome foglio fra apici singoli ('Foglio 1'!A1), per un nome
		// con spazi -- stessa convenzione gia' vista nei translator
		// XLSX/ODS per lo stesso identico problema.
		size_t sheetLen = strlen(sheetName);
		if (sheetLen >= 2 && sheetName[0] == '\'' && sheetName[sheetLen - 1] == '\'')
		{
			sheetName[sheetLen - 1] = 0;
			sheetName++;
		}

		ISheetResolver *resolver = cells->GetSheetResolver();
		targetCells = resolver ? resolver->ResolveSheetByName(sheetName) : NULL;
		if (!targetCells)
		{
			stack[0] = gRefNan;
			return;
		}
		cellsPart = bang + 1;
	}

	char clean[256];
	int w = 0;
	for (char *p = cellsPart; *p && w < (int)sizeof(clean) - 1; p++)
		if (*p != '$')
			clean[w++] = *p;
	clean[w] = 0;

	char *colon = strchr(clean, ':');
	cell topLeft, botRight;
	if (colon)
	{
		*colon = 0;
		if (!cell::GetCell(clean, topLeft) || !cell::GetCell(colon + 1, botRight))
		{
			stack[0] = gRefNan;
			return;
		}
	}
	else
	{
		if (!cell::GetCell(clean, topLeft))
		{
			stack[0] = gRefNan;
			return;
		}
		botRight = topLeft;
	}

	// Un riferimento a una SOLA cella (topLeft==botRight) restituisce il
	// suo VALORE, non un intervallo di una cella -- stesso principio
	// gia' seguito dall'interprete del bytecode per un riferimento
	// scritto direttamente in formula ("case valCell"/"case valRange"
	// col caso degenere, Formula.cpp): senza questo, "=INDIRECT(\"A1\")"
	// da solo (non passato a un'altra funzione come SUM, che invece
	// accetta comunque un intervallo) restava di tipo eRangeData
	// invece del numero/testo vero contenuto in A1.
	if (topLeft.h == botRight.h && topLeft.v == botRight.v)
	{
		Value val;
		targetCells->GetValue(topLeft, val);
		stack[0] = val;
		return;
	}

	int left = topLeft.h < botRight.h ? topLeft.h : botRight.h;
	int right = topLeft.h > botRight.h ? topLeft.h : botRight.h;
	int top = topLeft.v < botRight.v ? topLeft.v : botRight.v;
	int bottom = topLeft.v > botRight.v ? topLeft.v : botRight.v;

	stack[0] = range(left, top, right, bottom);
	stack[0].fRangeContainer = targetCells;
}

// ADDRESS(riga,colonna,[tipo_assoluto],[a1],[testo_foglio]): l'inversa
// concettuale di INDIRECT sopra -- costruisce il TESTO di un
// riferimento da numeri di riga/colonna. tipo_assoluto: 1 (predefinito)
// riga e colonna assolute, 2 riga assoluta/colonna relativa, 3
// viceversa, 4 entrambe relative. Solo lo stile A1 e' riconosciuto (il
// quarto argomento e' accettato ma ignorato, stesso limite di
// INDIRECT sopra).
void ADDRESSFunction(Value *stack, int argCnt, CContainer *cells)
{
	double rowArg, colArg;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetDoubleArgument(stack, argCnt, 1, &rowArg) || !GetDoubleArgument(stack, argCnt, 2, &colArg))
	{
		stack[0] = gValueNan;
		return;
	}

	int row = static_cast<int>(rint(rowArg));
	int col = static_cast<int>(rint(colArg));
	if (row < 1 || col < 1 || col > kColCount)
	{
		stack[0] = gValueNan;
		return;
	}

	double absArgD;
	int absNum = (argCnt >= 3 && GetDoubleArgument(stack, argCnt, 3, &absArgD))
		? static_cast<int>(rint(absArgD)) : 1;
	bool colAbs = (absNum == 1 || absNum == 3);
	bool rowAbs = (absNum == 1 || absNum == 2);

	char sheetText[256];
	bool hasSheet = argCnt >= 5 && GetTextArgument(stack, argCnt, 5, sheetText);

	char colLetters[4];
	NumToAString(col, colLetters);

	char out[300];
	out[0] = 0;
	if (hasSheet)
	{
		strncat(out, sheetText, sizeof(out) - strlen(out) - 1);
		strncat(out, "!", sizeof(out) - strlen(out) - 1);
	}
	if (colAbs)
		strncat(out, "$", sizeof(out) - strlen(out) - 1);
	strncat(out, colLetters, sizeof(out) - strlen(out) - 1);
	if (rowAbs)
		strncat(out, "$", sizeof(out) - strlen(out) - 1);

	char rowStr[16];
	snprintf(rowStr, sizeof(rowStr), "%d", row);
	strncat(out, rowStr, sizeof(out) - strlen(out) - 1);

	stack[0] = out;
}

// XMATCH(valore_cercato,array_cercato,[modo_corrispondenza],
// [modo_ricerca]): come MATCHFunction sopra ma con un'API piu' chiara
// -- modo_corrispondenza 0 (predefinito) esatta, -1 esatta o il valore
// piu' vicino PIU' PICCOLO, 1 esatta o il valore piu' vicino PIU'
// GRANDE (a differenza del match_type di MATCH, -1/1 qui NON
// richiedono dati ordinati: una scansione completa trova il candidato
// migliore comunque). modo_ricerca 1 (predefinito) dal primo
// all'ultimo, -1 dall'ultimo al primo; la ricerca binaria (modo 2/-2)
// non e' distinta da quella lineare, stesso risultato ma non piu'
// veloce. Le wildcard (modo_corrispondenza=2) non sono supportate.
void XMATCHFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 2, &cRange) || !cRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	int numRows = cRange.bottom - cRange.top + 1;
	int numCols = cRange.right - cRange.left + 1;
	if (numRows != 1 && numCols != 1)
	{
		stack[0] = gRefNan;
		return;
	}
	bool horizontal = (numRows == 1 && numCols > 1);
	int count = horizontal ? numCols : numRows;

	double matchModeArg;
	int matchMode = (argCnt >= 3 && GetDoubleArgument(stack, argCnt, 3, &matchModeArg))
		? static_cast<int>(rint(matchModeArg)) : 0;
	double searchModeArg;
	int searchMode = (argCnt >= 4 && GetDoubleArgument(stack, argCnt, 4, &searchModeArg))
		? static_cast<int>(rint(searchModeArg)) : 1;

	if (matchMode < -1 || matchMode > 1)
	{
		stack[0] = gRefNan;
		return;
	}

	char keyS[256];
	double key = 0;
	time_t keyD = 0;
	enum { kNumKey, kTextKey, kTimeKey } keyKind;

	if (GetDoubleArgument(stack, argCnt, 1, &key) && !isnan(key))
		keyKind = kNumKey;
	else if (GetTextArgument(stack, argCnt, 1, keyS))
		keyKind = kTextKey;
	else if (GetTimeArgument(stack, argCnt, 1, &keyD))
		keyKind = kTimeKey;
	else
	{
		stack[0] = gRefNan;
		return;
	}

	CContainer *rangeCells = GetRangeContainer(stack, 2, cells);

	int foundPos = -1;
	int bestSmallerPos = -1;
	double bestSmallerVal = 0;
	int bestLargerPos = -1;
	double bestLargerVal = 0;

	for (int step = 0; step < count; step++)
	{
		int i = (searchMode == -1) ? (count - 1 - step) : step;
		cell c = horizontal ? cell(cRange.left + i, cRange.top) : cell(cRange.left, cRange.top + i);
		Value val;
		rangeCells->GetValue(c, val);

		bool eq = (keyKind == kNumKey && val.fType == eNumData && key == val.fDouble)
			|| (keyKind == kTextKey && val.fType == eTextData && strcasecmp(keyS, val.fText) == 0)
			|| (keyKind == kTimeKey && val.fType == eTimeData && keyD == val.fTime);
		if (eq)
		{
			foundPos = i + 1;
			break;
		}

		if (matchMode == -1 && keyKind == kNumKey && val.fType == eNumData && val.fDouble < key)
		{
			if (bestSmallerPos == -1 || val.fDouble > bestSmallerVal)
			{
				bestSmallerVal = val.fDouble;
				bestSmallerPos = i + 1;
			}
		}
		else if (matchMode == 1 && keyKind == kNumKey && val.fType == eNumData && val.fDouble > key)
		{
			if (bestLargerPos == -1 || val.fDouble < bestLargerVal)
			{
				bestLargerVal = val.fDouble;
				bestLargerPos = i + 1;
			}
		}
	}

	if (foundPos != -1)
		stack[0] = (double)foundPos;
	else if (matchMode == -1 && bestSmallerPos != -1)
		stack[0] = (double)bestSmallerPos;
	else if (matchMode == 1 && bestLargerPos != -1)
		stack[0] = (double)bestLargerPos;
	else
		// #N/A, non #REF! -- stesso motivo del fix gemello in
		// MATCHFunction sopra.
		stack[0] = gNANan;
}

// true se il bytecode di "formulaBytecode" (vedi CContainer::
// GetCellFormula(const cell&), che restituisce il puntatore grezzo
// SENZA mai passare da CFormula::UnMangle) e' ESATTAMENTE una singola
// chiamata a "funcNr", niente altro prima o dopo -- distingue
// "=SEQUENCE(3,1)" (l'intera formula, spilla) da "=SEQUENCE(3,1)+0" o
// "=SUM(SEQUENCE(3,1))" (annidata, NON deve spillare: altrimenti
// scriverebbe nelle celle vicine come effetto collaterale di una
// formula che l'utente non ha scritto per fare quello).
//
// Cammina il bytecode grezzo (lo stesso formato di CFormula::Calculate/
// UnMangle in Formula.cpp, che questa funzione rispecchia SOLO per la
// parte "quanti word occupa ogni token", copiata da CFormula::AddToken)
// senza pero' costruire NESSUN testo -- e apposta: chiamare UnMangle
// da qui aveva prodotto un blocco reale, root-caused solo dopo
// un'intera sessione di debug a un dettaglio "ovvio" solo in
// retrospettiva, gia' documentato altrove in questo stesso file per
// CContainer: ftoa() (usata da UnMangle per formattare ogni valNum)
// chiama Font().StringWidth(), che richiede una vera connessione
// all'app_server -- si blocca per sempre in QUALUNQUE contesto
// headless (motore isolato senza BApplication, compresi TUTTI i test
// automatici di questo progetto). L'app vera non ne soffre (ha sempre
// una BApplication viva), ma un test headless si', quindi questa
// funzione non puo' permettersi di chiamare UnMangle in nessun caso.
//
// L'unica cosa che serve davvero e' sapere qual e' l'ULTIMO token
// prima di opEnd: se e' un opFunc verso "funcNr", la chiamata e'
// necessariamente l'operazione piu' esterna dell'intera formula (in
// notazione postfissa, l'ultimo token e' sempre la radice
// dell'albero) -- "=SUM(SEQUENCE(3,1))" finisce con opFunc(SUM), non
// opFunc(SEQUENCE), quindi viene gia' correttamente escluso senza
// bisogno di contare quante volte SEQUENCE compare in tutto.
static bool IsWholeFunctionCall(void *formulaBytecode, int funcNr)
{
	if (!formulaBytecode)
		return false;

	const int32 *fString = (const int32 *)formulaBytecode;
	int indx = 0;
	bool lastWasTargetCall = false;
	PFToken nextOpcode;

	while ((nextOpcode = (PFToken)fString[indx++]) != opEnd)
	{
		lastWasTargetCall = false;
		int toAdd = 0;

		switch (nextOpcode)
		{
			case opFunc:
			{
				FuncCallData fcd = *((FuncCallData *)(fString + indx));
				lastWasTargetCall = (fcd.funcNr == funcNr);
				toAdd = sizeof(FuncCallData);
				break;
			}
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
				toAdd = strlen((const char *)(fString + indx)) + 1;
				break;
			case valCell:
				toAdd = sizeof(cell);
				break;
			case valRange:
				toAdd = sizeof(range);
				break;
			case valXRef:
				toAdd = strlen((const char *)(fString + indx)) + 1 + sizeof(cell);
				break;
			case valXRange:
				toAdd = strlen((const char *)(fString + indx)) + 1 + sizeof(range);
				break;
			default:
				toAdd = 0;
				break;
		}

		if (toAdd & kPFAlignBits)
			toAdd = (toAdd & ~kPFAlignBits) + kPFWordSize;
		indx += toAdd / kPFWordSize;
	}

	return lastWasTargetCall;
}

// SEQUENCE(rows, [columns], [start], [step]) -- Fase 29, vedi
// ROADMAP.md "v3.0 Consolidation", ultimo elemento del backlog: la
// prima (e per ora unica) funzione "spill" di Atomo123, che scrive il
// proprio risultato in un intero blocco di celle invece che in una
// sola. Il meccanismo vero e proprio (collisione, sostituzione di uno
// spill precedente di forma diversa) vive in CContainer::ApplySpill
// (Container.spill.cpp) -- vedi il commento esteso li' per il design
// completo, in particolare il perche' NON serve persistere lo spill
// su ASCD.
//
// stack[0] resta sempre il valore "start" (l'angolo in alto a
// sinistra dell'array, riga 0 colonna 0): e' il valore che un
// chiamante annidato (es. "=SUM(SEQUENCE(3,1))") vede al posto
// dell'intero array -- limite dichiarato, non un errore. Lo spill vero
// e proprio scatta SOLO se IsWholeFunctionCall sopra conferma che
// questa chiamata e' l'INTERA formula della cella: annidata dentro
// un'altra funzione (o con qualunque altra cosa prima/dopo) si comporta
// come una normale funzione scalare, senza toccare nessun'altra cella.
void SEQUENCEFunction(Value *stack, int argCnt, CContainer *cells)
{
	if (CheckForNanParameters(stack, argCnt))
		return;

	double rowsArg = 0, colsArg = 1, start = 1, step = 1;
	if (!GetDoubleArgument(stack, argCnt, 1, &rowsArg))
	{
		stack[0] = gRefNan;
		return;
	}
	GetDoubleArgument(stack, argCnt, 2, &colsArg);
	GetDoubleArgument(stack, argCnt, 3, &start);
	GetDoubleArgument(stack, argCnt, 4, &step);

	int rows = static_cast<int>(rint(rowsArg));
	int cols = static_cast<int>(rint(colsArg));
	// Stesso limite del resto del motore (nessun controllo esplicito
	// altrove su dimensioni "ragionevoli" di un intervallo): un blocco
	// enorme (es. SEQUENCE(1000000,1) per errore di battitura) non
	// crasha, ma puo' essere lento -- nessun tetto arbitrario imposto
	// qui, coerente con come il resto del foglio si comporta gia'.
	if (rows <= 0 || cols <= 0)
	{
		stack[0] = gRefNan;
		return;
	}

	stack[0] = start; // angolo in alto a sinistra, vedi il commento sopra la funzione

	if (rows * cols <= 1 || !cells)
		return; // una sola cella: niente da spillare, il valore sopra basta

	cell owner = cells->CalculatingCell();
	if (!IsWholeFunctionCall(cells->GetCellFormula(owner), kSEQUENCEFuncNr))
		return; // annidata in un'altra formula: nessuno spill, vedi il commento sopra
	std::vector<Value> values;
	values.reserve(rows * cols);
	for (int r = 0; r < rows; r++)
		for (int c = 0; c < cols; c++)
			values.push_back(Value(start + step * (r * cols + c)));

	// Se ApplySpill rifiuta (collisione con la formula propria di
	// un'altra cella), stack[0] resta comunque "start" impostato sopra
	// -- nessun indicatore d'errore dedicato in questa prima versione
	// (vedi il commento su ApplySpill in Container.h): la cella owner
	// mostra silenziosamente solo il primo valore invece dell'intero
	// array. Limite dichiarato.
	cells->ApplySpill(owner, rows, cols, values);
}

// UNIQUE(array) -- Fase 34, "Path to full Excel parity" Tier 2, prima
// delle funzioni ad array dinamico oltre SEQUENCE: restituisce i
// valori distinti di "array", nell'ordine della prima comparsa (come
// il vero UNIQUE di Excel di default). Scala insieme a SEQUENCE lo
// stesso identico meccanismo di spill (CContainer::ApplySpill,
// Container.spill.cpp) -- questa e' la seconda funzione "spill" di
// Atomo123, non un nuovo meccanismo.
//
// Limite dichiarato: "array" deve essere una singola riga o una
// singola colonna (come lookup_array di XLOOKUP sopra). Un intervallo
// davvero bidimensionale (deduplicare intere RIGHE, non singoli
// valori) e gli argomenti opzionali "by_col"/"exactly_once" del vero
// UNIQUE di Excel non sono implementati -- caso raro nell'uso reale
// (la stragrande maggioranza delle formule UNIQUE viste in file veri
// lavora su un elenco a una colonna sola), stesso principio di scope
// gia' dichiarato per XLOOKUP (solo corrispondenza esatta) e VLOOKUP/
// HLOOKUP altrove in questo file.
void UNIQUEFunction(Value *stack, int argCnt, CContainer *cells)
{
	range r;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 1, &r) || !r.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	int rows = r.bottom - r.top + 1;
	int cols = r.right - r.left + 1;
	if (rows != 1 && cols != 1)
	{
		stack[0] = gRefNan;
		return;
	}
	bool horizontal = (rows == 1 && cols > 1);
	int count = horizontal ? cols : rows;

	CContainer *srcCells = GetRangeContainer(stack, 1, cells);

	// O(n^2) nel numero di celle dell'intervallo: nessun hash su Value
	// (fType misto numero/testo/data/booleano, vedi Value::operator==
	// per il confronto cross-tipo gia' esistente) -- corretto anche per
	// un elenco di qualche migliaio di righe, non pensato per decine di
	// migliaia.
	std::vector<Value> uniqueValues;
	uniqueValues.reserve(count);
	for (int i = 0; i < count; i++)
	{
		cell c = horizontal ? cell(r.left + i, r.top) : cell(r.left, r.top + i);
		Value v;
		srcCells->GetValue(c, v);

		bool found = false;
		for (size_t u = 0; u < uniqueValues.size() && !found; u++)
			found = (uniqueValues[u] == v);
		if (!found)
			uniqueValues.push_back(v);
	}

	stack[0] = uniqueValues[0]; // angolo in alto a sinistra, vedi il commento su SEQUENCE sopra

	if (uniqueValues.size() <= 1 || !cells)
		return;

	cell owner = cells->CalculatingCell();
	if (!IsWholeFunctionCall(cells->GetCellFormula(owner), kUNIQUEFuncNr))
		return; // annidata in un'altra formula: nessuno spill, vedi il commento su SEQUENCE sopra

	// Spilla nello stesso verso dell'intervallo sorgente (una riga se
	// orizzontale, una colonna altrimenti) -- uniqueValues e' gia' un
	// elenco piatto, quindi va bene sia per "1 riga x N colonne" che
	// per "N righe x 1 colonna" senza bisogno di riorganizzarlo.
	if (horizontal)
		cells->ApplySpill(owner, 1, (int)uniqueValues.size(), uniqueValues);
	else
		cells->ApplySpill(owner, (int)uniqueValues.size(), 1, uniqueValues);
}

// SORT(array, [sort_index], [sort_order]) -- Fase 34, "Path to full
// Excel parity" Tier 2, "Dynamic arrays beyond SEQUENCE", terza
// funzione "spill" di Atomo123 dopo SEQUENCE/UNIQUE, stesso
// meccanismo. A differenza di UNIQUE sopra, qui il supporto e'
// genuinamente bidimensionale: sort_index (1-based, default 1)
// sceglie la COLONNA di "array" da usare come chiave, ma ogni RIGA
// intera si sposta insieme come un'unita' -- esattamente come il vero
// SORT di Excel su una tabella a piu' colonne, non solo un elenco a
// una colonna sola. sort_order: 1 (default) crescente, -1
// decrescente, qualunque altro valore trattato come crescente (stesso
// principio permissivo gia' usato altrove in questo file per un
// argomento fuori dai valori attesi).
//
// Limite dichiarato: "by_col" (il quarto argomento del vero SORT di
// Excel, per ordinare per RIGA invece che per colonna) non e'
// implementato -- caso raro nell'uso reale, l'orientamento "ordina le
// righe di una tabella" copre la stragrande maggioranza delle formule
// SORT viste in file veri.
void SORTFunction(Value *stack, int argCnt, CContainer *cells)
{
	range r;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 1, &r) || !r.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	int rows = r.bottom - r.top + 1;
	int cols = r.right - r.left + 1;

	double sortIndexArg = 1, sortOrderArg = 1;
	GetDoubleArgument(stack, argCnt, 2, &sortIndexArg);
	GetDoubleArgument(stack, argCnt, 3, &sortOrderArg);
	int sortIndex = static_cast<int>(rint(sortIndexArg));
	bool ascending = (sortOrderArg >= 0);

	if (sortIndex < 1 || sortIndex > cols)
	{
		stack[0] = gRefNan;
		return;
	}

	CContainer *srcCells = GetRangeContainer(stack, 1, cells);

	// Legge l'intera tabella in memoria, riga per riga (row-major, lo
	// stesso ordine in cui ApplySpill si aspetta "values" piu' sotto) --
	// serve tutta prima di poter decidere l'ordine delle righe.
	std::vector<std::vector<Value> > table(rows, std::vector<Value>(cols));
	for (int row = 0; row < rows; row++)
		for (int col = 0; col < cols; col++)
			srcCells->GetValue(cell(r.left + col, r.top + row), table[row][col]);

	std::vector<int> order(rows);
	for (int i = 0; i < rows; i++)
		order[i] = i;

	int keyCol = sortIndex - 1;
	// stable_sort, non sort: righe con lo stesso valore nella colonna
	// chiave mantengono il loro ordine relativo originale, come il
	// vero SORT di Excel (e come "Ordina" nel menu Dati di questa
	// stessa app).
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
		Value va = table[a][keyCol], vb = table[b][keyCol];
		return ascending ? (va < vb) : (vb < va);
	});

	std::vector<Value> values;
	values.reserve(rows * cols);
	for (int i = 0; i < rows; i++)
		for (int col = 0; col < cols; col++)
			values.push_back(table[order[i]][col]);

	stack[0] = values[0]; // angolo in alto a sinistra, vedi il commento su SEQUENCE sopra

	if (rows * cols <= 1 || !cells)
		return;

	cell owner = cells->CalculatingCell();
	if (!IsWholeFunctionCall(cells->GetCellFormula(owner), kSORTFuncNr))
		return; // annidata in un'altra formula: nessuno spill, vedi il commento su SEQUENCE sopra

	cells->ApplySpill(owner, rows, cols, values);
}

// SORTBY(array, by_array, [sort_order]) -- Fase 34, stesso gruppo di
// SORT sopra, quarta funzione "spill" di Atomo123. Differenza rispetto
// a SORT: la chiave di ordinamento viene da un intervallo SEPARATO
// (by_array), non da una colonna di "array" stesso -- utile per
// ordinare una tabella in base a valori che non fanno parte della
// tabella da restituire (es. una classifica esterna). by_array deve
// avere tante voci quante le RIGHE di array (una per riga da spostare
// insieme, stesso principio "riga intera si sposta insieme" di SORT).
//
// Limiti dichiarati, entrambi rari nell'uso reale: il vero SORTBY di
// Excel supporta piu' coppie array/ordine per un ordinamento multi-
// livello (qui solo una coppia, un ordinamento a un solo livello);
// "array" orizzontale (una riga sola invece di una o piu' colonne)
// non e' supportato, stesso principio "verticale" gia' assunto da
// SORT sopra.
void SORTBYFunction(Value *stack, int argCnt, CContainer *cells)
{
	range arrayRange, byRange;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 1, &arrayRange) || !arrayRange.IsValid() ||
		!GetRangeArgument(stack, argCnt, 2, &byRange) || !byRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	int rows = arrayRange.bottom - arrayRange.top + 1;
	int cols = arrayRange.right - arrayRange.left + 1;
	int byRows = byRange.bottom - byRange.top + 1;
	int byCols = byRange.right - byRange.left + 1;
	bool byHorizontal = (byRows == 1 && byCols > 1);
	int byCount = byHorizontal ? byCols : byRows;

	if (byCount != rows || (byRows != 1 && byCols != 1))
	{
		stack[0] = gRefNan;
		return;
	}

	double sortOrderArg = 1;
	GetDoubleArgument(stack, argCnt, 3, &sortOrderArg);
	bool ascending = (sortOrderArg >= 0);

	CContainer *srcCells = GetRangeContainer(stack, 1, cells);
	CContainer *byCells = GetRangeContainer(stack, 2, cells);

	std::vector<Value> byValues(rows);
	for (int i = 0; i < rows; i++)
	{
		cell c = byHorizontal ? cell(byRange.left + i, byRange.top)
			: cell(byRange.left, byRange.top + i);
		byCells->GetValue(c, byValues[i]);
	}

	std::vector<std::vector<Value> > table(rows, std::vector<Value>(cols));
	for (int row = 0; row < rows; row++)
		for (int col = 0; col < cols; col++)
			srcCells->GetValue(cell(arrayRange.left + col, arrayRange.top + row), table[row][col]);

	std::vector<int> order(rows);
	for (int i = 0; i < rows; i++)
		order[i] = i;
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
		return ascending ? (byValues[a] < byValues[b]) : (byValues[b] < byValues[a]);
	});

	std::vector<Value> values;
	values.reserve(rows * cols);
	for (int i = 0; i < rows; i++)
		for (int col = 0; col < cols; col++)
			values.push_back(table[order[i]][col]);

	stack[0] = values[0]; // angolo in alto a sinistra, vedi il commento su SEQUENCE sopra

	if (rows * cols <= 1 || !cells)
		return;

	cell owner = cells->CalculatingCell();
	if (!IsWholeFunctionCall(cells->GetCellFormula(owner), kSORTBYFuncNr))
		return; // annidata in un'altra formula: nessuno spill, vedi il commento su SEQUENCE sopra

	cells->ApplySpill(owner, rows, cols, values);
}

// XLOOKUP(lookup_value, lookup_array, return_array, [if_not_found],
// [match_mode], [search_mode]) -- Fase 14, nome standard Excel piu'
// recente del formato dichiarato del file (scritto con "_xlfn."
// davanti nei file XLSX veri, vedi GetFunctionNr in Utils.cpp). Bug
// reale segnalato dall'utente: un file XLSX reale con XLOOKUP su
// colonne di una Tabella (ListObject) Excel (vedi CTableDef in
// Container.h/CContainer::ResolveName) mostrava il testo letterale
// della formula invece del valore calcolato.
//
// Stesso schema esatto di MATCHFunction sopra (lookup_array a una
// riga/colonna sola, scansione posizionale), ma con due intervalli
// paralleli invece di uno solo -- il risultato e' il valore di
// return_array alla STESSA posizione dove lookup_array corrisponde,
// non la posizione stessa. Solo corrispondenza esatta (match_mode 0,
// il valore di default e il piu' comune): le modalita' approssimata/
// carattere jolly del vero XLOOKUP di Excel non sono implementate --
// limite dichiarato, non un bug (nessuna formula reale vista finora le
// usa). search_mode (ultimo argomento, ordine di scansione) ignorato
// allo stesso modo: una scansione dal primo all'ultimo elemento resta
// corretta anche su dati non ordinati, solo piu' lenta di una ricerca
// binaria che qui non serve.
void XLOOKUPFunction(Value *stack, int argCnt, CContainer *cells)
{
	range lookupRange, returnRange;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetRangeArgument(stack, argCnt, 2, &lookupRange) || !lookupRange.IsValid() ||
		!GetRangeArgument(stack, argCnt, 3, &returnRange) || !returnRange.IsValid())
	{
		stack[0] = gRefNan;
		return;
	}

	int lookupRows = lookupRange.bottom - lookupRange.top + 1;
	int lookupCols = lookupRange.right - lookupRange.left + 1;
	if (lookupRows != 1 && lookupCols != 1)
	{
		stack[0] = gRefNan;
		return;
	}
	bool horizontal = (lookupRows == 1 && lookupCols > 1);
	int count = horizontal ? lookupCols : lookupRows;

	// return_array deve avere la STESSA lunghezza di lookup_array lungo
	// lo stesso verso (una colonna se lookup_array e' verticale, una
	// riga se orizzontale) -- stesso principio del vero XLOOKUP di
	// Excel, che segnala un errore se le dimensioni non corrispondono.
	int returnRows = returnRange.bottom - returnRange.top + 1;
	int returnCols = returnRange.right - returnRange.left + 1;
	int returnCount = horizontal ? returnCols : returnRows;
	if (returnCount != count)
	{
		stack[0] = gRefNan;
		return;
	}

	char keyS[256];
	double key = 0;
	time_t keyD = 0;
	enum { kNumKey, kTextKey, kTimeKey } keyKind;

	if (GetDoubleArgument(stack, argCnt, 1, &key) && !isnan(key))
		keyKind = kNumKey;
	else if (GetTextArgument(stack, argCnt, 1, keyS))
		keyKind = kTextKey;
	else if (GetTimeArgument(stack, argCnt, 1, &keyD))
		keyKind = kTimeKey;
	else
	{
		stack[0] = gRefNan;
		return;
	}

	// Fase 15: lookup_array e return_array possono venire da tabelle
	// strutturate su DUE fogli diversi (o entrambe da un foglio diverso
	// da quello di questa formula, il caso piu' comune) -- vedi
	// GetRangeContainer/Value::fRangeContainer. "cells" resta il
	// ripiego per un range locale, come sempre.
	CContainer *lookupCells = GetRangeContainer(stack, 2, cells);
	CContainer *returnCells = GetRangeContainer(stack, 3, cells);

	int foundPos = -1;
	Value val;
	for (int i = 0; i < count; i++)
	{
		cell c = horizontal ? cell(lookupRange.left + i, lookupRange.top)
			: cell(lookupRange.left, lookupRange.top + i);
		lookupCells->GetValue(c, val);

		bool eq = (keyKind == kNumKey && val.fType == eNumData && key == val.fDouble)
			|| (keyKind == kTextKey && val.fType == eTextData && strcmp(keyS, val.fText) == 0)
			|| (keyKind == kTimeKey && val.fType == eTimeData && keyD == val.fTime);
		if (eq)
		{
			foundPos = i;
			break;
		}
	}

	if (foundPos >= 0)
	{
		cell c = horizontal ? cell(returnRange.left + foundPos, returnRange.top)
			: cell(returnRange.left, returnRange.top + foundPos);
		returnCells->GetValue(c, stack[0]);
	}
	else if (argCnt >= 4)
		// if_not_found (quarto argomento, opzionale): qualunque valore
		// grezzo passato cosi' com'e', stesso schema di IFERR/IF sopra
		// per il ramo "vero"/"falso" -- puo' essere un numero, testo,
		// o qualunque altra cosa, non solo un errore.
		stack[0] = stack[3];
	else
		// #N/A, non #REF! -- stesso motivo del fix gemello in
		// MATCHFunction/XMATCHFunction sopra.
		stack[0] = gNANan;
}

// IFS(condizione1, valore1, [condizione2, valore2], ...) -- Fase 14,
// stesso nome/comportamento standard Excel di XLOOKUP sopra (prefisso
// "_xlfn." nei file XLSX veri). La prima coppia con condizione vera
// decide il risultato, come una catena di IF() innestati -- stesso
// schema di IFFunction sopra, ripetuto per ogni coppia.
void IFSFunction(Value *stack, int argCnt, CContainer *cells)
{
	bool b;

	for (int i = 1; i + 1 <= argCnt; i += 2)
	{
		if (GetBooleanArgument(stack, argCnt, i, &b))
		{
			if (b)
			{
				stack[0] = stack[i]; // il valore appaiato (argomento i+1, 0-based stack[i])
				return;
			}
		}
		else
		{
			// Una condizione non booleana (es. un errore propagato)
			// interrompe la valutazione, come farebbe IF() con lo
			// stesso argomento.
			double d;
			if (GetDoubleArgument(stack, argCnt, i, &d) && isnan(d))
			{
				stack[0] = d;
				return;
			}
			stack[0] = gRefNan;
			return;
		}
	}

	// Nessuna condizione vera: come il vero IFS di Excel ("nessun
	// risultato"), non un valore di sole zero/vuoto -- gRefNan, stesso
	// sentinella di errore gia' usato da VLOOKUP/MATCH/XLOOKUP sopra
	// per "non trovato" (nessun tipo di errore dedicato nel motore).
	stack[0] = gRefNan;
}

void NCOLSFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;
	double d;
	
	if (GetRangeArgument(stack, argCnt, 1, &cRange) && cRange.IsValid())
		stack[0] = (double)(cRange.right - cRange.left + 1);
	else if (GetDoubleArgument(stack, argCnt, 1, &d) && isnan(d))
		stack[0] = d;
	else
		stack[0] = gRefNan;
}

void NROWSFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;
	double d;
	
	if (GetRangeArgument(stack, argCnt, 1, &cRange) && cRange.IsValid())
		stack[0] = (double)(cRange.bottom - cRange.top + 1);
	else if (GetDoubleArgument(stack, argCnt, 1, &d) && isnan(d))
		stack[0] = d;
	else
		stack[0] = gRefNan;
}

void NUMPAGESFunction(Value *stack, int , CContainer *cells)
{
	CCellView *theView;
	
	if (cells && (theView = cells->GetOwner()) != NULL)
		stack[0] = (double)theView->CountPages();
	else
		stack[0] = gNANan;
}

void OFFSETFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;
	double dh, dv;
	int h, v;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetRangeArgument(stack, argCnt, 1, &cRange) &&
		cRange.IsValid() &&
		GetDoubleArgument(stack, argCnt, 2, &dh) &&
		( h = static_cast<int>(rint(dh)) ) + cRange.left > 0 && h + cRange.right <= kColCount &&
		GetDoubleArgument(stack, argCnt, 3, &dv) &&
		( v = static_cast<int>(rint(dv)) ) + cRange.top > 0 && v + cRange.bottom <= kRowCount)
	{
		cRange.OffsetBy(h, v);
		stack[0] = cRange;
	}
	else
		stack[0] = gRefNan;
}

void PAGEFunction(Value *stack, int , CContainer *cells)
{
	CCellView *theView;
	
	if (cells && (theView = cells->GetOwner()) != NULL)
		stack[0] = (double)theView->GetPageNrForCell(cells->CalculatingCell());
	else
		stack[0] = gNANan;
}

void DOCUMENTFunction(Value *stack, int, CContainer *cells)
{
	CCellView *thePane;
	
	if (cells && (thePane = cells->GetOwner()) != NULL)
#if !__BEOS__ && !__HAIKU__ 
	{
		Str255 s;
		thePane->Doc()->GetDescriptor(s);
		stack[0] = p2cstr(s);
	}
#else
		stack[0] = thePane->Window()->Title();
#endif
	else
		stack[0] = gNANan;
}

void ROWFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;
	double d;
	
	if (GetRangeArgument(stack, argCnt, 1, &cRange) && cRange.IsValid())
		stack[0] = (double)cRange.top;
	else if (GetDoubleArgument(stack, argCnt, 1, &d) && isnan(d))
		stack[0] = d;
	else
		stack[0] = gRefNan;
}

void VINDEXFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetRangeArgument(stack, argCnt, 2, &cRange) && cRange.IsValid())
	{
		CContainer *rangeCells = GetRangeContainer(stack, 2, cells);
		char keyS[256];
		double key;
		time_t keyD;
		int h = cRange.left;
		cell c;
		Value val;
		bool stop = false;

		if (GetDoubleArgument(stack, argCnt, 1, &key))
		{
			c.h = h; c.v = cRange.top - 1;
			do
			{
				c.v++;
				rangeCells->GetValue(c, val);
				if (val.fType == eNumData)
					stop = (key <= val.fDouble);
			}
			while (c.v <= cRange.bottom && !stop);

			stack[0] = (double)(c.v - cRange.top + 1);
			if (!stop)
				stack[0].fDouble += 1;
		}
		else if (GetTextArgument(stack, argCnt, 1, keyS))
		{
			c.h = h; c.v = cRange.top - 1;
			do
			{
				c.v++;
				rangeCells->GetValue(c, val);
				if (val.fType == eTextData)
					stop = (strcmp(keyS, val.fText) <= 0);
			}
			while (c.v <= cRange.bottom && !stop);

			stack[0] = (double)(c.v - cRange.top + 1);
			if (!stop)
				stack[0].fDouble += 1;
		}
		else if (GetTimeArgument(stack, argCnt, 1, &keyD))
		{
			c.h = h; c.v = cRange.top - 1;
			do
			{
				c.v++;
				rangeCells->GetValue(c, val);
				if (val.fType == eTimeData)
					stop = (keyD <= val.fTime);
			}
			while (c.v <= cRange.bottom && !stop);

			stack[0] = (double)(c.v - cRange.top + 1);
			if (!stop)
				stack[0].fDouble += 1;
		}
	}
	else
		stack[0] = gRefNan;
}

void VLOOKUPFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;
	double offset;
	bool handled = false;

	if (CheckForNanParameters(stack, argCnt))
		return;

	// Corrispondenza esatta (Fase 13): quarto argomento opzionale
	// (FALSE/0), come nel vero VLOOKUP di Excel -- prima non veniva
	// mai letto, quindi la ricerca era sempre "approssimata" (trova il
	// primo valore >= chiave, presuppone l'intervallo ordinato per la
	// colonna chiave), la forma di gran lunga meno comune nei fogli
	// reali. Bug reale scoperto su un file reale che chiedeva
	// esplicitamente VLOOKUP(...,4,0) -- corrispondenza esatta su un
	// intervallo NON ordinato per quella colonna, che con la vecchia
	// logica avrebbe restituito silenziosamente la riga sbagliata
	// invece del risultato atteso o di un errore.
	bool exactMatch = false;
	double rangeLookupNum;
	bool rangeLookupBool;
	if (GetDoubleArgument(stack, argCnt, 4, &rangeLookupNum))
		exactMatch = (rangeLookupNum == 0);
	else if (GetBooleanArgument(stack, argCnt, 4, &rangeLookupBool))
		exactMatch = !rangeLookupBool;

	if (GetRangeArgument(stack, argCnt, 2, &cRange) &&
		cRange.IsValid() &&
		GetDoubleArgument(stack, argCnt, 3, &offset))
	{
		// Fase 15: la tabella puo' vivere su un altro foglio (vedi
		// XLOOKUPFunction sopra per lo stesso principio) -- la colonna
		// di ritorno (c.h += offset piu' sotto) resta sempre dentro
		// QUESTA STESSA tabella, quindi un solo container basta per
		// tutta la funzione.
		CContainer *rangeCells = GetRangeContainer(stack, 2, cells);
		char keyS[256];
		double key;
		time_t keyD;
		int h = cRange.left;
		cell c;
		Value val;
		bool stop = false;

		if (GetDoubleArgument(stack, argCnt, 1, &key))
		{
			c.h = h; c.v = cRange.top - 1;
			do
			{
				c.v++;
				rangeCells->GetValue(c, val);
				if (val.fType == eNumData)
					stop = exactMatch ? (key == val.fDouble) : (key <= val.fDouble);
			}
			while (c.v <= cRange.bottom && !stop);

			if (stop)
			{
				// "- 1": c.h e' gia' fermo sulla colonna dell'intervallo
				// (cRange.left, la PRIMA colonna, indice 1 per la
				// convenzione di Excel col_index_num) -- vedi lo stesso
				// commento su HLOOKUPFunction sopra, identico bug pre-
				// esistente sull'asse opposto.
				c.h += static_cast<short>(rint(offset)) - 1;
				rangeCells->GetValue(c, stack[0]);
				handled = true;
			}
		}
		else if (GetTextArgument(stack, argCnt, 1, keyS))
		{
			c.h = h; c.v = cRange.top - 1;
			do
			{
				c.v++;
				rangeCells->GetValue(c, val);
				if (val.fType == eTextData)
					stop = exactMatch ? (strcmp(keyS, val.fText) == 0) : (strcmp(keyS, val.fText) <= 0);
			}
			while (c.v <= cRange.bottom && !stop);

			if (stop)
			{
				c.h += static_cast<short>(rint(offset)) - 1; // vedi il commento sopra
				rangeCells->GetValue(c, stack[0]);
				handled = true;
			}
		}
		else if (GetTimeArgument(stack, argCnt, 1, &keyD))
		{
			c.h = h; c.v = cRange.top - 1;
			do
			{
				c.v++;
				rangeCells->GetValue(c, val);
				if (val.fType == eTimeData)
					stop = exactMatch ? (keyD == val.fTime) : (keyD <= val.fTime);
			}
			while (c.v <= cRange.bottom && !stop);

			if (stop)
			{
				c.h += static_cast<short>(rint(offset)) - 1; // vedi il commento sopra
				rangeCells->GetValue(c, stack[0]);
				handled = true;
			}
		}
	}

	if (!handled)
		stack[0] = gRefNan;
}
