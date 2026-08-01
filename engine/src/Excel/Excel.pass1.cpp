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
	Excel.pass1.c

*/

#include <cstdio>
#include <cstring>
#include "Excel.h"
#include "XL_Biff_codes.h"
#include "Excel.colors.h"
#include "EngineViewStub.h"
#include "Utils.h"
#include "FontMetrics.h"
#include "Formatter.h"
#include "CellStyle.h"
#include "Container.h"

void CExcel5Filter::Pass1()
{
	CExcelStream es(fBook);

	short code, len, bofrec;
	int32 offset;

	offset = 0;

	fBook.Seek(offset, SEEK_SET);
	es >> code >> len;
	offset += 4 + len;

	if (code != 0x0809)
		throw CErr("Expected BOF record");

	es >> bofrec >> bofrec;
	if (bofrec != 0x0005) throw CErr("Expected global section");

	while (offset < fBook.BufferLength())
	{
		fBook.Seek(offset, SEEK_SET);
		es >> code >> len;
		offset += 4 + len;

		if (code == 0x0809)
			throw CErr("Unexpected start of new substream");
		else if (code == B_EOF)
			break;

		switch (code)
		{
			case LABEL:
			case RK:
			case RSTRING:
			case MULRK:
			case MULBLANK:
			case BLANK:
			case FORMULA:
				throw CErr("Did not expect data insize globals area");
			default:
				HandleXLRecordForPass1(code, len);
		}
	}

	fBook.Seek(offset, SEEK_SET);
	es >> code >> len;
	offset += 4 + len;

	if (code != 0x0809)
		throw CErr("Expected new substream");

	es >> bofrec >> bofrec;
	if (bofrec != 0x0010) throw CErr("Expected beginning of data for sheet 1");

	while (offset < fBook.BufferLength())
	{
		fBook.Seek(offset, SEEK_SET);
		es >> code >> len;
		offset += 4 + len;

		if (code == 0x0809)
			throw CErr("Unexpected start of new substream");
		else if (code == B_EOF)
			break;

		HandleXLRecordForPass1(code, len);
	}
} // CExcel5Filter::Pass1

void CExcel5Filter::Selection()
{
	CExcelStream es(fBook);

	char p;
	short x;

	es >> p;
	if (p == 3)
	{
		cell curCell;
		es >> curCell.v >> curCell.h;
//		fViewInfo.curCell.v++;
//		fViewInfo.curCell.h++;

		es >> x >> x;
		
		range r;
		char c;

		es >> r.top >> r.bottom;
		r.top++;
		r.bottom++;

		es >> c;
		r.left = c + 1;

		es >> c;
		r.right = c + 1;

		// fCellView e' NULL nei translator headless (nessuna UI
		// collegata, vedi il commento in XlsTranslator.cpp): questi
		// metadati di sola presentazione vengono scartati in quel
		// caso, non e' un errore.
		if (fCellView)
			fCellView->SetSelection(r);
	}
}

