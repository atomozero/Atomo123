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
	Excel.pass2.c
	$Log: Excel.pass2.cpp,v $
	Revision 1.2  2000/05/13 19:20:33  svenweidauer
	i've removed all warnings now (i hope)
	
*/

#include <cstdio>
#include <string>
#include <support/Debug.h>
#include "Excel.h"
#include "XL_Biff_codes.h"
#include "FileFormat.h"
#include "XL_Ptg.h"
#include "MyMath.h"
#include "Container.h"
#include "Formula.h"

void CExcel5Filter::Pass2()
{
	CExcelStream es(fBook);
	
	short code, len;
	ssize_t offset;
	
	offset = 0;

	fBook.Seek(offset, SEEK_SET);
	es >> code >> len;
	offset += 4 + len;

	while (offset < fBook.BufferLength())
	{
		fBook.Seek(offset, SEEK_SET);
		es >> code >> len;
		offset += 4 + len;

		if (code == B_EOF)
			break;

		HandleXLRecordForPass2(code, len);
	}

	fBook.Seek(offset, SEEK_SET);
	es >> code >> len;
	offset += 4 + len;

	while (offset < fBook.BufferLength())
	{
		fBook.Seek(offset, SEEK_SET);
		es >> code >> len;
		offset += 4 + len;

		if (code == B_EOF)
			break;

		HandleXLRecordForPass2(code, len);
	}
} // CExcel5Filter::Pass2

