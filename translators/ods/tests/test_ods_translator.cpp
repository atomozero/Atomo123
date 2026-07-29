/*
	test_ods_translator.cpp

	Test end-to-end del translator ODS: apre tests/sample.ods (un
	file ODS reale, costruito a mano con "zip" e verificato apribile
	con "unzip -l"), lo traduce in ASCD, poi legge l'ASCD prodotto e
	verifica che i valori e la formula siano stati importati e
	calcolati correttamente dal motore.

	sample.ods contiene (Foglio1):
		A1 = 15
		B1 = 25
		C1 = formula ODF "of:=[.A1]+[.B1]" (valore gia' calcolato: 40 —
		     il translator lo ignora e lascia che il nostro motore
		     ricalcoli la formula in modo indipendente)
		D1 = stringa "Ciao ODS"
		E1..N1 = celle vuote compresse con table:number-columns-repeated
		     (non devono generare celle nell'ASCD)
*/

#include <cstdio>
#include <cstring>
#include <string>

#include <File.h>
#include <DataIO.h>
#include <SupportDefs.h>

#include "OdsTranslator.h"
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
	BTranslator *translator = make_nth_translator(0, 0, 0);
	Check(translator != NULL, "make_nth_translator crea il translator");

	BFile odsFile("tests/sample.ods", B_READ_ONLY);
	Check(odsFile.InitCheck() == B_OK, "apertura di tests/sample.ods riuscita");

	translator_info info;
	status_t err = translator->Identify(&odsFile, NULL, NULL, &info, 0);
	Check(err == B_OK, "Identify riconosce il file ODS reale");
	Check(info.type == kAtomoOdsFormat, "Identify classifica il tipo come ODS");

	odsFile.Seek(0, SEEK_SET);
	BMallocIO ascdOut;
	err = translator->Translate(&odsFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
	Check(err == B_OK, "Translate ODS -> ASCD riesce");

	const unsigned char *ascdData = (const unsigned char *)ascdOut.Buffer();
	size_t ascdLen = ascdOut.BufferLength();

	Check(ascdLen > 12 && memcmp(ascdData, "ASCD", 4) == 0,
		"l'ASCD prodotto ha l'intestazione attesa");

	if (ascdLen > 12)
	{
		int32 count;
		memcpy(&count, ascdData + 8, 4);
		Check(count == 4, "l'ASCD contiene solo le 4 celle con contenuto (non quelle vuote ripetute)");

		bool foundA1 = false, foundB1 = false, foundFormula = false, foundString = false;

		CContainer &doc = *new CContainer(NULL, NULL);

		size_t pos = 12;
		for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
		{
			int16 row, col;
			int32 len;
			memcpy(&row, ascdData + pos, 2); pos += 2;
			memcpy(&col, ascdData + pos, 2); pos += 2;
			memcpy(&len, ascdData + pos, 4); pos += 4;
			if (pos + len > ascdLen)
				break;

			std::string text((const char *)ascdData + pos, len);
			pos += len;

			printf("  cella riga=%d colonna=%d -> \"%s\"\n", row, col, text.c_str());

			cell loc(col, row);
			TryToParseString(text.c_str(), loc, &doc, true);

			if (row == 1 && col == 1 && text == "15")
				foundA1 = true;
			if (row == 1 && col == 2 && text == "25")
				foundB1 = true;
			// La formula ODF "of:=[.A1]+[.B1]" deve essere stata
			// convertita in "A1+B1" (senza il prefisso ODF ne' le
			// parentesi quadre), non appiattita al valore gia'
			// calcolato da LibreOffice.
			if (row == 1 && col == 3 && text.find("A1") != std::string::npos
				&& text.find("B1") != std::string::npos
				&& text.find('[') == std::string::npos)
				foundFormula = true;
			if (row == 1 && col == 4 && text.find("Ciao ODS") != std::string::npos)
				foundString = true;
		}

		Check(foundA1, "A1 importato correttamente come 15");
		Check(foundB1, "B1 importato correttamente come 25");
		Check(foundFormula,
			"C1 (formula ODF) importata e convertita in formula nativa, non appiattita al valore");
		Check(foundString, "D1 importato correttamente come \"Ciao ODS\"");

		cell c1(3, 1);
		doc.CalcCell(c1);
		Value v;
		doc.GetValue(c1, v);
		Check((double)v == 40.0,
			"il motore ricalcola la formula importata dall'ODF e ottiene 40");

		doc.Release();
	}

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
