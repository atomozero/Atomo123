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
	CellStyle.h
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/
/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.pch++"
 ***/

#ifndef CELLSTYLE_H
#define CELLSTYLE_H

#include <map>

#include <SupportDefs.h>
#include <GraphicsDefs.h>

enum EAlignment {
	eAlignGeneral,
	eAlignLeft,
	eAlignCenter,
	eAlignRight,
	eAlignFill,
	eAlignJustify
};

struct CellStyle {
	int fFormat;
	int fFont;
	bool fLocked;
	bool fHidden;
	char fAlignment;
	rgb_color fLowColor;  // colore di sfondo della cella
	rgb_color fHighColor; // colore del testo della cella
	// Spessore del bordo per lato (Fase 11/13): il nome "colore" e'
	// ereditato dal codice storico di Sum-It (mai davvero implementato
	// ne' li' ne' qui prima della Fase 13) -- 0 = nessun bordo, 1 =
	// sottile, 2 = medio, 3 = spesso. Un valore gia' scritto da un file
	// .ascd della Fase 11 (sempre 0 o 1) resta valido cosi' com'e':
	// nessun cambiamento di formato, solo un significato piu' ampio
	// per lo stesso byte. Il colore VERO del bordo (uguale per tutti e
	// quattro i lati di una cella: la stragrande maggioranza dei fogli
	// reali non ha bisogno di un colore diverso per lato) e' invece
	// fBorderColor sotto.
	uchar fTBorderColor;
	uchar fLBorderColor;
	uchar fBBorderColor;
	uchar fRBorderColor;
	rgb_color fBorderColor; // colore condiviso da tutti i lati con bordo (Fase 13, predefinito nero)
	bool fUnderline; // BFont non ha un attributo sottolineato nativo (solo stile del font, vedi Fase 12): disegnato a mano in SheetView, non parte della tripla famiglia/stile/dimensione di fFont
	bool fWrapText; // a-capo del testo per larghezza di colonna (Fase 12): il motore non fa layout di testo, solo la UI (SheetView) sa disegnare piu' righe e far crescere l'altezza di riga di conseguenza

	CellStyle();
	bool operator==(const CellStyle& cs) const;
};

class CStyleTable {
public:
	CStyleTable();
	
	int GetStyleID(const CellStyle& cf);
	const CellStyle& operator[](int indx);

	long Count() const
		{ return fNextStyleID + 1; };
	
private:
	std::map<int,CellStyle> fStyles;
	int fNextStyleID;
};

extern CStyleTable gStyleTable;

#endif
