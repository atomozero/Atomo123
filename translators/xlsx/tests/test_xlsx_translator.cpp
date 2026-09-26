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

// Same idea as WriteASCDWithValidationForTest below, but for the print
// area instead (100% XLSX standard compatibility, Tier 2 -- print
// settings, step 4 of 4): print area comes right after chart title,
// before margins/scale, in the real WriteASCD section order.
static status_t WriteASCDWithPrintAreaForTest(CContainer* doc, int16 top, int16 left,
	int16 bottom, int16 right, BPositionIO* dest)
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
	// Tipo di grafico incorporato, colore del bordo, convalida dati,
	// formattazione condizionale, tabelle: cinque conteggi a zero.
	for (int i = 0; i < 5; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Titolo di grafico incorporato: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Colonne valore esplicite di grafico incorporato (Task 2): un
	// conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Area di stampa: i dati veri, un byte "has=1" + 4 int16.
	{
		uint8 has = 1;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
			|| dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
			|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
			|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right))
			return B_IO_ERROR;
	}

	return B_OK;
}

// Same idea as WriteASCDWithValidationForTest below, but for page
// margins/scale instead (100% XLSX standard compatibility, Tier 2 --
// print settings, step 2 of 4): margins/scale come much later in the
// real WriteASCD section order (after comments, hyperlinks, chart-
// type, border-color, data validation, conditional formatting,
// tables, chart title, and print area, all zero/absent here), so this
// helper needs to write through all of them first.
static status_t WriteASCDWithPrintSettingsForTest(CContainer* doc, double marginTopCm,
	double marginBottomCm, double marginLeftCm, double marginRightCm,
	int32 scaleMode, double scalePercent, BPositionIO* dest)
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
	// Colore del bordo, convalida dati, formattazione condizionale,
	// tabelle: quattro conteggi a zero.
	for (int i = 0; i < 4; i++)
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Titolo di grafico incorporato: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Colonne valore esplicite di grafico incorporato (Task 2): un
	// conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}
	// Area di stampa: un byte "has" + 4 int16, sempre presenti (assente qui).
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
	// Margini/scala: i dati veri, un byte "has=1" + 4 double + int32 + double.
	{
		uint8 has = 1;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&marginTopCm, sizeof(marginTopCm)) != (ssize_t)sizeof(marginTopCm)
			|| dest->Write(&marginBottomCm, sizeof(marginBottomCm)) != (ssize_t)sizeof(marginBottomCm)
			|| dest->Write(&marginLeftCm, sizeof(marginLeftCm)) != (ssize_t)sizeof(marginLeftCm)
			|| dest->Write(&marginRightCm, sizeof(marginRightCm)) != (ssize_t)sizeof(marginRightCm)
			|| dest->Write(&scaleMode, sizeof(scaleMode)) != (ssize_t)sizeof(scaleMode)
			|| dest->Write(&scalePercent, sizeof(scalePercent)) != (ssize_t)sizeof(scalePercent))
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

	// Colonne valore esplicite di grafico incorporato (Task 2, colonne
	// valore non adiacenti): stesso principio della sezione titolo
	// appena sopra -- chartValueColCount=0, nessun grafico in questo
	// documento di prova.
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

// Same idea as WriteASCDWithNameForTest above, but for a persisted pivot
// table object (Fase 2 delle tabelle pivot -- vedi ROADMAP.md/
// CHANGELOG.md): la sezione pivot e' la NUOVISSIMA ultima sezione del
// formato, DOPO quella di allineamento verticale (che a sua volta viene
// dopo i nomi) -- ogni sezione intermedia va scritta vuota per le stesse
// ragioni gia' spiegate sopra per WriteASCDWithNameForTest, con l'aggiunta
// di un conteggio a zero per l'allineamento verticale, nuovo anche lui
// rispetto a quella funzione. "pivot" e' scritta cosi' com'e', nessuna
// chiamata a BuildPivotTable/WritePivotTable (ui/src/Pivot.cpp, non
// linkato in questo binario di test): il chiamante deve gia' aver scritto
// sia le celle sorgente sia quelle di destinazione (header + righe) come
// vere celle tramite TryToParseString, esattamente come farebbe
// MainWindow::HandlePivotRequest nella vera app.
static status_t WriteASCDWithPivotForTest(CContainer* doc, const PivotTableObject& pivot,
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

	// Colonne valore esplicite di grafico incorporato (Task 2, colonne
	// valore non adiacenti): stesso principio della sezione titolo
	// appena sopra -- chartValueColCount=0.
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

	// Intervalli con nome: un conteggio a zero (non servono per questo test).
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Allineamento verticale: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Tabelle pivot, ULTIMA sezione del formato: i dati veri, stesso
	// ordine byte-per-byte del vero SaveASCD (ui/src/AscdIO.cpp).
	{
		int32 pivotCount = 1;
		if (dest->Write(&pivotCount, sizeof(pivotCount)) != (ssize_t)sizeof(pivotCount))
			return B_IO_ERROR;

		int16 srcLeft = pivot.sourceRange.left, srcTop = pivot.sourceRange.top,
			srcRight = pivot.sourceRange.right, srcBottom = pivot.sourceRange.bottom;
		int16 destCol = pivot.destAnchor.h, destRow = pivot.destAnchor.v;
		int32 aggFunc = (int32)pivot.aggFunc;
		if (dest->Write(&srcLeft, sizeof(srcLeft)) != (ssize_t)sizeof(srcLeft)
			|| dest->Write(&srcTop, sizeof(srcTop)) != (ssize_t)sizeof(srcTop)
			|| dest->Write(&srcRight, sizeof(srcRight)) != (ssize_t)sizeof(srcRight)
			|| dest->Write(&srcBottom, sizeof(srcBottom)) != (ssize_t)sizeof(srcBottom)
			|| dest->Write(&destCol, sizeof(destCol)) != (ssize_t)sizeof(destCol)
			|| dest->Write(&destRow, sizeof(destRow)) != (ssize_t)sizeof(destRow)
			|| dest->Write(&aggFunc, sizeof(aggFunc)) != (ssize_t)sizeof(aggFunc))
			return B_IO_ERROR;

		int32 rowCount = (int32)pivot.cachedRows.size();
		if (dest->Write(&rowCount, sizeof(rowCount)) != (ssize_t)sizeof(rowCount))
			return B_IO_ERROR;

		for (int32 r = 0; r < rowCount; r++)
		{
			const PivotRow& row = pivot.cachedRows[r];
			int32 catCount = (int32)row.categories.size();
			if (dest->Write(&catCount, sizeof(catCount)) != (ssize_t)sizeof(catCount))
				return B_IO_ERROR;
			for (int32 k = 0; k < catCount; k++)
			{
				int32 catLen = row.categories[k].Length();
				if (dest->Write(&catLen, sizeof(catLen)) != (ssize_t)sizeof(catLen))
					return B_IO_ERROR;
				if (catLen > 0 && dest->Write(row.categories[k].String(), catLen) != catLen)
					return B_IO_ERROR;
			}
			int32 count32 = (int32)row.count;
			if (dest->Write(&row.aggregate, sizeof(row.aggregate)) != (ssize_t)sizeof(row.aggregate)
				|| dest->Write(&count32, sizeof(count32)) != (ssize_t)sizeof(count32)
				|| dest->Write(&row.minVal, sizeof(row.minVal)) != (ssize_t)sizeof(row.minVal)
				|| dest->Write(&row.maxVal, sizeof(row.maxVal)) != (ssize_t)sizeof(row.maxVal))
				return B_IO_ERROR;
		}
	}

	return B_OK;
}

// Same idea as WriteASCDWithPivotForTest above, but appends the two
// sections that came after the 1D pivot section (Fase pivot 2D: campo
// Colonne + misure multiple, la NUOVISSIMA ultima sezione del formato):
// chart-rowOriented (sempre 0, nessun grafico in questo test) poi i
// dati 2D veri PER LA STESSA pivot (indice 0) gia' scritta sopra dalla
// sezione 1D -- "pivot.cachedRows" e' tipicamente vuoto qui (un pivot
// 2D puro non popola la cache 1D legacy, vedi il commento su
// PivotTableObject::cachedRows2D in Container.h), ma sourceRange/
// destAnchor restano condivisi fra le due sezioni per la stessa pivot.
static status_t WriteASCDWithPivot2DForTest(CContainer* doc, const PivotTableObject& pivot,
	BPositionIO* dest)
{
	status_t err = WriteASCDWithPivotForTest(doc, pivot, dest);
	if (err != B_OK)
		return err;

	// Orientamento riga di grafico incorporato: chartRowOrientCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Tabelle pivot 2D, ULTIMA sezione del formato: i dati veri, stesso
	// ordine byte-per-byte del vero WriteASCD/SaveASCD.
	{
		int32 pivot2DCount = 1;
		if (dest->Write(&pivot2DCount, sizeof(pivot2DCount)) != (ssize_t)sizeof(pivot2DCount))
			return B_IO_ERROR;

		int16 columnFieldCol = pivot.columnFieldCol;
		if (dest->Write(&columnFieldCol, sizeof(columnFieldCol)) != (ssize_t)sizeof(columnFieldCol))
			return B_IO_ERROR;

		int32 measureCount = (int32)pivot.measures.size();
		if (dest->Write(&measureCount, sizeof(measureCount)) != (ssize_t)sizeof(measureCount))
			return B_IO_ERROR;
		for (int32 m = 0; m < measureCount; m++)
		{
			const PivotMeasure& measure = pivot.measures[m];
			int32 aggFunc = (int32)measure.aggFunc;
			int32 labelLen = measure.label.Length();
			if (dest->Write(&measure.sourceCol, sizeof(measure.sourceCol)) != (ssize_t)sizeof(measure.sourceCol)
				|| dest->Write(&aggFunc, sizeof(aggFunc)) != (ssize_t)sizeof(aggFunc)
				|| dest->Write(&labelLen, sizeof(labelLen)) != (ssize_t)sizeof(labelLen))
				return B_IO_ERROR;
			if (labelLen > 0 && dest->Write(measure.label.String(), labelLen) != labelLen)
				return B_IO_ERROR;
		}

		int32 columnValueCount = (int32)pivot.columnValues.size();
		if (dest->Write(&columnValueCount, sizeof(columnValueCount)) != (ssize_t)sizeof(columnValueCount))
			return B_IO_ERROR;
		for (int32 c = 0; c < columnValueCount; c++)
		{
			int32 len = pivot.columnValues[c].Length();
			if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(pivot.columnValues[c].String(), len) != len)
				return B_IO_ERROR;
		}

		int32 rowCount2D = (int32)pivot.cachedRows2D.size();
		if (dest->Write(&rowCount2D, sizeof(rowCount2D)) != (ssize_t)sizeof(rowCount2D))
			return B_IO_ERROR;
		int32 numColSlots = columnValueCount > 0 ? columnValueCount : 1;
		for (int32 r = 0; r < rowCount2D; r++)
		{
			const PivotRow2D& row = pivot.cachedRows2D[r];
			int32 catCount = (int32)row.categories.size();
			if (dest->Write(&catCount, sizeof(catCount)) != (ssize_t)sizeof(catCount))
				return B_IO_ERROR;
			for (int32 k = 0; k < catCount; k++)
			{
				int32 catLen = row.categories[k].Length();
				if (dest->Write(&catLen, sizeof(catLen)) != (ssize_t)sizeof(catLen))
					return B_IO_ERROR;
				if (catLen > 0 && dest->Write(row.categories[k].String(), catLen) != catLen)
					return B_IO_ERROR;
			}
			for (int32 c = 0; c < numColSlots; c++)
			{
				for (int32 m = 0; m < measureCount; m++)
				{
					const PivotCellAgg& agg = row.cells[c][m];
					int32 count32 = (int32)agg.count;
					if (dest->Write(&agg.aggregate, sizeof(agg.aggregate)) != (ssize_t)sizeof(agg.aggregate)
						|| dest->Write(&count32, sizeof(count32)) != (ssize_t)sizeof(count32)
						|| dest->Write(&agg.minVal, sizeof(agg.minVal)) != (ssize_t)sizeof(agg.minVal)
						|| dest->Write(&agg.maxVal, sizeof(agg.maxVal)) != (ssize_t)sizeof(agg.maxVal))
						return B_IO_ERROR;
				}
			}
		}
	}

	return B_OK;
}

// Same idea as WriteASCDWithPivotForTest above, but for the
// conditional-formatting section (export gap: WriteXLSX never wrote a
// single <conditionalFormatting>/<dxf>, for any rule type -- see
// ROADMAP.md/CHANGELOG.md). Unlike the pivot helper, this data sits in
// the MIDDLE of the trailing-sections boilerplate (between "convalida
// dati" and "tabelle strutturate", both zero here), not at the very
// end, so this duplicates the full skeleton rather than calling another
// helper and appending -- same reasoning already given for
// WriteASCDWithPivotForTest itself. "rules" is written byte-for-byte
// like the real SaveASCD (ui/src/AscdIO.cpp).
//
// Unlike every other helper in this file, this one CANNOT reuse
// WriteASCDForTest's shared kASCDVersionForTest (1): the colorScale/
// dataBar/iconSet thresholds, compareIsCellRef and expressionFormula
// fields only exist from format version 3-5 onward (see the version
// checks in ReadASCD/kASCDVersion), so a version-1 stream would leave
// ReadASCD believing those fields were never written, misaligning
// everything after -- a real bug caught while writing this very test.
// Writing version 7 also means every cell needs an explicit "kind"
// byte (version >= 2, Fase 15) that WriteASCDForTest's shared cell
// loop never writes (it relies on kAscdCellFormula being the READER's
// own default for version 1) -- so this duplicates that loop too,
// always writing kAscdCellFormula explicitly, which is byte-for-byte
// what a version-1 reader would have assumed anyway.
static status_t WriteASCDWithCondFormatForTest(CContainer* doc,
	const std::vector<ConditionalFormatRule>& rules, BPositionIO* dest)
{
	static const int32 kVersion8 = 8;
	int32 count = 0;
	{
		CCellIterator counter(doc, NULL);
		cell c;
		while (counter.NextExisting(c))
			count++;
	}
	if (dest->Write(kASCDMagicForTest, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kVersion8, sizeof(kVersion8)) != (ssize_t)sizeof(kVersion8))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;
	{
		CCellIterator iter(doc, NULL);
		cell c;
		while (iter.NextExisting(c))
		{
			char text[512];
			doc->GetCellFormula(c, text, sizeof(text), false);

			int16 row = c.v, col = c.h;
			int32 len = strlen(text);
			uint8 kind = 0; // kAscdCellFormula

			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len)
				|| dest->Write(&kind, sizeof(kind)) != (ssize_t)sizeof(kind))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(text, len) != len)
				return B_IO_ERROR;
		}
	}

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

	// Convalida dati: un conteggio a zero (non serve per questo test).
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Formattazione condizionale: i dati VERI, stesso ordine
	// byte-per-byte del vero SaveASCD (ui/src/AscdIO.cpp).
	{
		int32 count = (int32)rules.size();
		if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
			return B_IO_ERROR;

		for (size_t i = 0; i < rules.size(); i++)
		{
			const ConditionalFormatRule& rule = rules[i];
			int8 type = (int8)rule.type;
			int32 valueLen = (int32)rule.compareValue.size();
			if (dest->Write(&type, sizeof(type)) != (ssize_t)sizeof(type)
				|| dest->Write(&valueLen, sizeof(valueLen)) != (ssize_t)sizeof(valueLen))
				return B_IO_ERROR;
			if (valueLen > 0 && dest->Write(rule.compareValue.data(), valueLen) != valueLen)
				return B_IO_ERROR;

			if (dest->Write(&rule.bgColor, sizeof(rule.bgColor)) != (ssize_t)sizeof(rule.bgColor))
				return B_IO_ERROR;

			int32 rangeCount = (int32)rule.ranges.size();
			if (dest->Write(&rangeCount, sizeof(rangeCount)) != (ssize_t)sizeof(rangeCount))
				return B_IO_ERROR;
			for (size_t r = 0; r < rule.ranges.size(); r++)
			{
				int16 left = rule.ranges[r].left, top = rule.ranges[r].top,
					right = rule.ranges[r].right, bottom = rule.ranges[r].bottom;
				if (dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right)
					|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
					return B_IO_ERROR;
			}

			int32 pointCount = (int32)rule.colorScalePoints.size();
			if (dest->Write(&pointCount, sizeof(pointCount)) != (ssize_t)sizeof(pointCount))
				return B_IO_ERROR;
			for (size_t p = 0; p < rule.colorScalePoints.size(); p++)
			{
				const ColorScalePoint& point = rule.colorScalePoints[p];
				int32 cfvoTypeLen = (int32)point.cfvoType.size();
				if (dest->Write(&cfvoTypeLen, sizeof(cfvoTypeLen)) != (ssize_t)sizeof(cfvoTypeLen))
					return B_IO_ERROR;
				if (cfvoTypeLen > 0 && dest->Write(point.cfvoType.data(), cfvoTypeLen) != cfvoTypeLen)
					return B_IO_ERROR;
				if (dest->Write(&point.cfvoValue, sizeof(point.cfvoValue)) != (ssize_t)sizeof(point.cfvoValue)
					|| dest->Write(&point.color, sizeof(point.color)) != (ssize_t)sizeof(point.color))
					return B_IO_ERROR;
			}

			int8 compareIsCellRef = rule.compareIsCellRef ? 1 : 0;
			int16 compareRefCol = rule.compareRefCell.h, compareRefRow = rule.compareRefCell.v;
			if (dest->Write(&compareIsCellRef, sizeof(compareIsCellRef)) != (ssize_t)sizeof(compareIsCellRef)
				|| dest->Write(&compareRefCol, sizeof(compareRefCol)) != (ssize_t)sizeof(compareRefCol)
				|| dest->Write(&compareRefRow, sizeof(compareRefRow)) != (ssize_t)sizeof(compareRefRow))
				return B_IO_ERROR;

			int32 exprLen = (int32)rule.expressionFormula.size();
			if (dest->Write(&exprLen, sizeof(exprLen)) != (ssize_t)sizeof(exprLen))
				return B_IO_ERROR;
			if (exprLen > 0 && dest->Write(rule.expressionFormula.data(), exprLen) != exprLen)
				return B_IO_ERROR;

			if (dest->Write(&rule.dataBarColor, sizeof(rule.dataBarColor)) != (ssize_t)sizeof(rule.dataBarColor))
				return B_IO_ERROR;

			int32 iconStyleLen = (int32)rule.iconSetStyle.size();
			if (dest->Write(&iconStyleLen, sizeof(iconStyleLen)) != (ssize_t)sizeof(iconStyleLen))
				return B_IO_ERROR;
			if (iconStyleLen > 0 && dest->Write(rule.iconSetStyle.data(), iconStyleLen) != iconStyleLen)
				return B_IO_ERROR;

			// Versione 8 (Path to full Excel parity, Tier 3): stesso
			// ordine byte-per-byte del vero SaveASCD.
			int8 ruleOperator = rule.ruleOperator;
			if (dest->Write(&ruleOperator, sizeof(ruleOperator)) != (ssize_t)sizeof(ruleOperator))
				return B_IO_ERROR;
			int32 value2Len = (int32)rule.compareValue2.size();
			if (dest->Write(&value2Len, sizeof(value2Len)) != (ssize_t)sizeof(value2Len))
				return B_IO_ERROR;
			if (value2Len > 0 && dest->Write(rule.compareValue2.data(), value2Len) != value2Len)
				return B_IO_ERROR;
			int8 top10Bottom = rule.top10Bottom ? 1 : 0;
			int8 top10Percent = rule.top10Percent ? 1 : 0;
			int32 top10Rank = rule.top10Rank;
			if (dest->Write(&top10Bottom, sizeof(top10Bottom)) != (ssize_t)sizeof(top10Bottom)
				|| dest->Write(&top10Percent, sizeof(top10Percent)) != (ssize_t)sizeof(top10Percent)
				|| dest->Write(&top10Rank, sizeof(top10Rank)) != (ssize_t)sizeof(top10Rank))
				return B_IO_ERROR;
			int8 belowAverage = rule.belowAverage ? 1 : 0;
			int8 equalAverage = rule.equalAverage ? 1 : 0;
			if (dest->Write(&belowAverage, sizeof(belowAverage)) != (ssize_t)sizeof(belowAverage)
				|| dest->Write(&equalAverage, sizeof(equalAverage)) != (ssize_t)sizeof(equalAverage))
				return B_IO_ERROR;
		}
	}

	// Tabelle strutturate: un conteggio a zero (non servono per questo test).
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

	// Colonne valore esplicite di grafico incorporato: chartValueColCount=0.
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

	// Intervalli con nome: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Allineamento verticale: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Tabelle pivot: un conteggio a zero (non servono per questo test).
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Orientamento riga di grafico incorporato: chartRowOrientCount=0.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
	}

	// Tabelle pivot 2D: un conteggio a zero.
	{
		int32 zero = 0;
		if (dest->Write(&zero, sizeof(zero)) != (ssize_t)sizeof(zero))
			return B_IO_ERROR;
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
	int8* outType, std::string* outTitle, float outFrame[4] = NULL,
	std::vector<int16>* outValueColumns = NULL,
	bool* outRowOriented = NULL, std::vector<int16>* outValueRows = NULL)
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
	pos += titleLen;

	// Colonne valore esplicite (Task 2, colonne valore non adiacenti):
	// stessa sezione aggiunta dopo il titolo del grafico in WriteASCD
	// (XlsxTranslator.cpp) -- un contatore + un elenco di colonne
	// (int16) per grafico. Facoltativa da leggere qui: un chiamante che
	// non passa "outValueColumns" si ferma al titolo come prima di
	// questa aggiunta.
	if (outValueColumns)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 1) return false;

		if (pos + 4 > ascdLen) return false;
		int32 colCount;
		memcpy(&colCount, ascdData + pos, 4); pos += 4;
		if (colCount < 0 || pos + (size_t)colCount * 2 > ascdLen) return false;
		outValueColumns->clear();
		for (int32 c = 0; c < colCount; c++)
		{
			int16 col;
			memcpy(&col, ascdData + pos, 2); pos += 2;
			outValueColumns->push_back(col);
		}

		// Orientamento riga (vedi ChartObject::rowOriented/valueRows in
		// ui/src/Chart.h): NUOVA ultima sezione del formato, MOLTO piu'
		// in coda di quella colonne valore appena sopra -- fra le due,
		// WriteASCD scrive ancora area di stampa, margini/scala,
		// progetto VBA, celle sbloccate+protezione, intervalli con
		// nome, allineamento verticale e tabelle pivot (vedi l'ordine
		// completo delle sezioni in WriteASCD). Questo blocco le salta
		// tutte alla cieca (stessa struttura fissa gia' verificata dal
		// test dedicato su sample.xlsx, che cammina l'intero formato)
		// per raggiungere l'orientamento riga -- valido SOLO per una
		// prova senza nessuna di quelle sezioni popolata (nessuna area
		// di stampa/VBA/nome/pivot/cella protetta o esplicitamente
		// allineata), come ogni fixture minima di questo file usata coi
		// nuovi parametri opzionali.
		if (outRowOriented && outValueRows)
		{
			if (pos + 9 > ascdLen) return false; // area di stampa
			pos += 9;
			if (pos + 45 > ascdLen) return false; // margini/scala
			pos += 45;
			if (pos + 1 > ascdLen) return false; // progetto VBA
			pos += 1;
			if (pos + 4 > ascdLen) return false; // celle sbloccate
			pos += 4;
			if (pos + 1 > ascdLen) return false; // protezione foglio
			pos += 1;
			// Hash di protezione VERO (versione 9): due byte "presente"
			// seguiti da quattro stringhe (conteggio+byte, tutte vuote
			// in questa prova) e uno spinCount -- vedi il commento
			// gemello in ApplyNamesFromAscdForTest.
			if (pos + 2 > ascdLen) return false;
			pos += 2;
			for (int s = 0; s < 4; s++)
			{
				if (pos + 4 > ascdLen) return false;
				int32 strLen;
				memcpy(&strLen, ascdData + pos, 4); pos += 4;
				if (strLen < 0 || pos + (size_t)strLen > ascdLen) return false;
				pos += strLen;
			}
			if (pos + 4 > ascdLen) return false;
			pos += 4;
			if (pos + 4 > ascdLen) return false; // intervalli con nome
			pos += 4;
			if (pos + 4 > ascdLen) return false; // allineamento verticale
			{
				int32 valignCount;
				memcpy(&valignCount, ascdData + pos, 4); pos += 4;
				if (valignCount < 0 || pos + (size_t)valignCount * 5 > ascdLen) return false;
				pos += valignCount * 5;
			}
			if (pos + 4 > ascdLen) return false; // tabelle pivot: solo il conteggio, mai popolate qui
			{
				int32 pivotCount;
				memcpy(&pivotCount, ascdData + pos, 4); pos += 4;
				if (pivotCount != 0) return false;
			}

			// Orientamento riga: un contatore di grafici (qui sempre 1,
			// stesso principio delle sezioni tipo/titolo sopra) seguito
			// da un record per grafico -- il contatore che avevo
			// dimenticato la prima volta, causando una lettura
			// disallineata di un intero byte (rowCount letto a partire
			// dal byte "rowOriented" vero, non dal vero inizio di
			// rowCount).
			if (pos + 4 > ascdLen) return false;
			{
				int32 n;
				memcpy(&n, ascdData + pos, 4); pos += 4;
				if (n != 1) return false;
			}

			if (pos + 1 > ascdLen) return false;
			uint8 rowOriented;
			memcpy(&rowOriented, ascdData + pos, 1); pos += 1;
			*outRowOriented = rowOriented != 0;

			if (pos + 4 > ascdLen) return false;
			int32 rowCount;
			memcpy(&rowCount, ascdData + pos, 4); pos += 4;
			if (rowCount < 0 || pos + (size_t)rowCount * 2 > ascdLen) return false;
			outValueRows->clear();
			for (int32 r = 0; r < rowCount; r++)
			{
				int16 row;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				outValueRows->push_back(row);
			}
		}
	}

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
// Legge la PRIMA voce della sezione "cellColors" (sfondo/testo per
// cella, la stessa gia' verificata per A1/B1 di sample.xlsx piu' sopra
// nel test grande) -- usata per verificare l'importazione del colore
// indicizzato legacy (indexed="N"), che finisce proprio in questa
// sezione come qualunque altro colore di sfondo risolto.
static bool ReadFirstCellColorFromAscdForTest(const unsigned char* ascdData, size_t ascdLen,
	cell* outCell, rgb_color* outBg)
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

	// Grafici incorporati, colWidths: due contatori (0 in questo
	// documento di prova).
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}

	if (pos + 4 > ascdLen) return false;
	int32 cellColorCount;
	memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
	if (cellColorCount < 1 || pos + 12 > ascdLen)
		return false;

	int16 row, col;
	uint8 bg[4], fg[4];
	memcpy(&row, ascdData + pos, 2); pos += 2;
	memcpy(&col, ascdData + pos, 2); pos += 2;
	memcpy(bg, ascdData + pos, 4); pos += 4;
	memcpy(fg, ascdData + pos, 4); pos += 4;
	(void)fg;

	outCell->Set(col, row);
	outBg->red = bg[0]; outBg->green = bg[1]; outBg->blue = bg[2]; outBg->alpha = bg[3];
	return true;
}

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

