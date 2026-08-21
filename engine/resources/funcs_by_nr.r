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

type 'Func'
{
	array {
		cstring[10];
		integer;
		integer;
		integer;
	}
};

resource 'Func' (128, "Functions")
{
	{
		"ABS", 1, 0, 4,
		"ACOS", 1, 1, 4,
		"ANNUITY", 2, 2, 3,
		"ASC", 1, 3, 6,
		"ASIN", 1, 4, 4,
		"ATAN", 1, 5, 4,
		"AVG", 65535, 6, 5,
		"CEILING", 1, 7, 5,
		"CELL", 2, 8, 1,
		"CHOOSE", 65534, 9, 7,
		"CHR", 1, 10, 6,
		"COLUMN", 1, 11, 1,
		"COMPOUND", 2, 12, 3,
		"COS", 1, 13, 4,
		"COT", 1, 14, 4,
		"COUNT", 65535, 15, 5,
		"DATE", 3, 16, 2,
		"DAY", 1, 17, 2,
		"DB", 4, 18, 3,
		"DOCUMENT", 0, 19, 1,
		"DOW", 1, 20, 2,
		"ERR", 0, 21, 7,
		"ERROR", 1, 22, 7,
		"EXP", 1, 23, 4,
		"FALSE", 0, 24, 7,
		"FLOOR", 1, 25, 5,
		"FRAC", 1, 26, 4,
		"FV", 3, 27, 3,
		"HINDEX", 2, 28, 1,
		"HLOOKUP", 65535, 29, 1, // 65535 = quarto argomento (corrispondenza esatta) opzionale, Fase 13
		"HOUR", 1, 30, 2,
		"IF", 65535, 31, 7, // 65535 = terzo argomento (valore_se_falso) opzionale, come in Excel -- IFFunction (Fase 13/14, Functions.spreadsheet.cpp) gestisce gia' argCnt==2 correttamente, era gia' pronta in attesa di questo cambiamento
		"IFERR", 65535, 32, 7, // 65535 = secondo/terzo argomento gestiti entrambi da IFERRFunction, Fase 13
		"INT", 1, 33, 4,
		"IRR", 2, 34, 3,
		"ISNULL", 1, 35, 7,
		"ISNUM", 1, 36, 7,
		"ISTEXT", 1, 37, 7,
		"LEFT", 2, 38, 6,
		"LENGTH", 1, 39, 6,
		"LN", 1, 40, 4,
		"LOG", 1, 41, 4,
		"MAX", 65535, 42, 5,
		"MID", 3, 43, 6,
		"MIN", 65535, 44, 5,
		"MINUTE", 1, 45, 2,
		"MOD", 2, 46, 4,
		"MONTH", 1, 47, 2,
		"NA", 0, 48, 7,
		"NCOLS", 1, 49, 1,
		"NOW", 0, 50, 2,
		"NPV", 2, 51, 3,
		"NROWS", 1, 52, 1,
		"NUM2C", 1, 53, 6,
		"NUMPAGES", 0, 54, 1,
		"OFFSET", 3, 55, 1,
		"PAGE", 0, 56, 1,
		"PI", 0, 57, 4,
		"PMT", 3, 58, 3,
		"PV", 3, 59, 3,
		"RANDOM", 1, 60, 4,
		"RIGHT", 2, 61, 6,
		"ROUND", 2, 62, 5,
		"ROW", 1, 63, 1,
		"SECOND", 1, 64, 2,
		"SIGN", 1, 65, 4,
		"SIN", 1, 66, 4,
		"SL", 3, 67, 3,
		"SOYD", 4, 68, 3,
		"SQRT", 1, 69, 4,
		"STDDEV", 65535, 70, 5,
		"SUM", 65535, 71, 5,
		"TAN", 1, 72, 4,
		"TIME", 3, 73, 2,
		"TIME2C", 1, 74, 6,
		"VARIANCE", 65535, 75, 5,
		"VINDEX", 2, 76, 1,
		"VLOOKUP", 65535, 77, 1, // 65535 = quarto argomento (corrispondenza esatta) opzionale, Fase 13
		"YEAR", 1, 78, 2,
		"TRUE", 0, 79, 7,
		"OR", 65535, 80, 7,
		"AND", 65535, 81, 7,
		"POWER", 2, 82, 4,
		"DEC2HEX", 2, 83, 8,
		"ATAN2", 2, 84, 4,
		"COUNTA", 65535, 85, 5, //check max argument quantity !!!
		"SUMIF", 65535, 86, 5,
		"COUNTIF", 2, 87, 5,
		"AVERAGEIF", 65535, 88, 5,
		"TRIM", 1, 89, 6,
		"UPPER", 1, 90, 6,
		"LOWER", 1, 91, 6,
		"PROPER", 1, 92, 6,
		"FIND", 65535, 93, 6,
		"SEARCH", 65535, 94, 6,
		"CONCAT", 65535, 95, 6,
		"MEDIAN", 65535, 96, 5,
		"MODE", 65535, 97, 5,
		// Nome standard Excel (IFERR sopra e' quello storico di Sum-It,
		// mai usato da un file XLSX vero): stessa IFERRFunction, non una
		// nuova implementazione -- vedi Functions.h/FunctionUtils.cpp.
		// Bug reale scoperto su un file reale: ogni cella con IFERROR(...)
		// veniva mostrata come testo grezzo della formula invece del
		// valore calcolato, perche' "IFERROR" non esisteva affatto nella
		// tabella (l'analisi grammaticale falliva con "funzione sconosciuta").
		"IFERROR", 65535, 98, 7,
		// INDEX/MATCH (Fase 13): non HINDEX/VINDEX sopra, che
		// nonostante il nome fanno una ricerca in stile MATCH
		// approssimato -- vedi il commento su INDEXFunction in
		// Functions.spreadsheet.cpp.
		"INDEX", 65535, 99, 1,
		"MATCH", 65535, 100, 1,
		// XLOOKUP/IFS (Fase 14): vedi il commento su kXLOOKUPFuncNr in
		// Functions.h. 65535 = numero di argomenti variabile, come
		// VLOOKUP/HLOOKUP sopra -- XLOOKUP accetta da 3 a 6 argomenti
		// (gli ultimi tre opzionali), IFS da 2 in su (coppie
		// condizione/valore).
		"XLOOKUP", 65535, 101, 1,
		"IFS", 65535, 102, 7,
		// COUNTIFS (Fase 14): vedi il commento su kCOUNTIFSFuncNr in
		// Functions.h. 65535 = numero di coppie intervallo/criterio
		// variabile, come SUMIF/COUNTIF sopra.
		"COUNTIFS", 65535, 103, 5,
		// ROUNDUP/ROUNDDOWN/TEXT (Fase 14): vedi il commento su
		// kROUNDUPFuncNr in Functions.h. Due argomenti esatti (numero,
		// cifre), come ROUND gia' esistente.
		"ROUNDUP", 2, 104, 5,
		"ROUNDDOWN", 2, 105, 5,
		"TEXT", 2, 106, 6,
		// NOT/XOR/SWITCH/IFNA/ISBLANK/ISERROR/ISNA/ISFORMULA (Fase 26,
		// vedi ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni
		// originali di Sum-It, mancanti confrontando la tabella con
		// l'elenco standard di Excel. 65535 = numero di argomenti
		// variabile (XOR da 1 in su, SWITCH da 3 in su).
		"NOT", 1, 107, 7,
		"XOR", 65535, 108, 7,
		"SWITCH", 65535, 109, 7,
		"IFNA", 2, 110, 7,
		"ISBLANK", 1, 111, 7,
		"ISERROR", 1, 112, 7,
		"ISNA", 1, 113, 7,
		"ISFORMULA", 1, 114, 7,
		// SUBSTITUTE/REPLACE/REPT/TEXTJOIN/VALUE/EXACT (Fase 26, vedi
		// ROADMAP.md "v3.0 Consolidation"). "SUBST" (non "SUBSTITUTE",
		// 10 caratteri, non entra nel campo a lunghezza fissa): alias
		// diretto in GetFunctionNr (Utils.cpp), stesso principio di
		// CEILING.MATH/CONCATENATE/LOG10 -- funziona anche digitato
		// come "SUBST()" direttamente, non solo come alias, ma non e'
		// un nome Excel vero quindi non e' un problema.
		"SUBST", 65535, 115, 6,
		"REPLACE", 4, 116, 6,
		"REPT", 2, 117, 6,
		"TEXTJOIN", 65535, 118, 6,
		"VALUE", 1, 119, 6,
		"EXACT", 2, 120, 6
	}
};
