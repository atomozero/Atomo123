/*
	test_xlsx_translator.cpp

	Test end-to-end del translator XLSX: apre tests/sample.xlsx (un
	file XLSX reale, costruito con il comando "zip" e verificato
	apribile con "unzip -l"), lo traduce in ASCD, poi legge l'ASCD
	prodotto e verifica che i valori e la formula siano stati
	importati e calcolati correttamente dal motore.

	sample.xlsx contiene:
		A1 = 15
		B1 = 25
		C1 = formula =A1+B1 (valore già calcolato da Excel/LibreOffice: 40 —
		     il translator lo ignora e lascia che il nostro motore
		     ricalcoli la formula in modo indipendente)
		D1 = stringa condivisa "Ciao XLSX"
*/

#include <cstdio>
#include <cstring>
#include <string>

#include <File.h>
#include <DataIO.h>
#include <SupportDefs.h>

#include "XlsxTranslator.h"
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

	BFile xlsxFile("tests/sample.xlsx", B_READ_ONLY);
	Check(xlsxFile.InitCheck() == B_OK, "apertura di tests/sample.xlsx riuscita");

	translator_info info;
	status_t err = translator->Identify(&xlsxFile, NULL, NULL, &info, 0);
	Check(err == B_OK, "Identify riconosce il file XLSX reale");
	Check(info.type == kAtomoXlsxFormat, "Identify classifica il tipo come XLSX");

	xlsxFile.Seek(0, SEEK_SET);
	BMallocIO ascdOut;
	err = translator->Translate(&xlsxFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
	Check(err == B_OK, "Translate XLSX -> ASCD riesce");

	// Rilegge l'ASCD prodotto per verificare i valori importati: si
	// riusa lo stesso formato del translator CSV, quindi basta
	// controllare che il testo delle celle (formula o valore
	// formattato) contenga quanto atteso.
	const unsigned char *ascdData = (const unsigned char *)ascdOut.Buffer();
	size_t ascdLen = ascdOut.BufferLength();

	Check(ascdLen > 12 && memcmp(ascdData, "ASCD", 4) == 0,
		"l'ASCD prodotto ha l'intestazione attesa");

	if (ascdLen > 12)
	{
		int32 count;
		memcpy(&count, ascdData + 8, 4);
		Check(count == 4, "l'ASCD contiene le 4 celle del foglio di esempio");

		bool foundA1 = false, foundFormulaResult = false, foundString = false;

		// Ricostruisce un documento a partire dai dati ASCD (stesso
		// procedimento di ReadASCD nel translator CSV) per verificare
		// non solo che il testo sia quello atteso, ma che il motore
		// calcoli davvero il risultato corretto dalla formula
		// importata.
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
			// ASCD preserva la formula (non il valore gia' calcolato
			// da Excel/LibreOffice): il testo della cella deve
			// contenere il riferimento alla formula originale.
			if (row == 1 && col == 3 && text.find("A1") != std::string::npos
				&& text.find("B1") != std::string::npos)
				foundFormulaResult = true;
			if (row == 1 && col == 4 && text.find("Ciao XLSX") != std::string::npos)
				foundString = true;
		}

		Check(foundA1, "A1 importato correttamente come 15");
		Check(foundFormulaResult,
			"C1 (formula =A1+B1) importata come formula, non appiattita al valore");
		Check(foundString, "D1 (stringa condivisa) importata come \"Ciao XLSX\"");

		cell c1(3, 1);
		doc.CalcCell(c1);
		Value v;
		doc.GetValue(c1, v);
		Check((double)v == 40.0,
			"il motore ricalcola la formula importata e ottiene 40");

		doc.Release();
	}

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
