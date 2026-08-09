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

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
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
#include "CellIterator.h"
#include "CellParser.h"

static int gFailures = 0;

// Scrive un flusso ASCD a mano (stesso formato di WriteASCD in
// OdsTranslator.cpp, che pero' e' privata a quel file): serve solo a
// costruire un sorgente di prova per il test di export sotto, senza
// passare dalla vera GUI.
static status_t WriteASCDForTest(CContainer *doc, BPositionIO *dest)
{
	static const char kMagic[4] = { 'A', 'S', 'C', 'D' };
	static const int32 kVersion = 1;

	range bounds;
	doc->GetBounds(bounds);

	int32 count = 0;
	CCellIterator counter(doc, &bounds);
	cell c;
	while (counter.NextExisting(c))
		count++;

	if (dest->Write(kMagic, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kVersion, sizeof(kVersion)) != (ssize_t)sizeof(kVersion))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;

	CCellIterator iter(doc, &bounds);
	while (iter.NextExisting(c))
	{
		char text[512];
		doc->GetCellFormula(c, text, sizeof(text), false);

		int16 row = c.v, col = c.h;
		int32 len = strlen(text);

		if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_IO_ERROR;
		if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_IO_ERROR;
		if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_IO_ERROR;
		if (len > 0 && dest->Write(text, len) != len)
			return B_IO_ERROR;
	}

	return B_OK;
}

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

	// Bug reale, crash vero di Tracker catturato in un .report: il
	// thumbnail worker chiama BTranslatorRoster::Translate() con
	// info=NULL per ogni file mentre genera le anteprime (significa
	// "identifica tu stesso il formato sorgente", documentato nel
	// Translation Kit), senza mai passare da Identify() prima.
	odsFile.Seek(0, SEEK_SET);
	BMallocIO ascdOutNullInfo;
	status_t errNullInfo = translator->Translate(&odsFile, NULL, NULL, kAtomoNativeFormat, &ascdOutNullInfo);
	Check(errNullInfo == B_OK, "Translate con info=NULL (come fa Tracker per le anteprime) non crasha, si identifica da solo");

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

	// Esportazione (ASCD -> ODS): scrive un documento con un numero,
	// una stringa e una formula, poi rilegge il file ODS prodotto con
	// lo stesso translator (round-trip completo) per verificare che
	// sia un file ODS valido e che i VALORI (non le formule, scelta
	// deliberata -- vedi WriteODS in OdsTranslator.cpp) siano
	// corretti.
	{
		CContainer &exportDoc = *new CContainer(NULL, NULL);
		TryToParseString("12", cell(1, 1), &exportDoc, true);   // A1 = 12
		TryToParseString("8", cell(2, 1), &exportDoc, true);    // B1 = 8
		TryToParseString("=A1+B1", cell(3, 1), &exportDoc, true); // C1 = 20
		TryToParseString("Prova export", cell(1, 2), &exportDoc, true); // A2
		exportDoc.CalcCell(cell(3, 1));

		BMallocIO ascdIn;
		status_t saveErr = WriteASCDForTest(&exportDoc, &ascdIn);
		Check(saveErr == B_OK, "preparazione dell'ASCD di prova per l'export riesce");
		exportDoc.Release();

		ascdIn.Seek(0, SEEK_SET);
		translator_info exportInfo;
		err = translator->Identify(&ascdIn, NULL, NULL, &exportInfo, kAtomoOdsFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD come sorgente per l'export");
		Check(exportInfo.type == kAtomoNativeFormat,
			"Identify classifica il sorgente come ASCD nativo");

		ascdIn.Seek(0, SEEK_SET);
		BMallocIO odsOut;
		err = translator->Translate(&ascdIn, &exportInfo, NULL, kAtomoOdsFormat, &odsOut);
		Check(err == B_OK, "Translate ASCD -> ODS riesce");

		// Round-trip: rilegge il file ODS appena scritto con lo stesso
		// translator, come se fosse un file reale aperto da "File ->
		// Apri" -- la prova piu' rigorosa possibile senza un vero
		// LibreOffice per aprirlo (vedi ROADMAP.md, corpus di file
		// reali, ancora da fare).
		odsOut.Seek(0, SEEK_SET);
		translator_info reimportInfo;
		err = translator->Identify(&odsOut, NULL, NULL, &reimportInfo, 0);
		Check(err == B_OK && reimportInfo.type == kAtomoOdsFormat,
			"il file ODS appena scritto viene riconosciuto come ODS valido rileggendolo");

		odsOut.Seek(0, SEEK_SET);
		BMallocIO ascdOut2;
		err = translator->Translate(&odsOut, &reimportInfo, NULL, kAtomoNativeFormat, &ascdOut2);
		Check(err == B_OK, "il file ODS appena scritto si rilegge correttamente (round-trip)");

		if (err == B_OK)
		{
			// Rilegge a mano lo stesso formato binario gia' decodificato
			// sopra per il primo test, invece di passare di nuovo dal
			// translator.
			const unsigned char *data = (const unsigned char *)ascdOut2.Buffer();
			size_t len = ascdOut2.BufferLength();
			size_t pos = 12;
			int32 cnt;
			memcpy(&cnt, data + 8, 4);

			bool reA1 = false, reB1 = false, reC1 = false, reA2 = false;
			for (int32 i = 0; i < cnt && pos + 8 <= len; i++)
			{
				int16 row, col;
				int32 tlen;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&tlen, data + pos, 4); pos += 4;
				if (pos + tlen > len)
					break;
				std::string text((const char *)data + pos, tlen);
				pos += tlen;

				if (row == 1 && col == 1 && text == "12") reA1 = true;
				if (row == 1 && col == 2 && text == "8") reB1 = true;
				if (row == 1 && col == 3 && text == "20") reC1 = true;
				if (row == 2 && col == 1 && text == "Prova export") reA2 = true;
			}

			Check(reA1, "dopo il round-trip, A1 vale ancora 12");
			Check(reB1, "dopo il round-trip, B1 vale ancora 8");
			Check(reC1,
				"dopo il round-trip, C1 (era una formula) vale 20 -- il valore calcolato, "
				"non la formula (scelta deliberata dell'export)");
			Check(reA2, "dopo il round-trip, A2 vale ancora \"Prova export\"");
		}
	}

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