// Same idea as ApplyNamesFromAscdForTest above, but stops at the page
// margins/scale section instead of continuing to named ranges (100%
// XLSX standard compatibility, Tier 2 -- print settings, item 6):
// assumes a plain fixture with no chart/comment/hyperlink/border/
// validation/table/print-area, matching every actual caller today.
static bool ReadPrintSettingsFromAscdForTest(const unsigned char* ascdData, size_t ascdLen,
	bool* outHasSettings, double* outMarginTop, double* outMarginBottom,
	double* outMarginLeft, double* outMarginRight, int32* outScaleMode, double* outScalePercent)
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
	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto contatori (0).
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
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }
	// AutoFilter: 9 byte fissi.
	if (pos + 9 > ascdLen) return false;
	pos += 9;
	// Commenti, collegamenti: due contatori (0).
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Tipo di grafico, colore bordo, convalida dati, formattazione
	// condizionale, tabelle: cinque contatori (0).
	for (int s = 0; s < 5; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Titolo di grafico: un contatore (0).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }
	// Colonne valore esplicite di grafico (Task 2): un contatore (0).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }
	// Area di stampa: 1 byte "has" + 4 int16 fissi (9 byte totali),
	// assunta assente (0) -- item separato dal roadmap, non toccato qui.
	if (pos + 9 > ascdLen) return false;
	if (ascdData[pos] != 0) return false;
	pos += 9;

	// Margini/scala: i dati veri, 1 byte "has" + 4 double + 1 int32 + 1 double.
	if (pos + 1 > ascdLen) return false;
	uint8 has = ascdData[pos]; pos += 1;
	*outHasSettings = (has != 0);

	if (pos + 4 * 8 + 4 + 8 > ascdLen) return false;
	memcpy(outMarginTop, ascdData + pos, 8); pos += 8;
	memcpy(outMarginBottom, ascdData + pos, 8); pos += 8;
	memcpy(outMarginLeft, ascdData + pos, 8); pos += 8;
	memcpy(outMarginRight, ascdData + pos, 8); pos += 8;
	memcpy(outScaleMode, ascdData + pos, 4); pos += 4;
	memcpy(outScalePercent, ascdData + pos, 8); pos += 8;

	return true;
}

// Same idea as ReadPrintSettingsFromAscdForTest above, but stops at
// the print-area section instead of continuing to margins/scale (100%
// XLSX standard compatibility, Tier 2 -- print settings, step 3 of 4):
// same plain-fixture assumptions (no chart/comment/hyperlink/border/
// validation/table).
static bool ReadFirstPrintAreaFromAscdForTest(const unsigned char* ascdData, size_t ascdLen,
	bool* outHasArea, int16* outTop, int16* outLeft, int16* outBottom, int16* outRight)
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
	// fonts, alignment, borders, numberFormat, underline, wrapText,
	// mergedCells, images: otto contatori (0).
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
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }
	// AutoFilter: 9 byte fissi.
	if (pos + 9 > ascdLen) return false;
	pos += 9;
	// Commenti, collegamenti: due contatori (0).
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Tipo di grafico, colore bordo, convalida dati, formattazione
	// condizionale, tabelle: cinque contatori (0).
	for (int s = 0; s < 5; s++)
	{
		if (pos + 4 > ascdLen) return false;
		int32 n;
		memcpy(&n, ascdData + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Titolo di grafico: un contatore (0).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }
	// Colonne valore esplicite di grafico (Task 2): un contatore (0).
	if (pos + 4 > ascdLen) return false;
	{ int32 n; memcpy(&n, ascdData + pos, 4); pos += 4; if (n != 0) return false; }

	// Area di stampa: i dati veri, 1 byte "has" + 4 int16.
	if (pos + 9 > ascdLen) return false;
	uint8 has = ascdData[pos]; pos += 1;
	*outHasArea = (has != 0);
	memcpy(outTop, ascdData + pos, 2); pos += 2;
	memcpy(outLeft, ascdData + pos, 2); pos += 2;
	memcpy(outBottom, ascdData + pos, 2); pos += 2;
	memcpy(outRight, ascdData + pos, 2); pos += 2;

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
	// Colonne valore esplicite di grafico (Task 2): un conteggio (0).
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

	// Hash di protezione VERO (versione 9): due byte "presente" seguiti
	// da quattro stringhe (conteggio+byte) e uno spinCount -- scritti
	// comunque dal vero WriteASCD per OGNI foglio, stesso principio di
	// ogni altra sezione a lunghezza fissa sopra.
	if (pos + 2 > len) return false;
	pos += 2;
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 strLen; memcpy(&strLen, data + pos, 4); pos += 4;
		if (strLen < 0 || pos + (size_t)strLen > len) return false;
		pos += strLen;
	}
	if (pos + 4 > len) return false;
	pos += 4;

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

// Same walk as ApplyNamesFromAscdForTest above (same assumptions: no
// chart, no VBA project, every other trailing section empty), but
// continues PAST named ranges into vertical alignment (assumed empty
// too) and then into the tabelle pivot section (Fase 3 delle tabelle
// pivot -- vedi ROADMAP.md/CHANGELOG.md), the real new LAST section of
// the format, reading its real data instead of skipping it. Names
// themselves are skipped (not applied to any doc) since this caller
// doesn't need them.
static bool ReadFirstPivotFromAscdForTest(const unsigned char* data, size_t len, size_t pos,
	std::vector<PivotTableObject>* out)
{
	out->clear();

	if (pos + 4 > len) return false;
	int32 chartCount;
	memcpy(&chartCount, data + pos, 4); pos += 4;
	if (chartCount != 0) return false;

	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 8 > len) return false;
	pos += 8;
	for (int s = 0; s < 8; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 1 > len) return false;
	pos += 1;
	if (pos + 4 > len) return false;
	pos += 4;
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	if (pos + 9 > len) return false;
	pos += 9;
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	// Colonne valore esplicite di grafico (Task 2): un conteggio (0).
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	if (pos + 9 > len) return false;
	pos += 9;
	if (pos + 45 > len) return false;
	pos += 45;
	if (pos + 1 > len) return false;
	if (data[pos] != 0) return false;
	pos += 1;
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	if (pos + 1 > len) return false;
	pos += 1;

	// Hash di protezione VERO (versione 9): stesso schema a lunghezza
	// fissa gia' usato sopra per le altre sezioni di questa funzione --
	// vedi il commento gemello in ApplyNamesFromAscdForTest.
	if (pos + 2 > len) return false;
	pos += 2;
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 strLen; memcpy(&strLen, data + pos, 4); pos += 4;
		if (strLen < 0 || pos + (size_t)strLen > len) return false;
		pos += strLen;
	}
	if (pos + 4 > len) return false;
	pos += 4;

	// Intervalli con nome: saltati, non serve applicarli qui.
	if (pos + 4 > len) return false;
	int32 nameCount;
	memcpy(&nameCount, data + pos, 4); pos += 4;
	for (int32 i = 0; i < nameCount; i++)
	{
		if (pos + 4 > len) return false;
		int32 nameLen;
		memcpy(&nameLen, data + pos, 4); pos += 4;
		if (nameLen < 0 || pos + (size_t)nameLen > len) return false;
		pos += nameLen;
		if (pos + 8 > len) return false;
		pos += 8;
	}

	// Allineamento verticale: assunto vuoto (nessuna cella del fixture
	// usa un allineamento non predefinito).
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }

	// Tabelle pivot (Fase 3), NUOVA ultima sezione del formato: i dati
	// veri, stesso schema byte per byte di ui/src/AscdIO.cpp (SaveASCD).
	if (pos + 4 > len) return false;
	int32 pivotCount;
	memcpy(&pivotCount, data + pos, 4); pos += 4;
	if (pivotCount < 0) return false;

	for (int32 i = 0; i < pivotCount; i++)
	{
		if (pos + 16 > len) return false;
		int16 srcLeft, srcTop, srcRight, srcBottom, destCol, destRow;
		int32 aggFunc;
		memcpy(&srcLeft, data + pos, 2); pos += 2;
		memcpy(&srcTop, data + pos, 2); pos += 2;
		memcpy(&srcRight, data + pos, 2); pos += 2;
		memcpy(&srcBottom, data + pos, 2); pos += 2;
		memcpy(&destCol, data + pos, 2); pos += 2;
		memcpy(&destRow, data + pos, 2); pos += 2;
		memcpy(&aggFunc, data + pos, 4); pos += 4;

		PivotTableObject pivot;
		pivot.sourceRange = range(srcLeft, srcTop, srcRight, srcBottom);
		pivot.destAnchor = cell(destCol, destRow);
		pivot.aggFunc = (PivotAggFunc)aggFunc;

		if (pos + 4 > len) return false;
		int32 rowCount;
		memcpy(&rowCount, data + pos, 4); pos += 4;
		if (rowCount < 0) return false;

		for (int32 r = 0; r < rowCount; r++)
		{
			if (pos + 4 > len) return false;
			int32 catCount;
			memcpy(&catCount, data + pos, 4); pos += 4;
			if (catCount < 0) return false;

			PivotRow row;
			for (int32 k = 0; k < catCount; k++)
			{
				if (pos + 4 > len) return false;
				int32 catLen;
				memcpy(&catLen, data + pos, 4); pos += 4;
				if (catLen < 0 || pos + (size_t)catLen > len) return false;
				row.categories.push_back(BString((const char*)data + pos, catLen));
				pos += catLen;
			}

			if (pos + 28 > len) return false;
			int32 count32;
			memcpy(&row.aggregate, data + pos, 8); pos += 8;
			memcpy(&count32, data + pos, 4); pos += 4;
			memcpy(&row.minVal, data + pos, 8); pos += 8;
			memcpy(&row.maxVal, data + pos, 8); pos += 8;
			row.count = count32;

			pivot.cachedRows.push_back(row);
		}

		out->push_back(pivot);
	}

	return true;
}

// Same walk as ReadFirstPivotFromAscdForTest above, but continues PAST
// the 1D pivot section into chart-rowOriented (assumed empty, count 0)
// and then the pivot 2D section (Fase pivot 2D: campo Colonne + misure
// multiple), the real new LAST section of the format -- applies the 2D
// fields to the FIRST entry of "out" by index, same principle as the
// real ReadASCD (ui/src/AscdIO.cpp/translators/xlsx/XlsxTranslator.cpp).
static bool ReadFirstPivot2DFromAscdForTest(const unsigned char* data, size_t len, size_t pos,
	std::vector<PivotTableObject>* out)
{
	if (!ReadFirstPivotFromAscdForTest(data, len, pos, out))
		return false;
	if (out->empty())
		return false;

	// Ripete lo stesso walk di ReadFirstPivotFromAscdForTest per
	// ritrovare "pos" alla fine della sezione pivot 1D -- duplicato
	// invece di farsela restituire, stesso principio di duplicazione
	// gia' seguito da questo file per ogni altro walker simile.
	if (pos + 4 > len) return false;
	int32 chartCount;
	memcpy(&chartCount, data + pos, 4); pos += 4;
	if (chartCount != 0) return false;

	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 8 > len) return false;
	pos += 8;
	for (int s = 0; s < 8; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 1 > len) return false;
	pos += 1;
	if (pos + 4 > len) return false;
	pos += 4;
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	if (pos + 9 > len) return false;
	pos += 9;
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	if (pos + 9 > len) return false;
	pos += 9;
	if (pos + 45 > len) return false;
	pos += 45;
	if (pos + 1 > len) return false;
	if (data[pos] != 0) return false;
	pos += 1;
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	if (pos + 1 > len) return false;
	pos += 1;

	// Hash di protezione VERO (versione 9): stesso schema a lunghezza
	// fissa gia' usato sopra, vedi il commento gemello in
	// ApplyNamesFromAscdForTest/ReadFirstPivotFromAscdForTest.
	if (pos + 2 > len) return false;
	pos += 2;
	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 strLen; memcpy(&strLen, data + pos, 4); pos += 4;
		if (strLen < 0 || pos + (size_t)strLen > len) return false;
		pos += strLen;
	}
	if (pos + 4 > len) return false;
	pos += 4;

	if (pos + 4 > len) return false;
	int32 nameCount;
	memcpy(&nameCount, data + pos, 4); pos += 4;
	for (int32 i = 0; i < nameCount; i++)
	{
		if (pos + 4 > len) return false;
		int32 nameLen;
		memcpy(&nameLen, data + pos, 4); pos += 4;
		if (nameLen < 0 || pos + (size_t)nameLen > len) return false;
		pos += nameLen;
		if (pos + 8 > len) return false;
		pos += 8;
	}

	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }

	if (pos + 4 > len) return false;
	int32 pivotCount;
	memcpy(&pivotCount, data + pos, 4); pos += 4;
	if (pivotCount < 0) return false;

	for (int32 i = 0; i < pivotCount; i++)
	{
		if (pos + 16 > len) return false;
		pos += 16;
		if (pos + 4 > len) return false;
		int32 rowCount;
		memcpy(&rowCount, data + pos, 4); pos += 4;
		if (rowCount < 0) return false;
		for (int32 r = 0; r < rowCount; r++)
		{
			if (pos + 4 > len) return false;
			int32 catCount;
			memcpy(&catCount, data + pos, 4); pos += 4;
			if (catCount < 0) return false;
			for (int32 k = 0; k < catCount; k++)
			{
				if (pos + 4 > len) return false;
				int32 catLen;
				memcpy(&catLen, data + pos, 4); pos += 4;
				if (catLen < 0 || pos + (size_t)catLen > len) return false;
				pos += catLen;
			}
			if (pos + 28 > len) return false;
			pos += 28;
		}
	}

	// Orientamento riga di grafico incorporato: assunto vuoto (nessun
	// grafico in questo test).
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }

	// Tabelle pivot 2D, NUOVA ultima sezione del formato: i dati veri,
	// stesso schema byte per byte del vero SaveASCD/WriteASCD.
	if (pos + 4 > len) return false;
	int32 pivot2DCount;
	memcpy(&pivot2DCount, data + pos, 4); pos += 4;
	if (pivot2DCount < 0) return false;

	for (int32 i = 0; i < pivot2DCount; i++)
	{
		if (pos + 2 > len) return false;
		int16 columnFieldCol;
		memcpy(&columnFieldCol, data + pos, 2); pos += 2;

		if (pos + 4 > len) return false;
		int32 measureCount;
		memcpy(&measureCount, data + pos, 4); pos += 4;
		if (measureCount < 0) return false;

		std::vector<PivotMeasure> measures;
		for (int32 m = 0; m < measureCount; m++)
		{
			if (pos + 10 > len) return false;
			PivotMeasure measure;
			memcpy(&measure.sourceCol, data + pos, 2); pos += 2;
			int32 aggFunc;
			memcpy(&aggFunc, data + pos, 4); pos += 4;
			measure.aggFunc = (PivotAggFunc)aggFunc;
			int32 labelLen;
			memcpy(&labelLen, data + pos, 4); pos += 4;
			if (labelLen < 0 || pos + (size_t)labelLen > len) return false;
			measure.label.SetTo((const char*)data + pos, labelLen);
			pos += labelLen;
			measures.push_back(measure);
		}

		if (pos + 4 > len) return false;
		int32 columnValueCount;
		memcpy(&columnValueCount, data + pos, 4); pos += 4;
		if (columnValueCount < 0) return false;

		std::vector<BString> columnValues;
		for (int32 c = 0; c < columnValueCount; c++)
		{
			if (pos + 4 > len) return false;
			int32 valLen;
			memcpy(&valLen, data + pos, 4); pos += 4;
			if (valLen < 0 || pos + (size_t)valLen > len) return false;
			columnValues.push_back(BString((const char*)data + pos, valLen));
			pos += valLen;
		}

		if (pos + 4 > len) return false;
		int32 rowCount2D;
		memcpy(&rowCount2D, data + pos, 4); pos += 4;
		if (rowCount2D < 0) return false;

		int32 numColSlots = columnValueCount > 0 ? columnValueCount : 1;
		std::vector<PivotRow2D> rows2D;
		for (int32 r = 0; r < rowCount2D; r++)
		{
			PivotRow2D row;
			if (pos + 4 > len) return false;
			int32 catCount;
			memcpy(&catCount, data + pos, 4); pos += 4;
			if (catCount < 0) return false;
			for (int32 k = 0; k < catCount; k++)
			{
				if (pos + 4 > len) return false;
				int32 catLen;
				memcpy(&catLen, data + pos, 4); pos += 4;
				if (catLen < 0 || pos + (size_t)catLen > len) return false;
				row.categories.push_back(BString((const char*)data + pos, catLen));
				pos += catLen;
			}
			row.cells.resize(numColSlots);
			for (int32 c = 0; c < numColSlots; c++)
			{
				row.cells[c].resize(measureCount);
				for (int32 m = 0; m < measureCount; m++)
				{
					if (pos + 28 > len) return false;
					PivotCellAgg agg;
					int32 count32;
					memcpy(&agg.aggregate, data + pos, 8); pos += 8;
					memcpy(&count32, data + pos, 4); pos += 4;
					memcpy(&agg.minVal, data + pos, 8); pos += 8;
					memcpy(&agg.maxVal, data + pos, 8); pos += 8;
					agg.count = count32;
					row.cells[c][m] = agg;
				}
			}
			rows2D.push_back(row);
		}

		if (i < (int32)out->size())
		{
			(*out)[i].columnFieldCol = columnFieldCol;
			(*out)[i].measures = measures;
			(*out)[i].columnValues = columnValues;
			(*out)[i].cachedRows2D = rows2D;
		}
	}

	return true;
}

