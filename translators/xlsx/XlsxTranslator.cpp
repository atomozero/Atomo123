/*
	XlsxTranslator.cpp

	Vedi XlsxTranslator.h per la descrizione generale.
*/

#include "XlsxTranslator.h"
#include "MiniZip.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <expat.h>

#include <Font.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "Constants.h"
#include "EmbeddedImage.h"
#include "Formatter.h"
#include "FontMetrics.h"

static const translation_format sInputFormats[] = {
	{
		kAtomoXlsxFormat, kAtomoSheetGroup, 0.9f, 0.9f,
		"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
		"Microsoft Excel 2007+ (XLSX)"
	}
};

static const translation_format sOutputFormats[] = {
	{
		kAtomoNativeFormat, kAtomoSheetGroup, 1.0f, 1.0f,
		"application/x-vnd.atomo-sheet-data", "Atomo Sheet Cell Data (ASCD)"
	},
	{
		kAtomoXlsxFormat, kAtomoSheetGroup, 0.7f, 0.7f,
		"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
		"Microsoft Excel 2007+ (XLSX)"
	}
};

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
// Versione 2 (era 1): aggiunge un byte "kind" per cella, vedi il
// commento su kASCDVersion in ui/src/AscdIO.cpp (stesso identico
// motivo, duplicato qui per lo stesso motivo di WriteASCD sotto).
static const int32 kASCDVersion = 2;
enum { kAscdCellFormula = 0, kAscdCellLiteralOther = 1, kAscdCellLiteralText = 2 };
// Formato "cartella di lavoro" multi-foglio (Fase 9): duplicato da
// ui/src/AscdIO.h/.cpp (magic "ASCB", conteggio fogli, poi per
// ciascuno nome + un blocco ASCD completo), stesso motivo della
// duplicazione gia' esistente di WriteASCD/ReadASCD sopra -- i
// translator non linkano contro ui/src/, per non introdurre una
// dipendenza di link fra loro e l'app.
static const char kASCDBookMagic[4] = { 'A', 'S', 'C', 'B' };

// Duplicati da ui/src/AscdIO.cpp (stesso formato binario per la
// sezione colori, vedi il commento su WriteASCD sotto).
static status_t WriteColorEntry(BPositionIO* dest, rgb_color bg, rgb_color fg)
{
	uint8 buf[8] = { bg.red, bg.green, bg.blue, bg.alpha, fg.red, fg.green, fg.blue, fg.alpha };
	return dest->Write(buf, sizeof(buf)) == (ssize_t)sizeof(buf) ? B_OK : B_IO_ERROR;
}

