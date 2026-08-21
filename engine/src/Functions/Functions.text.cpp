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
	Functions.text.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include "Container.h"
#include "FunctionUtils.h"
#include "Functions.h"
#include "Formatter.h"
#include "Utils.h"
#if __BEOS__ || __HAIKU__
#include "utf-support.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#else
#	define mstrlen strlen
#	define mstrcpy strncpy
#	define moffset(s,i)	((s) + i)
#endif

void ASCFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];
	double d;
	
	if (GetTextArgument(stack, argCnt, 1, s) &&
		strlen(s) == 1)
		stack[0] = (double)s[0];
	else if (GetDoubleArgument(stack, argCnt, 1, &d) && std::isnan(d))
		stack[0] = d;
	else
		stack[0] = gValueNan;
}

void CHRFunction(Value *stack, int argCnt, CContainer *cells)
{
	double num;
	
	if (GetDoubleArgument(stack, argCnt, 1, &num) &&
		num > 0.0 && num <= 127.0)
	{
		if (std::isnan(num))
			stack[0] = num;
		else
		{
			char s[2];
			s[1] = 0;
			s[0] = static_cast<char>(rint(num));
			stack[0] = s;
		}
	}
	else
		stack[0] = gValueNan;
}

void LEFTFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];
	double count;
	
	if (CheckForNanParameters(stack, argCnt))
		return;

	if (GetTextArgument(stack, argCnt, 1, s) && 
		GetDoubleArgument(stack, argCnt, 2, &count))
	{
		char s2[512];
		mstrcpy(s2, s, static_cast<int>(count) );
		stack[0] = s2;
	}
	else
		stack[0] = gValueNan;
}

void LENGTHFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];
	double d;
	
	if (GetTextArgument(stack, argCnt, 1, s))
		stack[0] = (double)mstrlen(s);
	else if (GetDoubleArgument(stack, argCnt, 1, &d) && std::isnan(d))
		stack[0] = d;
	else
		stack[0] = gValueNan;
}

void MIDFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];
	double start, count;
	int starti, counti;
	
	if (CheckForNanParameters(stack, argCnt))
		return;

	if (GetTextArgument(stack, argCnt, 1, s) && 
		GetDoubleArgument(stack, argCnt, 2, &start) &&
		GetDoubleArgument(stack, argCnt, 3, &count))
	{
		starti = static_cast<int>(rint(start) - 1);
		counti = static_cast<int>(rint(count) );
		int sLen = mstrlen(s);
		
		if (sLen > starti + counti)
			mstrcpy(s, moffset(s, starti), counti);
		else if (sLen > starti)
			mstrcpy(s, moffset(s, starti), sLen - starti);
		else
			s[0] = 0;

		stack[0] = s;
	}
	else
		stack[0] = gValueNan;
}

void NUM2CFunction(Value *stack, int argCnt, CContainer *cells)
{
	double num;
	
	if (GetDoubleArgument(stack, argCnt, 1, &num))
	{
		char s[32];
		ftoa(num, s);
		stack[0] = s;
	}
	else
		stack[0] = gValueNan;
}

void RIGHTFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];
	double count;
	int counti;
	
	if (CheckForNanParameters(stack, argCnt))
		return;

	if (GetTextArgument(stack, argCnt, 1, s) && 
		GetDoubleArgument(stack, argCnt, 2, &count))
	{
		counti = static_cast<int32>(rint(count) );
		int sLen = mstrlen(s);
		
		if (sLen > counti)
			mstrcpy(s, moffset(s, sLen - counti), counti);

		stack[0] = s;
	}
	else
		stack[0] = gValueNan;
}

// TRIM/UPPER/LOWER/PROPER/FIND/SEARCH/CONCAT (Fase 13): a differenza di
// LEFT/MID/RIGHT sopra, che usano mstrlen/mstrcpy/moffset (UTF-8
// consapevoli), qui si lavora byte per byte -- non esiste in questo
// motore una tabella di conversione maiuscolo/minuscolo UTF-8 (vedi
// utf-support.h: solo lunghezza/offset), quindi una lettera accentata
// (es. "città") resta invariata invece di convertirsi in UPPER/LOWER/
// PROPER. Stessa approssimazione gia' accettata altrove per il testo
// (famiglia del font non cercata all'import XLSX, solo stile/
// dimensione, vedi Fase 12).

void TRIMFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];

	if (GetTextArgument(stack, argCnt, 1, s))
	{
		char out[256];
		int o = 0, len = strlen(s), i = 0;
		bool pendingSpace = false;

		while (i < len && s[i] == ' ')
			i++;
		for (; i < len; i++)
		{
			if (s[i] == ' ')
				pendingSpace = true;
			else
			{
				if (pendingSpace)
				{
					out[o++] = ' ';
					pendingSpace = false;
				}
				out[o++] = s[i];
			}
		}
		out[o] = 0;
		stack[0] = out;
	}
	else
		stack[0] = gValueNan;
}

void UPPERFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];

	if (GetTextArgument(stack, argCnt, 1, s))
	{
		for (char *p = s; *p; p++)
			*p = toupper((unsigned char)*p);
		stack[0] = s;
	}
	else
		stack[0] = gValueNan;
}

void LOWERFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];

	if (GetTextArgument(stack, argCnt, 1, s))
	{
		for (char *p = s; *p; p++)
			*p = tolower((unsigned char)*p);
		stack[0] = s;
	}
	else
		stack[0] = gValueNan;
}

void PROPERFunction(Value *stack, int argCnt, CContainer *cells)
{
	char s[256];

	if (GetTextArgument(stack, argCnt, 1, s))
	{
		bool startOfWord = true;
		for (char *p = s; *p; p++)
		{
			if (isalpha((unsigned char)*p))
			{
				*p = startOfWord ? toupper((unsigned char)*p) : tolower((unsigned char)*p);
				startOfWord = false;
			}
			else
				startOfWord = true;
		}
		stack[0] = s;
	}
	else
		stack[0] = gValueNan;
}

