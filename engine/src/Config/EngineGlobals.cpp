/*
	EngineGlobals.cpp

	Definizioni delle variabili globali dichiarate "extern" in
	Globals.h. Nel codice storico venivano definite e inizializzate in
	Sum-It.cpp (App/), che e' l'entry point della UI e non fa parte
	della libreria engine isolata. Qui si forniscono valori di default
	ragionevoli per un uso headless (nessuna preferenza utente/locale
	da leggere): un chiamante headless che vuole un locale diverso puo'
	sovrascrivere queste variabili prima di usare l'engine.
*/

#include "Globals.h"
#include "Utils.h"

double	gRefNan, gCircleNan, gSqrtNan, gPowerNan, gValueNan, gDivNan,
		gAddNan, gFinanceNan, gEvalNan, gInvTrigNan, gLogNan, gMulNan,
		gNANan, gErrorNan, gDateNan, gTimeNan, gFuncNan, gNameNan;

char	gDecimalPoint = '.', gThousandSeparator = ',', gListSeparator = ';',
		gTimeSeparator = ':', gDateSeparator = '/';

bool	g24Hours = true;

// gCurrencyBefore/gCurrencyParens/gCurrencySymbol/gCurrencyDigits
// sono gia' definite in Formatter.cpp (fanno parte del suo stato di
// formattazione di default), quindi non vanno ridefinite qui.

BPath		gAppName;
BDirectory	*gAppDir = NULL, *gCWD = NULL;

namespace {

struct EngineGlobalsInit {
	EngineGlobalsInit()
	{
		gErrorNan = Nan(1);
		gRefNan = Nan(3);
		gCircleNan = Nan(4);
		gSqrtNan = Nan(5);
		gPowerNan = Nan(6);
		gValueNan = Nan(7);
		gDivNan = Nan(8);
		gAddNan = Nan(9);
		gFinanceNan = Nan(10);
		gEvalNan = Nan(11);
		gInvTrigNan = Nan(12);
		gLogNan = Nan(13);
		gMulNan = Nan(14);
		gNANan = Nan(15);
		gDateNan = Nan(16);
		gTimeNan = Nan(17);
		gFuncNan = Nan(19);
		gNameNan = Nan(20);
	}
};

EngineGlobalsInit gEngineGlobalsInit;

}