static bool ColorsEqual(rgb_color a, rgb_color b)
{
	return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

// Stessa serializzazione ASCD degli altri translator (vedi
// translators/csv/CsvTranslator.cpp per la descrizione completa).
static status_t WriteASCD(CContainer* doc, BPositionIO* dest,
	const std::vector<std::pair<int, float> >* colWidths = NULL,
	const std::vector<EmbeddedImage>* images = NULL,
	const std::vector<std::pair<int, float> >* rowHeights = NULL,
	const bool* showGrid = NULL,
	const bool* hasTabColor = NULL, const rgb_color* tabColor = NULL,
	const std::vector<int>* hiddenRows = NULL,
	const bool* hasAutoFilter = NULL, const range* autoFilterRange = NULL)
{
	// Range completo invece dei limiti di GetBounds: una cella con
	// formula non ancora calcolata (mType eNoData) verrebbe esclusa
	// dai limiti calcolati da GetBounds, e se e' anche la cella piu' a
	// destra/in basso del foglio sparirebbe del tutto dal file
	// prodotto (bug scoperto e corretto costruendo l'export ODS, vedi
	// ROADMAP.md Fase 5 -- stesso ragionamento del ciclo di ricalcolo
	// sotto).
	int32 count = 0;
	CCellIterator counter(doc, NULL);
	cell c;
	while (counter.NextExisting(c))
		count++;

	if (dest->Write(kASCDMagic, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kASCDVersion, sizeof(kASCDVersion)) != (ssize_t)sizeof(kASCDVersion))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;

	CCellIterator iter(doc, NULL);
	while (iter.NextExisting(c))
	{
		char text[4096];
		doc->GetCellFormula(c, text, sizeof(text), false);

		int16 row = c.v, col = c.h;
		int32 len = strlen(text);

		// "kind": vedi lo stesso identico commento in ui/src/AscdIO.cpp
		// (SaveASCD) -- senza questo byte, un valore testo importato da
		// questo stesso translator (es. "P-EL-a" nella colonna Codice di
		// una tabella strutturata) sopravviveva all'importazione in
		// memoria (vedi il ramo dedicato appena sopra in SheetEnd) ma
		// tornava a corrompersi in una formula NaN non appena questo
		// WriteASCD lo serializzava e ui/src/AscdIO.cpp::LoadASCD lo
		// rileggeva, perche' il testo grezzo da solo non basta a
		// distinguere "era gia' una formula" da "era solo testo".
		uint8 kind;
		if (doc->GetCellFormula(c) != NULL)
			kind = kAscdCellFormula;
		else
		{
			Value v;
			doc->GetValue(c, v);
			kind = (v.fType == eTextData) ? kAscdCellLiteralText : kAscdCellLiteralOther;
		}

		if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_IO_ERROR;
		if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_IO_ERROR;
		if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_IO_ERROR;
		if (dest->Write(&kind, sizeof(kind)) != (ssize_t)sizeof(kind))
			return B_IO_ERROR;
		if (len > 0 && dest->Write(text, len) != len)
			return B_IO_ERROR;
	}

	// Sezione grafici incorporati, in coda: sempre vuota qui (questo
	// translator non legge/scrive grafici), ma il campo va scritto
	// comunque per compatibilita' col formato di ui/src/AscdIO.cpp
	// (SaveASCD/LoadASCD), che lo prevede sempre. Senza questo campo
	// esplicito, quando WriteASCDBook incapsula piu' fogli in
	// sequenza nello stesso flusso, LoadASCD (chiamato da
	// LoadASCDBook una volta per foglio) non puo' distinguere "fine
	// del flusso, nessun grafico" (fine vera) da "qui non c'e' la
	// sezione grafici" (fine del SOLO blocco di questo foglio, con
	// altri fogli a seguire): interpreterebbe i primi 4 byte del
	// foglio successivo (la lunghezza del suo nome) come un numero di
	// grafici, disallineando la lettura di ogni foglio dopo il primo.
	// Bug reale scoperto aprendo un file .xlsm con 38 fogli: solo il
	// primo veniva letto correttamente.
	int32 chartCount = 0;
	if (dest->Write(&chartCount, sizeof(chartCount)) != (ssize_t)sizeof(chartCount))
		return B_IO_ERROR;

	// Sezione larghezze di colonna personalizzate, in coda: stesso
	// principio della sezione grafici sopra (sempre scritta anche se
	// vuota, mai NULL a questo livello) -- lette da <cols> nel foglio
	// XLSX originale (vedi ParseSheet/SheetStart), un elenco (colonna
	// 1-based, larghezza in pixel) delle sole colonne con una
	// larghezza esplicita diversa da quella predefinita.
	int32 colWidthCount = colWidths ? (int32)colWidths->size() : 0;
	if (dest->Write(&colWidthCount, sizeof(colWidthCount)) != (ssize_t)sizeof(colWidthCount))
		return B_IO_ERROR;

	for (int32 i = 0; i < colWidthCount; i++)
	{
		int16 col = (int16)(*colWidths)[i].first;
		float width = (*colWidths)[i].second;
		if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
			|| dest->Write(&width, sizeof(width)) != (ssize_t)sizeof(width))
			return B_IO_ERROR;
	}

	// Sezione colori di cella/colonna, in coda: stesso principio delle
	// due sezioni sopra -- lette da "doc" tramite CContainer::
	// GetCellStyle/GetColumnStyle (gia' popolate durante ParseSheet/
	// SheetStart per le celle/colonne con un colore esplicito nel file
	// XLSX originale), non un parametro a parte: il colore vive gia'
	// dentro il documento, esattamente come per ui/src/AscdIO.cpp
	// (duplicato qui, stesso motivo di WriteASCD in generale -- questo
	// translator non linka contro ui/src/).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, CellStyle> > cellStyles;
		CCellIterator styleIter(doc, NULL);
		cell sc;
		while (styleIter.NextExisting(sc))
		{
			CellStyle cs;
			doc->GetCellStyle(sc, cs);
			if (!ColorsEqual(cs.fLowColor, defaultStyle.fLowColor)
				|| !ColorsEqual(cs.fHighColor, defaultStyle.fHighColor))
				cellStyles.push_back(std::make_pair(sc, cs));
		}

		int32 cellColorCount = (int32)cellStyles.size();
		if (dest->Write(&cellColorCount, sizeof(cellColorCount))
				!= (ssize_t)sizeof(cellColorCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < cellColorCount; i++)
		{
			int16 row = cellStyles[i].first.v, col = cellStyles[i].first.h;
			const CellStyle& cs = cellStyles[i].second;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| WriteColorEntry(dest, cs.fLowColor, cs.fHighColor) != B_OK)
				return B_IO_ERROR;
		}

		std::vector<std::pair<int, CellStyle> > columnStyles;
		for (int col = 1; col <= kColCount; col++)
		{
			CellStyle cs;
			doc->GetColumnStyle(col, cs);
			if (!ColorsEqual(cs.fLowColor, defaultStyle.fLowColor)
				|| !ColorsEqual(cs.fHighColor, defaultStyle.fHighColor))
				columnStyles.push_back(std::make_pair(col, cs));
		}

		int32 columnColorCount = (int32)columnStyles.size();
		if (dest->Write(&columnColorCount, sizeof(columnColorCount))
				!= (ssize_t)sizeof(columnColorCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < columnColorCount; i++)
		{
			int16 col = (int16)columnStyles[i].first;
			const CellStyle& cs = columnStyles[i].second;
			if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| WriteColorEntry(dest, cs.fLowColor, cs.fHighColor) != B_OK)
				return B_IO_ERROR;
		}
	}

	// Sezione altezze di riga, in coda (Fase 10 di ui/src/AscdIO.cpp):
	// lette da <row ht="..." customHeight="1"> nel foglio XLSX
	// originale (vedi ParseSheet/SheetStart), stesso principio di
	// colWidths sopra -- un tempo sempre vuota qui, bug reale segnalato
	// dall'utente (immagini incorporate ancorate a righe alte
	// nell'originale finivano sovrapposte al testo sottostante,
	// disegnate sulle righe piu' basse di 20px predefinite invece delle
	// altezze vere del file). Blocca riquadri resta a zero: questo
	// translator non lo estrae ancora dal file XLSX originale. Il campo
	// va comunque scritto sempre, mai omesso, per lo stesso motivo della
	// sezione grafici sopra: LoadASCD (in ui/src/AscdIO.cpp, che legge
	// questo stesso flusso) si aspetta ORA tutte queste sezioni in coda
	// a ogni blocco ASCD -- ometterle disallineerebbe la lettura del
	// blocco successivo in una cartella di lavoro multi-foglio,
	// esattamente come il bug gia' descritto sopra per i grafici (bug
	// reale scoperto aprendo di nuovo lo stesso file .xlsm da 38 fogli
	// dopo l'aggiunta di quelle sezioni in Fase 10/11: leggeva byte del
	// foglio successivo come se fossero l'altezza di una riga del
	// foglio corrente).
	int32 rowHeightCount = rowHeights ? (int32)rowHeights->size() : 0;
	if (dest->Write(&rowHeightCount, sizeof(rowHeightCount)) != (ssize_t)sizeof(rowHeightCount))
		return B_IO_ERROR;

	for (int32 i = 0; i < rowHeightCount; i++)
	{
		int16 row = (int16)(*rowHeights)[i].first;
		float height = (*rowHeights)[i].second;
		if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
			|| dest->Write(&height, sizeof(height)) != (ssize_t)sizeof(height))
			return B_IO_ERROR;
	}

	int32 frozenRows = 0, frozenCols = 0;
	if (dest->Write(&frozenRows, sizeof(frozenRows)) != (ssize_t)sizeof(frozenRows)
		|| dest->Write(&frozenCols, sizeof(frozenCols)) != (ssize_t)sizeof(frozenCols))
		return B_IO_ERROR;

	// Sezione font di cella non predefinito, in coda (Fase 10, vedi
	// ui/src/AscdIO.cpp): a differenza delle sezioni vuote sopra/sotto,
	// questo translator estrae davvero grassetto/corsivo dal file XLSX
	// originale (Fase 12, ResolveStyle in ParseStyles, applicato a
	// CellStyle::fFont durante ParseSheet) -- va quindi scritta con i
	// valori reali, stesso formato di AscdIO.cpp (famiglia/stile/
	// dimensione gia' risolti, mai l'indice grezzo -- vedi il commento
	// li' sul perche' fFont e' un indice VOLATILE).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, CellStyle> > toWrite;
		CCellIterator fontIter(doc, NULL);
		cell fc2;
		while (fontIter.NextExisting(fc2))
		{
			CellStyle cs;
			doc->GetCellStyle(fc2, cs);
			if (cs.fFont != defaultStyle.fFont)
				toWrite.push_back(std::make_pair(fc2, cs));
		}

		int32 fontCount = (int32)toWrite.size();
		if (dest->Write(&fontCount, sizeof(fontCount)) != (ssize_t)sizeof(fontCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < fontCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			font_family family;
			font_style style;
			float size;
			gFontSizeTable.GetFontInfo(toWrite[i].second.fFont, &family, &style, &size);
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(family, sizeof(font_family)) != (ssize_t)sizeof(font_family)
				|| dest->Write(style, sizeof(font_style)) != (ssize_t)sizeof(font_style)
				|| dest->Write(&size, sizeof(size)) != (ssize_t)sizeof(size))
				return B_IO_ERROR;
		}
	}

	// Sezione allineamento di cella non predefinito, in coda (Fase 10,
	// vedi ui/src/AscdIO.cpp): a differenza della sezione bordi ancora
	// vuota sotto, questo translator estrae davvero l'allineamento
	// orizzontale dal file XLSX originale (Fase 12, <alignment
	// horizontal="..."/> in ParseStyles, applicato a CellStyle::
	// fAlignment durante ParseSheet) -- va quindi scritta con i valori
	// reali. Un solo byte per cella, nessuna risoluzione necessaria (a
	// differenza del font sopra, CellStyle::fAlignment e' gia' il
	// valore finale, non un indice).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, char> > toWrite;
		CCellIterator alignIter(doc, NULL);
		cell ac;
		while (alignIter.NextExisting(ac))
		{
			CellStyle cs;
			doc->GetCellStyle(ac, cs);
			if (cs.fAlignment != defaultStyle.fAlignment)
				toWrite.push_back(std::make_pair(ac, cs.fAlignment));
		}

		int32 alignCount = (int32)toWrite.size();
		if (dest->Write(&alignCount, sizeof(alignCount)) != (ssize_t)sizeof(alignCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < alignCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			int8 alignment = toWrite[i].second;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&alignment, sizeof(alignment)) != (ssize_t)sizeof(alignment))
				return B_IO_ERROR;
		}
	}

	// Sezione bordi di cella non predefiniti, in coda (Fase 11, vedi
	// ui/src/AscdIO.cpp): a differenza della sezione formato numero
	// ancora vuota sotto, questo translator estrae davvero i bordi dal
	// file XLSX originale (Fase 12, borderId risolto contro <borders>
	// in ParseStyles, applicato a CellStyle::fTBorderColor ecc durante
	// ParseSheet) -- va quindi scritta con i valori reali.
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, CellStyle> > toWrite;
		CCellIterator borderIter(doc, NULL);
		cell bc;
		while (borderIter.NextExisting(bc))
		{
			CellStyle cs;
			doc->GetCellStyle(bc, cs);
			if (cs.fTBorderColor != defaultStyle.fTBorderColor
				|| cs.fLBorderColor != defaultStyle.fLBorderColor
				|| cs.fBBorderColor != defaultStyle.fBBorderColor
				|| cs.fRBorderColor != defaultStyle.fRBorderColor)
				toWrite.push_back(std::make_pair(bc, cs));
		}

		int32 borderCount = (int32)toWrite.size();
		if (dest->Write(&borderCount, sizeof(borderCount)) != (ssize_t)sizeof(borderCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < borderCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			const CellStyle& cs = toWrite[i].second;
			uint8 sides[4] = { cs.fTBorderColor, cs.fLBorderColor,
				cs.fBBorderColor, cs.fRBorderColor };
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(sides, sizeof(sides)) != (ssize_t)sizeof(sides))
				return B_IO_ERROR;
		}
	}

	// Sezione formato numero di cella non predefinito, in coda (Fase
	// 12, vedi ui/src/AscdIO.cpp): a differenza delle cinque sezioni
	// vuote sopra, questo translator estrae davvero il formato numero
	// dal file XLSX originale (ResolveNumberFormat in ParseStyles,
	// applicato a CellStyle::fFormat durante ParseSheet) -- va quindi
	// scritta con i valori reali, non a zero.
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, int32> > toWrite;
		CCellIterator formatIter(doc, NULL);
		cell fc;
		while (formatIter.NextExisting(fc))
		{
			CellStyle cs;
			doc->GetCellStyle(fc, cs);
			if (cs.fFormat != defaultStyle.fFormat)
				toWrite.push_back(std::make_pair(fc, (int32)cs.fFormat));
		}

		int32 formatCount = (int32)toWrite.size();
		if (dest->Write(&formatCount, sizeof(formatCount)) != (ssize_t)sizeof(formatCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < formatCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			int32 format = toWrite[i].second;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&format, sizeof(format)) != (ssize_t)sizeof(format))
				return B_IO_ERROR;
		}
	}

	// Sezione sottolineato di cella non predefinito, in coda (Fase 12,
	// vedi ui/src/AscdIO.cpp): questo translator estrae davvero
	// <u/> dal file XLSX originale (applicato a CellStyle::fUnderline
	// durante ParseSheet) -- va quindi scritta con i valori reali. Solo
	// riga/colonna, nessun valore da scrivere (la sola presenza vuol
	// dire true, stesso principio di ui/src/AscdIO.cpp).
	{
		CellStyle defaultStyle;
		std::vector<cell> toWrite;
		CCellIterator underlineIter(doc, NULL);
		cell uc;
		while (underlineIter.NextExisting(uc))
		{
			CellStyle cs;
			doc->GetCellStyle(uc, cs);
			if (cs.fUnderline != defaultStyle.fUnderline)
				toWrite.push_back(uc);
		}

		int32 underlineCount = (int32)toWrite.size();
		if (dest->Write(&underlineCount, sizeof(underlineCount)) != (ssize_t)sizeof(underlineCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < underlineCount; i++)
		{
			int16 row = toWrite[i].v, col = toWrite[i].h;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
				return B_IO_ERROR;
		}
	}

	// Sezione testo a capo di cella non predefinito, in coda (Fase 12,
	// vedi ui/src/AscdIO.cpp): questo translator estrae davvero
	// wrapText="1" dal file XLSX originale (applicato a CellStyle::
	// fWrapText durante ParseSheet) -- va quindi scritta con i valori
	// reali. Solo riga/colonna, nessun valore da scrivere.
	{
		CellStyle defaultStyle;
		std::vector<cell> toWrite;
		CCellIterator wrapIter(doc, NULL);
		cell wc;
		while (wrapIter.NextExisting(wc))
		{
			CellStyle cs;
			doc->GetCellStyle(wc, cs);
			if (cs.fWrapText != defaultStyle.fWrapText)
				toWrite.push_back(wc);
		}

		int32 wrapCount = (int32)toWrite.size();
		if (dest->Write(&wrapCount, sizeof(wrapCount)) != (ssize_t)sizeof(wrapCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < wrapCount; i++)
		{
			int16 row = toWrite[i].v, col = toWrite[i].h;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
				return B_IO_ERROR;
		}
	}

	// Sezione celle unite, in coda (Fase 12, vedi ui/src/AscdIO.cpp):
	// questo translator estrae davvero <mergeCell ref="..."/> dal file
	// XLSX originale (CContainer::AddMergedRange, chiamato durante
	// ParseSheet) -- va quindi scritta con i valori reali.
	{
		const std::vector<range>& merged = doc->GetMergedRanges();
		int32 mergeCount = (int32)merged.size();
		if (dest->Write(&mergeCount, sizeof(mergeCount)) != (ssize_t)sizeof(mergeCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < mergeCount; i++)
		{
			int16 top = merged[i].top, left = merged[i].left;
			int16 bottom = merged[i].bottom, right = merged[i].right;
			if (dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
				|| dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
				|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
				|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right))
				return B_IO_ERROR;
		}
	}

	// Sezione immagini incorporate, in coda (Fase 12, vedi
	// ui/src/AscdIO.cpp): questo translator estrae davvero
	// xl/drawings/+xl/media/ (DrawingPic/ParseDrawing sotto) -- va
	// quindi scritta con i valori reali, non sempre vuota come i
	// grafici sopra.
	{
		int32 imageCount = images ? (int32)images->size() : 0;
		if (dest->Write(&imageCount, sizeof(imageCount)) != (ssize_t)sizeof(imageCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < imageCount; i++)
		{
			const EmbeddedImage& img = (*images)[i];
			int16 row = img.anchor.v, col = img.anchor.h;
			float geom[4] = { img.offsetX, img.offsetY, img.width, img.height };
			int32 pngLen = (int32)img.pngData.size();

			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(geom, sizeof(geom)) != (ssize_t)sizeof(geom)
				|| dest->Write(&pngLen, sizeof(pngLen)) != (ssize_t)sizeof(pngLen))
				return B_IO_ERROR;
			if (pngLen > 0 && dest->Write(&img.pngData[0], pngLen) != pngLen)
				return B_IO_ERROR;
		}
	}

	// Sezione visibilita' griglia, in coda (vedi ui/src/AscdIO.cpp):
	// questo translator estrae davvero <sheetView showGridLines="...">
	// dal file XLSX originale (SheetStart) -- va quindi scritta con il
	// valore reale, non sempre vera come i file scritti prima di questa
	// sezione.
	{
		uint8 sg = (showGrid ? *showGrid : true) ? 1 : 0;
		if (dest->Write(&sg, sizeof(sg)) != (ssize_t)sizeof(sg))
			return B_IO_ERROR;
	}

	// Sezione colore della linguetta del foglio, in coda (vedi
	// ui/src/AscdIO.cpp): questo translator estrae davvero
	// <sheetPr><tabColor rgb="..."/></sheetPr> dal file XLSX originale
	// (SheetStart) -- un byte "presente si'/no" seguito da tre byte
	// RGB (sempre scritti, ignorati in lettura se il primo byte e' 0).
	{
		uint8 has = (hasTabColor && *hasTabColor) ? 1 : 0;
		rgb_color color = { 0, 0, 0, 255 };
		if (has && tabColor)
			color = *tabColor;
		uint8 rgb[3] = { color.red, color.green, color.blue };
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
			return B_IO_ERROR;
	}

	// Sezione righe nascoste, in coda (vedi ui/src/AscdIO.cpp): questo
	// translator estrae davvero <row hidden="1"> dal file XLSX
	// originale (SheetStart) -- va quindi scritta con i valori reali.
	{
		int32 hiddenCount = hiddenRows ? (int32)hiddenRows->size() : 0;
		if (dest->Write(&hiddenCount, sizeof(hiddenCount)) != (ssize_t)sizeof(hiddenCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < hiddenCount; i++)
		{
			int16 row = (int16)(*hiddenRows)[i];
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row))
				return B_IO_ERROR;
		}
	}

	// Sezione AutoFilter, in coda (vedi ui/src/AscdIO.cpp): questo
	// translator estrae davvero <autoFilter ref="..."/> dal file XLSX
	// originale (SheetStart) -- va quindi scritta con i valori reali.
	{
		uint8 has = (hasAutoFilter && *hasAutoFilter) ? 1 : 0;
		range r = (has && autoFilterRange) ? *autoFilterRange : range();
		int16 top = r.top, left = r.left, bottom = r.bottom, right = r.right;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
			|| dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
			|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
			|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right))
			return B_IO_ERROR;
	}

	// Sezione commenti/note per cella, in coda (Fase 13, vedi
	// ui/src/AscdIO.cpp): questo translator non estrae ancora
	// <comments>/<legacyDrawing> da un file XLSX vero (rimandato, vedi
	// ROADMAP.md), quindi qui e' sempre vuota -- scritta comunque,
	// stesso principio delle altre sezioni "sempre presenti anche se
	// vuote" sopra: senza, il flusso prodotto da questo WriteASCD non
	// sarebbe piu' allineato con quanto ReadASCD/LoadASCD si aspettano
	// di leggere per QUALUNQUE altro foglio scritto dopo di questo in
	// una cartella di lavoro multi-foglio.
	{
		int32 commentCount = 0;
		if (dest->Write(&commentCount, sizeof(commentCount)) != (ssize_t)sizeof(commentCount))
			return B_IO_ERROR;
	}

	// Sezione collegamenti ipertestuali, in coda: stesso principio
	// della sezione commenti appena sopra -- questo translator non
	// estrae ancora <hyperlinks> da un file XLSX vero (rimandato,
	// vedi ROADMAP.md), quindi sempre vuota.
	{
		int32 linkCount = 0;
		if (dest->Write(&linkCount, sizeof(linkCount)) != (ssize_t)sizeof(linkCount))
			return B_IO_ERROR;
	}

	// Sezione tipo di grafico incorporato, in coda (vedi
	// ui/src/AscdIO.cpp): sempre vuota, stesso principio delle sezioni
	// commenti/collegamenti appena sopra -- il chartCount scritto piu'
	// sopra e' gia' sempre zero per questo translator.
	{
		int32 chartTypeCount = 0;
		if (dest->Write(&chartTypeCount, sizeof(chartTypeCount)) != (ssize_t)sizeof(chartTypeCount))
			return B_IO_ERROR;
	}

	// Sezione colore del bordo di cella non predefinito, in coda (vedi
	// ui/src/AscdIO.cpp): sempre vuota, stesso principio delle sezioni
	// sopra -- questo translator non estrae ancora il colore del
	// bordo da un file XLSX vero (solo la presenza/assenza per lato,
	// vedi ParseStyles), rimandato come le altre sezioni "non ancora
	// estratte" qui sopra.
	{
		int32 borderColorCount = 0;
		if (dest->Write(&borderColorCount, sizeof(borderColorCount)) != (ssize_t)sizeof(borderColorCount))
			return B_IO_ERROR;
	}

	// Sezione convalida dati, in coda (vedi ui/src/AscdIO.cpp): sempre
	// vuota, stesso principio delle sezioni sopra -- questo translator
	// non estrae la convalida dati (<dataValidations>) da un file XLSX
	// vero, rimandato come le altre sezioni "non ancora estratte" qui
	// sopra.
	{
		int32 validationCount = 0;
		if (dest->Write(&validationCount, sizeof(validationCount)) != (ssize_t)sizeof(validationCount))
			return B_IO_ERROR;
	}

	// Sezione formattazione condizionale VIVA, in coda (vedi
	// ui/src/AscdIO.cpp, STESSO ORDINE -- dopo la convalida dati, non
	// prima): a differenza delle altre sezioni "non ancora estratte"
	// qui sopra, questa SI viene popolata da questo translator
	// (ApplyConditionalFormatting sotto aggiunge le regole vere a
	// "doc" invece di congelare un colore, vedi il commento li'),
	// quindi va scritta per davvero, stesso formato di SaveASCD.
	{
		const std::vector<ConditionalFormatRule>& rules = doc->GetConditionalFormatRules();
		int32 ruleCount = (int32)rules.size();
		if (dest->Write(&ruleCount, sizeof(ruleCount)) != (ssize_t)sizeof(ruleCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < ruleCount; i++)
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
			for (int32 r = 0; r < rangeCount; r++)
			{
				const range& rg = rule.ranges[r];
				int16 left = rg.left, top = rg.top, right = rg.right, bottom = rg.bottom;
				if (dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right)
					|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
					return B_IO_ERROR;
			}
		}
	}

	// Sezione tabelle strutturate di Excel, in coda (Fase 14, vedi
	// ui/src/AscdIO.cpp, STESSO ORDINE -- dopo la formattazione
	// condizionale). A differenza delle sezioni "non ancora estratte"
	// sopra, QUESTA si popola per davvero (RegisterTable, chiamato da
	// ParseSheet quando il foglio ha <tableParts>): senza scriverla qui,
	// MainWindow::OpenFile leggerebbe un CContainer del tutto nuovo
	// senza nessuna tabella registrata (i byte ASCD sono la SOLA cosa
	// che sopravvive oltre questa funzione, vedi il commento gemello in
	// SaveASCD), e ogni "Tabella12[Colonna]" tornerebbe a calcolare
	// gNameNan invece del valore vero -- avrebbe vanificato XLOOKUP su
	// una vera Tabella Excel, lo scenario reale che ha fatto scoprire
	// questa fase.
	{
		const std::map<std::string, CTableDef>& tables = doc->GetTables();
		int32 tableCount = (int32)tables.size();
		if (dest->Write(&tableCount, sizeof(tableCount)) != (ssize_t)sizeof(tableCount))
			return B_IO_ERROR;

		for (std::map<std::string, CTableDef>::const_iterator it = tables.begin();
			it != tables.end(); ++it)
		{
			int32 nameLen = (int32)it->first.size();
			if (dest->Write(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
				return B_IO_ERROR;
			if (nameLen > 0 && dest->Write(it->first.data(), nameLen) != nameLen)
				return B_IO_ERROR;

			const CTableDef& def = it->second;
			int16 left = def.dataRange.left, top = def.dataRange.top,
				right = def.dataRange.right, bottom = def.dataRange.bottom;
			if (dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
				|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
				|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right)
				|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
				return B_IO_ERROR;

			int32 columnCount = (int32)def.columnNames.size();
			if (dest->Write(&columnCount, sizeof(columnCount)) != (ssize_t)sizeof(columnCount))
				return B_IO_ERROR;
			for (int32 c = 0; c < columnCount; c++)
			{
				int32 colLen = (int32)def.columnNames[c].size();
				if (dest->Write(&colLen, sizeof(colLen)) != (ssize_t)sizeof(colLen))
					return B_IO_ERROR;
				if (colLen > 0 && dest->Write(def.columnNames[c].data(), colLen) != colLen)
					return B_IO_ERROR;
			}
		}
	}

	return B_OK;
}

// Legge un flusso ASCD e ricostruisce le celle in "doc" (vuoto in
// ingresso) -- stessa logica di CsvTranslator.cpp/OdsTranslator.cpp,
// usata qui per l'esportazione (ASCD -> XLSX, la direzione opposta
// della normale importazione XLSX -> ASCD gestita da ParseSheet/
// WriteASCD sopra).
static status_t ReadASCD(BPositionIO* source, CContainer* doc)
{
	char magic[4];
	if (source->Read(magic, 4) != 4)
		return B_BAD_DATA;
	if (memcmp(magic, kASCDMagic, 4) != 0)
		return B_BAD_DATA;

	int32 version;
	if (source->Read(&version, sizeof(version)) != (ssize_t)sizeof(version))
		return B_BAD_DATA;
	// versione 1 e versione 2 (con il byte "kind" per cella, vedi
	// WriteASCD sopra e il commento su kASCDVersion) restano entrambe
	// leggibili -- stesso motivo di LoadASCD in ui/src/AscdIO.cpp.
	if (version != 1 && version != kASCDVersion)
		return B_MISMATCHED_VALUES;

	int32 count;
	if (source->Read(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_BAD_DATA;

	for (int32 i = 0; i < count; i++)
	{
		int16 row, col;
		int32 len;

		if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_BAD_DATA;
		if (source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_BAD_DATA;
		if (source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_BAD_DATA;

		uint8 kind = kAscdCellFormula;
		if (version >= 2 && source->Read(&kind, sizeof(kind)) != (ssize_t)sizeof(kind))
			return B_BAD_DATA;

		char text[4096];
		if (len < 0 || len >= (int32)sizeof(text))
			return B_BAD_DATA;
		if (len > 0 && source->Read(text, len) != len)
			return B_BAD_DATA;
		text[len] = 0;

		cell c(col, row);

		// Stesso principio di LoadASCD in ui/src/AscdIO.cpp: un valore
		// testo letterale (kind scritto da una versione 2 di WriteASCD)
		// non passa MAI per TryToParseString/Parse().
		if (kind == kAscdCellLiteralText)
		{
			Value v(text);
			doc->NewCell(c, v, NULL);
			continue;
		}

		try
		{
			// inWarnIfError=false, non true: stesso bug e stesso motivo
			// di LoadASCD in ui/src/AscdIO.cpp -- un valore TESTO che
			// assomiglia abbastanza a un numero/data da superare
			// l'analisi grammaticale del parser ma poi fallisce a
			// ridursi a un valore (es. "01.11.10", un codice ATECO
			// reale) fa rilanciare l'eccezione invece di ripiegare sul
			// testo originale, e il catch sotto trasformava l'intero
			// export in un fallimento totale (B_BAD_DATA) per una sola
			// cella di testo innocua.
			TryToParseString(text, c, doc, false);
		}
		catch (...)
		{
			return B_BAD_DATA;
		}
	}

	// Le formule vanno calcolate prima di esportare (l'export XLSX
	// scrive solo valori, non formule -- vedi BuildSheetXml sotto),
	// altrimenti una cella con formula risulterebbe vuota. Range
	// completo per lo stesso motivo di WriteASCD sopra.
	{
		bool changed = true;
		int guard = 0;
		while (changed && guard < 50)
		{
			changed = false;
			CCellIterator recalcIter(doc, NULL);
			cell rc;
			while (recalcIter.NextExisting(rc))
			{
				if (doc->CalcCell(rc))
					changed = true;
			}
			guard++;
		}
	}

	return B_OK;
}

static void AppendXmlEscaped(std::string& out, const char* text)
{
	for (const char* p = text; *p; p++)
	{
		switch (*p)
		{
			case '&': out += "&amp;"; break;
			case '<': out += "&lt;"; break;
			case '>': out += "&gt;"; break;
			default: out += *p;
		}
	}
}

// Genera xl/worksheets/sheet1.xml a partire dal documento: solo i
// valori calcolati (numeri/testo), non le formule -- stessa scelta
// gia' fatta per CSV/ODS. Le stringhe sono scritte inline
// (t="inlineStr"/<is><t>...</t></is>) invece che in una tabella di
// stringhe condivise (xl/sharedStrings.xml): richiederebbe una
// passata separata per raccogliere i valori unici, complessita' non
// necessaria per i fogli tipici esportati da questo programma, ed
// e' comunque sintassi OOXML valida (Excel/LibreOffice la leggono
// correttamente).
static std::string BuildSheetXml(CContainer* doc)
{
	range bounds;
	doc->GetBounds(bounds);

	std::string xml;
	xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	xml += "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">";
	xml += "<sheetData>";

	CCellIterator iter(doc, &bounds);
	cell c;
	int curRow = -1;
	char numBuf[64];
	char nameBuf[16];
	while (iter.NextExisting(c))
	{
		Value v;
		doc->GetValue(c, v);
		if (v.fType != eNumData && v.fType != eTextData)
			continue;
		if (v.fType == eNumData && v.IsNan())
			continue;

		if (c.v != curRow)
		{
			if (curRow != -1)
				xml += "</row>";
			snprintf(numBuf, sizeof(numBuf), "%d", (int)c.v);
			xml += "<row r=\"";
			xml += numBuf;
			xml += "\">";
			curRow = c.v;
		}

		c.GetName(nameBuf);

		if (v.fType == eNumData)
		{
			snprintf(numBuf, sizeof(numBuf), "%.15g", (double)v);
			xml += "<c r=\"";
			xml += nameBuf;
			xml += "\"><v>";
			xml += numBuf;
			xml += "</v></c>";
		}
		else
		{
			xml += "<c r=\"";
			xml += nameBuf;
			xml += "\" t=\"inlineStr\"><is><t>";
			AppendXmlEscaped(xml, (const char*)v);
			xml += "</t></is></c>";
		}
	}
	if (curRow != -1)
		xml += "</row>";

	xml += "</sheetData></worksheet>";
	return xml;
}

static status_t WriteXLSX(CContainer* doc, BPositionIO* dest)
{
	static const char kContentTypes[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
		"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
		"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
		"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n"
		"<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n"
		"</Types>\n";
	static const char kRootRels[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
		"</Relationships>\n";
	static const char kWorkbook[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
		"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
		"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n"
		"</workbook>\n";
	static const char kWorkbookRels[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
		"</Relationships>\n";

	std::string sheet = BuildSheetXml(doc);

	CZipWriter zip;
	zip.Begin(dest);

	if (!zip.AddEntry("[Content_Types].xml", kContentTypes, strlen(kContentTypes)))
		return B_IO_ERROR;
	if (!zip.AddEntry("_rels/.rels", kRootRels, strlen(kRootRels)))
		return B_IO_ERROR;
	if (!zip.AddEntry("xl/workbook.xml", kWorkbook, strlen(kWorkbook)))
		return B_IO_ERROR;
	if (!zip.AddEntry("xl/_rels/workbook.xml.rels", kWorkbookRels, strlen(kWorkbookRels)))
		return B_IO_ERROR;
	if (!zip.AddEntry("xl/worksheets/sheet1.xml", sheet.data(), sheet.size()))
		return B_IO_ERROR;

	return zip.Close() ? B_OK : B_IO_ERROR;
}

// Converte un serial Excel (giorni dall'epoca del sistema data della
// cartella di lavoro, con l'ora come frazione di giorno) in un
// time_t (Fase 12) -- costruendo direttamente un Value(time_t)
// (eTimeData) invece di passare da una stringa data testuale, che
// avrebbe richiesto indovinare l'ordine giorno/mese/anno locale
// (gDateOrder, dipendente dal sistema, non affidabile da un
// translator). 25569/24107 sono le costanti standard (usate anche da
// altre implementazioni non-Excel) per i giorni fra l'epoca Unix
// (1970-01-01) e l'epoca Excel nei due sistemi data: 1899-12-30 per
// il predefinito (non 1899-12-31: lo spostamento di un giorno
// compensa il famoso bug storico di Excel/Lotus 1-2-3 che tratta il
// 1900 come bisestile, con un errore di un solo giorno per le rare
// date di gennaio/febbraio 1900 -- limite accettato, stessa
// approssimazione usata da virtualmente ogni libreria non-Excel) o
// 1904-01-01 per il sistema Mac storico (<workbookPr date1904="1"/>,
// nessun bug del genere in quel sistema).
static time_t ExcelSerialToTime(double serial, bool date1904)
{
	double epochOffsetDays = date1904 ? 24107.0 : 25569.0;
	double unixSeconds = (serial - epochOffsetDays) * 86400.0;
	return (time_t)(unixSeconds + (unixSeconds >= 0 ? 0.5 : -0.5));
}

// Converte un riferimento di cella stile Excel ("A1", "AB12") in
// colonna (1-based) e riga (1-based). Restituisce false se il
// riferimento non e' valido.
static bool CellRefToColRow(const std::string& ref, int& outCol, int& outRow)
{
	size_t i = 0;
	int col = 0;
	while (i < ref.size() && isupper((unsigned char)ref[i]))
	{
		col = col * 26 + (ref[i] - 'A' + 1);
		i++;
	}
	if (i == 0 || i >= ref.size())
		return false;

	int row = atoi(ref.c_str() + i);
	if (row <= 0)
		return false;

	outCol = col;
	outRow = row;
	return true;
}

// Converte un riferimento di intervallo stile Excel ("A1:C1") in un
// "range" del motore. Un riferimento a una sola cella ("A1", senza
// ":") non e' valido qui: <mergeCell ref="A1"/> non e' un caso reale
// (Excel non scrive mai un intervallo di una sola cella), ma viene
// comunque scartato in sicurezza invece di produrre un range
// degenere.
static bool ParseMergeCellRef(const std::string& ref, range* out)
{
	size_t colon = ref.find(':');
	if (colon == std::string::npos)
		return false;

	int col1, row1, col2, row2;
	if (!CellRefToColRow(ref.substr(0, colon), col1, row1))
		return false;
	if (!CellRefToColRow(ref.substr(colon + 1), col2, row2))
		return false;

	out->Set(std::min(col1, col2), std::min(row1, row2),
		std::max(col1, col2), std::max(row1, row2));
	return true;
}

// --- Parsing di xl/sharedStrings.xml -----------------------------------
//
// Struttura minima gestita:
//   <sst><si><t>testo</t></si><si><t>altro</t></si>...</sst>
// Le stringhe "rich text" (<si><r><t>...</t></r>...</si>, con piu'
// "run" per formattazione mista) vengono concatenate: al motore non
// interessa la formattazione carattere per carattere.

struct SharedStringsContext {
	std::vector<std::string> strings;
	std::string current;
	bool inText;
};

static void XMLCALL SharedStringsStart(void* userData, const char* name,
	const char** atts)
{
	SharedStringsContext* ctx = (SharedStringsContext*)userData;
	if (strcmp(name, "si") == 0)
		ctx->current.clear();
	else if (strcmp(name, "t") == 0)
		ctx->inText = true;
}

static void XMLCALL SharedStringsEnd(void* userData, const char* name)
{
	SharedStringsContext* ctx = (SharedStringsContext*)userData;
	if (strcmp(name, "si") == 0)
		ctx->strings.push_back(ctx->current);
	else if (strcmp(name, "t") == 0)
		ctx->inText = false;
}

static void XMLCALL SharedStringsChars(void* userData, const char* s, int len)
{
	SharedStringsContext* ctx = (SharedStringsContext*)userData;
	if (ctx->inText)
		ctx->current.append(s, len);
}

static bool ParseSharedStrings(const std::vector<unsigned char>& xml,
	std::vector<std::string>& out)
{
	if (xml.empty())
		return true; // documento senza stringhe condivise: valido

	SharedStringsContext ctx;
	ctx.inText = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, SharedStringsStart, SharedStringsEnd);
	XML_SetCharacterDataHandler(parser, SharedStringsChars);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK)
		return false;

	out = ctx.strings;
	return true;
}

// --- Parsing di xl/theme/theme1.xml e xl/styles.xml (colori) -----------
//
// Un colore in XLSX si specifica in tre modi alternativi (attributi su
// <fgColor>/<color>): "rgb" (esadecimale diretto, es. "FFFFC9C9" --
// alpha+RRGGBB), "theme" (indice 0-11 nella tavolozza del documento,
// spesso con "tint" per schiarire/scurire), o "indexed" (la vecchia
// tavolozza fissa a 56 colori di Excel 97-2003, non gestita qui: rara
// nei file moderni, non documentata nello standard OOXML stesso --
// le celle che la usano restano al colore predefinito del motore).
//
// L'indice della tavolozza del tema (attributo theme="N") NON segue
// l'ordine degli elementi in <a:clrScheme> di theme1.xml (dk1/lt1/dk2/
// lt2/accent1-6/hlink/folHlink): scambia i primi quattro, una
// stranezza nota dello standard OOXML --
// 0=lt1, 1=dk1, 2=lt2, 3=dk2, 4-9=accent1-6, 10=hlink, 11=folHlink.

static bool HexToColor(const std::string& hex, rgb_color* out)
{
	if (hex.size() < 6)
		return false;
	unsigned int value = (unsigned int)strtoul(hex.substr(hex.size() - 6).c_str(), NULL, 16);
	out->red = (uint8)((value >> 16) & 0xFF);
	out->green = (uint8)((value >> 8) & 0xFF);
	out->blue = (uint8)(value & 0xFF);
	out->alpha = 255;
	return true;
}

// Formula approssimata (non l'esatta conversione RGB<->HSL luminosita'
// dello standard ECMA-376, 18.8.3): ampiamente usata da importatori piu'
// semplici, visivamente ragionevole -- stesso principio gia' scelto per
// ExcelColWidthToPixels sopra.
static uint8 ApplyTintToChannel(uint8 value, double tint)
{
	double v = value;
	if (tint < 0)
		v = v * (1.0 + tint);
	else
		v = v * (1.0 - tint) + (255.0 - 255.0 * (1.0 - tint));
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	return (uint8)(v + 0.5);
}

static rgb_color ApplyTint(rgb_color c, double tint)
{
	if (tint == 0)
		return c;
	rgb_color out = c;
	out.red = ApplyTintToChannel(c.red, tint);
	out.green = ApplyTintToChannel(c.green, tint);
	out.blue = ApplyTintToChannel(c.blue, tint);
	return out;
}

struct XlsxTheme {
	rgb_color colors[12];
	bool valid[12];
};

static int ThemeSlotForElement(const char* name)
{
	if (strcmp(name, "a:lt1") == 0) return 0;
	if (strcmp(name, "a:dk1") == 0) return 1;
	if (strcmp(name, "a:lt2") == 0) return 2;
	if (strcmp(name, "a:dk2") == 0) return 3;
	if (strcmp(name, "a:accent1") == 0) return 4;
	if (strcmp(name, "a:accent2") == 0) return 5;
	if (strcmp(name, "a:accent3") == 0) return 6;
	if (strcmp(name, "a:accent4") == 0) return 7;
	if (strcmp(name, "a:accent5") == 0) return 8;
	if (strcmp(name, "a:accent6") == 0) return 9;
	if (strcmp(name, "a:hlink") == 0) return 10;
	if (strcmp(name, "a:folHlink") == 0) return 11;
	return -1;
}

struct ThemeContext {
	bool inClrScheme;
	int currentSlot;
	std::string hex[12];
};

static void XMLCALL ThemeStart(void* userData, const char* name, const char** atts)
{
	ThemeContext* ctx = (ThemeContext*)userData;

	if (strcmp(name, "a:clrScheme") == 0)
	{
		ctx->inClrScheme = true;
		return;
	}
	if (!ctx->inClrScheme)
		return;

	int slot = ThemeSlotForElement(name);
	if (slot >= 0)
	{
		ctx->currentSlot = slot;
		return;
	}

	// <a:srgbClr val="RRGGBB"/> oppure <a:sysClr val="windowText"
	// lastClr="RRGGBB"/> (un colore di sistema, col valore RGB
	// effettivo nell'attributo lastClr).
	if (ctx->currentSlot >= 0
		&& (strcmp(name, "a:srgbClr") == 0 || strcmp(name, "a:sysClr") == 0))
	{
		const char* wantAttr = strcmp(name, "a:srgbClr") == 0 ? "val" : "lastClr";
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], wantAttr) == 0)
			{
				ctx->hex[ctx->currentSlot] = atts[i + 1];
				break;
			}
		}
	}
}

static void XMLCALL ThemeEnd(void* userData, const char* name)
{
	ThemeContext* ctx = (ThemeContext*)userData;
	if (strcmp(name, "a:clrScheme") == 0)
		ctx->inClrScheme = false;
	else if (ThemeSlotForElement(name) >= 0)
		ctx->currentSlot = -1;
}

// "theme1.xml" e' opzionale: un pacchetto malformato o senza tema
// lascia semplicemente "theme" con ogni voce non valida (valid[i] ==
// false), cosi' i colori theme="N" restano irrisolti piu' avanti
// (nessun colore applicato, non un errore).
static void ParseTheme(const std::vector<unsigned char>& xml, XlsxTheme* out)
{
	for (int i = 0; i < 12; i++)
		out->valid[i] = false;
	if (xml.empty())
		return;

	ThemeContext ctx;
	ctx.inClrScheme = false;
	ctx.currentSlot = -1;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, ThemeStart, ThemeEnd);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK)
		return;

	for (int i = 0; i < 12; i++)
	{
		if (!ctx.hex[i].empty() && HexToColor(ctx.hex[i], &out->colors[i]))
			out->valid[i] = true;
	}
}

// Risolve gli attributi di un elemento <fgColor>/<bgColor>/<color> in
// un rgb_color, in ordine di preferenza rgb > theme (indexed non
// gestito, vedi sopra). Restituisce false se l'elemento non specifica
// nessun colore risolvibile.
static bool ResolveColorAttrs(const char** atts, const XlsxTheme& theme, rgb_color* out)
{
	int themeIdx = -1;
	double tint = 0;
	std::string rgbHex;
	bool hasRgb = false;

	for (int i = 0; atts[i]; i += 2)
	{
		if (strcmp(atts[i], "rgb") == 0)
		{
			rgbHex = atts[i + 1];
			hasRgb = true;
		}
		else if (strcmp(atts[i], "theme") == 0)
			themeIdx = atoi(atts[i + 1]);
		else if (strcmp(atts[i], "tint") == 0)
			tint = atof(atts[i + 1]);
	}

	if (hasRgb)
		return HexToColor(rgbHex, out);

	if (themeIdx >= 0 && themeIdx < 12 && theme.valid[themeIdx])
	{
		*out = ApplyTint(theme.colors[themeIdx], tint);
		return true;
	}

	return false;
}

// Colore di sfondo/testo risolti per una singola voce di <cellXfs>
// (l'indice usato dall'attributo s="..." sulle celle e dall'attributo
// style="..." su <col>): "has*" false = quella voce di stile non
// specifica un colore per quel canale, il motore resta al suo
// predefinito.
struct ResolvedStyle {
	bool hasBg;
	rgb_color bg;
	bool hasFg;
	rgb_color fg;
	bool hasFormat;
	int format; // valore gia' pronto per CellStyle::fFormat
	bool isDateFormat; // Fase 12: true se numFmtId e' un formato data/ora (vedi IsDateNumFmt) -- il valore numerico grezzo va convertito in eTimeData, non lasciato come numero
	bool hasFontStyle; // true solo se grassetto e/o corsivo (il Regular predefinito non serve applicarlo)
	int fontID; // indice gia' risolto in gFontSizeTable, pronto per CellStyle::fFont
	bool hasAlignment; // true solo se diverso da eAlignGeneral (il predefinito non serve applicarlo)
	char alignment; // EAlignment, pronto per CellStyle::fAlignment
	bool hasBorders; // true solo se almeno un lato e' impostato
	uchar borderT, borderL, borderB, borderR; // 0/1, pronti per CellStyle::fTBorderColor ecc (Fase 11: booleano per lato, non un vero colore)
	bool underline; // pronto per CellStyle::fUnderline (nessun campo "has": false coincide gia' col predefinito)
	bool wrapText; // pronto per CellStyle::fWrapText (nessun campo "has", stesso motivo di underline sopra)
};

enum StylesSection { kStylesNone, kStylesNumFmts, kStylesFills, kStylesFonts, kStylesBorders, kStylesCellXfs };

// (fontId, fillId, numFmtId, borderId) per indice s= di una cella/colonna.
struct XfInfo {
	int fontId;
	int fillId;
	int numFmtId;
	int borderId;
	char alignment; // EAlignment, eAlignGeneral se <alignment> assente
	bool wrapText;
};

// Quattro lati di una voce di <borders>: presente/assente, stesso
// significato "booleano per lato" definito in Fase 11 (CellStyle::
// fTBorderColor ecc, non un vero colore/spessore nonostante il nome).
struct BorderSides {
	bool top, left, bottom, right;
};

// "general"/assente -> eAlignGeneral (nessuna preferenza esplicita,
// il ripiego predefinito di CellStyle). "fill"/"justify" mappati
// sull'equivalente piu' vicino gia' supportato da CellStyle::
// fAlignment; gli altri valori ECMA-376 (es. "centerContinuous",
// "distributed") non hanno un corrispondente e restano General.
static char ResolveHorizontalAlignment(const char* value)
{
	if (strcmp(value, "left") == 0) return eAlignLeft;
	if (strcmp(value, "center") == 0) return eAlignCenter;
	if (strcmp(value, "right") == 0) return eAlignRight;
	if (strcmp(value, "fill") == 0) return eAlignFill;
	if (strcmp(value, "justify") == 0) return eAlignJustify;
	return eAlignGeneral;
}

struct StylesContext {
	const XlsxTheme* theme;
	StylesSection section;

	std::map<int, std::string> numFmts; // numFmtId -> formatCode (solo i personalizzati, id >= 164 di norma)

	std::vector<rgb_color> fillColors;
	std::vector<bool> fillColorValid;
	std::string currentPatternType;

	std::vector<rgb_color> fontColors;
	std::vector<bool> fontColorValid;
	std::vector<bool> fontBold;
	std::vector<bool> fontItalic;
	std::vector<bool> fontUnderline;
	std::vector<float> fontSize; // -1 = non specificata nel font XLSX (<sz> assente)

	std::vector<BorderSides> borders;

	std::vector<XfInfo> cellXfs;
};

// I formati incorporati (numFmtId 0..163, es. 9="0%", 44=contabilita')
// non compaiono mai in <numFmts> (solo quelli personalizzati, di norma
// id >= 164, ma il file puo' ridefinirne uno incorporato esplicitamente
// come visto con id=44 in un file di gara reale): tabella dei piu'
// comuni, sufficiente per l'approssimazione di formato dichiarata in
// Fase 12 (vedi ResolveNumberFormat sotto). Elenco completo in ECMA-376
// 18.8.30; qui solo quelli che ricorrono davvero in fogli reali.
static const char* BuiltinNumFmtCode(int numFmtId)
{
	switch (numFmtId)
	{
		case 1: return "0";
		case 2: return "0.00";
		case 3: return "#,##0";
		case 4: return "#,##0.00";
		case 9: return "0%";
		case 10: return "0.00%";
		case 37: return "#,##0";
		case 38: return "#,##0";
		case 39: return "#,##0.00";
		case 40: return "#,##0.00";
		case 44: return "#,##0.00";
		default: return NULL;
	}
}

// Traduce un numFmtId XLSX nel formato piu' vicino gia' rappresentabile
// da CFormatter/CellStyle::fFormat, riusando la stessa euristica di
// CFormatter::ParseTemplate (cerca '$'/'%'/','/'.') invece di
// duplicarla: il formatCode XLSX (gia' decodificato dalle entita' XML
// da expat) e' quasi sempre un template compatibile cosi' com'e'.
// Limite dichiarato in ROADMAP.md (Fase 12): il colore condizionale
// per i negativi (es. "0.00;[Red]-0.00") e il simbolo di valuta
// specifico del formato vengono scartati, ParseTemplate guarda solo la
// parte prima del primo ';' e solo se contiene un carattere '$'
// letterale (non un simbolo Unicode come '€' scritto fra apici). I
// formati data/ora (heuristica: contengono lettere y/m/d/h/s fuori da
// apici, nessun carattere che ParseTemplate riconosca) sono lasciati
// al task dedicato "Formati data/ora da XLSX", non gestiti qui: se lo
// fossero, ParseTemplate li tratterebbe erroneamente come eGeneral.
// Guarda solo la parte prima del primo ';' (stessa porzione che
// ParseTemplate usa davvero, vedi sopra) e ignora sia il testo fra
// apici (es. l'unita' di misura in "0.00 \"kg\"") sia le direttive fra
// parentesi quadre (es. "[Red]", "[$€-410]"): senza quest'ultimo
// filtro la "d" di "[Red]" farebbe scambiare per data un formato
// numerico qualunque con un colore condizionale (bug reale scoperto
// scrivendo il test con numFmtId 166, "0.00;[Red]0.00").
static bool LooksLikeDateFormat(const std::string& code)
{
	bool inQuotes = false;
	bool inBrackets = false;
	for (size_t i = 0; i < code.size(); i++)
	{
		char c = code[i];
		if (!inBrackets && c == '"')
			inQuotes = !inQuotes;
		else if (!inQuotes && c == '[')
			inBrackets = true;
		else if (!inQuotes && c == ']')
			inBrackets = false;
		else if (!inQuotes && !inBrackets)
		{
			if (c == ';')
				return false; // fine della porzione rilevante, nessuna lettera data trovata
			if (c == 'y' || c == 'm' || c == 'd' || c == 'h' || c == 's'
				|| c == 'Y' || c == 'M' || c == 'D' || c == 'H' || c == 'S')
				return true;
		}
	}
	return false;
}

static bool ResolveNumberFormat(int numFmtId, const std::map<int, std::string>& numFmts,
	int* outFormat)
{
	if (numFmtId <= 0)
		return false; // 0 = General, nessuna formattazione esplicita da applicare

	std::string code;
	std::map<int, std::string>::const_iterator it = numFmts.find(numFmtId);
	if (it != numFmts.end())
		code = it->second;
	else
	{
		const char* builtin = BuiltinNumFmtCode(numFmtId);
		if (!builtin)
			return false; // id incorporato non nella nostra tabella, ignorato
		code = builtin;
	}

	if (LooksLikeDateFormat(code))
		return false;

	CFormatter formatter(code.c_str());
	*outFormat = formatter.FormatID();
	return true;
}

// true se numFmtId e' un formato data/ora (Fase 12) -- incorporato
// (id 14-22 = data, 45-47 = orario/durata, ECMA-376 18.8.30, nessuno
// di questi e' nella tabella BuiltinNumFmtCode sopra perche' quella
// serve solo ai formati NUMERICI) o personalizzato (via il testo del
// formatCode, LooksLikeDateFormat). Interrogata separatamente da
// ResolveNumberFormat sopra perche' qui serve sapere "e' una data?"
// anche per gli id incorporati che quella funzione scarta in
// sicurezza (nessun formatCode noto per costruire un CFormatter).
static bool IsDateNumFmt(int numFmtId, const std::map<int, std::string>& numFmts)
{
	if (numFmtId <= 0)
		return false;

	std::map<int, std::string>::const_iterator it = numFmts.find(numFmtId);
	if (it != numFmts.end())
		return LooksLikeDateFormat(it->second);

	return (numFmtId >= 14 && numFmtId <= 22) || (numFmtId >= 45 && numFmtId <= 47);
}

static void XMLCALL StylesStart(void* userData, const char* name, const char** atts)
{
	StylesContext* ctx = (StylesContext*)userData;

	if (strcmp(name, "numFmts") == 0) { ctx->section = kStylesNumFmts; return; }
	if (strcmp(name, "fills") == 0) { ctx->section = kStylesFills; return; }
	if (strcmp(name, "fonts") == 0) { ctx->section = kStylesFonts; return; }
	if (strcmp(name, "borders") == 0) { ctx->section = kStylesBorders; return; }
	if (strcmp(name, "cellXfs") == 0) { ctx->section = kStylesCellXfs; return; }

	if (ctx->section == kStylesNumFmts)
	{
		if (strcmp(name, "numFmt") == 0)
		{
			int numFmtId = -1;
			const char* formatCode = NULL;
			for (int i = 0; atts[i]; i += 2)
			{
				if (strcmp(atts[i], "numFmtId") == 0)
					numFmtId = atoi(atts[i + 1]);
				else if (strcmp(atts[i], "formatCode") == 0)
					formatCode = atts[i + 1];
			}
			if (numFmtId >= 0 && formatCode)
				ctx->numFmts[numFmtId] = formatCode;
		}
	}
	else if (ctx->section == kStylesFills)
	{
		if (strcmp(name, "fill") == 0)
		{
			ctx->fillColors.push_back(rgb_color());
			ctx->fillColorValid.push_back(false);
			ctx->currentPatternType.clear();
		}
		else if (strcmp(name, "patternFill") == 0)
		{
			for (int i = 0; atts[i]; i += 2)
				if (strcmp(atts[i], "patternType") == 0)
					ctx->currentPatternType = atts[i + 1];
		}
		// Per un riempimento a tinta unita (patternType="solid") e'
		// fgColor a determinare il colore visibile in Excel, non
		// bgColor (usato invece come sfondo di un pattern tratteggiato/
		// zebrato con patternType diverso da "solid") -- altra
		// stranezza nota del formato.
		else if (strcmp(name, "fgColor") == 0 && ctx->currentPatternType == "solid"
			&& !ctx->fillColors.empty())
		{
			rgb_color c;
			if (ResolveColorAttrs(atts, *ctx->theme, &c))
			{
				ctx->fillColors.back() = c;
				ctx->fillColorValid.back() = true;
			}
		}
	}
	else if (ctx->section == kStylesFonts)
	{
		if (strcmp(name, "font") == 0)
		{
			ctx->fontColors.push_back(rgb_color());
			ctx->fontColorValid.push_back(false);
			ctx->fontBold.push_back(false);
			ctx->fontItalic.push_back(false);
			ctx->fontUnderline.push_back(false);
			ctx->fontSize.push_back(-1);
		}
		else if (strcmp(name, "color") == 0 && !ctx->fontColors.empty())
		{
			rgb_color c;
			if (ResolveColorAttrs(atts, *ctx->theme, &c))
			{
				ctx->fontColors.back() = c;
				ctx->fontColorValid.back() = true;
			}
		}
		// <b/> e <i/> sono booleani "per presenza": nessun attributo
		// vuol dire vero, val="0"/"false" lo nega esplicitamente (raro,
		// ma valido ECMA-376 -- es. per azzerare un grassetto ereditato
		// da uno stile cellStyleXfs, non usato qui perche' questo
		// translator non risolve cellStyleXfs, ma innocuo da gestire).
		else if ((strcmp(name, "b") == 0 || strcmp(name, "i") == 0) && !ctx->fontBold.empty())
		{
			bool value = true;
			for (int i = 0; atts[i]; i += 2)
			{
				if (strcmp(atts[i], "val") == 0)
				{
					value = strcmp(atts[i + 1], "0") != 0 && strcmp(atts[i + 1], "false") != 0;
					break;
				}
			}
			if (name[0] == 'b')
				ctx->fontBold.back() = value;
			else
				ctx->fontItalic.back() = value;
		}
		else if (strcmp(name, "sz") == 0 && !ctx->fontSize.empty())
		{
			for (int i = 0; atts[i]; i += 2)
				if (strcmp(atts[i], "val") == 0)
					ctx->fontSize.back() = (float)atof(atts[i + 1]);
		}
		// <u/> senza attributi vuol dire "single" (sottolineato
		// semplice, l'unico stile che CellStyle::fUnderline sa
		// rappresentare): val="none" lo nega esplicitamente,
		// "double"/"singleAccounting"/"doubleAccounting" vengono
		// comunque trattati come sottolineato semplice (nessuna
		// distinzione fra stili nel motore, stesso limite dichiarato
		// per lo spessore/colore dei bordi in Fase 11).
		else if (strcmp(name, "u") == 0 && !ctx->fontUnderline.empty())
		{
			bool value = true;
			for (int i = 0; atts[i]; i += 2)
			{
				if (strcmp(atts[i], "val") == 0)
				{
					value = strcmp(atts[i + 1], "none") != 0;
					break;
				}
			}
			ctx->fontUnderline.back() = value;
		}
	}
	else if (ctx->section == kStylesBorders)
	{
		if (strcmp(name, "border") == 0)
			ctx->borders.push_back(BorderSides());
		// <left style="thin">...</left> vs <left/>: la presenza
		// dell'attributo style (con un valore diverso da "none", raro
		// ma valido) e' cio' che significa "questo lato ha un bordo"
		// in ECMA-376 -- non lo spessore o il colore, ininfluenti dato
		// il significato "booleano per lato" gia' definito in Fase 11.
		else if (!ctx->borders.empty() && (strcmp(name, "left") == 0
			|| strcmp(name, "right") == 0 || strcmp(name, "top") == 0
			|| strcmp(name, "bottom") == 0))
		{
			bool hasStyle = false;
			for (int i = 0; atts[i]; i += 2)
			{
				if (strcmp(atts[i], "style") == 0 && strcmp(atts[i + 1], "none") != 0)
				{
					hasStyle = true;
					break;
				}
			}
			BorderSides& sides = ctx->borders.back();
			if (name[0] == 'l') sides.left = hasStyle;
			else if (name[0] == 'r') sides.right = hasStyle;
			else if (name[0] == 't') sides.top = hasStyle;
			else sides.bottom = hasStyle;
		}
	}
	else if (ctx->section == kStylesCellXfs)
	{
		if (strcmp(name, "xf") == 0)
		{
			XfInfo xf;
			xf.fontId = 0;
			xf.fillId = 0;
			xf.numFmtId = 0;
			xf.borderId = 0;
			xf.alignment = eAlignGeneral;
			xf.wrapText = false;
			for (int i = 0; atts[i]; i += 2)
			{
				if (strcmp(atts[i], "fontId") == 0)
					xf.fontId = atoi(atts[i + 1]);
				else if (strcmp(atts[i], "fillId") == 0)
					xf.fillId = atoi(atts[i + 1]);
				else if (strcmp(atts[i], "numFmtId") == 0)
					xf.numFmtId = atoi(atts[i + 1]);
				else if (strcmp(atts[i], "borderId") == 0)
					xf.borderId = atoi(atts[i + 1]);
			}
			ctx->cellXfs.push_back(xf);
		}
		// <alignment horizontal="center"/> e' un figlio di <xf>, non un
		// suo attributo: arriva in un evento separato subito dopo lo
		// start di <xf> (mai prima, ECMA-376 lo mette sempre come
		// ultimo figlio), quindi si scrive sempre sull'ultimo elemento
		// appena aggiunto a cellXfs.
		else if (strcmp(name, "alignment") == 0 && !ctx->cellXfs.empty())
		{
			for (int i = 0; atts[i]; i += 2)
			{
				if (strcmp(atts[i], "horizontal") == 0)
					ctx->cellXfs.back().alignment = ResolveHorizontalAlignment(atts[i + 1]);
				// wrapText="1" (booleano XLSX: "1"/"true" = vero,
				// "0"/"false"/assente = falso -- qui basta escludere
				// "0" dato che l'attributo non compare affatto quando
				// e' falso).
				else if (strcmp(atts[i], "wrapText") == 0)
					ctx->cellXfs.back().wrapText = strcmp(atts[i + 1], "0") != 0;
			}
		}
	}
}

static void XMLCALL StylesEnd(void* userData, const char* name)
{
	StylesContext* ctx = (StylesContext*)userData;
	if (strcmp(name, "numFmts") == 0 || strcmp(name, "fills") == 0
		|| strcmp(name, "fonts") == 0 || strcmp(name, "borders") == 0
		|| strcmp(name, "cellXfs") == 0)
		ctx->section = kStylesNone;
}

// "styles.xml" e' opzionale (un pacchetto senza stili espliciti lascia
// "out" vuoto: nessun colore applicato, non un errore).
static void ParseStyles(const std::vector<unsigned char>& xml, const XlsxTheme& theme,
	std::vector<ResolvedStyle>* out)
{
	out->clear();
	if (xml.empty())
		return;

	StylesContext ctx;
	ctx.theme = &theme;
	ctx.section = kStylesNone;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, StylesStart, StylesEnd);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK)
		return;

	// Famiglia e dimensione predefinite per grassetto/corsivo (sotto):
	// stesso ripiego di MainWindow::GetCellFontInfo per una cella senza
	// font esplicito, il nome del font XLSX originale (es. "Calibri")
	// non e' quasi mai installato su Haiku e questo progetto non prova
	// a farne il match, solo lo STILE (Bold/Italic) viene importato.
	font_family defaultFamily;
	font_style defaultStyle;
	be_plain_font->GetFamilyAndStyle(&defaultFamily, &defaultStyle);
	float defaultSize = be_plain_font->Size();

	// Riserva la voce "Regular" per prima: se questo processo non ha
	// ancora mai chiamato GetFontID, il PRIMO indice mai assegnato e'
	// 0 -- lo stesso valore che CellStyle usa come sentinella "nessun
	// font esplicito" (memset a zero nel costruttore, vedi CellStyle.cpp).
	// Se un font grassetto/corsivo fosse il primo mai registrato invece
	// di Regular, finirebbe per caso proprio all'indice 0 e verrebbe
	// scambiato per "nessuno stile", sparendo silenziosamente (bug
	// reale scoperto scrivendo il test: la sezione font risultava con
	// una cella in meno del previsto, mancava esattamente la prima
	// verificata). Il risultato non serve, l'unico scopo e' occupare
	// un indice con un font che NON e' ne' grassetto ne' corsivo prima
	// di registrarne uno che lo e' -- se un font Regular esiste gia'
	// in tabella (uso normale dell'app, non headless), GetFontID lo
	// trova e basta, innocuo.
	gFontSizeTable.GetFontID(defaultFamily, defaultStyle, defaultSize);

	out->resize(ctx.cellXfs.size());
	for (size_t i = 0; i < ctx.cellXfs.size(); i++)
	{
		int fontId = ctx.cellXfs[i].fontId;
		int fillId = ctx.cellXfs[i].fillId;

		ResolvedStyle rs;
		rs.hasBg = fillId >= 0 && (size_t)fillId < ctx.fillColorValid.size()
			&& ctx.fillColorValid[fillId];
		if (rs.hasBg)
			rs.bg = ctx.fillColors[fillId];
		rs.hasFg = fontId >= 0 && (size_t)fontId < ctx.fontColorValid.size()
			&& ctx.fontColorValid[fontId];
		if (rs.hasFg)
			rs.fg = ctx.fontColors[fontId];
		rs.hasFormat = ResolveNumberFormat(ctx.cellXfs[i].numFmtId, ctx.numFmts, &rs.format);
		rs.isDateFormat = IsDateNumFmt(ctx.cellXfs[i].numFmtId, ctx.numFmts);

		bool bold = fontId >= 0 && (size_t)fontId < ctx.fontBold.size() && ctx.fontBold[fontId];
		bool italic = fontId >= 0 && (size_t)fontId < ctx.fontItalic.size() && ctx.fontItalic[fontId];
		rs.hasFontStyle = bold || italic;
		if (rs.hasFontStyle)
		{
			const char* styleStr = (bold && italic) ? "Bold Italic"
				: bold ? "Bold" : "Italic";
			float size = defaultSize;
			if (fontId >= 0 && (size_t)fontId < ctx.fontSize.size() && ctx.fontSize[fontId] > 0)
				size = ctx.fontSize[fontId];
			rs.fontID = (int)gFontSizeTable.GetFontID(defaultFamily, styleStr, size);
		}

		rs.alignment = ctx.cellXfs[i].alignment;
		rs.hasAlignment = rs.alignment != eAlignGeneral;

		int borderId = ctx.cellXfs[i].borderId;
		if (borderId >= 0 && (size_t)borderId < ctx.borders.size())
		{
			const BorderSides& sides = ctx.borders[borderId];
			rs.borderT = sides.top ? 1 : 0;
			rs.borderL = sides.left ? 1 : 0;
			rs.borderB = sides.bottom ? 1 : 0;
			rs.borderR = sides.right ? 1 : 0;
			rs.hasBorders = sides.top || sides.left || sides.bottom || sides.right;
		}
		else
			rs.hasBorders = false;

		rs.underline = fontId >= 0 && (size_t)fontId < ctx.fontUnderline.size()
			&& ctx.fontUnderline[fontId];
		rs.wrapText = ctx.cellXfs[i].wrapText;

		(*out)[i] = rs;
	}
}

