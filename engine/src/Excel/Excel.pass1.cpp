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

#include <algorithm>
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

// Windows-1252, 0x80-0x9F: gli unici byte per cui differisce da
// Latin-1 (0xA0-0xFF coincide byte per byte col codepoint Unicode).
// 0x00 marca le posizioni non assegnate dallo standard CP1252 (rare
// nei file reali): restituito com'e', un controllo C1 innocuo.
static const unsigned short kCP1252HighRange[32] = {
	0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
	0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178
};

void CExcel5Filter::AppendUnicodeAsUTF8(std::string& out, unsigned int codepoint)
{
	if (codepoint < 0x80)
		out += (char)codepoint;
	else if (codepoint < 0x800)
	{
		out += (char)(0xC0 | (codepoint >> 6));
		out += (char)(0x80 | (codepoint & 0x3F));
	}
	else if (codepoint < 0x10000)
	{
		out += (char)(0xE0 | (codepoint >> 12));
		out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
		out += (char)(0x80 | (codepoint & 0x3F));
	}
	else
	{
		out += (char)(0xF0 | (codepoint >> 18));
		out += (char)(0x80 | ((codepoint >> 12) & 0x3F));
		out += (char)(0x80 | ((codepoint >> 6) & 0x3F));
		out += (char)(0x80 | (codepoint & 0x3F));
	}
}

void CExcel5Filter::AppendCP1252Byte(std::string& out, unsigned char b)
{
	unsigned int codepoint = b;
	if (b >= 0x80 && b <= 0x9F)
	{
		unsigned short mapped = kCP1252HighRange[b - 0x80];
		codepoint = mapped != 0 ? mapped : b;
	}
	AppendUnicodeAsUTF8(out, codepoint);
}

// Euristica sulle lettere y/m/d/h/s fuori da apici e parentesi
// quadre, stessa idea (duplicata deliberatamente, vedi il commento in
// Excel.h) di LooksLikeDateFormat in translators/xlsx/XlsxTranslator.cpp
// -- usata solo per i formati PERSONALIZZATI (record FORMAT espliciti):
// i built-in noti (14-22/45-47) sono gia' marcati direttamente da
// InitBuiltinFormats, senza bisogno di ispezionare nessuna stringa.
bool CExcel5Filter::LooksLikeDateFormat(const std::string& code)
{
	bool inQuotes = false;
	bool inBrackets = false;
	for (size_t i = 0; i < code.size(); i++)
	{
		char c = code[i];
		if (!inBrackets && c == '"')
			inQuotes = !inQuotes;
		else if (!inQuotes && c == '[')
			inBrackets = true;
		else if (!inQuotes && c == ']')
			inBrackets = false;
		else if (!inQuotes && !inBrackets)
		{
			if (c == ';')
				return false; // fine della porzione rilevante, nessuna lettera data trovata
			if (c == 'y' || c == 'm' || c == 'd' || c == 'h' || c == 's'
				|| c == 'Y' || c == 'M' || c == 'D' || c == 'H' || c == 'S')
				return true;
		}
	}
	return false;
}

time_t CExcel5Filter::ExcelSerialToTime(double serial, bool date1904)
{
	double epochOffsetDays = date1904 ? 24107.0 : 25569.0;
	double unixSeconds = (serial - epochOffsetDays) * 86400.0;
	return (time_t)(unixSeconds + (unixSeconds >= 0 ? 0.5 : -0.5));
}