void CExcel5Filter::HandleXLRecordForPass2(int code, int len)
{
	CExcelStream es(fBook);
	
	Value v;
	cell c;
	ushort style;
	
	switch (code & 0x00FF)
	{
		case BLANK:	
		{
			es >> c.v >> c.h >> style;
			c.v++;
			c.h++;
			fContainer->NewCell(c, v, NULL);
			fContainer->SetCellStyleNr(c, fStyles[style]);
			break;
		}
		case NUMBER:
		{
			es >> c.v >> c.h >> style;
			c.v++;
			c.h++;

			double d;
			es >> d;
			// Il formato "sembra" una data (built-in 14-22/45-47 o
			// personalizzato, vedi InitBuiltinFormats/LooksLikeDateFormat
			// in Excel.pass1.cpp): converte il seriale Excel in un vero
			// Value(time_t) invece di un numero grezzo -- necessario
			// perche' CFormatter::FormatValue formatta come data solo
			// un Value gia' di tipo eTimeData, mai un eNumData con un
			// fFormat che assomiglia a una data (vedi il commento in
			// Formatter.cpp).
			if (style < fXfIsDate.size() && fXfIsDate[style])
				v = ExcelSerialToTime(d, f1904);
			else
				v = d;

			fContainer->NewCell(c, v, NULL);
			fContainer->SetCellStyleNr(c, fStyles[style]);
			break;
		}
		case LABEL:
		case RSTRING:
		{
			char label[512];
			short l;

			es >> c.v >> c.h >> style >> l;
			c.v++;
			c.h++;

			// "l" con segno: una lunghezza dichiarata >=32768 (rara ma
			// possibile, il campo BIFF e' un intero senza segno)
			// diventerebbe negativa e supererebbe il solo controllo
			// "> 511" sopra, arrivando a es.Read/label[l]=0 con un
			// indice negativo -- stesso principio delle altre difese
			// aggiunte in questa fase.
			if (l > 511 || l < 0) l = 511;
			es.Read(label, l);
			label[l] = 0;

			// LABEL/RSTRING (BIFF5/7, precedenti a SST) sono stringhe
			// "compresse" a un byte per carattere in Windows-1252, non
			// UTF-8 (vedi il commento su AppendCP1252Byte in Excel.h) --
			// riconvertite qui prima di passarle al motore.
			std::string utf8;
			for (short k = 0; k < l; k++)
				AppendCP1252Byte(utf8, (unsigned char)label[k]);
			v = utf8.c_str();

			fContainer->NewCell(c, v, NULL);
			fContainer->SetCellStyleNr(c, fStyles[style]);
			break;
		}
		case LABELSST:
		{
			// Vedi il commento su SST/LABELSST in XL_Biff_codes.h e su
			// ReadSST in Excel.pass1.cpp: qui c'e' solo l'indice nella
			// tabella (fSST), popolata durante Pass1. Un indice fuori
			// dai limiti (file corrotto, o SST non ancora vista per un
			// ordine di record inatteso) da' una cella vuota invece di
			// leggere fuori dal vector.
			long isst;
			es >> c.v >> c.h >> style >> isst;
			c.v++;
			c.h++;

			if (isst >= 0 && (size_t)isst < fSST.size())
				v = fSST[isst].c_str();

			fContainer->NewCell(c, v, NULL);
			fContainer->SetCellStyleNr(c, fStyles[style]);
			break;
		}
		case BOOLERR:
		{
			es >> c.v >> c.h >> style;
			c.v++;
			c.h++;

			char b, f;
			es >> b >> f;
			
			if (f)
			{
				switch (b)
				{
//					case 0: d = gRefNan; break;
					case 7: v = Nan(8); /*gDivNan;*/ break;
					case 15: v = Nan(7); /*gValueNan;*/ break;
					case 23: v = Nan(3); /*gRefNan;*/ break;
					case 29: v = Nan(3); /*gRefNan;*/ break;
//					case 36: v = nan(7); /*gValueNan;*/ break;
					case 42: v = Nan(15); /*gNANan;*/ break;
					default: v = Nan(1); /*gErrorNan;*/ break;
				}
				fContainer->NewCell(c, v, NULL);
				fContainer->SetCellStyleNr(c, fStyles[style]);
			}
			else
			{
				v = (bool)(b != 0);
				fContainer->NewCell(c, v, NULL);
				fContainer->SetCellStyleNr(c, fStyles[style]);
			}
			break;
		}
		case RK:
		{
			es >> c.v >> c.h >> style;
			c.v++;
			c.h++;

			long l;
			es >> l;
			
			double d;
			
			if (l & 0x02)
				d = (double)(l>>2);
			else
			{
				long long L = l & 0xFFFFFFFC;
				L <<= 32;
				memcpy(&d, &L, 8);
			}
			if (l & 0x01)
				d /= 100;

			// Vedi il commento sulla stessa verifica nel caso NUMBER
			// sopra.
			if (style < fXfIsDate.size() && fXfIsDate[style])
				v = ExcelSerialToTime(d, f1904);
			else
				v = d;
			fContainer->NewCell(c, v, NULL);
			fContainer->SetCellStyleNr(c, fStyles[style]);
			break;
		}
		case FORMULA:
		{
			es >> c.v >> c.h >> style;
			c.v++;
			c.h++;

			short cce, grbit, num[4];
			long chn;

			es >> num[3] >> num[2] >> num[1] >> num[0] >> grbit >> chn >> cce;

			if (num[0] == -1)
			{
				switch (num[3])
				{
					case 0:	// text cell
						fContainer->NewCell(c, v, NULL);
						fContainer->SetCellStyleNr(c, fStyles[style]);
						break;
					case 1: // bool cell
						v = (num[1] != 0);
						fContainer->NewCell(c, v, NULL);
						fContainer->SetCellStyleNr(c, fStyles[style]);
						break;
					case 2: // err cell
					{
						switch (num[1] >> 8)
						{
		//					case 0: d = gRefNan; break;
							case 7: v = Nan(8); /*gDivNan;*/ break;
							case 15: v = Nan(7); /*gValueNan;*/ break;
							case 23: v = Nan(3); /*gRefNan;*/ break;
							case 29: v = Nan(3); /*gRefNan;*/ break;
		//					case 36: v = Nan(7); /*gValueNan;*/ break;
							case 42: v = Nan(15); /*gNANan;*/ break;
							default: v = Nan(1); /*gErrorNan;*/ break;
						}
						fContainer->NewCell(c, v, NULL);
						fContainer->SetCellStyleNr(c, fStyles[style]);
						break;
					}
					default:
						// Tipo di risultato non riconosciuto (es. il
						// marcatore "non ancora calcolato" di alcuni
						// strumenti che scrivono .xls senza mai aprirlo
						// in Excel, come xlwt): non e' un errore fatale,
						// il ricalcolo vero dal token stream della
						// formula (subito sotto) e' comunque la fonte
						// autorevole del valore -- questo campo e' solo
						// una pre-compilazione best-effort dalla cache
						// del file. Cella vuota invece di interrompere
						// l'intera importazione con un'eccezione (bug
						// reale scoperto verificando il rilevamento
						// delle formule con un file di prova).
						fContainer->NewCell(c, v, NULL);
						fContainer->SetCellStyleNr(c, fStyles[style]);
						break;
				}
			}
			else
			{
				v = B_LENDIAN_TO_HOST_DOUBLE(*(double *)num);
				fContainer->NewCell(c, v, NULL);
				fContainer->SetCellStyleNr(c, fStyles[style]);
			}
			
			try
			{
				cell shared;
				CFormula form;
				shared.v = shared.h = 0;
				char o;

// grbit & 0x08 does not seem to work as advertised...
				
				es >> o;
				if (o == ptgExp)
				{
					es >> shared.v >> shared.h;
					
					int i;
					for (i = 0; i < fSharedFormulas.size(); i++)
					{
						if (fSharedFormulas[i].rwFirst == shared.v && fSharedFormulas[i].colFirst == shared.h)
							break;
					}
	
					if (i == fSharedFormulas.size())
						throw CErr("Could not find shared formula %d,%d for cell %d,%d",	
							shared.v, shared.h, c.v, c.h);				
					
					ParseXLFormula(form, c, shared, fSharedFormulas[i].p, fSharedFormulas[i].cce);
				}
				else
				{
					fBook.Seek(fBook.Position() - 1, SEEK_SET);
					void *p = malloc(cce);
					es.Read(p, cce);
					ParseXLFormula(form, c, shared, p, cce);
					free(p);
				}
				
				// Se il ricalcolo fallisce (es. una formula che referenzia
				// un nome definito che questo motore non e' riuscito a
				// risolvere, vedi il commento su "funcNr < 0" in Name()
				// sopra), la cella mantiene il valore già impostato poco
				// sopra dalla cache BIFF (lo stesso valore che Excel
				// stesso ha scritto l'ultima volta che ha calcolato il
				// file) invece di essere sovrascritta con la stringa
				// letterale "!ERROR" -- bug reale scoperto confrontando
				// visivamente con Excel vero l'importazione di una
				// fattura reale: celle che in Excel restavano vuote
				// mostravano "!ERROR" in Atomo123.
				bool calcFailed = false;
				try
				{
					form.Calculate(c, v, fContainer);
					fContainer->SetValue(c, v);
				}
				catch (...)
				{
					calcFailed = true;
				}

				if (!calcFailed)
					fContainer->SetCellFormula(c, form.DetachString());
				else
					form.Clear();	// libera il buffer di "form" senza attaccarlo alla cella
			}
			catch (CErr e) {
				puts(e);
//				e.DoError();
			}
			break;
		}
		case STRING:
		{
			char label[512];
			short l;

			es >> l;
			if (l > 511 || l < 0) l = 511;
			es.Read(label, l);
			label[l] = 0;

			// Stessa conversione Windows-1252 -> UTF-8 di LABEL/RSTRING
			// sopra: anche il risultato testuale di una formula (es.
			// CONCATENATE) puo' contenere caratteri accentati.
			std::string utf8;
			for (short k = 0; k < l; k++)
				AppendCP1252Byte(utf8, (unsigned char)label[k]);
			v = utf8.c_str();
			fContainer->SetValue(c, v);
			break;
		}
		case MULRK:
		{
			long l;
			double d;
			
			es >> c.v >> c.h;
			c.v++; c.h++;

			int i = (len-4)/6;
			// "len" viene dal record letto dal file: un valore
			// corrotto/negativo darebbe un "i" negativo, e
			// "while (i--)" su un intero negativo non finisce mai
			// (continua a decrescere) -- bug reale scoperto aprendo
			// un file .xls di un utente, stesso principio delle altre
			// verifiche difensive di questa fase.
			if (i < 0) i = 0;

			while (i--)
			{
				es >> style >> l;

				if (l & 0x02)
					d = (double)(l>>2);
				else
				{
					long long L = (l & 0xFFFFFFFC);
					L <<= 32;
					memcpy(&d, &L, 8);
				}
				if (l & 0x01)
					d /= 100;

				// Vedi il commento sulla stessa verifica nel caso
				// NUMBER sopra.
				if (style < fXfIsDate.size() && fXfIsDate[style])
					v = ExcelSerialToTime(d, f1904);
				else
					v = d;
				fContainer->NewCell(c, v, NULL);
				fContainer->SetCellStyleNr(c, fStyles[style]);
				c.h++;
			}
			break;
		}
		case MULBLANK:
		{
			es >> c.v; c.v++;
			es >> c.h; c.h++;
			
			int i = (len-4)/2;
			// Stesso motivo del controllo su MULRK sopra.
			if (i < 0) i = 0;

			while (i--)
			{
				es >> style;
				fContainer->SetCellStyleNr(c, fStyles[style]);
				c.h++;
			}
			break;
		}
	}
} // CExcel5Filter::HandleXLRecordForPass2
