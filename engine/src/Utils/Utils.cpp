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
	Utils.c

	Copyright 1997, Hekkelman Programmatuur
	Portions Copyright 2026 Andrea Bernardi

	Part of Sum-It for the BeBox version 1.1.

*/
/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.headers.pch++"
 *** TPV (2000-Feb-06) Added Headers Guards
 ***/

#ifndef   CONTAINER_H
#include "Container.h"
#include "Functions.h"
#endif

#ifndef   FUNCTIONUTILS_H
#include "FunctionUtils.h"
#endif

//#ifndef   MYRESOURCES_H
//#include "MyResources.h"
//#endif
#ifndef   RESOURCEMANAGER_H
#include "ResourceManager.h"
#endif

#ifndef   UTILS_H
#include "Utils.h"
#endif

#ifndef   COLORMENUITEM_H
#endif

#ifndef   MALERT_H
#include "MAlert.h"
#endif

#ifndef   GLOBALS_H
#include "Globals.h"
#endif

#ifndef   MYERROR_H
#include "MyError.h"
#endif


#include <ctype.h>
#include <cstdio>

#include <MenuBar.h>

#if DEBUG
void WarnForUnlockedTree(int line);
void WarnForUnlockedTree(int line)
{
	char s[256];
	sprintf(s, "Unlocked tree at line %d", line);
	MAlert *a = new MWarningAlert(s);
	a->Go();
} /* WarnForUnlockedTree */
#endif

void NumToAString(long num, char *s)
{
	
//	ASSERT (num > 0 && num < 26 * 26);
	
	if (num > 26)
	{
		num--;
		s[0] = num / 26 + '@';
		s[1] = num % 26 + 'A';
		s[2] = 0;
	}
	else
	{
		s[0] = num + '@';
		s[1] = 0;
	}
} /* NumToAString */

// Confronto senza distinguere maiuscole/minuscole, lunghezza esatta
// (non un prefisso) -- usato solo per i nomi Excel troppo lunghi per
// entrare nel campo funcName[10] a lunghezza fissa della risorsa
// 'Func' condivisa da ogni altra funzione (vedi il commento sotto in
// GetFunctionNr), che quindi non possono passare dalla normale ricerca
// binaria su quella tabella.
static bool MatchesFuncNameAlias(const char *name, const char *alias)
{
	size_t aliasLen = strlen(alias);
	if (strlen(name) != aliasLen)
		return false;
	for (size_t i = 0; i < aliasLen; i++)
	{
		if (toupper(name[i]) != alias[i])
			return false;
	}
	return true;
} /* MatchesFuncNameAlias */

