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
	Excel.OLE2.c
	
*/
/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.headers.pch++"
 *** TPV (2000-Feb-06) Added Headers Guards
 ***/

#ifndef   EXCEL_H
#include "Excel.h"
#endif

#include <DataIO.h>
#include <cstdio>

const uint32 BLOCKSIZE = 512;

// Tutti i campi dei settori OLE2 sul disco sono a 32 bit fissi (specifica
// Compound File Binary), quindi vanno letti con int32/uint32 (SupportDefs.h)
// e non con "long"/"unsigned long": su Haiku x86_64 "long" e' a 64 bit, non
// 32 come sul BeOS/PPC per cui questo codice era stato scritto in origine.
// Con "long" al posto di int32, sizeof(oleEntry) non corrispondeva piu' ai
// 128 byte reali di una voce di directory OLE2, e gli array indicizzati per
// settore (come "l[]" sotto) leggevano/saltavano offset sbagliati -- bug
// dello stesso tipo gia' corretto altrove (vedi il commento su cell::operator<
// in Cell.h), ma rimasto qui perche' mai esercitato da un file .xls reale
// prima d'ora (i test esistenti verificavano solo il rifiuto di file OLE2
// malformati, non un'importazione riuscita).
struct oleEntry
{
	short name[32];
	int32 unknown[13];
	int32 start;
	int32 size;
	int32 unknown2;
};

#if DEBUG
// this should be part of BeOS...

void Write(BPositionIO& dest, BPositionIO& source)
{
	dest.SetSize(0);
	source.Seek(0, SEEK_SET);
	
	char buffer[32768];
	int l;
	do
	{
		l = source.Read(buffer, 32768);
		if (l > 0)
			dest.Write(buffer, l);
	}
	while (l > 0);
}
#endif

status_t CExcel5Filter::GetBookStream(BPositionIO& stream)
{
	CExcelStream es(stream);
	uint32 l[128];
	long size = 512;
	int i;

	es.Read(l, size);

	if (l[0] != B_HOST_TO_LENDIAN_INT32(0xE011CFD0)) return B_ERROR;

	off_t indxAddress, dirAddress;
	int32 dirSectorId = (int32)B_LENDIAN_TO_HOST_INT32(l[12]);
	indxAddress = ((off_t)(int32)B_LENDIAN_TO_HOST_INT32(l[19]) + 1) * BLOCKSIZE;
	dirAddress = ((off_t)dirSectorId + 1) * BLOCKSIZE;

	// La FAT va caricata qui (sovrascrivendo l[], che fin qui conteneva
	// l'header) perche' serve gia' per seguire la catena di settori
	// della directory sotto, non solo per quella dello stream "Book"/
	// "Workbook" piu' avanti.
	if (stream.Seek(indxAddress, SEEK_SET) != indxAddress)
		return B_ERROR;
	size = 512;
	if (stream.Read(l, size) != size)
		return B_ERROR;

	oleEntry e;
	char e_name[32];

	if (stream.Seek(dirAddress, SEEK_SET) != dirAddress)
		return B_ERROR;
	size = sizeof(oleEntry);
	off_t dirSectorEnd = dirAddress + BLOCKSIZE;

	do
	{
		// La directory OLE2 e' essa stessa una catena di settori (non
		// necessariamente uno solo -- un file con anche solo qualche
		// stream in piu' oltre a "Workbook", tipicamente
		// "\5SummaryInformation", riempie gia' il primo settore da 512
		// byte, che contiene solo 4 voci da 128 byte ciascuna). Quando
		// il settore corrente e' esaurito, si segue la catena nella
		// FAT (l[]) fino al prossimo, esattamente come si fa piu' sotto
		// per i settori dello stream "Book"/"Workbook" stesso. Un
		// riferimento a un settore non valido (fine catena o libero,
		// entrambi rappresentati come interi negativi una volta letti
		// come int32 con segno) termina la ricerca.
		if (stream.Position() >= dirSectorEnd)
		{
			if (dirSectorId < 0 || (size_t)dirSectorId >= sizeof(l) / sizeof(l[0]))
				return B_ERROR;
			dirSectorId = (int32)B_LENDIAN_TO_HOST_INT32(l[dirSectorId]);
			if (dirSectorId < 0)
				return B_ERROR;
			dirAddress = ((off_t)dirSectorId + 1) * BLOCKSIZE;
			if (stream.Seek(dirAddress, SEEK_SET) != dirAddress)
				return B_ERROR;
			dirSectorEnd = dirAddress + BLOCKSIZE;
		}

		if (stream.Read(&e, size) != size)
			return B_ERROR;

		for (i = 0; i < 32; i++)
			e_name[i] = B_LENDIAN_TO_HOST_INT16(e.name[i]);
	}
	while (strcmp(e_name, "Book") && strcmp(e_name, "Workbook"));

	e.size = (int32)B_LENDIAN_TO_HOST_INT32(e.size);
	e.start = (int32)B_LENDIAN_TO_HOST_INT32(e.start);

	int32 indx = e.start;
	do
	{
		char buf[512];
		off_t sectorAddress = ((off_t)indx + 1) * BLOCKSIZE;
		if (stream.Seek(sectorAddress, SEEK_SET) != sectorAddress)
			return B_ERROR;
		int32 k = min_c(e.size, (int32)BLOCKSIZE);
		if (stream.Read(buf, k) != k)
			return B_ERROR;
		if (fBook.Write(buf, k) != k)
			return B_ERROR;
		e.size -= BLOCKSIZE;
		if (e.size < 0) break;

		if (indx < 0 || (size_t)indx >= sizeof(l) / sizeof(l[0]))
			return B_ERROR;
		indx = (int32)B_LENDIAN_TO_HOST_INT32(l[indx]);
	}
	while (e.size > 0 && indx > 0);

#if DEBUG
	BFile dump;
	BDirectory("/tmp").CreateFile("OLE", &dump);
	Write(dump, fBook);
#endif

	fBook.Seek(0, SEEK_SET);
	return B_NO_ERROR;
} /* GetBookStream */