// Formati built-in BIFF (0-49, standard ECMA-376/OOXML, invariati fra
// le versioni di Excel): un file reale li usa liberamente SENZA mai
// dichiararli con un proprio record FORMAT, dato che sono impliciti
// -- serve precaricarli prima di leggere il file perche' Xf() li
// referenzia per ifmt (vedi il commento su "fFormats" in Excel.h).
// Solo quelli davvero utili sono elencati: General e i numerici (0-4)
// restano gestiti dal ramo "y < 5" gia' esistente in Xf(), qui
// servono soprattutto percentuale/data/ora (9-22, 45-47) e
// contabilita' (37-49), che un file reale puo' usare senza
// personalizzarli.
void CExcel5Filter::InitBuiltinFormats()
{
	static const struct { unsigned short ifmt; const char* tmpl; bool isDate; } kBuiltins[] = {
		{ 9, "0%", false },
		{ 10, "0.00%", false },
		{ 11, "0.00E+00", false },
		{ 14, "mm-dd-yy", true },
		{ 15, "d-mmm-yy", true },
		{ 16, "d-mmm", true },
		{ 17, "mmm-yy", true },
		{ 18, "h:mm AM/PM", true },
		{ 19, "h:mm:ss AM/PM", true },
		{ 20, "h:mm", true },
		{ 21, "h:mm:ss", true },
		{ 22, "m/d/yy h:mm", true },
		{ 37, "#,##0_);(#,##0)", false },
		{ 38, "#,##0_);[Red](#,##0)", false },
		{ 39, "#,##0.00_);(#,##0.00)", false },
		{ 40, "#,##0.00_);[Red](#,##0.00)", false },
		{ 45, "mm:ss", true },
		{ 46, "[h]:mm:ss", true },
		{ 47, "mmss.0", true },
		{ 48, "##0.0E+0", false },
		{ 49, "@", false },
	};

	for (size_t i = 0; i < sizeof(kBuiltins) / sizeof(kBuiltins[0]); i++)
	{
		fFormats[kBuiltins[i].ifmt] = gFormatTable.GetFormatID(kBuiltins[i].tmpl);
		if (kBuiltins[i].isDate)
			fDateIfmts.insert(kBuiltins[i].ifmt);
	}
}

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