// Trovata solo con l'iniziale minuscola, per lo stesso motivo di
// SEARCH sotto rispetto a FIND: nessuna wildcard (? e *), a differenza
// di Excel vero -- approssimazione accettata per restare nello scope
// di un task "facile" (vedi ROADMAP.md Fase 13).
static const char* FindCaseInsensitive(const char *haystack, const char *needle)
{
	size_t needleLen = strlen(needle);
	if (needleLen == 0)
		return haystack;
	for (const char *p = haystack; *p; p++)
	{
		size_t i = 0;
		while (i < needleLen && p[i] != 0 &&
			tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
			i++;
		if (i == needleLen)
			return p;
	}
	return NULL;
}

void FINDFunction(Value *stack, int argCnt, CContainer *cells)
{
	char findText[256], withinText[256];
	double startArg;
	int start = 1;

	if (GetTextArgument(stack, argCnt, 1, findText) &&
		GetTextArgument(stack, argCnt, 2, withinText))
	{
		if (GetDoubleArgument(stack, argCnt, 3, &startArg))
			start = static_cast<int>(rint(startArg));

		int withinLen = strlen(withinText);
		if (start < 1 || start > withinLen + 1)
		{
			stack[0] = gValueNan;
			return;
		}

		const char *found = strstr(withinText + start - 1, findText);
		stack[0] = found ? (double)(found - withinText + 1) : gValueNan;
	}
	else
		stack[0] = gValueNan;
}

void SEARCHFunction(Value *stack, int argCnt, CContainer *cells)
{
	char findText[256], withinText[256];
	double startArg;
	int start = 1;

	if (GetTextArgument(stack, argCnt, 1, findText) &&
		GetTextArgument(stack, argCnt, 2, withinText))
	{
		if (GetDoubleArgument(stack, argCnt, 3, &startArg))
			start = static_cast<int>(rint(startArg));

		int withinLen = strlen(withinText);
		if (start < 1 || start > withinLen + 1)
		{
			stack[0] = gValueNan;
			return;
		}

		const char *found = FindCaseInsensitive(withinText + start - 1, findText);
		stack[0] = found ? (double)(found - withinText + 1) : gValueNan;
	}
	else
		stack[0] = gValueNan;
}

// A differenza di NUM2CFunction sopra, qui non si puo' usare ftoa
// (Formatter.cpp): internamente chiama BFont::StringWidth, che
// richiede una connessione app_server -- senza una BApplication vera
// (come nel test del motore isolato, named_functions_test.cpp, che
// non ne crea una apposta) resta bloccata in attesa di una risposta
// che non arriva mai. Il motore deve restare utilizzabile anche senza
// Interface Kit (vedi il commento in cima a Container.h sullo scopo
// della libreria isolata), quindi qui si usa una conversione diretta
// con snprintf invece di appoggiarsi al formattatore grafico.
static void FormatNumberForConcat(double d, char *out)
{
	snprintf(out, 32, "%.10g", d);
}

void CONCATFunction(Value *stack, int argCnt, CContainer *cells)
{
	char out[2048];
	double d;

	// stack[i-1].fText direttamente, non GetTextArgument in un buffer
	// intermedio fisso (char arg[256] prima di questo fix): GetTextArgument
	// fa una strcpy SENZA limite di lunghezza nel buffer del chiamante,
	// assumendo che sia "abbastanza grande" -- un testo di cella reale
	// piu' lungo di 255 caratteri (comunissimo, il limite vero di Excel
	// e' 32767) scriveva oltre la fine di quello scratch buffer sullo
	// stack, corrompendo la memoria in un modo che si manifestava altrove
	// come un errore completamente slegato ("errInsufficientMemory" da un
	// MALLOC successivo, mai dove il danno era davvero avvenuto) -- bug
	// preesistente in CONCAT, mai raggiunto prima su un testo cosi' lungo
	// finche' l'alias CONCATENATE non ha reso raggiungibile questo
	// percorso su un file XLSX reale con descrizioni lunghe.
	out[0] = 0;
	for (int i = 1; i <= argCnt; i++)
	{
		if (stack[i - 1].fType == eTextData)
			strncat(out, stack[i - 1].fText, sizeof(out) - strlen(out) - 1);
		else if (GetDoubleArgument(stack, argCnt, i, &d))
		{
			char num[32];
			FormatNumberForConcat(d, num);
			strncat(out, num, sizeof(out) - strlen(out) - 1);
		}
	}
	stack[0] = out;
}

// SUBSTITUTE/REPLACE/REPT/TEXTJOIN/VALUE/EXACT (Fase 26, vedi
// ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni originali
// di Sum-It, mancanti confrontando la tabella con l'elenco standard di
// Excel. Come CONCATFunction sopra, leggono il testo direttamente da
// stack[i-1].fText (mai in un buffer char[256] fisso via
// GetTextArgument) per lo stesso identico motivo gia' documentato li'.

// SUBSTITUTE(testo,testo_vecchio,testo_nuovo,[occorrenza]): senza
// "occorrenza" sostituisce OGNI ricorrenza di testo_vecchio, altrimenti
// solo la N-esima. Registrata nella risorsa 'Func' col nome interno
// "SUBST" (kSUBSTITUTEFuncNr): "SUBSTITUTE" e' di 10 caratteri, non
// entra nel campo funcName[10] a lunghezza fissa (9 utili piu' il
// terminatore) -- alias in GetFunctionNr verso questo stesso funcNr,
// identico principio di CEILING.MATH/CONCATENATE/LOG10 (vedi Utils.cpp).
void SUBSTITUTEFunction(Value *stack, int argCnt, CContainer *cells)
{
	if (CheckForNanParameters(stack, argCnt))
		return;

	if (stack[0].fType != eTextData || stack[1].fType != eTextData
		|| stack[2].fType != eTextData)
	{
		stack[0] = gValueNan;
		return;
	}

	const char *text = stack[0].fText;
	const char *oldText = stack[1].fText;
	const char *newText = stack[2].fText;
	size_t oldLen = strlen(oldText);

	double instanceArg;
	int instance = 0; // 0 = tutte le occorrenze
	if (argCnt >= 4 && GetDoubleArgument(stack, argCnt, 4, &instanceArg))
		instance = static_cast<int>(rint(instanceArg));

	char out[2048];
	out[0] = 0;

	if (oldLen == 0)
	{
		strncat(out, text, sizeof(out) - 1);
		stack[0] = out;
		return;
	}

	int occurrence = 0;
	const char *p = text;
	while (*p && strlen(out) + 1 < sizeof(out))
	{
		if (strncmp(p, oldText, oldLen) == 0)
		{
			occurrence++;
			if (instance == 0 || occurrence == instance)
			{
				strncat(out, newText, sizeof(out) - strlen(out) - 1);
				p += oldLen;
				continue;
			}
		}
		size_t outLen = strlen(out);
		out[outLen] = *p;
		out[outLen + 1] = 0;
		p++;
	}

	stack[0] = out;
}

// REPLACE(testo_vecchio,posizione,num_caratteri,testo_nuovo): sostituisce
// num_caratteri caratteri a partire da posizione (1-based) con
// testo_nuovo, indipendentemente dal loro contenuto (a differenza di
// SUBSTITUTE sopra, che cerca un testo).
void REPLACEFunction(Value *stack, int argCnt, CContainer *cells)
{
	double startArg, lenArg;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (stack[0].fType == eTextData
		&& GetDoubleArgument(stack, argCnt, 2, &startArg)
		&& GetDoubleArgument(stack, argCnt, 3, &lenArg)
		&& stack[3].fType == eTextData)
	{
		const char *text = stack[0].fText;
		const char *newText = stack[3].fText;
		int start = static_cast<int>(rint(startArg));
		int numChars = static_cast<int>(rint(lenArg));
		int textLen = (int)strlen(text);

		if (start < 1 || numChars < 0)
		{
			stack[0] = gValueNan;
			return;
		}

		char out[2048];
		out[0] = 0;
		int headLen = (start - 1 < textLen) ? (start - 1) : textLen;
		strncat(out, text, headLen);
		strncat(out, newText, sizeof(out) - strlen(out) - 1);
		int tailStart = start - 1 + numChars;
		if (tailStart < textLen)
			strncat(out, text + tailStart, sizeof(out) - strlen(out) - 1);

		stack[0] = out;
	}
	else
		stack[0] = gValueNan;
}

void REPTFunction(Value *stack, int argCnt, CContainer *cells)
{
	double countArg;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (stack[0].fType == eTextData && GetDoubleArgument(stack, argCnt, 2, &countArg))
	{
		int count = static_cast<int>(rint(countArg));
		if (count < 0)
		{
			stack[0] = gValueNan;
			return;
		}
		const char *text = stack[0].fText;
		char out[2048];
		out[0] = 0;
		for (int i = 0; i < count && strlen(out) + 1 < sizeof(out); i++)
			strncat(out, text, sizeof(out) - strlen(out) - 1);
		stack[0] = out;
	}
	else
		stack[0] = gValueNan;
}

// TEXTJOIN(separatore,ignora_vuoti,testo1,[testo2],...): come CONCAT
// sopra ma con un separatore fra i pezzi e la possibilita' di saltare
// gli argomenti vuoti -- FormatNumberForConcat e' la stessa funzione
// gia' definita sopra per CONCATFunction.
void TEXTJOINFunction(Value *stack, int argCnt, CContainer *cells)
{
	if (CheckForNanParameters(stack, argCnt))
		return;

	if (argCnt < 3 || stack[0].fType != eTextData)
	{
		stack[0] = gValueNan;
		return;
	}

	const char *delim = stack[0].fText;
	bool ignoreEmpty = true;
	if (stack[1].fType == eBoolData)
		ignoreEmpty = stack[1].fBool;

	char out[4096];
	out[0] = 0;
	bool first = true;
	double d;

	for (int i = 2; i < argCnt; i++)
	{
		const char *piece = NULL;
		char numBuf[32];
		if (stack[i].fType == eTextData)
			piece = stack[i].fText;
		else if (GetDoubleArgument(stack, argCnt, i + 1, &d))
		{
			FormatNumberForConcat(d, numBuf);
			piece = numBuf;
		}

		if (piece == NULL || (ignoreEmpty && piece[0] == 0))
			continue;

		if (!first)
			strncat(out, delim, sizeof(out) - strlen(out) - 1);
		strncat(out, piece, sizeof(out) - strlen(out) - 1);
		first = false;
	}

	stack[0] = out;
}

// VALUE(testo): converte una stringa numerica in numero, usando lo
// stesso analizzatore locale-aware gia' usato dal motore per i valori
// letterali digitati dall'utente (atof_i, Utils.cpp -- decimale/
// migliaia secondo gDecimalPoint/gThousandSeparator correnti). Limite
// ereditato da atof_i: si aspetta una stringa PULITA (solo cifre,
// separatore decimale/migliaia, segno meno iniziale), un simbolo di
// valuta o spazio finale la fanno fallire con #VALUE!, a differenza
// del VALUE() vero di Excel che e' piu' permissivo.
void VALUEFunction(Value *stack, int argCnt, CContainer *cells)
{
	double d;

	if (stack[0].fType == eTextData)
		stack[0] = atof_i(stack[0].fText);
	else if (GetDoubleArgument(stack, argCnt, 1, &d))
		stack[0] = d; // gia' un numero: passa cosi' com'e', come in Excel
	else
		stack[0] = gValueNan;
}

// EXACT(testo1,testo2): confronto CASE-SENSITIVE, a differenza di
// Value::operator== (Value.cpp, usato altrove nell'engine per es. da
// SWITCH) che ignora maiuscole/minuscole per il testo.
void EXACTFunction(Value *stack, int argCnt, CContainer *cells)
{
	if (CheckForNanParameters(stack, argCnt))
		return;

	if (stack[0].fType == eTextData && stack[1].fType == eTextData)
		stack[0] = (bool)(strcmp(stack[0].fText, stack[1].fText) == 0);
	else
		stack[0] = gValueNan;
}

