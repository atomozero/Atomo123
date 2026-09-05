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
	Container.graph.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include "Cell.h"
#include "Formula.h"
#include "CellData.h"
#include "EngineViewStub.h"
#include "Value.h"
#include "Container.h"
#include "MyError.h"
#include "BTree.t"
#include "CalcDefs.h"
#include "MThread.h"
#include "StLocker.h"

bool CContainer::CalcCell(const cell& c)
{
	Value val, newVal;
	bool result = false;
	cellmap::iterator i;
	
	if ((i = fCellData.find(c)) != fCellData.end())
	{
		fCalculatingCell = c;
		
		if ((*i).second.mFormula)
		{
			val = (*i).second;
			CFormula((*i).second.mFormula).Calculate(c, newVal, this);

			// Intervalli dinamici (Fase 36): una formula il cui risultato
			// FINALE e' un riferimento (es. "=OFFSET(H11,1,0)" o "=A1:B5"
			// scritte da sole, senza SUM/SUBTOTAL intorno) resta di tipo
			// eRangeData -- ma CellData/Value::operator=(const CellData&)
			// (CellData.cpp) non sanno rappresentare "un intervallo" come
			// valore permanente di una cella, solo numero/testo/booleano/
			// data: il "default" di quello switch la farebbe collassare
			// silenziosamente a eNoData (cella vuota), un dato REALE
			// perso senza nessun avviso. Stessa "intersezione implicita"
			// che Excel applica in questo caso: si prende il valore della
			// cella in alto a sinistra dell'intervallo, esattamente come
			// CFormula::Calculate gia' fa da solo per un valRange
			// LETTERALE degenere (una sola cella) -- qui si generalizza
			// a qualunque intervallo, letterale o calcolato, degenere o
			// no, quando e' il risultato ultimo dell'INTERA formula.
			if (newVal.fType == eRangeData)
			{
				range r = newVal;
				GetValue(r.TopLeft(), newVal);
			}

			if (newVal.fType == eNoData && fInView && fInView->DoesDisplayZero())
				newVal = 0.0;

			StLocker<CContainer> lock(this);
			(*i).second = newVal;
			(*i).second.mStatus = kCalculated;

			result = (val != newVal);
		}
	}
	
	return result;
} /* CContainer::CalcCell */

