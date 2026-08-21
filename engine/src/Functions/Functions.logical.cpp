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
	Functions.logical.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include "Container.h"
#include "FunctionUtils.h"
#include "Functions.h"
#include "Globals.h"
#include "Utils.h"

void ANDFunction(Value *stack, int argCnt, CContainer *cells)
{
	bool b;
	int i;
	Value result = (bool)true;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	for (i = 1; i <= argCnt; i++)
	{
		if (GetBooleanArgument(stack, argCnt, i, &b))
		{
			result.fBool = result.fBool && b;
		}
	}
	
	stack[0] = result;
}

void ORFunction(Value *stack, int argCnt, CContainer *cells)
{
	bool b;
	int i;
	Value result = false;

	if (CheckForNanParameters(stack, argCnt))
		return;
	
	for (i = 1; i <= argCnt; i++)
	{
		if (GetBooleanArgument(stack, argCnt, i, &b))
		{
			result.fBool = result.fBool || b;
		}
	}

	stack[0] = result;
}

// NOT/XOR/SWITCH/IFNA/ISBLANK/ISERROR/ISNA/ISFORMULA (Fase 26): assenti
// dalle funzioni originali di Sum-It, mancanti confrontando la tabella
// con l'elenco standard di Excel (vedi ROADMAP.md, "v3.0
// Consolidation").
void NOTFunction(Value *stack, int argCnt, CContainer *cells)
{
	bool b;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (GetBooleanArgument(stack, argCnt, 1, &b))
		stack[0] = (bool)!b;
	else
		stack[0] = gValueNan;
}

void XORFunction(Value *stack, int argCnt, CContainer *cells)
{
	bool b;
	int i, trueCount = 0;

	if (CheckForNanParameters(stack, argCnt))
		return;

	for (i = 1; i <= argCnt; i++)
		if (GetBooleanArgument(stack, argCnt, i, &b) && b)
			trueCount++;

	// VERO se un numero DISPARI di argomenti e' VERO -- stesso
	// comportamento di Excel (una "somma booleana modulo 2", non un OR
	// esclusivo a due soli argomenti come il nome potrebbe suggerire).
	stack[0] = (bool)((trueCount % 2) == 1);
}

// SWITCH(espressione, valore1, risultato1, [valore2, risultato2, ...],
// [predefinito]): confronta espressione con ogni valoreN in ordine,
// restituisce il risultatoN della prima corrispondenza -- se nessuna
// corrisponde, l'ultimo argomento "spaiato" (dopo l'ultima coppia
// valore/risultato) e' il predefinito, se presente, altrimenti #N/A.
// Value::operator== (Value.cpp) gestisce gia' il confronto fra tipi
// diversi (mai un errore, sempre "non uguale") e il testo senza
// distinzione maiuscole/minuscole, stesso confronto usato altrove
// nell'engine.
void SWITCHFunction(Value *stack, int argCnt, CContainer *cells)
{
	if (CheckForNanParameters(stack, argCnt))
		return;

	if (argCnt < 3)
	{
		stack[0] = gValueNan;
		return;
	}

	Value expr = stack[0];
	int i;
	for (i = 1; i + 1 < argCnt; i += 2)
	{
		if (expr == stack[i])
		{
			stack[0] = stack[i + 1];
			return;
		}
	}

	if (i < argCnt)
		stack[0] = stack[i];
	else
		stack[0] = gNANan;
}

// IFNA(valore, valore_se_na): come IFERRFunction sopra (IFERR/IFERROR),
// ma scatta SOLO su #N/A, non su un errore qualunque -- serve
// GetNanNr per distinguere il numero di errore taggato dentro il NaN
// (vedi Utils.cpp), isnan() da solo non basta.
void IFNAFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (GetDoubleArgument(stack, argCnt, 1, &d) && isnan(d)
		&& GetNanNr(d) == GetNanNr(gNANan) && argCnt >= 2)
		stack[0] = stack[1];
	// altrimenti (nessun #N/A, o valore non numerico): stack[0] resta
	// il valore originale, stesso principio di IFERRFunction sopra.
}

// ISBLANK: stesso identico test di ISNULLFunction sopra (eNoData, una
// cella davvero vuota) -- solo il nome standard Excel, non una nuova
// implementazione. Registrata come una voce a se' nella risorsa 'Func'
// (non un alias in GetFunctionNr come CEILING.MATH/CONCATENATE/LOG10)
// perche' "ISBLANK" entra comunque nel campo funcName[10] a differenza
// di quei tre, piu' lunghi di 9 caratteri.
void ISBLANKFunction(Value *stack, int argCnt, CContainer *cells)
{
	ISNULLFunction(stack, argCnt, cells);
}

void ISERRORFunction(Value *stack, int, CContainer *)
{
	stack[0] = (bool)(stack[0].fType == eNumData && isnan(stack[0].fDouble));
}

void ISNAFunction(Value *stack, int, CContainer *)
{
	stack[0] = (bool)(stack[0].fType == eNumData && isnan(stack[0].fDouble)
		&& GetNanNr(stack[0].fDouble) == GetNanNr(gNANan));
}

// ISFORMULA(riferimento): a differenza delle tre ISxxx sopra (che
// leggono il VALORE gia' calcolato dell'argomento), questa ha bisogno
// del riferimento di cella vero e proprio -- stesso schema di ROW/
// COLUMN sopra in Functions.spreadsheet.cpp (GetRangeArgument +
// GetRangeContainer, mai "cells" da solo: il riferimento puo' vivere
// su un altro foglio, vedi il commento su INDEXFunction li').
//
// LIMITE NOTO (scoperto scrivendo questa funzione, mai emerso prima
// per ROW/COLUMN che hanno lo stesso identico problema): il parser
// (Formula/parser.cpp, "case CELL") genera SEMPRE un valCell per un
// riferimento a una sola cella senza ":", e l'interprete del bytecode
// (Formula.cpp, "case valCell") lo deferenzia SUBITO al valore della
// cella, qualunque sia la funzione che lo riceve -- non esiste modo,
// in questo motore, di far arrivare a una funzione il riferimento
// NUDO invece del valore per un argomento a una sola cella. "=ISFORMULA(B1)"
// restituisce quindi #REF!, non VERO/FALSO: serve un intervallo VERO
// di almeno due celle (es. "B1:B2"), che l'interprete NON deferenzia
// (vedi "case valRange" in Formula.cpp) -- questa funzione legge
// sempre e solo l'angolo in alto a sinistra dell'intervallo dato.
// Superarlo davvero richiederebbe insegnare al parser quali argomenti
// di quali funzioni vogliono un riferimento invece di un valore
// (come fa Excel), una modifica strutturale non scritta qui.
void ISFORMULAFunction(Value *stack, int argCnt, CContainer *cells)
{
	range cRange;

	if (GetRangeArgument(stack, argCnt, 1, &cRange) && cRange.IsValid())
	{
		CContainer *rangeCells = GetRangeContainer(stack, 1, cells);
		stack[0] = (bool)(rangeCells->GetCellFormula(cRange.TopLeft()) != NULL);
	}
	else
		stack[0] = gRefNan;
}