// --- Parsing di <dxfs> in xl/styles.xml (formati differenziali, per la
// formattazione condizionale sotto) ---------------------------------
//
// Un dxf ("differential format") descrive solo le DIFFERENZE da
// applicare quando una regola di formattazione condizionale scatta,
// non uno stile completo come <cellXfs>: <dxf><font><color .../></font>
// <fill><patternFill><bgColor .../></patternFill></fill></dxf>. Nota
// la stranezza opposta a <fills> sopra: qui il colore di sfondo
// visibile e' in bgColor, non fgColor (i dxf non hanno patternType,
// sono impliciti "solid").

struct DxfInfo {
	bool hasBg;
	rgb_color bg;
	bool hasFg;
	rgb_color fg;
};

struct DxfsContext {
	const XlsxTheme* theme;
	std::vector<DxfInfo> dxfs;
	bool inFont;
	bool inFill;
};

static void XMLCALL DxfsStart(void* userData, const char* name, const char** atts)
{
	DxfsContext* ctx = (DxfsContext*)userData;

	if (strcmp(name, "dxf") == 0)
	{
		DxfInfo info;
		info.hasBg = false;
		info.hasFg = false;
		ctx->dxfs.push_back(info);
	}
	else if (strcmp(name, "font") == 0)
		ctx->inFont = true;
	else if (strcmp(name, "fill") == 0)
		ctx->inFill = true;
	else if (ctx->inFont && strcmp(name, "color") == 0 && !ctx->dxfs.empty())
	{
		rgb_color c;
		if (ResolveColorAttrs(atts, *ctx->theme, &c))
		{
			ctx->dxfs.back().fg = c;
			ctx->dxfs.back().hasFg = true;
		}
	}
	else if (ctx->inFill && strcmp(name, "bgColor") == 0 && !ctx->dxfs.empty())
	{
		rgb_color c;
		if (ResolveColorAttrs(atts, *ctx->theme, &c))
		{
			ctx->dxfs.back().bg = c;
			ctx->dxfs.back().hasBg = true;
		}
	}
}