// Same walk as ReadFirstPivotFromAscdForTest above through the border
// color count, but then reads the REAL conditional-formatting rules
// (byte-for-byte identical reconstruction logic to the real LoadASCD in
// ui/src/AscdIO.cpp) instead of asserting they're absent -- used to
// verify the export->reimport round trip for the new XLSX conditional-
// formatting export.
static bool ReadCondFormatRulesFromAscdForTest(const unsigned char* data, size_t len, size_t pos,
	std::vector<ConditionalFormatRule>* out)
{
	out->clear();

	if (pos + 4 > len) return false;
	int32 chartCount;
	memcpy(&chartCount, data + pos, 4); pos += 4;
	if (chartCount != 0) return false;

	for (int s = 0; s < 4; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 8 > len) return false;
	pos += 8;
	for (int s = 0; s < 8; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	if (pos + 1 > len) return false;
	pos += 1;
	if (pos + 4 > len) return false;
	pos += 4;
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	if (pos + 9 > len) return false;
	pos += 9;
	for (int s = 0; s < 2; s++)
	{
		if (pos + 4 > len) return false;
		int32 n; memcpy(&n, data + pos, 4); pos += 4;
		if (n != 0) return false;
	}
	// Tipo di grafico incorporato: un conteggio (0).
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	// Colore del bordo di cella: un conteggio (0).
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }
	// Convalida dati: un conteggio (0).
	if (pos + 4 > len) return false;
	{ int32 n; memcpy(&n, data + pos, 4); pos += 4; if (n != 0) return false; }

	// Formattazione condizionale: i dati VERI.
	if (pos + 4 > len) return false;
	int32 count;
	memcpy(&count, data + pos, 4); pos += 4;
	if (count < 0) return false;

	for (int32 i = 0; i < count; i++)
	{
		if (pos + 5 > len) return false;
		int8 type; int32 valueLen;
		memcpy(&type, data + pos, 1); pos += 1;
		memcpy(&valueLen, data + pos, 4); pos += 4;
		if (valueLen < 0 || pos + (size_t)valueLen > len) return false;

		ConditionalFormatRule rule;
		rule.type = (CondFormatRuleType)type;
		if (valueLen > 0)
		{
			rule.compareValue.assign((const char*)data + pos, valueLen);
			pos += valueLen;
		}

		if (pos + sizeof(rgb_color) > len) return false;
		memcpy(&rule.bgColor, data + pos, sizeof(rgb_color)); pos += sizeof(rgb_color);

		if (pos + 4 > len) return false;
		int32 rangeCount;
		memcpy(&rangeCount, data + pos, 4); pos += 4;
		if (rangeCount < 0) return false;
		for (int32 r = 0; r < rangeCount; r++)
		{
			if (pos + 8 > len) return false;
			int16 left, top, right, bottom;
			memcpy(&left, data + pos, 2); pos += 2;
			memcpy(&top, data + pos, 2); pos += 2;
			memcpy(&right, data + pos, 2); pos += 2;
			memcpy(&bottom, data + pos, 2); pos += 2;
			rule.ranges.push_back(range(left, top, right, bottom));
		}

		if (pos + 4 > len) return false;
		int32 pointCount;
		memcpy(&pointCount, data + pos, 4); pos += 4;
		if (pointCount < 0) return false;
		for (int32 p = 0; p < pointCount; p++)
		{
			if (pos + 4 > len) return false;
			int32 cfvoTypeLen;
			memcpy(&cfvoTypeLen, data + pos, 4); pos += 4;
			if (cfvoTypeLen < 0 || pos + (size_t)cfvoTypeLen > len) return false;

			ColorScalePoint point;
			if (cfvoTypeLen > 0)
			{
				point.cfvoType.assign((const char*)data + pos, cfvoTypeLen);
				pos += cfvoTypeLen;
			}
			if (pos + sizeof(double) + sizeof(rgb_color) > len) return false;
			memcpy(&point.cfvoValue, data + pos, sizeof(double)); pos += sizeof(double);
			memcpy(&point.color, data + pos, sizeof(rgb_color)); pos += sizeof(rgb_color);
			rule.colorScalePoints.push_back(point);
		}

		if (pos + 5 > len) return false;
		int8 compareIsCellRef;
		int16 compareRefCol, compareRefRow;
		memcpy(&compareIsCellRef, data + pos, 1); pos += 1;
		memcpy(&compareRefCol, data + pos, 2); pos += 2;
		memcpy(&compareRefRow, data + pos, 2); pos += 2;
		rule.compareIsCellRef = compareIsCellRef != 0;
		rule.compareRefCell = cell(compareRefCol, compareRefRow);

		if (pos + 4 > len) return false;
		int32 exprLen;
		memcpy(&exprLen, data + pos, 4); pos += 4;
		if (exprLen < 0 || pos + (size_t)exprLen > len) return false;
		if (exprLen > 0)
		{
			rule.expressionFormula.assign((const char*)data + pos, exprLen);
			pos += exprLen;
		}

		if (pos + sizeof(rgb_color) > len) return false;
		memcpy(&rule.dataBarColor, data + pos, sizeof(rgb_color)); pos += sizeof(rgb_color);

		if (pos + 4 > len) return false;
		int32 iconStyleLen;
		memcpy(&iconStyleLen, data + pos, 4); pos += 4;
		if (iconStyleLen < 0 || pos + (size_t)iconStyleLen > len) return false;
		if (iconStyleLen > 0)
		{
			rule.iconSetStyle.assign((const char*)data + pos, iconStyleLen);
			pos += iconStyleLen;
		}

		// Versione 8 (Path to full Excel parity, Tier 3): stesso ordine
		// byte-per-byte del vero LoadASCD.
		if (pos + 1 > len) return false;
		int8 ruleOperator;
		memcpy(&ruleOperator, data + pos, 1); pos += 1;
		rule.ruleOperator = ruleOperator;

		if (pos + 4 > len) return false;
		int32 value2Len;
		memcpy(&value2Len, data + pos, 4); pos += 4;
		if (value2Len < 0 || pos + (size_t)value2Len > len) return false;
		if (value2Len > 0)
		{
			rule.compareValue2.assign((const char*)data + pos, value2Len);
			pos += value2Len;
		}

		if (pos + 6 > len) return false;
		int8 top10Bottom, top10Percent;
		int32 top10Rank;
		memcpy(&top10Bottom, data + pos, 1); pos += 1;
		memcpy(&top10Percent, data + pos, 1); pos += 1;
		memcpy(&top10Rank, data + pos, 4); pos += 4;
		rule.top10Bottom = top10Bottom != 0;
		rule.top10Percent = top10Percent != 0;
		rule.top10Rank = top10Rank;

		if (pos + 2 > len) return false;
		int8 belowAverage, equalAverage;
		memcpy(&belowAverage, data + pos, 1); pos += 1;
		memcpy(&equalAverage, data + pos, 1); pos += 1;
		rule.belowAverage = belowAverage != 0;
		rule.equalAverage = equalAverage != 0;

		out->push_back(rule);
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

						// Colonne valore esplicite di grafico incorporato
						// (Task 2, colonne valore non adiacenti): stesso
						// principio della sezione titolo appena sopra --
						// sample.xlsx non ha grafici, quindi il conteggio
						// e' zero e non ci sono record a seguire.
						if (pos + 4 <= ascdLen)
						{
							int32 chartValueColCount;
							memcpy(&chartValueColCount, ascdData + pos, 4); pos += 4;
							Check(chartValueColCount == 0,
								"nessun grafico in sample.xlsx, il contatore delle colonne valore e' zero");
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

						// Hash di protezione VERO (versione 9, Tier 4 "Path
						// to 100% XLSX standard compatibility"): due byte
						// "presente" (qui zero, sample.xlsx non e'
						// protetto) seguiti da quattro conteggi/stringhe
						// (qui tutti vuoti) e uno spinCount (qui zero) --
						// scritti comunque per OGNI foglio, stesso principio
						// di ogni altra sezione a lunghezza fissa sopra.
						if (pos + 2 <= ascdLen)
						{
							uint8 hasPassword = ascdData[pos]; pos += 1;
							uint8 isModernHash = ascdData[pos]; pos += 1;
							Check(hasPassword == 0 && isModernHash == 0,
								"sample.xlsx non ha nessun hash di protezione, entrambi i byte sono zero");
						}
						for (int s = 0; s < 4; s++)
						{
							if (pos + 4 > ascdLen)
								break;
							int32 strLen;
							memcpy(&strLen, ascdData + pos, 4); pos += 4;
							Check(strLen == 0,
								"sample.xlsx non ha nessun hash di protezione, le quattro stringhe sono vuote");
							pos += strLen;
						}
						if (pos + 4 <= ascdLen)
						{
							int32 spinCount;
							memcpy(&spinCount, ascdData + pos, 4); pos += 4;
							Check(spinCount == 0,
								"sample.xlsx non ha nessun hash di protezione, spinCount e' zero");
						}

						// Intervalli con nome ("100% XLSX standard
						// compatibility" plan): un conteggio (qui zero,
						// sample.xlsx non ha <definedNames>).
						if (pos + 4 <= ascdLen)
						{
							int32 nameCount;
							memcpy(&nameCount, ascdData + pos, 4); pos += 4;
							Check(nameCount == 0,
								"nessun intervallo con nome in sample.xlsx, il conteggio e' zero");
						}

						// Allineamento verticale non predefinito (Fase
						// "Add vertical cell alignment"): un conteggio,
						// ORA l'ULTIMA sezione del formato, seguito da
						// altrettanti record (riga int16, colonna int16,
						// valore int8). Da quando l'importazione XLSX
						// applica il vero default di Excel (Bottom, non
						// Top -- bug reale scoperto confrontando
						// agile-kanban-board.xlsx con Excel vero, celle
						// "Days"/"14" senza allineamento verticale
						// esplicito nel file ma rese in basso da Excel)
						// OGNI cella con un s="..." risolto in
						// sample.xlsx riceve un allineamento verticale
						// esplicito, quindi il conteggio non e' piu'
						// zero: si legge e si scarta solo per avanzare
						// "pos" correttamente fino alla fine del buffer.
						if (pos + 4 <= ascdLen)
						{
							int32 valignCount;
							memcpy(&valignCount, ascdData + pos, 4); pos += 4;
							Check(valignCount > 0,
								"sample.xlsx ha celle stilizzate, quindi almeno un allineamento "
								"verticale esplicito (il vero default Bottom di Excel) e' persistito");
							pos += valignCount * (2 + 2 + 1);
						}

						// Tabelle pivot (Fase 3 delle tabelle pivot -- vedi
						// ROADMAP.md/CHANGELOG.md): un conteggio -- sample.xlsx
						// non ha nessuna tabella pivot, quindi il conteggio e'
						// zero e non ci sono record a seguire.
						if (pos + 4 <= ascdLen)
						{
							int32 pivotCount;
							memcpy(&pivotCount, ascdData + pos, 4); pos += 4;
							Check(pivotCount == 0,
								"nessuna tabella pivot in sample.xlsx, il conteggio e' zero");
						}

						// Orientamento riga di grafico incorporato: un
						// conteggio, la NUOVISSIMA ultima sezione del
						// formato -- sample.xlsx non ha nessun grafico,
						// quindi il conteggio e' zero e non ci sono
						// record a seguire.
						if (pos + 4 <= ascdLen)
						{
							int32 chartRowOrientCount;
							memcpy(&chartRowOrientCount, ascdData + pos, 4); pos += 4;
							Check(chartRowOrientCount == 0,
								"nessun grafico in sample.xlsx, il conteggio orientamento riga e' zero");
						}

						// Tabelle pivot 2D (campo Colonne + misure multiple):
						// un conteggio, la NUOVISSIMA ultima sezione del
						// formato -- sample.xlsx non ha nessuna tabella
						// pivot, quindi il conteggio e' zero e non ci sono
						// record a seguire.
						if (pos + 4 <= ascdLen)
						{
							int32 pivot2DCount;
							memcpy(&pivot2DCount, ascdData + pos, 4); pos += 4;
							Check(pivot2DCount == 0,
								"nessuna tabella pivot in sample.xlsx, il conteggio pivot 2D e' zero");
						}

						// Stile tabella con nome (Tier 4): un conteggio, la
						// NUOVISSIMA ultima sezione del formato -- sample.xlsx
						// non ha nessuna tabella strutturata, quindi il
						// conteggio e' zero e non ci sono record a seguire.
						if (pos + 4 <= ascdLen)
						{
							int32 tableStyleCount;
							memcpy(&tableStyleCount, ascdData + pos, 4); pos += 4;
							Check(tableStyleCount == 0,
								"nessuna tabella strutturata in sample.xlsx, il conteggio stile tabella e' zero");
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

	// Tabelle strutturate (Fase 12, riga totali aggiunta piu' tardi):
	// tests/sample_table.xlsx ha una tabella A1:B5 (TableStyleMedium2,
	// showRowStripes="1", totalsRowCount="1") -- A1 e' l'intestazione
	// (mai bandata), A2/B2 e A4/B4 sono la prima e terza riga dati
	// (bandate), A3/B3 la seconda (non bandata, alternanza corretta),
	// A5/B5 e' la riga totali finale (mai bandata ne' inclusa nei dati:
	// vedi RegisterTable/ApplyTableBanding in XlsxTranslator.cpp -- se
	// la riga totali finisse in CTableDef::dataRange, un "SUBTOTAL"
	// scritto davvero da Excel in quella riga per aggregare
	// "Tabella1[Colonna]" si autoincluderebbe nel proprio argomento).
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
			Check(count == 10, "l'ASCD contiene le 10 celle di sample_table.xlsx (incluse A5/B5, la riga totali)");

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

				// sample_table.xlsx dichiara <tableStyleInfo
				// name="TableStyleMedium2">, uno degli stili riconosciuti
				// da TableStyles.h (Tier 4 "named table styles") -- la
				// banda usa quindi il suo colore approssimato (blu), non
				// piu' il grigio neutro di prima di questa fase.
				bool isBand = bg.red == 197 && bg.green == 217 && bg.blue == 241;
				if (row == 2 && col == 1) { foundA2 = true; colorsCorrect &= isBand; }
				if (row == 2 && col == 2) { foundB2 = true; colorsCorrect &= isBand; }
				if (row == 4 && col == 1) { foundA4 = true; colorsCorrect &= isBand; }
				if (row == 4 && col == 2) { foundB4 = true; colorsCorrect &= isBand; }
				if (row == 1 || row == 3 || row == 5)
					Check(false, "nessuna cella dell'intestazione, della riga dati pari o della riga totali ha un colore");
			}

			Check(foundA2 && foundB2 && foundA4 && foundB4 && colorsCorrect,
				"A2/B2 (prima riga dati) e A4/B4 (terza) hanno il colore di banda di TableStyleMedium2 (blu)");

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
				// A1:B5 nel file originale (totalsRowCount="1"),
				// intestazione (riga 1) E riga totali (riga 5) escluse
				// da CTableDef::dataRange: A2:B4. Se RegisterTable
				// smettesse di sottrarre la riga totali (regressione),
				// tBottom tornerebbe a 5.
				Check(tLeft == 1 && tTop == 2 && tRight == 2 && tBottom == 4,
					"l'intervallo dati (A2:B4, intestazione e riga totali escluse) e' quello corretto");

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
			// verificare: le quattro regole VIVE importate da
			// sample_condformat.xlsx (non piu' un colore congelato) --
			// la terza (colorScale su C1:C10, Fase 33/A punto 6), la
			// quarta (dataBar su D1:D10, Tier 3 Fase B) e la quinta
			// (iconSet su E1:E10, Tier 3 Fase C) sono state aggiunte al
			// file di prova insieme al resto.
			int32 ruleCount = 0;
			if (sectionsOk && pos + 4 <= ascdLen)
			{
				memcpy(&ruleCount, ascdData + pos, 4); pos += 4;
			}
			Check(ruleCount == 5,
				"cinque regole di formattazione condizionale importate da sample_condformat.xlsx");

			bool foundCellIsRule = false, foundDuplicatesRule = false, foundColorScaleRule = false;
			bool foundDataBarRule = false, foundIconSetRule = false;
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

				bool rangeMatchesA1A3 = false, rangeMatchesB1B3 = false, rangeMatchesC1C10 = false;
				bool rangeMatchesD1D10 = false, rangeMatchesE1E10 = false;
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
					if (left == 3 && right == 3 && top == 1 && bottom == 10)
						rangeMatchesC1C10 = true;
					if (left == 4 && right == 4 && top == 1 && bottom == 10)
						rangeMatchesD1D10 = true;
					if (left == 5 && right == 5 && top == 1 && bottom == 10)
						rangeMatchesE1E10 = true;
				}

				// Punti di controllo della scala di colori (versione 3
				// del formato ASCD, Fase 33/A punto 5): zero per le
				// prime due regole di questo file di prova, tre per la
				// terza (colorScale su C1:C10, Fase 33/A punto 6) --
				// letti per davvero, non solo saltati, per verificare
				// che l'importazione da <colorScale>/<cfvo> sia corretta.
				std::vector<std::string> cfvoTypes;
				std::vector<double> cfvoValues;
				std::vector<int> cfvoColorsPacked;
				if (pos + 4 <= ascdLen)
				{
					int32 pointCount;
					memcpy(&pointCount, ascdData + pos, 4); pos += 4;
					for (int32 p = 0; p < pointCount && pos + 4 <= ascdLen; p++)
					{
						int32 cfvoTypeLen;
						memcpy(&cfvoTypeLen, ascdData + pos, 4); pos += 4;
						if (pos + (size_t)cfvoTypeLen + 8 + 4 > ascdLen)
							break;
						cfvoTypes.push_back(std::string((const char*)ascdData + pos, cfvoTypeLen));
						pos += cfvoTypeLen; // cfvoType

						double val;
						memcpy(&val, ascdData + pos, 8); pos += 8;
						cfvoValues.push_back(val);

						rgb_color pointColor;
						memcpy(&pointColor, ascdData + pos, 4); pos += 4;
						cfvoColorsPacked.push_back(
							(pointColor.red << 16) | (pointColor.green << 8) | pointColor.blue);
					}
				}

				// Riferimento di cella per il confronto (versione 4 del
				// formato ASCD): 1 byte (compareIsCellRef) + due int16
				// (colonna/riga), scritti per OGNI regola ormai -- vedi
				// il commento su ConditionalFormatRule::compareIsCellRef
				// in Container.h. Nessuna delle tre regole di questo
				// file di prova ne usa uno (tutte contro un letterale o
				// una scala di colori), ma i byte vanno comunque
				// consumati per non disallineare la regola successiva.
				if (pos + 1 + 2 + 2 <= ascdLen)
					pos += 1 + 2 + 2;

				// Formula "expression" (versione 5 del formato ASCD):
				// int32 lunghezza + testo, scritta per OGNI regola ormai
				// -- vedi il commento su
				// ConditionalFormatRule::expressionFormula in
				// Container.h. Nessuna delle tre regole di questo file
				// di prova la usa (nessuna e' di tipo "expression"), ma
				// i byte vanno comunque consumati per non disallineare
				// la regola successiva.
				if (pos + 4 <= ascdLen)
				{
					int32 exprLen;
					memcpy(&exprLen, ascdData + pos, 4); pos += 4;
					if (exprLen > 0 && pos + (size_t)exprLen <= ascdLen)
						pos += exprLen;
				}

				// Colore della barra dei dati (versione 6 del formato
				// ASCD, Tier 3 Fase B): rgb_color (4 byte), scritto per
				// OGNI regola ormai -- vedi il commento su
				// ConditionalFormatRule::dataBarColor in Container.h.
				// Non significativo per le prime tre regole di questo
				// file di prova, ma i byte vanno comunque consumati.
				int dataBarColorPacked = -1;
				if (pos + 4 <= ascdLen)
				{
					rgb_color dataBarColor;
					memcpy(&dataBarColor, ascdData + pos, 4); pos += 4;
					dataBarColorPacked = (dataBarColor.red << 16) | (dataBarColor.green << 8)
						| dataBarColor.blue;
				}

				// Nome dello stile dell'icon set (versione 7 del
				// formato ASCD, Tier 3 Fase C): int32 lunghezza +
				// testo, scritto per OGNI regola ormai -- vedi il
				// commento su ConditionalFormatRule::iconSetStyle in
				// Container.h. Vuoto per le prime quattro regole di
				// questo file di prova.
				std::string iconSetStyle;
				if (pos + 4 <= ascdLen)
				{
					int32 iconStyleLen;
					memcpy(&iconStyleLen, ascdData + pos, 4); pos += 4;
					if (iconStyleLen > 0 && pos + (size_t)iconStyleLen <= ascdLen)
					{
						iconSetStyle.assign((const char*)ascdData + pos, iconStyleLen);
						pos += iconStyleLen;
					}
				}

				// ruleOperator/compareValue2/top10*/belowAverage/
				// equalAverage (versione 8 del formato ASCD, Path to
				// full Excel parity Tier 3): scritti per OGNI regola
				// ormai -- vedi il commento su kASCDVersion. Non
				// significativi per nessuna delle cinque regole di
				// questo file di prova (tutte precedenti a questa
				// versione), ma i byte vanno comunque consumati per non
				// disallineare la regola successiva.
				if (pos + 1 <= ascdLen)
					pos += 1; // ruleOperator
				if (pos + 4 <= ascdLen)
				{
					int32 value2Len;
					memcpy(&value2Len, ascdData + pos, 4); pos += 4;
					if (value2Len > 0 && pos + (size_t)value2Len <= ascdLen)
						pos += value2Len;
				}
				if (pos + 1 + 1 + 4 <= ascdLen)
					pos += 1 + 1 + 4; // top10Bottom, top10Percent, top10Rank
				if (pos + 1 + 1 <= ascdLen)
					pos += 1 + 1; // belowAverage, equalAverage

				if (type == eCondCellIsEqual && compareValue == "Mancante" && packed == 0xFFC7CE
					&& rangeMatchesA1A3)
					foundCellIsRule = true;
				if (type == eCondDuplicateValues && packed == 0xFFEB9C && rangeMatchesB1B3)
					foundDuplicatesRule = true;
				if (type == eCondColorScale && rangeMatchesC1C10 && cfvoTypes.size() == 3
					&& cfvoTypes[0] == "min" && cfvoTypes[1] == "percentile" && cfvoTypes[2] == "max"
					&& cfvoValues[1] == 50
					&& cfvoColorsPacked[0] == 0xF8696B && cfvoColorsPacked[1] == 0xFFEB84
					&& cfvoColorsPacked[2] == 0x63BE7B)
					foundColorScaleRule = true;
				if (type == eCondDataBar && rangeMatchesD1D10 && cfvoTypes.size() == 2
					&& cfvoTypes[0] == "min" && cfvoTypes[1] == "max"
					&& dataBarColorPacked == 0x638EC6)
					foundDataBarRule = true;
				if (type == eCondIconSet && rangeMatchesE1E10 && cfvoTypes.size() == 3
					&& cfvoTypes[0] == "percent" && cfvoTypes[1] == "percent"
					&& cfvoTypes[2] == "percent"
					&& cfvoValues[0] == 0 && cfvoValues[1] == 33 && cfvoValues[2] == 67
					&& iconSetStyle == "3TrafficLights1")
					foundIconSetRule = true;
			}
			Check(foundCellIsRule,
				"la regola cellIs/equal (\"Mancante\", dxf 0 = FFC7CE) e' importata correttamente");
			Check(foundDuplicatesRule,
				"la regola duplicateValues (dxf 1 = FFEB9C) e' importata correttamente");
			Check(foundColorScaleRule,
				"la regola colorScale (min/percentile 50/max, C1:C10) e' importata correttamente "
				"con tutti e tre i punti di controllo e i loro colori veri");
			Check(foundDataBarRule,
				"la regola dataBar (min/max, D1:D10, colore 638EC6) e' importata correttamente");
			Check(foundIconSetRule,
				"la regola iconSet (percent 0/33/67, E1:E10, stile 3TrafficLights1) e' importata "
				"correttamente");

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
		float impFrame[4] = { 0, 0, 0, 0 };
		bool importChartRead = importUnwrapped && ReadFirstChartForTest(importAscdData, importAscdLen,
			&impLeft, &impTop, &impRight, &impBottom, &impType, &impTitle, impFrame);
		Check(importChartRead, "il grafico a barre in stile Excel (xdr:twoCellAnchor) arriva fino all'ASCD");
		Check(importChartRead && impLeft == 1 && impTop == 1 && impRight == 2 && impBottom == 3,
			"il grafico a barre importato punta ad A1:B3, ricostruito dai riferimenti veri di chart1.xml");
		Check(importChartRead && impType == 0,
			"il grafico importato e' di tipo \"barre\" (0), <c:barChart>/<c:barDir val=\"col\"/> di chart1.xml");
		// Il vero <xdr:twoCellAnchor> di chart1.xml e' from col=2/row=0,
		// to col=7/row=15 (nessuna colonna/riga larga/alta esplicita in
		// questo foglio, quindi kDefColWidth/kDefRowHeight, 80/20px,
		// ovunque): il frame in pixel DEVE includere lo scarto
		// dell'intestazione (SheetView::kHeaderWidth/kHeaderHeight,
		// 30/20px -- lo stesso sistema di coordinate di CellRect/
		// CellOrigin, usato dalla creazione manuale di un grafico via
		// MainWindow::HandleChartInsert), non le sole colonne/righe
		// sommate da sole -- bug reale: un grafico importato da un vero
		// xdr:twoCellAnchor finiva SEMPRE disegnato uno scarto
		// d'intestazione piu' in alto/a sinistra della cella di
		// ancoraggio vera, invisibile finche' non si guarda un file con
		// grafici davvero renderizzati.
		Check(importChartRead && (int)impFrame[0] == 190 && (int)impFrame[1] == 20
				&& (int)impFrame[2] == 590 && (int)impFrame[3] == 320,
			"il frame del grafico a barre importato (190,20,590,320) include lo scarto "
			"dell'intestazione (30,20px), non solo colonna 2/riga 0 * 80/20px (160,0)");

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

	// Stesso grafico a barre di sopra, ma con gli spazi dei nomi di
	// disegno/grafico dichiarati PREDEFINITI (xmlns="...", niente
	// prefisso "xdr:"/"c:") invece che con il prefisso convenzionale
	// che Excel/LibreOffice scrivono sempre -- esattamente come li
	// scrive openpyxl (libreria Python molto diffusa per generare file
	// XLSX via script, non un caso raro), scoperto costruendo un file
	// dimostrativo con quella libreria. "a:" resta comunque esplicito
	// (openpyxl non lo lascia mai predefinito), stesso principio del
	// commento su QualifyElementName in XlsxTranslator.cpp. Prima del
	// fix, questo grafico non sarebbe arrivato affatto in ASCD: ne'
	// l'ancoraggio (<oneCellAnchor> non riconosciuto, mai letto come
	// <xdr:oneCellAnchor>) ne' il tipo (<barChart> non riconosciuto).
	{
		static const char kNsContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kNsRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kNsWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kNsWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kNsSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheetData>"
			"<row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Gen</t></is></c><c r=\"B1\"><v>10</v></c></row>"
			"<row r=\"2\"><c r=\"A2\" t=\"inlineStr\"><is><t>Feb</t></is></c><c r=\"B2\"><v>20</v></c></row>"
			"<row r=\"3\"><c r=\"A3\" t=\"inlineStr\"><is><t>Mar</t></is></c><c r=\"B3\"><v>30</v></c></row>"
			"</sheetData>"
			"<drawing r:id=\"rId1\"/>"
			"</worksheet>\n";
		static const char kNsSheetRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing1.xml\"/>\n"
			"</Relationships>\n";
		// Nessun prefisso "xdr:": xmlns predefinito sull'elemento radice,
		// "a:"/"c:" invece restano espliciti (esattamente come li scrive
		// openpyxl).
		static const char kNsDrawing[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<wsDr xmlns=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" "
			"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
			"xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
			"<oneCellAnchor>"
			"<from><col>3</col><colOff>0</colOff><row>0</row><rowOff>0</rowOff></from>"
			"<ext cx=\"3000000\" cy=\"2000000\"/>"
			"<graphicFrame>"
			"<nvGraphicFramePr><cNvPr id=\"1\" name=\"Chart 1\"/><cNvGraphicFramePr/></nvGraphicFramePr>"
			"<xfrm/>"
			"<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\">"
			"<c:chart r:id=\"rId1\"/></a:graphicData></a:graphic>"
			"</graphicFrame>"
			"<clientData/>"
			"</oneCellAnchor>"
			"</wsDr>\n";
		static const char kNsDrawingRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart1.xml\"/>\n"
			"</Relationships>\n";
		// Nessun prefisso "c:" qui: xmlns predefinito sull'elemento
		// radice, "a:" resta esplicito (nessun titolo in questa prova
		// minima, quindi non serve nemmeno usarlo).
		static const char kNsChart[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<chartSpace xmlns=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
			"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
			"<chart><plotArea><barChart><barDir val=\"col\"/>"
			"<ser><idx val=\"0\"/><order val=\"0\"/>"
			"<cat><strRef><f>Foglio1!$A$1:$A$3</f></strRef></cat>"
			"<val><numRef><f>Foglio1!$B$1:$B$3</f></numRef></val>"
			"</ser></barChart></plotArea></chart>"
			"</chartSpace>\n";

		BMallocIO nsXlsx;
		CZipWriter nsZip;
		nsZip.Begin(&nsXlsx);
		nsZip.AddEntry("[Content_Types].xml", kNsContentTypes, strlen(kNsContentTypes));
		nsZip.AddEntry("_rels/.rels", kNsRootRels, strlen(kNsRootRels));
		nsZip.AddEntry("xl/workbook.xml", kNsWorkbook, strlen(kNsWorkbook));
		nsZip.AddEntry("xl/_rels/workbook.xml.rels", kNsWorkbookRels, strlen(kNsWorkbookRels));
		nsZip.AddEntry("xl/worksheets/sheet1.xml", kNsSheet, strlen(kNsSheet));
		nsZip.AddEntry("xl/worksheets/_rels/sheet1.xml.rels", kNsSheetRels, strlen(kNsSheetRels));
		nsZip.AddEntry("xl/drawings/drawing1.xml", kNsDrawing, strlen(kNsDrawing));
		nsZip.AddEntry("xl/drawings/_rels/drawing1.xml.rels", kNsDrawingRels, strlen(kNsDrawingRels));
		nsZip.AddEntry("xl/charts/chart1.xml", kNsChart, strlen(kNsChart));
		Check(nsZip.Close(),
			"costruzione del file XLSX di prova con grafico a spazio dei nomi predefinito riuscita");

		nsXlsx.Seek(0, SEEK_SET);
		translator_info nsInfo;
		err = translator->Identify(&nsXlsx, NULL, NULL, &nsInfo, 0);
		Check(err == B_OK && nsInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con grafico a spazio dei nomi predefinito");

		nsXlsx.Seek(0, SEEK_SET);
		BMallocIO nsOut;
		err = translator->Translate(&nsXlsx, &nsInfo, NULL, kAtomoNativeFormat, &nsOut);
		Check(err == B_OK, "Translate del file di prova con grafico a spazio dei nomi predefinito riesce");

		if (err == B_OK)
		{
			const unsigned char* nsAscdData = NULL;
			size_t nsAscdLen = 0;
			bool nsUnwrapped = UnwrapFirstSheet((const unsigned char*)nsOut.Buffer(),
				nsOut.BufferLength(), &nsAscdData, &nsAscdLen);
			Check(nsUnwrapped, "l'output di Translate del file con grafico a spazio dei nomi predefinito e' un ASCD valido");

			int16 nsLeft = 0, nsTop = 0, nsRight = 0, nsBottom = 0;
			int8 nsType = -1;
			std::string nsTitle;
			bool nsChartRead = nsUnwrapped && ReadFirstChartForTest(nsAscdData, nsAscdLen,
				&nsLeft, &nsTop, &nsRight, &nsBottom, &nsType, &nsTitle);
			Check(nsChartRead,
				"il grafico con ancoraggio/tipo a spazio dei nomi predefinito (niente \"xdr:\"/\"c:\") "
				"arriva comunque fino all'ASCD");
			Check(nsChartRead && nsLeft == 1 && nsTop == 1 && nsRight == 2 && nsBottom == 3,
				"punta ad A1:B3, ricostruito dai riferimenti veri di chart1.xml");
			Check(nsChartRead && nsType == 0,
				"il tipo e' riconosciuto come \"barre\" (0) anche con <barChart>/<barDir val=\"col\"/> "
				"senza il prefisso \"c:\"");
		}
	}

	// Barre orizzontali con colonne valore NON adiacenti (Task 1 + Task
	// 2): la forma ESATTA trovata in un file utente reale
	// (money-manager-2.xlsx) -- categoria A, due serie B e D con una
	// colonna C SEMPRE VUOTA in mezzo (nessuna cella scritta), grafico
	// dichiarato <c:barDir val="bar"/> (il vero "Bar" orizzontale di
	// Excel, non "col"). Prima di questo lavoro il barDir da solo
	// sarebbe stato rifiutato come "Barre orizzontali"; con SOLO quella
	// correzione (senza Task 2) sarebbe stato rifiutato lo stesso con
	// "layout dati non compatibile" per l'adiacenza delle colonne
	// valore. Questo test verifica che, con entrambi i fix, il grafico
	// arrivi fino all'ASCD con il tipo giusto (6) e le colonne valore
	// esplicite giuste (B=2, D=4), non contigue.
	{
		static const char kHBarContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kHBarRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kHBarWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kHBarWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		// Colonna C (indice 3) deliberatamente ASSENTE da ogni riga --
		// lo spacer vuoto reale trovato in money-manager-2.xlsx.
		static const char kHBarSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheetData>"
			"<row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Gen</t></is></c>"
			"<c r=\"B1\"><v>1</v></c><c r=\"D1\"><v>100</v></c></row>"
			"<row r=\"2\"><c r=\"A2\" t=\"inlineStr\"><is><t>Feb</t></is></c>"
			"<c r=\"B2\"><v>2</v></c><c r=\"D2\"><v>200</v></c></row>"
			"<row r=\"3\"><c r=\"A3\" t=\"inlineStr\"><is><t>Mar</t></is></c>"
			"<c r=\"B3\"><v>3</v></c><c r=\"D3\"><v>300</v></c></row>"
			"</sheetData>"
			"<drawing r:id=\"rId1\"/>"
			"</worksheet>\n";
		static const char kHBarSheetRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing1.xml\"/>\n"
			"</Relationships>\n";
		static const char kHBarDrawing[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" "
			"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
			"xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
			"<xdr:oneCellAnchor>"
			"<xdr:from><xdr:col>5</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>0</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from>"
			"<xdr:ext cx=\"3000000\" cy=\"2000000\"/>"
			"<xdr:graphicFrame>"
			"<xdr:nvGraphicFramePr><xdr:cNvPr id=\"1\" name=\"Chart 1\"/><xdr:cNvGraphicFramePr/></xdr:nvGraphicFramePr>"
			"<xdr:xfrm/>"
			"<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\">"
			"<c:chart r:id=\"rId1\"/></a:graphicData></a:graphic>"
			"</xdr:graphicFrame>"
			"<xdr:clientData/>"
			"</xdr:oneCellAnchor>"
			"</xdr:wsDr>\n";
		static const char kHBarDrawingRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart1.xml\"/>\n"
			"</Relationships>\n";
		static const char kHBarChart[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<c:chartSpace xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
			"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
			"<c:chart><c:plotArea><c:barChart><c:barDir val=\"bar\"/><c:grouping val=\"clustered\"/>"
			"<c:ser><c:idx val=\"0\"/><c:order val=\"0\"/>"
			"<c:cat><c:strRef><c:f>Foglio1!$A$1:$A$3</c:f></c:strRef></c:cat>"
			"<c:val><c:numRef><c:f>Foglio1!$B$1:$B$3</c:f></c:numRef></c:val>"
			"</c:ser>"
			"<c:ser><c:idx val=\"1\"/><c:order val=\"1\"/>"
			"<c:cat><c:strRef><c:f>Foglio1!$A$1:$A$3</c:f></c:strRef></c:cat>"
			"<c:val><c:numRef><c:f>Foglio1!$D$1:$D$3</c:f></c:numRef></c:val>"
			"</c:ser>"
			"</c:barChart></c:plotArea></c:chart>"
			"</c:chartSpace>\n";

		BMallocIO hbarXlsx;
		CZipWriter hbarZip;
		hbarZip.Begin(&hbarXlsx);
		hbarZip.AddEntry("[Content_Types].xml", kHBarContentTypes, strlen(kHBarContentTypes));
		hbarZip.AddEntry("_rels/.rels", kHBarRootRels, strlen(kHBarRootRels));
		hbarZip.AddEntry("xl/workbook.xml", kHBarWorkbook, strlen(kHBarWorkbook));
		hbarZip.AddEntry("xl/_rels/workbook.xml.rels", kHBarWorkbookRels, strlen(kHBarWorkbookRels));
		hbarZip.AddEntry("xl/worksheets/sheet1.xml", kHBarSheet, strlen(kHBarSheet));
		hbarZip.AddEntry("xl/worksheets/_rels/sheet1.xml.rels", kHBarSheetRels, strlen(kHBarSheetRels));
		hbarZip.AddEntry("xl/drawings/drawing1.xml", kHBarDrawing, strlen(kHBarDrawing));
		hbarZip.AddEntry("xl/drawings/_rels/drawing1.xml.rels", kHBarDrawingRels, strlen(kHBarDrawingRels));
		hbarZip.AddEntry("xl/charts/chart1.xml", kHBarChart, strlen(kHBarChart));
		Check(hbarZip.Close(),
			"costruzione del file XLSX di prova con barre orizzontali/colonne non adiacenti riuscita");

		hbarXlsx.Seek(0, SEEK_SET);
		translator_info hbarInfo;
		err = translator->Identify(&hbarXlsx, NULL, NULL, &hbarInfo, 0);
		Check(err == B_OK && hbarInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con barre orizzontali");

		hbarXlsx.Seek(0, SEEK_SET);
		BMallocIO hbarOut;
		BMessage hbarExtension;
		err = translator->Translate(&hbarXlsx, &hbarInfo, &hbarExtension, kAtomoNativeFormat, &hbarOut);
		Check(err == B_OK, "Translate del file di prova con barre orizzontali riesce");

		const unsigned char* hbarAscdData = NULL;
		size_t hbarAscdLen = 0;
		bool hbarUnwrapped = UnwrapFirstSheet((const unsigned char*)hbarOut.Buffer(),
			hbarOut.BufferLength(), &hbarAscdData, &hbarAscdLen);
		Check(hbarUnwrapped, "l'output di Translate del file con barre orizzontali e' un ASCD valido");

		int16 hbarLeft = 0, hbarTop = 0, hbarRight = 0, hbarBottom = 0;
		int8 hbarType = -1;
		std::string hbarTitle;
		std::vector<int16> hbarValueColumns;
		float hbarFrame[4] = { 0, 0, 0, 0 };
		bool hbarChartRead = hbarUnwrapped && ReadFirstChartForTest(hbarAscdData, hbarAscdLen,
			&hbarLeft, &hbarTop, &hbarRight, &hbarBottom, &hbarType, &hbarTitle, hbarFrame, &hbarValueColumns);
		Check(hbarChartRead,
			"il grafico a barre orizzontali con colonne non adiacenti arriva fino all'ASCD "
			"(prima di questo lavoro sarebbe stato rifiutato, prima per il tipo poi per il layout)");
		Check(hbarChartRead && hbarType == 6,
			"il tipo importato e' 6 (eHBarChart), non 0 (barre verticali) -- <c:barDir val=\"bar\"/> riconosciuto");
		Check(hbarChartRead && hbarLeft == 1 && hbarRight == 4,
			"il rettangolo racchiude categoria+entrambe le serie comprendendo la colonna spacer C "
			"(A=1 a D=4), non solo le colonne effettivamente lette come serie");
		Check(hbarChartRead && hbarValueColumns.size() == 2
				&& hbarValueColumns[0] == 2 && hbarValueColumns[1] == 4,
			"le colonne valore esplicite sono B (2) e D (4), NON contigue (manca la 3, colonna C)");
		// <xdr:oneCellAnchor> con <xdr:ext> esplicito (non <xdr:to>, un
		// percorso di codice diverso dal test sample_chart_import.xlsx
		// sopra): from col=5/row=0, nessuna colonna/riga larga/alta
		// esplicita in questo foglio di prova, quindi lo scarto
		// dell'intestazione (30,20px) deve comunque comparire nel frame
		// finale.
		Check(hbarChartRead && (int)hbarFrame[0] == 430 && (int)hbarFrame[1] == 20,
			"il frame del grafico a barre orizzontali (430,20,...) include lo scarto "
			"dell'intestazione anche col percorso <xdr:ext> esplicito (non solo <xdr:to>)");

		type_code hbarMsgType;
		int32 hbarUnsupportedCount = 0;
		hbarExtension.GetInfo("atomo:unsupportedChart", &hbarMsgType, &hbarUnsupportedCount);
		Check(hbarUnsupportedCount == 0,
			"nessun grafico segnalato come non supportato (ne' per il tipo ne' per il layout dati)");
	}

	// Grafico con orientamento riga (categoria/serie disposte per RIGA
	// invece che per colonna): la forma ESATTA trovata in due file utente
	// reali (earned-value-management.xlsx, family-budget-planner.xlsx) --
	// categoria su UNA riga sola (qui riga 5, colonne B:D), ogni serie su
	// una riga propria NON contigua alla categoria ne' fra loro (righe 10
	// e 8, non 6/7 subito sotto -- stesso spacer "a riga" del caso
	// "colonne non adiacenti" sopra, solo trasposto). Prima di questo
	// lavoro un riferimento di categoria a singola riga multi-colonna
	// veniva rifiutato subito da ReconstructChartRange (richiedeva
	// SEMPRE una singola colonna) con "layout dati non compatibile".
	{
		static const char kRowContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kRowRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kRowWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kRowWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		// Riga 5 = categoria (1,2,3), riga 8 = serie 2 (20,40,60), riga
		// 10 = serie 1 (100,200,300) -- deliberatamente NON adiacenti ne'
		// fra loro ne' alla categoria, come nei due file reali.
		static const char kRowSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheetData>"
			"<row r=\"5\"><c r=\"B5\"><v>1</v></c><c r=\"C5\"><v>2</v></c><c r=\"D5\"><v>3</v></c></row>"
			"<row r=\"8\"><c r=\"B8\"><v>20</v></c><c r=\"C8\"><v>40</v></c><c r=\"D8\"><v>60</v></c></row>"
			"<row r=\"10\"><c r=\"B10\"><v>100</v></c><c r=\"C10\"><v>200</v></c><c r=\"D10\"><v>300</v></c></row>"
			"</sheetData>"
			"<drawing r:id=\"rId1\"/>"
			"</worksheet>\n";
		static const char kRowSheetRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" Target=\"../drawings/drawing1.xml\"/>\n"
			"</Relationships>\n";
		static const char kRowDrawing[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" "
			"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
			"xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
			"<xdr:oneCellAnchor>"
			"<xdr:from><xdr:col>5</xdr:col><xdr:colOff>0</xdr:colOff><xdr:row>0</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from>"
			"<xdr:ext cx=\"3000000\" cy=\"2000000\"/>"
			"<xdr:graphicFrame>"
			"<xdr:nvGraphicFramePr><xdr:cNvPr id=\"1\" name=\"Chart 1\"/><xdr:cNvGraphicFramePr/></xdr:nvGraphicFramePr>"
			"<xdr:xfrm/>"
			"<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\">"
			"<c:chart r:id=\"rId1\"/></a:graphicData></a:graphic>"
			"</xdr:graphicFrame>"
			"<xdr:clientData/>"
			"</xdr:oneCellAnchor>"
			"</xdr:wsDr>\n";
		static const char kRowDrawingRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" Target=\"../charts/chart1.xml\"/>\n"
			"</Relationships>\n";
		// <c:cat> come <c:numRef> (non <c:strRef>): stessa forma esatta
		// di earned-value-management.xlsx, dove la categoria e' un
		// numero progressivo (1, 2, 3...), non testo.
		static const char kRowChart[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<c:chartSpace xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
			"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
			"<c:chart><c:plotArea><c:lineChart><c:grouping val=\"standard\"/>"
			"<c:ser><c:idx val=\"0\"/><c:order val=\"0\"/>"
			"<c:cat><c:numRef><c:f>Foglio1!$B$5:$D$5</c:f></c:numRef></c:cat>"
			"<c:val><c:numRef><c:f>Foglio1!$B$10:$D$10</c:f></c:numRef></c:val>"
			"</c:ser>"
			"<c:ser><c:idx val=\"1\"/><c:order val=\"1\"/>"
			"<c:cat><c:numRef><c:f>Foglio1!$B$5:$D$5</c:f></c:numRef></c:cat>"
			"<c:val><c:numRef><c:f>Foglio1!$B$8:$D$8</c:f></c:numRef></c:val>"
			"</c:ser>"
			"</c:lineChart></c:plotArea></c:chart>"
			"</c:chartSpace>\n";

		BMallocIO rowXlsx;
		CZipWriter rowZip;
		rowZip.Begin(&rowXlsx);
		rowZip.AddEntry("[Content_Types].xml", kRowContentTypes, strlen(kRowContentTypes));
		rowZip.AddEntry("_rels/.rels", kRowRootRels, strlen(kRowRootRels));
		rowZip.AddEntry("xl/workbook.xml", kRowWorkbook, strlen(kRowWorkbook));
		rowZip.AddEntry("xl/_rels/workbook.xml.rels", kRowWorkbookRels, strlen(kRowWorkbookRels));
		rowZip.AddEntry("xl/worksheets/sheet1.xml", kRowSheet, strlen(kRowSheet));
		rowZip.AddEntry("xl/worksheets/_rels/sheet1.xml.rels", kRowSheetRels, strlen(kRowSheetRels));
		rowZip.AddEntry("xl/drawings/drawing1.xml", kRowDrawing, strlen(kRowDrawing));
		rowZip.AddEntry("xl/drawings/_rels/drawing1.xml.rels", kRowDrawingRels, strlen(kRowDrawingRels));
		rowZip.AddEntry("xl/charts/chart1.xml", kRowChart, strlen(kRowChart));
		Check(rowZip.Close(),
			"costruzione del file XLSX di prova con grafico per riga riuscita");

		rowXlsx.Seek(0, SEEK_SET);
		translator_info rowInfo;
		err = translator->Identify(&rowXlsx, NULL, NULL, &rowInfo, 0);
		Check(err == B_OK && rowInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con grafico per riga");

		rowXlsx.Seek(0, SEEK_SET);
		BMallocIO rowOut;
		BMessage rowExtension;
		err = translator->Translate(&rowXlsx, &rowInfo, &rowExtension, kAtomoNativeFormat, &rowOut);
		Check(err == B_OK, "Translate del file di prova con grafico per riga riesce");

		const unsigned char* rowAscdData = NULL;
		size_t rowAscdLen = 0;
		bool rowUnwrapped = UnwrapFirstSheet((const unsigned char*)rowOut.Buffer(),
			rowOut.BufferLength(), &rowAscdData, &rowAscdLen);
		Check(rowUnwrapped, "l'output di Translate del file con grafico per riga e' un ASCD valido");

		int16 rowLeft = 0, rowTop = 0, rowRight = 0, rowBottom = 0;
		int8 rowType = -1;
		std::string rowTitle;
		std::vector<int16> rowValueColumns;
		bool rowRowOriented = false;
		std::vector<int16> rowValueRows;
		bool rowChartRead = rowUnwrapped && ReadFirstChartForTest(rowAscdData, rowAscdLen,
			&rowLeft, &rowTop, &rowRight, &rowBottom, &rowType, &rowTitle, NULL,
			&rowValueColumns, &rowRowOriented, &rowValueRows);
		Check(rowChartRead,
			"il grafico con orientamento riga arriva fino all'ASCD (prima di questo lavoro sarebbe "
			"stato rifiutato con \"layout dati non compatibile\")");
		Check(rowChartRead && rowType == 1, "il tipo importato e' 1 (eLineChart), da <c:lineChart>");
		Check(rowChartRead && rowRowOriented, "il grafico e' marcato per orientamento riga");
		Check(rowChartRead && rowLeft == 2 && rowTop == 5 && rowRight == 4 && rowBottom == 10,
			"il rettangolo racchiude la riga di categoria (5) e la riga valore piu' bassa (10), "
			"colonne B..D (2..4)");
		Check(rowChartRead && rowValueRows.size() == 2
				&& rowValueRows[0] == 10 && rowValueRows[1] == 8,
			"le righe valore esplicite sono 10 (prima serie) e 8 (seconda), nell'ordine delle serie "
			"nel file, NON contigue ne' fra loro ne' con la riga di categoria (5)");

		type_code rowMsgType;
		int32 rowUnsupportedCount = 0;
		rowExtension.GetInfo("atomo:unsupportedChart", &rowMsgType, &rowUnsupportedCount);
		Check(rowUnsupportedCount == 0,
			"nessun grafico segnalato come non supportato (l'orientamento riga e' ora riconosciuto)");
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

	// Formula condivisa con testo NON valido (funzione sconosciuta):
	// CompileSharedFormulaAt (sopra) chiama CParser::Parse() SENZA
	// nessun try/catch attorno -- a differenza di ogni altra chiamata
	// di parsing in questo file -- e CParser::Parse lancia CParseErr
	// per una funzione sconosciuta (parser.cpp, errUnknownFunction).
	// Bug reale: due .report veri catturati aprendo un vero file XLSX,
	// std::terminate/abort() nel thread di caricamento file per
	// un'eccezione mai presa che risaliva fuori da
	// CXlsxTranslator::Translate() intero, facendo crashare l'intera
	// app. La cella ancora (B1, con l'intero <f>) non crasha MAI:
	// passa dal ramo generico protetto da catch(...) qualche riga
	// sotto in SheetEnd. Solo B2 (la cella "vuota" <f t="shared"
	// si="0"/> che dichiara CompileSharedFormulaAt) colpiva il buco.
	{
		static const char kBadShContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kBadShRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kBadShWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kBadShWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kBadShSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData>"
			"<row r=\"1\"><c r=\"A1\"><v>5</v></c>"
			"<c r=\"B1\"><f t=\"shared\" ref=\"B1:B2\" si=\"0\">NOTAREALFUNC(A1)</f><v>999</v></c></row>"
			"<row r=\"2\"><c r=\"A2\"><v>10</v></c>"
			"<c r=\"B2\"><f t=\"shared\" si=\"0\"/><v>999</v></c></row>"
			"</sheetData>"
			"</worksheet>\n";

		BMallocIO badShXlsx;
		CZipWriter badShZip;
		badShZip.Begin(&badShXlsx);
		badShZip.AddEntry("[Content_Types].xml", kBadShContentTypes, strlen(kBadShContentTypes));
		badShZip.AddEntry("_rels/.rels", kBadShRootRels, strlen(kBadShRootRels));
		badShZip.AddEntry("xl/workbook.xml", kBadShWorkbook, strlen(kBadShWorkbook));
		badShZip.AddEntry("xl/_rels/workbook.xml.rels", kBadShWorkbookRels, strlen(kBadShWorkbookRels));
		badShZip.AddEntry("xl/worksheets/sheet1.xml", kBadShSheet, strlen(kBadShSheet));
		Check(badShZip.Close(), "costruzione del file XLSX di prova per la formula condivisa non valida riuscita");

		badShXlsx.Seek(0, SEEK_SET);
		translator_info badShInfo;
		err = translator->Identify(&badShXlsx, NULL, NULL, &badShInfo, 0);
		Check(err == B_OK && badShInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova per la formula condivisa non valida");

		badShXlsx.Seek(0, SEEK_SET);
		BMallocIO badShAscdOut;
		err = translator->Translate(&badShXlsx, &badShInfo, NULL, kAtomoNativeFormat, &badShAscdOut);
		Check(err == B_OK,
			"Translate NON crasha su una formula condivisa con funzione sconosciuta: l'intero "
			"documento si importa comunque, non solo la cella incriminata viene saltata");
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

	// Commenti "threaded" (xl/threadedComments/threadedCommentN.xml, il
	// formato "Comments" reale di Excel dal 2019 in poi): un file reale
	// ha SEMPRE anche una voce nel <comments> legacy, ma con un testo
	// segnaposto boilerplate invece del contenuto vero -- il testo
	// reale deve vincere, non il segnaposto. Stessa struttura minima di
	// documento del blocco <comments> sopra (una sola cella A1 con
	// valore, nessun'altra formattazione), cosi' ReadFirstCommentFromAscdForTest
	// resta valido cosi' com'e'.
	{
		static const char kThreadedContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"<Override PartName=\"/xl/comments1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.comments+xml\"/>\n"
			"</Types>\n";
		static const char kThreadedRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kThreadedWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kThreadedWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kThreadedSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"</worksheet>\n";
		static const char kThreadedSheetRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments\" "
			"Target=\"../comments1.xml\"/>\n"
			"<Relationship Id=\"rId2\" Type=\"http://schemas.microsoft.com/office/2017/10/relationships/threadedComment\" "
			"Target=\"../threadedComments/threadedComment1.xml\"/>\n</Relationships>\n";
		static const char kThreadedLegacyComment[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<comments xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
			"<authors><author>Microsoft Office User</author></authors>"
			"<commentList><comment ref=\"B2\" authorId=\"0\">"
			"<text><r><t>[Threaded comment]\n\nYour version of Excel allows you to read this "
			"threaded comment; however, any edits to it will get removed if the file is opened "
			"in a newer version of Excel.\n\nComment:\n    Segnaposto, non il testo vero"
			"</t></r></text></comment></commentList></comments>\n";
		static const char kThreadedComment1[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<ThreadedComments xmlns=\"http://schemas.microsoft.com/office/spreadsheetml/2018/threadedcomments\">"
			"<threadedComment ref=\"B2\" dT=\"2026-01-01T10:00:00.00Z\" "
			"personId=\"{00000000-0001-0000-0000-000000000000}\" "
			"id=\"{11111111-1111-1111-1111-111111111111}\"><text>Testo reale</text></threadedComment>"
			"<threadedComment ref=\"B2\" dT=\"2026-01-01T10:05:00.00Z\" "
			"personId=\"{00000000-0001-0000-0000-000000000000}\" "
			"id=\"{22222222-2222-2222-2222-222222222222}\" "
			"parentId=\"{11111111-1111-1111-1111-111111111111}\"><text>Risposta</text></threadedComment>"
			"</ThreadedComments>\n";

		BMallocIO threadedXlsx;
		CZipWriter threadedZip;
		threadedZip.Begin(&threadedXlsx);
		threadedZip.AddEntry("[Content_Types].xml", kThreadedContentTypes, strlen(kThreadedContentTypes));
		threadedZip.AddEntry("_rels/.rels", kThreadedRootRels, strlen(kThreadedRootRels));
		threadedZip.AddEntry("xl/workbook.xml", kThreadedWorkbook, strlen(kThreadedWorkbook));
		threadedZip.AddEntry("xl/_rels/workbook.xml.rels", kThreadedWorkbookRels, strlen(kThreadedWorkbookRels));
		threadedZip.AddEntry("xl/worksheets/sheet1.xml", kThreadedSheet, strlen(kThreadedSheet));
		threadedZip.AddEntry("xl/worksheets/_rels/sheet1.xml.rels", kThreadedSheetRels, strlen(kThreadedSheetRels));
		threadedZip.AddEntry("xl/comments1.xml", kThreadedLegacyComment, strlen(kThreadedLegacyComment));
		threadedZip.AddEntry("xl/threadedComments/threadedComment1.xml", kThreadedComment1, strlen(kThreadedComment1));
		Check(threadedZip.Close(), "costruzione del file XLSX di prova con un commento threaded riuscita");

		threadedXlsx.Seek(0, SEEK_SET);
		translator_info threadedInfo;
		err = translator->Identify(&threadedXlsx, NULL, NULL, &threadedInfo, 0);
		Check(err == B_OK && threadedInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con un commento threaded");

		threadedXlsx.Seek(0, SEEK_SET);
		BMallocIO threadedAscdOut;
		err = translator->Translate(&threadedXlsx, &threadedInfo, NULL, kAtomoNativeFormat, &threadedAscdOut);
		Check(err == B_OK, "Translate del file di prova con un commento threaded riesce");

		const unsigned char* threadedAscdData = NULL;
		size_t threadedAscdLen = 0;
		bool threadedUnwrapped = UnwrapFirstSheet((const unsigned char*)threadedAscdOut.Buffer(),
			threadedAscdOut.BufferLength(), &threadedAscdData, &threadedAscdLen);
		Check(threadedUnwrapped, "l'output di Translate del file di prova con un commento threaded e' un ASCD valido");

		if (threadedUnwrapped)
		{
			cell importedThreadedCell;
			std::string importedThreadedText;
			bool threadedRead = ReadFirstCommentFromAscdForTest(threadedAscdData, threadedAscdLen,
				&importedThreadedCell, &importedThreadedText);
			Check(threadedRead, "la sezione commenti dell'ASCD prodotto (caso threaded) si legge correttamente");
			Check(threadedRead && importedThreadedCell == cell(2, 2),
				"il commento threaded importato e' ancorato a B2, come <threadedComment ref=\"B2\">");
			Check(threadedRead && importedThreadedText == "Testo reale\n\nRisposta",
				"il testo importato e' quello vero del thread (root + risposta), "
				"NON il segnaposto boilerplate del <comments> legacy che Excel scrive accanto");
		}
	}

	// Target di relationship "package-relative" (radice "/", es.
	// "/xl/worksheets/sheet2.xml") invece di "part-relative" (es.
	// "worksheets/sheet2.xml" in xl/_rels/workbook.xml.rels, o
	// "../tables/table1.xml" nei _rels di un foglio): entrambe le
	// forme sono legali per lo standard OPC (ECMA-376 parte 2), ma
	// openpyxl (libreria Python molto diffusa per generare file XLSX
	// via script) scrive SEMPRE la forma assoluta per fogli/tabelle/
	// commenti/disegni/grafici. Prima del fix in RelationshipsStart,
	// nessuna voce di workbook.xml.rels risolveva mai il proprio r:id
	// (la mappa costruita altrove confronta "xl/" + target, mai un
	// percorso assoluto), quindi ogni file XLSX multi-foglio scritto
	// da uno script del genere collassava silenziosamente su un solo
	// foglio -- il ripiego "nessun foglio risolto, prova
	// xl/worksheets/sheet1.xml" (vedi Translate) trovava comunque
	// quel primo file per coincidenza di nome, ma con un nome
	// generico "Foglio1" al posto di quello VERO dichiarato in
	// workbook.xml, perdendo ogni foglio successivo. Qui due fogli
	// veri ("Uno"/"Due"), entrambi risolti tramite target assoluti.
	{
		static const char kAbsContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet2.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kAbsRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kAbsWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Uno\" sheetId=\"1\" r:id=\"rId1\"/>"
			"<sheet name=\"Due\" sheetId=\"2\" r:id=\"rId2\"/></sheets>\n"
			"</workbook>\n";
		// Target assoluti ("/xl/...") per ENTRAMBI i fogli, esattamente
		// come li scrive openpyxl -- la forma che RelationshipsStart
		// deve normalizzare.
		static const char kAbsWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"/xl/worksheets/sheet1.xml\"/>\n"
			"<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"/xl/worksheets/sheet2.xml\"/>\n"
			"</Relationships>\n";
		static const char kAbsSheet1[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Uno</t></is></c></row></sheetData>"
			"</worksheet>\n";
		static const char kAbsSheet2[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\" t=\"inlineStr\"><is><t>Due</t></is></c></row></sheetData>"
			"</worksheet>\n";

		BMallocIO absXlsx;
		CZipWriter absZip;
		absZip.Begin(&absXlsx);
		absZip.AddEntry("[Content_Types].xml", kAbsContentTypes, strlen(kAbsContentTypes));
		absZip.AddEntry("_rels/.rels", kAbsRootRels, strlen(kAbsRootRels));
		absZip.AddEntry("xl/workbook.xml", kAbsWorkbook, strlen(kAbsWorkbook));
		absZip.AddEntry("xl/_rels/workbook.xml.rels", kAbsWorkbookRels, strlen(kAbsWorkbookRels));
		absZip.AddEntry("xl/worksheets/sheet1.xml", kAbsSheet1, strlen(kAbsSheet1));
		absZip.AddEntry("xl/worksheets/sheet2.xml", kAbsSheet2, strlen(kAbsSheet2));
		Check(absZip.Close(), "costruzione del file XLSX di prova con target di relationship assoluti riuscita");

		absXlsx.Seek(0, SEEK_SET);
		translator_info absInfo;
		err = translator->Identify(&absXlsx, NULL, NULL, &absInfo, 0);
		Check(err == B_OK && absInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con target assoluti");

		absXlsx.Seek(0, SEEK_SET);
		BMallocIO absAscdOut;
		err = translator->Translate(&absXlsx, &absInfo, NULL, kAtomoNativeFormat, &absAscdOut);
		Check(err == B_OK, "Translate del file di prova con target assoluti riesce");

		if (err == B_OK)
		{
			const unsigned char* absData = (const unsigned char*)absAscdOut.Buffer();
			size_t absLen = absAscdOut.BufferLength();

			bool isBook = absLen >= 8
				&& (memcmp(absData, "ASC2", 4) == 0 || memcmp(absData, "ASCB", 4) == 0);
			Check(isBook,
				"l'output e' una cartella di lavoro multi-foglio, non un singolo ASCD nudo");

			int32 sheetCount = 0;
			if (isBook)
				memcpy(&sheetCount, absData + 4, 4);
			Check(isBook && sheetCount == 2,
				"entrambi i fogli (\"Uno\" e \"Due\") sono presenti, nessuno perso per il "
				"target assoluto nei _rels della cartella di lavoro");

			std::string firstSheetName;
			if (isBook && absLen >= 12)
			{
				int32 nameLen;
				memcpy(&nameLen, absData + 8, 4);
				if (nameLen > 0 && (size_t)(12 + nameLen) <= absLen)
					firstSheetName.assign((const char*)(absData + 12), nameLen);
			}
			Check(firstSheetName == "Uno",
				"il primo foglio si chiama davvero \"Uno\" (il nome vero da workbook.xml), "
				"non il \"Foglio1\" generico del ripiego a un solo foglio");

			const unsigned char* firstAscd = NULL;
			size_t firstLen = 0;
			bool firstUnwrapped = UnwrapFirstSheet(absData, absLen, &firstAscd, &firstLen);
			Check(firstUnwrapped, "il primo foglio si sblocca correttamente");
			if (firstUnwrapped)
			{
				int32 count = 0;
				if (firstLen > 12)
					memcpy(&count, firstAscd + 8, 4);
				Check(count == 1, "il primo foglio (\"Uno\") ha davvero la sua unica cella");
			}
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
			"<fonts count=\"1\"><font><name val=\"Calibri\"/></font></fonts>\n"
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

	// Legacy indexed color palette (indexed="N" on a fill/font/border
	// color): the fixed Excel 97-2003 palette, resolved as a fallback
	// when neither rgb= nor theme= is present. index 2 in that palette
	// is pure red ("FF0000") -- verified end to end (real XLSX -> ASCD
	// import), not just the palette table in isolation.
	{
		static const char kIndexedContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n"
			"</Types>\n";
		static const char kIndexedRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kIndexedWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kIndexedWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kIndexedSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\" s=\"1\"><v>9</v></c></row></sheetData>\n"
			"</worksheet>\n";
		static const char kIndexedStyles[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<fonts count=\"1\"><font><name val=\"Calibri\"/></font></fonts>\n"
			"<fills count=\"3\"><fill><patternFill patternType=\"none\"/></fill>"
			"<fill><patternFill patternType=\"gray125\"/></fill>"
			"<fill><patternFill patternType=\"solid\"><fgColor indexed=\"2\"/><bgColor indexed=\"64\"/></patternFill></fill></fills>\n"
			"<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>\n"
			"<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>\n"
			"<cellXfs count=\"2\">"
			"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
			"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"2\" borderId=\"0\" xfId=\"0\" applyFill=\"1\"/>"
			"</cellXfs>\n"
			"</styleSheet>\n";

		BMallocIO indexedXlsx;
		CZipWriter indexedZip;
		indexedZip.Begin(&indexedXlsx);
		indexedZip.AddEntry("[Content_Types].xml", kIndexedContentTypes, strlen(kIndexedContentTypes));
		indexedZip.AddEntry("_rels/.rels", kIndexedRootRels, strlen(kIndexedRootRels));
		indexedZip.AddEntry("xl/workbook.xml", kIndexedWorkbook, strlen(kIndexedWorkbook));
		indexedZip.AddEntry("xl/_rels/workbook.xml.rels", kIndexedWorkbookRels, strlen(kIndexedWorkbookRels));
		indexedZip.AddEntry("xl/worksheets/sheet1.xml", kIndexedSheet, strlen(kIndexedSheet));
		indexedZip.AddEntry("xl/styles.xml", kIndexedStyles, strlen(kIndexedStyles));
		Check(indexedZip.Close(), "costruzione del file XLSX di prova con fgColor indexed=\"2\" riuscita");

		indexedXlsx.Seek(0, SEEK_SET);
		translator_info indexedInfo;
		err = translator->Identify(&indexedXlsx, NULL, NULL, &indexedInfo, 0);
		Check(err == B_OK && indexedInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con colore indicizzato");

		indexedXlsx.Seek(0, SEEK_SET);
		BMallocIO indexedAscdOut;
		err = translator->Translate(&indexedXlsx, &indexedInfo, NULL, kAtomoNativeFormat, &indexedAscdOut);
		Check(err == B_OK, "Translate del file di prova con colore indicizzato riesce");

		const unsigned char* indexedAscdData = NULL;
		size_t indexedAscdLen = 0;
		bool indexedUnwrapped = UnwrapFirstSheet((const unsigned char*)indexedAscdOut.Buffer(),
			indexedAscdOut.BufferLength(), &indexedAscdData, &indexedAscdLen);
		Check(indexedUnwrapped, "l'output di Translate con colore indicizzato e' un ASCD valido");

		if (indexedUnwrapped)
		{
			cell importedCell;
			rgb_color importedBg = { 0, 0, 0, 0 };
			bool colorRead = ReadFirstCellColorFromAscdForTest(indexedAscdData, indexedAscdLen,
				&importedCell, &importedBg);
			Check(colorRead && importedCell == cell(1, 1),
				"il colore indicizzato importato e' ancorato ad A1");
			Check(colorRead && importedBg.red == 255 && importedBg.green == 0 && importedBg.blue == 0,
				"indexed=\"2\" risolve al rosso puro (FF0000) della tavolozza Excel 97-2003, "
				"non piu' al colore predefinito del motore");
		}
	}

	// Page margins/scale (<pageMargins>/<pageSetup>), the sixth item of
	// Tier 2 in the "100% XLSX standard compatibility" plan (import
	// only -- export is a separate follow-up, see ROADMAP.md): a fixed
	// percentage scale, the common case (no <pageSetUpPr fitToPage>).
	// Margins are always in inches in XLSX, converted to cm here
	// (AscdPrintSettings' own unit) -- 0.5in and 1in were picked
	// specifically because they don't round-trip to a suspiciously
	// "already the default" 2cm, so this test can tell a real
	// conversion from a value that was never actually read.
	{
		static const char kPrintContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kPrintRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kPrintWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kPrintWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kPrintSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"<pageMargins left=\"0.5\" right=\"0.5\" top=\"1\" bottom=\"1\" header=\"0.3\" footer=\"0.3\"/>"
			"<pageSetup scale=\"150\"/>"
			"</worksheet>\n";

		BMallocIO printXlsx;
		CZipWriter printZip;
		printZip.Begin(&printXlsx);
		printZip.AddEntry("[Content_Types].xml", kPrintContentTypes, strlen(kPrintContentTypes));
		printZip.AddEntry("_rels/.rels", kPrintRootRels, strlen(kPrintRootRels));
		printZip.AddEntry("xl/workbook.xml", kPrintWorkbook, strlen(kPrintWorkbook));
		printZip.AddEntry("xl/_rels/workbook.xml.rels", kPrintWorkbookRels, strlen(kPrintWorkbookRels));
		printZip.AddEntry("xl/worksheets/sheet1.xml", kPrintSheet, strlen(kPrintSheet));
		Check(printZip.Close(), "costruzione del file XLSX di prova con <pageMargins>/<pageSetup> riuscita");

		printXlsx.Seek(0, SEEK_SET);
		translator_info printInfo;
		err = translator->Identify(&printXlsx, NULL, NULL, &printInfo, 0);
		Check(err == B_OK && printInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con margini/scala");

		printXlsx.Seek(0, SEEK_SET);
		BMallocIO printAscdOut;
		err = translator->Translate(&printXlsx, &printInfo, NULL, kAtomoNativeFormat, &printAscdOut);
		Check(err == B_OK, "Translate del file di prova con margini/scala riesce");

		const unsigned char* printAscdData = NULL;
		size_t printAscdLen = 0;
		bool printUnwrapped = UnwrapFirstSheet((const unsigned char*)printAscdOut.Buffer(),
			printAscdOut.BufferLength(), &printAscdData, &printAscdLen);
		Check(printUnwrapped, "l'output di Translate con margini/scala e' un ASCD valido");

		if (printUnwrapped)
		{
			bool hasSettings = false;
			double marginTop = -1, marginBottom = -1, marginLeft = -1, marginRight = -1;
			int32 scaleMode = -1;
			double scalePercent = -1;
			bool printRead = ReadPrintSettingsFromAscdForTest(printAscdData, printAscdLen,
				&hasSettings, &marginTop, &marginBottom, &marginLeft, &marginRight,
				&scaleMode, &scalePercent);
			Check(printRead, "la sezione margini/scala dell'ASCD prodotto si legge correttamente");
			Check(printRead && hasSettings, "le impostazioni di stampa importate risultano presenti");
			Check(printRead && fabs(marginTop - 2.54) < 0.001 && fabs(marginBottom - 2.54) < 0.001,
				"i margini superiore/inferiore importati sono 2.54cm (1 pollice reale), non piu' sempre 2cm");
			Check(printRead && fabs(marginLeft - 1.27) < 0.001 && fabs(marginRight - 1.27) < 0.001,
				"i margini sinistro/destro importati sono 1.27cm (0.5 pollici reali), non piu' sempre 2cm");
			Check(printRead && scaleMode == 0,
				"la modalita' di scala importata e' \"percentuale fissa\" (0), <pageSetUpPr fitToPage> assente");
			Check(printRead && fabs(scalePercent - 150.0) < 0.001,
				"la percentuale di scala importata e' quella vera (150), non piu' sempre 100");
		}
	}

	// Stesso scenario ma con <sheetPr><pageSetUpPr fitToPage="1"/></sheetPr>
	// e fitToHeight="0": Excel usa questo per dire "adatta alla
	// larghezza, nessun limite di pagine in altezza" -- deve mappare su
	// kPrintFitWidth (1), non sulla percentuale letta da scale (che
	// Excel stesso ignora quando fitToPage e' attivo).
	{
		static const char kFitSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetPr><pageSetUpPr fitToPage=\"1\"/></sheetPr>"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"<pageMargins left=\"0.5\" right=\"0.5\" top=\"1\" bottom=\"1\"/>"
			"<pageSetup scale=\"150\" fitToWidth=\"1\" fitToHeight=\"0\"/>"
			"</worksheet>\n";
		static const char kFitContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kFitRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kFitWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"</workbook>\n";
		static const char kFitWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";

		BMallocIO fitXlsx;
		CZipWriter fitZip;
		fitZip.Begin(&fitXlsx);
		fitZip.AddEntry("[Content_Types].xml", kFitContentTypes, strlen(kFitContentTypes));
		fitZip.AddEntry("_rels/.rels", kFitRootRels, strlen(kFitRootRels));
		fitZip.AddEntry("xl/workbook.xml", kFitWorkbook, strlen(kFitWorkbook));
		fitZip.AddEntry("xl/_rels/workbook.xml.rels", kFitWorkbookRels, strlen(kFitWorkbookRels));
		fitZip.AddEntry("xl/worksheets/sheet1.xml", kFitSheet, strlen(kFitSheet));
		Check(fitZip.Close(), "costruzione del file XLSX di prova con fitToPage riuscita");

		fitXlsx.Seek(0, SEEK_SET);
		translator_info fitInfo;
		err = translator->Identify(&fitXlsx, NULL, NULL, &fitInfo, 0);
		Check(err == B_OK && fitInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con fitToPage");

		fitXlsx.Seek(0, SEEK_SET);
		BMallocIO fitAscdOut;
		err = translator->Translate(&fitXlsx, &fitInfo, NULL, kAtomoNativeFormat, &fitAscdOut);
		Check(err == B_OK, "Translate del file di prova con fitToPage riesce");

		const unsigned char* fitAscdData = NULL;
		size_t fitAscdLen = 0;
		bool fitUnwrapped = UnwrapFirstSheet((const unsigned char*)fitAscdOut.Buffer(),
			fitAscdOut.BufferLength(), &fitAscdData, &fitAscdLen);
		Check(fitUnwrapped, "l'output di Translate con fitToPage e' un ASCD valido");

		if (fitUnwrapped)
		{
			bool hasSettings = false;
			double marginTop = -1, marginBottom = -1, marginLeft = -1, marginRight = -1;
			int32 scaleMode = -1;
			double scalePercent = -1;
			bool fitRead = ReadPrintSettingsFromAscdForTest(fitAscdData, fitAscdLen,
				&hasSettings, &marginTop, &marginBottom, &marginLeft, &marginRight,
				&scaleMode, &scalePercent);
			Check(fitRead, "la sezione margini/scala (fitToPage) dell'ASCD prodotto si legge correttamente");
			Check(fitRead && scaleMode == 1,
				"con fitToPage attivo e fitToHeight=\"0\", la modalita' importata e' "
				"\"adatta alla larghezza\" (kPrintFitWidth=1), non la percentuale ignorata da Excel stesso");
		}
	}

	// Stesso scenario ma sull'export (ASCD -> XLSX), il passo 2 di 4
	// per le impostazioni di stampa: un documento con margini/scala
	// espliciti (percentuale fissa) esporta un vero <pageMargins>/
	// <pageSetup scale="..."> nel foglio, e quel file si rilegge
	// correttamente. Margini scelti (3cm/1.5cm) per non coincidere con
	// il predefinito 2cm.
	{
		CContainer& printExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &printExportDoc, true); // A1

		BMallocIO printAscdIn;
		status_t printSaveErr = WriteASCDWithPrintSettingsForTest(&printExportDoc,
			3.0, 3.0, 1.5, 1.5, 0, 150.0, &printAscdIn);
		Check(printSaveErr == B_OK, "preparazione dell'ASCD di prova con margini/scala riesce");
		printExportDoc.Release();

		printAscdIn.Seek(0, SEEK_SET);
		translator_info printExportInfo;
		err = translator->Identify(&printAscdIn, NULL, NULL, &printExportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con margini/scala come sorgente per l'export");

		printAscdIn.Seek(0, SEEK_SET);
		BMallocIO printXlsxOut;
		err = translator->Translate(&printAscdIn, &printExportInfo, NULL, kAtomoXlsxFormat, &printXlsxOut);
		Check(err == B_OK, "Translate ASCD (con margini/scala) -> XLSX riesce");

		if (err == B_OK)
		{
			printXlsxOut.Seek(0, SEEK_SET);
			CZipReader printOutZip;
			Check(printOutZip.Open(&printXlsxOut),
				"il file XLSX esportato con margini/scala e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> exportedSheetXml;
			bool readSheet = printOutZip.ReadEntry("xl/worksheets/sheet1.xml", exportedSheetXml);
			std::string sheetText(exportedSheetXml.begin(), exportedSheetXml.end());
			// 3cm / 2.54 = 1.181... pollici, 1.5cm / 2.54 = 0.5906 pollici (arrotondato a 4 cifre significative).
			Check(readSheet && sheetText.find("<pageMargins ") != std::string::npos
				&& sheetText.find("top=\"1.181") != std::string::npos
				&& sheetText.find("left=\"0.5906") != std::string::npos,
				"xl/worksheets/sheet1.xml esportato contiene <pageMargins> con i valori "
				"veri in pollici (3cm/1.5cm riconvertiti)");
			Check(readSheet && sheetText.find("<pageSetup scale=\"150\"") != std::string::npos,
				"xl/worksheets/sheet1.xml esportato contiene <pageSetup scale=\"150\">, "
				"la percentuale fissa vera");

			// Round-trip completo: rileggendo il file appena esportato,
			// margini/scala devono arrivare di nuovo (arrotondati
			// dall'andata e ritorno pollici/cm, non piu' precisi al
			// millesimo ma comunque vicini agli originali).
			printXlsxOut.Seek(0, SEEK_SET);
			translator_info printReimportInfo;
			err = translator->Identify(&printXlsxOut, NULL, NULL, &printReimportInfo, 0);
			Check(err == B_OK && printReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX esportato con margini/scala si riconosce ancora come XLSX valido rileggendolo");

			printXlsxOut.Seek(0, SEEK_SET);
			BMallocIO printRoundTripAscd;
			err = translator->Translate(&printXlsxOut, &printReimportInfo, NULL,
				kAtomoNativeFormat, &printRoundTripAscd);
			Check(err == B_OK, "il file XLSX esportato con margini/scala si rilegge correttamente (round-trip)");

			if (err == B_OK)
			{
				const unsigned char* rtPrintData = NULL;
				size_t rtPrintLen = 0;
				bool rtPrintUnwrapped = UnwrapFirstSheet((const unsigned char*)printRoundTripAscd.Buffer(),
					printRoundTripAscd.BufferLength(), &rtPrintData, &rtPrintLen);
				Check(rtPrintUnwrapped, "il round-trip di margini/scala produce anch'esso una cartella ASCB valida");

				if (rtPrintUnwrapped)
				{
					bool rtHasSettings = false;
					double rtMarginTop = -1, rtMarginBottom = -1, rtMarginLeft = -1, rtMarginRight = -1;
					int32 rtScaleMode = -1;
					double rtScalePercent = -1;
					bool rtPrintRead = ReadPrintSettingsFromAscdForTest(rtPrintData, rtPrintLen,
						&rtHasSettings, &rtMarginTop, &rtMarginBottom, &rtMarginLeft, &rtMarginRight,
						&rtScaleMode, &rtScalePercent);
					Check(rtPrintRead && rtHasSettings
						&& fabs(rtMarginTop - 3.0) < 0.01 && fabs(rtMarginLeft - 1.5) < 0.01
						&& rtScaleMode == 0 && fabs(rtScalePercent - 150.0) < 0.5,
						"dopo il giro completo ASCD -> XLSX -> ASCD, margini/scala si ritrovano "
						"ancora vicini agli originali (3cm/1.5cm, 150%)");
				}
			}
		}
	}

	// Stesso scenario ma con modalita' "adatta alla larghezza"
	// (kPrintFitWidth=1): deve esportare <sheetPr><pageSetUpPr
	// fitToPage="1"/></sheetPr> e <pageSetup fitToWidth="1"
	// fitToHeight="0"/>, non una percentuale.
	{
		CContainer& fitExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &fitExportDoc, true); // A1

		BMallocIO fitAscdIn;
		status_t fitSaveErr = WriteASCDWithPrintSettingsForTest(&fitExportDoc,
			2.0, 2.0, 2.0, 2.0, 1 /* kPrintFitWidth */, 100.0, &fitAscdIn);
		Check(fitSaveErr == B_OK, "preparazione dell'ASCD di prova con fitToPage riesce");
		fitExportDoc.Release();

		fitAscdIn.Seek(0, SEEK_SET);
		translator_info fitExportInfo;
		err = translator->Identify(&fitAscdIn, NULL, NULL, &fitExportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con fitToPage come sorgente per l'export");

		fitAscdIn.Seek(0, SEEK_SET);
		BMallocIO fitXlsxOut;
		err = translator->Translate(&fitAscdIn, &fitExportInfo, NULL, kAtomoXlsxFormat, &fitXlsxOut);
		Check(err == B_OK, "Translate ASCD (con fitToPage) -> XLSX riesce");

		if (err == B_OK)
		{
			fitXlsxOut.Seek(0, SEEK_SET);
			CZipReader fitOutZip;
			Check(fitOutZip.Open(&fitXlsxOut),
				"il file XLSX esportato con fitToPage e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> exportedFitSheetXml;
			bool readFitSheet = fitOutZip.ReadEntry("xl/worksheets/sheet1.xml", exportedFitSheetXml);
			std::string fitSheetText(exportedFitSheetXml.begin(), exportedFitSheetXml.end());
			Check(readFitSheet && fitSheetText.find("<pageSetUpPr fitToPage=\"1\"") != std::string::npos,
				"xl/worksheets/sheet1.xml esportato contiene <pageSetUpPr fitToPage=\"1\">");
			Check(readFitSheet
				&& fitSheetText.find("<pageSetup fitToWidth=\"1\" fitToHeight=\"0\"") != std::string::npos,
				"xl/worksheets/sheet1.xml esportato contiene fitToWidth=\"1\" fitToHeight=\"0\", "
				"non una percentuale (kPrintFitWidth)");

			fitXlsxOut.Seek(0, SEEK_SET);
			translator_info fitReimportInfo;
			err = translator->Identify(&fitXlsxOut, NULL, NULL, &fitReimportInfo, 0);
			Check(err == B_OK && fitReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX esportato con fitToPage si riconosce ancora come XLSX valido rileggendolo");

			fitXlsxOut.Seek(0, SEEK_SET);
			BMallocIO fitRoundTripAscd;
			err = translator->Translate(&fitXlsxOut, &fitReimportInfo, NULL,
				kAtomoNativeFormat, &fitRoundTripAscd);
			Check(err == B_OK, "il file XLSX esportato con fitToPage si rilegge correttamente (round-trip)");

			if (err == B_OK)
			{
				const unsigned char* rtFitData = NULL;
				size_t rtFitLen = 0;
				bool rtFitUnwrapped = UnwrapFirstSheet((const unsigned char*)fitRoundTripAscd.Buffer(),
					fitRoundTripAscd.BufferLength(), &rtFitData, &rtFitLen);
				Check(rtFitUnwrapped, "il round-trip di fitToPage produce anch'esso una cartella ASCB valida");

				if (rtFitUnwrapped)
				{
					bool rtFitHasSettings = false;
					double rtFitMarginTop = -1, rtFitMarginBottom = -1, rtFitMarginLeft = -1, rtFitMarginRight = -1;
					int32 rtFitScaleMode = -1;
					double rtFitScalePercent = -1;
					bool rtFitRead = ReadPrintSettingsFromAscdForTest(rtFitData, rtFitLen,
						&rtFitHasSettings, &rtFitMarginTop, &rtFitMarginBottom,
						&rtFitMarginLeft, &rtFitMarginRight, &rtFitScaleMode, &rtFitScalePercent);
					Check(rtFitRead && rtFitHasSettings && rtFitScaleMode == 1,
						"dopo il giro completo ASCD -> XLSX -> ASCD, la modalita' \"adatta alla "
						"larghezza\" si ritrova ancora impostata");
				}
			}
		}
	}

	// Print area (_xlnm.Print_Area, a reserved defined name), the third
	// step of 4 for XLSX print settings: the raw range text was
	// already captured while parsing defined names (DefinedNameInfo::
	// refText), just discarded for every "_xlnm.*" name including this
	// one -- now applied to AscdSheet::hasPrintArea/printArea instead,
	// via the same ParseSheetRangeRef helper already used for real
	// named ranges. A multi-area value (comma-separated, rare) is cut
	// down to just the first rectangle, matching the native model's
	// single-range limit.
	{
		static const char kPrintAreaContentTypes[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
			"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
			"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
			"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
			"</Types>\n";
		static const char kPrintAreaRootRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
			"</Relationships>\n";
		static const char kPrintAreaWorkbook[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
			"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
			"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
			"<definedNames>"
			"<definedName name=\"_xlnm.Print_Area\" localSheetId=\"0\">"
			"Foglio1!$A$1:$C$10,Foglio1!$E$1:$F$2</definedName>"
			"</definedNames>\n"
			"</workbook>\n";
		static const char kPrintAreaWorkbookRels[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
			"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
			"</Relationships>\n";
		static const char kPrintAreaSheet[] =
			"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
			"<sheetData><row r=\"1\"><c r=\"A1\"><v>5</v></c></row></sheetData>"
			"</worksheet>\n";

		BMallocIO printAreaXlsx;
		CZipWriter printAreaZip;
		printAreaZip.Begin(&printAreaXlsx);
		printAreaZip.AddEntry("[Content_Types].xml", kPrintAreaContentTypes, strlen(kPrintAreaContentTypes));
		printAreaZip.AddEntry("_rels/.rels", kPrintAreaRootRels, strlen(kPrintAreaRootRels));
		printAreaZip.AddEntry("xl/workbook.xml", kPrintAreaWorkbook, strlen(kPrintAreaWorkbook));
		printAreaZip.AddEntry("xl/_rels/workbook.xml.rels", kPrintAreaWorkbookRels, strlen(kPrintAreaWorkbookRels));
		printAreaZip.AddEntry("xl/worksheets/sheet1.xml", kPrintAreaSheet, strlen(kPrintAreaSheet));
		Check(printAreaZip.Close(), "costruzione del file XLSX di prova con _xlnm.Print_Area riuscita");

		printAreaXlsx.Seek(0, SEEK_SET);
		translator_info printAreaInfo;
		err = translator->Identify(&printAreaXlsx, NULL, NULL, &printAreaInfo, 0);
		Check(err == B_OK && printAreaInfo.type == kAtomoXlsxFormat,
			"Identify riconosce il file XLSX di prova con _xlnm.Print_Area");

		printAreaXlsx.Seek(0, SEEK_SET);
		BMallocIO printAreaAscdOut;
		err = translator->Translate(&printAreaXlsx, &printAreaInfo, NULL,
			kAtomoNativeFormat, &printAreaAscdOut);
		Check(err == B_OK, "Translate del file di prova con _xlnm.Print_Area riesce");

		const unsigned char* printAreaAscdData = NULL;
		size_t printAreaAscdLen = 0;
		bool printAreaUnwrapped = UnwrapFirstSheet((const unsigned char*)printAreaAscdOut.Buffer(),
			printAreaAscdOut.BufferLength(), &printAreaAscdData, &printAreaAscdLen);
		Check(printAreaUnwrapped, "l'output di Translate con _xlnm.Print_Area e' un ASCD valido");

		if (printAreaUnwrapped)
		{
			bool hasArea = false;
			int16 top = -1, left = -1, bottom = -1, right = -1;
			bool areaRead = ReadFirstPrintAreaFromAscdForTest(printAreaAscdData, printAreaAscdLen,
				&hasArea, &top, &left, &bottom, &right);
			Check(areaRead, "la sezione area di stampa dell'ASCD prodotto si legge correttamente");
			Check(areaRead && hasArea, "l'area di stampa importata risulta presente");
			Check(areaRead && top == 1 && left == 1 && bottom == 10 && right == 3,
				"l'area di stampa importata e' A1:C10, la PRIMA delle due aree di "
				"_xlnm.Print_Area (separate da virgola), non piu' scartata in silenzio");
		}
	}

	// Stesso scenario ma sull'export (ASCD -> XLSX), l'ULTIMO dei
	// quattro passi per le impostazioni di stampa: un documento con
	// un'area di stampa esplicita esporta un vero _xlnm.Print_Area
	// (sempre con ambito di foglio, localSheetId="0", l'unico foglio
	// che questo export produce) accanto ai nomi definiti reali, e
	// quel file si rilegge correttamente.
	{
		CContainer& printAreaExportDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &printAreaExportDoc, true); // A1

		BMallocIO printAreaAscdIn;
		status_t printAreaSaveErr = WriteASCDWithPrintAreaForTest(&printAreaExportDoc,
			1, 1, 10, 3, &printAreaAscdIn); // A1:C10
		Check(printAreaSaveErr == B_OK, "preparazione dell'ASCD di prova con un'area di stampa riesce");
		printAreaExportDoc.Release();

		printAreaAscdIn.Seek(0, SEEK_SET);
		translator_info printAreaExportInfo;
		err = translator->Identify(&printAreaAscdIn, NULL, NULL, &printAreaExportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD con un'area di stampa come sorgente per l'export");

		printAreaAscdIn.Seek(0, SEEK_SET);
		BMallocIO printAreaXlsxOut;
		err = translator->Translate(&printAreaAscdIn, &printAreaExportInfo, NULL,
			kAtomoXlsxFormat, &printAreaXlsxOut);
		Check(err == B_OK, "Translate ASCD (con un'area di stampa) -> XLSX riesce");

		if (err == B_OK)
		{
			printAreaXlsxOut.Seek(0, SEEK_SET);
			CZipReader printAreaOutZip;
			Check(printAreaOutZip.Open(&printAreaXlsxOut),
				"il file XLSX esportato con un'area di stampa e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> exportedWorkbookXml;
			bool readWorkbook = printAreaOutZip.ReadEntry("xl/workbook.xml", exportedWorkbookXml);
			Check(readWorkbook, "il file XLSX esportato contiene xl/workbook.xml");

			std::string workbookText(exportedWorkbookXml.begin(), exportedWorkbookXml.end());
			Check(workbookText.find(
					"<definedName name=\"_xlnm.Print_Area\" localSheetId=\"0\">Foglio1!$A$1:$C$10</definedName>")
					!= std::string::npos,
				"xl/workbook.xml esportato contiene _xlnm.Print_Area con ambito di foglio, "
				"su Foglio1!$A$1:$C$10");

			printAreaXlsxOut.Seek(0, SEEK_SET);
			translator_info printAreaReimportInfo;
			err = translator->Identify(&printAreaXlsxOut, NULL, NULL, &printAreaReimportInfo, 0);
			Check(err == B_OK && printAreaReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX esportato con un'area di stampa si riconosce ancora come XLSX valido rileggendolo");

			printAreaXlsxOut.Seek(0, SEEK_SET);
			BMallocIO printAreaRoundTripAscd;
			err = translator->Translate(&printAreaXlsxOut, &printAreaReimportInfo, NULL,
				kAtomoNativeFormat, &printAreaRoundTripAscd);
			Check(err == B_OK,
				"il file XLSX esportato con un'area di stampa si rilegge correttamente (round-trip)");

			if (err == B_OK)
			{
				const unsigned char* rtAreaData = NULL;
				size_t rtAreaLen = 0;
				bool rtAreaUnwrapped = UnwrapFirstSheet((const unsigned char*)printAreaRoundTripAscd.Buffer(),
					printAreaRoundTripAscd.BufferLength(), &rtAreaData, &rtAreaLen);
				Check(rtAreaUnwrapped, "il round-trip dell'area di stampa produce anch'esso una cartella ASCB valida");

				if (rtAreaUnwrapped)
				{
					bool rtHasArea = false;
					int16 rtTop = -1, rtLeft = -1, rtBottom = -1, rtRight = -1;
					bool rtAreaRead = ReadFirstPrintAreaFromAscdForTest(rtAreaData, rtAreaLen,
						&rtHasArea, &rtTop, &rtLeft, &rtBottom, &rtRight);
					Check(rtAreaRead && rtHasArea
						&& rtTop == 1 && rtLeft == 1 && rtBottom == 10 && rtRight == 3,
						"dopo il giro completo ASCD -> XLSX -> ASCD, l'area di stampa si ritrova "
						"ancora A1:C10");
				}
			}
		}
	}

	// docProps/core.xml e docProps/app.xml sull'export (ASCD -> XLSX):
	// il pacchetto prodotto non ha mai avuto queste due parti opzionali
	// -- niente le richiede per aprire il file, ma la loro assenza e'
	// facilmente visibile aprendo "Proprieta'" in un vero Excel/
	// LibreOffice. Nessun campo autore/titolo esiste nel modello del
	// documento, quindi il test verifica solo cio' che l'export
	// realmente scrive: la data di creazione/modifica in formato
	// W3CDTF e il nome applicazione.
	{
		CContainer& docPropsDoc = *new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), &docPropsDoc, true); // A1

		BMallocIO docPropsAscdIn;
		status_t docPropsSaveErr = WriteASCDForTest(&docPropsDoc, &docPropsAscdIn);
		Check(docPropsSaveErr == B_OK, "preparazione dell'ASCD di prova per docProps riesce");
		docPropsDoc.Release();

		docPropsAscdIn.Seek(0, SEEK_SET);
		translator_info docPropsInfo;
		err = translator->Identify(&docPropsAscdIn, NULL, NULL, &docPropsInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD di prova per docProps come sorgente per l'export");

		docPropsAscdIn.Seek(0, SEEK_SET);
		BMallocIO docPropsXlsxOut;
		err = translator->Translate(&docPropsAscdIn, &docPropsInfo, NULL, kAtomoXlsxFormat, &docPropsXlsxOut);
		Check(err == B_OK, "Translate ASCD -> XLSX per docProps riesce");

		if (err == B_OK)
		{
			docPropsXlsxOut.Seek(0, SEEK_SET);
			CZipReader docPropsZip;
			Check(docPropsZip.Open(&docPropsXlsxOut),
				"il file XLSX esportato per docProps e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> coreXmlBytes, appXmlBytes;
			bool readCore = docPropsZip.ReadEntry("docProps/core.xml", coreXmlBytes);
			bool readApp = docPropsZip.ReadEntry("docProps/app.xml", appXmlBytes);
			Check(readCore, "il file XLSX esportato contiene docProps/core.xml");
			Check(readApp, "il file XLSX esportato contiene docProps/app.xml");

			if (readCore)
			{
				std::string coreText(coreXmlBytes.begin(), coreXmlBytes.end());
				Check(coreText.find("<dcterms:created") != std::string::npos
						&& coreText.find("<dcterms:modified") != std::string::npos,
					"docProps/core.xml contiene dcterms:created/modified in formato W3CDTF");
			}
			if (readApp)
			{
				std::string appText(appXmlBytes.begin(), appXmlBytes.end());
				Check(appText.find("<Application>Atomo123</Application>") != std::string::npos,
					"docProps/app.xml identifica Atomo123 come applicazione generatrice");
			}

			std::vector<unsigned char> contentTypesBytes;
			bool readContentTypes = docPropsZip.ReadEntry("[Content_Types].xml", contentTypesBytes);
			Check(readContentTypes, "il file XLSX esportato contiene [Content_Types].xml");
			if (readContentTypes)
			{
				std::string ctText(contentTypesBytes.begin(), contentTypesBytes.end());
				Check(ctText.find("/docProps/core.xml") != std::string::npos
						&& ctText.find("/docProps/app.xml") != std::string::npos,
					"[Content_Types].xml dichiara le Override per docProps/core.xml e docProps/app.xml");
			}
		}
	}

	// Tabelle pivot, Fase 2 (export XLSX di un PivotTableObject
	// persistito come vere parti OOXML pivotCache/pivotTable -- vedi
	// ROADMAP.md/CHANGELOG.md): stesso schema Nord/Sud gia' usato nella
	// dimostrazione dal vivo di questa sessione. Ambito v1: una sola
	// colonna di categoria (A) piu' una di valore (B) -- questo e'
	// esattamente il caso che deve produrre parti pivot vere.
	{
		CContainer& pivotDoc = *new CContainer(NULL, NULL);
		// Sorgente: A1:B5, 5 righe grezze (non raggruppate).
		TryToParseString("Nord", cell(1, 1), &pivotDoc, true); // A1
		TryToParseString("100", cell(2, 1), &pivotDoc, true);  // B1
		TryToParseString("Nord", cell(1, 2), &pivotDoc, true); // A2
		TryToParseString("150", cell(2, 2), &pivotDoc, true);  // B2
		TryToParseString("Sud", cell(1, 3), &pivotDoc, true);  // A3
		TryToParseString("80", cell(2, 3), &pivotDoc, true);   // B3
		TryToParseString("Sud", cell(1, 4), &pivotDoc, true);  // A4
		TryToParseString("60", cell(2, 4), &pivotDoc, true);   // B4
		TryToParseString("Nord", cell(1, 5), &pivotDoc, true); // A5
		TryToParseString("120", cell(2, 5), &pivotDoc, true);  // B5

		// Destinazione: D1:E3, stesso layout che WritePivotTable avrebbe
		// scritto (header + una riga per categoria, gia' ordinate/
		// deduplicate -- niente riga di totale generale).
		TryToParseString("Category", cell(4, 1), &pivotDoc, true); // D1
		TryToParseString("Sum", cell(5, 1), &pivotDoc, true);      // E1
		TryToParseString("Nord", cell(4, 2), &pivotDoc, true);     // D2
		TryToParseString("370", cell(5, 2), &pivotDoc, true);      // E2
		TryToParseString("Sud", cell(4, 3), &pivotDoc, true);      // D3
		TryToParseString("140", cell(5, 3), &pivotDoc, true);      // E3

		PivotTableObject pivot;
		pivot.sourceRange = range(1, 1, 2, 5);
		pivot.destAnchor = cell(4, 1);
		pivot.aggFunc = ePivotSum;
		{
			PivotRow r;
			r.categories.push_back(BString("Nord"));
			r.aggregate = 370; r.count = 3; r.minVal = 100; r.maxVal = 150;
			pivot.cachedRows.push_back(r);
		}
		{
			PivotRow r;
			r.categories.push_back(BString("Sud"));
			r.aggregate = 140; r.count = 2; r.minVal = 60; r.maxVal = 80;
			pivot.cachedRows.push_back(r);
		}

		BMallocIO pivotAscdIn;
		status_t pivotSaveErr = WriteASCDWithPivotForTest(&pivotDoc, pivot, &pivotAscdIn);
		Check(pivotSaveErr == B_OK, "preparazione dell'ASCD di prova con una tabella pivot riesce");
		pivotDoc.Release();

		pivotAscdIn.Seek(0, SEEK_SET);
		translator_info pivotInfo;
		err = translator->Identify(&pivotAscdIn, NULL, NULL, &pivotInfo, kAtomoXlsxFormat);
		Check(err == B_OK && pivotInfo.type == kAtomoNativeFormat,
			"Identify riconosce l'ASCD di prova con una tabella pivot");

		pivotAscdIn.Seek(0, SEEK_SET);
		BMallocIO pivotXlsxOut;
		err = translator->Translate(&pivotAscdIn, &pivotInfo, NULL, kAtomoXlsxFormat, &pivotXlsxOut);
		Check(err == B_OK, "Translate ASCD (con una tabella pivot) -> XLSX riesce");

		if (err == B_OK)
		{
			pivotXlsxOut.Seek(0, SEEK_SET);
			CZipReader pivotZip;
			Check(pivotZip.Open(&pivotXlsxOut),
				"il file XLSX con una tabella pivot e' un vero archivio ZIP leggibile");

			Check(pivotZip.HasEntry("xl/pivotCache/pivotCacheDefinition1.xml"),
				"il file XLSX contiene xl/pivotCache/pivotCacheDefinition1.xml");
			Check(pivotZip.HasEntry("xl/pivotCache/_rels/pivotCacheDefinition1.xml.rels"),
				"il file XLSX contiene la relazione della cache verso pivotCacheRecords1.xml");
			Check(pivotZip.HasEntry("xl/pivotCache/pivotCacheRecords1.xml"),
				"il file XLSX contiene xl/pivotCache/pivotCacheRecords1.xml");
			Check(pivotZip.HasEntry("xl/pivotTables/pivotTable1.xml"),
				"il file XLSX contiene xl/pivotTables/pivotTable1.xml");
			Check(pivotZip.HasEntry("xl/pivotTables/_rels/pivotTable1.xml.rels"),
				"il file XLSX contiene la relazione della tabella pivot verso la cache");

			std::vector<unsigned char> ctBytes;
			if (pivotZip.ReadEntry("[Content_Types].xml", ctBytes))
			{
				std::string ct((const char*)&ctBytes[0], ctBytes.size());
				Check(ct.find("pivotCacheDefinition+xml") != std::string::npos
						&& ct.find("pivotCacheRecords+xml") != std::string::npos
						&& ct.find("spreadsheetml.pivotTable+xml") != std::string::npos,
					"[Content_Types].xml dichiara le tre Override per le parti pivot");
			}
			else
				Check(false, "[Content_Types].xml si legge dall'archivio");

			std::vector<unsigned char> wbBytes;
			if (pivotZip.ReadEntry("xl/workbook.xml", wbBytes))
			{
				std::string wb((const char*)&wbBytes[0], wbBytes.size());
				Check(wb.find("<pivotCaches>") != std::string::npos
						&& wb.find("<pivotCache cacheId=\"0\"") != std::string::npos,
					"xl/workbook.xml dichiara <pivotCaches><pivotCache cacheId=\"0\" .../></pivotCaches>");
			}
			else
				Check(false, "xl/workbook.xml si legge dall'archivio");

			std::vector<unsigned char> defBytes;
			if (pivotZip.ReadEntry("xl/pivotCache/pivotCacheDefinition1.xml", defBytes))
			{
				std::string def((const char*)&defBytes[0], defBytes.size());
				Check(def.find("recordCount=\"5\"") != std::string::npos,
					"pivotCacheDefinition1.xml conta le 5 righe GREZZE della sorgente, non le 2 categorie aggregate");
				Check(def.find("<s v=\"Nord\"/>") != std::string::npos
						&& def.find("<s v=\"Sud\"/>") != std::string::npos,
					"pivotCacheDefinition1.xml elenca le categorie vere come elementi condivisi");
				Check(def.find("sharedItems count=") == std::string::npos,
					"pivotCacheDefinition1.xml non scrive il fasullo attributo count= su <sharedItems> (bocciato dalla validazione OOXML)");
				Check(def.find("Foglio1!$A$1:$B$5") != std::string::npos,
					"pivotCacheDefinition1.xml referenzia la vera sorgente (Foglio1!$A$1:$B$5)");
			}
			else
				Check(false, "xl/pivotCache/pivotCacheDefinition1.xml si legge dall'archivio");

			std::vector<unsigned char> recBytes;
			if (pivotZip.ReadEntry("xl/pivotCache/pivotCacheRecords1.xml", recBytes))
			{
				std::string rec((const char*)&recBytes[0], recBytes.size());
				int rCount = 0;
				size_t pos = 0;
				while ((pos = rec.find("<r>", pos)) != std::string::npos)
				{
					rCount++;
					pos += 3;
				}
				Check(rCount == 5,
					"pivotCacheRecords1.xml ha un <r> per ognuna delle 5 righe grezze, non per le 2 categorie aggregate");
			}
			else
				Check(false, "xl/pivotCache/pivotCacheRecords1.xml si legge dall'archivio");

			std::vector<unsigned char> ptBytes;
			if (pivotZip.ReadEntry("xl/pivotTables/pivotTable1.xml", ptBytes))
			{
				std::string pt((const char*)&ptBytes[0], ptBytes.size());
				Check(pt.find("cacheId=\"0\"") != std::string::npos,
					"pivotTable1.xml usa lo stesso cacheId=0 dichiarato in xl/workbook.xml");
				Check(pt.find("<location ref=\"D1:E3\"") != std::string::npos,
					"pivotTable1.xml posiziona la tabella esattamente sulle celle gia' scritte (D1:E3)");
				Check(pt.find("subtotal=\"sum\"") != std::string::npos,
					"pivotTable1.xml usa subtotal=\"sum\" (l'aggregazione scelta)");
				Check(pt.find("rowGrandTotals=\"0\"") != std::string::npos,
					"pivotTable1.xml non ha riga di totale generale (WritePivotTable non ne scrive una)");
				Check(pt.find(" dataField=\"1\"") == std::string::npos,
					"pivotTable1.xml non scrive il fasullo attributo dataField= su <pivotField> (bocciato dalla validazione OOXML)");
				Check(pt.find("<pivotTableStyleInfo") != std::string::npos,
					"pivotTable1.xml include <pivotTableStyleInfo> (presente in ogni file vero scritto da Excel)");
			}
			else
				Check(false, "xl/pivotTables/pivotTable1.xml si legge dall'archivio");

			std::vector<unsigned char> sheetRelsBytes;
			if (pivotZip.ReadEntry("xl/worksheets/_rels/sheet1.xml.rels", sheetRelsBytes))
			{
				std::string sr((const char*)&sheetRelsBytes[0], sheetRelsBytes.size());
				Check(sr.find("relationships/pivotTable") != std::string::npos,
					"xl/worksheets/_rels/sheet1.xml.rels collega il foglio a pivotTable1.xml");
			}
			else
				Check(false, "xl/worksheets/_rels/sheet1.xml.rels si legge dall'archivio");

			// Fase 3 delle tabelle pivot (import XLSX di un <pivotTable>
			// reale): lo stesso file appena esportato si RIIMPORTA con lo
			// stesso translator, verificando che un vero PivotTableObject
			// venga ricostruito -- non solo le celle statiche gia'
			// verificate sopra. Round-trip simmetrico con la Fase 2.
			pivotXlsxOut.Seek(0, SEEK_SET);
			translator_info pivotReimportInfo;
			err = translator->Identify(&pivotXlsxOut, NULL, NULL, &pivotReimportInfo, 0);
			Check(err == B_OK && pivotReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX con una tabella pivot si riconosce ancora come XLSX valido rileggendolo");

			pivotXlsxOut.Seek(0, SEEK_SET);
			BMallocIO pivotReimportAscd;
			err = translator->Translate(&pivotXlsxOut, &pivotReimportInfo, NULL,
				kAtomoNativeFormat, &pivotReimportAscd);
			Check(err == B_OK, "il file XLSX con una tabella pivot si rilegge correttamente (round-trip)");

			const unsigned char* pivotReimportData = NULL;
			size_t pivotReimportLen = 0;
			bool pivotReimportUnwrapped = UnwrapFirstSheet(
				(const unsigned char*)pivotReimportAscd.Buffer(), pivotReimportAscd.BufferLength(),
				&pivotReimportData, &pivotReimportLen);
			Check(pivotReimportUnwrapped,
				"il round-trip della tabella pivot produce anch'esso una cartella ASCB valida");

			if (pivotReimportUnwrapped && pivotReimportLen > 12
				&& memcmp(pivotReimportData, "ASCD", 4) == 0)
			{
				int32 reimportCellCount;
				memcpy(&reimportCellCount, pivotReimportData + 8, 4);

				size_t pos = 12;
				for (int32 i = 0; i < reimportCellCount && pos + 9 <= pivotReimportLen; i++)
				{
					int32 clen;
					memcpy(&clen, pivotReimportData + pos + 4, 4);
					pos += 9 + clen;
				}

				std::vector<PivotTableObject> reimportedPivots;
				bool pivotSectionRead = ReadFirstPivotFromAscdForTest(pivotReimportData, pivotReimportLen,
					pos, &reimportedPivots);
				Check(pivotSectionRead,
					"la sezione tabelle pivot in coda all'ASCD riletto si legge correttamente");

				Check(reimportedPivots.size() == 1,
					"il giro completo XLSX -> ASCD ricostruisce un vero PivotTableObject "
					"(Fase 3 delle tabelle pivot), non solo le celle statiche");
				if (reimportedPivots.size() == 1)
				{
					const PivotTableObject& p = reimportedPivots[0];
					Check(p.sourceRange.left == 1 && p.sourceRange.top == 1
							&& p.sourceRange.right == 2 && p.sourceRange.bottom == 5,
						"sourceRange ricostruito combacia con la sorgente vera (A1:B5)");
					Check(p.destAnchor.h == 4 && p.destAnchor.v == 1,
						"destAnchor ricostruito combacia con la destinazione vera (D1)");
					Check(p.aggFunc == ePivotSum, "aggFunc ricostruito e' quello vero (Sum)");
					Check(p.cachedRows.size() == 2,
						"cachedRows ricostruito ha le due categorie vere (Nord/Sud)");
					if (p.cachedRows.size() == 2)
					{
						Check(BString((const char*)p.cachedRows[0].categories[0]) == "Nord"
								&& p.cachedRows[0].aggregate == 370.0 && p.cachedRows[0].count == 3,
							"la riga Nord ricostruita ha i valori veri (somma 370, 3 righe grezze)");
						Check(BString((const char*)p.cachedRows[1].categories[0]) == "Sud"
								&& p.cachedRows[1].aggregate == 140.0 && p.cachedRows[1].count == 2,
							"la riga Sud ricostruita ha i valori veri (somma 140, 2 righe grezze)");
					}
				}
			}
		}
	}

	// Ambito v1 dichiarato: una tabella pivot con 2+ colonne di
	// categoria NON produce parti OOXML pivot (layout <rowItems>
	// annidato troppo delicato per questa fase) -- le celle gia' scritte
	// restano comunque corrette, e' solo un limite noto, non un errore
	// silenzioso (vedi il commento su BuildPivotXmlParts in
	// XlsxTranslator.cpp).
	{
		CContainer& multiCatDoc = *new CContainer(NULL, NULL);
		TryToParseString("Nord", cell(1, 1), &multiCatDoc, true);   // A1
		TryToParseString("Rosso", cell(2, 1), &multiCatDoc, true);  // B1
		TryToParseString("100", cell(3, 1), &multiCatDoc, true);    // C1
		TryToParseString("Sud", cell(1, 2), &multiCatDoc, true);    // A2
		TryToParseString("Blu", cell(2, 2), &multiCatDoc, true);    // B2
		TryToParseString("50", cell(3, 2), &multiCatDoc, true);     // C2
		TryToParseString("Category1", cell(5, 1), &multiCatDoc, true); // E1
		TryToParseString("Category2", cell(6, 1), &multiCatDoc, true); // F1
		TryToParseString("Sum", cell(7, 1), &multiCatDoc, true);       // G1
		TryToParseString("Nord", cell(5, 2), &multiCatDoc, true);      // E2
		TryToParseString("Rosso", cell(6, 2), &multiCatDoc, true);     // F2
		TryToParseString("100", cell(7, 2), &multiCatDoc, true);       // G2
		TryToParseString("Sud", cell(5, 3), &multiCatDoc, true);       // E3
		TryToParseString("Blu", cell(6, 3), &multiCatDoc, true);       // F3
		TryToParseString("50", cell(7, 3), &multiCatDoc, true);        // G3

		PivotTableObject multiCatPivot;
		multiCatPivot.sourceRange = range(1, 1, 3, 2); // A1:C2, DUE colonne di categoria
		multiCatPivot.destAnchor = cell(5, 1);
		multiCatPivot.aggFunc = ePivotSum;
		{
			PivotRow r;
			r.categories.push_back(BString("Nord")); r.categories.push_back(BString("Rosso"));
			r.aggregate = 100; r.count = 1; r.minVal = 100; r.maxVal = 100;
			multiCatPivot.cachedRows.push_back(r);
		}
		{
			PivotRow r;
			r.categories.push_back(BString("Sud")); r.categories.push_back(BString("Blu"));
			r.aggregate = 50; r.count = 1; r.minVal = 50; r.maxVal = 50;
			multiCatPivot.cachedRows.push_back(r);
		}

		BMallocIO multiCatAscdIn;
		status_t multiCatSaveErr = WriteASCDWithPivotForTest(&multiCatDoc, multiCatPivot, &multiCatAscdIn);
		Check(multiCatSaveErr == B_OK,
			"preparazione dell'ASCD di prova con una tabella pivot a 2 colonne di categoria riesce");
		multiCatDoc.Release();

		multiCatAscdIn.Seek(0, SEEK_SET);
		translator_info multiCatInfo;
		err = translator->Identify(&multiCatAscdIn, NULL, NULL, &multiCatInfo, kAtomoXlsxFormat);
		Check(err == B_OK && multiCatInfo.type == kAtomoNativeFormat,
			"Identify riconosce l'ASCD di prova con una tabella pivot a 2 colonne di categoria");

		multiCatAscdIn.Seek(0, SEEK_SET);
		BMallocIO multiCatXlsxOut;
		err = translator->Translate(&multiCatAscdIn, &multiCatInfo, NULL, kAtomoXlsxFormat, &multiCatXlsxOut);
		Check(err == B_OK,
			"Translate ASCD (pivot a 2 colonne di categoria) -> XLSX riesce comunque (solo celle, ambito v1)");

		if (err == B_OK)
		{
			multiCatXlsxOut.Seek(0, SEEK_SET);
			CZipReader multiCatZip;
			Check(multiCatZip.Open(&multiCatXlsxOut),
				"il file XLSX (pivot a 2 colonne di categoria) e' un vero archivio ZIP leggibile");
			Check(!multiCatZip.HasEntry("xl/pivotTables/pivotTable1.xml"),
				"nessuna parte pivotTable viene scritta per una tabella pivot fuori dall'ambito v1 (2+ colonne di categoria)");
			Check(!multiCatZip.HasEntry("xl/pivotCache/pivotCacheDefinition1.xml"),
				"nessuna parte pivotCache viene scritta per una tabella pivot fuori dall'ambito v1 (2+ colonne di categoria)");

			std::vector<unsigned char> sheetBytes;
			if (multiCatZip.ReadEntry("xl/worksheets/sheet1.xml", sheetBytes))
			{
				std::string sheet((const char*)&sheetBytes[0], sheetBytes.size());
				Check(sheet.find("Category1") != std::string::npos && sheet.find("Rosso") != std::string::npos,
					"le celle gia' scritte (fuori ambito v1) restano comunque corrette nell'export");
			}
			else
				Check(false, "xl/worksheets/sheet1.xml si legge dall'archivio (pivot a 2 colonne di categoria)");

			// Fase 3: con nessuna parte pivot esportata (verificato sopra),
			// la riimportazione non ha nulla da ricostruire -- solo le
			// celle, nessun PivotTableObject fasullo.
			multiCatXlsxOut.Seek(0, SEEK_SET);
			translator_info multiCatReimportInfo;
			err = translator->Identify(&multiCatXlsxOut, NULL, NULL, &multiCatReimportInfo, 0);
			Check(err == B_OK && multiCatReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX (pivot a 2 colonne di categoria) si riconosce ancora come XLSX valido rileggendolo");

			multiCatXlsxOut.Seek(0, SEEK_SET);
			BMallocIO multiCatReimportAscd;
			err = translator->Translate(&multiCatXlsxOut, &multiCatReimportInfo, NULL,
				kAtomoNativeFormat, &multiCatReimportAscd);
			Check(err == B_OK,
				"il file XLSX (pivot a 2 colonne di categoria) si rilegge correttamente (round-trip)");

			const unsigned char* multiCatReimportData = NULL;
			size_t multiCatReimportLen = 0;
			bool multiCatReimportUnwrapped = UnwrapFirstSheet(
				(const unsigned char*)multiCatReimportAscd.Buffer(), multiCatReimportAscd.BufferLength(),
				&multiCatReimportData, &multiCatReimportLen);
			Check(multiCatReimportUnwrapped,
				"il round-trip (pivot a 2 colonne di categoria) produce anch'esso una cartella ASCB valida");

			if (multiCatReimportUnwrapped && multiCatReimportLen > 12
				&& memcmp(multiCatReimportData, "ASCD", 4) == 0)
			{
				int32 reimportCellCount;
				memcpy(&reimportCellCount, multiCatReimportData + 8, 4);

				size_t pos = 12;
				for (int32 i = 0; i < reimportCellCount && pos + 9 <= multiCatReimportLen; i++)
				{
					int32 clen;
					memcpy(&clen, multiCatReimportData + pos + 4, 4);
					pos += 9 + clen;
				}

				std::vector<PivotTableObject> reimportedPivots;
				bool pivotSectionRead = ReadFirstPivotFromAscdForTest(multiCatReimportData,
					multiCatReimportLen, pos, &reimportedPivots);
				Check(pivotSectionRead,
					"la sezione tabelle pivot in coda all'ASCD riletto (2 colonne di categoria) si legge correttamente");
				Check(reimportedPivots.empty(),
					"nessun PivotTableObject fasullo ricostruito per una tabella pivot fuori dall'ambito v1 "
					"(2+ colonne di categoria), coerente con l'assenza di parti pivot in esportazione");
			}
		}
	}

	// Tabelle pivot 2D (campo Colonne + misure multiple): sorgente A
	// (chiave di riga), B (campo Colonne), C/D (due misure) -- stesso
	// schema Nord/Sud/Est/Ovest usato per dimostrare la funzione dal
	// vivo, con una riga ripetuta per gruppo per esercitare davvero
	// l'aggregazione (non solo il passaggio di un valore singolo).
	{
		CContainer& pivot2DDoc = *new CContainer(NULL, NULL);
		// Sorgente: A1:D6, 6 righe grezze.
		TryToParseString("Nord", cell(1, 1), &pivot2DDoc, true);  TryToParseString("Est", cell(2, 1), &pivot2DDoc, true);
		TryToParseString("100", cell(3, 1), &pivot2DDoc, true);   TryToParseString("10", cell(4, 1), &pivot2DDoc, true);
		TryToParseString("Nord", cell(1, 2), &pivot2DDoc, true);  TryToParseString("Ovest", cell(2, 2), &pivot2DDoc, true);
		TryToParseString("200", cell(3, 2), &pivot2DDoc, true);   TryToParseString("20", cell(4, 2), &pivot2DDoc, true);
		TryToParseString("Sud", cell(1, 3), &pivot2DDoc, true);   TryToParseString("Est", cell(2, 3), &pivot2DDoc, true);
		TryToParseString("50", cell(3, 3), &pivot2DDoc, true);    TryToParseString("5", cell(4, 3), &pivot2DDoc, true);
		TryToParseString("Sud", cell(1, 4), &pivot2DDoc, true);   TryToParseString("Ovest", cell(2, 4), &pivot2DDoc, true);
		TryToParseString("80", cell(3, 4), &pivot2DDoc, true);    TryToParseString("8", cell(4, 4), &pivot2DDoc, true);
		TryToParseString("Nord", cell(1, 5), &pivot2DDoc, true);  TryToParseString("Est", cell(2, 5), &pivot2DDoc, true);
		TryToParseString("10", cell(3, 5), &pivot2DDoc, true);    TryToParseString("1", cell(4, 5), &pivot2DDoc, true);
		TryToParseString("Sud", cell(1, 6), &pivot2DDoc, true);   TryToParseString("Ovest", cell(2, 6), &pivot2DDoc, true);
		TryToParseString("20", cell(3, 6), &pivot2DDoc, true);    TryToParseString("2", cell(4, 6), &pivot2DDoc, true);

		// Destinazione: F1:J4, stesso layout a DUE righe di intestazione
		// che WritePivotTable2D avrebbe scritto (riga 1 = valore del
		// campo Colonne ripetuto per misura, riga 2 = etichetta di
		// categoria/misura), poi una riga per gruppo di riga (Nord/Sud,
		// ordine lessicografico).
		TryToParseString("Est", cell(7, 1), &pivot2DDoc, true);   TryToParseString("Est", cell(8, 1), &pivot2DDoc, true);
		TryToParseString("Ovest", cell(9, 1), &pivot2DDoc, true); TryToParseString("Ovest", cell(10, 1), &pivot2DDoc, true);
		TryToParseString("Categoria", cell(6, 2), &pivot2DDoc, true);
		TryToParseString("TotA", cell(7, 2), &pivot2DDoc, true);  TryToParseString("TotB", cell(8, 2), &pivot2DDoc, true);
		TryToParseString("TotA", cell(9, 2), &pivot2DDoc, true);  TryToParseString("TotB", cell(10, 2), &pivot2DDoc, true);
		TryToParseString("Nord", cell(6, 3), &pivot2DDoc, true);
		TryToParseString("110", cell(7, 3), &pivot2DDoc, true);   TryToParseString("11", cell(8, 3), &pivot2DDoc, true);
		TryToParseString("200", cell(9, 3), &pivot2DDoc, true);   TryToParseString("20", cell(10, 3), &pivot2DDoc, true);
		TryToParseString("Sud", cell(6, 4), &pivot2DDoc, true);
		TryToParseString("50", cell(7, 4), &pivot2DDoc, true);    TryToParseString("5", cell(8, 4), &pivot2DDoc, true);
		TryToParseString("100", cell(9, 4), &pivot2DDoc, true);   TryToParseString("10", cell(10, 4), &pivot2DDoc, true);

		PivotTableObject pivot2D;
		pivot2D.sourceRange = range(1, 1, 4, 6); // A1:D6
		pivot2D.destAnchor = cell(6, 1); // F1
		pivot2D.columnFieldCol = 2; // B
		{
			PivotMeasure m;
			m.sourceCol = 3; m.aggFunc = ePivotSum; m.label = "TotA"; // C
			pivot2D.measures.push_back(m);
		}
		{
			PivotMeasure m;
			m.sourceCol = 4; m.aggFunc = ePivotSum; m.label = "TotB"; // D
			pivot2D.measures.push_back(m);
		}
		pivot2D.columnValues.push_back(BString("Est"));
		pivot2D.columnValues.push_back(BString("Ovest"));
		{
			PivotRow2D r;
			r.categories.push_back(BString("Nord"));
			r.cells.resize(2);
			r.cells[0].resize(2); r.cells[1].resize(2);
			r.cells[0][0].aggregate = 110; r.cells[0][0].count = 2; r.cells[0][0].minVal = 10; r.cells[0][0].maxVal = 100;
			r.cells[0][1].aggregate = 11;  r.cells[0][1].count = 2; r.cells[0][1].minVal = 1;  r.cells[0][1].maxVal = 10;
			r.cells[1][0].aggregate = 200; r.cells[1][0].count = 1; r.cells[1][0].minVal = 200; r.cells[1][0].maxVal = 200;
			r.cells[1][1].aggregate = 20;  r.cells[1][1].count = 1; r.cells[1][1].minVal = 20;  r.cells[1][1].maxVal = 20;
			pivot2D.cachedRows2D.push_back(r);
		}
		{
			PivotRow2D r;
			r.categories.push_back(BString("Sud"));
			r.cells.resize(2);
			r.cells[0].resize(2); r.cells[1].resize(2);
			r.cells[0][0].aggregate = 50; r.cells[0][0].count = 1; r.cells[0][0].minVal = 50; r.cells[0][0].maxVal = 50;
			r.cells[0][1].aggregate = 5;  r.cells[0][1].count = 1; r.cells[0][1].minVal = 5;  r.cells[0][1].maxVal = 5;
			r.cells[1][0].aggregate = 100; r.cells[1][0].count = 2; r.cells[1][0].minVal = 20; r.cells[1][0].maxVal = 80;
			r.cells[1][1].aggregate = 10;  r.cells[1][1].count = 2; r.cells[1][1].minVal = 2;  r.cells[1][1].maxVal = 8;
			pivot2D.cachedRows2D.push_back(r);
		}

		BMallocIO pivot2DAscdIn;
		status_t pivot2DSaveErr = WriteASCDWithPivot2DForTest(&pivot2DDoc, pivot2D, &pivot2DAscdIn);
		Check(pivot2DSaveErr == B_OK, "preparazione dell'ASCD di prova con una tabella pivot 2D riesce");
		pivot2DDoc.Release();

		pivot2DAscdIn.Seek(0, SEEK_SET);
		translator_info pivot2DInfo;
		err = translator->Identify(&pivot2DAscdIn, NULL, NULL, &pivot2DInfo, kAtomoXlsxFormat);
		Check(err == B_OK && pivot2DInfo.type == kAtomoNativeFormat,
			"Identify riconosce l'ASCD di prova con una tabella pivot 2D");

		pivot2DAscdIn.Seek(0, SEEK_SET);
		BMallocIO pivot2DXlsxOut;
		err = translator->Translate(&pivot2DAscdIn, &pivot2DInfo, NULL, kAtomoXlsxFormat, &pivot2DXlsxOut);
		Check(err == B_OK, "Translate ASCD (con una tabella pivot 2D) -> XLSX riesce");

		if (err == B_OK)
		{
			pivot2DXlsxOut.Seek(0, SEEK_SET);
			CZipReader pivot2DZip;
			Check(pivot2DZip.Open(&pivot2DXlsxOut),
				"il file XLSX con una tabella pivot 2D e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> pt2DBytes;
			bool readPt2D = pivot2DZip.ReadEntry("xl/pivotTables/pivotTable1.xml", pt2DBytes);
			Check(readPt2D, "il file XLSX con una tabella pivot 2D contiene xl/pivotTables/pivotTable1.xml");
			if (readPt2D)
			{
				std::string pt2D((const char*)&pt2DBytes[0], pt2DBytes.size());
				Check(pt2D.find("<colFields count=\"1\">") != std::string::npos
						&& pt2D.find("<field x=\"1\"/></colFields>") != std::string::npos,
					"pivotTable1.xml (2D) scrive un vero <colFields> per il campo Colonne");
				Check(pt2D.find("<dataFields count=\"2\">") != std::string::npos,
					"pivotTable1.xml (2D) scrive due <dataField> reali, una per misura");
				Check(pt2D.find("fld=\"2\"") != std::string::npos && pt2D.find("fld=\"3\"") != std::string::npos,
					"pivotTable1.xml (2D) referenzia i cache field giusti per le due misure (2 e 3)");
				Check(pt2D.find("axis=\"axisCol\"") != std::string::npos,
					"pivotTable1.xml (2D) marca il campo Colonne con axis=\"axisCol\"");
			}

			std::vector<unsigned char> cd2DBytes;
			bool readCd2D = pivot2DZip.ReadEntry("xl/pivotCache/pivotCacheDefinition1.xml", cd2DBytes);
			Check(readCd2D, "il file XLSX con una tabella pivot 2D contiene pivotCacheDefinition1.xml");
			if (readCd2D)
			{
				std::string cd2D((const char*)&cd2DBytes[0], cd2DBytes.size());
				Check(cd2D.find("<cacheFields count=\"4\">") != std::string::npos,
					"pivotCacheDefinition1.xml (2D) ha 4 cacheField (chiave riga + campo Colonne + 2 misure)");
				Check(cd2D.find("Est") != std::string::npos && cd2D.find("Ovest") != std::string::npos,
					"pivotCacheDefinition1.xml (2D) elenca i valori veri del campo Colonne");
			}

			std::vector<unsigned char> cr2DBytes;
			bool readCr2D = pivot2DZip.ReadEntry("xl/pivotCache/pivotCacheRecords1.xml", cr2DBytes);
			Check(readCr2D, "il file XLSX con una tabella pivot 2D contiene pivotCacheRecords1.xml");
			if (readCr2D)
			{
				std::string cr2D((const char*)&cr2DBytes[0], cr2DBytes.size());
				Check(cr2D.find("count=\"6\"") != std::string::npos,
					"pivotCacheRecords1.xml (2D) conta le 6 righe GREZZE della sorgente");
			}

			// Fase 3 (import): lo stesso file appena esportato si
			// riimporta, verificando che un vero PivotTableObject 2D
			// venga ricostruito -- non solo le celle statiche.
			pivot2DXlsxOut.Seek(0, SEEK_SET);
			translator_info pivot2DReimportInfo;
			err = translator->Identify(&pivot2DXlsxOut, NULL, NULL, &pivot2DReimportInfo, 0);
			Check(err == B_OK && pivot2DReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX con una tabella pivot 2D si riconosce ancora come XLSX valido rileggendolo");

			pivot2DXlsxOut.Seek(0, SEEK_SET);
			BMallocIO pivot2DReimportAscd;
			err = translator->Translate(&pivot2DXlsxOut, &pivot2DReimportInfo, NULL,
				kAtomoNativeFormat, &pivot2DReimportAscd);
			Check(err == B_OK, "il file XLSX con una tabella pivot 2D si rilegge correttamente (round-trip)");

			const unsigned char* pivot2DReimportData = NULL;
			size_t pivot2DReimportLen = 0;
			bool pivot2DReimportUnwrapped = UnwrapFirstSheet(
				(const unsigned char*)pivot2DReimportAscd.Buffer(), pivot2DReimportAscd.BufferLength(),
				&pivot2DReimportData, &pivot2DReimportLen);
			Check(pivot2DReimportUnwrapped,
				"il round-trip della tabella pivot 2D produce anch'esso una cartella ASCB valida");

			if (pivot2DReimportUnwrapped && pivot2DReimportLen > 12
				&& memcmp(pivot2DReimportData, "ASCD", 4) == 0)
			{
				int32 reimportCellCount;
				memcpy(&reimportCellCount, pivot2DReimportData + 8, 4);

				size_t pos = 12;
				for (int32 i = 0; i < reimportCellCount && pos + 9 <= pivot2DReimportLen; i++)
				{
					int32 clen;
					memcpy(&clen, pivot2DReimportData + pos + 4, 4);
					pos += 9 + clen;
				}

				std::vector<PivotTableObject> reimportedPivots2D;
				bool pivot2DSectionRead = ReadFirstPivot2DFromAscdForTest(pivot2DReimportData,
					pivot2DReimportLen, pos, &reimportedPivots2D);
				Check(pivot2DSectionRead,
					"la sezione tabelle pivot 2D in coda all'ASCD riletto si legge correttamente");

				Check(reimportedPivots2D.size() == 1,
					"il giro completo XLSX -> ASCD ricostruisce un vero PivotTableObject 2D");
				if (reimportedPivots2D.size() == 1)
				{
					const PivotTableObject& p = reimportedPivots2D[0];
					Check(p.columnFieldCol == 2, "columnFieldCol ricostruito combacia con la colonna vera (B)");
					Check(p.measures.size() == 2, "measures ricostruite sono due");
					if (p.measures.size() == 2)
					{
						Check(p.measures[0].sourceCol == 3 && p.measures[0].aggFunc == ePivotSum,
							"la prima misura ricostruita combacia (colonna C, Somma)");
						Check(p.measures[1].sourceCol == 4 && p.measures[1].aggFunc == ePivotSum,
							"la seconda misura ricostruita combacia (colonna D, Somma)");
					}
					Check(p.columnValues.size() == 2
							&& BString((const char*)p.columnValues[0]) == "Est"
							&& BString((const char*)p.columnValues[1]) == "Ovest",
						"columnValues ricostruiti sono Est/Ovest, ordine lessicografico");
					Check(p.cachedRows2D.size() == 2,
						"cachedRows2D ricostruite hanno i due gruppi veri (Nord/Sud)");
					if (p.cachedRows2D.size() == 2)
					{
						const PivotRow2D& nordRow = p.cachedRows2D[0];
						Check(BString((const char*)nordRow.categories[0]) == "Nord",
							"il primo gruppo ricostruito e' Nord");
						Check(nordRow.cells[0][0].aggregate == 110 && nordRow.cells[0][0].count == 2,
							"Nord/Est/TotA ricostruito e' corretto (somma 110, 2 righe grezze)");
						Check(nordRow.cells[1][1].aggregate == 20 && nordRow.cells[1][1].count == 1,
							"Nord/Ovest/TotB ricostruito e' corretto (somma 20, 1 riga grezza)");

						const PivotRow2D& sudRow = p.cachedRows2D[1];
						Check(BString((const char*)sudRow.categories[0]) == "Sud",
							"il secondo gruppo ricostruito e' Sud");
						Check(sudRow.cells[1][0].aggregate == 100 && sudRow.cells[1][0].count == 2,
							"Sud/Ovest/TotA ricostruito e' corretto (somma 100, 2 righe grezze)");
					}
				}
			}
		}
	}

	// Ambito v1 dichiarato (esportazione 2D): una tabella pivot 2D con
	// 2+ colonne chiave di riga (dopo aver escluso campo Colonne/misure)
	// NON produce parti OOXML pivot -- stesso principio del caso 1D
	// analogo sopra, le celle gia' scritte restano comunque corrette.
	{
		CContainer& multiRowKeyDoc = *new CContainer(NULL, NULL);
		TryToParseString("Nord", cell(1, 1), &multiRowKeyDoc, true);  // A1: chiave di riga 1
		TryToParseString("Rosso", cell(2, 1), &multiRowKeyDoc, true); // B1: chiave di riga 2 (non e' il campo Colonne ne' una misura)
		TryToParseString("Est", cell(3, 1), &multiRowKeyDoc, true);   // C1: campo Colonne
		TryToParseString("100", cell(4, 1), &multiRowKeyDoc, true);   // D1: misura
		TryToParseString("Sud", cell(1, 2), &multiRowKeyDoc, true);
		TryToParseString("Blu", cell(2, 2), &multiRowKeyDoc, true);
		TryToParseString("Ovest", cell(3, 2), &multiRowKeyDoc, true);
		TryToParseString("50", cell(4, 2), &multiRowKeyDoc, true);
		TryToParseString("Marker", cell(6, 1), &multiRowKeyDoc, true); // F1: marcatore per il controllo sotto

		PivotTableObject multiRowKeyPivot;
		multiRowKeyPivot.sourceRange = range(1, 1, 4, 2); // A1:D2
		multiRowKeyPivot.destAnchor = cell(8, 1);
		multiRowKeyPivot.columnFieldCol = 3; // C
		{
			PivotMeasure m;
			m.sourceCol = 4; m.aggFunc = ePivotSum; m.label = "Tot";
			multiRowKeyPivot.measures.push_back(m);
		}
		// cachedRows2D lasciato vuoto apposta: irrilevante per questo
		// controllo (si verifica solo che NESSUNA parte pivot venga
		// scritta, non il contenuto di una eventuale parte).

		BMallocIO multiRowKeyAscdIn;
		status_t multiRowKeySaveErr = WriteASCDWithPivot2DForTest(&multiRowKeyDoc, multiRowKeyPivot,
			&multiRowKeyAscdIn);
		Check(multiRowKeySaveErr == B_OK,
			"preparazione dell'ASCD di prova con una tabella pivot 2D a 2 colonne chiave di riga riesce");
		multiRowKeyDoc.Release();

		multiRowKeyAscdIn.Seek(0, SEEK_SET);
		translator_info multiRowKeyInfo;
		err = translator->Identify(&multiRowKeyAscdIn, NULL, NULL, &multiRowKeyInfo, kAtomoXlsxFormat);
		Check(err == B_OK && multiRowKeyInfo.type == kAtomoNativeFormat,
			"Identify riconosce l'ASCD di prova con una tabella pivot 2D a 2 colonne chiave di riga");

		multiRowKeyAscdIn.Seek(0, SEEK_SET);
		BMallocIO multiRowKeyXlsxOut;
		err = translator->Translate(&multiRowKeyAscdIn, &multiRowKeyInfo, NULL, kAtomoXlsxFormat,
			&multiRowKeyXlsxOut);
		Check(err == B_OK,
			"Translate ASCD (pivot 2D a 2 colonne chiave di riga) -> XLSX riesce comunque (solo celle, ambito v1)");

		if (err == B_OK)
		{
			multiRowKeyXlsxOut.Seek(0, SEEK_SET);
			CZipReader multiRowKeyZip;
			Check(multiRowKeyZip.Open(&multiRowKeyXlsxOut),
				"il file XLSX (pivot 2D a 2 colonne chiave di riga) e' un vero archivio ZIP leggibile");
			Check(!multiRowKeyZip.HasEntry("xl/pivotTables/pivotTable1.xml"),
				"nessuna parte pivotTable viene scritta per un pivot 2D fuori dall'ambito v1 (2+ colonne chiave di riga)");
			Check(!multiRowKeyZip.HasEntry("xl/pivotCache/pivotCacheDefinition1.xml"),
				"nessuna parte pivotCache viene scritta per un pivot 2D fuori dall'ambito v1 (2+ colonne chiave di riga)");

			std::vector<unsigned char> sheetBytes;
			if (multiRowKeyZip.ReadEntry("xl/worksheets/sheet1.xml", sheetBytes))
			{
				std::string sheet((const char*)&sheetBytes[0], sheetBytes.size());
				Check(sheet.find("Marker") != std::string::npos,
					"le celle gia' scritte (fuori ambito v1) restano comunque corrette nell'export (pivot 2D)");
			}
			else
				Check(false, "xl/worksheets/sheet1.xml si legge dall'archivio (pivot 2D a 2 colonne chiave di riga)");
		}
	}

	// Formattazione condizionale, esportazione XLSX (ROADMAP.md "Path to
	// full Excel parity" Tier 3, export -- prima di questo, WriteXLSX
	// non scriveva MAI un <conditionalFormatting>/<dxf>, per nessun
	// tipo di regola, vecchia o nuova): un fixture con tutti e sei i
	// tipi di regola modellati dal motore, su colonne non sovrapposte.
	// Le regole cellIs e duplicateValues condividono deliberatamente lo
	// stesso bgColor (rosso) per verificare anche il dedup del dxf.
	{
		CContainer& cfDoc = *new CContainer(NULL, NULL);
		TryToParseString("High", cell(1, 1), &cfDoc, true);  // A1: cellIs
		TryToParseString("Low", cell(1, 2), &cfDoc, true);   // A2
		TryToParseString("X", cell(2, 1), &cfDoc, true);     // B1: duplicateValues
		TryToParseString("X", cell(2, 2), &cfDoc, true);     // B2
		TryToParseString("1", cell(3, 1), &cfDoc, true);     // C1: expression
		TryToParseString("2", cell(3, 2), &cfDoc, true);     // C2
		TryToParseString("10", cell(4, 1), &cfDoc, true);    // D1: colorScale
		TryToParseString("20", cell(4, 2), &cfDoc, true);    // D2
		TryToParseString("30", cell(5, 1), &cfDoc, true);    // E1: dataBar
		TryToParseString("40", cell(5, 2), &cfDoc, true);    // E2
		TryToParseString("50", cell(6, 1), &cfDoc, true);    // F1: iconSet
		TryToParseString("60", cell(6, 2), &cfDoc, true);    // F2

		rgb_color red = { 255, 0, 0, 255 };
		rgb_color green = { 0, 255, 0, 255 };
		rgb_color white = { 255, 255, 255, 255 };
		rgb_color blue = { 0, 0, 255, 255 };

		std::vector<ConditionalFormatRule> cfRules;
		{
			ConditionalFormatRule r;
			r.type = eCondCellIsEqual;
			r.compareValue = "High";
			r.bgColor = red;
			r.ranges.push_back(range(1, 1, 1, 3));
			cfRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondDuplicateValues;
			r.bgColor = red; // stesso colore della regola sopra: deve condividere il dxfId
			r.ranges.push_back(range(2, 1, 2, 3));
			cfRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondExpression;
			r.expressionFormula = "C1=1";
			r.bgColor = green;
			r.ranges.push_back(range(3, 1, 3, 3));
			cfRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondColorScale;
			ColorScalePoint pMin; pMin.cfvoType = "min"; pMin.color = white;
			ColorScalePoint pMax; pMax.cfvoType = "max"; pMax.color = blue;
			r.colorScalePoints.push_back(pMin);
			r.colorScalePoints.push_back(pMax);
			r.ranges.push_back(range(4, 1, 4, 3));
			cfRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondDataBar;
			ColorScalePoint pMin; pMin.cfvoType = "min";
			ColorScalePoint pMax; pMax.cfvoType = "max";
			r.colorScalePoints.push_back(pMin);
			r.colorScalePoints.push_back(pMax);
			r.dataBarColor = blue;
			r.ranges.push_back(range(5, 1, 5, 3));
			cfRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondIconSet;
			ColorScalePoint p0; p0.cfvoType = "percent"; p0.cfvoValue = 0;
			ColorScalePoint p1; p1.cfvoType = "percent"; p1.cfvoValue = 33;
			ColorScalePoint p2; p2.cfvoType = "percent"; p2.cfvoValue = 67;
			r.colorScalePoints.push_back(p0);
			r.colorScalePoints.push_back(p1);
			r.colorScalePoints.push_back(p2);
			r.iconSetStyle = "3TrafficLights1";
			r.ranges.push_back(range(6, 1, 6, 3));
			cfRules.push_back(r);
		}

		BMallocIO cfAscdIn;
		status_t cfSaveErr = WriteASCDWithCondFormatForTest(&cfDoc, cfRules, &cfAscdIn);
		Check(cfSaveErr == B_OK, "preparazione dell'ASCD di prova con sei regole di formattazione condizionale riesce");
		cfDoc.Release();

		cfAscdIn.Seek(0, SEEK_SET);
		translator_info cfInfo;
		err = translator->Identify(&cfAscdIn, NULL, NULL, &cfInfo, kAtomoXlsxFormat);
		Check(err == B_OK && cfInfo.type == kAtomoNativeFormat,
			"Identify riconosce l'ASCD di prova con formattazione condizionale");

		cfAscdIn.Seek(0, SEEK_SET);
		BMallocIO cfXlsxOut;
		err = translator->Translate(&cfAscdIn, &cfInfo, NULL, kAtomoXlsxFormat, &cfXlsxOut);
		Check(err == B_OK, "Translate ASCD (con formattazione condizionale) -> XLSX riesce");

		if (err == B_OK)
		{
			cfXlsxOut.Seek(0, SEEK_SET);
			CZipReader cfZip;
			Check(cfZip.Open(&cfXlsxOut),
				"il file XLSX con formattazione condizionale e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> sheetBytes;
			bool readSheet = cfZip.ReadEntry("xl/worksheets/sheet1.xml", sheetBytes);
			Check(readSheet, "il file XLSX con formattazione condizionale contiene xl/worksheets/sheet1.xml");

			std::vector<unsigned char> stylesBytes;
			bool readStyles = cfZip.ReadEntry("xl/styles.xml", stylesBytes);
			Check(readStyles, "il file XLSX con formattazione condizionale contiene xl/styles.xml");

			if (readSheet && readStyles)
			{
				std::string sheet((const char*)&sheetBytes[0], sheetBytes.size());
				std::string styles((const char*)&stylesBytes[0], stylesBytes.size());

				Check(styles.find("<dxfs count=\"2\">") != std::string::npos,
					"styles.xml ha due dxf (cellIs/duplicateValues condividono lo stesso colore, expression ne ha uno diverso)");
				Check(styles.find("<dxf><fill><patternFill><bgColor rgb=\"FFFF0000\"/>") != std::string::npos,
					"styles.xml ha il dxf rosso vero (cellIs/duplicateValues)");
				Check(styles.find("<dxf><fill><patternFill><bgColor rgb=\"FF00FF00\"/>") != std::string::npos,
					"styles.xml ha il dxf verde vero (expression)");

				Check(sheet.find("<conditionalFormatting sqref=\"A1:A3\">") != std::string::npos,
					"sheet1.xml scrive il vero sqref della regola cellIs (A1:A3)");
				Check(sheet.find("<cfRule type=\"cellIs\" dxfId=\"0\" priority=\"1\" operator=\"equal\">"
						"<formula>&quot;High&quot;</formula></cfRule>") != std::string::npos,
					"sheet1.xml scrive un vero <cfRule type=\"cellIs\"> con la formula letterale corretta");

				Check(sheet.find("<conditionalFormatting sqref=\"B1:B3\">"
						"<cfRule type=\"duplicateValues\" dxfId=\"0\" priority=\"2\"/>") != std::string::npos,
					"sheet1.xml scrive un vero <cfRule type=\"duplicateValues\">, stesso dxfId di cellIs (colore condiviso)");

				Check(sheet.find("<cfRule type=\"expression\" dxfId=\"1\" priority=\"3\">"
						"<formula>C1=1</formula></cfRule>") != std::string::npos,
					"sheet1.xml scrive un vero <cfRule type=\"expression\"> con la formula grezza, dxfId diverso (colore diverso)");

				Check(sheet.find("<cfRule type=\"colorScale\" priority=\"4\"><colorScale>"
						"<cfvo type=\"min\"/><cfvo type=\"max\"/>"
						"<color rgb=\"FFFFFFFF\"/><color rgb=\"FF0000FF\"/></colorScale></cfRule>") != std::string::npos,
					"sheet1.xml scrive un vero <cfRule type=\"colorScale\">, val omesso per min/max");

				Check(sheet.find("<cfRule type=\"dataBar\" priority=\"5\"><dataBar>"
						"<cfvo type=\"min\"/><cfvo type=\"max\"/>"
						"<color rgb=\"FF0000FF\"/></dataBar></cfRule>") != std::string::npos,
					"sheet1.xml scrive un vero <cfRule type=\"dataBar\">, un solo colore (dataBarColor)");

				Check(sheet.find("<cfRule type=\"iconSet\" priority=\"6\">"
						"<iconSet iconSet=\"3TrafficLights1\">"
						"<cfvo type=\"percent\" val=\"0\"/><cfvo type=\"percent\" val=\"33\"/>"
						"<cfvo type=\"percent\" val=\"67\"/></iconSet></cfRule>") != std::string::npos,
					"sheet1.xml scrive un vero <cfRule type=\"iconSet\">, nessun <color> (mai previsto per questo tipo)");
			}

			// Round-trip: la stessa formattazione condizionale
			// riesportata deve sopravvivere alla riimportazione XLSX ->
			// ASCD -- prova che sia l'importazione (gia' esistente) sia
			// la nuova esportazione producono dati coerenti fra loro.
			cfXlsxOut.Seek(0, SEEK_SET);
			translator_info cfReimportInfo;
			err = translator->Identify(&cfXlsxOut, NULL, NULL, &cfReimportInfo, 0);
			Check(err == B_OK && cfReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX con formattazione condizionale si riconosce ancora come XLSX valido rileggendolo");

			cfXlsxOut.Seek(0, SEEK_SET);
			BMallocIO cfReimportAscd;
			err = translator->Translate(&cfXlsxOut, &cfReimportInfo, NULL, kAtomoNativeFormat, &cfReimportAscd);
			Check(err == B_OK, "il file XLSX con formattazione condizionale si rilegge correttamente (round-trip)");

			const unsigned char* cfReimportData = NULL;
			size_t cfReimportLen = 0;
			bool cfReimportUnwrapped = UnwrapFirstSheet(
				(const unsigned char*)cfReimportAscd.Buffer(), cfReimportAscd.BufferLength(),
				&cfReimportData, &cfReimportLen);
			Check(cfReimportUnwrapped,
				"il round-trip della formattazione condizionale produce anch'esso una cartella ASCB valida");

			if (cfReimportUnwrapped && cfReimportLen > 12 && memcmp(cfReimportData, "ASCD", 4) == 0)
			{
				int32 reimportCellCount;
				memcpy(&reimportCellCount, cfReimportData + 8, 4);

				size_t pos = 12;
				for (int32 i = 0; i < reimportCellCount && pos + 9 <= cfReimportLen; i++)
				{
					int32 clen;
					memcpy(&clen, cfReimportData + pos + 4, 4);
					pos += 9 + clen;
				}

				std::vector<ConditionalFormatRule> reimportedRules;
				bool cfSectionRead = ReadCondFormatRulesFromAscdForTest(cfReimportData, cfReimportLen,
					pos, &reimportedRules);
				Check(cfSectionRead,
					"la sezione formattazione condizionale in coda all'ASCD riletto si legge correttamente");
				Check(reimportedRules.size() == 6,
					"il giro completo XLSX -> ASCD ricostruisce tutte e sei le regole");
				if (reimportedRules.size() == 6)
				{
					Check(reimportedRules[0].type == eCondCellIsEqual
							&& reimportedRules[0].compareValue == "High"
							&& !reimportedRules[0].compareIsCellRef
							&& reimportedRules[0].bgColor.red == 255 && reimportedRules[0].bgColor.green == 0,
						"la regola cellIs ricostruita combacia (valore letterale, colore rosso)");
					Check(reimportedRules[1].type == eCondDuplicateValues
							&& reimportedRules[1].bgColor.red == 255 && reimportedRules[1].bgColor.green == 0,
						"la regola duplicateValues ricostruita combacia (colore rosso, condiviso col dxf di cellIs)");
					Check(reimportedRules[2].type == eCondExpression
							&& reimportedRules[2].expressionFormula == "C1=1",
						"la regola expression ricostruita combacia (formula grezza)");
					Check(reimportedRules[3].type == eCondColorScale
							&& reimportedRules[3].colorScalePoints.size() == 2
							&& reimportedRules[3].colorScalePoints[0].cfvoType == "min"
							&& reimportedRules[3].colorScalePoints[1].cfvoType == "max"
							&& reimportedRules[3].colorScalePoints[1].color.blue == 255,
						"la regola colorScale ricostruita combacia (due soglie min/max, colore massimo blu)");
					Check(reimportedRules[4].type == eCondDataBar
							&& reimportedRules[4].colorScalePoints.size() == 2
							&& reimportedRules[4].dataBarColor.blue == 255,
						"la regola dataBar ricostruita combacia (due soglie, colore della barra blu)");
					Check(reimportedRules[5].type == eCondIconSet
							&& reimportedRules[5].colorScalePoints.size() == 3
							&& reimportedRules[5].iconSetStyle == "3TrafficLights1",
						"la regola iconSet ricostruita combacia (tre soglie, nome dello stile)");
				}
			}
		}
	}

	// Formattazione condizionale, ROADMAP "Path to full Excel parity"
	// Tier 3: cellIs con un operatore diverso da "equal" (greaterThan),
	// cellIs "between" (le due <formula>/<formula2>), containsText,
	// containsBlanks, top10 e aboveAverage -- un fixture con un
	// rappresentante per famiglia, su colonne non sovrapposte (G..L),
	// colori tutti diversi cosi' ogni dxfId e' univoco e facile da
	// verificare senza dipendere dal dedup (gia' provato sopra).
	{
		CContainer& newDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(7, 1), &newDoc, true);  // G1: cellIs greaterThan
		TryToParseString("20", cell(7, 2), &newDoc, true);  // G2
		TryToParseString("5", cell(8, 1), &newDoc, true);   // H1: cellIs between
		TryToParseString("15", cell(8, 2), &newDoc, true);  // H2
		TryToParseString("apple pie", cell(9, 1), &newDoc, true); // I1: containsText
		TryToParseString("grape", cell(9, 2), &newDoc, true);     // I2
		// J1..J3 (containsBlanks) restano deliberatamente senza contenuto.
		TryToParseString("30", cell(11, 1), &newDoc, true); // K1: top10
		TryToParseString("40", cell(11, 2), &newDoc, true); // K2
		TryToParseString("50", cell(12, 1), &newDoc, true); // L1: aboveAverage
		TryToParseString("60", cell(12, 2), &newDoc, true); // L2

		rgb_color colorA = { 255, 0, 0, 255 };
		rgb_color colorB = { 0, 255, 0, 255 };
		rgb_color colorC = { 0, 0, 255, 255 };
		rgb_color colorD = { 255, 255, 0, 255 };
		rgb_color colorE = { 255, 0, 255, 255 };
		rgb_color colorF = { 0, 255, 255, 255 };

		std::vector<ConditionalFormatRule> newRules;
		{
			ConditionalFormatRule r;
			r.type = eCondCellIsEqual;
			r.ruleOperator = 2; // greaterThan
			r.compareValue = "15";
			r.bgColor = colorA;
			r.ranges.push_back(range(7, 1, 7, 3));
			newRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondCellIsEqual;
			r.ruleOperator = 6; // between
			r.compareValue = "10";
			r.compareValue2 = "20";
			r.bgColor = colorB;
			r.ranges.push_back(range(8, 1, 8, 3));
			newRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondTextRule;
			r.ruleOperator = 0; // containsText
			r.compareValue = "apple";
			r.bgColor = colorC;
			r.ranges.push_back(range(9, 1, 9, 3));
			newRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondBlankErrorRule;
			r.ruleOperator = 0; // containsBlanks
			r.bgColor = colorD;
			r.ranges.push_back(range(10, 1, 10, 3));
			newRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondTop10;
			r.top10Rank = 3;
			r.bgColor = colorE;
			r.ranges.push_back(range(11, 1, 11, 3));
			newRules.push_back(r);
		}
		{
			ConditionalFormatRule r;
			r.type = eCondAboveAverage;
			r.bgColor = colorF;
			r.ranges.push_back(range(12, 1, 12, 3));
			newRules.push_back(r);
		}

		BMallocIO newAscdIn;
		status_t newSaveErr = WriteASCDWithCondFormatForTest(&newDoc, newRules, &newAscdIn);
		Check(newSaveErr == B_OK,
			"preparazione dell'ASCD di prova con le regole ECMA-376 aggiuntive (cellIs/containsText/top10/aboveAverage) riesce");
		newDoc.Release();

		newAscdIn.Seek(0, SEEK_SET);
		translator_info newInfo;
		err = translator->Identify(&newAscdIn, NULL, NULL, &newInfo, kAtomoXlsxFormat);
		Check(err == B_OK && newInfo.type == kAtomoNativeFormat,
			"Identify riconosce l'ASCD di prova con le regole ECMA-376 aggiuntive");

		newAscdIn.Seek(0, SEEK_SET);
		BMallocIO newXlsxOut;
		err = translator->Translate(&newAscdIn, &newInfo, NULL, kAtomoXlsxFormat, &newXlsxOut);
		Check(err == B_OK, "Translate ASCD (con le regole ECMA-376 aggiuntive) -> XLSX riesce");

		if (err == B_OK)
		{
			newXlsxOut.Seek(0, SEEK_SET);
			CZipReader newZip;
			Check(newZip.Open(&newXlsxOut),
				"il file XLSX con le regole ECMA-376 aggiuntive e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> sheetBytes, stylesBytes;
			bool readSheet = newZip.ReadEntry("xl/worksheets/sheet1.xml", sheetBytes);
			bool readStyles = newZip.ReadEntry("xl/styles.xml", stylesBytes);
			Check(readSheet && readStyles,
				"xl/worksheets/sheet1.xml e xl/styles.xml si leggono dall'archivio (regole ECMA-376 aggiuntive)");

			if (readSheet && readStyles)
			{
				std::string sheet((const char*)&sheetBytes[0], sheetBytes.size());
				std::string styles((const char*)&stylesBytes[0], stylesBytes.size());

				Check(styles.find("<dxfs count=\"6\">") != std::string::npos,
					"styles.xml ha sei dxf distinti (un colore diverso per regola)");

				Check(sheet.find("<conditionalFormatting sqref=\"G1:G3\">"
						"<cfRule type=\"cellIs\" dxfId=\"0\" priority=\"1\" operator=\"greaterThan\">"
						"<formula>&quot;15&quot;</formula></cfRule>") != std::string::npos,
					"sheet1.xml scrive cellIs con operator=\"greaterThan\" (non piu' solo \"equal\")");

				Check(sheet.find("<conditionalFormatting sqref=\"H1:H3\">"
						"<cfRule type=\"cellIs\" dxfId=\"1\" priority=\"2\" operator=\"between\">"
						"<formula>&quot;10&quot;</formula><formula2>&quot;20&quot;</formula2></cfRule>") != std::string::npos,
					"sheet1.xml scrive cellIs \"between\" con <formula> E <formula2>");

				Check(sheet.find("<conditionalFormatting sqref=\"I1:I3\">"
						"<cfRule type=\"containsText\" dxfId=\"2\" priority=\"3\" operator=\"containsText\" text=\"apple\">"
						"<formula>NOT(ISERROR(SEARCH(&quot;apple&quot;,I1)))</formula></cfRule>") != std::string::npos,
					"sheet1.xml scrive containsText con l'attributo text= e una formula SEARCH equivalente");

				Check(sheet.find("<conditionalFormatting sqref=\"J1:J3\">"
						"<cfRule type=\"containsBlanks\" dxfId=\"3\" priority=\"4\">"
						"<formula>LEN(TRIM(J1))=0</formula></cfRule>") != std::string::npos,
					"sheet1.xml scrive containsBlanks con una formula LEN(TRIM(...))=0 equivalente");

				Check(sheet.find("<conditionalFormatting sqref=\"K1:K3\">"
						"<cfRule type=\"top10\" dxfId=\"4\" priority=\"5\" rank=\"3\"/>") != std::string::npos,
					"sheet1.xml scrive top10 con rank=\"3\", nessun percent/bottom (entrambi falsi)");

				Check(sheet.find("<conditionalFormatting sqref=\"L1:L3\">"
						"<cfRule type=\"aboveAverage\" dxfId=\"5\" priority=\"6\" "
						"aboveAverage=\"1\" equalAverage=\"0\"/>") != std::string::npos,
					"sheet1.xml scrive aboveAverage con aboveAverage=\"1\" (sopra, non sotto)");
			}

			// Round-trip: le stesse sei regole riesportate devono
			// sopravvivere alla riimportazione XLSX -> ASCD.
			newXlsxOut.Seek(0, SEEK_SET);
			translator_info newReimportInfo;
			err = translator->Identify(&newXlsxOut, NULL, NULL, &newReimportInfo, 0);
			Check(err == B_OK && newReimportInfo.type == kAtomoXlsxFormat,
				"il file XLSX con le regole ECMA-376 aggiuntive si riconosce ancora come XLSX valido rileggendolo");

			newXlsxOut.Seek(0, SEEK_SET);
			BMallocIO newReimportAscd;
			err = translator->Translate(&newXlsxOut, &newReimportInfo, NULL, kAtomoNativeFormat, &newReimportAscd);
			Check(err == B_OK, "il file XLSX con le regole ECMA-376 aggiuntive si rilegge correttamente (round-trip)");

			const unsigned char* newReimportData = NULL;
			size_t newReimportLen = 0;
			bool newReimportUnwrapped = UnwrapFirstSheet(
				(const unsigned char*)newReimportAscd.Buffer(), newReimportAscd.BufferLength(),
				&newReimportData, &newReimportLen);
			Check(newReimportUnwrapped,
				"il round-trip delle regole ECMA-376 aggiuntive produce anch'esso una cartella ASCB valida");

			if (newReimportUnwrapped && newReimportLen > 12 && memcmp(newReimportData, "ASCD", 4) == 0)
			{
				int32 reimportCellCount;
				memcpy(&reimportCellCount, newReimportData + 8, 4);

				size_t pos = 12;
				for (int32 i = 0; i < reimportCellCount && pos + 9 <= newReimportLen; i++)
				{
					int32 clen;
					memcpy(&clen, newReimportData + pos + 4, 4);
					pos += 9 + clen;
				}

				std::vector<ConditionalFormatRule> reimportedNewRules;
				bool newSectionRead = ReadCondFormatRulesFromAscdForTest(newReimportData, newReimportLen,
					pos, &reimportedNewRules);
				Check(newSectionRead,
					"la sezione formattazione condizionale (regole ECMA-376 aggiuntive) si legge correttamente");
				Check(reimportedNewRules.size() == 6,
					"il giro completo XLSX -> ASCD ricostruisce tutte e sei le regole ECMA-376 aggiuntive");
				if (reimportedNewRules.size() == 6)
				{
					Check(reimportedNewRules[0].type == eCondCellIsEqual
							&& reimportedNewRules[0].ruleOperator == 2
							&& reimportedNewRules[0].compareValue == "15",
						"la regola cellIs greaterThan ricostruita combacia (operatore e valore)");
					Check(reimportedNewRules[1].type == eCondCellIsEqual
							&& reimportedNewRules[1].ruleOperator == 6
							&& reimportedNewRules[1].compareValue == "10"
							&& reimportedNewRules[1].compareValue2 == "20",
						"la regola cellIs between ricostruita combacia (limite inferiore E superiore)");
					Check(reimportedNewRules[2].type == eCondTextRule
							&& reimportedNewRules[2].ruleOperator == 0
							&& reimportedNewRules[2].compareValue == "apple",
						"la regola containsText ricostruita combacia (testo cercato)");
					Check(reimportedNewRules[3].type == eCondBlankErrorRule
							&& reimportedNewRules[3].ruleOperator == 0,
						"la regola containsBlanks ricostruita combacia (tipo e operatore)");
					Check(reimportedNewRules[4].type == eCondTop10
							&& reimportedNewRules[4].top10Rank == 3
							&& !reimportedNewRules[4].top10Percent
							&& !reimportedNewRules[4].top10Bottom,
						"la regola top10 ricostruita combacia (rank=3, non percentuale, non ultimi)");
					Check(reimportedNewRules[5].type == eCondAboveAverage
							&& !reimportedNewRules[5].belowAverage,
						"la regola aboveAverage ricostruita combacia (sopra, non sotto)");
				}
			}
		}
	}

	// Regressione: un documento SENZA nessuna regola di formattazione
	// condizionale non deve scrivere ne' <dxfs> ne' <conditionalFormatting>
	// per niente -- non solo un <dxfs count="0"> vuoto, l'elemento stesso
	// deve mancare, per non introdurre rumore in un file altrimenti
	// identico a prima di questa funzionalita'.
	{
		CContainer& noCfDoc = *new CContainer(NULL, NULL);
		TryToParseString("Ciao", cell(1, 1), &noCfDoc, true);

		std::vector<ConditionalFormatRule> noRules;
		BMallocIO noCfAscdIn;
		status_t noCfSaveErr = WriteASCDWithCondFormatForTest(&noCfDoc, noRules, &noCfAscdIn);
		Check(noCfSaveErr == B_OK, "preparazione dell'ASCD di prova senza regole riesce");
		noCfDoc.Release();

		noCfAscdIn.Seek(0, SEEK_SET);
		translator_info noCfInfo;
		err = translator->Identify(&noCfAscdIn, NULL, NULL, &noCfInfo, kAtomoXlsxFormat);
		Check(err == B_OK && noCfInfo.type == kAtomoNativeFormat,
			"Identify riconosce l'ASCD di prova senza regole");

		noCfAscdIn.Seek(0, SEEK_SET);
		BMallocIO noCfXlsxOut;
		err = translator->Translate(&noCfAscdIn, &noCfInfo, NULL, kAtomoXlsxFormat, &noCfXlsxOut);
		Check(err == B_OK, "Translate ASCD (senza regole) -> XLSX riesce");

		if (err == B_OK)
		{
			noCfXlsxOut.Seek(0, SEEK_SET);
			CZipReader noCfZip;
			Check(noCfZip.Open(&noCfXlsxOut), "il file XLSX senza regole e' un vero archivio ZIP leggibile");

			std::vector<unsigned char> sheetBytes, stylesBytes;
			bool readSheet = noCfZip.ReadEntry("xl/worksheets/sheet1.xml", sheetBytes);
			bool readStyles = noCfZip.ReadEntry("xl/styles.xml", stylesBytes);
			if (readSheet && readStyles)
			{
				std::string sheet((const char*)&sheetBytes[0], sheetBytes.size());
				std::string styles((const char*)&stylesBytes[0], stylesBytes.size());
				Check(sheet.find("conditionalFormatting") == std::string::npos,
					"un documento senza regole non scrive nessun <conditionalFormatting>");
				Check(styles.find("dxfs") == std::string::npos,
					"un documento senza regole non scrive nemmeno un <dxfs> vuoto");
			}
			else
				Check(false, "xl/worksheets/sheet1.xml e xl/styles.xml si leggono dall'archivio (senza regole)");
		}
	}

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
