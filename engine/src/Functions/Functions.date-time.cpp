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
	Functions.date-time.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include "Container.h"
#include "CellIterator.h"
#include "FunctionUtils.h"
#include "Functions.h"
#include "Formatter.h"
#include "Globals.h"

#include <cstring>

void DATEFunction(Value *stack, int argCnt, CContainer *cells)
{
	double day, month, year;
	bool validDate = false;

	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetDoubleArgument(stack, argCnt, 1, &year) &&
		GetDoubleArgument(stack, argCnt, 2, &month) &&
		GetDoubleArgument(stack, argCnt, 3, &day))
	{
		int d, m, y;
		
		d = static_cast<int>(day);
		m = static_cast<int>(month) ;
		y = static_cast<int>(year) ;
		if( y > 1900 )
			y -= 1900 ;
		
		if (d < 1 || d > 31 || m < 1 || m > 12 || y < 0 || y > 200)
			validDate = false;
		else
		{
			struct tm time, *lt;
			memset(&time, 0, sizeof(time));
			time.tm_year = y;
			time.tm_mon = m - 1;
			time.tm_mday = d;
			
			time_t t = mktime(&time);
			stack[0] = t;
			
			lt = localtime(&t);
			
			validDate =
				lt->tm_year == y &&
				lt->tm_mon == m - 1 &&
				lt->tm_mday == d;
		}
	}

	if (!validDate)
		stack[0] = gDateNan;
}

void DAYFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t theDate;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetTimeArgument(stack, argCnt, 1, &theDate))
	{
		struct tm *theDateRec;
		theDateRec = localtime(&theDate);
		
		stack[0] = (double)theDateRec->tm_mday;
	}
	else
		stack[0] = gDateNan;
}

void DOWFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t theDate;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetTimeArgument(stack, argCnt, 1, &theDate))
	{
		struct tm *theDateRec;
		theDateRec = localtime(&theDate);
		
		stack[0] = (double)theDateRec->tm_wday + 1;
	}
	else
		stack[0] = gDateNan;
}

void HOURFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t theDate;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetTimeArgument(stack, argCnt, 1, &theDate))
	{
		struct tm *theDateRec;
		theDateRec = localtime(&theDate);
		
		stack[0] = (double)theDateRec->tm_hour;
	}
	else
		stack[0] = gDateNan;
}

void MINUTEFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t theDate;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetTimeArgument(stack, argCnt, 1, &theDate))
	{
		struct tm *theDateRec;
		theDateRec = localtime(&theDate);
		
		stack[0] = (double)theDateRec->tm_min;
	}
	else
		stack[0] = gDateNan;
}

void MONTHFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t theDate;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetTimeArgument(stack, argCnt, 1, &theDate))
	{
		struct tm *theDateRec;
		theDateRec = localtime(&theDate);
		
		stack[0] = (double)(theDateRec->tm_mon + 1);
	}
	else
		stack[0] = gDateNan;
}

void NOWFunction(Value *stack, int , CContainer *)
{
	time_t now;
	time(&now);
	stack[0] = now;
} /* NOWFunction */

void SECONDFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t theDate;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetTimeArgument(stack, argCnt, 1, &theDate))
	{
		struct tm *theDateRec;
		theDateRec = localtime(&theDate);
		
		stack[0] = (double)theDateRec->tm_sec;
	}
	else
		stack[0] = gDateNan;
}

void TIMEFunction(Value *stack, int argCnt, CContainer *cells)
{
	double hour, minute, second;
	bool validTime = false;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetDoubleArgument(stack, argCnt, 1, &hour) &&
		GetDoubleArgument(stack, argCnt, 2, &minute) &&
		GetDoubleArgument(stack, argCnt, 3, &second))
	{
		int h, m, s;
		
		h = static_cast<int>(hour) ;
		m = static_cast<int>(minute) ;
		s = static_cast<int>(second) ;

		validTime = (h >= 0 && h < 24) &&
					(m >= 0 && m < 60) &&
					(s >= 0 && s < 60);

		stack[0] = (time_t)(((h * 60) + m) * 60 + s);
	}

	if (!validTime)
		stack[0] = gTimeNan;
}