static void XMLCALL DxfsEnd(void* userData, const char* name)
{
	DxfsContext* ctx = (DxfsContext*)userData;
	if (strcmp(name, "font") == 0)
		ctx->inFont = false;
	else if (strcmp(name, "fill") == 0)
		ctx->inFill = false;
}

static void ParseDxfs(const std::vector<unsigned char>& xml, const XlsxTheme& theme,
	std::vector<DxfInfo>* out)
{
	out->clear();
	if (xml.empty())
		return;

	DxfsContext ctx;
	ctx.theme = &theme;
	ctx.inFont = false;
	ctx.inFill = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, DxfsStart, DxfsEnd);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	if (status == XML_STATUS_OK)
		*out = ctx.dxfs;
}

// --- Parsing di xl/worksheets/sheetN.xml --------------------------------
//
// Struttura minima gestita:
//   <sheetData>
//     <row r="1">
//       <c r="A1" t="s"><v>0</v></c>              -- stringa condivisa (indice)
//       <c r="B1"><v>10</v></c>                    -- numero
//       <c r="C1" t="str"><f>A1&amp;B1</f><v>...</v></c>  -- formula (risultato stringa)
//       <c r="D1"><f>A1+B1</f><v>11</v></c>        -- formula (risultato numerico)
//       <c r="E1" t="inlineStr"><is><t>ciao</t></is></c> -- stringa inline (scritta dal nostro export)
//     </row>
//   </sheetData>
//
// Le formule vengono importate come testo (con "=" davanti) tramite
// TryToParseString, che le ricalcola con il motore -- non si usa il
// valore gia' calcolato da Excel/LibreOffice (<v> nella cella con
// <f>), per verificare che il nostro motore produca lo stesso
// risultato in modo indipendente.

