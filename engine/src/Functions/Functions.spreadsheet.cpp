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
	
	Part of Sum-It for the BeBox version 1.1.

*/

#ifndef   CONTAINER_H
#include "Container.h"
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
				cells->GetValue(c, val);
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
				cells->GetValue(c, val);
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
				cells->GetValue(c, val);
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

	int foundPos = -1;
	Value val;
	for (int i = 0; i < count; i++)
	{
		cell c = horizontal ? cell(cRange.left + i, cRange.top) : cell(cRange.left, cRange.top + i);
		cells->GetValue(c, val);

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
		stack[0] = gRefNan;
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
		// o qualunque altra cosa, non solo gRefNan.
		stack[0] = stack[3];
	else
		stack[0] = gRefNan;
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
				cells->GetValue(c, val);
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
				cells->GetValue(c, val);
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
				cells->GetValue(c, val);
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
