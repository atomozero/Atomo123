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
	FontMetrics.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#ifndef   FONTMETRICS_H
#include <cstring>
#include "FontMetrics.h"
#endif

#ifndef   BTREE_T
#include "BTree.t"
#endif

#ifndef   PREFERENCES_H
#include "Preferences.h"
#endif

#ifndef   UTF_SUPPORT_H
#include "utf-support.h"
#endif

#ifndef   FONTSTYLE_H
#include "FontStyle.h"
#endif


#include <StLocker.h>

#include <support/Debug.h>

#include <Autolock.h>

CFontSizeTable gFontSizeTable;

CFontMetrics::CFontMetrics()
{
	fFontStyle = NULL;
} // CFontMetrics::CFontMetrics

CFontMetrics::CFontMetrics(const char *inFamily, const char *inStyle,
	float inFontSize, rgb_color inFontColor)
{
	// "inFamily" (es. un font Windows come "Calibri", tipico nei file
	// .xls/.xlsx reali) puo' non essere installato su questo sistema.
	// Il codice storico rilevava questo caso confrontando il nome
	// riletto con la stringa letterale "<unknown family>"/
	// "<unknown style>" -- una convenzione di BeOS R5 che Haiku non
	// segue piu': qui SetFamilyAndStyle fallita sostituisce
	// silenziosamente sia famiglia CHE stile col font di sistema in
	// un solo colpo (senza passare da nessuna stringa "<unknown...>"
	// rilevabile), perdendo lo stile richiesto anche quando la
	// famiglia di ripiego lo supporterebbe benissimo -- bug reale
	// scoperto confrontando visivamente con Excel vero l'importazione
	// di una fattura reale: nessun testo risultava mai in grassetto.
	// Il valore di ritorno di SetFamilyAndStyle e' il segnale
	// affidabile su Haiku: se fallisce, si riprova esplicitamente con
	// la famiglia di ripiego ma lo STILE originale, prima di arrendersi
	// anche su quello.
	status_t err = fFont.SetFamilyAndStyle(inFamily, inStyle);
	if (err != B_OK)
	{
		// gPrefs puo' essere NULL in un contesto senza una vera
		// App::App() (translator headless con solo un BApplication
		// minimo, vedi il commento in XlsxTranslator.cpp) -- be_plain_font
		// resta comunque un ripiego valido, stesso principio gia' usato
		// in Container.cpp per lo stesso motivo.
		font_family sysFamily;
		font_style sysStyle;
		be_plain_font->GetFamilyAndStyle(&sysFamily, &sysStyle);
		const char *fallbackFamily = gPrefs
			? gPrefs->GetPrefString("defdoc font family", sysFamily) : sysFamily;

		err = fFont.SetFamilyAndStyle(fallbackFamily, inStyle);
		if (err != B_OK)
		{
			const char *fallbackStyle = gPrefs
				? gPrefs->GetPrefString("defdoc font style", sysStyle) : sysStyle;
			fFont.SetFamilyAndStyle(fallbackFamily, fallbackStyle);
		}
	}

	fFont.GetFamilyAndStyle(&fFamily, &fStyle);
	fFont.SetSize(fSize = inFontSize);

	fFontColor = inFontColor;
	fFontStyle = CFontStyle::Locate(fFamily, fStyle, fSize);
} /* CFontMetrics::CFontMetrics */

CFontMetrics::CFontMetrics(const CFontMetrics& m)
{
	fFont = m.fFont;
	fFontStyle = m.fFontStyle;
	strcpy(fFamily, m.fFamily);
	strcpy(fStyle, m.fStyle);
	fFontColor = m.fFontColor;
	fSize = m.fSize;
} // CFontMetrics::CFontMetrics

float CFontMetrics::operator[] (int inChar) const
{
	char s[2] = { 0, 0 };
	s[0] = inChar;

	if (!fFontStyle)
	{
		/* Nessun font reale caricato (caso normale nella libreria
		   engine isolata, dove nessuna CCellView popola mai
		   gFontSizeTable tramite GetFontID). be_plain_font->StringWidth
		   richiede una connessione all'app_server: senza (motore
		   headless) la chiamata si blocca in attesa di una risposta
		   che non arriva mai. Si restituisce una stima fissa
		   ragionevole invece di interrogare l'Interface Kit. */
		return 8.0f;
	}

	return fFontStyle->CharWidth(s);
} /* CFontMetrics::operator[] */