// Formattazione condizionale (Fase 12): una regola grezza cosi' come
// appare nel foglio (<conditionalFormatting sqref="B8 D8"><cfRule
// type="cellIs" dxfId="22" operator="equal"><formula>"..."</formula>
// </cfRule></conditionalFormatting>), valutata contro i valori gia'
// importati SOLO dopo che l'intero foglio e' stato letto (vedi
// ApplyConditionalFormatting sotto) -- non un motore di regole vive,
// il colore risultante e' congelato all'importazione.
struct CondFormatRule {
	std::vector<range> ranges; // da sqref, uno o piu' intervalli/celle separati da spazio
	std::string type;          // "cellIs", "duplicateValues", ... (solo questi due gestiti)
	std::string operatorAttr;  // solo per "cellIs": "equal" e' l'unico gestito
	std::string formula;       // solo per "cellIs"
	int dxfId;
};

struct SheetContext {
	CContainer* doc;
	const std::vector<std::string>* sharedStrings;
	std::vector<std::pair<int, float> >* colWidths; // opzionale (NULL = non raccolte)
	std::vector<std::pair<int, float> >* rowHeights; // opzionale (NULL = non raccolte)
	bool* showGrid; // opzionale (NULL = non raccolto)
	bool* hasTabColor; // opzionale (NULL = non raccolto)
	rgb_color* tabColor; // valido solo se *hasTabColor diventa true
	std::vector<int>* hiddenRows; // opzionale (NULL = non raccolte)
	bool* hasAutoFilter; // opzionale (NULL = non raccolto)
	range* autoFilterRange; // valido solo se *hasAutoFilter diventa true
	const std::vector<ResolvedStyle>* styles; // opzionale (NULL = non applica colori)
	std::vector<CondFormatRule>* condRules; // opzionale (NULL = non raccolte)
	bool date1904; // Fase 12: epoca del sistema data, da <workbookPr>

	std::string cellRef;
	std::string cellType; // valore dell'attributo t="..." (puo' essere vuoto)
	std::string value;    // testo dentro <v>
	std::string formula;  // testo dentro <f>
	int cellStyleIndex;   // valore di s="..." sulla cella corrente, -1 se assente
	bool inValue;
	bool inFormula;

	// Stato per <conditionalFormatting>/<cfRule>/<formula> (solo se
	// condRules non e' NULL).
	std::vector<range> currentSqref;
	bool inCfRule;
	CondFormatRule currentRule;
	bool inCondFormula;
	std::string condFormula;
};

// Un singolo token di sqref ("B8" o "B6:B8") in un "range" del
// motore -- a differenza di ParseMergeCellRef, qui un riferimento a
// una sola cella (senza ':') e' il caso comune, non un errore.
static bool ParseSqrefToken(const std::string& token, range* out)
{
	int col1, row1;
	if (!CellRefToColRow(token.substr(0, token.find(':')), col1, row1))
		return false;

	size_t colon = token.find(':');
	if (colon == std::string::npos)
	{
		out->Set(col1, row1, col1, row1);
		return true;
	}

	int col2, row2;
	if (!CellRefToColRow(token.substr(colon + 1), col2, row2))
		return false;

	out->Set(std::min(col1, col2), std::min(row1, row2),
		std::max(col1, col2), std::max(row1, row2));
	return true;
}

// sqref e' un elenco di riferimenti separati da spazio ("B8 D8" o
// "D8 B6:B8") -- ognuno un token da passare a ParseSqrefToken.
static void ParseSqref(const std::string& sqref, std::vector<range>* out)
{
	out->clear();
	size_t start = 0;
	while (start < sqref.size())
	{
		size_t end = sqref.find(' ', start);
		if (end == std::string::npos)
			end = sqref.size();
		if (end > start)
		{
			range r;
			if (ParseSqrefToken(sqref.substr(start, end - start), &r))
				out->push_back(r);
		}
		start = end + 1;
	}
}

// La larghezza in <col width="..."> e' nell'unita' di misura di Excel
// ("numero di caratteri '0' del carattere piu' largo del font
// predefinito", non pixel): formula esatta dallo standard ECMA-376
// 18.3.1.13, con MDW (Maximum Digit Width) = 7 pixel, quello del font
// predefinito piu' comune (Calibri 11 a 96 DPI) -- lo stesso di questo
// file di prova. Bug reale segnalato dall'utente confrontando un file
// XLSX con lo stesso aperto in Excel vero: la vecchia approssimazione
// (charWidth * 7 + 5, un margine fisso largamente usato da importatori
// piu' semplici) sovrastimava sistematicamente ogni colonna di circa
// 5 pixel -- poco per una colonna sola, ma cumulativo su piu' colonne
// consecutive (misurato ~16px di troppo su tre colonne in un file
// reale), spostando verso destra qualunque oggetto ancorato a una
// colonna successiva (immagini incorporate, Fase 12).
static float ExcelColWidthToPixels(double charWidth)
{
	const double kMDW = 7.0;
	double n = std::floor(128.0 / kMDW);
	return (float)std::floor(((256.0 * charWidth + n) / 256.0) * kMDW);
}