void TIME2CFunction(Value *stack, int argCnt, CContainer* cells)
{
	time_t t;
	
	if (CheckForNanParameters(stack, argCnt))
		return;
	
	if (GetTimeArgument(stack, argCnt, 1, &t))
	{
		Value v = t;
		char s[64];
		gFormatTable.FormatValue(0, v, s);
		stack[0] = s;
	}
	else
		stack[0] = gTimeNan;
}

void YEARFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t theDate;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (GetTimeArgument(stack, argCnt, 1, &theDate))
	{
		struct tm *theDateRec;
		theDateRec = localtime(&theDate);

		stack[0] = (double)(theDateRec->tm_year + 1900);
	}
	else
		stack[0] = gDateNan;
}

// TODAY/NETWORKDAYS/WORKDAY/EDATE/EOMONTH/DATEDIF (Fase 26, vedi
// ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni originali
// di Sum-It, mancanti confrontando la tabella con l'elenco standard di
// Excel.

// Tronca un time_t a mezzanotte (locale) -- stesso principio del
// "solo giorno, mai un'ora" che TODAY() restituisce sempre.
static time_t TruncateToMidnight(time_t t)
{
	struct tm tmDate = *localtime(&t);
	tmDate.tm_hour = 0;
	tmDate.tm_min = 0;
	tmDate.tm_sec = 0;
	return mktime(&tmDate);
}

void TODAYFunction(Value *stack, int, CContainer *)
{
	time_t now;
	time(&now);
	stack[0] = TruncateToMidnight(now);
}

// Giorni nel mese "month0" (0-11) dell'anno "year" (a 4 cifre) --
// serve a EDATE/EOMONTH sotto per "saturare" un giorno del mese non
// valido nel mese di destinazione (es. 31 gennaio + 1 mese -> 28/29
// febbraio, non "3 marzo" come farebbe mktime() lasciato a se stesso),
// stesso comportamento di Excel.
static int DaysInMonth(int year, int month0)
{
	static const int kDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (month0 == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
		return 29;
	return kDays[month0];
}

// Giorni trascorsi dall'epoca civile (1970-01-01) per una data
// (anno a 4 cifre, mese 1-based, giorno) -- calcolo puramente
// calendariale ("days_from_civil", Howard Hinnant, di dominio
// pubblico), MAI time_t/mktime per contare i giorni fra due date:
// sottrarre due time_t in secondi e dividere per 86400 sballa di
// un'ora ogni volta che l'intervallo attraversa un cambio d'ora legale
// (bug reale scoperto scrivendo il test di DATEDIF "YD" sotto, un
// intervallo che attraversa l'inizio dell'ora legale di fine marzo).
static long long DaysFromCivil(int year, int month, int day)
{
	year -= month <= 2;
	long long era = (year >= 0 ? year : year - 399) / 400;
	unsigned yoe = (unsigned)(year - era * 400);
	unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (long long)doe - 719468;
}

static long long DaysFromCivil(const struct tm &t)
{
	return DaysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

// Riporta un mese "grezzo" (puo' essere negativo o >= 12, dopo aver
// sommato/sottratto un numero di mesi qualunque) nell'intervallo
// valido [0,11], aggiustando l'anno di conseguenza -- stesso calcolo
// che servirebbe sia a EDATE sia a EOMONTH sotto.
static void NormalizeMonth(int &year, int &month0)
{
	year += month0 / 12;
	month0 %= 12;
	if (month0 < 0)
	{
		month0 += 12;
		year--;
	}
}

void EDATEFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t startDate;
	double monthsArg;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (GetTimeArgument(stack, argCnt, 1, &startDate)
		&& GetDoubleArgument(stack, argCnt, 2, &monthsArg))
	{
		struct tm tmDate = *localtime(&startDate);
		int year = tmDate.tm_year + 1900;
		int month0 = tmDate.tm_mon + static_cast<int>(rint(monthsArg));
		NormalizeMonth(year, month0);

		int maxDay = DaysInMonth(year, month0);
		tmDate.tm_year = year - 1900;
		tmDate.tm_mon = month0;
		tmDate.tm_mday = (tmDate.tm_mday > maxDay) ? maxDay : tmDate.tm_mday;
		tmDate.tm_hour = tmDate.tm_min = tmDate.tm_sec = 0;

		stack[0] = mktime(&tmDate);
	}
	else
		stack[0] = gDateNan;
}

void EOMONTHFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t startDate;
	double monthsArg;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (GetTimeArgument(stack, argCnt, 1, &startDate)
		&& GetDoubleArgument(stack, argCnt, 2, &monthsArg))
	{
		struct tm tmDate = *localtime(&startDate);
		int year = tmDate.tm_year + 1900;
		int month0 = tmDate.tm_mon + static_cast<int>(rint(monthsArg));
		NormalizeMonth(year, month0);

		tmDate.tm_year = year - 1900;
		tmDate.tm_mon = month0;
		tmDate.tm_mday = DaysInMonth(year, month0); // ultimo giorno del mese
		tmDate.tm_hour = tmDate.tm_min = tmDate.tm_sec = 0;

		stack[0] = mktime(&tmDate);
	}
	else
		stack[0] = gDateNan;
}

