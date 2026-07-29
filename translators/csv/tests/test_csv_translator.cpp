/*
	test_csv_translator.cpp

	Test diretto del translator CSV/ASCD: parte da un CSV di esempio,
	lo traduce in formato nativo ASCD tramite Translate(), poi
	ritraduce l'ASCD in CSV e verifica che i valori sopravvivano al
	giro completo (round-trip CSV -> ASCD -> CSV).
*/

#include <cstdio>
#include <cstring>

#include <DataIO.h>

#include "CsvTranslator.h"

static int gFailures = 0;

static void Check(bool condition, const char *what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	BTranslator *translator = make_nth_translator(0, 0, 0);
	Check(translator != NULL, "make_nth_translator crea il translator");

	const char *sampleCsv = "10,60\n20,\nCiao,\n";
	BMemoryIO csvIn(sampleCsv, strlen(sampleCsv));

	translator_info csvInfo;
	status_t err = translator->Identify(&csvIn, NULL, NULL, &csvInfo, kAtomoNativeFormat);
	Check(err == B_OK, "Identify riconosce il CSV di esempio");
	Check(csvInfo.type == kAtomoCsvFormat, "Identify classifica il tipo come CSV");

	csvIn.Seek(0, SEEK_SET);
	BMallocIO ascdOut;
	err = translator->Translate(&csvIn, &csvInfo, NULL, kAtomoNativeFormat, &ascdOut);
	Check(err == B_OK, "Translate CSV -> ASCD riesce");

	ascdOut.Seek(0, SEEK_SET);
	translator_info ascdInfo;
	err = translator->Identify(&ascdOut, NULL, NULL, &ascdInfo, kAtomoCsvFormat);
	Check(err == B_OK, "Identify riconosce l'ASCD appena generato");
	Check(ascdInfo.type == kAtomoNativeFormat, "Identify classifica il tipo come ASCD");

	ascdOut.Seek(0, SEEK_SET);
	BMallocIO csvRoundtrip;
	err = translator->Translate(&ascdOut, &ascdInfo, NULL, kAtomoCsvFormat, &csvRoundtrip);
	Check(err == B_OK, "Translate ASCD -> CSV riesce");

	const char *result = (const char *)csvRoundtrip.Buffer();
	size_t resultLen = csvRoundtrip.BufferLength();
	printf("--- CSV round-trip ---\n%.*s\n----------------------\n",
		(int)resultLen, result);

	Check(strstr(result, "10") != NULL, "il round-trip contiene il valore 10");
	Check(strstr(result, "20") != NULL, "il round-trip contiene il valore 20");
	Check(strstr(result, "Ciao") != NULL, "il round-trip contiene il testo Ciao");

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