static void XMLCALL SheetStart(void* userData, const char* name, const char** atts)
{
	SheetContext* ctx = (SheetContext*)userData;

	// <sheetView showGridLines="0" .../>, dentro <sheetViews> prima di
	// <sheetData> -- l'attributo e' assente quando la griglia e'
	// semplicemente visibile (il default di Excel, "1" implicito, mai
	// scritto esplicitamente in quel caso): solo "0" esplicito la
	// nasconde. Bug reale segnalato dall'utente confrontando un file
	// con Excel: un foglio con la griglia nascosta appositamente
	// dall'autore (un look pulito da documento ufficiale) veniva
	// comunque importato con la griglia visibile, perche' prima questo
	// translator non leggeva affatto l'attributo.
	if (strcmp(name, "sheetView") == 0 && ctx->showGrid)
	{
		bool show = true;
		for (int i = 0; atts[i]; i += 2)
			if (strcmp(atts[i], "showGridLines") == 0)
				show = atoi(atts[i + 1]) != 0;
		*ctx->showGrid = show;
	}
	// <sheetPr><tabColor rgb="FF00B050"/></sheetPr>, prima di
	// <sheetViews>/<cols>/<sheetData> -- il colore scelto dall'utente
	// per la linguetta del foglio (HexToColor, definita sopra, gia'
	// usata per gli stessi colori "rgb=" di font/sfondo cella). Assente
	// del tutto se il foglio non ha una linguetta colorata (il caso
	// comune): *hasTabColor resta false, il valore di partenza passato
	// dal chiamante.
	else if (strcmp(name, "tabColor") == 0 && ctx->hasTabColor)
	{
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "rgb") == 0 && HexToColor(atts[i + 1], ctx->tabColor))
				*ctx->hasTabColor = true;
		}
	}
	// <row r="1" ht="48.75" customHeight="1" hidden="1">...</row>, dentro
	// <sheetData>, un fratello di <c> (una entry per riga, non per
	// cella) -- stesso principio di <col> per le larghezze: "ht" e' in
	// punti (1/72 di pollice, l'unita' tipografica di Excel per le
	// altezze), va in pixel a 96 DPI con lo stesso fattore 4/3 gia'
	// usato altrove in questo progetto per kRowHeight (15pt * 4/3 =
	// 20px, l'altezza di riga predefinita di SheetView). "customHeight"
	// distingue un'altezza scelta dall'utente da un semplice suggerimento
	// di autofit di Excel, stesso principio di "customWidth" per <col>.
	// "hidden" e' lo STESSO attributo sia per un nascondimento manuale
	// (tasto destro sull'intestazione > Nascondi) sia per una riga
	// esclusa da un AutoFilter attivo (vedi <autoFilter> sotto) -- Excel
	// non li distingue nel file, quindi nemmeno questo import: importa
	// il risultato (riga nascosta), non il motivo.
	if (strcmp(name, "row") == 0 && (ctx->rowHeights || ctx->hiddenRows))
	{
		int row = 0;
		bool hasHeight = false, customHeight = false, hidden = false;
		double heightPt = 0;
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "r") == 0)
				row = atoi(atts[i + 1]);
			else if (strcmp(atts[i], "ht") == 0)
			{
				heightPt = atof(atts[i + 1]);
				hasHeight = true;
			}
			else if (strcmp(atts[i], "customHeight") == 0)
				customHeight = atoi(atts[i + 1]) != 0;
			else if (strcmp(atts[i], "hidden") == 0)
				hidden = atoi(atts[i + 1]) != 0;
		}
		if (row > 0 && hasHeight && customHeight && ctx->rowHeights)
			ctx->rowHeights->push_back(std::make_pair(row, (float)(heightPt * 4.0 / 3.0)));
		if (row > 0 && hidden && ctx->hiddenRows)
			ctx->hiddenRows->push_back(row);
	}
	// <autoFilter ref="A8:N8"/>, dentro <worksheet> (fratello di
	// <sheetData>, prima o dopo a seconda dello strumento che ha
	// scritto il file): la riga di intestazione + intervallo di
	// colonne su cui Excel disegna le frecce a discesa -- riusa
	// ParseMergeCellRef (definita sopra per <mergeCell ref="...">),
	// stesso formato "A1:B2". Le eventuali condizioni gia' applicate
	// (<filterColumn><filters>...) non si leggono qui: il risultato
	// (quali righe sono nascoste) arriva gia' da "hidden" su <row>
	// sopra, che basta per mostrare il foglio come in Excel -- limite
	// dichiarato, vedi ROADMAP.md.
	else if (strcmp(name, "autoFilter") == 0 && ctx->hasAutoFilter)
	{
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "ref") == 0 && ParseMergeCellRef(atts[i + 1], ctx->autoFilterRange))
				*ctx->hasAutoFilter = true;
		}
	}
	else if (strcmp(name, "c") == 0)
	{
		ctx->cellRef.clear();
		ctx->cellType.clear();
		ctx->value.clear();
		ctx->formula.clear();
		ctx->cellStyleIndex = -1;
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "r") == 0)
				ctx->cellRef = atts[i + 1];
			else if (strcmp(atts[i], "t") == 0)
				ctx->cellType = atts[i + 1];
			else if (strcmp(atts[i], "s") == 0)
				ctx->cellStyleIndex = atoi(atts[i + 1]);
		}
	}
	// <mergeCells><mergeCell ref="A1:C1"/>...</mergeCells>, un fratello
	// di <sheetData> (non un figlio): l'ordine nel file non e'
	// garantito rispetto alle celle stesse, ma qui non serve, si scrive
	// direttamente su "doc" (CContainer::AddMergedRange) indipendente
	// dall'ordine di lettura.
	else if (strcmp(name, "mergeCell") == 0 && ctx->doc)
	{
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "ref") == 0)
			{
				range r;
				if (ParseMergeCellRef(atts[i + 1], &r))
					ctx->doc->AddMergedRange(r);
				break;
			}
		}
	}
	// Formattazione condizionale (Fase 12): <conditionalFormatting
	// sqref="B8 D8"> puo' contenere piu' <cfRule>, ognuna diventa una
	// CondFormatRule a parte ma tutte condividono lo stesso sqref
	// (catturato una volta sola qui, riusato per ogni <cfRule> dentro).
	else if (strcmp(name, "conditionalFormatting") == 0 && ctx->condRules)
	{
		ctx->currentSqref.clear();
		for (int i = 0; atts[i]; i += 2)
			if (strcmp(atts[i], "sqref") == 0)
				ParseSqref(atts[i + 1], &ctx->currentSqref);
	}
	else if (strcmp(name, "cfRule") == 0 && ctx->condRules)
	{
		ctx->inCfRule = true;
		ctx->currentRule = CondFormatRule();
		ctx->currentRule.ranges = ctx->currentSqref;
		ctx->currentRule.dxfId = -1;
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "type") == 0)
				ctx->currentRule.type = atts[i + 1];
			else if (strcmp(atts[i], "operator") == 0)
				ctx->currentRule.operatorAttr = atts[i + 1];
			else if (strcmp(atts[i], "dxfId") == 0)
				ctx->currentRule.dxfId = atoi(atts[i + 1]);
		}
	}
	else if (strcmp(name, "formula") == 0 && ctx->inCfRule)
	{
		ctx->inCondFormula = true;
		ctx->condFormula.clear();
	}
	else if (strcmp(name, "v") == 0)
		ctx->inValue = true;
	else if (strcmp(name, "f") == 0)
		ctx->inFormula = true;
	// Le stringhe inline (t="inlineStr", scritte dal nostro export
	// invece di usare una tabella di stringhe condivise) mettono il
	// testo dentro <is><t>...</t></is> anziche' <v>: si riusa lo
	// stesso campo "value" e lo stesso fallback in SheetEnd (nessun
	// "t" nella cella diverso da "s" finisce li').
	else if (strcmp(name, "t") == 0)
		ctx->inValue = true;
	// <col min="1" max="3" width="12.5" customWidth="1"/>, prima di
	// <sheetData>: min/max sono un intervallo di colonne 1-based
	// INCLUSIVO (spesso min==max, ma non sempre -- una sola voce puo'
	// coprire piu' colonne contigue con la stessa larghezza). Ignora
	// le voci senza width esplicito (bestFit da solo, senza
	// customWidth, e' solo un suggerimento per l'autofit di Excel, non
	// una larghezza scelta dall'utente).
	else if (strcmp(name, "col") == 0 && (ctx->colWidths || (ctx->styles && ctx->doc)))
	{
		int min = 0, max = 0;
		bool hasWidth = false;
		double width = 0;
		int styleIndex = -1;
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "min") == 0)
				min = atoi(atts[i + 1]);
			else if (strcmp(atts[i], "max") == 0)
				max = atoi(atts[i + 1]);
			else if (strcmp(atts[i], "width") == 0)
			{
				width = atof(atts[i + 1]);
				hasWidth = true;
			}
			else if (strcmp(atts[i], "style") == 0)
				styleIndex = atoi(atts[i + 1]);
		}
		// max puo' arrivare a 16384 (il limite reale di Excel) per
		// indicare "tutte le colonne restanti hanno questa larghezza":
		// tagliato a kColCount, il limite di questo motore, per non
		// riempire colWidths di migliaia di voci che verrebbero comunque
		// scartate piu' avanti (SheetView::SetColumnWidths ignora le
		// colonne oltre kColCount).
		if (min > 0 && max >= min)
		{
			int clampedMax = std::min(max, (int)kColCount);

			if (hasWidth && ctx->colWidths)
			{
				float pixels = ExcelColWidthToPixels(width);
				for (int col = min; col <= clampedMax; col++)
					ctx->colWidths->push_back(std::make_pair(col, pixels));
			}

			// style="..." su <col> imposta il colore predefinito
			// dell'intera colonna (CContainer::SetColumnStyle, gia'
			// usato da GetCellStyleNr come fallback per le celle senza
			// una voce propria): le celle vuote di una colonna colorata
			// mostrano cosi' lo sfondo giusto anche senza un contenuto.
			if (ctx->styles && ctx->doc && styleIndex >= 0
				&& (size_t)styleIndex < ctx->styles->size())
			{
				const ResolvedStyle& rs = (*ctx->styles)[styleIndex];
				if (rs.hasBg || rs.hasFg || rs.hasFormat || rs.hasFontStyle || rs.hasAlignment
					|| rs.hasBorders || rs.underline || rs.wrapText)
				{
					for (int col = min; col <= clampedMax; col++)
					{
						CellStyle cs;
						ctx->doc->GetColumnStyle(col, cs);
						if (rs.hasBg) cs.fLowColor = rs.bg;
						if (rs.hasFg) cs.fHighColor = rs.fg;
						if (rs.hasFormat) cs.fFormat = rs.format;
						if (rs.hasFontStyle) cs.fFont = rs.fontID;
						if (rs.hasAlignment) cs.fAlignment = rs.alignment;
						if (rs.hasBorders)
						{
							cs.fTBorderColor = rs.borderT;
							cs.fLBorderColor = rs.borderL;
							cs.fBBorderColor = rs.borderB;
							cs.fRBorderColor = rs.borderR;
						}
						if (rs.underline) cs.fUnderline = true;
						if (rs.wrapText) cs.fWrapText = true;
						ctx->doc->SetColumnStyle(col, cs);
					}
				}
			}
		}
	}
}

static void XMLCALL SheetEnd(void* userData, const char* name)
{
	SheetContext* ctx = (SheetContext*)userData;

	if (strcmp(name, "v") == 0)
		ctx->inValue = false;
	else if (strcmp(name, "f") == 0)
		ctx->inFormula = false;
	else if (strcmp(name, "formula") == 0 && ctx->inCondFormula)
	{
		ctx->inCondFormula = false;
		ctx->currentRule.formula = ctx->condFormula;
	}
	else if (strcmp(name, "cfRule") == 0 && ctx->inCfRule)
	{
		ctx->inCfRule = false;
		ctx->condRules->push_back(ctx->currentRule);
	}
	else if (strcmp(name, "t") == 0)
		ctx->inValue = false;
	else if (strcmp(name, "c") == 0)
	{
		if (ctx->cellRef.empty())
			return;

		int col, row;
		if (!CellRefToColRow(ctx->cellRef, col, row))
			return;

		cell loc(col, row);

		std::string text;
		if (!ctx->formula.empty())
			text = "=" + ctx->formula;
		else if (ctx->cellType == "s")
		{
			int idx = atoi(ctx->value.c_str());
			if (ctx->sharedStrings && idx >= 0
				&& (size_t)idx < ctx->sharedStrings->size())
				text = (*ctx->sharedStrings)[idx];
		}
		else
			text = ctx->value;

		// Formati data/ora (Fase 12): solo per una cella NUMERICA vera
		// (niente t="...", niente formula -- una data e' sempre un
		// numero grezzo con uno stile che dice "mostrami come data",
		// mai una stringa) il cui stile risolto e' un formato data.
		// Le formule restano sempre formule ricalcolate (stesso
		// principio gia' documentato in cima al file), indipendenti
		// dal loro numFmt.
		bool isDateCell = ctx->formula.empty() && ctx->cellType.empty()
			&& ctx->styles && ctx->cellStyleIndex >= 0
			&& (size_t)ctx->cellStyleIndex < ctx->styles->size()
			&& (*ctx->styles)[ctx->cellStyleIndex].isDateFormat;

		if (!text.empty())
		{
			try
			{
				char* end = NULL;
				double serial = isDateCell ? strtod(text.c_str(), &end) : 0;
				if (isDateCell && end != text.c_str() && *end == 0)
				{
					Value v(ExcelSerialToTime(serial, ctx->date1904));
					ctx->doc->NewCell(loc, v, NULL);
				}
				else if (!ctx->formula.empty())
				{
					// decSep='.'/listSep=',' ESPLICITI, non i valori
					// predefiniti (0 -> gDecimalPoint/gListSeparator,
					// globali legati alle preferenze locali dell'utente,
					// ';' di default per l'Italia -- vedi App.cpp): il
					// testo di <f> in un file XLSX e' SEMPRE nel formato
					// canonico ECMA-376, indipendente dalla lingua/locale
					// con cui e' stato scritto in Excel (che mostra
					// "=SE(A1>5;100;200)" con SE/punto e virgola in
					// italiano, ma salva sempre "=IF(A1>5,100,200)" nel
					// file). Bug reale scoperto su un file reale: OGNI
					// formula con piu' di un argomento (IF, VLOOKUP,
					// IFERROR, ecc.) falliva l'analisi grammaticale con
					// gListSeparator=';' e ripiegava sul testo grezzo
					// della formula invece di calcolarla.
					TryToParseString(text.c_str(), loc, ctx->doc, false, '.', ',');
				}
				else
				{
					// Valore diretto, MAI una formula (XLSX lo dice
					// esplicitamente: nessun <f> su questa cella) -- non
					// passa MAI per TryToParseString/Parse(), che tenta di
					// interpretare come espressione QUALSIASI stringa
					// anche senza "=" iniziale (comportamento storico di
					// Sum-It, utile quando l'utente digita "A1+A2" a
					// mano). Bug reale scoperto verificando XLOOKUP su una
					// tabella strutturata vera: un codice come "P-EL-b" si
					// tokenizza come "P meno EL meno b" (tre nomi non
					// definiti concatenati da un meno), un'espressione
					// sintatticamente valida -- Parse() la accettava e la
					// trasformava in una formula viva che calcola NaN,
					// corrompendo silenziosamente il testo della colonna
					// Codice all'importazione (XLOOKUP non trovava piu'
					// nessuna corrispondenza).
					Value v;
					if (ctx->cellType == "s" || ctx->cellType == "inlineStr" || ctx->cellType == "e")
						v = text.c_str();
					else
					{
						char* numEnd = NULL;
						double num = strtod(text.c_str(), &numEnd);
						if (numEnd != text.c_str() && *numEnd == 0)
							v = num;
						else
							v = text.c_str();
					}
					ctx->doc->NewCell(loc, v, NULL);
				}
			}
			catch (...)
			{
				// Una singola cella non importabile non deve far fallire
				// l'intero documento: viene semplicemente saltata.
			}
		}

		// Il colore va applicato DOPO TryToParseString sopra, mai prima:
		// CContainer::NewCell (chiamata anche da TryToParseString per
		// scrivere il contenuto) sovrascrive SEMPRE l'intero CellData
		// della cella, stile compreso, azzerandolo al predefinito --
		// applicarlo prima verrebbe quindi perso non appena la cella
		// riceve un contenuto. Si applica comunque ANCHE a una cella
		// senza contenuto (es. un'intestazione colorata ma vuota, o
		// uno sfondo decorativo), per questo fuori dal blocco
		// "!text.empty()" sopra: qui SetCellStyle crea la cella da
		// zero (nessun NewCell precedente su cui inciampare). Bug
		// reale scoperto scrivendo tests/test_xlsx_translator.cpp: le
		// celle con un contenuto risultavano sempre al colore
		// predefinito nonostante s="..." fosse letto e risolto
		// correttamente.
		if (ctx->styles && ctx->cellStyleIndex >= 0
			&& (size_t)ctx->cellStyleIndex < ctx->styles->size())
		{
			const ResolvedStyle& rs = (*ctx->styles)[ctx->cellStyleIndex];
			if (rs.hasBg || rs.hasFg || rs.hasFormat || rs.hasFontStyle || rs.hasAlignment
				|| rs.hasBorders || rs.underline || rs.wrapText)
			{
				CellStyle cs;
				ctx->doc->GetCellStyle(loc, cs);
				if (rs.hasBg) cs.fLowColor = rs.bg;
				if (rs.hasFg) cs.fHighColor = rs.fg;
				if (rs.hasFormat) cs.fFormat = rs.format;
				if (rs.hasFontStyle) cs.fFont = rs.fontID;
				if (rs.hasAlignment) cs.fAlignment = rs.alignment;
				if (rs.hasBorders)
				{
					cs.fTBorderColor = rs.borderT;
					cs.fLBorderColor = rs.borderL;
					cs.fBBorderColor = rs.borderB;
					cs.fRBorderColor = rs.borderR;
				}
				if (rs.underline) cs.fUnderline = true;
				if (rs.wrapText) cs.fWrapText = true;
				ctx->doc->SetCellStyle(loc, cs);
			}
		}
	}
}

static void XMLCALL SheetChars(void* userData, const char* s, int len)
{
	SheetContext* ctx = (SheetContext*)userData;
	if (ctx->inValue)
		ctx->value.append(s, len);
	else if (ctx->inFormula)
		ctx->formula.append(s, len);
	else if (ctx->inCondFormula)
		ctx->condFormula.append(s, len);
}

static bool ParseSheet(const std::vector<unsigned char>& xml, CContainer* doc,
	const std::vector<std::string>& sharedStrings,
	std::vector<std::pair<int, float> >* colWidths,
	const std::vector<ResolvedStyle>* styles,
	std::vector<CondFormatRule>* condRules = NULL,
	bool date1904 = false,
	std::vector<std::pair<int, float> >* rowHeights = NULL,
	bool* showGrid = NULL,
	bool* hasTabColor = NULL, rgb_color* tabColor = NULL,
	std::vector<int>* hiddenRows = NULL,
	bool* hasAutoFilter = NULL, range* autoFilterRange = NULL)
{
	SheetContext ctx;
	ctx.doc = doc;
	ctx.sharedStrings = &sharedStrings;
	ctx.colWidths = colWidths;
	ctx.rowHeights = rowHeights;
	ctx.showGrid = showGrid;
	ctx.hasTabColor = hasTabColor;
	ctx.tabColor = tabColor;
	ctx.hiddenRows = hiddenRows;
	ctx.hasAutoFilter = hasAutoFilter;
	ctx.autoFilterRange = autoFilterRange;
	ctx.styles = styles;
	ctx.condRules = condRules;
	ctx.date1904 = date1904;
	ctx.cellStyleIndex = -1;
	ctx.inValue = false;
	ctx.inFormula = false;
	ctx.inCfRule = false;
	ctx.inCondFormula = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, SheetStart, SheetEnd);
	XML_SetCharacterDataHandler(parser, SheetChars);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	return status == XML_STATUS_OK;
}

CXlsxTranslator::CXlsxTranslator()
	: BTranslator()
{
}

CXlsxTranslator::~CXlsxTranslator()
{
}

const char* CXlsxTranslator::TranslatorName() const
{
	return "XLSX Translator";
}

const char* CXlsxTranslator::TranslatorInfo() const
{
	return "Importa/esporta fogli di calcolo dal/al formato Excel 2007+ (XLSX) "
		"-- l'esportazione scrive solo i valori calcolati, non le formule";
}