void CExcel5Filter::Name()
{
	CExcelStream es(fBook);

	char name[256], c;
	// "c" resta un char CON segno (serve invariato per lo switch sotto
	// e per range.left/right, dove il segno non conta per valori
	// piccoli) -- ma il secondo byte letto qui e' davvero la
	// lunghezza del nome che segue (0-255), va tenuto SENZA segno a
	// parte: letto come char con segno, una lunghezza dichiarata
	// >=128 diventava negativa, e "name[c] = 0" scriveva PRIMA
	// dell'inizio del buffer "name" invece che dopo -- bug reale
	// scoperto aprendo un file .xls di un utente (corrompeva lo stack
	// invece di un crash pulito, manifestandosi come un blocco
	// indefinito piu' avanti).
	unsigned char nameLen;
	short grbit, deflen;

	es >> grbit >> c >> nameLen >> deflen;

	if (deflen == 0)	// dan zal het wel een functie zijn, hoop ik
	{
		fBook.Seek(fBook.Position() + 8, SEEK_SET);
		es.Read(name, nameLen);
		name[nameLen] = 0;

		int funcNr = GetFunctionNr(name);

		// CErr::DoError() (vedi MyError.cpp) mostra un vero MStopAlert
		// modale e aspetta un clic -- appropriato per un errore
		// dell'utente durante l'uso interattivo dell'app, ma non qui:
		// l'importazione di un file .xls prosegue record per record
		// anche quando un nome definito referenzia una funzione che
		// questo motore non conosce (xlName con funcNr=-1 resta
		// comunque un valore valido da inserire in fNames, usato
		// altrove solo se davvero referenziato da una formula). Un
		// alert modale qui bloccherebbe l'intera applicazione in
		// attesa di un clic nel bel mezzo dell'apertura di un file --
		// bug reale scoperto aprendo un file di un utente con un nome
		// del genere: un translator headless (o uno script) resta
		// bloccato per sempre, senza nessuna finestra su cui cliccare.
		if (funcNr < 0)
			fprintf(stderr, "Atomo123: nome definito con funzione sconosciuta: %s\n", name);

		fNames.push_back(xlName(funcNr));
	}
	else
	{
		fBook.Seek(fBook.Position() + 8, SEEK_SET);
		es.Read(name, nameLen);
		name[nameLen] = 0;

		range r;
		es >> c;

		switch (c)
		{
			case 0x3B:
				fBook.Seek(fBook.Position() + 14, SEEK_SET);
			case 0x07:
			case 0x2D:
				es >> r.top;			r.top = (r.top & 0x3FFF) + 1;
				es >> r.bottom;	r.bottom = (r.bottom & 0x3FFF) + 1;
				es >> c;				r.left = c + 1;
				es >> c;				r.right = c + 1;
				break;
			default:
//				throw CErr("Named reference %s too complex: %d", name, c);
				return;
		}
		
		// fCellView e' NULL nei translator headless: il nome resta
		// comunque utilizzabile per risolvere i riferimenti nelle
		// formule (fNames sotto), solo la visualizzazione nella UI
		// viene scartata.
		if (fCellView)
			fCellView->AddNamedRange(name, r);
		fNames.push_back(name);
	}
} 

void CExcel5Filter::Font()
{
	CExcelStream es(fBook);

	short x;
	float size;
	es >> x;
	size = x / 20.0;
	
	short grbit;
	es >> grbit;
	
	short color;
	es >> color;
	
	es >> x;
	font_style fs;
	if (x <= 0x0190)
		if (grbit & 2)
			strcpy(fs, "Italic");
		else
			strcpy(fs, "Roman");
	else
		if (grbit & 2)
			strcpy(fs, "Bold Italic");
		else
			strcpy(fs, "Bold");
		
	es >> x;
	es >> x;
	es >> x;

	// operator>>(char*) legge una stringa lunga fino a 255 byte (il
	// prefisso di lunghezza e' un byte 0-255, vedi il commento in
	// SwapStream.h) in un buffer fornito dal chiamante senza
	// conoscerne la dimensione -- font_family (Font.h) e' pero' molto
	// piu' piccola (B_FONT_FAMILY_LENGTH+1, 64 byte): un nome di font
	// dichiarato piu' lungo di cosi' nel file (visto in pratica, non
	// solo teorico) scriverebbe oltre la fine di "fn". Si legge prima
	// in un buffer da 256 byte, capiente per qualunque lunghezza
	// valida dell'operatore, poi si tronca in modo sicuro.
	char fontName[256];
	es >> (unsigned char *)fontName;

	font_family fn;
	strncpy(fn, fontName, sizeof(fn) - 1);
	fn[sizeof(fn) - 1] = 0;

	rgb_color c = { 0, 0, 0, 0};
	if (color >= 8 && color < 64)
		c = kExcelColorTable[color - 8];
	
	fFonts.push_back(gFontSizeTable.GetFontID(fn, fs, size, c));
} 

void CExcel5Filter::Xf()
{
	CExcelStream es(fBook);
	
	short x;
	ushort y;
	CellStyle style;
	
	style.fLowColor.alpha = 255;
	
	es >> y;
	if (y >= 4) y--;
	style.fFont = fFonts[y];
	
	es >> y;
	
	if (y < 5)
	{
		switch (y)
		{
// gokje... (that means gamble in Dutch)
			case 1: 
				style.fFormat = 3; //eFixed;
				break;

			case 2:
				style.fFormat = 3; //eFixed;
				style.fFormat |= 2 << 4;
				break;

			case 3:
				style.fFormat = 3; //eFixed;
				style.fFormat |= 2 << 4;
				style.fFormat |= 1 << 9;
				break;

			case 4:
				style.fFormat = 1; //eCurrency;
				style.fFormat |= 2 << 4;
				style.fFormat |= 1 << 9;
				break;
				
			case 0:
			default:
				style.fFormat = 0;	// general
				break;
		}
	}
	else	
		style.fFormat = fFormats[y];

	es >> x >> x;
	style.fAlignment = x & 0x07;
	
	fStyles.push_back(gStyleTable.GetStyleID(style));
} 