float CFontMetrics::StringWidth(const char *inString) const
{
	int i = 0;
	float result = 0;

	if (!fFontStyle)
		return strlen(inString) * 8.0f;

	while (inString[i])
	{
		result += fFontStyle->CharWidth(inString + i);
		i += mcharlen(inString + i);
	}

	return result;
} /* CFontMetrics::StringWidth */

void CFontMetrics::SetFontSizeColor(BView* inView)
{
	/* Era un no-op totale: "il disegno e' compito della UI, non del
	   motore" era vero solo a meta'. La UI (SheetView::Draw/
	   GetCellResult, vedi CContainer/SheetView.cpp) chiama
	   SetFontID -> SetFontSizeColor proprio per applicare famiglia/
	   stile/DIMENSIONE alla view prima di disegnare -- ma senza
	   questa chiamata a BView::SetFont, il font della view non
	   cambiava mai: ogni cella disegnava col font predefinito
	   dell'ultima SetFont esplicita (o quello di sistema), a
	   prescindere da CellStyle::fFont. Bug reale scoperto
	   confrontando visivamente con Excel vero l'importazione di una
	   fattura reale: un'intestazione a 20pt appariva alla stessa
	   dimensione del testo normale circostante -- non solo la
	   dimensione, lo stesso meccanismo copre anche grassetto/corsivo/
	   famiglia, mai davvero verificati a schermo finora (solo a
	   livello di dato esportato, es. il campo "stile" della sezione
	   font di ASCD). BView::SetFont non richiede un app_server "vivo"
	   per il solo aggiornamento locale dello stato (serve solo per un
	   ridisegno effettivo, gia' gestito altrove) -- e libengine.a
	   collega gia' -lbe ovunque (motore, translator, UI), quindi
	   nessun nuovo vincolo di link. "inView" resta comunque
	   controllato: SetFontID puo' in teoria essere chiamato con una
	   view non ancora valida. */
	if (inView)
		inView->SetFont(&fFont);
} /* CFontMetrics::SetFontSize */

bool CFontMetrics::operator==(const CFontMetrics& inOther)
{
	/* In origine confrontava *(long*)&fFontColor: rgb_color e' una
	   struct da 4 byte, ma su Haiku x86_64 sizeof(long)==8, quindi il
	   cast leggeva 4 byte oltre la struct (undefined behavior). Si usa
	   memcmp sulla dimensione reale della struct. */
	if (fFont != inOther.fFont ||
		memcmp(&fFontColor, &inOther.fFontColor, sizeof(fFontColor)) != 0)
		return false;
	else
		return true;
} /* CFontMetrics::operator== */

//////////////////////////////////////////////////////////
// 
// FontSizeTable
//
CFontSizeTable::CFontSizeTable()
{
} /* CFontSizeTable::CFontSizeTable */

ulong CFontSizeTable::GetFontID(const char *fontName, const char *fontStyle,
		float fontSize, rgb_color fontColor)
{
	StLocker<CFontSizeTable> lock(this);
	std::vector<CFontMetrics>::iterator i;
	CFontMetrics ns(fontName, fontStyle, fontSize, fontColor);
	
	for (i = fFonts.begin(); i != fFonts.end(); i++)
	{
		if ((*i) == ns)
			return i - fFonts.begin();
	}
	
	fFonts.push_back(ns);
	return fFonts.size() - 1;
} /* CFontSizeTable::GetFontID */

void CFontSizeTable::SetFontID(BView *view, ulong formatID,
	CFontMetrics **outMetrics)
{
	BAutolock lock(this);
	
	if (formatID < 0 || formatID >= fFonts.size())
	{
		ASSERT(false);
		formatID = 0;
	}
	
	fFonts[formatID].SetFontSizeColor(view);
	if (outMetrics)
		*outMetrics = &fFonts[formatID];
} /* CFontSizeTable::SetFontID */

void CFontSizeTable::GetFontInfo(ulong formatID,
	font_family *fontName, font_style *fontStyle, float *fontSize,
	rgb_color *fontColor)
{
	CFontMetrics *fm;
	
	BAutolock lock(this);
	fm = &fFonts[formatID];
	fm->fFont.GetFamilyAndStyle(fontName, fontStyle);
	*fontSize = fm->fFont.Size();
	if (fontColor) *fontColor = fm->fFontColor;
} /* CFontSizeTable::GetFontAndSize */
