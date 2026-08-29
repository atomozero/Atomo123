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

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <Application.h>
#include <File.h>
#include <DataIO.h>
#include <Font.h>
#include <Message.h>
#include <Path.h>
#include <String.h>
#include <SupportDefs.h>

#include "XlsxTranslator.h"
#include "MiniZip.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "Globals.h"
#include "NameTable.h"
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

static const char kASCDMagicForTest[4] = { 'A', 'S', 'C', 'D' };
static const int32 kASCDVersionForTest = 1;

// Duplica la logica di WriteASCD (static in XlsxTranslator.cpp, non
// esportata) solo per costruire qui un flusso ASCD di prova da dare
// in pasto al translator nella direzione di export (ASCD -> XLSX).
static status_t WriteASCDForTest(CContainer* doc, BPositionIO* dest)
{
	range bounds;
	doc->GetBounds(bounds);

	int32 count = 0;
	CCellIterator counter(doc, NULL);
	cell c;
	while (counter.NextExisting(c))
		count++;

	if (dest->Write(kASCDMagicForTest, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kASCDVersionForTest, sizeof(kASCDVersionForTest))
		!= (ssize_t)sizeof(kASCDVersionForTest))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;

	CCellIterator iter(doc, NULL);
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

// Same idea as WriteASCDForTest above, but with a real frozen-panes
// value instead of the implicit 0,0 (100% XLSX standard compatibility,
// Tier 2): freeze comes right after rowHeights in the real WriteASCD
// section order, well before comments/hyperlinks/validation -- only
// chart/colWidths/cellColors/columnColors/rowHeights need a real
// (empty) entry before it, everything after can be omitted.
static status_t WriteASCDWithFreezeForTest(CContainer* doc, int32 frozenRows, int32 frozenCols,
	BPositionIO* dest)
{
	status_t err = WriteASCDForTest(doc, dest);
	if (err != B_OK)
		return err;

	// Grafici incorporati, colWidths, cellColors, columnColors,
	// rowHeights: cinque conteggi a zero.
	for (int i = 0; i < 5; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Blocca riquadri: due int32 veri, MAI un conteggio davanti.
	if (dest->Write(&frozenRows, sizeof(frozenRows)) != (ssize_t)sizeof(frozenRows)
		|| dest->Write(&frozenCols, sizeof(frozenCols)) != (ssize_t)sizeof(frozenCols))
		return B_IO_ERROR;

	return B_OK;
}

// Come WriteASCDForTest sopra, ma scrive anche UN grafico incorporato
// (Fase 24, esportazione dei grafici verso XLSX) -- replica a mano
// l'INTERO formato ASCD in coda (vedi SaveASCD in ui/src/AscdIO.cpp),
// non solo la sezione grafici, perche' XlsxTranslator::ReadASCD legge
// le sezioni IN ORDINE fino al titolo del grafico incluso (l'ultima):
// scrivere solo chartCount e fermarsi qui lascerebbe il tipo/titolo al
// valore predefinito (0=barre, titolo vuoto), che basta per un test
// ma non per verificare che tipo/titolo arrivino davvero fino a
// chart1.xml. Ogni sezione intermedia e' vuota (count/valore a zero),
// solo grafico/tipo/titolo hanno un valore vero.
static status_t WriteASCDWithChartForTest(CContainer* doc, int16 chartLeft, int16 chartTop,
	int16 chartRight, int16 chartBottom, float frameLeft, float frameTop,
	float frameRight, float frameBottom, int8 chartType, const char* chartTitle,
	BPositionIO* dest)
{
	status_t err = WriteASCDForTest(doc, dest);
	if (err != B_OK)
		return err;

	// Grafici incorporati: chartCount=1, un record.
	{
		int32 chartCount = 1;
		float frame[4] = { frameLeft, frameTop, frameRight, frameBottom };
		if (dest->Write(&chartCount, sizeof(chartCount)) != (ssize_t)sizeof(chartCount)
			|| dest->Write(&chartLeft, sizeof(chartLeft)) != (ssize_t)sizeof(chartLeft)
			|| dest->Write(&chartTop, sizeof(chartTop)) != (ssize_t)sizeof(chartTop)
			|| dest->Write(&chartRight, sizeof(chartRight)) != (ssize_t)sizeof(chartRight)
			|| dest->Write(&chartBottom, sizeof(chartBottom)) != (ssize_t)sizeof(chartBottom)
			|| dest->Write(frame, sizeof(frame)) != (ssize_t)sizeof(frame))
			return B_IO_ERROR;
	}

	// colWidths, cellColors, columnColors, rowHeights: quattro conteggi
	// a zero, stesso schema (int32 count).
	for (int i = 0; i < 4; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Blocca riquadri: due int32, sempre presenti (non un conteggio).
	{
		int32 fr = 0, fc = 0;
		if (dest->Write(&fr, sizeof(fr)) != (ssize_t)sizeof(fr)
			|| dest->Write(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
			return B_IO_ERROR;
	}

	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto conteggi a zero.
	for (int i = 0; i < 8; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Visibilita' griglia: un byte, sempre presente.
	{
		uint8 sg = 1;
		if (dest->Write(&sg, sizeof(sg)) != (ssize_t)sizeof(sg))
			return B_IO_ERROR;
	}

	// Colore linguetta foglio: un byte "has" + 3 byte rgb, sempre presenti.
	{
		uint8 has = 0;
		uint8 rgb[3] = { 0, 0, 0 };
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
			return B_IO_ERROR;
	}

	// Righe nascoste: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// AutoFilter: un byte "has" + 4 int16, sempre presenti.
	{
		uint8 has = 0;
		int16 z16 = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16))
			return B_IO_ERROR;
	}

	// commenti, collegamenti ipertestuali: due conteggi a zero.
	for (int i = 0; i < 2; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Tipo di grafico incorporato: chartTypeCount=1, un byte -- stesso
	// ordine dell'array di grafici scritto piu' sopra.
	{
		int32 chartTypeCount = 1;
		if (dest->Write(&chartTypeCount, sizeof(chartTypeCount)) != (ssize_t)sizeof(chartTypeCount)
			|| dest->Write(&chartType, sizeof(chartType)) != (ssize_t)sizeof(chartType))
			return B_IO_ERROR;
	}

	// Colore del bordo di cella: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Convalida dati, formattazione condizionale, tabelle strutturate:
	// tre conteggi a zero.
	for (int i = 0; i < 3; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Titolo di grafico incorporato: ULTIMA sezione, chartTitleCount=1,
	// una stringa -- stesso ordine dell'array di grafici.
	{
		int32 chartTitleCount = 1;
		int32 titleLen = (int32)strlen(chartTitle);
		if (dest->Write(&chartTitleCount, sizeof(chartTitleCount)) != (ssize_t)sizeof(chartTitleCount)
			|| dest->Write(&titleLen, sizeof(titleLen)) != (ssize_t)sizeof(titleLen))
			return B_IO_ERROR;
		if (titleLen > 0 && dest->Write(chartTitle, titleLen) != titleLen)
			return B_IO_ERROR;
	}

	return B_OK;
}

// Same idea as WriteASCDWithChartForTest above, but for a cell comment
// (100% XLSX standard compatibility, Tier 2): comments sit much
// earlier in the real WriteASCD section order (right after
// AutoFilter, well before charts-type/print-area/named-range) -- only
// the sections up to and including comments need a real (if empty)
// entry, everything after can simply be omitted, relying on the same
// EOF-tolerant reading WriteASCDForTest above already depends on for
// every trailing section.
static status_t WriteASCDWithCommentForTest(CContainer* doc, const char* ref, const char* text,
	BPositionIO* dest)
{
	status_t err = WriteASCDForTest(doc, dest);
	if (err != B_OK)
		return err;

	// Grafici incorporati: chartCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// colWidths, cellColors, columnColors, rowHeights: quattro conteggi a zero.
	for (int i = 0; i < 4; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Blocca riquadri: due int32, sempre presenti.
	{
		int32 fr = 0, fc = 0;
		if (dest->Write(&fr, sizeof(fr)) != (ssize_t)sizeof(fr)
			|| dest->Write(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
			return B_IO_ERROR;
	}
	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto conteggi a zero.
	for (int i = 0; i < 8; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Visibilita' griglia: un byte, sempre presente.
	{
		uint8 sg = 1;
		if (dest->Write(&sg, sizeof(sg)) != (ssize_t)sizeof(sg))
			return B_IO_ERROR;
	}
	// Colore linguetta foglio: un byte "has" + 3 byte rgb, sempre presenti.
	{
		uint8 has = 0;
		uint8 rgb[3] = { 0, 0, 0 };
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
			return B_IO_ERROR;
	}
	// Righe nascoste: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// AutoFilter: un byte "has" + 4 int16, sempre presenti.
	{
		uint8 has = 0;
		int16 z16 = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16))
			return B_IO_ERROR;
	}
	// Commenti: un record vero.
	{
		cell c;
		c.Set(ref);
		int32 commentCount = 1;
		int16 row = c.v, col = c.h;
		int32 len = (int32)strlen(text);
		if (dest->Write(&commentCount, sizeof(commentCount)) != (ssize_t)sizeof(commentCount)
			|| dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
			|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
			|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_IO_ERROR;
		if (len > 0 && dest->Write(text, len) != len)
			return B_IO_ERROR;
	}

	return B_OK;
}

// Same idea as WriteASCDWithCommentForTest above, but for a hyperlink
// instead (100% XLSX standard compatibility, Tier 2): the hyperlinks
// section comes right after comments, so a real comment count of zero
// is still needed in between.
static status_t WriteASCDWithHyperlinkForTest(CContainer* doc, const char* ref, const char* url,
	BPositionIO* dest)
{
	status_t err = WriteASCDForTest(doc, dest);
	if (err != B_OK)
		return err;

	// Grafici incorporati: chartCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// colWidths, cellColors, columnColors, rowHeights: quattro conteggi a zero.
	for (int i = 0; i < 4; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Blocca riquadri: due int32, sempre presenti.
	{
		int32 fr = 0, fc = 0;
		if (dest->Write(&fr, sizeof(fr)) != (ssize_t)sizeof(fr)
			|| dest->Write(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
			return B_IO_ERROR;
	}
	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto conteggi a zero.
	for (int i = 0; i < 8; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Visibilita' griglia: un byte, sempre presente.
	{
		uint8 sg = 1;
		if (dest->Write(&sg, sizeof(sg)) != (ssize_t)sizeof(sg))
			return B_IO_ERROR;
	}
	// Colore linguetta foglio: un byte "has" + 3 byte rgb, sempre presenti.
	{
		uint8 has = 0;
		uint8 rgb[3] = { 0, 0, 0 };
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
			return B_IO_ERROR;
	}
	// Righe nascoste: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// AutoFilter: un byte "has" + 4 int16, sempre presenti.
	{
		uint8 has = 0;
		int16 z16 = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16))
			return B_IO_ERROR;
	}
	// Commenti: nessuno.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Collegamenti ipertestuali: un record vero.
	{
		cell c;
		c.Set(ref);
		int32 linkCount = 1;
		int16 row = c.v, col = c.h;
		int32 len = (int32)strlen(url);
		if (dest->Write(&linkCount, sizeof(linkCount)) != (ssize_t)sizeof(linkCount)
			|| dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
			|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
			|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_IO_ERROR;
		if (len > 0 && dest->Write(url, len) != len)
			return B_IO_ERROR;
	}

	return B_OK;
}

// Same idea as WriteASCDWithHyperlinkForTest above, but for a data
// validation rule instead (100% XLSX standard compatibility, Tier 2):
// the validation section comes after hyperlinks, chart-type, and
// border-color (all zero here), see the section order in WriteASCD.
// "type" is 1 for eListValidation, 2 for eNumberRangeValidation (see
// ValidationType in Container.h) -- only "list" ever writes to
// "listText", only "range" ever writes real min/max.
// Same idea as WriteASCDWithHyperlinkForTest above, but for a border
// color instead (100% XLSX standard compatibility, Tier 2 -- border
// color, the fifth item): the border-color section comes right after
// chart-type, before data validation, see WriteASCDWithValidationForTest
// below for the identical section order up to that point.
static status_t WriteASCDWithBorderColorForTest(CContainer* doc, const char* ref, rgb_color color,
	BPositionIO* dest)
{
	status_t err = WriteASCDForTest(doc, dest);
	if (err != B_OK)
		return err;

	// Grafici incorporati: chartCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// colWidths, cellColors, columnColors, rowHeights: quattro conteggi a zero.
	for (int i = 0; i < 4; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Blocca riquadri: due int32, sempre presenti.
	{
		int32 fr = 0, fc = 0;
		if (dest->Write(&fr, sizeof(fr)) != (ssize_t)sizeof(fr)
			|| dest->Write(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
			return B_IO_ERROR;
	}
	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto conteggi a zero.
	for (int i = 0; i < 8; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Visibilita' griglia: un byte, sempre presente.
	{
		uint8 sg = 1;
		if (dest->Write(&sg, sizeof(sg)) != (ssize_t)sizeof(sg))
			return B_IO_ERROR;
	}
	// Colore linguetta foglio: un byte "has" + 3 byte rgb, sempre presenti.
	{
		uint8 has = 0;
		uint8 rgb[3] = { 0, 0, 0 };
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
			return B_IO_ERROR;
	}
	// Righe nascoste: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// AutoFilter: un byte "has" + 4 int16, sempre presenti.
	{
		uint8 has = 0;
		int16 z16 = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16))
			return B_IO_ERROR;
	}
	// Commenti, collegamenti ipertestuali: nessuno.
	for (int i = 0; i < 2; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Tipo di grafico incorporato: chartTypeCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Colore del bordo di cella: un record vero.
	{
		cell c;
		c.Set(ref);
		int32 colorCount = 1;
		int16 row = c.v, col = c.h;
		if (dest->Write(&colorCount, sizeof(colorCount)) != (ssize_t)sizeof(colorCount)
			|| dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
			|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
			|| dest->Write(&color, sizeof(color)) != (ssize_t)sizeof(color))
			return B_IO_ERROR;
	}

	return B_OK;
}

static status_t WriteASCDWithValidationForTest(CContainer* doc, const char* ref, int8 type,
	const char* listText, double min, double max, BPositionIO* dest)
{
	status_t err = WriteASCDForTest(doc, dest);
	if (err != B_OK)
		return err;

	// Grafici incorporati: chartCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// colWidths, cellColors, columnColors, rowHeights: quattro conteggi a zero.
	for (int i = 0; i < 4; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Blocca riquadri: due int32, sempre presenti.
	{
		int32 fr = 0, fc = 0;
		if (dest->Write(&fr, sizeof(fr)) != (ssize_t)sizeof(fr)
			|| dest->Write(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
			return B_IO_ERROR;
	}
	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto conteggi a zero.
	for (int i = 0; i < 8; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Visibilita' griglia: un byte, sempre presente.
	{
		uint8 sg = 1;
		if (dest->Write(&sg, sizeof(sg)) != (ssize_t)sizeof(sg))
			return B_IO_ERROR;
	}
	// Colore linguetta foglio: un byte "has" + 3 byte rgb, sempre presenti.
	{
		uint8 has = 0;
		uint8 rgb[3] = { 0, 0, 0 };
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
			return B_IO_ERROR;
	}
	// Righe nascoste: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// AutoFilter: un byte "has" + 4 int16, sempre presenti.
	{
		uint8 has = 0;
		int16 z16 = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16))
			return B_IO_ERROR;
	}
	// Commenti, collegamenti ipertestuali: nessuno.
	for (int i = 0; i < 2; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Tipo di grafico incorporato: chartTypeCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Colore del bordo di cella: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Convalida dati: un record vero.
	{
		cell c;
		c.Set(ref);
		int32 validationCount = 1;
		int16 row = c.v, col = c.h;
		int32 len = (int32)strlen(listText);
		if (dest->Write(&validationCount, sizeof(validationCount)) != (ssize_t)sizeof(validationCount)
			|| dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
			|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
			|| dest->Write(&type, sizeof(type)) != (ssize_t)sizeof(type)
			|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_IO_ERROR;
		if (len > 0 && dest->Write(listText, len) != len)
			return B_IO_ERROR;
		if (dest->Write(&min, sizeof(min)) != (ssize_t)sizeof(min)
			|| dest->Write(&max, sizeof(max)) != (ssize_t)sizeof(max))
			return B_IO_ERROR;
	}

	return B_OK;
}

// Same idea as WriteASCDWithChartForTest above, but for a named range
// instead of a chart: real named-range data has to go at the very END
// of the format (see the comment on the same section in WriteASCD,
// XlsxTranslator.cpp), so every OTHER trailing section in between has
// to be written too, all empty -- can't just append after
// WriteASCDForTest like the chart helper does, since that would put
// the real data right after the cells instead of at the end.
static status_t WriteASCDWithNameForTest(CContainer* doc, BPositionIO* dest)
{
	status_t err = WriteASCDForTest(doc, dest);
	if (err != B_OK)
		return err;

	// Grafici incorporati: chartCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// colWidths, cellColors, columnColors, rowHeights: quattro conteggi a zero.
	for (int i = 0; i < 4; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Blocca riquadri: due int32, sempre presenti.
	{
		int32 fr = 0, fc = 0;
		if (dest->Write(&fr, sizeof(fr)) != (ssize_t)sizeof(fr)
			|| dest->Write(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
			return B_IO_ERROR;
	}

	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto conteggi a zero.
	for (int i = 0; i < 8; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Visibilita' griglia: un byte, sempre presente.
	{
		uint8 sg = 1;
		if (dest->Write(&sg, sizeof(sg)) != (ssize_t)sizeof(sg))
			return B_IO_ERROR;
	}

	// Colore linguetta foglio: un byte "has" + 3 byte rgb, sempre presenti.
	{
		uint8 has = 0;
		uint8 rgb[3] = { 0, 0, 0 };
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
			return B_IO_ERROR;
	}

	// Righe nascoste: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// AutoFilter: un byte "has" + 4 int16, sempre presenti.
	{
		uint8 has = 0;
		int16 z16 = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16))
			return B_IO_ERROR;
	}

	// commenti, collegamenti ipertestuali: due conteggi a zero.
	for (int i = 0; i < 2; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Tipo di grafico incorporato: chartTypeCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Colore del bordo di cella: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Convalida dati, formattazione condizionale, tabelle strutturate:
	// tre conteggi a zero.
	for (int i = 0; i < 3; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Titolo di grafico incorporato: chartTitleCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Area di stampa: un byte "has" + 4 int16, sempre presenti.
	{
		uint8 has = 0;
		int16 z16 = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16)
			|| dest->Write(&z16, sizeof(z16)) != (ssize_t)sizeof(z16))
			return B_IO_ERROR;
	}

	// Margini/scala di "Imposta pagina": un byte "has" + quattro
	// margini (double), la modalita' di scala (int32) e la percentuale
	// (double), sempre presenti.
	{
		uint8 has = 0;
		double zD = 0;
		int32 zeroMode = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&zD, sizeof(zD)) != (ssize_t)sizeof(zD)
			|| dest->Write(&zD, sizeof(zD)) != (ssize_t)sizeof(zD)
			|| dest->Write(&zD, sizeof(zD)) != (ssize_t)sizeof(zD)
			|| dest->Write(&zD, sizeof(zD)) != (ssize_t)sizeof(zD)
			|| dest->Write(&zeroMode, sizeof(zeroMode)) != (ssize_t)sizeof(zeroMode)
			|| dest->Write(&zD, sizeof(zD)) != (ssize_t)sizeof(zD))
			return B_IO_ERROR;
	}

	// Progetto VBA: un byte "has"=0, nient'altro.
	{
		uint8 has = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has))
			return B_IO_ERROR;
	}

	// Celle sbloccate: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Protezione foglio: un byte a zero.
	{
		uint8 protectedByte = 0;
		if (dest->Write(&protectedByte, sizeof(protectedByte)) != (ssize_t)sizeof(protectedByte))
			return B_IO_ERROR;
	}

	// Intervalli con nome, ULTIMA sezione del formato: i dati veri,
	// stesso motivo di questa intera funzione.
	{
		CNameTable* names = doc->GetNameTable();
		int32 nameCount = names ? (int32)names->size() : 0;
		if (dest->Write(&nameCount, sizeof(nameCount)) != (ssize_t)sizeof(nameCount))
			return B_IO_ERROR;
		if (names)
		{
			for (CNameTable::const_iterator it = names->begin(); it != names->end(); ++it)
			{
				const char* nameStr = (const char*)it->first;
				int32 nameLen = (int32)strlen(nameStr);
				const range& r = it->second;
				int16 top = r.top, left = r.left, bottom = r.bottom, right = r.right;
				if (dest->Write(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
					return B_IO_ERROR;
				if (nameLen > 0 && dest->Write(nameStr, nameLen) != nameLen)
					return B_IO_ERROR;
				if (dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
					|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right))
					return B_IO_ERROR;
			}
		}
	}

	return B_OK;
}

// Translate(XLSX -> nativo) produce ora sempre una cartella di lavoro
// multi-foglio ("ASCB", Fase 9), anche per un file XLSX con un solo
// foglio come tests/sample.xlsx: salta l'header e il nome del primo
// foglio per arrivare al blocco "ASCD" vero e proprio -- i controlli
// qui sotto verificano solo il contenuto delle celle del primo foglio,
// non l'incapsulamento multi-foglio in se' (gia' verificato a parte in
// ui/tests/test_ascd_book.cpp/test_multisheet.cpp). Riconosce anche un
// vecchio "ASCD" nudo, per restare valido se il formato dovesse mai
// tornare a produrne uno (difensivo, non il caso atteso oggi).
static bool UnwrapFirstSheet(const unsigned char* data, size_t len,
	const unsigned char** outAscd, size_t* outLen)
{
	// "ASC2" (Fase 32b, vedi il commento su kASCDBook2Magic in
	// XlsxTranslator.cpp): come "ASCB" sotto, ma ogni blocco per foglio
	// e' preceduto dalla propria lunghezza in byte -- qui basta leggerla
	// per sapere ESATTAMENTE dove finisce il primo foglio, invece di
	// assumere "tutto cio' che resta nel buffer" (falso non appena il
	// buffer contiene piu' di un foglio).
	if (len >= 4 && memcmp(data, "ASC2", 4) == 0)
	{
		if (len < 8)
			return false;
		int32 sheetCount;
		memcpy(&sheetCount, data + 4, 4);
		if (sheetCount < 1)
			return false;

		size_t pos = 8;
		if (pos + 4 > len)
			return false;
		int32 nameLen;
		memcpy(&nameLen, data + pos, 4);
		pos += 4;
		if (nameLen < 0 || pos + (size_t)nameLen > len)
			return false;
		pos += nameLen;

		if (pos + 4 > len)
			return false;
		int32 blockLen;
		memcpy(&blockLen, data + pos, 4);
		pos += 4;
		if (blockLen < 0 || pos + (size_t)blockLen > len)
			return false;

		*outAscd = data + pos;
		*outLen = blockLen;
		return true;
	}

	// "ASCB": formato LEGACY (congelato, mai piu' scritto da
	// WriteASCDBook -- vedi il commento su kASCDBookMagic li'), nessun
	// confine di lunghezza esplicito per blocco: "tutto cio' che resta
	// nel buffer" e' corretto SOLO perche' questo formato non viene piu'
	// prodotto da un vero export a piu' di un foglio in questi test.
	if (len >= 4 && memcmp(data, "ASCB", 4) == 0)
	{
		if (len < 8)
			return false;
		int32 sheetCount;
		memcpy(&sheetCount, data + 4, 4);
		if (sheetCount < 1)
			return false;

		size_t pos = 8;
		if (pos + 4 > len)
			return false;
		int32 nameLen;
		memcpy(&nameLen, data + pos, 4);
		pos += 4;
		if (nameLen < 0 || pos + (size_t)nameLen > len)
			return false;
		pos += nameLen;

		*outAscd = data + pos;
		*outLen = len - pos;
		return true;
	}

	if (len >= 4 && memcmp(data, "ASCD", 4) == 0)
	{
		*outAscd = data;
		*outLen = len;
		return true;
	}

	return false;
}

// Rilegge il blocca-riquadri da un blocco ASCD (100% XLSX standard
// compatibility, Tier 2): cammina fino alla sezione freeze (subito
// dopo rowHeights, PRIMA di font/allineamento/ecc. -- vedi il
// commento su WriteASCDWithFreezeForTest sopra), assumendo un
// documento di prova minimo con chart/colWidths/cellColors/
// columnColors/rowHeights tutti vuoti. Usato SOLO su output prodotto
// dal vero WriteASCD del translator (mai direttamente sull'ASCD di
// prova costruito a mano), quindi ogni cella ha il byte "kind"
// (formato versione 2, "9 + len" -- stesso principio di
// ReadFirstChartForTest/ReadFirstCommentForTest sopra).
static bool ReadFreezeFromAscdForTest(const unsigned char* ascdData, size_t ascdLen,
	int32* outFrozenRows, int32* outFrozenCols)
{
	if (ascdLen < 12 || memcmp(ascdData, "ASCD", 4) != 0)
		return false;

	int32 cellCount;
	memcpy(&cellCount, ascdData + 8, 4);

	size_t pos = 12;
	for (int32 i = 0; i < cellCount; i++)
	{
		if (pos + 9 > ascdLen)
			return false;
		int32 len;
		memcpy(&len, ascdData + pos + 4, 4);
		pos += 9 + len;
	}

	// Grafici incorporati, colWidths, cellColors, columnColors,
	// rowHeights: cinque contatori (0 in questo documento di prova).
	for (int s = 0; s < 5; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Blocca riquadri: i dati veri, due int32 senza contatore davanti.
	if (pos + 8 > ascdLen) return false;
	memcpy(outFrozenRows, ascdData + pos, 4); pos += 4;
	memcpy(outFrozenCols, ascdData + pos, 4); pos += 4;

	return true;
}

// Rilegge dataRange/tipo/titolo del PRIMO grafico incorporato da un
// blocco ASCD (Fase 25, importazione dei grafici): cammina esattamente
// lo stesso formato scritto da WriteASCD in XlsxTranslator.cpp,
// sezione per sezione fino al titolo del grafico incluso (l'ultima) --
// stesso principio EOF-tollerante gia' verificato altrove in questo
// file (vedi le sezioni "restano allineate" per sample_table.xlsx/
// sample_condformat.xlsx piu' sotto). Presuppone un documento di prova
// MINIMO (nessuno stile/font/bordo/immagine espliciti), quindi ogni
// sezione intermedia e' sempre vuota -- non un parser ASCD generico.
static bool ReadFirstChartForTest(const unsigned char* ascdData, size_t ascdLen,
	int16* outLeft, int16* outTop, int16* outRight, int16* outBottom,
	int8* outType, std::string* outTitle, float outFrame[4] = NULL)
{
	if (ascdLen < 12 || memcmp(ascdData, "ASCD", 4) != 0)
		return false;

	int32 cellCount;
	memcpy(&cellCount, ascdData + 8, 4);

	size_t pos = 12;
	for (int32 i = 0; i < cellCount; i++)
	{
		if (pos + 9 > ascdLen)
			return false;
		int32 len;
		memcpy(&len, ascdData + pos + 4, 4);
		pos += 9 + len;
	}

	if (pos + 4 > ascdLen)
		return false;
	int32 chartCount;
	memcpy(&chartCount, ascdData + pos, 4); pos += 4;
	if (chartCount != 1 || pos + 24 > ascdLen)
		return false;
	memcpy(outLeft, ascdData + pos, 2); pos += 2;
	memcpy(outTop, ascdData + pos, 2); pos += 2;
	memcpy(outRight, ascdData + pos, 2); pos += 2;
	memcpy(outBottom, ascdData + pos, 2); pos += 2;
	if (outFrame)
		memcpy(outFrame, ascdData + pos, 16);
	pos += 16; // frame (4 float)

	// colWidths, cellColors, columnColors, rowHeights: quattro contatori.
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Blocca riquadri: due interi fissi (non un contatore).
	if (pos + 8 > ascdLen) return false;
	pos += 8;

	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto contatori.
	for (int s = 0; s < 8; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Visibilita' griglia: un byte fisso.
	if (pos + 1 > ascdLen) return false;
	pos += 1;

	// Colore della linguetta: 4 byte fissi.
	if (pos + 4 > ascdLen) return false;
	pos += 4;

	// Righe nascoste: un contatore.
	if (pos + 4 > ascdLen) return false;
	{
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// AutoFilter: 9 byte fissi.
	if (pos + 9 > ascdLen) return false;
	pos += 9;

	// Commenti, collegamenti: due contatori.
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Tipo di grafico: un contatore + un byte per grafico.
	if (pos + 4 > ascdLen) return false;
	{
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 1) return false;
	}
	if (pos + 1 > ascdLen) return false;
	memcpy(outType, ascdData + pos, 1); pos += 1;

	// Colore del bordo, convalida dati, formattazione condizionale,
	// tabelle: quattro contatori (tutti a zero in questo documento di
	// prova minimo -- nessuno stile/regola/tabella).
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Titolo di grafico: un contatore + un titolo length-prefixed per
	// grafico, l'ULTIMA sezione del formato.
	if (pos + 4 > ascdLen) return false;
	{
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 1) return false;
	}
	if (pos + 4 > ascdLen) return false;
	int32 titleLen;
	memcpy(&titleLen, ascdData + pos, 4); pos += 4;
	if (titleLen < 0 || pos + (size_t)titleLen > ascdLen) return false;
	outTitle->assign((const char*)ascdData + pos, titleLen);

	return true;
}

// Same idea as ReadFirstChartForTest above, but for the comments
// section instead (100% XLSX standard compatibility, Tier 2): stops
// right after reading the first comment instead of continuing to the
// chart-type/title sections, since a fixture built for this test has
// no chart. Assumes exactly one comment, matching every caller today.
static bool ReadFirstCommentFromAscdForTest(const unsigned char* ascdData, size_t ascdLen,
	cell* outCell, std::string* outText)
{
	if (ascdLen < 12 || memcmp(ascdData, "ASCD", 4) != 0)
		return false;

	int32 cellCount;
	memcpy(&cellCount, ascdData + 8, 4);

	size_t pos = 12;
	for (int32 i = 0; i < cellCount; i++)
	{
		if (pos + 9 > ascdLen)
			return false;
		int32 len;
		memcpy(&len, ascdData + pos + 4, 4);
		pos += 9 + len;
	}

	// Grafici incorporati: un contatore (0 in questo documento di prova).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }

	// colWidths, cellColors, columnColors, rowHeights: quattro contatori.
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Blocca riquadri: due interi fissi.
	if (pos + 8 > ascdLen) return false;
	pos += 8;

	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto contatori.
	for (int s = 0; s < 8; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Visibilita' griglia: un byte fisso.
	if (pos + 1 > ascdLen) return false;
	pos += 1;

	// Colore della linguetta: 4 byte fissi.
	if (pos + 4 > ascdLen) return false;
	pos += 4;

	// Righe nascoste: un contatore.
	if (pos + 4 > ascdLen) return false;
	{
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// AutoFilter: 9 byte fissi.
	if (pos + 9 > ascdLen) return false;
	pos += 9;

	// Commenti: i dati veri, primo (e unico atteso) record.
	if (pos + 4 > ascdLen) return false;
	int32 commentCount;
	memcpy(&commentCount, ascdData + pos, 4); pos += 4;
	if (commentCount != 1) return false;

	if (pos + 8 > ascdLen) return false;
	int16 row, col;
	int32 textLen;
	memcpy(&row, ascdData + pos, 2); pos += 2;
	memcpy(&col, ascdData + pos, 2); pos += 2;
	memcpy(&textLen, ascdData + pos, 4); pos += 4;
	if (textLen < 0 || pos + (size_t)textLen > ascdLen) return false;

	outCell->Set(col, row);
	outText->assign((const char*)ascdData + pos, textLen);

	return true;
}

// Same idea as ReadFirstCommentFromAscdForTest above, but for the
// hyperlinks section right after it (100% XLSX standard compatibility,
// Tier 2): assumes no comments and exactly one hyperlink, matching
// every caller today.
static bool ReadFirstHyperlinkFromAscdForTest(const unsigned char* ascdData, size_t ascdLen,
	cell* outCell, std::string* outUrl)
{
	if (ascdLen < 12 || memcmp(ascdData, "ASCD", 4) != 0)
		return false;

	int32 cellCount;
	memcpy(&cellCount, ascdData + 8, 4);

	size_t pos = 12;
	for (int32 i = 0; i < cellCount; i++)
	{
		if (pos + 9 > ascdLen)
			return false;
		int32 len;
		memcpy(&len, ascdData + pos + 4, 4);
		pos += 9 + len;
	}

	// Grafici incorporati: un contatore (0 in questo documento di prova).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }

	// colWidths, cellColors, columnColors, rowHeights: quattro contatori.
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Blocca riquadri: due interi fissi.
	if (pos + 8 > ascdLen) return false;
	pos += 8;

	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto contatori.
	for (int s = 0; s < 8; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Visibilita' griglia: un byte fisso.
	if (pos + 1 > ascdLen) return false;
	pos += 1;

	// Colore della linguetta: 4 byte fissi.
	if (pos + 4 > ascdLen) return false;
	pos += 4;

	// Righe nascoste: un contatore.
	if (pos + 4 > ascdLen) return false;
	{
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// AutoFilter: 9 byte fissi.
	if (pos + 9 > ascdLen) return false;
	pos += 9;

	// Commenti: un contatore (0 in questo documento di prova).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }

	// Collegamenti ipertestuali: i dati veri, primo (e unico atteso) record.
	if (pos + 4 > ascdLen) return false;
	int32 linkCount;
	memcpy(&linkCount, ascdData + pos, 4); pos += 4;
	if (linkCount != 1) return false;

	if (pos + 8 > ascdLen) return false;
	int16 row, col;
	int32 urlLen;
	memcpy(&row, ascdData + pos, 2); pos += 2;
	memcpy(&col, ascdData + pos, 2); pos += 2;
	memcpy(&urlLen, ascdData + pos, 4); pos += 4;
	if (urlLen < 0 || pos + (size_t)urlLen > ascdLen) return false;

	outCell->Set(col, row);
	outUrl->assign((const char*)ascdData + pos, urlLen);

	return true;
}

// Same idea as ReadFirstHyperlinkFromAscdForTest above, but for the
// border-color section (100% XLSX standard compatibility, Tier 2 --
// border color, the fifth item): unlike every other fixture in this
// file, the test cell here DOES have a non-default border (a real
// <color> requires a real <left style="thin"> etc. side to attach to,
// see ParseStyles), so the "bordi di cella" (thickness) section --
// distinct from "colore del bordo" here -- has exactly ONE real entry
// too, not zero like every other section walked past.
static bool ReadFirstBorderColorFromAscdForTest(const unsigned char* ascdData, size_t ascdLen,
	cell* outCell, rgb_color* outColor)
{
	if (ascdLen < 12 || memcmp(ascdData, "ASCD", 4) != 0)
		return false;

	int32 cellCount;
	memcpy(&cellCount, ascdData + 8, 4);

	size_t pos = 12;
	for (int32 i = 0; i < cellCount; i++)
	{
		if (pos + 9 > ascdLen)
			return false;
		int32 len;
		memcpy(&len, ascdData + pos + 4, 4);
		pos += 9 + len;
	}

	// Grafici incorporati, colWidths, cellColors, columnColors,
	// rowHeights: cinque contatori (0 in questo documento di prova).
	for (int s = 0; s < 5; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Blocca riquadri: due interi fissi.
	if (pos + 8 > ascdLen) return false;
	pos += 8;

	// fonts, alignment: due contatori (0).
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Bordi di cella (SPESSORE per lato, sezione diversa dal colore
	// letto piu' sotto): zero o piu' record, a seconda che il
	// documento di prova abbia impostato anche lo spessore (import da
	// un vero <border style="thin">) o solo il colore diretto (round-
	// trip ASCD -> ASCD, vedi il commento sopra la funzione) -- non
	// assunto fisso, solo saltato -- (int16 row, col, 4 byte di
	// spessore per lato per record).
	if (pos + 4 > ascdLen) return false;
	{
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n < 0) return false;
		size_t skip = (size_t)n * 8;
		if (pos + skip > ascdLen) return false;
		pos += skip;
	}

	// numberFormat, underline, wrapText, mergedCells, images: cinque contatori (0).
	for (int s = 0; s < 5; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Visibilita' griglia: un byte fisso.
	if (pos + 1 > ascdLen) return false;
	pos += 1;

	// Colore della linguetta: 4 byte fissi.
	if (pos + 4 > ascdLen) return false;
	pos += 4;

	// Righe nascoste: un contatore.
	if (pos + 4 > ascdLen) return false;
	{
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// AutoFilter: 9 byte fissi.
	if (pos + 9 > ascdLen) return false;
	pos += 9;

	// Commenti, collegamenti ipertestuali: due contatori (0).
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Tipo di grafico: un contatore (0).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }

	// Colore del bordo: i dati veri, primo (e unico atteso) record.
	if (pos + 4 > ascdLen) return false;
	int32 colorCount;
	memcpy(&colorCount, ascdData + pos, 4); pos += 4;
	if (colorCount != 1) return false;

	if (pos + 8 > ascdLen) return false;
	int16 row, col;
	memcpy(&row, ascdData + pos, 2); pos += 2;
	memcpy(&col, ascdData + pos, 2); pos += 2;
	if (pos + sizeof(rgb_color) > ascdLen) return false;
	memcpy(outColor, ascdData + pos, sizeof(rgb_color));
	pos += sizeof(rgb_color);

	outCell->Set(col, row);
	return true;
}

// Same idea as ReadFirstHyperlinkFromAscdForTest above, but for the
// data validation section (100% XLSX standard compatibility, Tier 2):
// continues past hyperlinks, chart-type, and border-color (assumed
// zero) into the real validation record.
static bool ReadFirstValidationFromAscdForTest(const unsigned char* ascdData, size_t ascdLen,
	cell* outCell, int8* outType, std::string* outList, double* outMin, double* outMax)
{
	if (ascdLen < 12 || memcmp(ascdData, "ASCD", 4) != 0)
		return false;

	int32 cellCount;
	memcpy(&cellCount, ascdData + 8, 4);

	size_t pos = 12;
	for (int32 i = 0; i < cellCount; i++)
	{
		if (pos + 9 > ascdLen)
			return false;
		int32 len;
		memcpy(&len, ascdData + pos + 4, 4);
		pos += 9 + len;
	}

	// Grafici incorporati: un contatore (0 in questo documento di prova).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }

	// colWidths, cellColors, columnColors, rowHeights: quattro contatori.
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Blocca riquadri: due interi fissi.
	if (pos + 8 > ascdLen) return false;
	pos += 8;

	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto contatori.
	for (int s = 0; s < 8; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Visibilita' griglia: un byte fisso.
	if (pos + 1 > ascdLen) return false;
	pos += 1;

	// Colore della linguetta: 4 byte fissi.
	if (pos + 4 > ascdLen) return false;
	pos += 4;

	// Righe nascoste: un contatore.
	if (pos + 4 > ascdLen) return false;
	{
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// AutoFilter: 9 byte fissi.
	if (pos + 9 > ascdLen) return false;
	pos += 9;

	// Commenti, collegamenti ipertestuali: due contatori (0 in questo
	// documento di prova).
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	// Tipo di grafico: un contatore (0 in questo documento di prova).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }

	// Colore del bordo: un contatore (0 in questo documento di prova).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }

	// Convalida dati: i dati veri, primo (e unico atteso) record.
	if (pos + 4 > ascdLen) return false;
	int32 validationCount;
	memcpy(&validationCount, ascdData + pos, 4); pos += 4;
	if (validationCount != 1) return false;

	if (pos + 9 > ascdLen) return false;
	int16 row, col;
	int8 type;
	int32 listLen;
	memcpy(&row, ascdData + pos, 2); pos += 2;
	memcpy(&col, ascdData + pos, 2); pos += 2;
	memcpy(&type, ascdData + pos, 1); pos += 1;
	memcpy(&listLen, ascdData + pos, 4); pos += 4;
	if (listLen < 0 || pos + (size_t)listLen > ascdLen) return false;

	std::string list((const char*)ascdData + pos, listLen);
	pos += listLen;

	if (pos + 16 > ascdLen) return false;
	double min, max;
	memcpy(&min, ascdData + pos, 8); pos += 8;
	memcpy(&max, ascdData + pos, 8); pos += 8;

	outCell->Set(col, row);
	*outType = type;
	*outList = list;
	*outMin = min;
	*outMax = max;

	return true;
}

// After a per-cell reconstruction loop has advanced "pos" past every
// cell of an ASCD block, walks every EOF-tolerant trailing section in
// the exact order WriteASCD writes them (XlsxTranslator.cpp) up to
// and including named ranges (the last section), applying any name
// found to "doc" -- same principle as ReadFirstChartForTest above,
// but for the section at the very end instead of the one in the
// middle. Assumes every fixture calling this has no chart and no VBA
// project (both asserted, not just skipped), matching every actual
// caller today; a real caller with either would need its own walk,
// same as ReadFirstChartForTest is its own dedicated walk for charts.
static bool ApplyNamesFromAscdForTest(const unsigned char* data, size_t len, size_t pos, CContainer* doc)
{
	if (pos + 4 > len) return false;
	int32 chartCount;
	memcpy(&chartCount, data + pos, 4); pos += 4;
	if (chartCount != 0) return false;

	// colWidths, cellColors, columnColors, rowHeights: quattro conteggi.
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Blocca riquadri: due interi fissi.
	if (pos + 8 > len) return false;
	pos += 8;
	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto conteggi.
	for (int s = 0; s < 8; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Visibilita' griglia: un byte.
	if (pos + 1 > len) return false;
	pos += 1;
	// Colore linguetta: 4 byte fissi.
	if (pos + 4 > len) return false;
	pos += 4;
	// Righe nascoste: un conteggio.
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	// AutoFilter: 9 byte fissi.
	if (pos + 9 > len) return false;
	pos += 9;
	// Commenti, collegamenti: due conteggi.
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Tipo di grafico: un conteggio (0, nessun grafico in questi fixture).
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	// Colore bordo, convalida dati, formattazione condizionale, tabelle: quattro conteggi.
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Titolo di grafico: un conteggio (0).
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	// Area di stampa: 1 byte "has" + 4 int16 fissi (9 byte totali).
	if (pos + 9 > len) return false;
	pos += 9;
	// Margini/scala: 1 byte "has" + 4 double + 1 int32 + 1 double (45 byte totali).
	if (pos + 45 > len) return false;
	pos += 45;
	// Progetto VBA: 1 byte "has", assunto 0 (nessun progetto in questi fixture).
	if (pos + 1 > len) return false;
	if (data[pos] != 0) return false;
	pos += 1;
	// Celle sbloccate: un conteggio.
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	// Protezione foglio: un byte.
	if (pos + 1 > len) return false;
	pos += 1;

	// Intervalli con nome, ULTIMA sezione: i dati veri.
	if (pos + 4 > len) return false;
	int32 nameCount;
	memcpy(&nameCount, data + pos, 4); pos += 4;
	for (int32 i = 0; i < nameCount; i++)
	{
		if (pos + 4 > len) return false;
		int32 nameLen;
		memcpy(&nameLen, data + pos, 4); pos += 4;
		if (nameLen < 0 || pos + (size_t)nameLen > len) return false;
		std::string nameStr((const char*)data + pos, nameLen);
		pos += nameLen;
		if (pos + 8 > len) return false;
		int16 top, left, bottom, right;
		memcpy(&top, data + pos, 2); pos += 2;
		memcpy(&left, data + pos, 2); pos += 2;
		memcpy(&bottom, data + pos, 2); pos += 2;
		memcpy(&right, data + pos, 2); pos += 2;
		(*doc->GetOrCreateNameTable())[CName(nameStr.c_str())] = range(left, top, right, bottom);
	}
	return true;
}

int main()
{
	// Serve da Fase 12 (import grassetto/corsivo): gFontSizeTable::
	// GetFontID risolve un BFont reale (CFontStyle::Locate ->
	// BFont::GetEscapements), una chiamata che senza una BApplication
	// viva si blocca in attesa di una risposta dall'app_server che non
	// arriva mai -- stesso motivo gia' noto in ui/tests/test_ascd_io.cpp
	// e test_persistence.cpp per lo stesso genere di chiamate.
	BApplication app("application/x-vnd.Atomo-TestXlsxTranslator");

	// Senza questo, GetFunctionNr tratta OGNI nome di funzione (IF,
	// VLOOKUP, IFERROR, ecc.) come identificatore sconosciuto
	// (gFuncCount resta 0): una formula con funzione con nome
	// importata da un file XLSX reale ripiegherebbe sempre sul testo
	// grezzo invece del valore calcolato, in questo test come nella
	// vera app senza App::ReadyToRun -- stesso principio di
	// engine/tests/named_functions_test.cpp.
	{
		BPath funcRsrcPath("tests/named_functions.rsrc");
		gAppName = funcRsrcPath;
		if (gResourceManager.SetTo(&funcRsrcPath) == B_OK)
			InitFunctions();
	}

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

	// Bug reale, crash vero di Tracker catturato in un .report: il
	// thumbnail worker chiama BTranslatorRoster::Translate() con
	// info=NULL per ogni file mentre genera le anteprime (significa
	// "identifica tu stesso il formato sorgente", documentato nel
	// Translation Kit), senza mai passare da Identify() prima --
	// "info->type" letto senza controllo faceva crashare Tracker
	// stesso, non solo Atomo123.
	xlsxFile.Seek(0, SEEK_SET);
	BMallocIO ascdOutNullInfo;
	status_t errNullInfo = translator->Translate(&xlsxFile, NULL, NULL, kAtomoNativeFormat, &ascdOutNullInfo);
	Check(errNullInfo == B_OK, "Translate con info=NULL (come fa Tracker per le anteprime) non crasha, si identifica da solo");

	// Rilegge l'ASCD prodotto per verificare i valori importati: si
	// riusa lo stesso formato del translator CSV, quindi basta
	// controllare che il testo delle celle (formula o valore
	// formattato) contenga quanto atteso.
	const unsigned char *rawData = (const unsigned char *)ascdOut.Buffer();
	size_t rawLen = ascdOut.BufferLength();

	const unsigned char *ascdData = NULL;
	size_t ascdLen = 0;
	bool unwrapped = UnwrapFirstSheet(rawData, rawLen, &ascdData, &ascdLen);
	Check(unwrapped, "l'output di Translate e' una cartella ASCB valida o un ASCD nudo");

	Check(ascdLen > 12 && memcmp(ascdData, "ASCD", 4) == 0,
		"il primo foglio ha l'intestazione ASCD attesa");

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
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
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

		// Larghezze di colonna lette da <cols> in tests/sample.xlsx:
		// colonna 1 a larghezza 20 caratteri, colonne 3-4 a larghezza 8
		// -- convertite in pixel con la formula esatta di Excel
		// (ExcelColWidthToPixels, ECMA-376 18.3.1.13, MDW=7):
		// floor(((256*w + floor(128/7)) / 256) * 7). Per w=20:
		// floor(((5120+18)/256)*7) = floor(140.49) = 140. Per w=8:
		// floor(((2048+18)/256)*7) = floor(56.49) = 56.
		// "pos" punta gia' subito dopo l'ultima cella, cioe' all'inizio
		// della sezione grafici (sempre scritta, vuota qui) seguita
		// dalla sezione larghezze di colonna.
		if (pos + 4 <= ascdLen)
		{
			int32 chartCount;
			memcpy(&chartCount, ascdData + pos, 4); pos += 4;
			Check(chartCount == 0, "nessun grafico incorporato (il translator XLSX non li gestisce)");
			pos += chartCount * (2 * 4 + 4 * 4); // salta eventuali record grafico (non attesi qui)

			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				Check(colWidthCount == 3,
					"tre colonne con larghezza esplicita (1, 3, 4 -- da min=3 max=4)");

				bool foundCol1 = false, foundCol3 = false, foundCol4 = false;
				for (int32 i = 0; i < colWidthCount && pos + 6 <= ascdLen; i++)
				{
					int16 col;
					float width;
					memcpy(&col, ascdData + pos, 2); pos += 2;
					memcpy(&width, ascdData + pos, 4); pos += 4;

					if (col == 1 && fabs(width - 140.0f) < 0.01f)
						foundCol1 = true;
					if (col == 3 && fabs(width - 56.0f) < 0.01f)
						foundCol3 = true;
					if (col == 4 && fabs(width - 56.0f) < 0.01f)
						foundCol4 = true;
				}
				Check(foundCol1, "la larghezza della colonna 1 (20 caratteri -> 140 pixel) e' importata");
				Check(foundCol3, "la larghezza della colonna 3 (8 caratteri -> 56 pixel, da min=3) e' importata");
				Check(foundCol4, "la larghezza della colonna 4 (8 caratteri -> 56 pixel, da max=4) e' importata");

				// Colori di cella: A1 (s="1" in sample.xlsx) usa fontId=1
				// (testo blu, rgb="FF0000FF") e fillId=2 (sfondo dal tema
				// -- theme="4" con tint="0.4", cioe' accent1 "4472C4"
				// schiarito con la stessa formula approssimata di
				// ApplyTint in XlsxTranslator.cpp). B1 (s="2") usa lo
				// stesso sfondo ma fontId=0 (nessun <color>, testo
				// predefinito nero) -- verifica sia la risoluzione del
				// tema con tint sia quella diretta rgb=, e che uno stile
				// senza colore testo non forzi comunque un nero
				// esplicito diverso dal predefinito.
				if (pos + 4 <= ascdLen)
				{
					int32 cellColorCount;
					memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
					Check(cellColorCount == 2,
						"due celle (A1, B1) hanno un colore esplicito (s=\"1\"/\"2\")");

					bool foundA1Color = false, foundB1Color = false;
					for (int32 i = 0; i < cellColorCount && pos + 12 <= ascdLen; i++)
					{
						int16 row, col;
						uint8 bg[4], fg[4];
						memcpy(&row, ascdData + pos, 2); pos += 2;
						memcpy(&col, ascdData + pos, 2); pos += 2;
						memcpy(bg, ascdData + pos, 4); pos += 4;
						memcpy(fg, ascdData + pos, 4); pos += 4;

						// ApplyTint(0x4472C4, 0.4) canale per canale:
						// v*0.6 + 102, arrotondato -- vedi ApplyTintToChannel.
						bool bgMatches = abs((int)bg[0] - 143) <= 1
							&& abs((int)bg[1] - 170) <= 1 && abs((int)bg[2] - 220) <= 1;

						if (row == 1 && col == 1 && bgMatches
							&& fg[0] == 0 && fg[1] == 0 && fg[2] == 255)
							foundA1Color = true;
						if (row == 1 && col == 2 && bgMatches
							&& fg[0] == 0 && fg[1] == 0 && fg[2] == 0)
							foundB1Color = true;
					}
					Check(foundA1Color,
						"A1: sfondo dal tema (accent1 schiarito) e testo blu (rgb diretto) importati");
					Check(foundB1Color,
						"B1: stesso sfondo dal tema, testo predefinito nero (il font non ne specifica uno)");

					// Sezione colori di colonna (esistente prima di
					// Fase 10/11, sample.xlsx non ne ha): saltata senza
					// verificarne il conteggio, serve solo per arrivare
					// alla posizione giusta per le sezioni sotto.
					if (pos + 4 <= ascdLen)
					{
						int32 columnColorCount;
						memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
						pos += columnColorCount * (2 + 8); // col (int16) + WriteColorEntry (8 byte)

						// Regressione reale (crash "Assert failed:
						// inIndex <= fMax" in RunArray2.cpp, riprodotto
						// aprendo un file .xlsm da 38 fogli): quando
						// SaveASCD/LoadASCD (ui/src/AscdIO.cpp) hanno
						// guadagnato le cinque sezioni sotto (Fase
						// 10/11 -- altezze di riga, Blocca riquadri,
						// font, allineamento, bordi), la copia duplicata
						// di WriteASCD in questo file non era stata
						// aggiornata: per un foglio SINGOLO lo stream
						// finiva semplicemente prima (nessun crash,
						// LoadASCD tratta un EOF pulito come "sezione
						// assente" -- perche' questo test non l'aveva
						// gia' scoperto da solo), ma in una cartella di
						// lavoro multi-foglio (WriteASCDBook, un blocco
						// ASCD dopo l'altro sullo stesso flusso) i byte
						// del foglio SUCCESSIVO venivano letti come se
						// fossero queste sezioni del foglio corrente.
						// Le cinque verifiche sotto controllano che
						// questo translator le scriva sempre, anche
						// vuote, esattamente come SaveASCD.
						// Riga 1 di sample.xlsx ha ht="30" customHeight="1"
						// (aggiunta apposta per questo test): 30 punti a
						// 96 DPI -> 30 * 4/3 = 40 pixel, stesso fattore
						// gia' usato altrove in questo progetto per
						// SheetView::kRowHeight (15pt predefiniti -> 20px).
						if (pos + 4 <= ascdLen)
						{
							int32 rowHeightCount;
							memcpy(&rowHeightCount, ascdData + pos, 4); pos += 4;
							Check(rowHeightCount == 1,
								"una riga con altezza esplicita (riga 1, ht=\"30\" customHeight=\"1\")");

							bool foundRow1Height = false;
							for (int32 i = 0; i < rowHeightCount && pos + 6 <= ascdLen; i++)
							{
								int16 row;
								float height;
								memcpy(&row, ascdData + pos, 2); pos += 2;
								memcpy(&height, ascdData + pos, 4); pos += 4;
								if (row == 1 && fabs(height - 40.0f) < 0.01f)
									foundRow1Height = true;
							}
							Check(foundRow1Height,
								"l'altezza della riga 1 (30pt -> 40px) e' importata dal file XLSX originale");
						}

						if (pos + 8 <= ascdLen)
						{
							int32 frozenRows, frozenCols;
							memcpy(&frozenRows, ascdData + pos, 4); pos += 4;
							memcpy(&frozenCols, ascdData + pos, 4); pos += 4;
							Check(frozenRows == 0 && frozenCols == 0,
								"sezione Blocca riquadri presente (nessun blocco, Fase 10)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 fontCount;
							memcpy(&fontCount, ascdData + pos, 4); pos += 4;
							Check(fontCount == 0,
								"sezione font di cella presente (vuota, Fase 10)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 alignCount;
							memcpy(&alignCount, ascdData + pos, 4); pos += 4;
							Check(alignCount == 0,
								"sezione allineamento di cella presente (vuota, Fase 10)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 borderCount;
							memcpy(&borderCount, ascdData + pos, 4); pos += 4;
							Check(borderCount == 0,
								"sezione bordi di cella presente (vuota, Fase 11)");
						}

						// Sezione formato numero (Fase 12): sample.xlsx
						// non ha nessun numFmt esplicito in styles.xml
						// (tutti gli xf hanno numFmtId="0" = General),
						// quindi 0 celle -- ma la sezione stessa deve
						// esserci (a differenza delle cinque sopra,
						// questo translator la scrive con valori REALI
						// quando presenti, non sempre vuota: qui e'
						// vuota perche' il file di prova non ha formati,
						// non perche' la sezione manchi).
						if (pos + 4 <= ascdLen)
						{
							int32 formatCount;
							memcpy(&formatCount, ascdData + pos, 4); pos += 4;
							Check(formatCount == 0,
								"sezione formato numero presente (vuota per questo file di prova, Fase 12)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 underlineCount;
							memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
							Check(underlineCount == 0,
								"sezione sottolineato presente (vuota per questo file di prova, Fase 12)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 wrapCount;
							memcpy(&wrapCount, ascdData + pos, 4); pos += 4;
							Check(wrapCount == 0,
								"sezione testo a capo presente (vuota per questo file di prova, Fase 12)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 mergeCount;
							memcpy(&mergeCount, ascdData + pos, 4); pos += 4;
							Check(mergeCount == 0,
								"sezione celle unite presente (vuota per questo file di prova, Fase 12)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 imageCount;
							memcpy(&imageCount, ascdData + pos, 4); pos += 4;
							Check(imageCount == 0,
								"sezione immagini incorporate presente (vuota per questo file di prova, Fase 12)");
						}

						// Visibilita' griglia: sample.xlsx ha
						// <sheetView showGridLines="0"/> (aggiunto
						// apposta per questo test) -- un solo byte.
						if (pos + 1 <= ascdLen)
						{
							uint8 showGrid = ascdData[pos]; pos += 1;
							Check(showGrid == 0,
								"la griglia nascosta (showGridLines=\"0\") e' importata dal file XLSX originale");
						}

						// Colore della linguetta: sample.xlsx ha
						// <sheetPr><tabColor rgb="FF00B050"/></sheetPr>
						// (aggiunto apposta per questo test, lo stesso
						// verde del file reale che ha motivato questa
						// fase) -- un byte "presente" seguito da tre
						// byte RGB, ultima sezione del formato.
						if (pos + 4 <= ascdLen)
						{
							uint8 hasTabColor = ascdData[pos]; pos += 1;
							uint8 r = ascdData[pos]; pos += 1;
							uint8 g = ascdData[pos]; pos += 1;
							uint8 b = ascdData[pos]; pos += 1;
							Check(hasTabColor == 1 && r == 0 && g == 176 && b == 80,
								"il colore verde della linguetta (tabColor rgb=\"FF00B050\") "
								"e' importato dal file XLSX originale");
						}

						// Righe nascoste: sample.xlsx ha <row r="2"
						// hidden="1"/> (aggiunta apposta per questo
						// test) -- una sola riga nascosta.
						if (pos + 4 <= ascdLen)
						{
							int32 hiddenCount;
							memcpy(&hiddenCount, ascdData + pos, 4); pos += 4;
							Check(hiddenCount == 1, "una riga nascosta (<row r=\"2\" hidden=\"1\"/>) e' importata");

							bool foundRow2Hidden = false;
							for (int32 i = 0; i < hiddenCount && pos + 2 <= ascdLen; i++)
							{
								int16 row;
								memcpy(&row, ascdData + pos, 2); pos += 2;
								if (row == 2)
									foundRow2Hidden = true;
							}
							Check(foundRow2Hidden, "la riga nascosta e' proprio la 2, come nel file XLSX originale");
						}

						// AutoFilter: sample.xlsx ha <autoFilter
						// ref="A1:D1"/> (aggiunto apposta per questo
						// test, la stessa riga di intestazione gia'
						// usata per le altre sezioni sopra) -- un byte
						// "presente" seguito da quattro interi,
						// ultima sezione del formato.
						if (pos + 9 <= ascdLen)
						{
							uint8 hasAutoFilter = ascdData[pos]; pos += 1;
							int16 top, left, bottom, right;
							memcpy(&top, ascdData + pos, 2); pos += 2;
							memcpy(&left, ascdData + pos, 2); pos += 2;
							memcpy(&bottom, ascdData + pos, 2); pos += 2;
							memcpy(&right, ascdData + pos, 2); pos += 2;
							Check(hasAutoFilter == 1 && top == 1 && left == 1 && bottom == 1 && right == 4,
								"l'intervallo dell'AutoFilter (A1:D1) e' importato dal file XLSX originale");
						}

						// Commenti/note per cella (Fase 13): sample.xlsx
						// non ne ha nessuno, quindi solo il contatore a
						// zero, ultima sezione del formato (vedi
						// WriteASCD sopra).
						if (pos + 4 <= ascdLen)
						{
							int32 commentCount;
							memcpy(&commentCount, ascdData + pos, 4); pos += 4;
							Check(commentCount == 0,
								"nessun commento in sample.xlsx, il contatore e' zero");
						}

						// Collegamenti ipertestuali (Fase 13): stesso
						// principio della sezione commenti appena sopra,
						// ultima sezione del formato (vedi WriteASCD sopra).
						if (pos + 4 <= ascdLen)
						{
							int32 linkCount;
							memcpy(&linkCount, ascdData + pos, 4); pos += 4;
							Check(linkCount == 0,
								"nessun collegamento ipertestuale in sample.xlsx, il contatore e' zero");
						}

						// Tipo di grafico incorporato (Fase 13): stesso
						// principio delle sezioni commenti/collegamenti
						// appena sopra, ultima sezione del formato (vedi
						// WriteASCD sopra).
						if (pos + 4 <= ascdLen)
						{
							int32 chartTypeCount;
							memcpy(&chartTypeCount, ascdData + pos, 4); pos += 4;
							Check(chartTypeCount == 0,
								"nessun grafico in sample.xlsx, il contatore dei tipi e' zero");
						}

						// Colore del bordo di cella (Fase 13): stesso
						// principio delle sezioni sopra, ultima sezione del
						// formato (vedi WriteASCD sopra).
						if (pos + 4 <= ascdLen)
						{
							int32 borderColorCount;
							memcpy(&borderColorCount, ascdData + pos, 4); pos += 4;
							Check(borderColorCount == 0,
								"nessun colore di bordo personalizzato in sample.xlsx, il contatore e' zero");
						}

						// Convalida dati (Fase 13): stesso principio delle
						// sezioni sopra, ultima sezione del formato (vedi
						// WriteASCD sopra).
						if (pos + 4 <= ascdLen)
						{
							int32 validationCount;
							memcpy(&validationCount, ascdData + pos, 4); pos += 4;
							Check(validationCount == 0,
								"nessuna convalida dati in sample.xlsx, il contatore e' zero");
						}

						// Formattazione condizionale (Fase 13): stesso
						// principio delle sezioni sopra, ultima sezione del
						// formato (vedi WriteASCD sopra) -- sample.xlsx non
						// ha nessuna regola.
						if (pos + 4 <= ascdLen)
						{
							int32 ruleCount;
							memcpy(&ruleCount, ascdData + pos, 4); pos += 4;
							Check(ruleCount == 0,
								"nessuna regola di formattazione condizionale in sample.xlsx, il contatore e' zero");
						}

						// Tabelle strutturate (Fase 14): stesso principio delle
						// sezioni sopra, ultima sezione del formato (vedi
						// WriteASCD sopra) -- sample.xlsx non ha nessuna
						// tabella (nessun <tableParts> nei suoi fogli).
						if (pos + 4 <= ascdLen)
						{
							int32 tableCount;
							memcpy(&tableCount, ascdData + pos, 4); pos += 4;
							Check(tableCount == 0,
								"nessuna tabella strutturata in sample.xlsx, il contatore e' zero");
						}

						// Titolo di grafico incorporato (Fase 17): stesso
						// principio delle sezioni sopra, ULTIMA sezione del
						// formato (vedi WriteASCD sopra) -- bug reale
						// scoperto su un file utente vero: senza questa
						// sezione (aggiunta qui insieme al titolo dei
						// grafici, ma dimenticata nel translator la prima
						// volta) LoadASCDBook disallineava la lettura di
						// OGNI foglio tranne l'ultimo in una cartella di
						// lavoro multi-foglio, perche' l'EOF-tolleranza di
						// LoadASCD funziona solo quando la sezione mancante
						// e' davvero l'ultima cosa nell'intero stream, non
						// solo nel singolo blocco di un foglio.
						if (pos + 4 <= ascdLen)
						{
							int32 chartTitleCount;
							memcpy(&chartTitleCount, ascdData + pos, 4); pos += 4;
							Check(chartTitleCount == 0,
								"nessun grafico in sample.xlsx, il contatore dei titoli e' zero");
						}

						// Area di stampa (Fase 29 di ui/src/AscdIO.cpp):
						// questo translator non legge ancora l'area di
						// stampa dal file XLSX originale, quindi scrive
						// sempre "assente" -- un byte "presente" (qui
						// zero) seguito da quattro interi, sempre
						// scritti anche quando "presente" e' zero.
						if (pos + 9 <= ascdLen)
						{
							uint8 hasPrintArea = ascdData[pos]; pos += 1;
							pos += 8; // top/left/bottom/right, int16 ciascuno
							Check(hasPrintArea == 0,
								"nessuna area di stampa in sample.xlsx, il byte presente/assente e' zero");
						}

						// Margini/scala di "Imposta pagina" (Fase 29):
						// stesso principio della sezione area di stampa
						// appena sopra -- byte "presente" (qui zero) seguito da quattro
						// margini (double), la modalita' di scala
						// (int32) e la percentuale (double), sempre
						// scritti anche quando "presente" e' zero.
						if (pos + 45 <= ascdLen)
						{
							uint8 hasPrintSettings = ascdData[pos]; pos += 1;
							pos += 32; // quattro margini, double ciascuno
							pos += 4; // modalita' di scala, int32
							pos += 8; // percentuale di scala, double
							Check(hasPrintSettings == 0,
								"nessuna impostazione di stampa propria in sample.xlsx, il byte "
								"presente/assente e' zero");
						}

						// Progetto VBA (XLSM, Fase 31): stesso principio
						// della sezione margini/scala appena sopra -- un byte
						// "presente" (qui zero, sample.xlsx non ha
						// macro) e nient'altro quando e' zero (nessuna
						// lunghezza/bytes a seguire, a differenza delle
						// sezioni con valori di default fissi sopra).
						if (pos + 1 <= ascdLen)
						{
							uint8 hasVbaProject = ascdData[pos]; pos += 1;
							Check(hasVbaProject == 0,
								"nessun progetto VBA in sample.xlsx, il byte presente/assente e' zero");
						}

						// Celle sbloccate (Fase 32): un conteggio (qui
						// zero, sample.xlsx non ha celle esplicitamente
						// sbloccate) senza record a seguire quando e'
						// zero.
						if (pos + 4 <= ascdLen)
						{
							int32 unlockedCount;
							memcpy(&unlockedCount, ascdData + pos, 4); pos += 4;
							Check(unlockedCount == 0,
								"nessuna cella sbloccata in sample.xlsx, il conteggio e' zero");
						}

						// Protezione foglio (Fase 32): un solo byte.
						if (pos + 1 <= ascdLen)
						{
							uint8 isProtected = ascdData[pos]; pos += 1;
							Check(isProtected == 0,
								"sample.xlsx non e' protetto, il byte e' zero");
						}

						// Intervalli con nome ("100% XLSX standard
						// compatibility" plan): un conteggio (qui zero,
						// sample.xlsx non ha <definedNames>), ORA
						// l'ULTIMA sezione del formato.
						if (pos + 4 <= ascdLen)
						{
							int32 nameCount;
							memcpy(&nameCount, ascdData + pos, 4); pos += 4;
							Check(nameCount == 0,
								"nessun intervallo con nome in sample.xlsx, il conteggio e' zero");
						}

						// sample.xlsx e' un solo foglio: dopo tutte le
						// sezioni lo stream deve finire ESATTAMENTE qui,
						// non prima (sezione mancante) ne' dopo (byte
						// avanzati, altro sintomo di disallineamento).
						Check(pos == ascdLen,
							"dopo tutte le sezioni lo stream ASCD del foglio finisce esattamente "
							"alla fine del buffer, nessun byte mancante o avanzato");
					}
				}
			}
		}

		doc.Release();
	}

	// Formati numero (Fase 12): tests/sample_numfmt.xlsx ha cinque
	// celle sulla riga 1 -- A1 con un formato incorporato (numFmtId 44,
	// mai definito esplicitamente in <numFmts>, dalla tabella
	// BuiltinNumFmtCode), B1 con un altro incorporato (9 = "0%"), C1 e
	// D1 con due formati personalizzati definiti in <numFmts> (165 =
	// "0.0", 166 = "0.00;[Red]0.00" -- quest'ultimo per verificare che
	// solo la parte prima del ";" venga usata, il colore condizionale
	// del negativo scartato come da limite dichiarato in ROADMAP.md),
	// E1 senza stile esplicito (numFmtId 0 = General, nessun formato
	// da applicare). Gli ID attesi sono calcolati a mano seguendo la
	// stessa logica di CFormatter::ParseTemplate/FormatID (vedi
	// ResolveNumberFormat in XlsxTranslator.cpp).
	{
		BFile numFmtFile("tests/sample_numfmt.xlsx", B_READ_ONLY);
		Check(numFmtFile.InitCheck() == B_OK, "apertura di tests/sample_numfmt.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&numFmtFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_numfmt.xlsx");

		numFmtFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&numFmtFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_numfmt.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_numfmt.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			CContainer &doc = *new CContainer(NULL, NULL);

			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 5, "l'ASCD contiene le 5 celle di sample_numfmt.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				std::string text((const char *)ascdData + pos, len);
				pos += len;
				cell loc(col, row);
				TryToParseString(text.c_str(), loc, &doc, true);
			}

			// Salta grafici (sempre 0)/larghezze colonna (nessuna qui)
			// per arrivare alla sezione colori, poi a quella formato.
			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8); // row+col+WriteColorEntry
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			// Le cinque sezioni vuote di Fase 10/11 (altezze riga,
			// Blocca riquadri, font, allineamento, bordi).
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) { pos += 8; } // Blocca riquadri (due int32 fissi)
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; /* font: lunghezza variabile, non atteso qui (0) */ }

			// A questo punto pos e' dopo fontCount (atteso 0 per questo
			// file, nessuna riga di skip necessaria oltre i 4 byte del
			// contatore gia' consumati sopra) -- allineamento e bordi
			// sotto, entrambe attese vuote.
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+1); } // allineamento
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // bordi

			bool haveFormatCount = false;
			int32 formatCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				haveFormatCount = true;
			}
			Check(haveFormatCount && formatCount == 4,
				"sezione formato numero: 4 celle con formato esplicito (A1/B1/C1/D1, non E1)");

			int foundA1 = -1, foundB1 = -1, foundC1 = -1, foundD1 = -1;
			for (int32 i = 0; i < formatCount && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 format;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&format, ascdData + pos, 4); pos += 4;

				if (row == 1 && col == 1) foundA1 = format;
				if (row == 1 && col == 2) foundB1 = format;
				if (row == 1 && col == 3) foundC1 = format;
				if (row == 1 && col == 4) foundD1 = format;
			}

			// eFixed(3) | cifre<<4 | commas<<9: "#,##0.00" (numFmtId 44
			// incorporato) -> fisso, 2 cifre, virgola = 3|32|512 = 547.
			Check(foundA1 == 547,
				"A1 (numFmtId 44 incorporato, \"#,##0.00\") risolto a fisso/2 cifre/virgola");
			// ePercent(4), "0%" -> nessuna cifra, nessuna virgola = 4.
			Check(foundB1 == 4,
				"B1 (numFmtId 9 incorporato, \"0%\") risolto a percentuale");
			// eFixed(3) | 1<<4, "0.0" (personalizzato) -> 3|16 = 19.
			Check(foundC1 == 19,
				"C1 (numFmtId 165 personalizzato, \"0.0\") risolto a fisso/1 cifra");
			// eFixed(3) | 2<<4, "0.00;[Red]0.00" -> solo la parte prima
			// del ';' conta (colore del negativo scartato) = 3|32 = 35.
			Check(foundD1 == 35,
				"D1 (numFmtId 166 personalizzato, \"0.00;[Red]0.00\") risolto a fisso/2 cifre, "
				"colore negativo scartato come da limite dichiarato");

			doc.Release();
		}
	}

	// Grassetto/corsivo (Fase 12): tests/sample_fontstyle.xlsx ha
	// cinque celle -- A1 grassetto, B1 corsivo, C1 grassetto+corsivo a
	// dimensione 16 (esplicita, diversa dall'11 delle altre), D1 con
	// <b val="0"/> (l'elemento c'e' ma val="0" lo nega esplicitamente:
	// NON deve risultare in grassetto), E1 senza stile esplicito. Solo
	// A1/B1/C1 devono finire nella sezione font della Fase 10 (ora
	// scritta con valori reali invece che sempre vuota) -- D1/E1 non
	// hanno nessuno stile da applicare, restano col font predefinito.
	// La famiglia usata e' sempre quella di be_plain_font (il nome
	// originale "Calibri" nel file non viene cercato/installato, vedi
	// il commento su ResolveStyle in XlsxTranslator.cpp), quindi il
	// test verifica solo stile e dimensione, non la famiglia esatta.
	{
		BFile fontFile("tests/sample_fontstyle.xlsx", B_READ_ONLY);
		Check(fontFile.InitCheck() == B_OK, "apertura di tests/sample_fontstyle.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&fontFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_fontstyle.xlsx");

		fontFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&fontFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_fontstyle.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_fontstyle.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 5, "l'ASCD contiene le 5 celle di sample_fontstyle.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri

			bool haveFontCount = false;
			int32 fontCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				haveFontCount = true;
			}
			Check(haveFontCount && fontCount == 3,
				"sezione font: 3 celle con stile esplicito (A1/B1/C1, non D1/E1)");

			bool foundA1Bold = false, foundA1Italic = false;
			bool foundB1Bold = false, foundB1Italic = false;
			bool foundC1Bold = false, foundC1Italic = false;
			float foundA1Size = 0, foundC1Size = 0;

			for (int32 i = 0; i < fontCount && pos + 4 + sizeof(font_family) + sizeof(font_style) + 4 <= ascdLen; i++)
			{
				int16 row, col;
				font_family family;
				font_style style;
				float size;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(family, ascdData + pos, sizeof(font_family)); pos += sizeof(font_family);
				memcpy(style, ascdData + pos, sizeof(font_style)); pos += sizeof(font_style);
				memcpy(&size, ascdData + pos, 4); pos += 4;

				BString styleStr(style);
				bool bold = styleStr.IFindFirst("Bold") >= 0;
				bool italic = styleStr.IFindFirst("Italic") >= 0;

				if (row == 1 && col == 1) { foundA1Bold = bold; foundA1Italic = italic; foundA1Size = size; }
				if (row == 1 && col == 2) { foundB1Bold = bold; foundB1Italic = italic; }
				if (row == 1 && col == 3) { foundC1Bold = bold; foundC1Italic = italic; foundC1Size = size; }
			}

			Check(foundA1Bold && !foundA1Italic, "A1 (<b/>) importato come grassetto, non corsivo");
			Check(foundA1Size == 11.0f, "A1 usa la dimensione esplicita del file (11), non quella predefinita");
			Check(!foundB1Bold && foundB1Italic, "B1 (<i/>) importato come corsivo, non grassetto");
			Check(foundC1Bold && foundC1Italic, "C1 (<b/><i/>) importato come grassetto E corsivo");
			Check(foundC1Size == 16.0f, "C1 usa la dimensione esplicita del file (16)");
		}
	}

	// Allineamento orizzontale (Fase 12): tests/sample_align.xlsx ha
	// cinque celle -- A1 centrato, B1 a destra, C1 a sinistra
	// (esplicito, per verificare che venga comunque letto e non
	// scambiato per "nessuno stile" come General anche se il risultato
	// visivo coincide col predefinito), D1 giustificato, E1 senza
	// stile esplicito (General, non nella sezione).
	{
		BFile alignFile("tests/sample_align.xlsx", B_READ_ONLY);
		Check(alignFile.InitCheck() == B_OK, "apertura di tests/sample_align.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&alignFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_align.xlsx");

		alignFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&alignFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_align.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_align.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 5, "l'ASCD contiene le 5 celle di sample_align.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount, nessuno stile grassetto/corsivo in questo file
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}

			bool haveAlignCount = false;
			int32 alignCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				haveAlignCount = true;
			}
			Check(haveAlignCount && alignCount == 4,
				"sezione allineamento: 4 celle con stile esplicito (A1/B1/C1/D1, non E1)");

			int foundA1 = -1, foundB1 = -1, foundC1 = -1, foundD1 = -1;
			for (int32 i = 0; i < alignCount && pos + 5 <= ascdLen; i++)
			{
				int16 row, col;
				int8 alignment;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&alignment, ascdData + pos, 1); pos += 1;

				if (row == 1 && col == 1) foundA1 = alignment;
				if (row == 1 && col == 2) foundB1 = alignment;
				if (row == 1 && col == 3) foundC1 = alignment;
				if (row == 1 && col == 4) foundD1 = alignment;
			}

			Check(foundA1 == eAlignCenter, "A1 (horizontal=\"center\") importato come centrato");
			Check(foundB1 == eAlignRight, "B1 (horizontal=\"right\") importato come allineato a destra");
			Check(foundC1 == eAlignLeft,
				"C1 (horizontal=\"left\" esplicito) importato come sinistra, non scambiato per General");
			Check(foundD1 == eAlignJustify, "D1 (horizontal=\"justify\") importato come giustificato");
		}
	}

	// Bordi da stile (Fase 11 -> import XLSX in Fase 12):
	// tests/sample_borders.xlsx ha tre celle -- A1 con tutti e quattro
	// i lati (borderId 1), B1 con solo sinistro e inferiore (borderId
	// 2), C1 senza stile esplicito (borderId implicito 0, nessun
	// lato). Verifica anche che borderId si risolva correttamente
	// contro <borders> (indice separato da fontId/fillId/numFmtId
	// nello stesso <xf>).
	{
		BFile bordersFile("tests/sample_borders.xlsx", B_READ_ONLY);
		Check(bordersFile.InitCheck() == B_OK, "apertura di tests/sample_borders.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&bordersFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_borders.xlsx");

		bordersFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&bordersFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_borders.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_borders.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 3, "l'ASCD contiene le 3 celle di sample_borders.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount, nessuno stile grassetto/corsivo qui
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			if (pos + 4 <= ascdLen) // alignCount, nessun allineamento esplicito qui
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				pos += alignCount * (2 + 2 + 1);
			}

			bool haveBorderCount = false;
			int32 borderCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				haveBorderCount = true;
			}
			Check(haveBorderCount && borderCount == 2,
				"sezione bordi: 2 celle con almeno un lato esplicito (A1/B1, non C1)");

			int foundA1T = -1, foundA1L = -1, foundA1B = -1, foundA1R = -1;
			int foundB1T = -1, foundB1L = -1, foundB1B = -1, foundB1R = -1;
			for (int32 i = 0; i < borderCount && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				uint8 sides[4];
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(sides, ascdData + pos, 4); pos += 4;

				if (row == 1 && col == 1)
				{
					foundA1T = sides[0]; foundA1L = sides[1]; foundA1B = sides[2]; foundA1R = sides[3];
				}
				if (row == 1 && col == 2)
				{
					foundB1T = sides[0]; foundB1L = sides[1]; foundB1B = sides[2]; foundB1R = sides[3];
				}
			}

			Check(foundA1T == 1 && foundA1L == 1 && foundA1B == 1 && foundA1R == 1,
				"A1 (borderId 1) importato con tutti e quattro i lati");
			Check(foundB1T == 0 && foundB1L == 1 && foundB1B == 1 && foundB1R == 0,
				"B1 (borderId 2) importato con solo sinistro e inferiore");
		}
	}

	// Sottolineato (Fase 12): tests/sample_underline.xlsx ha quattro
	// celle -- A1 con <u/> (semplice), B1 con <u val="double"/>
	// (trattato comunque come sottolineato semplice, nessuna
	// distinzione di stile), C1 con <u val="none"/> esplicito (NON
	// sottolineato nonostante l'elemento presente), D1 senza stile
	// esplicito.
	{
		BFile underlineFile("tests/sample_underline.xlsx", B_READ_ONLY);
		Check(underlineFile.InitCheck() == B_OK, "apertura di tests/sample_underline.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&underlineFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_underline.xlsx");

		underlineFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&underlineFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_underline.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_underline.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 4, "l'ASCD contiene le 4 celle di sample_underline.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount, nessuno stile grassetto/corsivo qui
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			if (pos + 4 <= ascdLen) // alignCount, nessun allineamento esplicito qui
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				pos += alignCount * (2 + 2 + 1);
			}
			if (pos + 4 <= ascdLen) // borderCount, nessun bordo esplicito qui
			{
				int32 borderCount;
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				pos += borderCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // formatCount, nessun numFmt esplicito qui
			{
				int32 formatCount;
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				pos += formatCount * (2 + 2 + 4);
			}

			bool haveUnderlineCount = false;
			int32 underlineCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
				haveUnderlineCount = true;
			}
			Check(haveUnderlineCount && underlineCount == 2,
				"sezione sottolineato: 2 celle sottolineate (A1/B1, non C1/D1)");

			bool foundA1 = false, foundB1 = false;
			for (int32 i = 0; i < underlineCount && pos + 4 <= ascdLen; i++)
			{
				int16 row, col;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;

				if (row == 1 && col == 1) foundA1 = true;
				if (row == 1 && col == 2) foundB1 = true;
			}

			Check(foundA1, "A1 (<u/>) importato come sottolineato");
			Check(foundB1, "B1 (<u val=\"double\"/>) importato come sottolineato semplice");
		}
	}

	// Testo a capo (Fase 12): tests/sample_wraptext.xlsx ha quattro
	// celle -- A1 con wrapText="1", B1 con wrapText="1" insieme a
	// horizontal="center" (le due proprieta' dello stesso <alignment>
	// devono convivere), C1 senza stile esplicito, D1 con
	// wrapText="false" per esteso (stile LibreOffice Calc anziche' il
	// solito "0" di Excel -- BUG REALE trovato su un file utente vero:
	// il parsing controllava solo "0", quindi "false" veniva letto come
	// vero, avvolgendo su piu' righe un testo che doveva restare su una
	// riga sola, vedi XlsxAttrIsTrue in XlsxTranslator.cpp).
	{
		BFile wrapFile("tests/sample_wraptext.xlsx", B_READ_ONLY);
		Check(wrapFile.InitCheck() == B_OK, "apertura di tests/sample_wraptext.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&wrapFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_wraptext.xlsx");

		wrapFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&wrapFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_wraptext.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_wraptext.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 4, "l'ASCD contiene le 4 celle di sample_wraptext.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount, nessuno stile grassetto/corsivo qui
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			int foundB1Align = -1;
			if (pos + 4 <= ascdLen) // alignCount: B1 e' anche centrato
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				for (int32 i = 0; i < alignCount && pos + 5 <= ascdLen; i++)
				{
					int16 row, col;
					int8 alignment;
					memcpy(&row, ascdData + pos, 2); pos += 2;
					memcpy(&col, ascdData + pos, 2); pos += 2;
					memcpy(&alignment, ascdData + pos, 1); pos += 1;
					if (row == 1 && col == 2) foundB1Align = alignment;
				}
			}
			if (pos + 4 <= ascdLen) // borderCount, nessun bordo esplicito qui
			{
				int32 borderCount;
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				pos += borderCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // formatCount, nessun numFmt esplicito qui
			{
				int32 formatCount;
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				pos += formatCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // underlineCount, nessun sottolineato qui
			{
				int32 underlineCount;
				memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
				pos += underlineCount * (2 + 2);
			}

			bool haveWrapCount = false;
			int32 wrapCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&wrapCount, ascdData + pos, 4); pos += 4;
				haveWrapCount = true;
			}
			Check(haveWrapCount && wrapCount == 2,
				"sezione testo a capo: 2 celle con a capo attivo (A1/B1, non C1/D1)");

			bool foundA1 = false, foundB1 = false, foundD1 = false;
			for (int32 i = 0; i < wrapCount && pos + 4 <= ascdLen; i++)
			{
				int16 row, col;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;

				if (row == 1 && col == 1) foundA1 = true;
				if (row == 1 && col == 2) foundB1 = true;
				if (row == 1 && col == 4) foundD1 = true;
			}

			Check(foundA1, "A1 (wrapText=\"1\") importato con a capo attivo");
			Check(foundB1, "B1 (wrapText=\"1\" + horizontal=\"center\") importato con a capo attivo");
			Check(foundB1Align == eAlignCenter,
				"B1 mantiene anche l'allineamento centrato, le due proprieta' convivono");
			Check(!foundD1,
				"D1 (wrapText=\"false\" per esteso, stile LibreOffice) NON importato con a capo attivo");
		}
	}

	// Celle unite (Fase 12): tests/sample_merge.xlsx ha due intervalli
	// -- A1:C1 (intestazione orizzontale) e A2:A3 (verticale) -- piu'
	// D1 normale, mai coinvolta.
	{
		BFile mergeFile("tests/sample_merge.xlsx", B_READ_ONLY);
		Check(mergeFile.InitCheck() == B_OK, "apertura di tests/sample_merge.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&mergeFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_merge.xlsx");

		mergeFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&mergeFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_merge.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_merge.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 3, "l'ASCD contiene le 3 celle con contenuto di sample_merge.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			if (pos + 4 <= ascdLen) // alignCount
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				pos += alignCount * (2 + 2 + 1);
			}
			if (pos + 4 <= ascdLen) // borderCount
			{
				int32 borderCount;
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				pos += borderCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // formatCount
			{
				int32 formatCount;
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				pos += formatCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // underlineCount
			{
				int32 underlineCount;
				memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
				pos += underlineCount * (2 + 2);
			}
			if (pos + 4 <= ascdLen) // wrapCount
			{
				int32 wrapCount;
				memcpy(&wrapCount, ascdData + pos, 4); pos += 4;
				pos += wrapCount * (2 + 2);
			}

			bool haveMergeCount = false;
			int32 mergeCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&mergeCount, ascdData + pos, 4); pos += 4;
				haveMergeCount = true;
			}
			Check(haveMergeCount && mergeCount == 2,
				"sezione celle unite: 2 intervalli (A1:C1 e A2:A3)");

			bool foundHorizontal = false, foundVertical = false;
			for (int32 i = 0; i < mergeCount && pos + 8 <= ascdLen; i++)
			{
				int16 top, left, bottom, right;
				memcpy(&top, ascdData + pos, 2); pos += 2;
				memcpy(&left, ascdData + pos, 2); pos += 2;
				memcpy(&bottom, ascdData + pos, 2); pos += 2;
				memcpy(&right, ascdData + pos, 2); pos += 2;

				if (top == 1 && left == 1 && bottom == 1 && right == 3)
					foundHorizontal = true;
				if (top == 2 && left == 1 && bottom == 3 && right == 1)
					foundVertical = true;
			}

			Check(foundHorizontal, "A1:C1 importato come intervallo unito orizzontale");
			Check(foundVertical, "A2:A3 importato come intervallo unito verticale");
		}
	}

	// Immagini incorporate (Fase 12): tests/sample_image.xlsx ha A1
	// ("Testo", una cella normale che deve convivere con l'immagine)
	// piu' un logo PNG 4x3 ancorato a B2 (xdr:from col=1/row=1,
	// 0-based) con uno scarto di 95250x47625 EMU (10x5 pixel) e una
	// dimensione esplicita di 381000x285750 EMU (40x30 pixel) -- il
	// caso "gia' ridimensionata a mano", diverso dal file di gara
	// reale (cx/cy=0, dove si usa la dimensione naturale del PNG).
	{
		BFile imageFile("tests/sample_image.xlsx", B_READ_ONLY);
		Check(imageFile.InitCheck() == B_OK, "apertura di tests/sample_image.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&imageFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_image.xlsx");

		imageFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&imageFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_image.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_image.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 1, "l'ASCD contiene la sola cella con contenuto di sample_image.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			if (pos + 4 <= ascdLen) // alignCount
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				pos += alignCount * (2 + 2 + 1);
			}
			if (pos + 4 <= ascdLen) // borderCount
			{
				int32 borderCount;
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				pos += borderCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // formatCount
			{
				int32 formatCount;
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				pos += formatCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // underlineCount
			{
				int32 underlineCount;
				memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
				pos += underlineCount * (2 + 2);
			}
			if (pos + 4 <= ascdLen) // wrapCount
			{
				int32 wrapCount;
				memcpy(&wrapCount, ascdData + pos, 4); pos += 4;
				pos += wrapCount * (2 + 2);
			}
			if (pos + 4 <= ascdLen) // mergeCount
			{
				int32 mergeCount;
				memcpy(&mergeCount, ascdData + pos, 4); pos += 4;
				pos += mergeCount * (2 * 4);
			}

			bool haveImageCount = false;
			int32 imageCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&imageCount, ascdData + pos, 4); pos += 4;
				haveImageCount = true;
			}
			Check(haveImageCount && imageCount == 1,
				"sezione immagini incorporate: 1 immagine (il logo di sample_image.xlsx)");

			if (haveImageCount && imageCount == 1 && pos + 2 + 2 + 16 + 4 <= ascdLen)
			{
				int16 row, col;
				float geom[4];
				int32 pngLen;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(geom, ascdData + pos, 16); pos += 16;
				memcpy(&pngLen, ascdData + pos, 4); pos += 4;

				Check(row == 2 && col == 2, "l'immagine e' ancorata a B2 (xdr:from col=1/row=1, 0-based)");
				Check(geom[0] == 10.0f && geom[1] == 5.0f,
					"lo scarto dall'angolo di B2 e' 10x5 pixel (95250x47625 EMU / 9525)");
				Check(geom[2] == 40.0f && geom[3] == 30.0f,
					"la dimensione esplicita e' 40x30 pixel (381000x285750 EMU / 9525), non quella naturale del PNG");
				Check(pngLen == 75, "il blob PNG incorporato ha la lunghezza originale (75 byte)");

				if (pos + (size_t)pngLen <= ascdLen && pngLen >= 8)
				{
					static const unsigned char kPngSig[8] =
						{ 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
					Check(memcmp(ascdData + pos, kPngSig, 8) == 0,
						"il blob PNG incorporato inizia con la firma PNG vera");
					pos += pngLen;
				}
			}
		}
	}

	// Tabelle strutturate (Fase 12): tests/sample_table.xlsx ha una
	// tabella A1:B4 (TableStyleMedium2, showRowStripes="1") -- A1 e'
	// l'intestazione (mai bandata), A2/B2 e A4/B4 sono la prima e
	// terza riga dati (bandate), A3/B3 la seconda (non bandata,
	// alternanza corretta).
	{
		BFile tableFile("tests/sample_table.xlsx", B_READ_ONLY);
		Check(tableFile.InitCheck() == B_OK, "apertura di tests/sample_table.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&tableFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_table.xlsx");

		tableFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&tableFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_table.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_table.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 8, "l'ASCD contiene le 8 celle di sample_table.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}

			bool haveCellColorCount = false;
			int32 cellColorCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				haveCellColorCount = true;
			}
			Check(haveCellColorCount && cellColorCount == 4,
				"sezione colori di cella: 4 celle bandate (A2/B2/A4/B4)");

			bool foundA2 = false, foundB2 = false, foundA4 = false, foundB4 = false;
			bool colorsCorrect = true;
			for (int32 i = 0; i < cellColorCount && pos + 4 + 8 <= ascdLen; i++)
			{
				int16 row, col;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				// WriteColorEntry: due rgb_color da 4 byte (bg poi fg).
				rgb_color bg;
				memcpy(&bg, ascdData + pos, 4); pos += 4;
				pos += 4; // fg, non verificato qui

				bool isBand = bg.red == 242 && bg.green == 242 && bg.blue == 242;
				if (row == 2 && col == 1) { foundA2 = true; colorsCorrect &= isBand; }
				if (row == 2 && col == 2) { foundB2 = true; colorsCorrect &= isBand; }
				if (row == 4 && col == 1) { foundA4 = true; colorsCorrect &= isBand; }
				if (row == 4 && col == 2) { foundB4 = true; colorsCorrect &= isBand; }
				if (row == 1 || row == 3)
					Check(false, "nessuna cella dell'intestazione o della riga dati pari ha un colore");
			}

			Check(foundA2 && foundB2 && foundA4 && foundB4 && colorsCorrect,
				"A2/B2 (prima riga dati) e A4/B4 (terza) hanno il colore di banda grigio chiaro");

			// Tutte le sezioni "in coda" successive fino ad AutoFilter
			// (colori di colonna, altezze di riga, font/allineamento/
			// bordi/formato/sottolineato/testo a capo per cella, celle
			// unite, immagini incorporate): sample_table.xlsx non ne
			// popola nessuna -- stesso identico elenco/ordine gia'
			// verificato nel blocco di sample_condformat.xlsx piu' sotto,
			// qui interessa solo restare allineati fino alla sezione
			// tabelle, non riverificarle una per una in questo file.
			bool sectionsOk = true;
			const char* kEmptyListSections[] = {
				"colori di colonna", "altezze di riga", "font di cella",
				"allineamento di cella", "bordi di cella",
				"formato numero di cella", "sottolineato di cella",
				"testo a capo di cella", "celle unite", "immagini incorporate"
			};
			for (size_t s = 0; sectionsOk
					&& s < sizeof(kEmptyListSections) / sizeof(kEmptyListSections[0]); s++)
			{
				if (pos + 4 > ascdLen) { sectionsOk = false; break; }
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				sectionsOk = (n == 0);
			}
			Check(sectionsOk, "le sezioni fra i colori di cella e Blocca riquadri restano allineate in sample_table.xlsx");

			// Blocca riquadri: due interi FISSI (non un elenco).
			if (sectionsOk && pos + 8 <= ascdLen)
				pos += 8;
			else
				sectionsOk = false;

			// Visibilita' griglia: un solo byte FISSO.
			if (sectionsOk && pos + 1 <= ascdLen)
				pos += 1;
			else
				sectionsOk = false;

			// Colore della linguetta: presenza + rgb, 4 byte FISSI.
			if (sectionsOk && pos + 4 <= ascdLen)
				pos += 4;
			else
				sectionsOk = false;

			// Righe nascoste: elenco con contatore, vuoto qui.
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				sectionsOk = (n == 0);
			}
			else
				sectionsOk = false;

			// AutoFilter: presenza + 4 interi a 16 bit, 9 byte FISSI --
			// sample_table.xlsx non ha <autoFilter> nel foglio stesso
			// (solo dentro la tabella, un'altra sezione, letta piu'
			// sotto), quindi "has" e' sempre 0 qui.
			if (sectionsOk && pos + 9 <= ascdLen)
				pos += 9;
			else
				sectionsOk = false;

			// Commenti/collegamenti/tipo di grafico/colore del bordo/
			// convalida dati, in coda: tutti contatori a 4 byte, sempre
			// zero per questo file.
			for (int s = 0; sectionsOk && s < 5 && pos + 4 <= ascdLen; s++)
			{
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				sectionsOk = (n == 0);
			}
			Check(sectionsOk, "le sezioni fra Blocca riquadri e la formattazione condizionale restano allineate in sample_table.xlsx");

			// Formattazione condizionale VIVA: sample_table.xlsx non ha
			// nessuna regola.
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				int32 ruleCount;
				memcpy(&ruleCount, ascdData + pos, 4); pos += 4;
				sectionsOk = (ruleCount == 0);
			}
			else
				sectionsOk = false;
			Check(sectionsOk, "nessuna regola di formattazione condizionale in sample_table.xlsx, resta allineato");

			// Tabelle strutturate (Fase 14): sample_table.xlsx ha una vera
			// <table name="Tabella1" ref="A1:B4"><tableColumns>...
			// Codice...Descrizione...</tableColumns></table> -- la prova
			// che RegisterTable legge davvero l'XML di Excel, non solo un
			// CTableDef costruito a mano nei test del motore (vedi
			// engine/tests/table_refs_test.cpp).
			bool haveTableCount = false;
			int32 tableCount = 0;
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				memcpy(&tableCount, ascdData + pos, 4); pos += 4;
				haveTableCount = true;
			}
			Check(haveTableCount && tableCount == 1,
				"una tabella strutturata (\"Tabella1\") registrata da sample_table.xlsx");

			if (haveTableCount && tableCount == 1 && pos + 4 <= ascdLen)
			{
				int32 nameLen = 0;
				memcpy(&nameLen, ascdData + pos, 4); pos += 4;
				std::string name;
				if (nameLen > 0 && pos + (size_t)nameLen <= ascdLen)
				{
					name.assign((const char *)(ascdData + pos), nameLen);
					pos += nameLen;
				}
				Check(name == "Tabella1", "il nome della tabella (\"Tabella1\") e' quello corretto");

				int16 tLeft = 0, tTop = 0, tRight = 0, tBottom = 0;
				if (pos + 8 <= ascdLen)
				{
					memcpy(&tLeft, ascdData + pos, 2); pos += 2;
					memcpy(&tTop, ascdData + pos, 2); pos += 2;
					memcpy(&tRight, ascdData + pos, 2); pos += 2;
					memcpy(&tBottom, ascdData + pos, 2); pos += 2;
				}
				// A1:B4 nel file originale, intestazione (riga 1) esclusa
				// da CTableDef::dataRange: A2:B4.
				Check(tLeft == 1 && tTop == 2 && tRight == 2 && tBottom == 4,
					"l'intervallo dati (A2:B4, intestazione esclusa) e' quello corretto");

				int32 columnCount = 0;
				if (pos + 4 <= ascdLen)
				{
					memcpy(&columnCount, ascdData + pos, 4); pos += 4;
				}
				Check(columnCount == 2, "due colonne registrate (Codice, Descrizione)");

				std::string col0, col1;
				for (int32 c = 0; c < columnCount && pos + 4 <= ascdLen; c++)
				{
					int32 colLen = 0;
					memcpy(&colLen, ascdData + pos, 4); pos += 4;
					std::string colName;
					if (colLen > 0 && pos + (size_t)colLen <= ascdLen)
					{
						colName.assign((const char *)(ascdData + pos), colLen);
						pos += colLen;
					}
					if (c == 0) col0 = colName;
					if (c == 1) col1 = colName;
				}
				Check(col0 == "Codice" && col1 == "Descrizione",
					"i nomi delle colonne (\"Codice\", \"Descrizione\") sono nell'ordine giusto");
			}
		}
	}

	// Formattazione condizionale VIVA (Fase 13, prima Fase 12):
	// tests/sample_condformat.xlsx ha due regole -- cellIs/equal
	// "Mancante" su A1:A3 (dxf 0 = rgb FFC7CE) e duplicateValues su
	// B1:B3 (dxf 1 = rgb FFEB9C). Dalla Fase 13 in poi questo
	// translator non scrive piu' un colore congelato per le celle che
	// corrispondono ORA (A1, B1, B3): aggiunge invece la REGOLA vera e
	// propria al documento (CContainer::AddConditionalFormatRule),
	// verificata qui leggendo la nuova sezione dedicata del formato
	// nativo -- la valutazione VIVA vera e propria (quali celle
	// corrispondono, che si aggiorna da sola se il valore cambia) e'
	// invece verificata in ui/tests/test_ascd_io.cpp, che puo'
	// collegare ui/src/AscdIO.cpp (una dipendenza che i translator
	// evitano deliberatamente).
	{
		BFile condFile("tests/sample_condformat.xlsx", B_READ_ONLY);
		Check(condFile.InitCheck() == B_OK, "apertura di tests/sample_condformat.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&condFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_condformat.xlsx");

		condFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&condFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_condformat.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_condformat.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 6, "l'ASCD contiene le 6 celle di sample_condformat.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}

			// Sezione colori di cella (Fase 7): 0, non piu' 3 come prima
			// della Fase 13 -- la formattazione condizionale non
			// congela piu' un colore qui (vedi il commento sopra
			// ApplyConditionalFormatting in XlsxTranslator.cpp), resta
			// invece una REGOLA viva, verificata piu' sotto.
			bool sectionsOk = true;
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				Check(cellColorCount == 0,
					"sezione colori di cella: nessuna, la formattazione condizionale non scrive piu' qui");
				sectionsOk = (cellColorCount == 0);
			}
			else
				sectionsOk = false;

			// Tutte le sezioni "in coda" successive (colori di colonna,
			// altezze di riga, font/allineamento/bordi/formato/
			// sottolineato/testo a capo per cella, celle unite,
			// immagini incorporate) sono elenchi con un contatore --
			// questo file di prova minimo (solo due colonne di dati e
			// due regole di formattazione condizionale) non ne popola
			// nessuna: basta leggere e verificare che il contatore sia
			// zero per restare allineati, senza bisogno di conoscere
			// il formato esatto di ogni record (che qui non esiste).
			const char* kEmptyListSections[] = {
				"colori di colonna", "altezze di riga", "font di cella",
				"allineamento di cella", "bordi di cella",
				"formato numero di cella", "sottolineato di cella",
				"testo a capo di cella", "celle unite", "immagini incorporate"
			};
			for (size_t s = 0; sectionsOk
					&& s < sizeof(kEmptyListSections) / sizeof(kEmptyListSections[0]); s++)
			{
				if (pos + 4 > ascdLen) { sectionsOk = false; break; }
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				BString what;
				what << "sezione " << kEmptyListSections[s]
					<< " vuota in sample_condformat.xlsx (allineamento)";
				Check(n == 0, what.String());
				sectionsOk = (n == 0);
			}

			// Blocca riquadri: due interi FISSI (non un elenco).
			if (sectionsOk && pos + 8 <= ascdLen)
				pos += 8;
			else
				sectionsOk = false;

			// Visibilita' griglia: un solo byte FISSO.
			if (sectionsOk && pos + 1 <= ascdLen)
				pos += 1;
			else
				sectionsOk = false;

			// Colore della linguetta: presenza + rgb, 4 byte FISSI.
			if (sectionsOk && pos + 4 <= ascdLen)
				pos += 4;
			else
				sectionsOk = false;

			// Righe nascoste: elenco con contatore, vuoto qui.
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				Check(n == 0, "sezione righe nascoste vuota in sample_condformat.xlsx (allineamento)");
				sectionsOk = (n == 0);
			}
			else
				sectionsOk = false;

			// AutoFilter: presenza + 4 interi a 16 bit, 9 byte FISSI.
			if (sectionsOk && pos + 9 <= ascdLen)
				pos += 9;
			else
				sectionsOk = false;

			// Commenti/collegamenti ipertestuali: elenchi a lunghezza
			// variabile, ma vuoti qui -- basta il contatore.
			const char* kEmptyVariableSections[] = { "commenti", "collegamenti ipertestuali" };
			for (size_t s = 0; sectionsOk
					&& s < sizeof(kEmptyVariableSections) / sizeof(kEmptyVariableSections[0]); s++)
			{
				if (pos + 4 > ascdLen) { sectionsOk = false; break; }
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				BString what;
				what << "sezione " << kEmptyVariableSections[s]
					<< " vuota in sample_condformat.xlsx (allineamento)";
				Check(n == 0, what.String());
				sectionsOk = (n == 0);
			}

			// Tipo di grafico: un byte per grafico incorporato -- 0
			// qui (questo file non ne ha nessuno, vedi il chartCount
			// letto piu' sopra).
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				Check(n == 0, "sezione tipo di grafico vuota in sample_condformat.xlsx (allineamento)");
				sectionsOk = (n == 0);
			}
			else
				sectionsOk = false;

			// Colore del bordo: elenco, vuoto qui.
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				Check(n == 0, "sezione colore del bordo vuota in sample_condformat.xlsx (allineamento)");
				sectionsOk = (n == 0);
			}
			else
				sectionsOk = false;

			// Convalida dati: elenco a lunghezza variabile, vuoto qui.
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				Check(n == 0, "sezione convalida dati vuota in sample_condformat.xlsx (allineamento)");
				sectionsOk = (n == 0);
			}
			else
				sectionsOk = false;

			Check(sectionsOk,
				"tutte le sezioni intermedie (vuote in questo file minimo) restano allineate "
				"fino alla formattazione condizionale");

			// Finalmente, la sezione che questo test vuole davvero
			// verificare: le due regole VIVE importate da
			// sample_condformat.xlsx (non piu' un colore congelato).
			int32 ruleCount = 0;
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				memcpy(&ruleCount, ascdData + pos, 4); pos += 4;
			}
			Check(ruleCount == 2,
				"due regole di formattazione condizionale importate da sample_condformat.xlsx");

			bool foundCellIsRule = false, foundDuplicatesRule = false;
			for (int32 i = 0; i < ruleCount && pos + 1 + 4 <= ascdLen; i++)
			{
				int8 type;
				memcpy(&type, ascdData + pos, 1); pos += 1;
				int32 valueLen;
				memcpy(&valueLen, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)valueLen > ascdLen)
					break;
				std::string compareValue((const char*)ascdData + pos, valueLen);
				pos += valueLen;

				if (pos + 4 > ascdLen)
					break;
				rgb_color bg;
				memcpy(&bg, ascdData + pos, 4); pos += 4;
				int packed = (bg.red << 16) | (bg.green << 8) | bg.blue;

				if (pos + 4 > ascdLen)
					break;
				int32 rangeCount;
				memcpy(&rangeCount, ascdData + pos, 4); pos += 4;

				bool rangeMatchesA1A3 = false, rangeMatchesB1B3 = false;
				for (int32 r = 0; r < rangeCount && pos + 8 <= ascdLen; r++)
				{
					int16 left, top, right, bottom;
					memcpy(&left, ascdData + pos, 2); pos += 2;
					memcpy(&top, ascdData + pos, 2); pos += 2;
					memcpy(&right, ascdData + pos, 2); pos += 2;
					memcpy(&bottom, ascdData + pos, 2); pos += 2;

					if (left == 1 && right == 1 && top == 1 && bottom == 3)
						rangeMatchesA1A3 = true;
					if (left == 2 && right == 2 && top == 1 && bottom == 3)
						rangeMatchesB1B3 = true;
				}

				if (type == eCondCellIsEqual && compareValue == "Mancante" && packed == 0xFFC7CE
					&& rangeMatchesA1A3)
					foundCellIsRule = true;
				if (type == eCondDuplicateValues && packed == 0xFFEB9C && rangeMatchesB1B3)
					foundDuplicatesRule = true;
			}
			Check(foundCellIsRule,
				"la regola cellIs/equal (\"Mancante\", dxf 0 = FFC7CE) e' importata correttamente");
			Check(foundDuplicatesRule,
				"la regola duplicateValues (dxf 1 = FFEB9C) e' importata correttamente");

			// La valutazione VIVA vera e propria (il valore di ogni
			// cella confrontato con la regola, non solo che la regola
			// sia stata letta) e' verificata a parte in
			// ui/tests/test_ascd_io.cpp, che puo' ricostruire un
			// CContainer vero con CContainer::EvaluateConditionalFormatting
			// -- qui non e' disponibile senza collegare ui/src/AscdIO.cpp,
			// una dipendenza che i translator evitano deliberatamente.
		}
	}

	// Formati data/ora (Fase 12): tests/sample_dates.xlsx ha tre celle
	// -- A1 (serial 45892, numFmtId 14 incorporato) e B1 (serial
	// 44197, numFmtId 165 personalizzato "dd/mm/yyyy") sono numeri
	// puri con uno stile data, D1 (100, nessuno stile data) resta un
	// numero normale. Sistema data predefinito (1899-12-30, nessun
	// <workbookPr date1904="1"/> nel file). Il testo scritto in ASCD
	// per una cella senza formula passa comunque da GetCellFormula ->
	// CFormatter::FormatValue (vedi Container.cpp), che per un Value
	// eTimeData chiama sempre FormatDate indipendentemente da
	// CellStyle::fFormat -- il valore vero e' quindi gia' verificabile
	// dal testo stesso, senza dover leggere Value/CellStyle a parte.
	{
		BFile dateFile("tests/sample_dates.xlsx", B_READ_ONLY);
		Check(dateFile.InitCheck() == B_OK, "apertura di tests/sample_dates.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&dateFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_dates.xlsx");

		dateFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&dateFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_dates.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_dates.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 3, "l'ASCD contiene le 3 celle di sample_dates.xlsx");

			bool foundA1 = false, foundB1 = false, foundD1 = false;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				std::string text((const char *)ascdData + pos, len);
				pos += len;

				if (row == 1 && col == 1 && text == "23/08/2025") foundA1 = true;
				if (row == 1 && col == 2 && text == "01/01/2021") foundB1 = true;
				if (row == 1 && col == 4 && text == "100") foundD1 = true;
			}

			Check(foundA1,
				"A1 (serial 45892, numFmtId 14 incorporato) importato come data 23/08/2025");
			Check(foundB1,
				"B1 (serial 44197, numFmtId 165 personalizzato) importato come data 01/01/2021");
			Check(foundD1, "D1 (100, nessuno stile data) resta un numero normale, non una data");
		}
	}

	// Esportazione (ASCD -> XLSX): scrive un documento con un numero,
	// una stringa e una formula, poi rilegge il file XLSX prodotto con
	// lo stesso translator (round-trip completo) per verificare che i
	// valori sopravvivano e che la formula sia sopravvissuta VIVA
	// (scrive sia <f> che <v>, vedi BuildSheetXml in
	// XlsxTranslator.cpp), non solo il suo valore calcolato come CSV
	// (che non ha un concetto di formula) -- comportamento cambiato
	// rispetto a prima: l'export scriveva solo il valore anche per
	// XLSX/ODS.
	{
		CContainer &exportDoc = *new CContainer(NULL, NULL);
		TryToParseString("12", cell(1, 1), &exportDoc, true);       // A1 = 12
		TryToParseString("8", cell(2, 1), &exportDoc, true);        // B1 = 8
		TryToParseString("=A1+B1", cell(3, 1), &exportDoc, true);   // C1 = 20
		TryToParseString("Prova export", cell(1, 2), &exportDoc, true); // A2
		exportDoc.CalcCell(cell(3, 1));

		BMallocIO ascdIn;
		status_t saveErr = WriteASCDForTest(&exportDoc, &ascdIn);
		Check(saveErr == B_OK, "preparazione dell'ASCD di prova per l'export riesce");
		exportDoc.Release();

		ascdIn.Seek(0, SEEK_SET);
		translator_info exportInfo;
		err = translator->Identify(&ascdIn, NULL, NULL, &exportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD come sorgente per l'export");
		Check(exportInfo.type == kAtomoNativeFormat,
			"Identify classifica il sorgente come ASCD nativo");

		ascdIn.Seek(0, SEEK_SET);
		BMallocIO xlsxOut;
		err = translator->Translate(&ascdIn, &exportInfo, NULL, kAtomoXlsxFormat, &xlsxOut);
		Check(err == B_OK, "Translate ASCD -> XLSX riesce");

		xlsxOut.Seek(0, SEEK_SET);
		translator_info reimportInfo;
		err = translator->Identify(&xlsxOut, NULL, NULL, &reimportInfo, 0);
		Check(err == B_OK && reimportInfo.type == kAtomoXlsxFormat,
			"il file XLSX appena scritto viene riconosciuto come XLSX valido rileggendolo");

		xlsxOut.Seek(0, SEEK_SET);
		BMallocIO ascdOut2;
		err = translator->Translate(&xlsxOut, &reimportInfo, NULL, kAtomoNativeFormat, &ascdOut2);
		Check(err == B_OK, "il file XLSX appena scritto si rilegge correttamente (round-trip)");

		if (err == B_OK)
		{
			const unsigned char *rawData2 = (const unsigned char *)ascdOut2.Buffer();
			size_t rawLen2 = ascdOut2.BufferLength();
			const unsigned char *data = NULL;
			size_t len = 0;
			Check(UnwrapFirstSheet(rawData2, rawLen2, &data, &len),
				"il round-trip produce anch'esso una cartella ASCB valida o un ASCD nudo");
			size_t pos = 12;
			int32 cnt;
			memcpy(&cnt, data + 8, 4);

			// 0 = kAscdCellFormula (vedi lo stesso enum privato in
			// XlsxTranslator.cpp): l'unico modo affidabile di sapere se
			// la cella C1 e' tornata una formula viva invece di un
			// valore statico, dato che il TESTO da solo ("A1+B1") non
			// lo direbbe con certezza in ogni caso possibile.
			static const uint8 kCellKindFormula = 0;

			bool reA1 = false, reB1 = false, reC1Formula = false, reA2 = false;
			for (int32 i = 0; i < cnt && pos + 8 <= len; i++)
			{
				int16 row, col;
				int32 tlen;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&tlen, data + pos, 4); pos += 4;
				uint8 kind = data[pos]; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				pos += 1;
				if (pos + tlen > len)
					break;
				std::string text((const char *)data + pos, tlen);
				pos += tlen;

				if (row == 1 && col == 1 && text == "12") reA1 = true;
				if (row == 1 && col == 2 && text == "8") reB1 = true;
				if (row == 1 && col == 3 && kind == kCellKindFormula
						&& text.find("A1") != std::string::npos
						&& text.find("B1") != std::string::npos)
					reC1Formula = true;
				if (row == 2 && col == 1 && text == "Prova export") reA2 = true;
			}

			Check(reA1, "dopo il round-trip, A1 vale ancora 12");
			Check(reB1, "dopo il round-trip, B1 vale ancora 8");
			Check(reC1Formula,
				"dopo il round-trip, C1 (era una formula) e' sopravvissuta come formula viva "
				"(A1+B1), non appiattita al suo valore calcolato (20)");
			Check(reA2, "dopo il round-trip, A2 vale ancora \"Prova export\"");
		}
	}

	// Formula con un riferimento a un altro foglio (qui mai risolto,
	// "AltroFoglio" non esiste davvero -- non serve perche' il punto e'
	// solo verificare COSA viene scritto in export, non il calcolo):
	// l'export NON deve scrivere una formula viva per questa cella,
	// solo il suo valore calcolato -- questo translator esporta un
	// solo foglio per file (vedi BuildSheetXml), un riferimento
	// incrociato punterebbe a dati che nel file esportato non
	// esistono affatto (vedi CFormula::ReferencesOtherSheet).
	{
		CContainer &xrefDoc = *new CContainer(NULL, NULL);
		TryToParseString("=AltroFoglio!A1+5", cell(1, 1), &xrefDoc, true); // A1
		xrefDoc.CalcCell(cell(1, 1)); // non risolve (nessun resolver), ma non ci interessa il valore

		BMallocIO xrefAscdIn;
		WriteASCDForTest(&xrefDoc, &xrefAscdIn);
		xrefDoc.Release();

		xrefAscdIn.Seek(0, SEEK_SET);
		translator_info xrefInfo;
		translator->Identify(&xrefAscdIn, NULL, NULL, &xrefInfo, kAtomoXlsxFormat);
		xrefAscdIn.Seek(0, SEEK_SET);
		BMallocIO xrefXlsxOut;
		err = translator->Translate(&xrefAscdIn, &xrefInfo, NULL, kAtomoXlsxFormat, &xrefXlsxOut);
		Check(err == B_OK, "Translate ASCD -> XLSX riesce anche con un riferimento a un altro foglio");

		// Cerca direttamente nel file XLSX prodotto (un archivio ZIP):
		// "<f>" non deve comparire affatto per questa cella -- basta
		// cercarlo nel blob intero, l'unica formula presente in questo
		// documento di prova e' proprio questa.
		std::string xlsxBytes((const char *)xrefXlsxOut.Buffer(), xrefXlsxOut.BufferLength());
		Check(xlsxBytes.find("<f>") == std::string::npos,
			"una formula con riferimento a un altro foglio non viene scritta come <f> "
			"(solo il suo valore, per non puntare a dati assenti nel file esportato)");
	}

	// Separatori canonici indipendenti dalle preferenze correnti
	// dell'utente (decSep/listSep, vedi CFormula::UnMangle): una
	// formula con un letterale decimale e piu' argomenti deve
	// esportarsi sempre con "." e "," (sintassi canonica ECMA-376),
	// anche se l'utente ha impostato virgola come separatore
	// decimale e punto e virgola come separatore di elenco (il
	// default italiano) -- altrimenti Excel non capirebbe la formula
	// scritta nel file. gDecimalPoint/gListSeparator sono globali
	// dell'engine: si ripristinano al valore originale alla fine di
	// questo blocco, per non alterare lo stato degli altri test.
	{
		char savedDecimalPoint = gDecimalPoint;
		char savedListSeparator = gListSeparator;
		gDecimalPoint = ',';
		gListSeparator = ';';

		CContainer &sepDoc = *new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), &sepDoc, true);   // A1
		TryToParseString("2", cell(1, 2), &sepDoc, true);   // A2
		TryToParseString("=SUM(A1;A2)+1,5", cell(1, 3), &sepDoc, true); // A3, sintassi italiana
		sepDoc.CalcCell(cell(1, 3));

		BMallocIO sepAscdIn;
		WriteASCDForTest(&sepDoc, &sepAscdIn);
		sepDoc.Release();

		sepAscdIn.Seek(0, SEEK_SET);
		translator_info sepInfo;
		translator->Identify(&sepAscdIn, NULL, NULL, &sepInfo, kAtomoXlsxFormat);
		sepAscdIn.Seek(0, SEEK_SET);
		BMallocIO sepXlsxOut;
		err = translator->Translate(&sepAscdIn, &sepInfo, NULL, kAtomoXlsxFormat, &sepXlsxOut);
		Check(err == B_OK, "Translate ASCD -> XLSX riesce con gDecimalPoint=',' e gListSeparator=';'");

		gDecimalPoint = savedDecimalPoint;
		gListSeparator = savedListSeparator;

		std::string sepXlsxBytes((const char *)sepXlsxOut.Buffer(), sepXlsxOut.BufferLength());
		Check(sepXlsxBytes.find("<f>SUM(A1,A2)+1.5</f>") != std::string::npos,
			"la formula esportata usa sempre \",\" come separatore di argomenti e \".\" come "
			"decimale, indipendentemente dalle preferenze locali correnti dell'utente");
	}

	// Formule con piu' argomenti separati da virgola (Fase 13): bug
	// reale scoperto su un file reale, tre cause distinte nello stesso
	// sintomo (una formula mostrata come testo grezzo invece del
	// valore calcolato) --
	// 1. "IFERROR" (nome standard Excel) non esisteva affatto nella
	//    tabella funzioni, solo "IFERR" (nome storico di Sum-It).
	// 2. VLOOKUP aveva argCnt=3 ESATTO nella risorsa 'Func', ma il
	//    quarto argomento (corrispondenza esatta/approssimata) e'
	//    quasi sempre presente in un file reale.
	// 3. ParseSheet chiamava TryToParseString senza mai passare
	//    decSep='.'/listSep=',' espliciti: il testo di <f> in un file
	//    XLSX e' SEMPRE virgola fra gli argomenti (formato canonico
	//    ECMA-376), indipendente dalla lingua con cui e' stato scritto
	//    in Excel -- con gListSeparator=';' (il default per l'Italia)
	//    OGNI formula con piu' di un argomento falliva l'analisi
	//    grammaticale.
	// sample_formulas.xlsx (creato con un piccolo script Python, ZIP
	// minimo scritto a mano sullo stesso modello di sample_numfmt.xlsx
	// -- nessun Excel/LibreOffice disponibile per generarlo, stesso
	// principio delle altre fixture di questo file) ha tre formule reali
	// con virgole: IF/VLOOKUP/IFERROR.
	{
		BFile formulasFile("tests/sample_formulas.xlsx", B_READ_ONLY);
		Check(formulasFile.InitCheck() == B_OK, "apertura di tests/sample_formulas.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&formulasFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_formulas.xlsx");

		formulasFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&formulasFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_formulas.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_formulas.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			// Le celle si riportano in un CContainer vero (non solo
			// bytewise) cosi' si puo' davvero CalcCell/GetValue,
			// esattamente come farebbe MainWindow::OpenFile dopo aver
			// letto lo stesso ASCD con LoadASCD.
			CContainer &doc = *new CContainer(NULL, NULL);

			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
			pos += 1; // "kind" per cella (Fase 15, versione 2 del formato ASCD)
				if (pos + (size_t)len > ascdLen)
					break;
				std::string text((const char *)ascdData + pos, len);
				pos += len;
				cell loc(col, row);
				// Nessun separatore esplicito qui, a differenza
				// dell'import XLSX sopra: questo testo arriva dal byte
				// stream ASCD (scritto da WriteASCD/GetCellFormula/
				// UnMangle, che usa sempre gListSeparator -- il default
				// ';' del motore, non ',' come XLSX), quindi va riletto
				// con lo stesso separatore predefinito, esattamente
				// come fa LoadASCD nella vera app.
				TryToParseString(text.c_str(), loc, &doc, false);
			}

			doc.CalcCell(cell(4, 1)); // D1
			doc.CalcCell(cell(4, 2)); // D2
			doc.CalcCell(cell(4, 3)); // D3

			Value v;
			doc.GetValue(cell(4, 1), v);
			Check(v.fType == eTextData && strcmp((const char *)v, "falso") == 0,
				"D1 (IF(C1<>2,\"vero\",\"falso\") con C1=2) calcola \"falso\" dopo il giro completo");

			doc.GetValue(cell(4, 2), v);
			Check(v.fType == eTextData && strcmp((const char *)v, "due") == 0,
				"D2 (VLOOKUP(2,A1:B3,2,0) con quattro argomenti) calcola \"due\" dopo il giro completo");

			doc.GetValue(cell(4, 3), v);
			Check(v.fType == eNumData && (double)v == 99.0,
				"D3 (IFERROR(1/0,99), nome standard Excel) calcola 99 dopo il giro completo");

			doc.Release();
		}
	}

	// Esportazione dei grafici incorporati verso XLSX (Fase 24): prima
	// di questo lavoro un grafico Atomo123 spariva del tutto
	// esportando in .xlsx (nessuna parte xl/charts/xl/drawings mai
	// scritta). Due scenari: un grafico a barre a UNA serie senza
	// titolo (A1:B3, etichetta+valore), e uno a torta CON titolo,
	// entrambi verificati aprendo il vero file XLSX prodotto come
	// archivio ZIP e leggendo xl/charts/chart1.xml/xl/drawings/
	// drawing1.xml al loro interno -- non solo che il round-trip
	// ASCD->XLSX->ASCD conservi i dati (gia' verificato sopra per le
	// celle), ma che le parti OOXML del grafico vero e proprio
	// esistano e contengano i valori giusti.
	{
		CContainer &chartDoc = *new CContainer(NULL, NULL);
		TryToParseString("Gen", cell(1, 1), &chartDoc, true); // A1
		TryToParseString("10", cell(2, 1), &chartDoc, true);  // B1
		TryToParseString("Feb", cell(1, 2), &chartDoc, true); // A2
		TryToParseString("20", cell(2, 2), &chartDoc, true);  // B2
		TryToParseString("Mar", cell(1, 3), &chartDoc, true); // A3
		TryToParseString("30", cell(2, 3), &chartDoc, true);  // B3

		BMallocIO chartAscdIn;
		status_t chartSaveErr = WriteASCDWithChartForTest(&chartDoc,
			1, 1, 2, 3,			// dataRange A1:B3
			100, 100, 500, 400,	// frame (pixel)
			0, "",					// tipo barre, nessun titolo
			&chartAscdIn);
		Check(chartSaveErr == B_OK, "preparazione dell'ASCD di prova con un grafico riesce");
		chartDoc.Release();

		chartAscdIn.Seek(0, SEEK_SET);
		translator_info chartInfo;
		err = translator->Identify(&chartAscdIn, NULL, NULL, &chartInfo, kAtomoXlsxFormat);
		Check(err == B_OK && chartInfo.type == kAtomoNativeFormat,
			"Identify riconosce l'ASCD di prova con un grafico");

		chartAscdIn.Seek(0, SEEK_SET);
		BMallocIO chartXlsxOut;
		err = translator->Translate(&chartAscdIn, &chartInfo, NULL, kAtomoXlsxFormat, &chartXlsxOut);
		Check(err == B_OK, "Translate ASCD (con un grafico) -> XLSX riesce");

		chartXlsxOut.Seek(0, SEEK_SET);
		CZipReader zip;
		Check(zip.Open(&chartXlsxOut), "il file XLSX con un grafico e' un vero archivio ZIP leggibile");

		Check(zip.HasEntry("xl/drawings/drawing1.xml"),
			"il file XLSX contiene xl/drawings/drawing1.xml (il grafico e' davvero ancorato al foglio)");
		Check(zip.HasEntry("xl/charts/chart1.xml"),
			"il file XLSX contiene xl/charts/chart1.xml (la definizione vera del grafico)");
		Check(zip.HasEntry("xl/worksheets/_rels/sheet1.xml.rels"),
			"il file XLSX collega il foglio al drawing tramite un vero .rels");

		std::vector<unsigned char> sheetXmlBytes;
		if (zip.ReadEntry("xl/worksheets/sheet1.xml", sheetXmlBytes))
		{
			std::string sheetXml((const char*)&sheetXmlBytes[0], sheetXmlBytes.size());
			Check(sheetXml.find("<drawing r:id=\"rId1\"/>") != std::string::npos,
				"xl/worksheets/sheet1.xml referenzia il drawing (<drawing r:id=\"rId1\"/>)");
		}
		else
			Check(false, "xl/worksheets/sheet1.xml si legge dall'archivio");

		std::vector<unsigned char> chartXmlBytes;
		if (zip.ReadEntry("xl/charts/chart1.xml", chartXmlBytes))
		{
			std::string chartXml((const char*)&chartXmlBytes[0], chartXmlBytes.size());
			Check(chartXml.find("<c:barChart>") != std::string::npos,
				"chart1.xml e' un grafico a barre (<c:barChart>), come richiesto");
			Check(chartXml.find("Foglio1!$A$1:$A$3") != std::string::npos,
				"chart1.xml referenzia le categorie vere (Foglio1!$A$1:$A$3)");
			Check(chartXml.find("Foglio1!$B$1:$B$3") != std::string::npos,
				"chart1.xml referenzia i valori veri (Foglio1!$B$1:$B$3)");
			Check(chartXml.find("<c:v>Gen</c:v>") != std::string::npos
					&& chartXml.find("<c:v>Feb</c:v>") != std::string::npos
					&& chartXml.find("<c:v>Mar</c:v>") != std::string::npos,
				"chart1.xml ha in cache le tre etichette vere (Gen/Feb/Mar)");
			Check(chartXml.find("<c:v>10</c:v>") != std::string::npos
					&& chartXml.find("<c:v>20</c:v>") != std::string::npos
					&& chartXml.find("<c:v>30</c:v>") != std::string::npos,
				"chart1.xml ha in cache i tre valori veri (10/20/30)");
			Check(chartXml.find("<c:autoTitleDeleted val=\"1\"/>") != std::string::npos,
				"chart1.xml non ha nessun titolo (autoTitleDeleted, come richiesto)");
			// Un grafico a una sola serie non ha bisogno di legenda,
			// stesso principio gia' seguito da DrawBarChart nell'app.
			Check(chartXml.find("<c:legend>") == std::string::npos,
				"chart1.xml (una sola serie) non ha nessuna legenda");
		}
		else
			Check(false, "xl/charts/chart1.xml si legge dall'archivio");

		// Round-trip completo: il file XLSX appena scritto (dati +
		// grafico) si riapre ancora correttamente con lo stesso
		// translator, stessa identica verifica gia' fatta per le sole
		// celle piu' sopra -- un grafico incorporato non deve rompere
		// l'importazione dei dati.
		chartXlsxOut.Seek(0, SEEK_SET);
		translator_info chartReimportInfo;
		err = translator->Identify(&chartXlsxOut, NULL, NULL, &chartReimportInfo, 0);
		Check(err == B_OK && chartReimportInfo.type == kAtomoXlsxFormat,
			"il file XLSX con un grafico si riconosce ancora come XLSX valido rileggendolo");

		chartXlsxOut.Seek(0, SEEK_SET);
		BMallocIO chartAscdOut;
		err = translator->Translate(&chartXlsxOut, &chartReimportInfo, NULL, kAtomoNativeFormat, &chartAscdOut);
		Check(err == B_OK, "il file XLSX con un grafico si rilegge correttamente (round-trip)");

		// Fase 25 (importazione dei grafici): oltre a rileggersi senza
		// errori, il grafico incorporato deve davvero riapparire
		// nell'ASCD prodotto, con lo stesso dataRange/tipo con cui e'
		// stato esportato sopra -- non silenziosamente perso, e non
		// scambiato per un anchor assoluto non riconosciuto (vedi il
		// commento su isAbsolute in DrawingPic).
		const unsigned char* chartAscdData = NULL;
		size_t chartAscdLen = 0;
		bool chartUnwrapped = UnwrapFirstSheet((const unsigned char*)chartAscdOut.Buffer(),
			chartAscdOut.BufferLength(), &chartAscdData, &chartAscdLen);
		Check(chartUnwrapped, "il round-trip del grafico produce anch'esso una cartella ASCB valida");

		int16 rtLeft = 0, rtTop = 0, rtRight = 0, rtBottom = 0;
		int8 rtType = -1;
		std::string rtTitle;
		float rtFrame[4] = { 0, 0, 0, 0 };
		bool chartRead = chartUnwrapped && ReadFirstChartForTest(chartAscdData, chartAscdLen,
			&rtLeft, &rtTop, &rtRight, &rtBottom, &rtType, &rtTitle, rtFrame);
		Check(chartRead, "il grafico incorporato (UNO, con tutte le sezioni intermedie allineate) "
			"riappare dopo il round-trip XLSX -> ASCD");
		Check(chartRead && rtLeft == 1 && rtTop == 1 && rtRight == 2 && rtBottom == 3,
			"il grafico riletto punta ancora ad A1:B3, lo stesso dataRange con cui e' stato esportato");
		Check(chartRead && rtType == 0,
			"il tipo di grafico riletto e' ancora \"barre\" (0), lo stesso con cui e' stato esportato");
		Check(chartRead && rtTitle.empty(),
			"il grafico riletto non ha titolo, come nell'originale (autoTitleDeleted)");
		// Il grafico e' stato esportato con <xdr:absoluteAnchor><xdr:pos
		// x=".." y=".."/><xdr:ext cx=".." cy=".."/>...<xdr:graphicFrame>
		// <xdr:xfrm><a:ext cx="0" cy="0"/>...: un bug reale (scoperto
		// mentre si costruiva questo stesso test) confondeva il primo
		// <xdr:ext> (la dimensione vera) con quello ZERO annidato dentro
		// xfrm, azzerando extCxEmu/extCyEmu -- il frame ricostruito qui
		// deve essere quello ESPORTATO (100,100,500,400), non un
		// ripiego indovinato per coincidenza (400x300, vedi il commento
		// gemello sulla torta piu' sotto per un caso che NON coincide).
		Check(chartRead && (int)rtFrame[0] == 100 && (int)rtFrame[1] == 100
				&& (int)rtFrame[2] == 500 && (int)rtFrame[3] == 400,
			"il frame del grafico riletto (100,100,500,400) e' quello vero, "
			"non il ripiego predefinito (avrebbe coinciso per caso su questo grafico)");
	}

	// Stesso scenario, ma una torta CON titolo (Assunzioni: A1:B3 e'
	// comunque a due colonne, quindi resta a una serie anche per la
	// torta -- vedi il dispatch "columnCount > 2 && type != ePieChart"
	// in SheetView::Draw, replicato in BuildChartXml).
	{
		CContainer &pieDoc = *new CContainer(NULL, NULL);
		TryToParseString("Rosso", cell(1, 1), &pieDoc, true);
		TryToParseString("40", cell(2, 1), &pieDoc, true);
		TryToParseString("Blu", cell(1, 2), &pieDoc, true);
		TryToParseString("60", cell(2, 2), &pieDoc, true);

		BMallocIO pieAscdIn;
		status_t pieSaveErr = WriteASCDWithChartForTest(&pieDoc,
			1, 1, 2, 2,
			0, 0, 300, 300,
			2, "Distribuzione colori", // tipo torta, con titolo
			&pieAscdIn);
		Check(pieSaveErr == B_OK, "preparazione dell'ASCD di prova con una torta con titolo riesce");
		pieDoc.Release();

		pieAscdIn.Seek(0, SEEK_SET);
		translator_info pieInfo;
		translator->Identify(&pieAscdIn, NULL, NULL, &pieInfo, kAtomoXlsxFormat);

		pieAscdIn.Seek(0, SEEK_SET);
		BMallocIO pieXlsxOut;
		err = translator->Translate(&pieAscdIn, &pieInfo, NULL, kAtomoXlsxFormat, &pieXlsxOut);
		Check(err == B_OK, "Translate ASCD (con una torta con titolo) -> XLSX riesce");

		pieXlsxOut.Seek(0, SEEK_SET);
		CZipReader pieZip;
		pieZip.Open(&pieXlsxOut);

		std::vector<unsigned char> pieChartBytes;
		if (pieZip.ReadEntry("xl/charts/chart1.xml", pieChartBytes))
		{
			std::string pieXml((const char*)&pieChartBytes[0], pieChartBytes.size());
			Check(pieXml.find("<c:pieChart>") != std::string::npos,
				"chart1.xml di una torta e' davvero <c:pieChart>, non barre/linee");
			Check(pieXml.find("<a:t>Distribuzione colori</a:t>") != std::string::npos,
				"chart1.xml conserva il titolo scelto (\"Distribuzione colori\")");
			Check(pieXml.find("<c:legend>") != std::string::npos,
				"chart1.xml di una torta ha una legenda (le fette si distinguono per colore)");
		}
		else
			Check(false, "xl/charts/chart1.xml (torta) si legge dall'archivio");

		// Fase 25: stesso round-trip di importazione della sezione
		// precedente, qui per verificare che tipo TORTA e titolo
		// sopravvivano entrambi, non solo il dataRange.
		pieXlsxOut.Seek(0, SEEK_SET);
		translator_info pieReimportInfo;
		err = translator->Identify(&pieXlsxOut, NULL, NULL, &pieReimportInfo, 0);
		Check(err == B_OK && pieReimportInfo.type == kAtomoXlsxFormat,
			"il file XLSX della torta con titolo si riconosce ancora come XLSX valido rileggendolo");

		pieXlsxOut.Seek(0, SEEK_SET);
		BMallocIO pieAscdOut;
		err = translator->Translate(&pieXlsxOut, &pieReimportInfo, NULL, kAtomoNativeFormat, &pieAscdOut);
		Check(err == B_OK, "il file XLSX della torta con titolo si rilegge correttamente (round-trip)");

		const unsigned char* pieAscdData = NULL;
		size_t pieAscdLen = 0;
		bool pieUnwrapped = UnwrapFirstSheet((const unsigned char*)pieAscdOut.Buffer(),
			pieAscdOut.BufferLength(), &pieAscdData, &pieAscdLen);
		Check(pieUnwrapped, "il round-trip della torta produce anch'esso una cartella ASCB valida");

		int16 pieRtLeft = 0, pieRtTop = 0, pieRtRight = 0, pieRtBottom = 0;
		int8 pieRtType = -1;
		std::string pieRtTitle;
		float pieRtFrame[4] = { 0, 0, 0, 0 };
		bool pieChartRead = pieUnwrapped && ReadFirstChartForTest(pieAscdData, pieAscdLen,
			&pieRtLeft, &pieRtTop, &pieRtRight, &pieRtBottom, &pieRtType, &pieRtTitle, pieRtFrame);
		Check(pieChartRead && pieRtLeft == 1 && pieRtTop == 1 && pieRtRight == 2 && pieRtBottom == 2,
			"la torta riletta punta ancora ad A1:B2, lo stesso dataRange con cui e' stata esportata");
		Check(pieChartRead && pieRtType == 2,
			"il tipo di grafico riletto e' ancora \"torta\" (2), lo stesso con cui e' stato esportato");
		Check(pieChartRead && pieRtTitle == "Distribuzione colori",
			"il titolo riletto (\"Distribuzione colori\") e' ancora quello con cui e' stato esportato");
		// La torta e' stata esportata con frame (0,0,300,300) -- 300x300,
		// DIVERSO dal ripiego predefinito 400x300 usato quando l'ancoraggio
		// non da' una dimensione esplicita: a differenza del grafico a
		// barre sopra (dove il ripiego coincideva per caso), qui una
		// dimensione sbagliata si vede subito. Questo e' l'assert che ha
		// davvero smascherato il bug xdr:ext/xfrm descritto sopra.
		Check(pieChartRead && (int)pieRtFrame[0] == 0 && (int)pieRtFrame[1] == 0
				&& (int)pieRtFrame[2] == 300 && (int)pieRtFrame[3] == 300,
			"il frame della torta riletta (0,0,300,300) e' quello vero, non il ripiego predefinito (400x300)");
	}

	// Importazione di un vero file XLSX in stile Excel (Fase 25):
	// tests/sample_chart_import.xlsx e' costruito a mano (non con
	// questo stesso translator, a differenza dei round-trip sopra) con
	// <xdr:twoCellAnchor>/<xdr:from>/<xdr:to> -- l'ancoraggio che Excel
	// scrive DAVVERO, mai <xdr:absoluteAnchor> (quello e' solo cio' che
	// scrive l'export di QUESTA app, gia' verificato sopra). Due
	// grafici sullo stesso foglio (A1:B3, Gen/Feb/Mar + 10/20/30):
	// chart1.xml e' un grafico a barre (riconosciuto), chart2.xml e'
	// un grafico ad area (NON fra i 3 tipi disegnati da questa app) --
	// verifica sia che il primo arrivi fino a ChartObject sia che il
	// secondo sia segnalato come "non implementato" invece di
	// sparire silenziosamente o rompere l'importazione degli altri dati.
	{
		BFile importFile("tests/sample_chart_import.xlsx", B_READ_ONLY);
		Check(importFile.InitCheck() == B_OK, "apertura di tests/sample_chart_import.xlsx riuscita");

		translator_info importInfo;
		err = translator->Identify(&importFile, NULL, NULL, &importInfo, 0);
		Check(err == B_OK && importInfo.type == kAtomoXlsxFormat,
			"Identify riconosce sample_chart_import.xlsx");

		importFile.Seek(0, SEEK_SET);
		BMallocIO importOut;
		BMessage importExtension;
		err = translator->Translate(&importFile, &importInfo, &importExtension,
			kAtomoNativeFormat, &importOut);
		Check(err == B_OK, "Translate di sample_chart_import.xlsx riesce");

		const unsigned char* importAscdData = NULL;
		size_t importAscdLen = 0;
		bool importUnwrapped = UnwrapFirstSheet((const unsigned char*)importOut.Buffer(),
			importOut.BufferLength(), &importAscdData, &importAscdLen);
		Check(importUnwrapped, "l'output di Translate di sample_chart_import.xlsx e' un ASCD valido");

		int16 impLeft = 0, impTop = 0, impRight = 0, impBottom = 0;
		int8 impType = -1;
		std::string impTitle;
		bool importChartRead = importUnwrapped && ReadFirstChartForTest(importAscdData, importAscdLen,
			&impLeft, &impTop, &impRight, &impBottom, &impType, &impTitle);
		Check(importChartRead, "il grafico a barre in stile Excel (xdr:twoCellAnchor) arriva fino all'ASCD");
		Check(importChartRead && impLeft == 1 && impTop == 1 && impRight == 2 && impBottom == 3,
			"il grafico a barre importato punta ad A1:B3, ricostruito dai riferimenti veri di chart1.xml");
		Check(importChartRead && impType == 0,
			"il grafico importato e' di tipo \"barre\" (0), <c:barChart>/<c:barDir val=\"col\"/> di chart1.xml");

		// Il grafico ad area (chart2.xml) non deve essere il SECONDO
		// grafico nell'ASCD (solo 1 record atteso, non 2): la sua
		// mancata implementazione non deve ne' comparire come un
		// grafico fasullo ne' rompere il primo.
		type_code msgType;
		int32 unsupportedCount = 0;
		importExtension.GetInfo("atomo:unsupportedChart", &msgType, &unsupportedCount);
		Check(unsupportedCount == 1,
			"esattamente un grafico non implementato segnalato tramite \"extension\" (l'area di chart2.xml)");
		const char* unsupportedName = NULL;
		bool foundAreaName = unsupportedCount == 1
			&& importExtension.FindString("atomo:unsupportedChart", 0, &unsupportedName) == B_OK
			&& std::string(unsupportedName).find("area") != std::string::npos;
		Check(foundAreaName,
			"il nome del grafico non implementato menziona \"areaChart\", non generico");
	}

	// Formula array legacy (CSE, Ctrl+Maiusc+Invio): in un file XLSX
	// vero, <f t="array" ref="B1:B2">FORMULA</f> compare SOLO sulla
	// cella in alto a sinistra dell'intervallo (B1) -- B2 non ha
	// affatto un <f> proprio, solo un <v> con il valore congelato che
	// Excel aveva calcolato l'ultima volta. Prima della correzione,
	// B2 veniva importata come quel valore statico invece che come la
	// STESSA formula di B1 (nessuno spostamento di riferimenti
	// relativi per un'array formula, a differenza di una formula
	// condivisa). Il valore <v> di entrambe le celle e' deliberatamente
	// SBAGLIATO (999) per dimostrare che il motore ricalcola davvero
	// la formula importata invece di fidarsi della cache di Excel.
	{
		static const char kArrContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kArrRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kArrWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kArrWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kArrSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData>"
			"<row r=\"1\"><c r=\"A1\"><v>10</v></c>"
			"<c r=\"B1\"><f t=\"array\" ref=\"B1:B2\">A1+A2</f><v>999</v></c></row>"
			"<row r=\"2\"><c r=\"A2\"><v>20</v></c>"
			"<c r=\"B2\"><v>999</v></c></row>"
			"</sheetData>"
			"</worksheet>\n";

		BMallocIO arrXlsx;
		CZipWriter arrZip;
		arrZip.Begin(&arrXlsx);
		arrZip.AddEntry("[Content_Types].xml", kArrContentTypes, strlen(kArrContentTypes));
		arrZip.AddEntry("_rels/.rels", kArrRootRels, strlen(kArrRootRels));
		arrZip.AddEntry("xl/workbook.xml", kArrWorkbook, strlen(kArrWorkbook));
		arrZip.AddEntry("xl/_rels/workbook.xml.rels", kArrWorkbookRels, strlen(kArrWorkbookRels));
		arrZip.AddEntry("xl/worksheets/sheet1.xml", kArrSheet, strlen(kArrSheet));
		Check(arrZip.Close(), "costruzione del file XLSX di prova per la formula array riuscita");

		arrXlsx.Seek(0, SEEK_SET);
		translator_info arrInfo;
		err = translator->Identify(&arrXlsx, NULL, NULL, &arrInfo, 0);
		Check(err == B_OK && arrInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova per la formula array");

		arrXlsx.Seek(0, SEEK_SET);
		BMallocIO arrAscdOut;
		err = translator->Translate(&arrXlsx, &arrInfo, NULL, kAtomoNativeFormat, &arrAscdOut);
		Check(err == B_OK, "Translate del file di prova per la formula array riesce");

		const unsigned char* arrAscdData = NULL;
		size_t arrAscdLen = 0;
		bool arrUnwrapped = UnwrapFirstSheet((const unsigned char*)arrAscdOut.Buffer(),
			arrAscdOut.BufferLength(), &arrAscdData, &arrAscdLen);
		Check(arrUnwrapped, "l'output di Translate del file di prova per la formula array e' un ASCD valido");

		if (arrUnwrapped && arrAscdLen > 12 && memcmp(arrAscdData, "ASCD", 4) == 0)
		{
			int32 arrCount;
			memcpy(&arrCount, arrAscdData + 8, 4);

			CContainer& arrDoc = *new CContainer(NULL, NULL);
			bool foundB1Formula = false, foundB2Formula = false;

			size_t pos = 12;
			for (int32 i = 0; i < arrCount && pos + 8 <= arrAscdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, arrAscdData + pos, 2); pos += 2;
				memcpy(&col, arrAscdData + pos, 2); pos += 2;
				memcpy(&len, arrAscdData + pos, 4); pos += 4;
				pos += 1; // "kind" per cella (versione 2 del formato ASCD)
				if (pos + (size_t)len > arrAscdLen)
					break;

				std::string text((const char*)arrAscdData + pos, len);
				pos += len;

				cell loc(col, row);
				TryToParseString(text.c_str(), loc, &arrDoc, true);

				if (row == 1 && col == 2 && text.find("A1") != std::string::npos
					&& text.find("A2") != std::string::npos)
					foundB1Formula = true;
				if (row == 2 && col == 2 && text.find("A1") != std::string::npos
					&& text.find("A2") != std::string::npos)
					foundB2Formula = true;
			}

			Check(foundB1Formula,
				"B1 (ancora della formula array, <f t=\"array\"> col testo) importata come formula");
			Check(foundB2Formula,
				"B2 (nell'intervallo dell'array ma senza <f> proprio) importata come formula, "
				"non come il valore congelato 999");

			cell b1(2, 1), b2(2, 2);
			arrDoc.CalcCell(b1);
			arrDoc.CalcCell(b2);
			Value v1, v2;
			arrDoc.GetValue(b1, v1);
			arrDoc.GetValue(b2, v2);
			Check((double)v1 == 30.0,
				"il motore ricalcola B1 e ottiene 30 (10+20), non il valore congelato 999");
			Check((double)v2 == 30.0,
				"il motore ricalcola B2 e ottiene 30 (10+20), non il valore congelato 999");
		}
	}

	// Formule condivise (<f t="shared" si="N"/>): a differenza delle
	// formule array appena verificate, qui i riferimenti RELATIVI
	// devono spostarsi in base alla posizione di ogni cella -- un
	// riferimento assoluto ($A$1) invece resta fisso. Due gruppi
	// indipendenti nello stesso foglio (si="0" e si="1", come farebbe
	// Excel vero con due trascinamenti separati) per verificare
	// entrambi i casi insieme: colonna B (solo riferimenti relativi,
	// B1:B3 = A1:A3 * 2) e colonna C (un riferimento fisso piu' uno
	// relativo nella stessa formula, C1:C3 = $A$1 + A1:A3). Valore in
	// cache deliberatamente sbagliato (999) su ogni cella, come per il
	// test delle formule array sopra.
	{
		static const char kShContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kShRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kShWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kShWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kShSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData>"
			"<row r=\"1\"><c r=\"A1\"><v>5</v></c>"
			"<c r=\"B1\"><f t=\"shared\" ref=\"B1:B3\" si=\"0\">A1*2</f><v>999</v></c>"
			"<c r=\"C1\"><f t=\"shared\" ref=\"C1:C3\" si=\"1\">$A$1+A1</f><v>999</v></c></row>"
			"<row r=\"2\"><c r=\"A2\"><v>10</v></c>"
			"<c r=\"B2\"><f t=\"shared\" si=\"0\"/><v>999</v></c>"
			"<c r=\"C2\"><f t=\"shared\" si=\"1\"/><v>999</v></c></row>"
			"<row r=\"3\"><c r=\"A3\"><v>15</v></c>"
			"<c r=\"B3\"><f t=\"shared\" si=\"0\"/><v>999</v></c>"
			"<c r=\"C3\"><f t=\"shared\" si=\"1\"/><v>999</v></c></row>"
			"</sheetData>"
			"</worksheet>\n";

		BMallocIO shXlsx;
		CZipWriter shZip;
		shZip.Begin(&shXlsx);
		shZip.AddEntry("[Content_Types].xml", kShContentTypes, strlen(kShContentTypes));
		shZip.AddEntry("_rels/.rels", kShRootRels, strlen(kShRootRels));
		shZip.AddEntry("xl/workbook.xml", kShWorkbook, strlen(kShWorkbook));
		shZip.AddEntry("xl/_rels/workbook.xml.rels", kShWorkbookRels, strlen(kShWorkbookRels));
		shZip.AddEntry("xl/worksheets/sheet1.xml", kShSheet, strlen(kShSheet));
		Check(shZip.Close(), "costruzione del file XLSX di prova per le formule condivise riuscita");

		shXlsx.Seek(0, SEEK_SET);
		translator_info shInfo;
		err = translator->Identify(&shXlsx, NULL, NULL, &shInfo, 0);
		Check(err == B_OK && shInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova per le formule condivise");

		shXlsx.Seek(0, SEEK_SET);
		BMallocIO shAscdOut;
		err = translator->Translate(&shXlsx, &shInfo, NULL, kAtomoNativeFormat, &shAscdOut);
		Check(err == B_OK, "Translate del file di prova per le formule condivise riesce");

		const unsigned char* shAscdData = NULL;
		size_t shAscdLen = 0;
		bool shUnwrapped = UnwrapFirstSheet((const unsigned char*)shAscdOut.Buffer(),
			shAscdOut.BufferLength(), &shAscdData, &shAscdLen);
		Check(shUnwrapped, "l'output di Translate del file di prova per le formule condivise e' un ASCD valido");

		if (shUnwrapped && shAscdLen > 12 && memcmp(shAscdData, "ASCD", 4) == 0)
		{
			int32 shCount;
			memcpy(&shCount, shAscdData + 8, 4);

			CContainer& shDoc = *new CContainer(NULL, NULL);
			bool foundB2Formula = false, foundB3Formula = false;
			bool foundC2Formula = false, foundC3Formula = false;

			size_t pos = 12;
			for (int32 i = 0; i < shCount && pos + 8 <= shAscdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, shAscdData + pos, 2); pos += 2;
				memcpy(&col, shAscdData + pos, 2); pos += 2;
				memcpy(&len, shAscdData + pos, 4); pos += 4;
				pos += 1; // "kind" per cella (versione 2 del formato ASCD)
				if (pos + (size_t)len > shAscdLen)
					break;

				std::string text((const char*)shAscdData + pos, len);
				pos += len;

				cell loc(col, row);
				TryToParseString(text.c_str(), loc, &shDoc, true);

				// B2/B3 (colonna 2): la formula ricostruita deve
				// riferirsi alla riga PROPRIA (A2/A3), non sempre ad A1
				// come l'ancora -- e' proprio questo lo spostamento dei
				// riferimenti relativi che si sta verificando.
				if (row == 2 && col == 2 && text.find("A2") != std::string::npos)
					foundB2Formula = true;
				if (row == 3 && col == 2 && text.find("A3") != std::string::npos)
					foundB3Formula = true;
				// C2/C3 (colonna 3): devono contenere SIA il riferimento
				// fisso ($A$1, sempre A1) SIA quello relativo spostato
				// (A2/A3) nella stessa formula.
				if (row == 2 && col == 3 && text.find("$A$1") != std::string::npos
					&& text.find("A2") != std::string::npos)
					foundC2Formula = true;
				if (row == 3 && col == 3 && text.find("$A$1") != std::string::npos
					&& text.find("A3") != std::string::npos)
					foundC3Formula = true;
			}

			Check(foundB2Formula,
				"B2 (formula condivisa, si=\"0\" vuota) importata riferendosi ad A2, non A1 come l'ancora");
			Check(foundB3Formula,
				"B3 (formula condivisa, si=\"0\" vuota) importata riferendosi ad A3, non A1 come l'ancora");
			Check(foundC2Formula,
				"C2 (formula condivisa mista) tiene fisso $A$1 e sposta il riferimento relativo ad A2");
			Check(foundC3Formula,
				"C3 (formula condivisa mista) tiene fisso $A$1 e sposta il riferimento relativo ad A3");

			cell b1(2, 1), b2(2, 2), b3(2, 3), c1(3, 1), c2(3, 2), c3(3, 3);
			shDoc.CalcCell(b1); shDoc.CalcCell(b2); shDoc.CalcCell(b3);
			shDoc.CalcCell(c1); shDoc.CalcCell(c2); shDoc.CalcCell(c3);
			Value vb1, vb2, vb3, vc1, vc2, vc3;
			shDoc.GetValue(b1, vb1); shDoc.GetValue(b2, vb2); shDoc.GetValue(b3, vb3);
			shDoc.GetValue(c1, vc1); shDoc.GetValue(c2, vc2); shDoc.GetValue(c3, vc3);
			Check((double)vb1 == 10.0, "B1 (ancora, A1*2) ricalcola 10, non il valore congelato 999");
			Check((double)vb2 == 20.0, "B2 (condivisa, A2*2 dopo lo spostamento) ricalcola 20, non 999");
			Check((double)vb3 == 30.0, "B3 (condivisa, A3*2 dopo lo spostamento) ricalcola 30, non 999");
			Check((double)vc1 == 10.0, "C1 (ancora, $A$1+A1) ricalcola 10, non il valore congelato 999");
			Check((double)vc2 == 15.0, "C2 (condivisa, $A$1+A2 dopo lo spostamento) ricalcola 15, non 999");
			Check((double)vc3 == 20.0, "C3 (condivisa, $A$1+A3 dopo lo spostamento) ricalcola 20, non 999");
		}
	}

	// Named ranges (<definedNames> in xl/workbook.xml), the last item
	// of Tier 1 in the "100% XLSX standard compatibility" plan: a
	// workbook-scoped name (no localSheetId) should resolve on this
	// sheet's document after import, while Excel's own reserved
	// "_xlnm.Print_Area" bookkeeping name should NOT show up as a
	// resolvable name (see ApplyDefinedNames in XlsxTranslator.cpp).
	{
		static const char kNameContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kNameRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kNameWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"<definedNames>"
			"<definedName name=\"Budget\">Foglio1!$A$1:$A$2</definedName>"
			"<definedName name=\"_xlnm.Print_Area\">Foglio1!$A$1:$A$2</definedName>"
			"</definedNames>\n"
			"</workbook>\n";
		static const char kNameWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kNameSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData>"
			"<row r=\"1\"><c r=\"A1\"><v>5</v></c></row>"
			"<row r=\"2\"><c r=\"A2\"><v>10</v></c></row>"
			"</sheetData>"
			"</worksheet>\n";

		BMallocIO nameXlsx;
		CZipWriter nameZip;
		nameZip.Begin(&nameXlsx);
		nameZip.AddEntry("[Content_Types].xml", kNameContentTypes, strlen(kNameContentTypes));
		nameZip.AddEntry("_rels/.rels", kNameRootRels, strlen(kNameRootRels));
		nameZip.AddEntry("xl/workbook.xml", kNameWorkbook, strlen(kNameWorkbook));
		nameZip.AddEntry("xl/_rels/workbook.xml.rels", kNameWorkbookRels, strlen(kNameWorkbookRels));
		nameZip.AddEntry("xl/worksheets/sheet1.xml", kNameSheet, strlen(kNameSheet));
		Check(nameZip.Close(), "costruzione del file XLSX di prova con <definedNames> riuscita");

		nameXlsx.Seek(0, SEEK_SET);
		translator_info nameInfo;
		err = translator->Identify(&nameXlsx, NULL, NULL, &nameInfo, 0);
		Check(err == B_OK && nameInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con <definedNames>");

		nameXlsx.Seek(0, SEEK_SET);
		BMallocIO nameAscdOut;
		err = translator->Translate(&nameXlsx, &nameInfo, NULL, kAtomoNativeFormat, &nameAscdOut);
		Check(err == B_OK, "Translate del file di prova con <definedNames> riesce");

		const unsigned char* nameAscdData = NULL;
		size_t nameAscdLen = 0;
		bool nameUnwrapped = UnwrapFirstSheet((const unsigned char*)nameAscdOut.Buffer(),
			nameAscdOut.BufferLength(), &nameAscdData, &nameAscdLen);
		Check(nameUnwrapped, "l'output di Translate del file di prova con <definedNames> e' un ASCD valido");

		if (nameUnwrapped && nameAscdLen > 12 && memcmp(nameAscdData, "ASCD", 4) == 0)
		{
			CContainer& importedDoc = *new CContainer(NULL, NULL);
			int32 importedCount;
			memcpy(&importedCount, nameAscdData + 8, 4);

			size_t pos = 12;
			for (int32 i = 0; i < importedCount && pos + 8 <= nameAscdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, nameAscdData + pos, 2); pos += 2;
				memcpy(&col, nameAscdData + pos, 2); pos += 2;
				memcpy(&len, nameAscdData + pos, 4); pos += 4;
				pos += 1; // "kind" per cella (versione 2 del formato ASCD)
				if (pos + (size_t)len > nameAscdLen)
					break;
				std::string text((const char*)nameAscdData + pos, len);
				pos += len;
				TryToParseString(text.c_str(), cell(col, row), &importedDoc, true);
			}

			// "pos" e' ferma alla fine delle celle: il nome e' gia'
			// stato applicato al documento usato DENTRO al translator
			// (ApplyDefinedNames, prima ancora che WriteASCD scrivesse
			// l'ASCD), quindi qui va letta la sezione dei nomi in coda
			// del blocco appena prodotto e riapplicata a "importedDoc"
			// (un documento diverso, ricostruito solo dalle celle sopra).
			bool namesApplied = ApplyNamesFromAscdForTest(nameAscdData, nameAscdLen, pos, &importedDoc);
			Check(namesApplied, "la sezione degli intervalli con nome in coda all'ASCD si legge correttamente");

			range budgetRange = importedDoc.ResolveName("Budget");
			Check(budgetRange.TopLeft() == cell(1, 1) && budgetRange.BotRight() == cell(1, 2),
				"\"Budget\" (nome a livello di cartella, senza localSheetId) importato "
				"da <definedNames> e risolvibile su A1:A2");

			bool printAreaLeaked = true;
			try { importedDoc.ResolveName("_xlnm.Print_Area"); }
			catch (...) { printAreaLeaked = false; }
			Check(!printAreaLeaked,
				"\"_xlnm.Print_Area\" (nome riservato di Excel, non un intervallo con nome vero) "
				"NON compare nella tabella nomi importata");
		}
	}

	// Stesso scenario, direzione opposta (ASCD -> XLSX): un documento
	// con un nome definito esporta un vero <definedName> in
	// xl/workbook.xml, e quel file si rilegge correttamente.
	{
		CContainer& nameExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &nameExportDoc, true);  // A1
		TryToParseString("10", cell(1, 2), &nameExportDoc, true); // A2
		(*nameExportDoc.GetOrCreateNameTable())[CName("Budget")] = range(1, 1, 1, 2);

		BMallocIO nameAscdIn;
		status_t saveErr = WriteASCDWithNameForTest(&nameExportDoc, &nameAscdIn);
		Check(saveErr == B_OK, "preparazione dell'ASCD di prova con un nome definito riesce");
		nameExportDoc.Release();

		nameAscdIn.Seek(0, SEEK_SET);
		translator_info nameExportInfo;
		err = translator->Identify(&nameAscdIn, NULL, NULL, &nameExportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con un nome definito come sorgente per l'export");

		nameAscdIn.Seek(0, SEEK_SET);
		BMallocIO nameXlsxOut;
		err = translator->Translate(&nameAscdIn, &nameExportInfo, NULL, kAtomoXlsxFormat, &nameXlsxOut);
		Check(err == B_OK, "Translate ASCD (con un nome definito) -> XLSX riesce");

		if (err == B_OK)
		{
			nameXlsxOut.Seek(0, SEEK_SET);
			CZipReader nameOutZip;
			Check(nameOutZip.Open(&nameXlsxOut), "il file XLSX esportato con un nome e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> exportedWorkbookXml;
			bool readWorkbook = nameOutZip.ReadEntry("xl/workbook.xml", exportedWorkbookXml);
			Check(readWorkbook, "il file XLSX esportato contiene xl/workbook.xml");

			std::string workbookText(exportedWorkbookXml.begin(), exportedWorkbookXml.end());
			Check(workbookText.find("<definedName name=\"Budget\">Foglio1!$A$1:$A$2</definedName>")
				!= std::string::npos,
				"xl/workbook.xml esportato contiene <definedName name=\"Budget\">Foglio1!$A$1:$A$2</definedName>");

			// Round-trip completo: rileggendo il file appena esportato,
			// il nome deve risolversi di nuovo.
			nameXlsxOut.Seek(0, SEEK_SET);
			translator_info nameReimportInfo;
			err = translator->Identify(&nameXlsxOut, NULL, NULL, &nameReimportInfo, 0);
			Check(err == B_OK && nameReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX esportato con un nome si riconosce ancora come XLSX valido rileggendolo");

			nameXlsxOut.Seek(0, SEEK_SET);
			BMallocIO nameRoundTripAscd;
			err = translator->Translate(&nameXlsxOut, &nameReimportInfo, NULL,
				kAtomoNativeFormat, &nameRoundTripAscd);
			Check(err == B_OK, "il file XLSX esportato con un nome si rilegge correttamente (round-trip)");

			if (err == B_OK)
			{
				const unsigned char* rtData = NULL;
				size_t rtLen = 0;
				bool rtUnwrapped = UnwrapFirstSheet((const unsigned char*)nameRoundTripAscd.Buffer(),
					nameRoundTripAscd.BufferLength(), &rtData, &rtLen);
				Check(rtUnwrapped, "il round-trip del nome produce anch'esso una cartella ASCB valida");

				if (rtUnwrapped && rtLen > 12 && memcmp(rtData, "ASCD", 4) == 0)
				{
					CContainer& rtDoc = *new CContainer(NULL, NULL);
					int32 rtCount;
					memcpy(&rtCount, rtData + 8, 4);

					size_t pos = 12;
					for (int32 i = 0; i < rtCount && pos + 8 <= rtLen; i++)
					{
						int16 row, col;
						int32 len;
						memcpy(&row, rtData + pos, 2); pos += 2;
						memcpy(&col, rtData + pos, 2); pos += 2;
						memcpy(&len, rtData + pos, 4); pos += 4;
						pos += 1;
						if (pos + (size_t)len > rtLen)
							break;
						std::string text((const char*)rtData + pos, len);
						pos += len;
						TryToParseString(text.c_str(), cell(col, row), &rtDoc, true);
					}

					bool rtNamesApplied = ApplyNamesFromAscdForTest(rtData, rtLen, pos, &rtDoc);
					Check(rtNamesApplied,
						"la sezione degli intervalli con nome si legge correttamente anche dopo il giro completo");

					range rtRange = rtDoc.ResolveName("Budget");
					Check(rtRange.TopLeft() == cell(1, 1) && rtRange.BotRight() == cell(1, 2),
						"dopo il giro completo ASCD -> XLSX -> ASCD, \"Budget\" si risolve ancora su A1:A2");
				}
			}
		}
	}

	// Cell comments/notes (<comments> in xl/comments1.xml + the sheet's
	// own _rels), the first item of Tier 2 in the "100% XLSX standard
	// compatibility" plan: a comment on B2 should arrive in the
	// imported document via CContainer::SetComment, not be silently
	// discarded like before this fix.
	{
		static const char kCommentContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"<Override PartName=\"/xl/comments1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.comments+xml\"/>\n"
			"</Types>\n";
		static const char kCommentRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kCommentWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kCommentWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kCommentSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"</worksheet>\n";
		static const char kCommentSheetRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments\" "
			"Target=\"../comments1.xml\"/>\n</Relationships>\n";
		static const char kComment1[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<comments xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
			"<authors><author>Un revisore</author></authors>"
			"<commentList><comment ref=\"B2\" authorId=\"0\">"
			"<text><r><t>Da ricontrollare</t></r></text></comment></commentList></comments>\n";

		BMallocIO commentXlsx;
		CZipWriter commentZip;
		commentZip.Begin(&commentXlsx);
		commentZip.AddEntry("[Content_Types].xml", kCommentContentTypes, strlen(kCommentContentTypes));
		commentZip.AddEntry("_rels/.rels", kCommentRootRels, strlen(kCommentRootRels));
		commentZip.AddEntry("xl/workbook.xml", kCommentWorkbook, strlen(kCommentWorkbook));
		commentZip.AddEntry("xl/_rels/workbook.xml.rels", kCommentWorkbookRels, strlen(kCommentWorkbookRels));
		commentZip.AddEntry("xl/worksheets/sheet1.xml", kCommentSheet, strlen(kCommentSheet));
		commentZip.AddEntry("xl/worksheets/_rels/sheet1.xml.rels", kCommentSheetRels, strlen(kCommentSheetRels));
		commentZip.AddEntry("xl/comments1.xml", kComment1, strlen(kComment1));
		Check(commentZip.Close(), "costruzione del file XLSX di prova con <comments> riuscita");

		commentXlsx.Seek(0, SEEK_SET);
		translator_info commentInfo;
		err = translator->Identify(&commentXlsx, NULL, NULL, &commentInfo, 0);
		Check(err == B_OK && commentInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con <comments>");

		commentXlsx.Seek(0, SEEK_SET);
		BMallocIO commentAscdOut;
		err = translator->Translate(&commentXlsx, &commentInfo, NULL, kAtomoNativeFormat, &commentAscdOut);
		Check(err == B_OK, "Translate del file di prova con <comments> riesce");

		const unsigned char* commentAscdData = NULL;
		size_t commentAscdLen = 0;
		bool commentUnwrapped = UnwrapFirstSheet((const unsigned char*)commentAscdOut.Buffer(),
			commentAscdOut.BufferLength(), &commentAscdData, &commentAscdLen);
		Check(commentUnwrapped, "l'output di Translate del file di prova con <comments> e' un ASCD valido");

		if (commentUnwrapped)
		{
			cell importedCommentCell;
			std::string importedCommentText;
			bool commentRead = ReadFirstCommentFromAscdForTest(commentAscdData, commentAscdLen,
				&importedCommentCell, &importedCommentText);
			Check(commentRead, "la sezione commenti dell'ASCD prodotto si legge correttamente");
			Check(commentRead && importedCommentCell == cell(2, 2),
				"il commento importato e' ancorato a B2, lo stesso riferimento di <comment ref=\"B2\">");
			Check(commentRead && importedCommentText == "Da ricontrollare",
				"il testo del commento importato e' quello vero (\"Da ricontrollare\"), "
				"non piu' scartato in silenzio");
		}
	}

	// Stesso scenario, direzione opposta (ASCD -> XLSX): un documento
	// con un commento su una cella esporta un vero xl/comments1.xml,
	// collegato al foglio tramite i suoi _rels, e quel file si rilegge
	// correttamente.
	{
		CContainer& commentExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &commentExportDoc, true); // A1
		commentExportDoc.SetComment(cell(2, 2), "Da ricontrollare"); // B2

		BMallocIO commentAscdIn;
		status_t commentSaveErr = WriteASCDWithCommentForTest(&commentExportDoc, "B2",
			"Da ricontrollare", &commentAscdIn);
		Check(commentSaveErr == B_OK, "preparazione dell'ASCD di prova con un commento riesce");
		commentExportDoc.Release();

		commentAscdIn.Seek(0, SEEK_SET);
		translator_info commentExportInfo;
		err = translator->Identify(&commentAscdIn, NULL, NULL, &commentExportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con un commento come sorgente per l'export");

		commentAscdIn.Seek(0, SEEK_SET);
		BMallocIO commentXlsxOut;
		err = translator->Translate(&commentAscdIn, &commentExportInfo, NULL, kAtomoXlsxFormat, &commentXlsxOut);
		Check(err == B_OK, "Translate ASCD (con un commento) -> XLSX riesce");

		if (err == B_OK)
		{
			commentXlsxOut.Seek(0, SEEK_SET);
			CZipReader commentOutZip;
			Check(commentOutZip.Open(&commentXlsxOut),
				"il file XLSX esportato con un commento e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> exportedCommentsXml;
			bool readComments = commentOutZip.ReadEntry("xl/comments1.xml", exportedCommentsXml);
			Check(readComments, "il file XLSX esportato contiene xl/comments1.xml");

			std::string commentsText(exportedCommentsXml.begin(), exportedCommentsXml.end());
			Check(commentsText.find("ref=\"B2\"") != std::string::npos
				&& commentsText.find("Da ricontrollare") != std::string::npos,
				"xl/comments1.xml esportato contiene il commento vero, ancorato a B2");

			std::vector<unsigned char> exportedSheetRels;
			bool readSheetRels = commentOutZip.ReadEntry("xl/worksheets/_rels/sheet1.xml.rels",
				exportedSheetRels);
			std::string sheetRelsText(exportedSheetRels.begin(), exportedSheetRels.end());
			Check(readSheetRels && sheetRelsText.find("comments1.xml") != std::string::npos,
				"il foglio esportato si collega a xl/comments1.xml tramite i propri _rels");

			// Round-trip completo: rileggendo il file appena esportato,
			// il commento deve arrivare di nuovo.
			commentXlsxOut.Seek(0, SEEK_SET);
			translator_info commentReimportInfo;
			err = translator->Identify(&commentXlsxOut, NULL, NULL, &commentReimportInfo, 0);
			Check(err == B_OK && commentReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX esportato con un commento si riconosce ancora come XLSX valido rileggendolo");

			commentXlsxOut.Seek(0, SEEK_SET);
			BMallocIO commentRoundTripAscd;
			err = translator->Translate(&commentXlsxOut, &commentReimportInfo, NULL,
				kAtomoNativeFormat, &commentRoundTripAscd);
			Check(err == B_OK, "il file XLSX esportato con un commento si rilegge correttamente (round-trip)");

			if (err == B_OK)
			{
				const unsigned char* rtCommentData = NULL;
				size_t rtCommentLen = 0;
				bool rtCommentUnwrapped = UnwrapFirstSheet((const unsigned char*)commentRoundTripAscd.Buffer(),
					commentRoundTripAscd.BufferLength(), &rtCommentData, &rtCommentLen);
				Check(rtCommentUnwrapped, "il round-trip del commento produce anch'esso una cartella ASCB valida");

				if (rtCommentUnwrapped)
				{
					cell rtCommentCell;
					std::string rtCommentText;
					bool rtCommentRead = ReadFirstCommentFromAscdForTest(rtCommentData, rtCommentLen,
						&rtCommentCell, &rtCommentText);
					Check(rtCommentRead && rtCommentCell == cell(2, 2) && rtCommentText == "Da ricontrollare",
						"dopo il giro completo ASCD -> XLSX -> ASCD, il commento si ritrova ancora su B2");
				}
			}
		}
	}

	// Hyperlinks (<hyperlinks> inside xl/worksheets/sheet1.xml + the
	// sheet's own _rels, TargetMode="External"), the second item of
	// Tier 2 in the "100% XLSX standard compatibility" plan, same exact
	// shape as the comments fix above: a link on B2 should arrive in
	// the imported document via CContainer::SetHyperlink, not be
	// silently discarded like before this fix.
	{
		static const char kLinkContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kLinkRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kLinkWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kLinkWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kLinkSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"<hyperlinks><hyperlink ref=\"B2\" r:id=\"rId1\"/></hyperlinks>"
			"</worksheet>\n";
		static const char kLinkSheetRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" "
			"Target=\"https://www.haiku-os.org/\" TargetMode=\"External\"/>\n</Relationships>\n";

		BMallocIO linkXlsx;
		CZipWriter linkZip;
		linkZip.Begin(&linkXlsx);
		linkZip.AddEntry("[Content_Types].xml", kLinkContentTypes, strlen(kLinkContentTypes));
		linkZip.AddEntry("_rels/.rels", kLinkRootRels, strlen(kLinkRootRels));
		linkZip.AddEntry("xl/workbook.xml", kLinkWorkbook, strlen(kLinkWorkbook));
		linkZip.AddEntry("xl/_rels/workbook.xml.rels", kLinkWorkbookRels, strlen(kLinkWorkbookRels));
		linkZip.AddEntry("xl/worksheets/sheet1.xml", kLinkSheet, strlen(kLinkSheet));
		linkZip.AddEntry("xl/worksheets/_rels/sheet1.xml.rels", kLinkSheetRels, strlen(kLinkSheetRels));
		Check(linkZip.Close(), "costruzione del file XLSX di prova con <hyperlinks> riuscita");

		linkXlsx.Seek(0, SEEK_SET);
		translator_info linkInfo;
		err = translator->Identify(&linkXlsx, NULL, NULL, &linkInfo, 0);
		Check(err == B_OK && linkInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con <hyperlinks>");

		linkXlsx.Seek(0, SEEK_SET);
		BMallocIO linkAscdOut;
		err = translator->Translate(&linkXlsx, &linkInfo, NULL, kAtomoNativeFormat, &linkAscdOut);
		Check(err == B_OK, "Translate del file di prova con <hyperlinks> riesce");

		const unsigned char* linkAscdData = NULL;
		size_t linkAscdLen = 0;
		bool linkUnwrapped = UnwrapFirstSheet((const unsigned char*)linkAscdOut.Buffer(),
			linkAscdOut.BufferLength(), &linkAscdData, &linkAscdLen);
		Check(linkUnwrapped, "l'output di Translate del file di prova con <hyperlinks> e' un ASCD valido");

		if (linkUnwrapped)
		{
			cell importedLinkCell;
			std::string importedLinkUrl;
			bool linkRead = ReadFirstHyperlinkFromAscdForTest(linkAscdData, linkAscdLen,
				&importedLinkCell, &importedLinkUrl);
			Check(linkRead, "la sezione collegamenti dell'ASCD prodotto si legge correttamente");
			Check(linkRead && importedLinkCell == cell(2, 2),
				"il collegamento importato e' ancorato a B2, lo stesso riferimento di <hyperlink ref=\"B2\">");
			Check(linkRead && importedLinkUrl == "https://www.haiku-os.org/",
				"l'URL del collegamento importato e' quello vero, risolto tramite r:id "
				"nei _rels del foglio, non piu' scartato in silenzio");
		}
	}

	// Stesso scenario, direzione opposta (ASCD -> XLSX): un documento
	// con un collegamento su una cella esporta un vero <hyperlinks> nel
	// foglio, con la relazione esterna corrispondente nei suoi _rels, e
	// quel file si rilegge correttamente.
	{
		CContainer& linkExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &linkExportDoc, true); // A1
		linkExportDoc.SetHyperlink(cell(2, 2), "https://www.haiku-os.org/"); // B2

		BMallocIO linkAscdIn;
		status_t linkSaveErr = WriteASCDWithHyperlinkForTest(&linkExportDoc, "B2",
			"https://www.haiku-os.org/", &linkAscdIn);
		Check(linkSaveErr == B_OK, "preparazione dell'ASCD di prova con un collegamento riesce");
		linkExportDoc.Release();

		linkAscdIn.Seek(0, SEEK_SET);
		translator_info linkExportInfo;
		err = translator->Identify(&linkAscdIn, NULL, NULL, &linkExportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con un collegamento come sorgente per l'export");

		linkAscdIn.Seek(0, SEEK_SET);
		BMallocIO linkXlsxOut;
		err = translator->Translate(&linkAscdIn, &linkExportInfo, NULL, kAtomoXlsxFormat, &linkXlsxOut);
		Check(err == B_OK, "Translate ASCD (con un collegamento) -> XLSX riesce");

		if (err == B_OK)
		{
			linkXlsxOut.Seek(0, SEEK_SET);
			CZipReader linkOutZip;
			Check(linkOutZip.Open(&linkXlsxOut),
				"il file XLSX esportato con un collegamento e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> exportedSheetXml;
			bool readSheet = linkOutZip.ReadEntry("xl/worksheets/sheet1.xml", exportedSheetXml);
			std::string sheetText(exportedSheetXml.begin(), exportedSheetXml.end());
			Check(readSheet && sheetText.find("<hyperlink ref=\"B2\"") != std::string::npos,
				"xl/worksheets/sheet1.xml esportato contiene <hyperlink ref=\"B2\">");

			std::vector<unsigned char> exportedSheetRels;
			bool readSheetRels = linkOutZip.ReadEntry("xl/worksheets/_rels/sheet1.xml.rels",
				exportedSheetRels);
			std::string sheetRelsText(exportedSheetRels.begin(), exportedSheetRels.end());
			Check(readSheetRels
				&& sheetRelsText.find("https://www.haiku-os.org/") != std::string::npos
				&& sheetRelsText.find("TargetMode=\"External\"") != std::string::npos,
				"i _rels del foglio esportato contengono l'URL vero, con TargetMode=\"External\"");

			// Round-trip completo: rileggendo il file appena esportato,
			// il collegamento deve arrivare di nuovo.
			linkXlsxOut.Seek(0, SEEK_SET);
			translator_info linkReimportInfo;
			err = translator->Identify(&linkXlsxOut, NULL, NULL, &linkReimportInfo, 0);
			Check(err == B_OK && linkReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX esportato con un collegamento si riconosce ancora come XLSX valido rileggendolo");

			linkXlsxOut.Seek(0, SEEK_SET);
			BMallocIO linkRoundTripAscd;
			err = translator->Translate(&linkXlsxOut, &linkReimportInfo, NULL,
				kAtomoNativeFormat, &linkRoundTripAscd);
			Check(err == B_OK, "il file XLSX esportato con un collegamento si rilegge correttamente (round-trip)");

			if (err == B_OK)
			{
				const unsigned char* rtLinkData = NULL;
				size_t rtLinkLen = 0;
				bool rtLinkUnwrapped = UnwrapFirstSheet((const unsigned char*)linkRoundTripAscd.Buffer(),
					linkRoundTripAscd.BufferLength(), &rtLinkData, &rtLinkLen);
				Check(rtLinkUnwrapped, "il round-trip del collegamento produce anch'esso una cartella ASCB valida");

				if (rtLinkUnwrapped)
				{
					cell rtLinkCell;
					std::string rtLinkUrl;
					bool rtLinkRead = ReadFirstHyperlinkFromAscdForTest(rtLinkData, rtLinkLen,
						&rtLinkCell, &rtLinkUrl);
					Check(rtLinkRead && rtLinkCell == cell(2, 2)
						&& rtLinkUrl == "https://www.haiku-os.org/",
						"dopo il giro completo ASCD -> XLSX -> ASCD, il collegamento si ritrova ancora su B2");
				}
			}
		}
	}

	// Data validation (<dataValidations> inside xl/worksheets/sheet1.xml),
	// the third item of Tier 2 in the "100% XLSX standard compatibility"
	// plan: only two shapes have a real equivalent in this engine (see
	// DataValidationRefInfo in XlsxTranslator.cpp) -- a literal list
	// (tested here) and a numeric "between" range (tested further
	// below). A commented-out example, or anything else, is silently
	// skipped, not a bug to fix.
	{
		static const char kListValContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kListValRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kListValWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kListValWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kListValSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"<dataValidations count=\"1\">"
			"<dataValidation type=\"list\" allowBlank=\"1\" showInputMessage=\"1\" showErrorMessage=\"1\" sqref=\"B2\">"
			"<formula1>\"Rosso,Verde,Blu\"</formula1></dataValidation>"
			"</dataValidations>"
			"</worksheet>\n";

		BMallocIO listValXlsx;
		CZipWriter listValZip;
		listValZip.Begin(&listValXlsx);
		listValZip.AddEntry("[Content_Types].xml", kListValContentTypes, strlen(kListValContentTypes));
		listValZip.AddEntry("_rels/.rels", kListValRootRels, strlen(kListValRootRels));
		listValZip.AddEntry("xl/workbook.xml", kListValWorkbook, strlen(kListValWorkbook));
		listValZip.AddEntry("xl/_rels/workbook.xml.rels", kListValWorkbookRels, strlen(kListValWorkbookRels));
		listValZip.AddEntry("xl/worksheets/sheet1.xml", kListValSheet, strlen(kListValSheet));
		Check(listValZip.Close(), "costruzione del file XLSX di prova con <dataValidation type=\"list\"> riuscita");

		listValXlsx.Seek(0, SEEK_SET);
		translator_info listValInfo;
		err = translator->Identify(&listValXlsx, NULL, NULL, &listValInfo, 0);
		Check(err == B_OK && listValInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con la convalida a elenco");

		listValXlsx.Seek(0, SEEK_SET);
		BMallocIO listValAscdOut;
		err = translator->Translate(&listValXlsx, &listValInfo, NULL, kAtomoNativeFormat, &listValAscdOut);
		Check(err == B_OK, "Translate del file di prova con la convalida a elenco riesce");

		const unsigned char* listValAscdData = NULL;
		size_t listValAscdLen = 0;
		bool listValUnwrapped = UnwrapFirstSheet((const unsigned char*)listValAscdOut.Buffer(),
			listValAscdOut.BufferLength(), &listValAscdData, &listValAscdLen);
		Check(listValUnwrapped, "l'output di Translate con la convalida a elenco e' un ASCD valido");

		if (listValUnwrapped)
		{
			cell importedValCell;
			int8 importedValType = -1;
			std::string importedValList;
			double importedValMin = 0, importedValMax = 0;
			bool valRead = ReadFirstValidationFromAscdForTest(listValAscdData, listValAscdLen,
				&importedValCell, &importedValType, &importedValList, &importedValMin, &importedValMax);
			Check(valRead, "la sezione convalida dati dell'ASCD prodotto si legge correttamente");
			Check(valRead && importedValCell == cell(2, 2),
				"la convalida importata e' ancorata a B2, lo stesso riferimento di sqref=\"B2\"");
			Check(valRead && importedValType == (int8)eListValidation,
				"la convalida importata e' di tipo elenco (eListValidation)");
			Check(valRead && importedValList == "Rosso,Verde,Blu",
				"l'elenco importato e' quello vero (\"Rosso,Verde,Blu\"), senza le virgolette "
				"del letterale XLSX, non piu' scartato in silenzio");
		}
	}

	// Same fixture shape, but for a numeric "between" range instead of
	// a list -- the other real shape this engine models.
	{
		static const char kRangeValSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"<dataValidations count=\"1\">"
			"<dataValidation type=\"whole\" operator=\"between\" allowBlank=\"1\" "
			"showInputMessage=\"1\" showErrorMessage=\"1\" sqref=\"C3\">"
			"<formula1>1</formula1><formula2>100</formula2></dataValidation>"
			"</dataValidations>"
			"</worksheet>\n";
		static const char kRangeValContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kRangeValRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kRangeValWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kRangeValWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";

		BMallocIO rangeValXlsx;
		CZipWriter rangeValZip;
		rangeValZip.Begin(&rangeValXlsx);
		rangeValZip.AddEntry("[Content_Types].xml", kRangeValContentTypes, strlen(kRangeValContentTypes));
		rangeValZip.AddEntry("_rels/.rels", kRangeValRootRels, strlen(kRangeValRootRels));
		rangeValZip.AddEntry("xl/workbook.xml", kRangeValWorkbook, strlen(kRangeValWorkbook));
		rangeValZip.AddEntry("xl/_rels/workbook.xml.rels", kRangeValWorkbookRels, strlen(kRangeValWorkbookRels));
		rangeValZip.AddEntry("xl/worksheets/sheet1.xml", kRangeValSheet, strlen(kRangeValSheet));
		Check(rangeValZip.Close(), "costruzione del file XLSX di prova con <dataValidation type=\"whole\"> riuscita");

		rangeValXlsx.Seek(0, SEEK_SET);
		translator_info rangeValInfo;
		err = translator->Identify(&rangeValXlsx, NULL, NULL, &rangeValInfo, 0);
		Check(err == B_OK && rangeValInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con la convalida a intervallo");

		rangeValXlsx.Seek(0, SEEK_SET);
		BMallocIO rangeValAscdOut;
		err = translator->Translate(&rangeValXlsx, &rangeValInfo, NULL, kAtomoNativeFormat, &rangeValAscdOut);
		Check(err == B_OK, "Translate del file di prova con la convalida a intervallo riesce");

		const unsigned char* rangeValAscdData = NULL;
		size_t rangeValAscdLen = 0;
		bool rangeValUnwrapped = UnwrapFirstSheet((const unsigned char*)rangeValAscdOut.Buffer(),
			rangeValAscdOut.BufferLength(), &rangeValAscdData, &rangeValAscdLen);
		Check(rangeValUnwrapped, "l'output di Translate con la convalida a intervallo e' un ASCD valido");

		if (rangeValUnwrapped)
		{
			cell importedRangeCell;
			int8 importedRangeType = -1;
			std::string importedRangeList;
			double importedRangeMin = 0, importedRangeMax = 0;
			bool rangeRead = ReadFirstValidationFromAscdForTest(rangeValAscdData, rangeValAscdLen,
				&importedRangeCell, &importedRangeType, &importedRangeList,
				&importedRangeMin, &importedRangeMax);
			Check(rangeRead, "la sezione convalida dati (intervallo) dell'ASCD prodotto si legge correttamente");
			Check(rangeRead && importedRangeCell == cell(3, 3),
				"la convalida importata e' ancorata a C3, lo stesso riferimento di sqref=\"C3\"");
			Check(rangeRead && importedRangeType == (int8)eNumberRangeValidation,
				"la convalida importata e' di tipo intervallo numerico (eNumberRangeValidation)");
			Check(rangeRead && importedRangeMin == 1.0 && importedRangeMax == 100.0,
				"l'intervallo importato e' quello vero (1-100), risolto da formula1/formula2 letterali, "
				"non piu' scartato in silenzio");
		}
	}

	// Stesso scenario, direzione opposta (ASCD -> XLSX): un documento
	// con una convalida a elenco su una cella esporta un vero
	// <dataValidation type="list"> nel foglio, e quel file si rilegge
	// correttamente.
	{
		CContainer& valExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &valExportDoc, true); // A1
		ValidationRule listRule;
		listRule.type = eListValidation;
		listRule.list = "Rosso,Verde,Blu";
		valExportDoc.SetValidation(cell(2, 2), listRule); // B2

		BMallocIO valAscdIn;
		status_t valSaveErr = WriteASCDWithValidationForTest(&valExportDoc, "B2",
			(int8)eListValidation, "Rosso,Verde,Blu", 0.0, 0.0, &valAscdIn);
		Check(valSaveErr == B_OK, "preparazione dell'ASCD di prova con una convalida a elenco riesce");
		valExportDoc.Release();

		valAscdIn.Seek(0, SEEK_SET);
		translator_info valExportInfo;
		err = translator->Identify(&valAscdIn, NULL, NULL, &valExportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con una convalida a elenco come sorgente per l'export");

		valAscdIn.Seek(0, SEEK_SET);
		BMallocIO valXlsxOut;
		err = translator->Translate(&valAscdIn, &valExportInfo, NULL, kAtomoXlsxFormat, &valXlsxOut);
		Check(err == B_OK, "Translate ASCD (con una convalida a elenco) -> XLSX riesce");

		if (err == B_OK)
		{
			valXlsxOut.Seek(0, SEEK_SET);
			CZipReader valOutZip;
			Check(valOutZip.Open(&valXlsxOut),
				"il file XLSX esportato con una convalida a elenco e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> exportedSheetXml;
			bool readSheet = valOutZip.ReadEntry("xl/worksheets/sheet1.xml", exportedSheetXml);
			std::string sheetText(exportedSheetXml.begin(), exportedSheetXml.end());
			Check(readSheet && sheetText.find("<dataValidation type=\"list\"") != std::string::npos
				&& sheetText.find("sqref=\"B2\"") != std::string::npos
				&& sheetText.find("Rosso,Verde,Blu") != std::string::npos,
				"xl/worksheets/sheet1.xml esportato contiene <dataValidation type=\"list\"> su B2 "
				"con l'elenco vero");

			// Round-trip completo: rileggendo il file appena esportato,
			// la convalida deve arrivare di nuovo.
			valXlsxOut.Seek(0, SEEK_SET);
			translator_info valReimportInfo;
			err = translator->Identify(&valXlsxOut, NULL, NULL, &valReimportInfo, 0);
			Check(err == B_OK && valReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX esportato con una convalida a elenco si riconosce ancora come XLSX valido rileggendolo");

			valXlsxOut.Seek(0, SEEK_SET);
			BMallocIO valRoundTripAscd;
			err = translator->Translate(&valXlsxOut, &valReimportInfo, NULL,
				kAtomoNativeFormat, &valRoundTripAscd);
			Check(err == B_OK,
				"il file XLSX esportato con una convalida a elenco si rilegge correttamente (round-trip)");

			if (err == B_OK)
			{
				const unsigned char* rtValData = NULL;
				size_t rtValLen = 0;
				bool rtValUnwrapped = UnwrapFirstSheet((const unsigned char*)valRoundTripAscd.Buffer(),
					valRoundTripAscd.BufferLength(), &rtValData, &rtValLen);
				Check(rtValUnwrapped,
					"il round-trip della convalida a elenco produce anch'esso una cartella ASCB valida");

				if (rtValUnwrapped)
				{
					cell rtValCell;
					int8 rtValType = -1;
					std::string rtValList;
					double rtValMin = 0, rtValMax = 0;
					bool rtValRead = ReadFirstValidationFromAscdForTest(rtValData, rtValLen,
						&rtValCell, &rtValType, &rtValList, &rtValMin, &rtValMax);
					Check(rtValRead && rtValCell == cell(2, 2)
						&& rtValType == (int8)eListValidation && rtValList == "Rosso,Verde,Blu",
						"dopo il giro completo ASCD -> XLSX -> ASCD, la convalida a elenco "
						"si ritrova ancora su B2");
				}
			}
		}
	}

	// Freeze panes (<pane state="frozen"/> inside <sheetView>), the
	// fourth item of Tier 2 in the "100% XLSX standard compatibility"
	// plan: only state="frozen"/"frozenSplit" means a real freeze
	// (xSplit/ySplit as row/column counts) -- a plain draggable split
	// (no "state", or state="split") has no equivalent in this app and
	// is correctly NOT tested here, since it should stay 0,0.
	{
		static const char kFreezeContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kFreezeRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kFreezeWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kFreezeWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kFreezeSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetViews><sheetView tabSelected=\"1\" workbookViewId=\"0\">"
			"<pane xSplit=\"1\" ySplit=\"2\" topLeftCell=\"B3\" activePane=\"bottomRight\" state=\"frozen\"/>"
			"</sheetView></sheetViews>"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"</worksheet>\n";

		BMallocIO freezeXlsx;
		CZipWriter freezeZip;
		freezeZip.Begin(&freezeXlsx);
		freezeZip.AddEntry("[Content_Types].xml", kFreezeContentTypes, strlen(kFreezeContentTypes));
		freezeZip.AddEntry("_rels/.rels", kFreezeRootRels, strlen(kFreezeRootRels));
		freezeZip.AddEntry("xl/workbook.xml", kFreezeWorkbook, strlen(kFreezeWorkbook));
		freezeZip.AddEntry("xl/_rels/workbook.xml.rels", kFreezeWorkbookRels, strlen(kFreezeWorkbookRels));
		freezeZip.AddEntry("xl/worksheets/sheet1.xml", kFreezeSheet, strlen(kFreezeSheet));
		Check(freezeZip.Close(), "costruzione del file XLSX di prova con <pane state=\"frozen\"> riuscita");

		freezeXlsx.Seek(0, SEEK_SET);
		translator_info freezeInfo;
		err = translator->Identify(&freezeXlsx, NULL, NULL, &freezeInfo, 0);
		Check(err == B_OK && freezeInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con i riquadri bloccati");

		freezeXlsx.Seek(0, SEEK_SET);
		BMallocIO freezeAscdOut;
		err = translator->Translate(&freezeXlsx, &freezeInfo, NULL, kAtomoNativeFormat, &freezeAscdOut);
		Check(err == B_OK, "Translate del file di prova con i riquadri bloccati riesce");

		const unsigned char* freezeAscdData = NULL;
		size_t freezeAscdLen = 0;
		bool freezeUnwrapped = UnwrapFirstSheet((const unsigned char*)freezeAscdOut.Buffer(),
			freezeAscdOut.BufferLength(), &freezeAscdData, &freezeAscdLen);
		Check(freezeUnwrapped, "l'output di Translate con i riquadri bloccati e' un ASCD valido");

		if (freezeUnwrapped)
		{
			int32 importedFrozenRows = -1, importedFrozenCols = -1;
			bool freezeRead = ReadFreezeFromAscdForTest(freezeAscdData, freezeAscdLen,
				&importedFrozenRows, &importedFrozenCols);
			Check(freezeRead, "la sezione blocca-riquadri dell'ASCD prodotto si legge correttamente");
			Check(freezeRead && importedFrozenRows == 2 && importedFrozenCols == 1,
				"i riquadri bloccati importati sono quelli veri (2 righe, 1 colonna), "
				"da <pane ySplit=\"2\" xSplit=\"1\" state=\"frozen\">, non piu' sempre 0");
		}
	}

	// Stesso scenario, direzione opposta (ASCD -> XLSX): un documento
	// con i riquadri bloccati esporta un vero <pane state="frozen">
	// dentro <sheetView>, e quel file si rilegge correttamente.
	{
		CContainer& freezeExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &freezeExportDoc, true); // A1

		BMallocIO freezeAscdIn;
		status_t freezeSaveErr = WriteASCDWithFreezeForTest(&freezeExportDoc, 2, 1, &freezeAscdIn);
		Check(freezeSaveErr == B_OK, "preparazione dell'ASCD di prova con i riquadri bloccati riesce");
		freezeExportDoc.Release();

		freezeAscdIn.Seek(0, SEEK_SET);
		translator_info freezeExportInfo;
		err = translator->Identify(&freezeAscdIn, NULL, NULL, &freezeExportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con i riquadri bloccati come sorgente per l'export");

		freezeAscdIn.Seek(0, SEEK_SET);
		BMallocIO freezeXlsxOut;
		err = translator->Translate(&freezeAscdIn, &freezeExportInfo, NULL, kAtomoXlsxFormat, &freezeXlsxOut);
		Check(err == B_OK, "Translate ASCD (con i riquadri bloccati) -> XLSX riesce");

		if (err == B_OK)
		{
			freezeXlsxOut.Seek(0, SEEK_SET);
			CZipReader freezeOutZip;
			Check(freezeOutZip.Open(&freezeXlsxOut),
				"il file XLSX esportato con i riquadri bloccati e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> exportedSheetXml;
			bool readSheet = freezeOutZip.ReadEntry("xl/worksheets/sheet1.xml", exportedSheetXml);
			std::string sheetText(exportedSheetXml.begin(), exportedSheetXml.end());
			Check(readSheet && sheetText.find("<pane xSplit=\"1\" ySplit=\"2\"") != std::string::npos
				&& sheetText.find("state=\"frozen\"") != std::string::npos,
				"xl/worksheets/sheet1.xml esportato contiene <pane xSplit=\"1\" ySplit=\"2\" ... state=\"frozen\">");

			// Round-trip completo: rileggendo il file appena esportato,
			// i riquadri bloccati devono arrivare di nuovo.
			freezeXlsxOut.Seek(0, SEEK_SET);
			translator_info freezeReimportInfo;
			err = translator->Identify(&freezeXlsxOut, NULL, NULL, &freezeReimportInfo, 0);
			Check(err == B_OK && freezeReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX esportato con i riquadri bloccati si riconosce ancora come XLSX valido rileggendolo");

			freezeXlsxOut.Seek(0, SEEK_SET);
			BMallocIO freezeRoundTripAscd;
			err = translator->Translate(&freezeXlsxOut, &freezeReimportInfo, NULL,
				kAtomoNativeFormat, &freezeRoundTripAscd);
			Check(err == B_OK,
				"il file XLSX esportato con i riquadri bloccati si rilegge correttamente (round-trip)");

			if (err == B_OK)
			{
				const unsigned char* rtFreezeData = NULL;
				size_t rtFreezeLen = 0;
				bool rtFreezeUnwrapped = UnwrapFirstSheet((const unsigned char*)freezeRoundTripAscd.Buffer(),
					freezeRoundTripAscd.BufferLength(), &rtFreezeData, &rtFreezeLen);
				Check(rtFreezeUnwrapped,
					"il round-trip dei riquadri bloccati produce anch'esso una cartella ASCB valida");

				if (rtFreezeUnwrapped)
				{
					int32 rtFrozenRows = -1, rtFrozenCols = -1;
					bool rtFreezeRead = ReadFreezeFromAscdForTest(rtFreezeData, rtFreezeLen,
						&rtFrozenRows, &rtFrozenCols);
					Check(rtFreezeRead && rtFrozenRows == 2 && rtFrozenCols == 1,
						"dopo il giro completo ASCD -> XLSX -> ASCD, i riquadri bloccati "
						"si ritrovano ancora a 2 righe, 1 colonna");
				}
			}
		}
	}

	// Border color (<color rgb="..."/> inside a <border> side, xl/
	// styles.xml), the fifth item of Tier 2 in the "100% XLSX standard
	// compatibility" plan: before this fix, ParseStyles only tracked
	// presence/absence per side, never the real RGB -- a red border
	// imported as the engine's default black. Uses a distinctive color
	// (red) rather than black specifically so this test can actually
	// tell "real color read" apart from "never set, still default".
	{
		static const char kBorderColorContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n"
			"</Types>\n";
		static const char kBorderColorRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kBorderColorWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kBorderColorWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kBorderColorSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData>"
			"<row r=\"1\"><c r=\"A1\"><v>5</v></c></row>"
			"<row r=\"2\"><c r=\"B2\" s=\"1\"><v>7</v></c></row>"
			"</sheetData>"
			"</worksheet>\n";
		static const char kBorderColorStyles[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<fonts count=\"1\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>\n"
			"<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
			"<fill><patternFill patternType=\"gray125\"/></fill></fills>\n"
			"<borders count=\"2\">"
			"<border><left/><right/><top/><bottom/><diagonal/></border>"
			"<border><left style=\"thin\"><color rgb=\"FFFF0000\"/></left><right/><top/><bottom/><diagonal/></border>"
			"</borders>\n"
			"<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>\n"
			"<cellXfs count=\"2\">"
			"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
			"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"1\" xfId=\"0\" applyBorder=\"1\"/>"
			"</cellXfs>\n"
			"</styleSheet>\n";

		BMallocIO borderColorXlsx;
		CZipWriter borderColorZip;
		borderColorZip.Begin(&borderColorXlsx);
		borderColorZip.AddEntry("[Content_Types].xml", kBorderColorContentTypes, strlen(kBorderColorContentTypes));
		borderColorZip.AddEntry("_rels/.rels", kBorderColorRootRels, strlen(kBorderColorRootRels));
		borderColorZip.AddEntry("xl/workbook.xml", kBorderColorWorkbook, strlen(kBorderColorWorkbook));
		borderColorZip.AddEntry("xl/_rels/workbook.xml.rels", kBorderColorWorkbookRels, strlen(kBorderColorWorkbookRels));
		borderColorZip.AddEntry("xl/worksheets/sheet1.xml", kBorderColorSheet, strlen(kBorderColorSheet));
		borderColorZip.AddEntry("xl/styles.xml", kBorderColorStyles, strlen(kBorderColorStyles));
		Check(borderColorZip.Close(), "costruzione del file XLSX di prova con <color rgb=\"FFFF0000\"> sul bordo riuscita");

		borderColorXlsx.Seek(0, SEEK_SET);
		translator_info borderColorInfo;
		err = translator->Identify(&borderColorXlsx, NULL, NULL, &borderColorInfo, 0);
		Check(err == B_OK && borderColorInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con il colore del bordo");

		borderColorXlsx.Seek(0, SEEK_SET);
		BMallocIO borderColorAscdOut;
		err = translator->Translate(&borderColorXlsx, &borderColorInfo, NULL,
			kAtomoNativeFormat, &borderColorAscdOut);
		Check(err == B_OK, "Translate del file di prova con il colore del bordo riesce");

		const unsigned char* borderColorAscdData = NULL;
		size_t borderColorAscdLen = 0;
		bool borderColorUnwrapped = UnwrapFirstSheet((const unsigned char*)borderColorAscdOut.Buffer(),
			borderColorAscdOut.BufferLength(), &borderColorAscdData, &borderColorAscdLen);
		Check(borderColorUnwrapped, "l'output di Translate con il colore del bordo e' un ASCD valido");

		if (borderColorUnwrapped)
		{
			cell importedBorderCell;
			rgb_color importedBorderColor = { 0, 0, 0, 0 };
			bool borderColorRead = ReadFirstBorderColorFromAscdForTest(borderColorAscdData,
				borderColorAscdLen, &importedBorderCell, &importedBorderColor);
			Check(borderColorRead, "la sezione colore del bordo dell'ASCD prodotto si legge correttamente");
			Check(borderColorRead && importedBorderCell == cell(2, 2),
				"il colore del bordo importato e' ancorato a B2, la cella con borderId=\"1\"");
			Check(borderColorRead && importedBorderColor.red == 255 && importedBorderColor.green == 0
				&& importedBorderColor.blue == 0,
				"il colore del bordo importato e' quello vero (rosso, FFFF0000), "
				"non piu' sempre nero (il predefinito del motore)");
		}
	}

	// Stesso scenario ma sull'infrastruttura ASCD intermedia di questo
	// translator (ASCD -> ASCD, non ASCD -> XLSX: l'esportazione verso
	// un vero styles.xml resta fuori scopo qui, vedi ROADMAP.md): un
	// documento con un colore del bordo esplicito deve attraversare
	// ReadASCD (che prima scartava questa sezione) e WriteASCD (che
	// prima scriveva sempre un conteggio a zero) senza perdere il
	// colore vero.
	{
		CContainer& borderColorExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &borderColorExportDoc, true); // A1

		rgb_color red = { 255, 0, 0, 255 };
		BMallocIO borderColorAscdIn;
		status_t borderColorSaveErr = WriteASCDWithBorderColorForTest(&borderColorExportDoc, "B2",
			red, &borderColorAscdIn);
		Check(borderColorSaveErr == B_OK, "preparazione dell'ASCD di prova con un colore del bordo riesce");
		borderColorExportDoc.Release();

		borderColorAscdIn.Seek(0, SEEK_SET);
		translator_info borderColorNativeInfo;
		err = translator->Identify(&borderColorAscdIn, NULL, NULL, &borderColorNativeInfo, kAtomoNativeFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con un colore del bordo come sorgente nativa");

		borderColorAscdIn.Seek(0, SEEK_SET);
		BMallocIO borderColorAscdRoundTrip;
		err = translator->Translate(&borderColorAscdIn, &borderColorNativeInfo, NULL,
			kAtomoNativeFormat, &borderColorAscdRoundTrip);
		Check(err == B_OK, "Translate ASCD (con un colore del bordo) -> ASCD riesce (ReadASCD + WriteASCD)");

		if (err == B_OK)
		{
			cell rtBorderCell;
			rgb_color rtBorderColor = { 0, 0, 0, 0 };
			bool rtBorderRead = ReadFirstBorderColorFromAscdForTest(
				(const unsigned char*)borderColorAscdRoundTrip.Buffer(),
				borderColorAscdRoundTrip.BufferLength(), &rtBorderCell, &rtBorderColor);
			Check(rtBorderRead && rtBorderCell == cell(2, 2)
				&& rtBorderColor.red == 255 && rtBorderColor.green == 0 && rtBorderColor.blue == 0,
				"dopo il giro ASCD -> ASCD attraverso questo translator, il colore del bordo "
				"si ritrova ancora rosso su B2 (non piu' scartato da ReadASCD ne' azzerato da WriteASCD)");
		}
	}

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