void CExcel5Filter::HandleXLRecordForPass1(int code, int len)
{
	CExcelStream es(fBook);
	
	short x;
	
	switch (code & 0x00FF)
	{
		case BLANK:
		case RK:
		case FORMULA:
		case NUMBER:
		case LABEL:
		case RSTRING:
		case BOOLERR:
//			fHeaderInfo.cellCount++;
			break;
		case MULRK:
//			fHeaderInfo.cellCount += (len-4)/6;
			break;
		case MULBLANK:
//			fHeaderInfo.cellCount += (len-4)/2;
			break;

		case B_1904:
			es >> x;
			f1904 = (x == 1);
			break;
		case SELECTION:
			Selection();
			break;
		case NAME:
			Name();
			break;
		case DEFAULTROWHEIGHT:
		{
			es >> x;
			if (!(x & 0x01))
			{
				es >> x;
				// fCellView e' NULL nei translator headless: questi
				// metadati di sola presentazione vengono scartati in
				// quel caso (vedi il commento in XlsTranslator.cpp).
				if (fCellView)
					fCellView->GetHeights().SetValue(1, kRowCount,
						ceil(x / 20.0) + 1);
			}
			break;
		}
		case ROW:
		{
			short h;
			es >> x >> h >> h >> h;
			if (fCellView)
				fCellView->GetHeights().SetValue(x + 1, ceil(h / 20) + 1);
			break;
		}
		case FONT:
			Font();
			break;
		case WINDOW2:
		{
			es >> x;
			if (fCellView)
			{
				fCellView->SetShowGrid(x & 0x02);
				fCellView->SetShowBorders(x & 0x04);
			}

			es >> x;
//			fViewInfo.position.v = x + 1;
			es >> x;
//			fViewInfo.position.h = x + 1;
			break;
		}
//		case STYLE:
//			break;
//		case PRINTHEADERS:
//			ReadSwapped(inStream, x);
//			fPrBorders = (x == 1);
//			break;
//		case PRINTGRIDLINES:
//			ReadSwapped(inStream, x);
//			fPrGrid = (x == 1);
//			break;
		case FORMAT:
		{
			char s[256];
			es >> x;
			es >> (unsigned char *)s;

			fFormats.push_back(gFormatTable.GetFormatID(s));
			break;
		}
		case DEFCOLWIDTH:
		{
			es >> x;

			// fCellView e' NULL nei translator headless: questi
			// metadati di sola presentazione vengono scartati in quel
			// caso -- anche perche' be_plain_font->StringWidth
			// richiede una vera connessione all'app_server, che un
			// translator headless non ha (vedi il commento in
			// XlsTranslator.cpp).
			if (fCellView)
				fCellView->GetWidths().SetValue(1, kColCount,
					ceil(x * be_plain_font->StringWidth("x")) + 1);
			break;
		}
		case COLINFO:
		{
			short first, last, wi, f;

			es >> first >> last >> wi >> x >> f;

			// Un file .xls reale puo' contenere un record COLINFO con
			// "first" > "last" (visto in pratica, non solo teorico:
			// bug reale scoperto aprendo un file di un utente, crash
			// riproducibile) -- CRunArray2::SetValue richiede un
			// intervallo ordinato (ASSERT(inStart <= inStop)) e non lo
			// verifica da sola. Un record del genere non descrive
			// nessuna colonna valida: va scartato, non fatto
			// crashare l'intera applicazione.
			if (first <= last)
			{
				if (fCellView)
				{
					if (f & 0x11)
						fCellView->GetWidths().SetValue(first + 1,last + 1,0);
					else
					{
						fCellView->GetWidths().SetValue(first + 1, last + 1,
							floor(be_plain_font->StringWidth("x") * wi / 256));
					}
				}

				fContainer->GetColumnStyles().SetValue(first + 1, last + 1, x);
			}
			break;
		}
		case XF:
			Xf();
			break;
		case SHRFMLA:
		{
			XLSHFormula shxl;
			es >> shxl.rwFirst >> shxl.rwLast >> shxl.colFirst >> shxl.colLast >> x >> shxl.cce;
			
			shxl.p = (char *)calloc(1, shxl.cce);
			if (!shxl.p) throw CErr(1);
			es.Read(shxl.p, shxl.cce);
			
			fSharedFormulas.push_back(shxl);
			break;
		}
	}
} 
