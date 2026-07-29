/*
	smoke_test.cpp

	Test minimo, headless, del motore di calcolo isolato: crea un
	documento (CContainer) SENZA alcuna view collegata (nessuna UI,
	nessun app_server), inserisce alcuni valori e delle formule, e
	verifica che i risultati calcolati siano corretti.

	Dimostra concretamente che il motore di calcolo storico di
	Sum-It funziona headless su Haiku moderno a 64 bit, senza
	dipendere dall'Interface Kit (BView/BWindow).
*/

#include <cstdio>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"

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
	// Nessuna CCellView: il documento vive completamente headless.
	// CContainer usa un distruttore privato con reference counting,
	// quindi va allocato sull'heap e rilasciato con Release().
	CContainer &doc = *new CContainer(NULL, NULL);

	cell a1(1, 1);	// colonna A (h=1), riga 1 (v=1)
	cell a2(1, 2);
	cell a3(1, 3);
	cell b1(2, 1);	// colonna B (h=2), riga 1
	cell b2(2, 2);

	TryToParseString("10", a1, &doc, true);
	TryToParseString("20", a2, &doc, true);
	TryToParseString("30", a3, &doc, true);

	TryToParseString("=A1+A2+A3", b1, &doc, true);
	doc.CalcCell(b1);

	TryToParseString("=A1*3", b2, &doc, true);
	doc.CalcCell(b2);

	Value v;
	doc.GetValue(b1, v);
	Check((double)v == 60.0, "B1 (=A1+A2+A3) calcola 60");

	doc.GetValue(b2, v);
	Check((double)v == 30.0, "B2 (=A1*3) calcola 30");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