int32 CXlsxTranslator::TranslatorVersion() const
{
	return B_TRANSLATION_MAKE_VERSION(1, 0, 0);
}

const translation_format* CXlsxTranslator::InputFormats(int32* _count) const
{
	*_count = 1;
	return sInputFormats;
}

const translation_format* CXlsxTranslator::OutputFormats(int32* _count) const
{
	*_count = 2;
	return sOutputFormats;
}

// --- Parsing di xl/workbook.xml (elenco fogli) e xl/_rels/
// workbook.xml.rels (nome parte fisica di ciascun foglio) -------------
//
// xl/workbook.xml elenca i fogli nell'ordine delle schede, con nome e
// r:id (es. <sheet name="P-MDO_Manodopera" sheetId="10" r:id="rId9"/>),
// ma NON il nome del file XML che contiene i dati di quel foglio:
// quello sta in xl/_rels/workbook.xml.rels, che fa corrispondere ogni
// r:id al percorso reale (es. rId9 -> worksheets/sheet9.xml) -- i due
// non sono necessariamente nello stesso ordine numerico (dipende da
// come lo strumento che ha generato il file assegna gli ID interni),
// quindi vanno letti entrambi e incrociati.

struct WorkbookSheetInfo {
	std::string name;
	std::string rId;
};

struct WorkbookContext {
	std::vector<WorkbookSheetInfo> sheets;
	bool inSheets;
	bool date1904; // <workbookPr date1904="1"/>: epoca Mac storica (1904-01-01) invece della predefinita (1899-12-30, Fase 12)
};

static void XMLCALL WorkbookStart(void* userData, const char* name, const char** atts)
{
	WorkbookContext* ctx = (WorkbookContext*)userData;
	if (strcmp(name, "sheets") == 0)
		ctx->inSheets = true;
	else if (ctx->inSheets && strcmp(name, "sheet") == 0)
	{
		WorkbookSheetInfo info;
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "name") == 0)
				info.name = atts[i + 1];
			else if (strcmp(atts[i], "r:id") == 0)
				info.rId = atts[i + 1];
		}
		if (!info.rId.empty())
			ctx->sheets.push_back(info);
	}
	else if (strcmp(name, "workbookPr") == 0)
	{
		for (int i = 0; atts[i]; i += 2)
			if (strcmp(atts[i], "date1904") == 0)
				ctx->date1904 = strcmp(atts[i + 1], "0") != 0 && strcmp(atts[i + 1], "false") != 0;
	}
}

static void XMLCALL WorkbookEnd(void* userData, const char* name)
{
	WorkbookContext* ctx = (WorkbookContext*)userData;
	if (strcmp(name, "sheets") == 0)
		ctx->inSheets = false;
}

static bool ParseWorkbookSheetList(const std::vector<unsigned char>& xml,
	std::vector<WorkbookSheetInfo>& out, bool* outDate1904 = NULL)
{
	if (xml.empty())
		return false;

	WorkbookContext ctx;
	ctx.inSheets = false;
	ctx.date1904 = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, WorkbookStart, WorkbookEnd);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK || ctx.sheets.empty())
		return false;

	out = ctx.sheets;
	if (outDate1904)
		*outDate1904 = ctx.date1904;
	return true;
}

static void XMLCALL RelationshipsStart(void* userData, const char* name, const char** atts)
{
	std::map<std::string, std::string>* map = (std::map<std::string, std::string>*)userData;
	if (strcmp(name, "Relationship") != 0)
		return;

	std::string id, target;
	for (int i = 0; atts[i]; i += 2)
	{
		if (strcmp(atts[i], "Id") == 0)
			id = atts[i + 1];
		else if (strcmp(atts[i], "Target") == 0)
			target = atts[i + 1];
	}
	if (!id.empty() && !target.empty())
		(*map)[id] = target;
}

static bool ParseRelationships(const std::vector<unsigned char>& xml,
	std::map<std::string, std::string>& out)
{
	if (xml.empty())
		return false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &out);
	XML_SetElementHandler(parser, RelationshipsStart, NULL);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	return status == XML_STATUS_OK;
}

// --- Parsing di xl/tables/tableN.xml (Fase 12) ---------------------
//
// Una tabella strutturata Excel (xl/tables/tableN.xml, collegata a un
// foglio tramite <tableParts> nel foglio stesso + i _rels del
// foglio): <table ref="A10:D121" ...><tableStyleInfo
// name="TableStyleMedium2" showRowStripes="1" .../></table>. Il
// colore VERO delle bande alternate di uno stile incorporato
// (es. "TableStyleMedium2") non e' nel file: e' un tema grafico
// predefinito di Excel stesso, non salvato nella cartella di lavoro a
// meno che l'utente non abbia creato uno stile personalizzato (raro,
// non gestito qui). Approssimazione dichiarata: banda grigio chiaro
// neutra invece del colore esatto dello stile con nome, applicata
// come normale colore di sfondo per cella (riusa l'infrastruttura
// colori di Fase 7) -- non un vero oggetto tabella "vivo" (niente
// filtro/riga totali ricalcolata).

struct TableInfo {
	range tableRange;
	bool hasRange;
	bool showStripes;
	// Nome della tabella (Fase 14, "Tabella12[Codice]" nelle formule) e
	// nomi di colonna nello stesso ordine di <tableColumns> (sinistra a
	// destra in tableRange) -- vedi CContainer::AddTable/ResolveName in
	// Container.h/.cpp. "name", non "displayName": e' l'attributo che
	// le formule usano davvero (i due di norma coincidono, ma solo
	// "name" e' garantito dallo standard ECMA-376).
	std::string name;
	std::vector<std::string> columnNames;
};

static void XMLCALL TableStart(void* userData, const char* name, const char** atts)
{
	TableInfo* info = (TableInfo*)userData;
	if (strcmp(name, "table") == 0)
	{
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "ref") == 0)
				info->hasRange = ParseMergeCellRef(atts[i + 1], &info->tableRange);
			else if (strcmp(atts[i], "name") == 0)
				info->name = atts[i + 1];
		}
	}
	else if (strcmp(name, "tableColumn") == 0)
	{
		for (int i = 0; atts[i]; i += 2)
			if (strcmp(atts[i], "name") == 0)
				info->columnNames.push_back(atts[i + 1]);
	}
	else if (strcmp(name, "tableStyleInfo") == 0)
	{
		for (int i = 0; atts[i]; i += 2)
			if (strcmp(atts[i], "showRowStripes") == 0)
				info->showStripes = strcmp(atts[i + 1], "0") != 0;
	}
}

static bool ParseTableInfo(const std::vector<unsigned char>& xml, TableInfo* out)
{
	out->hasRange = false;
	out->showStripes = false;
	out->name.clear();
	out->columnNames.clear();
	if (xml.empty())
		return false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, out);
	XML_SetElementHandler(parser, TableStart, NULL);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	return status == XML_STATUS_OK && out->hasRange;
}

// Applica la banda grigio chiaro alle righe dati dispari (la prima
// riga del range e' l'intestazione, esclusa dalla banda) -- solo
// alle celle che non hanno gia' un colore di sfondo esplicito
// dall'importazione dei colori sopra (ParseSheet/SheetEnd), per non
// coprire uno sfondo scelto apposta dall'utente nel file originale.
static void ApplyTableBanding(CContainer* doc, const range& tableRange)
{
	const rgb_color kBandColor = { 242, 242, 242, 255 };
	CellStyle defaultStyle;

	for (int row = tableRange.top + 1; row <= tableRange.bottom; row++)
	{
		// "row - (tableRange.top + 1)" e' l'indice 0-based della riga
		// dati (0 = la prima subito sotto l'intestazione): banda le
		// righe dati dispari in ordine (1a, 3a, 5a...), cioe' indice
		// pari. Contare da tableRange.top (l'intestazione) invece che
		// dalla prima riga dati sfaserebbe la banda di una riga (bug
		// reale scoperto scrivendo il test: bandava la 2a/4a riga dati
		// invece della 1a/3a).
		if ((row - tableRange.top - 1) % 2 != 0)
			continue;

		for (int col = tableRange.left; col <= tableRange.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			doc->GetCellStyle(c, cs);
			if (!ColorsEqual(cs.fLowColor, defaultStyle.fLowColor))
				continue;
			cs.fLowColor = kBandColor;
			doc->SetCellStyle(c, cs);
		}
	}
}

// Registra una tabella strutturata per i riferimenti nelle formule
// (Fase 14, "Tabella12[Codice]") -- vedi CContainer::AddTable/
// ResolveName. tableRange (da TableInfo::ref) include la riga di
// intestazione, CTableDef::dataRange no: un riferimento a colonna
// indica solo i dati, mai il nome della colonna stesso. Se il numero
// di colonne non torna con l'intervallo (file corrotto/non standard),
// la tabella semplicemente non si registra -- un riferimento a un nome
// mai registrato resta comunque una formula viva (gNameNan), stesso
// principio di un nome di intervallo indefinito, non un errore fatale
// per l'intera importazione.
static void RegisterTable(CContainer* doc, const TableInfo& info)
{
	if (info.name.empty() || info.columnNames.empty())
		return;
	if ((int)info.columnNames.size() != info.tableRange.right - info.tableRange.left + 1)
		return;
	if (info.tableRange.top >= info.tableRange.bottom)
		return; // nessuna riga dati sotto l'intestazione, solo l'intestazione stessa

	CTableDef def;
	def.dataRange = info.tableRange;
	def.dataRange.top += 1; // esclude la riga di intestazione
	def.columnNames = info.columnNames;
	doc->AddTable(info.name, def);
}

// Toglie le virgolette che racchiudono un letterale stringa in una
// formula di cfRule (es. "\"Missing price\"" -> "Missing price") --
// un letterale numerico non le ha, resta invariato.
static std::string StripQuotes(const std::string& s)
{
	if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
		return s.substr(1, s.size() - 2);
	return s;
}

// Formattazione condizionale VIVA (Fase 13, prima Fase 12): non piu'
// valutata una tantum e congelata come colore statico -- la regola
// stessa viene convertita e aggiunta al documento
// (CContainer::AddConditionalFormatRule), rivalutata da SheetView::
// Draw a ogni ridisegno contro i valori CORRENTI (vedi
// CContainer::EvaluateConditionalFormatting in Container.styles.cpp).
// Stessi due tipi di regola gia' gestiti alla Fase 12, gli unici
// davvero comuni in un file reale: "cellIs"/"equal" (confronto con un
// letterale, stringa o numero) e "duplicateValues" (celle il cui
// valore compare piu' di una volta nello stesso intervallo). Gli
// altri tipi ECMA-376 (containsText, top10, colorScale, dataBar,
// iconSet, expression con formula arbitraria...) restano ignorati in
// sicurezza, nessuna regola aggiunta -- richiederebbero un vero
// motore di valutazione formule contro un valore ipotetico, fuori
// scope. Solo il colore di SFONDO del dxf (non anche il colore del
// testo): ConditionalFormatRule dell'engine porta un solo colore,
// vedi il commento su quello struct in Container.h.
static void ApplyConditionalFormatting(CContainer* doc,
	const std::vector<CondFormatRule>& rules, const std::vector<DxfInfo>& dxfs)
{
	for (size_t i = 0; i < rules.size(); i++)
	{
		const CondFormatRule& rule = rules[i];
		if (rule.dxfId < 0 || (size_t)rule.dxfId >= dxfs.size())
			continue;
		const DxfInfo& dxf = dxfs[rule.dxfId];
		if (!dxf.hasBg)
			continue;

		ConditionalFormatRule engineRule;
		engineRule.ranges = rule.ranges;
		engineRule.bgColor = dxf.bg;

		if (rule.type == "cellIs" && rule.operatorAttr == "equal")
		{
			engineRule.type = eCondCellIsEqual;
			engineRule.compareValue = StripQuotes(rule.formula);
		}
		else if (rule.type == "duplicateValues")
			engineRule.type = eCondDuplicateValues;
		else
			continue; // altri tipi: ignorati, vedi il commento sopra la funzione.

		doc->AddConditionalFormatRule(engineRule);
	}
}

// --- Parsing di xl/drawings/drawingN.xml (Fase 12) ------------------
//
// Un'immagine incorporata (es. un logo, presente nel file di gara
// reale) e' ancorata a una cella tramite <xdr:twoCellAnchor>/
// <xdr:oneCellAnchor>: <xdr:from> da' sempre colonna/riga 0-based piu'
// uno scarto in EMU (unita' di DrawingML, 914400 per pollice = 9525
// per pixel a 96 DPI, la risoluzione predefinita di Excel), mentre la
// dimensione si legge da <xdr:ext> (oneCellAnchor) o da
// <a:xfrm><a:ext> (twoCellAnchor) -- spesso 0x0 quando l'immagine non
// e' mai stata ridimensionata a mano (visto nel file reale): in quel
// caso si usa la dimensione naturale del PNG (dal proprio IHDR, vedi
// PngDimensions sotto) invece di non disegnare nulla. Il riferimento
// all'immagine vera e propria (<a:blip r:embed="rId..">) si risolve
// dopo, tramite i _rels dello stesso file drawing.
struct DrawingPic {
	int fromCol, fromRow; // 0-based, come nel file XLSX originale
	long fromColOffEmu, fromRowOffEmu;
	long extCxEmu, extCyEmu; // 0 = non specificato, vedi sopra
	std::string relId;

	DrawingPic() : fromCol(0), fromRow(0), fromColOffEmu(0), fromRowOffEmu(0),
		extCxEmu(0), extCyEmu(0) {}
};

struct DrawingContext {
	std::vector<DrawingPic> pics;
	DrawingPic current;
	bool inAnchor;
	bool inFrom;
	bool captureNum;
	std::string numText;
};

static void XMLCALL DrawingStart(void* userData, const char* name, const char** atts)
{
	DrawingContext* ctx = (DrawingContext*)userData;

	if (strcmp(name, "xdr:twoCellAnchor") == 0 || strcmp(name, "xdr:oneCellAnchor") == 0)
	{
		ctx->inAnchor = true;
		ctx->current = DrawingPic();
		return;
	}
	if (!ctx->inAnchor)
		return;

	if (strcmp(name, "xdr:from") == 0)
		ctx->inFrom = true;
	else if (ctx->inFrom && (strcmp(name, "xdr:col") == 0 || strcmp(name, "xdr:colOff") == 0
		|| strcmp(name, "xdr:row") == 0 || strcmp(name, "xdr:rowOff") == 0))
	{
		ctx->numText.clear();
		ctx->captureNum = true;
	}
	else if (strcmp(name, "xdr:ext") == 0 || strcmp(name, "a:ext") == 0)
	{
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "cx") == 0)
				ctx->current.extCxEmu = atol(atts[i + 1]);
			else if (strcmp(atts[i], "cy") == 0)
				ctx->current.extCyEmu = atol(atts[i + 1]);
		}
	}
	else if (strcmp(name, "a:blip") == 0)
	{
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "r:embed") == 0)
				ctx->current.relId = atts[i + 1];
		}
	}
}

static void XMLCALL DrawingEnd(void* userData, const char* name)
{
	DrawingContext* ctx = (DrawingContext*)userData;

	if (strcmp(name, "xdr:twoCellAnchor") == 0 || strcmp(name, "xdr:oneCellAnchor") == 0)
	{
		ctx->inAnchor = false;
		if (!ctx->current.relId.empty())
			ctx->pics.push_back(ctx->current);
	}
	else if (strcmp(name, "xdr:from") == 0)
		ctx->inFrom = false;
	else if (ctx->captureNum)
	{
		long value = atol(ctx->numText.c_str());
		if (strcmp(name, "xdr:col") == 0)
			ctx->current.fromCol = (int)value;
		else if (strcmp(name, "xdr:colOff") == 0)
			ctx->current.fromColOffEmu = value;
		else if (strcmp(name, "xdr:row") == 0)
			ctx->current.fromRow = (int)value;
		else if (strcmp(name, "xdr:rowOff") == 0)
			ctx->current.fromRowOffEmu = value;
		ctx->captureNum = false;
	}
}

static void XMLCALL DrawingChars(void* userData, const char* s, int len)
{
	DrawingContext* ctx = (DrawingContext*)userData;
	if (ctx->captureNum)
		ctx->numText.append(s, len);
}

static bool ParseDrawing(const std::vector<unsigned char>& xml, std::vector<DrawingPic>* out)
{
	if (xml.empty())
		return false;

	DrawingContext ctx;
	ctx.inAnchor = false;
	ctx.inFrom = false;
	ctx.captureNum = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, DrawingStart, DrawingEnd);
	XML_SetCharacterDataHandler(parser, DrawingChars);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), (int)xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK)
		return false;

	*out = ctx.pics;
	return true;
}

