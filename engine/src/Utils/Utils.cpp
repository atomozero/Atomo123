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
	
	Part of Sum-It for the BeBox version 1.1.

*/
/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.headers.pch++"
 *** TPV (2000-Feb-06) Added Headers Guards
 ***/

#ifndef   CONTAINER_H
#include "Container.h"
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

int GetFunctionNr(const char *name)
{
	long l, R, i, sLen;
	char myFunc[10];
	int d;

	sLen = strlen(name);
	if (sLen >= 9)
		return -1;

	/* Se la tabella delle funzioni non e' mai stata caricata (nessuno
	   ha chiamato InitFunctions(), che la popola da una risorsa 'Func'
	   — caso normale nella libreria engine isolata, che non allega
	   ancora risorse), gFuncArrayByName resta NULL e gFuncCount resta
	   0. Senza questo controllo la ricerca binaria sottostante
	   dereferenzia un puntatore nullo. */
	if (gFuncCount <= 0)
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
