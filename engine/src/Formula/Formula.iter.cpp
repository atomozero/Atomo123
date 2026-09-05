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
	Formula.iter.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

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

#ifndef   MYERROR_H
#include "MyError.h"
#endif

#ifndef   NAMETABLE_H
#include "NameTable.h"
#endif

#ifndef   SET_H
#include "Set.h"
#endif

#ifndef   STRINGTABLE_H
#include "StringTable.h"
#endif

#include <support/Debug.h>
#include <cstring>

CFormulaIterator::CFormulaIterator(void *inFormula, cell inLocation)
{
	fIndex = 0;
	fString = (int32 *)inFormula;
	fLocation = inLocation;
} /* CFormulaIterator::CFormulaIterator */

bool CFormulaIterator::Next(cell& ioCell)
{
	int l;
	PFToken theOpcode;

	if (fIndex < 0) { /* fIndex is kleiner dan nul om aan te geven dat
								   we in een range zitten */
		ioCell.h++;
		if (ioCell.h > fRange.right) {
			ioCell.h = fRange.left;
			ioCell.v++;
			if (ioCell.v > fRange.bottom)
				fIndex = -fIndex;
			else
				return true;
		}
		else
			return true;
	}

	do {
		theOpcode = (PFToken)fString[fIndex];
		fIndex++;

		switch(theOpcode) {
			case opFunc:
				fIndex += sizeof(FuncCallData) / kPFWordSize;
				break;
			case valNum:
			case valPerc:
				fIndex += sizeof(double) / kPFWordSize;
				break;
			case valTime:
				fIndex += sizeof(time_t) / kPFWordSize;
				break;
			case valBool:
				fIndex++;
				break;
			case valStr:
			case valName:
				l = 1 + strlen((char *)(fString + fIndex));
				if (l & kPFAlignBits)
					l = (l & ~kPFAlignBits) + kPFWordSize;
				fIndex += l / kPFWordSize;
				break;
			case valCell:
				ioCell = ((cell *)(fString + fIndex))->GetFlatCell(fLocation);
				fIndex += sizeof(cell) / kPFWordSize;
				break;
			case valRange:
			// Intervalli dinamici ("OFFSET(H11,1,0):OFFSET(H15,-1,0)"):
			// stesso trattamento esatto di valRange, che sia il
			// riferimento base di OFFSET o un operando di opRangeOp --
			// il bytecode e' postfisso/piatto, quindi questi token
			// vengono incontrati qui indipendentemente da quale
			// funzione/operatore li consuma dopo (mai "dentro" a
			// opFunc, solo prima di esso nello stream), esattamente
			// come un valRange letterale gia' faceva. E' questo che fa
			// funzionare GetPrecedents/GetDependents e l'ordinamento di
			// ricalcolo di CCalcStack anche per un intervallo dinamico,
			// senza bisogno di nessun'altra modifica a questo iteratore.
			case valRefRange:
				fRange = ((range *)(fString + fIndex))->GetFlatRange(fLocation);
				fIndex += sizeof(range) / kPFWordSize;
				fIndex = -fIndex;
				ioCell = fRange.TopLeft();
				break;
			case valXRef:
				// Punta a un CContainer diverso: irrilevante per
				// questo iteratore, che segue le dipendenze SOLO
				// dentro il container corrente (vedi CCalcStack) --
				// si salta la dimensione senza farlo contare come
				// riferimento trovato (non e' fra le condizioni del
				// while sotto).
				l = strlen((char *)(fString + fIndex)) + 1 + sizeof(cell);
				if (l & kPFAlignBits)
					l = (l & ~kPFAlignBits) + kPFWordSize;
				fIndex += l / kPFWordSize;
				break;
			case valXRange:
				l = strlen((char *)(fString + fIndex)) + 1 + sizeof(range);
				if (l & kPFAlignBits)
					l = (l & ~kPFAlignBits) + kPFWordSize;
				fIndex += l / kPFWordSize;
				break;
			default:
				// there was a warning about not all enum values handled in
				// switch statement.
				break;
		}
	}
	while (theOpcode != valCell && theOpcode != valRange && theOpcode != valRefRange
		&& theOpcode != opEnd);

	return (theOpcode == valCell || theOpcode == valRange || theOpcode == valRefRange);
} /* CFormulaIterator::Next */

void CFormulaIterator::SetData(const IterData& inData)
{
	memcpy(this, &inData, sizeof(CFormulaIterator));
} /* CFormulaIterator::SetData */

void CFormulaIterator::GetData(IterData& outData)
{
	memcpy(&outData, this, sizeof(CFormulaIterator));
} /* CFormulaIterator::GetData */