// VERO se "t" (gia' troncato a mezzanotte da chi chiama) e' un giorno
// lavorativo: non sabato/domenica, e non presente in
// holidayCells/holidayRange (entrambi NULL se NETWORKDAYS/WORKDAY non
// hanno ricevuto il terzo argomento opzionale) -- serve a entrambe.
static bool IsWorkday(time_t t, CContainer *holidayCells, range *holidayRange)
{
	struct tm tmDate = *localtime(&t);
	if (tmDate.tm_wday == 0 || tmDate.tm_wday == 6)
		return false;

	if (holidayCells && holidayRange)
	{
		CCellIterator iter(holidayCells, holidayRange);
		cell c;
		while (iter.NextExisting(c))
		{
			Value val;
			holidayCells->GetValue(c, val);
			if (val.fType == eTimeData && TruncateToMidnight(val.fTime) == t)
				return false;
		}
	}
	return true;
}

// NETWORKDAYS (11 caratteri) non entra nel campo funcName[10] a
// lunghezza fissa della risorsa 'Func': registrata internamente come
// "NETDAYS" (vedi funcs_by_nr.r), stesso principio di
// SUBSTITUTE/"SUBST" in Functions.text.cpp -- alias in GetFunctionNr
// (Utils.cpp) verso lo stesso funcNr.
void NETWORKDAYSFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t startDate, endDate;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetTimeArgument(stack, argCnt, 1, &startDate)
		|| !GetTimeArgument(stack, argCnt, 2, &endDate))
	{
		stack[0] = gDateNan;
		return;
	}

	bool reversed = startDate > endDate;
	if (reversed)
	{
		time_t tmp = startDate;
		startDate = endDate;
		endDate = tmp;
	}

	range holidayRange;
	CContainer *holidayCells = NULL;
	bool hasHolidays = argCnt >= 3 && GetRangeArgument(stack, argCnt, 3, &holidayRange)
		&& holidayRange.IsValid();
	if (hasHolidays)
		holidayCells = GetRangeContainer(stack, 3, cells);

	struct tm tmDay = *localtime(&startDate);
	tmDay.tm_hour = tmDay.tm_min = tmDay.tm_sec = 0;
	time_t day = mktime(&tmDay);
	time_t lastDay = TruncateToMidnight(endDate);

	int count = 0;
	while (day <= lastDay)
	{
		if (IsWorkday(day, holidayCells, hasHolidays ? &holidayRange : NULL))
			count++;
		// tm_mday++/mktime, non "+86400 secondi": mktime rinormalizza da
		// solo un giorno del mese fuori intervallo, senza il rischio di
		// saltare/ripetere un giorno attorno a un cambio d'ora legale.
		tmDay.tm_mday++;
		day = mktime(&tmDay);
	}

	stack[0] = (double)(reversed ? -count : count);
}

void WORKDAYFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t startDate;
	double daysArg;

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetTimeArgument(stack, argCnt, 1, &startDate)
		|| !GetDoubleArgument(stack, argCnt, 2, &daysArg))
	{
		stack[0] = gDateNan;
		return;
	}

	int daysToMove = static_cast<int>(rint(daysArg));
	int step = (daysToMove >= 0) ? 1 : -1;
	int remaining = (daysToMove >= 0) ? daysToMove : -daysToMove;

	range holidayRange;
	CContainer *holidayCells = NULL;
	bool hasHolidays = argCnt >= 3 && GetRangeArgument(stack, argCnt, 3, &holidayRange)
		&& holidayRange.IsValid();
	if (hasHolidays)
		holidayCells = GetRangeContainer(stack, 3, cells);

	struct tm tmDay = *localtime(&startDate);
	tmDay.tm_hour = tmDay.tm_min = tmDay.tm_sec = 0;
	time_t day = mktime(&tmDay);

	while (remaining > 0)
	{
		tmDay.tm_mday += step;
		day = mktime(&tmDay);
		if (IsWorkday(day, holidayCells, hasHolidays ? &holidayRange : NULL))
			remaining--;
	}

	stack[0] = day;
}

// DATEDIF(data_inizio,data_fine,unita'): "unita'" e' testo ("Y","M","D",
// "MD","YM","YD", non distingue maiuscole/minuscole) -- stesse sei
// modalita' della vera DATEDIF di Excel (mai stata una funzione
// "ufficiale" nemmeno li', ma comunissima nei fogli reali). Rifiuta
// data_inizio > data_fine: la vera Excel restituisce #NUM! in quel
// caso, non un risultato negativo.
void DATEDIFFunction(Value *stack, int argCnt, CContainer *cells)
{
	time_t startDate, endDate;
	char unit[8];

	if (CheckForNanParameters(stack, argCnt))
		return;

	if (!GetTimeArgument(stack, argCnt, 1, &startDate)
		|| !GetTimeArgument(stack, argCnt, 2, &endDate)
		|| !GetTextArgument(stack, argCnt, 3, unit)
		|| startDate > endDate)
	{
		stack[0] = gValueNan;
		return;
	}

	for (char *p = unit; *p; p++)
		*p = toupper((unsigned char)*p);

	struct tm start = *localtime(&startDate);
	struct tm end = *localtime(&endDate);

	if (strcmp(unit, "D") == 0)
		stack[0] = (double)(DaysFromCivil(end) - DaysFromCivil(start));
	else if (strcmp(unit, "Y") == 0)
	{
		int years = end.tm_year - start.tm_year;
		if (end.tm_mon < start.tm_mon || (end.tm_mon == start.tm_mon && end.tm_mday < start.tm_mday))
			years--;
		stack[0] = (double)years;
	}
	else if (strcmp(unit, "M") == 0)
	{
		int months = (end.tm_year - start.tm_year) * 12 + (end.tm_mon - start.tm_mon);
		if (end.tm_mday < start.tm_mday)
			months--;
		stack[0] = (double)months;
	}
	else if (strcmp(unit, "YM") == 0)
	{
		// Mesi restanti ignorando gli anni interi trascorsi (0-11).
		int months = end.tm_mon - start.tm_mon;
		if (end.tm_mday < start.tm_mday)
			months--;
		if (months < 0)
			months += 12;
		stack[0] = (double)months;
	}
	else if (strcmp(unit, "MD") == 0)
	{
		// Giorni restanti ignorando mesi/anni interi trascorsi.
		int days = end.tm_mday - start.tm_mday;
		if (days < 0)
		{
			int prevMonth = end.tm_mon - 1;
			int prevYear = end.tm_year + 1900;
			if (prevMonth < 0)
			{
				prevMonth = 11;
				prevYear--;
			}
			days += DaysInMonth(prevYear, prevMonth);
		}
		stack[0] = (double)days;
	}
	else if (strcmp(unit, "YD") == 0)
	{
		// Giorni restanti ignorando gli anni interi trascorsi: la
		// differenza fra data_fine e data_inizio "portata" all'anno di
		// data_fine (o quello precedente, se cosi' verrebbe dopo).
		struct tm anchor = start;
		anchor.tm_year = end.tm_year;
		if (DaysFromCivil(anchor) > DaysFromCivil(end))
			anchor.tm_year--;
		stack[0] = (double)(DaysFromCivil(end) - DaysFromCivil(anchor));
	}
	else
		stack[0] = gValueNan;
}