// SST (BIFF8/Excel97+, vedi il commento su SST/LABELSST in
// XL_Biff_codes.h): count(4, totale riferimenti, ignorato) +
// uniqueCount(4) seguiti da "uniqueCount" XLUnicodeRichExtendedString
// -- cch(2) + grbit(1) + [cRun(2) se fRichSt] + [cbExtRst(4) se
// fExtSt] + i caratteri veri e propri (1 byte/carattere se
// "compresso", 2 se no) + [rgRun, cRun*4 byte] + [ExtRst, cbExtRst
// byte]. Una tabella di stringhe reale supera quasi sempre la
// dimensione massima di un record BIFF (~8KB): il resto prosegue in
// uno o piu' record CONTINUE, attraversati qui in modo trasparente da
// "advance" -- compresa la stranezza BIFF8 per cui un CONTINUE che
// interrompe una stringa A META' CARATTERI ricomincia con un byte
// "grbit" (compresso/non compresso) tutto suo, che puo' differire da
// quello della stringa originale.
void CExcel5Filter::ReadSST(int len)
{
	off_t pos = fBook.Position();
	off_t recordEnd = pos + (unsigned short)len;

	// Legge "n" byte grezzi, attraversando un confine CONTINUE se
	// serve (senza reinterpretare il contenuto: usato per i campi a
	// lunghezza fissa e per i dati da scartare, non per i caratteri
	// della stringa, che hanno bisogno di ricontrollare il flag
	// "compresso" a ogni attraversamento -- vedi sotto).
	auto readRaw = [&](void* buf, int n)
	{
		unsigned char* p = (unsigned char*)buf;
		while (n > 0)
		{
			if (pos >= recordEnd)
			{
				fBook.Seek(pos, SEEK_SET);
				short nextCode, nextLen;
				CExcelStream cs(fBook);
				cs >> nextCode >> nextLen;
				if (nextCode != CONTINUE)
					throw CErr("SST troncata: atteso un record CONTINUE");
				pos += 4;
				recordEnd = pos + (unsigned short)nextLen;
			}
			int chunk = (int)std::min((off_t)n, recordEnd - pos);
			fBook.Seek(pos, SEEK_SET);
			if (fBook.Read(p, chunk) != chunk)
				throw CErr("SST troncata: lettura incompleta");
			p += chunk;
			pos += chunk;
			n -= chunk;
		}
	};

	unsigned char header[8];
	readRaw(header, 8);
	long uniqueCount;
	memcpy(&uniqueCount, header + 4, 4);
	uniqueCount = B_LENDIAN_TO_HOST_INT32(uniqueCount);
	if (uniqueCount < 0)
		throw CErr("SST: conteggio stringhe non valido");

	fSST.clear();
	fSST.reserve(uniqueCount);

	for (long i = 0; i < uniqueCount; i++)
	{
		unsigned char strHeader[3];
		readRaw(strHeader, 3);
		unsigned short charCount = strHeader[0] | (strHeader[1] << 8);
		unsigned char flags = strHeader[2];
		bool wide = (flags & 0x01) != 0;
		bool farEast = (flags & 0x04) != 0;	// fExtSt
		bool richText = (flags & 0x08) != 0;	// fRichSt

		unsigned short cRun = 0;
		if (richText)
		{
			unsigned char b[2];
			readRaw(b, 2);
			cRun = b[0] | (b[1] << 8);
		}
		unsigned long cbExtRst = 0;
		if (farEast)
		{
			unsigned char b[4];
			readRaw(b, 4);
			memcpy(&cbExtRst, b, 4);
			cbExtRst = B_LENDIAN_TO_HOST_INT32(cbExtRst);
		}

		std::string text;
		text.reserve(charCount);
		bool curWide = wide;
		for (unsigned short ch = 0; ch < charCount; ch++)
		{
			if (pos >= recordEnd)
			{
				fBook.Seek(pos, SEEK_SET);
				short nextCode, nextLen;
				CExcelStream cs(fBook);
				cs >> nextCode >> nextLen;
				if (nextCode != CONTINUE)
					throw CErr("SST troncata: stringa interrotta senza CONTINUE");
				pos += 4;
				recordEnd = pos + (unsigned short)nextLen;

				unsigned char newFlag;
				readRaw(&newFlag, 1);
				curWide = (newFlag & 0x01) != 0;
			}

			if (curWide)
			{
				unsigned char c2[2];
				readRaw(c2, 2);
				unsigned short u = c2[0] | (c2[1] << 8);
				// Ogni unita' UTF-16 codificata nella corrispondente
				// sequenza UTF-8 -- non gestisce le coppie di surrogati
				// (caratteri oltre il BMP, U+10000 in su), estremamente
				// rari in un foglio di calcolo reale: limite dichiarato,
				// non un troncamento silenzioso come la "riduzione a
				// byte singolo" precedente (che perdeva qualunque
				// carattere fuori dall'ASCII/Latin-1 con un '?' letterale).
				AppendUnicodeAsUTF8(text, u);
			}
			else
			{
				unsigned char c1;
				readRaw(&c1, 1);
				// Compresso = Windows-1252 (vedi il commento su
				// AppendCP1252Byte in Excel.h), non Latin-1/ASCII puro.
				AppendCP1252Byte(text, c1);
			}
		}

		if (richText && cRun > 0)
		{
			std::vector<unsigned char> skip(cRun * 4);
			readRaw(&skip[0], cRun * 4);
		}
		if (farEast && cbExtRst > 0)
		{
			std::vector<unsigned char> skip(cbExtRst);
			readRaw(&skip[0], cbExtRst);
		}

		fSST.push_back(text);
	}

	fBook.Seek(pos, SEEK_SET);
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

	// Il nome del font e' una "ShortXLUnicodeString" (cch a 1 byte,
	// non 2 come le stringhe "lunghe" del resto del BIFF8, es. il
	// template di FORMAT sopra) -- ma resta comunque Unicode: cch(1) +
	// grbit(1, bit0 = wide) + cch caratteri, compressi (1 byte,
	// Windows-1252) o wide (2 byte, UTF-16) secondo grbit. Il vecchio
	// codice leggeva solo un prefisso di lunghezza a un byte e poi
	// copiava "cch" byte grezzi SENZA saltare grbit ne' convertire i
	// caratteri -- per un nome scritto in wide (il caso comune anche
	// per nomi puramente ASCII come "Calibri", osservato in un file
	// reale) il risultato erano pochi byte spazzatura interrotti dal
	// primo byte alto nullo di un carattere UTF-16, es. "Calibri"
	// letto come "\x01C" -- bug reale scoperto confrontando
	// visivamente con Excel vero l'importazione di una fattura reale:
	// nessun font risultava mai in grassetto, perche' il nome
	// spazzatura non corrispondeva a nessun font installato e la
	// sostituzione del sistema perdeva anche lo stile richiesto.
	unsigned char cch, fontGrbit;
	es >> cch >> fontGrbit;
	bool fontWide = (fontGrbit & 0x01) != 0;

	std::string fontNameUtf8;
	for (unsigned char k = 0; k < cch; k++)
	{
		if (fontWide)
		{
			unsigned char c2[2];
			es.Read(c2, 2);
			unsigned short u = c2[0] | (c2[1] << 8);
			AppendUnicodeAsUTF8(fontNameUtf8, u);
		}
		else
		{
			unsigned char c1;
			es >> c1;
			AppendCP1252Byte(fontNameUtf8, c1);
		}
	}

	// font_family (Font.h) e' B_FONT_FAMILY_LENGTH+1 (64 byte): un
	// nome dichiarato piu' lungo (raro ma possibile) va troncato in
	// modo sicuro, non scritto oltre la fine di "fn".
	font_family fn;
	strncpy(fn, fontNameUtf8.c_str(), sizeof(fn) - 1);
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

	unsigned short ifmt;
	es >> ifmt;
	y = ifmt;

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
	{
		// "ifmt" e' un identificativo assoluto (built-in o
		// personalizzato), non un indice sequenziale -- vedi il
		// commento su "fFormats" in Excel.h. Un ifmt mai visto (file
		// che referenzia un built-in raro non precaricato da
		// InitBuiltinFormats, o un indice corrotto) resta al formato
		// generico invece di leggere una voce sbagliata.
		std::map<unsigned short, int>::iterator it = fFormats.find(ifmt);
		style.fFormat = it != fFormats.end() ? it->second : 0;
	}

	es >> x >> x;
	style.fAlignment = x & 0x07;
	// Bit 3 dello stesso byte di allineamento gia' letto sopra (nessun
	// nuovo campo da leggere): testo a capo (Fase 12, CellStyle::
	// fWrapText).
	style.fWrapText = (x & 0x08) != 0;

	// Bordi e sfondo: i restanti 12 byte del record XF (20 byte in
	// tutto, 8 gia' letti sopra), mai letti prima d'ora -- vedi il
	// commento sul confronto con Excel vero nell'intestazione del
	// file. Indent/flags e flag "attributi usati" non servono qui,
	// solo per saltarli alla posizione giusta.
	unsigned char indentFlags, attrUsed;
	es >> indentFlags >> attrUsed;

	unsigned long border1, border2;
	es >> border1 >> border2;

	unsigned short bgWord;
	es >> bgWord;

	// Stile del bordo, 4 bit per lato (0 = nessuno): CellStyle::
	// fTBorderColor/fLBorderColor/fBBorderColor/fRBorderColor sono in
	// realta' booleani nonostante il nome (vedi SheetView.cpp -- un
	// bordo e' sempre disegnato nero pieno, mai col colore/spessore
	// reale), quindi qui basta la sola presenza.
	style.fLBorderColor = (border1 & 0x0F) ? 1 : 0;
	style.fRBorderColor = ((border1 >> 4) & 0x0F) ? 1 : 0;
	style.fTBorderColor = ((border1 >> 8) & 0x0F) ? 1 : 0;
	style.fBBorderColor = ((border1 >> 12) & 0x0F) ? 1 : 0;

	// Sfondo: un pattern di riempimento diverso da zero (7 bit alti
	// di border2) usa il colore di primo piano del pattern (7 bit
	// bassi di bgWord) come sfondo pieno approssimato -- l'app non
	// distingue i pattern tratteggiati/a righe da un riempimento
	// pieno, la stessa approssimazione usata per i bordi sopra.
	unsigned int fillPattern = (border2 >> 25) & 0x7F;
	if (fillPattern != 0)
	{
		unsigned short fgColorIdx = bgWord & 0x7F;
		if (fgColorIdx >= 8 && fgColorIdx < 64)
		{
			rgb_color fill = kExcelColorTable[fgColorIdx - 8];
			fill.alpha = 255;
			style.fLowColor = fill;
		}
	}

	fStyles.push_back(gStyleTable.GetStyleID(style));
	fXfIsDate.push_back(fDateIfmts.count(ifmt) > 0);
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

			// Altezza di riga esplicita (record ROW, "h" e' l'ultimo
			// dei tre valori appena letti sopra: colMic/colMac
			// scartati, miyRw tenuto -- vedi il commento sul layout
			// del record BIFF8 nel case COLINFO poco sopra) in twips
			// (1/20 di punto). Catturata SEMPRE, non solo con una
			// vista viva (fCellView e' sempre NULL in pratica, vedi
			// il commento sopra) -- stesso principio di fColWidths
			// per COLINFO: un translator headless la recupera dopo
			// Translate() per scriverla nel formato nativo (vedi
			// WriteASCD in XlsTranslator.cpp). h/15.0 converte da
			// twips a pixel assumendo 96 DPI (la stessa risoluzione
			// gia' assunta altrove in questo file per le immagini
			// incorporate, vedi Excel.escher.cpp): 1 twip = 1/20
			// punto, 1 punto = 96/72 pixel a 96 DPI, quindi
			// 1 twip = 96/(72*20) = 1/15 pixel. Bug reale scoperto
			// confrontando con Excel vero una fattura reale: ogni
			// riga tornava sempre all'altezza predefinita (kRowHeight,
			// 20px in ui/src/SheetView.h), ignorando qualunque riga
			// ridimensionata a mano nel file originale.
			if (h > 0)
				fRowHeights.push_back(std::make_pair(x + 1, h / 15.0f));
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
		case SST:
			ReadSST(len);
			break;
		case FORMAT:
		{
			// BIFF8 (Excel97+): il template del formato e' una vera
			// stringa Unicode (cch a 2 byte + grbit + caratteri
			// compressi/wide), non la stringa "compressa" a un byte di
			// lunghezza usata altrove nel filtro per i nomi di font
			// (operator>>(unsigned char*), che qui leggerebbe solo il
			// byte basso di "cch" come lunghezza e i primi caratteri
			// come dati a caso) -- bug reale scoperto confrontando
			// visivamente con Excel vero l'importazione di una fattura
			// reale: ogni formato personalizzato risultava vuoto o
			// illeggibile, facendo scattare il ramo "formato generico"
			// anche per celle con un formato esplicito nel file.
			unsigned short ifmt, cch;
			unsigned char grbit;
			es >> ifmt >> cch >> grbit;
			bool wide = (grbit & 0x01) != 0;

			std::string tmpl;
			for (unsigned short k = 0; k < cch; k++)
			{
				if (wide)
				{
					unsigned char c2[2];
					es.Read(c2, 2);
					unsigned short u = c2[0] | (c2[1] << 8);
					AppendUnicodeAsUTF8(tmpl, u);
				}
				else
				{
					unsigned char c1;
					es >> c1;
					AppendCP1252Byte(tmpl, c1);
				}
			}

			fFormats[ifmt] = gFormatTable.GetFormatID(tmpl.c_str());
			if (LooksLikeDateFormat(tmpl))
				fDateIfmts.insert(ifmt);
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

				// "fColWidths" e' catturata anche senza fCellView (il
				// caso comune: translator headless, vedi il commento
				// in XlsTranslator.cpp), a differenza della riga sopra
				// che aggiorna la vista solo se ne esiste gia' una
				// viva -- serve a WriteASCD per scrivere la sezione
				// larghezze di colonna, finora sempre vuota per XLS
				// (limite dichiarato, non piu' vero da questa fase:
				// una colonna troppo stretta per il testo reale che
				// contiene, es. dello studio tagliato a
				// "Studio Tecnicc", era un problema reale confrontando
				// visivamente con Excel vero l'importazione di una
				// fattura reale). Le colonne nascoste (f&0x11) restano
				// escluse, stesso principio della vista sopra: una
				// larghezza 0 esplicita non aiuterebbe la resa.
				if (!(f & 0x11))
				{
					float widthPixels = floor(be_plain_font->StringWidth("x") * wi / 256);
					for (int col = first + 1; col <= last + 1; col++)
						fColWidths.push_back(std::make_pair(col, widthPixels));
				}

				fContainer->GetColumnStyles().SetValue(first + 1, last + 1, x);
			}
			break;
		}
		case XF:
			Xf();
			break;
		case MSODRAWINGGROUP:
			HandleMsoDrawingGroup(len);
			break;
		case MSODRAWING:
			HandleMsoDrawing(len);
			break;
		case MERGEDCELLS:
		{
			// n(2) + n * (rowFirst(2) rowLast(2) colFirst(2) colLast(2)),
			// tutti 0-based come il resto del BIFF -- convertiti a
			// 1-based (c.v++/c.h++, la stessa convenzione usata per
			// ogni altra cella) prima di registrarli.
			unsigned short n;
			es >> n;
			for (unsigned short i = 0; i < n; i++)
			{
				unsigned short rowFirst, rowLast, colFirst, colLast;
				es >> rowFirst >> rowLast >> colFirst >> colLast;
				range r;
				r.Set(colFirst + 1, rowFirst + 1, colLast + 1, rowLast + 1);
				fMergedRanges.push_back(r);
			}
			break;
		}
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
