/*
	test_circular_reference.cpp

	Verifica il rilevamento dei riferimenti circolari (RecalculateAll/
	RecalculateWorkbook in AscdIO.cpp): bug reale segnalato da un
	utente, "=A1+A2+A3+A4" scritta in A4 stessa non dava nessun errore,
	calcolava un valore arbitrario (~30000 con A1=100/A2=200/A3=300) --
	il prodotto meccanico esatto del "guard" a 50 passate su una
	formula che diverge linearmente (+600 a ogni passata), non un
	valore a caso. Copre sia il caso diretto (una cella che referenzia
	se stessa) sia quello indiretto (due celle che si referenziano a
	vicenda), su un solo foglio e su un workbook multi-foglio.
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <Path.h>
#include <Roster.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "ResourceManager.h"

static int gFailures = 0;

static void Check(bool condition, const char* what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

static bool IsCircularMarker(CContainer* doc, cell c)
{
	Value v;
	doc->GetValue(c, v);
	return v.fType == eTextData && strcmp((const char*)v, "#CIRCULAR!") == 0;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestCircularRef");

	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	// Caso diretto, esattamente la segnalazione originale: A1=100,
	// A2=200, A3=300, A4="=A1+A2+A3+A4" (A4 referenzia se stessa).
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("100", cell(1, 1), doc, true);
		TryToParseString("200", cell(1, 2), doc, true);
		TryToParseString("300", cell(1, 3), doc, true);
		TryToParseString("=A1+A2+A3+A4", cell(1, 4), doc, true);

		RecalculateAll(doc);

		Check(IsCircularMarker(doc, cell(1, 4)),
			"A4=\"=A1+A2+A3+A4\" (riferimento diretto a se stessa) viene marcata "
			"come errore invece di calcolare un valore arbitrario");

		Value v;
		doc->GetValue(cell(1, 1), v);
		Check(v.fType == eNumData && (double)v == 100,
			"A1 (letterale, non coinvolta nel ciclo) resta 100");

		doc->Release();
	}

	// Caso indiretto: B1 = "=B2+1", B2 = "=B1+1" (nessuna delle due
	// referenzia se stessa direttamente, ma insieme formano un ciclo).
	// Entrambe devono finire marcate, non solo una.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("=B2+1", cell(2, 1), doc, true);
		TryToParseString("=B1+1", cell(2, 2), doc, true);

		RecalculateAll(doc);

		Check(IsCircularMarker(doc, cell(2, 1)),
			"riferimento circolare indiretto: B1 (dipende da B2, che dipende di nuovo "
			"da B1) viene marcata come errore");
		Check(IsCircularMarker(doc, cell(2, 2)),
			"riferimento circolare indiretto: anche B2 viene marcata, non solo B1");

		doc->Release();
	}

	// Una formula normale che referenzia una cella coinvolta in un
	// ciclo (ma non ne fa essa stessa parte) non deve, a sua volta,
	// finire marcata come "#CIRCULAR!" -- il suo output naturale
	// (qualunque esso sia, propagato dall'aritmetica su un valore non
	// numerico) e' un problema diverso, gia' preesistente, non
	// l'oggetto di questa correzione.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("=A1", cell(1, 1), doc, true); // A1 referenzia se stessa
		TryToParseString("=A1*2", cell(2, 1), doc, true); // B1 dipende da A1, non e' nel ciclo

		RecalculateAll(doc);

		Check(IsCircularMarker(doc, cell(1, 1)), "A1=\"=A1\" viene marcata come errore");
		Check(!IsCircularMarker(doc, cell(2, 1)),
			"B1=\"=A1*2\" (dipende dalla cella circolare ma non ne fa parte) NON viene "
			"marcata essa stessa come \"#CIRCULAR!\"");

		doc->Release();
	}

	// Stesso identico caso diretto, ma attraverso RecalculateWorkbook
	// (il percorso multi-foglio usato dall'apertura di un vero file):
	// MarkCircularReferences va applicata per-foglio li' dentro, non
	// solo in RecalculateAll.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("100", cell(1, 1), doc, true);
		TryToParseString("200", cell(1, 2), doc, true);
		TryToParseString("300", cell(1, 3), doc, true);
		TryToParseString("=A1+A2+A3+A4", cell(1, 4), doc, true);

		std::vector<AscdSheet> sheets;
		AscdSheet s; s.name = "Foglio1"; s.doc = doc;
		sheets.push_back(s);

		RecalculateWorkbook(sheets, NULL, NULL);

		Check(IsCircularMarker(doc, cell(1, 4)),
			"RecalculateWorkbook marca allo stesso modo una cella circolare "
			"(percorso multi-foglio usato dall'apertura di un file)");

		doc->Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