// Legge larghezza/altezza in pixel dall'header IHDR di un PNG, senza
// serve un decodificatore completo (il Translation Kit non e'
// disponibile qui, questo translator resta senza dipendenze
// dall'Interface Kit) -- 8 byte di firma PNG, poi length(4)+"IHDR"(4)+
// width(4, big-endian)+height(4, big-endian)+... Usata solo quando
// l'anchor XLSX non da' gia' una dimensione esplicita (vedi sopra);
// altri formati immagine (JPEG...) restano senza dimensione naturale
// nota, limite dichiarato -- il file di gara reale usa solo PNG.
static bool PngDimensions(const std::vector<unsigned char>& data, uint32* outW, uint32* outH)
{
	static const unsigned char kPngSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	if (data.size() < 24 || memcmp(&data[0], kPngSig, 8) != 0)
		return false;
	if (memcmp(&data[12], "IHDR", 4) != 0)
		return false;

	*outW = ((uint32)data[16] << 24) | ((uint32)data[17] << 16) | ((uint32)data[18] << 8) | data[19];
	*outH = ((uint32)data[20] << 24) | ((uint32)data[21] << 16) | ((uint32)data[22] << 8) | data[23];
	return true;
}

static const double kEmuPerPixel = 9525.0; // DrawingML, 96 DPI (predefinito Excel)

// Un foglio gia' analizzato, pronto per essere scritto in formato
// ASCD/ASCB: nome, documento, e le sole colonne con una larghezza
// esplicita nel file XLSX originale (vedi ParseSheet/SheetStart).
struct ParsedSheet {
	std::string name;
	CContainer* doc;
	std::vector<std::pair<int, float> > colWidths;
	std::vector<std::pair<int, float> > rowHeights;
	std::vector<EmbeddedImage> images;
	bool showGrid = true;
	bool hasTabColor = false;
	rgb_color tabColor = { 0, 0, 0, 255 };
	std::vector<int> hiddenRows;
	bool hasAutoFilter = false;
	range autoFilterRange;
};

// Scrive una cartella di lavoro multi-foglio in formato "ASCB" (vedi
// il commento su kASCDBookMagic sopra): riusa WriteASCD cosi' com'e'
// per ogni foglio, nessuna duplicazione della serializzazione per
// cella.
static status_t WriteASCDBook(const std::vector<ParsedSheet>& sheets, BPositionIO* dest)
{
	if (dest->Write(kASCDBookMagic, 4) != 4)
		return B_IO_ERROR;

	int32 sheetCount = (int32)sheets.size();
	if (dest->Write(&sheetCount, sizeof(sheetCount)) != (ssize_t)sizeof(sheetCount))
		return B_IO_ERROR;

	for (int32 i = 0; i < sheetCount; i++)
	{
		const std::string& name = sheets[i].name;
		int32 nameLen = (int32)name.size();
		if (dest->Write(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
			return B_IO_ERROR;
		if (nameLen > 0 && dest->Write(name.data(), nameLen) != nameLen)
			return B_IO_ERROR;

		status_t err = WriteASCD(sheets[i].doc, dest, &sheets[i].colWidths, &sheets[i].images,
			&sheets[i].rowHeights, &sheets[i].showGrid,
			&sheets[i].hasTabColor, &sheets[i].tabColor,
			&sheets[i].hiddenRows, &sheets[i].hasAutoFilter, &sheets[i].autoFilterRange);
		if (err != B_OK)
			return err;
	}

	return B_OK;
}

status_t CXlsxTranslator::Identify(BPositionIO* source,
	const translation_format* format, BMessage* extension,
	translator_info* info, uint32 outType)
{
	off_t pos = source->Position();
	unsigned char header[4];
	ssize_t read = source->Read(header, 4);
	source->Seek(pos, SEEK_SET);

	// Riconosce anche un sorgente ASCD nativo: serve per l'esportazione
	// (ASCD -> XLSX), la direzione opposta della normale importazione
	// gestita sotto -- stesso approccio di CsvTranslator/OdsTranslator.
	if (read == 4 && memcmp(header, kASCDMagic, 4) == 0)
	{
		info->type = kAtomoNativeFormat;
		info->group = kAtomoSheetGroup;
		info->quality = 1.0f;
		info->capability = 1.0f;
		strlcpy(info->name, "Atomo Sheet Cell Data (ASCD)", sizeof(info->name));
		strlcpy(info->MIME, "application/x-vnd.atomo-sheet-data", sizeof(info->MIME));
		return B_OK;
	}

	// Firma ZIP locale ("PK\x03\x04"): necessaria ma non sufficiente
	// (qualunque ZIP la ha) -- si verifica anche la presenza della
	// voce [Content_Types].xml, che solo i pacchetti OOXML hanno.
	static const unsigned char kZipSig[4] = { 'P', 'K', 0x03, 0x04 };
	if (read != 4 || memcmp(header, kZipSig, 4) != 0)
		return B_NO_TRANSLATOR;

	CZipReader zip;
	if (!zip.Open(source) || !zip.HasEntry("[Content_Types].xml"))
	{
		source->Seek(pos, SEEK_SET);
		return B_NO_TRANSLATOR;
	}
	source->Seek(pos, SEEK_SET);

	info->type = kAtomoXlsxFormat;
	info->group = kAtomoSheetGroup;
	info->quality = 0.9f;
	info->capability = 0.9f;
	strlcpy(info->name, "Microsoft Excel 2007+ (XLSX)", sizeof(info->name));
	strlcpy(info->MIME,
		"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
		sizeof(info->MIME));

	return B_OK;
}

status_t CXlsxTranslator::Translate(BPositionIO* source,
	const translator_info* info, BMessage* extension, uint32 outType,
	BPositionIO* destination)
{
	if (info->type != kAtomoXlsxFormat && info->type != kAtomoNativeFormat)
		return B_NO_TRANSLATOR;
	if (outType == 0)
		outType = kAtomoNativeFormat;
	if (outType != kAtomoNativeFormat && outType != kAtomoXlsxFormat)
		return B_NO_TRANSLATOR;

	if (info->type == kAtomoNativeFormat)
	{
		// ASCD -> XLSX (esportazione, vedi WriteXLSX sotto): un solo
		// foglio, come da sempre -- non cambia con il supporto
		// multi-foglio, che riguarda solo l'IMPORTAZIONE (vedi sotto).
		CContainer* doc = new CContainer(NULL, NULL);
		status_t err = ReadASCD(source, doc);
		if (err == B_OK)
			err = (outType == kAtomoNativeFormat) ? WriteASCD(doc, destination)
				: WriteXLSX(doc, destination);
		doc->Release();
		return err;
	}

	// XLSX -> ASCD/ASCB (importazione): legge TUTTI i fogli della
	// cartella di lavoro (Fase 9), non solo il primo -- xl/workbook.xml
	// elenca nome e r:id di ciascun foglio nell'ordine delle schede,
	// xl/_rels/workbook.xml.rels fa corrispondere ogni r:id al file XML
	// fisico che contiene i dati. Se manca anche solo uno di questi due
	// pezzi (pacchetto malformato, o generato da uno strumento che non
	// li scrive nel modo atteso) si torna al comportamento precedente
	// -- un solo foglio, xl/worksheets/sheet1.xml -- invece di fallire
	// del tutto: un foglio solo e' comunque meglio di niente.
	CZipReader zip;
	if (!zip.Open(source))
		return B_BAD_DATA;

	std::vector<unsigned char> sharedStringsXml;
	zip.ReadEntry("xl/sharedStrings.xml", sharedStringsXml); // opzionale

	std::vector<std::string> sharedStrings;
	if (!ParseSharedStrings(sharedStringsXml, sharedStrings))
		return B_BAD_DATA;

	// Tema e stili (colori di sfondo/testo): entrambi opzionali, un
	// pacchetto senza xl/theme/theme1.xml o xl/styles.xml importa
	// semplicemente senza colori invece di fallire (vedi ParseTheme/
	// ParseStyles sopra).
	std::vector<unsigned char> themeXml;
	zip.ReadEntry("xl/theme/theme1.xml", themeXml);
	XlsxTheme theme;
	ParseTheme(themeXml, &theme);

	std::vector<unsigned char> stylesXml;
	zip.ReadEntry("xl/styles.xml", stylesXml);
	std::vector<ResolvedStyle> resolvedStyles;
	ParseStyles(stylesXml, theme, &resolvedStyles);

	// Formati differenziali (Fase 12), per la formattazione
	// condizionale sotto: stesso file styles.xml di sopra, una
	// sezione <dxfs> a parte da <cellXfs>.
	std::vector<DxfInfo> dxfs;
	ParseDxfs(stylesXml, theme, &dxfs);

	std::vector<std::pair<std::string, std::string> > sheetsToRead; // (nome, percorso XML)

	std::vector<unsigned char> workbookXml, relsXml;
	std::vector<WorkbookSheetInfo> sheetList;
	std::map<std::string, std::string> relTargets;
	bool date1904 = false; // <workbookPr date1904="1"/>, Fase 12: resta false (predefinito) se il parse sotto non arriva a leggerlo
	if (zip.ReadEntry("xl/workbook.xml", workbookXml)
		&& ParseWorkbookSheetList(workbookXml, sheetList, &date1904)
		&& zip.ReadEntry("xl/_rels/workbook.xml.rels", relsXml)
		&& ParseRelationships(relsXml, relTargets))
	{
		for (size_t i = 0; i < sheetList.size(); i++)
		{
			std::map<std::string, std::string>::iterator it =
				relTargets.find(sheetList[i].rId);
			if (it == relTargets.end())
				continue; // r:id senza una voce corrispondente nei _rels: salta questo foglio

			std::string path = "xl/" + it->second; // i _rels sono relativi a xl/
			if (zip.HasEntry(path.c_str()))
				sheetsToRead.push_back(std::make_pair(sheetList[i].name, path));
		}
	}

	if (sheetsToRead.empty())
	{
		// Ripiego: un solo foglio, come prima del supporto multi-foglio.
		if (!zip.HasEntry("xl/worksheets/sheet1.xml"))
			return B_BAD_DATA;
		sheetsToRead.push_back(std::make_pair(std::string("Foglio1"),
			std::string("xl/worksheets/sheet1.xml")));
	}

	std::vector<ParsedSheet> sheets;
	status_t err = B_OK;

	for (size_t i = 0; i < sheetsToRead.size() && err == B_OK; i++)
	{
		std::vector<unsigned char> sheetXml;
		if (!zip.ReadEntry(sheetsToRead[i].second.c_str(), sheetXml))
		{
			err = B_BAD_DATA;
			break;
		}

		ParsedSheet parsed;
		parsed.name = sheetsToRead[i].first;
		parsed.doc = new CContainer(NULL, NULL);
		std::vector<CondFormatRule> condRules;
		if (!ParseSheet(sheetXml, parsed.doc, sharedStrings, &parsed.colWidths, &resolvedStyles,
			&condRules, date1904, &parsed.rowHeights, &parsed.showGrid,
			&parsed.hasTabColor, &parsed.tabColor,
			&parsed.hiddenRows, &parsed.hasAutoFilter, &parsed.autoFilterRange))
		{
			parsed.doc->Release();
			err = B_BAD_DATA;
			break;
		}

		// Formattazione condizionale (Fase 12): valutata ORA, dopo che
		// l'intero foglio e' stato letto e ogni cella ha gia' il suo
		// valore vero (serve per "cellIs"/"duplicateValues", che
		// confrontano contenuti di celle) -- vedi il commento sopra
		// ApplyConditionalFormatting.
		ApplyConditionalFormatting(parsed.doc, condRules, dxfs);

		// Tabelle strutturate (Fase 12): collegate a QUESTO foglio
		// tramite i suoi stessi _rels (xl/worksheets/_rels/sheetN.xml.rels),
		// non tramite quelli della cartella di lavoro sopra -- un
		// foglio senza tabelle semplicemente non ha questo file, non
		// e' un errore. I target dei _rels di un foglio sono relativi
		// a xl/worksheets/ (es. "../tables/table1.xml"), un livello
		// piu' in profondita' di quelli del workbook sopra: si toglie
		// il prefisso ".." e si ricompone su "xl/".
		{
			const std::string& sheetPath = sheetsToRead[i].second;
			size_t slash = sheetPath.find_last_of('/');
			std::string dir = slash == std::string::npos ? "" : sheetPath.substr(0, slash);
			std::string file = slash == std::string::npos ? sheetPath : sheetPath.substr(slash + 1);
			std::string sheetRelsPath = dir + "/_rels/" + file + ".rels";

			std::vector<unsigned char> sheetRelsXml;
			std::map<std::string, std::string> sheetRelTargets;
			if (zip.ReadEntry(sheetRelsPath.c_str(), sheetRelsXml)
				&& ParseRelationships(sheetRelsXml, sheetRelTargets))
			{
				for (std::map<std::string, std::string>::iterator it = sheetRelTargets.begin();
					it != sheetRelTargets.end(); ++it)
				{
					std::string target = it->second;
					if (target.compare(0, 3, "../") == 0)
						target = target.substr(3);

					if (target.compare(0, 7, "tables/") == 0)
					{
						std::string tablePath = "xl/" + target;
						std::vector<unsigned char> tableXml;
						TableInfo info;
						if (zip.ReadEntry(tablePath.c_str(), tableXml)
							&& ParseTableInfo(tableXml, &info))
						{
							if (info.showStripes)
								ApplyTableBanding(parsed.doc, info.tableRange);
							RegisterTable(parsed.doc, info);
						}
					}
					else if (target.compare(0, 9, "drawings/") == 0)
					{
						// Immagini incorporate (Fase 12): il file drawing
						// elenca solo l'ancoraggio (cella/scarto/dimensione)
						// piu' un r:embed -- l'immagine vera si risolve
						// tramite i _rels DI QUESTO drawing (un livello di
						// indirizzamento indipendente da quelli del foglio
						// sopra, stesso principio ma un passo piu' in
						// profondita').
						std::string drawingPath = "xl/" + target;
						std::vector<unsigned char> drawingXml;
						std::vector<DrawingPic> pics;
						if (!zip.ReadEntry(drawingPath.c_str(), drawingXml)
							|| !ParseDrawing(drawingXml, &pics) || pics.empty())
							continue;

						size_t dSlash = drawingPath.find_last_of('/');
						std::string dDir = dSlash == std::string::npos ? ""
							: drawingPath.substr(0, dSlash);
						std::string dFile = dSlash == std::string::npos ? drawingPath
							: drawingPath.substr(dSlash + 1);
						std::string drawingRelsPath = dDir + "/_rels/" + dFile + ".rels";

						std::vector<unsigned char> drawingRelsXml;
						std::map<std::string, std::string> drawingRelTargets;
						if (!zip.ReadEntry(drawingRelsPath.c_str(), drawingRelsXml)
							|| !ParseRelationships(drawingRelsXml, drawingRelTargets))
							continue;

						for (size_t p = 0; p < pics.size(); p++)
						{
							std::map<std::string, std::string>::iterator rit =
								drawingRelTargets.find(pics[p].relId);
							if (rit == drawingRelTargets.end())
								continue;

							std::string mediaTarget = rit->second;
							if (mediaTarget.compare(0, 3, "../") == 0)
								mediaTarget = mediaTarget.substr(3);
							std::string mediaPath = "xl/" + mediaTarget;

							std::vector<unsigned char> pngBytes;
							if (!zip.ReadEntry(mediaPath.c_str(), pngBytes) || pngBytes.empty())
								continue;

							uint32 natW = 0, natH = 0;
							PngDimensions(pngBytes, &natW, &natH);

							EmbeddedImage img;
							img.anchor = cell(pics[p].fromCol + 1, pics[p].fromRow + 1);
							img.offsetX = (float)(pics[p].fromColOffEmu / kEmuPerPixel);
							img.offsetY = (float)(pics[p].fromRowOffEmu / kEmuPerPixel);
							img.width = pics[p].extCxEmu > 0
								? (float)(pics[p].extCxEmu / kEmuPerPixel) : (float)natW;
							img.height = pics[p].extCyEmu > 0
								? (float)(pics[p].extCyEmu / kEmuPerPixel) : (float)natH;
							img.pngData.assign(pngBytes.begin(), pngBytes.end());

							if (img.width > 0 && img.height > 0)
								parsed.images.push_back(img);
						}
					}
				}
			}
		}

		sheets.push_back(parsed);
	}

	if (err == B_OK)
	{
		if (outType == kAtomoNativeFormat)
			err = WriteASCDBook(sheets, destination);
		else
			// L'esportazione XLSX resta a un solo foglio (quello
			// attivo, il primo qui): i writer non nativi non
			// supportano ancora piu' fogli, vedi WriteXLSX.
			err = WriteXLSX(sheets[0].doc, destination);
	}

	for (size_t i = 0; i < sheets.size(); i++)
		sheets[i].doc->Release();

	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
		return new CXlsxTranslator();
	return NULL;
}