int GetFunctionNr(const char *name)
{
	long l, R, i, sLen;
	char myFunc[10];
	int d;

	// Prefisso di compatibilita' che Excel scrive davanti a ogni
	// funzione piu' recente del formato dichiarato del file (es.
	// "_xlfn.XLOOKUP", "_xlfn.IFS"): la funzione nella tabella si
	// chiama comunque "XLOOKUP"/"IFS", quindi va saltato per il
	// confronto -- richiede che il lessico riconosca "_xlfn.XLOOKUP"
	// come un unico identificatore (vedi lexer.cpp, stato 42) prima
	// che questa stringa completa arrivi qui.
	static const char kXlfnPrefix[] = "_XLFN.";
	const int kXlfnPrefixLen = sizeof(kXlfnPrefix) - 1;
	bool hasXlfnPrefix = true;
	for (i = 0; i < kXlfnPrefixLen; i++)
	{
		if (name[i] == 0 || toupper(name[i]) != kXlfnPrefix[i])
		{
			hasXlfnPrefix = false;
			break;
		}
	}
	if (hasXlfnPrefix)
		name += kXlfnPrefixLen;

	sLen = strlen(name);

	// myFunc[10] ospita fino a 9 caratteri piu' il terminatore: il
	// controllo era "sLen >= 9", che scartava per errore anche i nomi
	// di esattamente 9 caratteri (che entrano nel buffer esattamente,
	// vedi il ciclo sotto: myFunc[0..8] + myFunc[sLen]=0, 10 byte in
	// tutto, nessun overflow) -- scoperto aggiungendo AVERAGEIF (9
	// caratteri), trattato come identificatore sconosciuto invece che
	// come funzione nonostante fosse nella tabella. Nessuno degli 86
	// nomi originali di Sum-It arrivava a 9 caratteri, quindi il bug
	// non si era mai manifestato prima. Non si applica agli alias
	// sotto (CEILING.MATH/CONCATENATE, piu' lunghi di 9 caratteri per
	// definizione): controllata solo DOPO di loro.

	/* Se la tabella delle funzioni non e' mai stata caricata (nessuno
	   ha chiamato InitFunctions(), che la popola da una risorsa 'Func'
	   — caso normale nella libreria engine isolata, che non allega
	   ancora risorse), gFuncArrayByName resta NULL e gFuncCount resta
	   0. Senza questo controllo la ricerca binaria sottostante
	   dereferenzia un puntatore nullo -- e cosi' farebbero anche gli
	   alias sotto, che restituiscono un funcNr valido (es. 7 per
	   CEILING) SENZA passare da quella ricerca: parser.cpp indicizza
	   comunque gFuncArrayByNr[funcNr] subito dopo per leggere argCnt,
	   quindi un funcNr "valido" ma con la tabella non caricata e'
	   ESATTAMENTE cosi' pericoloso quanto uno trovato dalla ricerca
	   binaria stessa. Bug reale: mandava in crash (non solo "funzione
	   sconosciuta") l'apertura di un file XLSX vero con
	   "_xlfn.CEILING.MATH" nel translator XLSX, che ha una propria
	   copia di gFuncArrayByNr separata da quella dell'app (add-on
	   caricato dinamicamente, non collegato staticamente come nei
	   test) -- MAI inizializzata dall'app stessa, vedi il commento su
	   InitFunctions() in XlsxTranslator.cpp.
	*/
	if (gFuncCount <= 0)
		return -1;

	// "CEILING.MATH" (12 caratteri) non entra affatto nel campo
	// funcName[10] della risorsa 'Func' (9 caratteri utili piu' il
	// terminatore) -- allargare quel formato binario condiviso da ogni
	// altra funzione solo per questa non vale il rischio. Alias diretto
	// verso CEILING invece: nell'unico uso reale visto finora
	// (CEILING.MATH con un solo argomento, senza significativita'/
	// modalita' esplicite) i due si comportano identici (arrotondano
	// per eccesso all'intero piu' vicino, vedi CEILINGFunction) -- se
	// un file dovesse mai usare CEILING.MATH con piu' di un argomento,
	// l'analisi grammaticale lo rifiuta comunque (CEILING accetta un
	// solo argomento), non calcola silenziosamente un risultato
	// sbagliato.
	if (MatchesFuncNameAlias(name, "CEILING.MATH"))
		return kCEILINGFuncNr;

	// "CONCATENATE" (11 caratteri) ha lo stesso problema di
	// "CEILING.MATH" sopra: alias diretto verso CONCAT, gia' esistente
	// -- stesso comportamento di concatenazione testo/numeri, solo il
	// nome storico di Excel invece di quello moderno.
	if (MatchesFuncNameAlias(name, "CONCATENATE"))
		return kCONCATFuncNr;

	// "LOG10" (5 caratteri, entrerebbe nella tabella normale, ma alias
	// diretto qui comunque per semplicita': stessa funzione di LOG a un
	// solo argomento, che gia' calcola log10() -- vedi LOGFunction in
	// Functions.math.cpp, dove LOG(number) senza secondo argomento e'
	// sempre in base 10, esattamente il comportamento di Excel. Bug
	// reale scoperto su un file utente vero: LOG10() e' comune nelle
	// formule di perdita di carico (formula di Colebrook-White), la
	// funzione mancante faceva fallire l'analisi grammaticale dell'INTERA
	// formula che la contiene, che restava testo grezzo invece che
	// calcolata -- e quel valore testuale, usato poi in un prodotto con
	// altre celle, propagava NaN a cascata fino al totale finale.
	if (MatchesFuncNameAlias(name, "LOG10"))
		return kLOGFuncNr;

	if (sLen >= (long)sizeof(myFunc))
		return -1;

	for (i = 0; i < sLen; i++)
		myFunc[i] = toupper(name[i]);
	myFunc[sLen] = 0;

	l = 0;
	R = gFuncCount - 1;

	do
	{
		i = (l + R) / 2;
		d = strcmp(myFunc, gFuncArrayByName[i].funcName);
		if (d < 0)
			R = i - 1;
		else if (d > 0)
			l = i + 1;
		else
			return gFuncArrayByName[i].funcNr;
	}
	while (l <= R);

	return -1;
} /* GetFunctionNr */

