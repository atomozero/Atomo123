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
// Tavolozza predefinita a 56 colori di Excel (ColorIndex 1-56), usata
// come colore reale quando un file XLS non allega un proprio record
// PALETTE (0x0092, mai visto in pratica nei file reali usati per
// verificare questo importatore, ne' gestito da questo codice) --
// vedi il commento su fillPattern/fgColorIdx in Excel.pass1.cpp.
// Valori corretti confrontando con Excel vero (una fattura reale
// mostrava un ciano chiaro, RGB(204,255,255), dove questa tavolozza
// aveva 0x9F,0xDF,0xDF -- un colore vicino ma sbagliato) e verificati
// contro due elenchi pubblici indipendenti e concordanti dei 56
// ColorIndex standard di Excel (non contro un file .xls specifico:
// senza un record PALETTE questi SONO i valori che Excel stesso usa,
// non un'approssimazione). La versione precedente di questa tavola,
// ereditata dal codice storico di Sum-It/Hekkelman Programmatuur,
// aveva diversi valori sbagliati oltre a quello -- fra cui gli indici
// 13/14 scambiati fra loro (non solo imprecisi, proprio invertiti).
const rgb_color
	kExcelColorTable[56] = {
		{ 0x00, 0x00, 0x00, 0x00 },			// 1
		{ 0xFF, 0xFF, 0xFF, 0x00 },			// 2
		{ 0xFF, 0x00, 0x00, 0x00 },			// 3
		{ 0x00, 0xFF, 0x00, 0x00 },			// 4
		{ 0x00, 0x00, 0xFF, 0x00 },			// 5
		{ 0xFF, 0xFF, 0x00, 0x00 },			// 6
		{ 0xFF, 0x00, 0xFF, 0x00 },			// 7
		{ 0x00, 0xFF, 0xFF, 0x00 },			// 8
		{ 0x80, 0x00, 0x00, 0x00 },			// 9
		{ 0x00, 0x80, 0x00, 0x00 },			// 10
		{ 0x00, 0x00, 0x80, 0x00 },			// 11
		{ 0x80, 0x80, 0x00, 0x00 },			// 12
		{ 0x80, 0x00, 0x80, 0x00 },			// 13
		{ 0x00, 0x80, 0x80, 0x00 },			// 14
		{ 0xC0, 0xC0, 0xC0, 0x00 },			// 15
		{ 0x80, 0x80, 0x80, 0x00 },			// 16
		{ 0x99, 0x99, 0xFF, 0x00 },			// 17
		{ 0x99, 0x33, 0x66, 0x00 },			// 18
		{ 0xFF, 0xFF, 0xCC, 0x00 },			// 19
		{ 0xCC, 0xFF, 0xFF, 0x00 },			// 20
		{ 0x66, 0x00, 0x66, 0x00 },			// 21
		{ 0xFF, 0x80, 0x80, 0x00 },			// 22
		{ 0x00, 0x66, 0xCC, 0x00 },			// 23
		{ 0xCC, 0xCC, 0xFF, 0x00 },			// 24
		{ 0x00, 0x00, 0x80, 0x00 },			// 25
		{ 0xFF, 0x00, 0xFF, 0x00 },			// 26
		{ 0xFF, 0xFF, 0x00, 0x00 },			// 27
		{ 0x00, 0xFF, 0xFF, 0x00 },			// 28
		{ 0x80, 0x00, 0x80, 0x00 },			// 29
		{ 0x80, 0x00, 0x00, 0x00 },			// 30
		{ 0x00, 0x80, 0x80, 0x00 },			// 31
		{ 0x00, 0x00, 0xFF, 0x00 },			// 32
		{ 0x00, 0xCC, 0xFF, 0x00 },			// 33
		{ 0xCC, 0xFF, 0xFF, 0x00 },			// 34
		{ 0xCC, 0xFF, 0xCC, 0x00 },			// 35
		{ 0xFF, 0xFF, 0x99, 0x00 },			// 36
		{ 0x99, 0xCC, 0xFF, 0x00 },			// 37
		{ 0xFF, 0x99, 0xCC, 0x00 },			// 38
		{ 0xCC, 0x99, 0xFF, 0x00 },			// 39
		{ 0xFF, 0xCC, 0x99, 0x00 },			// 40
		{ 0x33, 0x66, 0xFF, 0x00 },			// 41
		{ 0x33, 0xCC, 0xCC, 0x00 },			// 42
		{ 0x99, 0xCC, 0x00, 0x00 },			// 43
		{ 0xFF, 0xCC, 0x00, 0x00 },			// 44
		{ 0xFF, 0x99, 0x00, 0x00 },			// 45
		{ 0xFF, 0x66, 0x00, 0x00 },			// 46
		{ 0x66, 0x66, 0x99, 0x00 },			// 47
		{ 0x96, 0x96, 0x96, 0x00 },			// 48
		{ 0x00, 0x33, 0x66, 0x00 },			// 49
		{ 0x33, 0x99, 0x66, 0x00 },			// 50
		{ 0x00, 0x33, 0x00, 0x00 },			// 51
		{ 0x33, 0x33, 0x00, 0x00 },			// 52
		{ 0x99, 0x33, 0x00, 0x00 },			// 53
		{ 0x99, 0x33, 0x66, 0x00 },			// 54
		{ 0x33, 0x33, 0x99, 0x00 },			// 55
		{ 0x33, 0x33, 0x33, 0x00 }			// 56
	};
