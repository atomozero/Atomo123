/*
	named_functions_test.cpp

	Verifica che le formule con funzioni con nome (SUM, IF, ecc.)
	funzionino davvero, non solo che il motore compili: fino a questa
	sessione la libreria engine non chiamava mai InitFunctions(),
	quindi GetFunctionNr trattava sempre "SUM(...)" come un
	identificatore sconosciuto invece che come una chiamata di
	funzione (vedi docs/ENGINE_API.md, nota "funzioni con nome non
	sono ancora realmente utilizzabili").

	A differenza di smoke_test.cpp (che testa solo formule aritmetiche
	senza funzioni con nome, e quindi non aveva mai bisogno di questo
	passaggio), questo test lega esplicitamente gResourceManager/
	gAppName a un file di risorse compilato al volo da questo stesso
	Makefile (con gli strumenti storici rez/bsl, vedi target
	test-functions) e chiama InitFunctions() prima di analizzare le
	formule -- lo stesso passaggio che ui/src/App.cpp fa all'avvio
	dell'app vera.
*/

#include <cstdio>

#include <Path.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "ResourceManager.h"

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
	BPath path("tests/named_functions.rsrc");
	gAppName = path;
	if (gResourceManager.SetTo(&path) != B_OK)
	{
		printf("FAIL impossibile aprire tests/named_functions.rsrc\n");
		return 1;
	}

	try
	{
		InitFunctions();
	}
	catch (CErr &e)
	{
		printf("FAIL InitFunctions ha lanciato un'eccezione: %s\n", (char *)e);
		return 1;
	}

	Check(gFuncCount == 89, "InitFunctions carica tutte le 89 funzioni della risorsa 'Func'");

	CContainer &doc = *new CContainer(NULL, NULL);

	cell a1(1, 1), a2(1, 2), a3(1, 3);
	cell b1(2, 1), b2(2, 2), b3(2, 3);

	TryToParseString("10", a1, &doc, true);
	TryToParseString("20", a2, &doc, true);
	TryToParseString("30", a3, &doc, true);

	Value v;
	try
	{
		TryToParseString("=SUM(A1:A3)", b1, &doc, true);
		doc.CalcCell(b1);
		doc.GetValue(b1, v);
		Check((double)v == 60.0, "=SUM(A1:A3) calcola 60");
	}
	catch (CErr &e)
	{
		printf("FAIL =SUM(A1:A3): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=IF(A1>5;100;200)", b2, &doc, true);
		doc.CalcCell(b2);
		doc.GetValue(b2, v);
		Check((double)v == 100.0, "=IF(A1>5;100;200) calcola 100 (ramo vero)");
	}
	catch (CErr &e)
	{
		printf("FAIL =IF(A1>5;100;200): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=MAX(A1:A3)", b3, &doc, true);
		doc.CalcCell(b3);
		doc.GetValue(b3, v);
		Check((double)v == 30.0, "=MAX(A1:A3) calcola 30");
	}
	catch (CErr &e)
	{
		printf("FAIL =MAX(A1:A3): %s\n", (char *)e);
		gFailures++;
	}

	// SUMIF/COUNTIF/AVERAGEIF: assenti dalle 86 funzioni originali di
	// Sum-It, aggiunte in questa sessione perche' mancava proprio
	// l'aggregazione condizionata. D1:D4 = categoria, E1:E4 = valore.
	TryToParseString("Mela", cell(4, 1), &doc, true);
	TryToParseString("10", cell(5, 1), &doc, true);
	TryToParseString("Pera", cell(4, 2), &doc, true);
	TryToParseString("5", cell(5, 2), &doc, true);
	TryToParseString("Mela", cell(4, 3), &doc, true);
	TryToParseString("20", cell(5, 3), &doc, true);
	TryToParseString("Banana", cell(4, 4), &doc, true);
	TryToParseString("7", cell(5, 4), &doc, true);

	try
	{
		TryToParseString("=SUMIF(D1:D4;\"Mela\";E1:E4)", cell(6, 1), &doc, true);
		doc.CalcCell(cell(6, 1));
		doc.GetValue(cell(6, 1), v);
		Check((double)v == 30.0, "=SUMIF(D1:D4;\"Mela\";E1:E4) calcola 30 (10+20)");
	}
	catch (CErr &e)
	{
		printf("FAIL =SUMIF: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=COUNTIF(D1:D4;\"Mela\")", cell(6, 2), &doc, true);
		doc.CalcCell(cell(6, 2));
		doc.GetValue(cell(6, 2), v);
		Check((double)v == 2.0, "=COUNTIF(D1:D4;\"Mela\") calcola 2");
	}
	catch (CErr &e)
	{
		printf("FAIL =COUNTIF: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=AVERAGEIF(D1:D4;\"Mela\";E1:E4)", cell(6, 3), &doc, true);
		doc.CalcCell(cell(6, 3));
		doc.GetValue(cell(6, 3), v);
		Check((double)v == 15.0, "=AVERAGEIF(D1:D4;\"Mela\";E1:E4) calcola 15 (media di 10 e 20)");
	}
	catch (CErr &e)
	{
		printf("FAIL =AVERAGEIF: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=SUMIF(E1:E4;\">8\")", cell(6, 4), &doc, true);
		doc.CalcCell(cell(6, 4));
		doc.GetValue(cell(6, 4), v);
		Check((double)v == 30.0,
			"=SUMIF(E1:E4;\">8\") con operatore di confronto calcola 30 (10+20, esclusi 5 e 7)");
	}
	catch (CErr &e)
	{
		printf("FAIL =SUMIF con operatore: %s\n", (char *)e);
		gFailures++;
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