void ReadCString(BPositionIO& inStream, int inMaxLen, char *outString)
{
	long l = 1;
	
	do inStream.Read(outString++, l);
	while (l && --inMaxLen && outString[-1]);
} // ReadCString

void ReadString(BPositionIO& inStream, char *outString)
{
	char sl;
	long l = 1;
	
	inStream.Read(&sl, l);
	while (l && sl--)
		inStream.Read(outString++, l);
	*outString = 0;
} // ReadString

double atof_i(const char *s)
{
	char nr[32];
	int i = 0, j = 0, sign = 1;

	while (isspace(s[i])) i++;
	if (s[i] == '-')
	{
		sign = -1;
		i++;
	}
	
	do
	{
		if (s[i] == gDecimalPoint)
		{
			nr[j++] = '.';
			i++;
		}
		else if (s[i] == gThousandSeparator)
			i++;
		else if (isdigit(s[i]))
			nr[j++] = s[i++];
		else
			return gValueNan;
		
		if (j >= 31)
			return gValueNan;
	}
	while (s[i]);
	
	nr[j] = 0;
	return atof(nr) * sign;
} // atof_i

bool IsOptionalClick(BMessage *msg)
{
	try
	{
		int32 buttons, modifiers;
		FailOSErr(msg->FindInt32("buttons", &buttons), errMessageMissing);
		FailOSErr(msg->FindInt32("modifiers", &modifiers), errMessageMissing);
		return buttons & B_SECONDARY_MOUSE_BUTTON || modifiers & B_CONTROL_KEY;
	}
	catch(...) {}
	return false;
} /* IsOptionalClick */

const char gHexChar[] = "0123456789ABCDEF";

char* Bin2Hex(void *data, ssize_t size)
{
	char *hex = (char *)MALLOC(size * 2 + 1);
	
	if (hex)
	{
		unsigned char *hp = (unsigned char *)hex, *dp = (unsigned char *)data;
		
		while (size--)
		{
			unsigned char n1, n2;
			
			n1 = *dp >> 4;
			n2 = *dp & 0x0F;
			
			*hp++ = gHexChar[n1];
			*hp++ = gHexChar[n2];
			dp++;
		}
		*hp = 0;
	}
	
	return hex;
} /* Bin2Hex */

void* Hex2Bin(const char *hex, ssize_t& size)
{
	const char *hp;
	char *dp, *result;
	int sLen = strlen(hex);
	
	size = (sLen / 2 + sLen % 2);
	result = (char *)MALLOC(size);

	if (result)
	{
		hp = hex;
		dp = result;
		int i = size;

		while (i--)
		{
			char n1 = *hp++;
			char n2 = *hp++;
			
			if (!isxdigit(n1)) THROW((errNotAHexString));
			if (n2 && !isxdigit(n2)) THROW((errNotAHexString));
			
			if (n1 >= '0' && n1 <= '9')
				n1 -= '0';
			else
				n1 = toupper(n1) - 'A' + 10;
			
			if (n2)
			{
				if (n2 >= '0' && n2 <= '9')
					n2 -= '0';
				else
					n2 = toupper(n2) - 'A' + 10;
			}
			
			*dp++ = n1 << 4 | n2;
		}
	}
	
	return result;
} /* Hex2Bin */

double Nan(int nannr)
{
	double x;
	__HI(x) = 0x7FF00000 | ((nannr & 0x000000FF) << 5);
	__LO(x) = 0;
	return x;
} /* Nan */

int GetNanNr(double x)
{
	return (__HI(x) >> 5) & 0x000000FF;
} /* GetNanNr */
