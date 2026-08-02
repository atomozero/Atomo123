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
	Formatter.template.c


*/
/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.headers.pch++"
 *** TPV (2000-Feb-06) Added Headers Guards
 ***/

#ifndef   FORMATTER_H
#include "Formatter.h"
#endif

#ifndef   CONFIG_H
#include "Config.h"
#endif


void CFormatter::ParseTemplate(const char *inTemplate)
{
	char *sp, *t;
	t = STRDUP(inTemplate);

	if ((sp = strchr(t, ';')) != NULL)
		*sp = 0;

	// Template fatto solo di '0'/'#' (es. "00000", "#00000000000"):
	// intero riempito di zeri a sinistra fino alla larghezza indicata,
	// il caso comune per un numero identificativo (numero fattura,
	// CAP) che deve conservare gli zeri iniziali. Il resto di questa
	// funzione (ereditato da Sum-It) non ha mai avuto il concetto di
	// "riempimento intero" -- solo "$"/"%"/"." per distinguere
	// valuta/percentuale/cifre decimali -- quindi un template cosi'
	// finiva silenziosamente nel ramo "generico" sotto, perdendo gli
	// zeri iniziali: bug reale scoperto confrontando visivamente con
	// Excel vero l'importazione di una fattura reale ("Preavviso N.
	// 00073" mostrato come "73").
	bool isZeroPad = t[0] != 0;
	int zeroPadWidth = 0;
	for (const char *c = t; *c; c++)
	{
		if (*c == '0' || *c == '#')
			zeroPadWidth++;
		else
		{
			isZeroPad = false;
			break;
		}
	}
	if (isZeroPad)
	{
		fFormatID = eZeroPad;
		fDigits = zeroPadWidth;
		fCommas = false;
		FREE(t);
		return;
	}

	// "$" e' l'unico simbolo di valuta mai riconosciuto qui (eredita'
	// da Sum-It, mai esteso): un template con "€" al posto di "$" (il
	// caso comune per una fattura italiana, es. "\€\ ##,###.00")
	// finiva nel ramo eFixed sotto invece di eCurrency -- bug reale
	// scoperto confrontando visivamente con Excel vero l'importazione
	// di una fattura reale (importi mostrati come numeri puri, senza
	// simbolo ne' decimali forzati). "€" e' UTF-8 multi-byte (0xE2
	// 0x82 0xAC): strstr sulla sequenza di byte funziona qui perche'
	// "t" e' gia' una stringa UTF-8 (Excel.pass1.cpp la costruisce con
	// AppendUnicodeAsUTF8/AppendCP1252Byte), non serve decodificarla.
	if (strchr(t, '$') || strstr(t, "\xe2\x82\xac"))
		fFormatID = eCurrency;
	else if (strchr(t, '%'))
		fFormatID = ePercent;
	else
		fFormatID = eGeneral;
	
	fCommas = strchr(t, ',') != NULL;
	
	if ((sp = strchr(t, '.')) != NULL)
	{
		if (fFormatID == eGeneral)
			fFormatID = eFixed;
		
		fDigits = 0;
		while (*++sp == '0')
			fDigits++;
	}
	else
		fDigits = 0;
	
	FREE(t);
} /* CFormatter::ParseTemplate */
