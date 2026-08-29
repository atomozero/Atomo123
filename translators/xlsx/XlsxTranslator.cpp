/*
	XlsxTranslator.cpp

	Vedi XlsxTranslator.h per la descrizione generale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
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

#include <Catalog.h>
#include <Cursor.h>
#include <DataIO.h>
#include <Entry.h>
#include <Font.h>
#include <image.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Roster.h>
#include <String.h>
#include <StringView.h>
#include <View.h>

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
#include "Formula.h"
#include "FunctionUtils.h"
#include "parser.h"
#include "Globals.h"
#include "NameTable.h"

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
// "ASC2" (Fase 32b), non piu' "ASCB": duplicato da ui/src/AscdIO.cpp,
// stesso motivo di ogni altra sezione di questo file -- vedi il
// commento su kASCDBook2Magic li' sul BUG REALE che questo cambio di
// formato risolve (un foglio senza le sezioni vbaProject/blocco
// celle/protezione, in mezzo a una cartella multi-foglio, faceva
// leggere a LoadASCD i primi byte del foglio successivo come se fossero
// l'inizio di quelle sezioni). Ogni blocco per foglio e' ora preceduto
// dalla propria lunghezza in byte.
static const char kASCDBook2Magic[4] = { 'A', 'S', 'C', '2' };

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

// Un grafico incorporato, sia in lettura (Fase 24, esportazione verso
// XLSX) sia in scrittura (Fase 25, importazione da XLSX): stessi campi
// di ChartObject in ui/src/Chart.h, MAI quell'header incluso qui
// apposta -- stesso principio di ogni altro pezzo del formato ASCD
// duplicato in questo file (vedi AscdIO.h): un translator resta
// autonomo dall'app, nessun collegamento a ui/src/*.
struct XlsxChartInfo {
	int16 dataLeft, dataTop, dataRight, dataBottom; // ChartObject::dataRange (celle, 1-based)
	float frameLeft, frameTop, frameRight, frameBottom; // ChartObject::frame (pixel nel foglio)
	int8 type; // 0 = barre, 1 = linee, 2 = torta (ChartType in Chart.h)
	std::string title;
};

// Stessa serializzazione ASCD degli altri translator (vedi
// translators/csv/CsvTranslator.cpp per la descrizione completa).
static status_t WriteASCD(CContainer* doc, BPositionIO* dest,
	const std::vector<std::pair<int, float> >* colWidths = NULL,
	const std::vector<EmbeddedImage>* images = NULL,
	const std::vector<std::pair<int, float> >* rowHeights = NULL,
	const bool* showGrid = NULL,
	const bool* hasTabColor = NULL, const rgb_color* tabColor = NULL,
	const std::vector<int>* hiddenRows = NULL,
	const bool* hasAutoFilter = NULL, const range* autoFilterRange = NULL,
	const std::vector<XlsxChartInfo>* charts = NULL,
	// XLSM (Fase 31): vedi il commento gemello in ui/src/AscdIO.h --
	// bytes grezzi di "xl/vbaProject.bin", mai analizzati.
	const std::vector<unsigned char>* vbaProject = NULL,
	// Protezione foglio (Fase 32): vedi il commento gemello in
	// ui/src/AscdIO.h -- il blocco per-cella viaggia dentro "doc" come
	// ogni altro attributo di CellStyle, nessun parametro dedicato qui.
	const bool* isProtected = NULL,
	// Blocca riquadri (100% XLSX standard compatibility, Tier 2): vedi
	// il commento gemello in ui/src/AscdIO.h -- un valore vero ora che
	// ParseSheet/WriteXLSX estraggono/scrivono davvero <pane> (prima
	// sempre 0,0, vedi il commento piu' sotto dove venivano scritti).
	int frozenRows = 0, int frozenCols = 0)
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

	// Sezione grafici incorporati, in coda (Fase 25: prima sempre
	// vuota, "charts" e' NULL per ogni chiamante che non importa
	// grafici, es. l'esportazione ASCD -> XLSX/nativo). Stesso formato
	// binario di SaveASCD in ui/src/AscdIO.cpp, letto da LoadASCD.
	// Anche quando vuota il campo va scritto comunque: quando
	// WriteASCDBook incapsula piu' fogli in sequenza nello stesso
	// flusso, LoadASCD (chiamato da LoadASCDBook una volta per foglio)
	// non puo' distinguere "fine del flusso, nessun grafico" (fine
	// vera) da "qui non c'e' la sezione grafici" (fine del SOLO blocco
	// di questo foglio, con altri fogli a seguire): interpreterebbe i
	// primi 4 byte del foglio successivo (la lunghezza del suo nome)
	// come un numero di grafici, disallineando la lettura di ogni
	// foglio dopo il primo. Bug reale scoperto aprendo un file .xlsm
	// con 38 fogli: solo il primo veniva letto correttamente.
	int32 chartCount = charts ? (int32)charts->size() : 0;
	if (dest->Write(&chartCount, sizeof(chartCount)) != (ssize_t)sizeof(chartCount))
		return B_IO_ERROR;
	for (int32 i = 0; i < chartCount; i++)
	{
		const XlsxChartInfo& info = (*charts)[i];
		float frame[4] = { info.frameLeft, info.frameTop, info.frameRight, info.frameBottom };
		if (dest->Write(&info.dataLeft, sizeof(info.dataLeft)) != (ssize_t)sizeof(info.dataLeft)
			|| dest->Write(&info.dataTop, sizeof(info.dataTop)) != (ssize_t)sizeof(info.dataTop)
			|| dest->Write(&info.dataRight, sizeof(info.dataRight)) != (ssize_t)sizeof(info.dataRight)
			|| dest->Write(&info.dataBottom, sizeof(info.dataBottom)) != (ssize_t)sizeof(info.dataBottom)
			|| dest->Write(frame, sizeof(frame)) != (ssize_t)sizeof(frame))
			return B_IO_ERROR;
	}

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
	// altezze vere del file). Blocca riquadri (100% XLSX standard
	// compatibility, Tier 2) ora arriva dal parametro "frozenRows"/
	// "frozenCols" -- prima sempre a zero, questo translator non lo
	// estraeva ancora dal file XLSX originale. Il campo va comunque
	// scritto sempre, mai omesso, per lo stesso motivo della
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

	int32 frozenRows32 = frozenRows, frozenCols32 = frozenCols;
	if (dest->Write(&frozenRows32, sizeof(frozenRows32)) != (ssize_t)sizeof(frozenRows32)
		|| dest->Write(&frozenCols32, sizeof(frozenCols32)) != (ssize_t)sizeof(frozenCols32))
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

	// Sezione commenti/note per cella, in coda (100% XLSX standard
	// compatibility, Tier 2): stesso schema binario e stessa fonte
	// (CContainer::GetComments) di ui/src/AscdIO.cpp (SaveASCD) --
	// scritta con i valori reali ora che ParseSheet/il ciclo dei _rels
	// del foglio (vedi Translate) popolano davvero "doc" tramite
	// <comments>/xl/worksheets/_rels/sheetN.xml.rels.
	{
		const std::map<cell, std::string>& comments = doc->GetComments();
		int32 commentCount = (int32)comments.size();
		if (dest->Write(&commentCount, sizeof(commentCount)) != (ssize_t)sizeof(commentCount))
			return B_IO_ERROR;

		for (std::map<cell, std::string>::const_iterator it = comments.begin();
			it != comments.end(); ++it)
		{
			int16 row = it->first.v, col = it->first.h;
			int32 len = (int32)it->second.size();
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(it->second.data(), len) != len)
				return B_IO_ERROR;
		}
	}

	// Sezione collegamenti ipertestuali, in coda (100% XLSX standard
	// compatibility, Tier 2): stesso schema esatto della sezione
	// commenti appena sopra, stessa fonte (CContainer::GetHyperlinks).
	{
		const std::map<cell, std::string>& links = doc->GetHyperlinks();
		int32 linkCount = (int32)links.size();
		if (dest->Write(&linkCount, sizeof(linkCount)) != (ssize_t)sizeof(linkCount))
			return B_IO_ERROR;

		for (std::map<cell, std::string>::const_iterator it = links.begin();
			it != links.end(); ++it)
		{
			int16 row = it->first.v, col = it->first.h;
			int32 len = (int32)it->second.size();
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(it->second.data(), len) != len)
				return B_IO_ERROR;
		}
	}

	// Sezione tipo di grafico incorporato, in coda (Fase 25, vedi
	// ui/src/AscdIO.cpp): un byte per grafico, nello STESSO ordine
	// dell'array "charts" scritto nella sezione grafici sopra.
	{
		int32 chartTypeCount = charts ? (int32)charts->size() : 0;
		if (dest->Write(&chartTypeCount, sizeof(chartTypeCount)) != (ssize_t)sizeof(chartTypeCount))
			return B_IO_ERROR;
		for (int32 i = 0; i < chartTypeCount; i++)
		{
			int8 type = (*charts)[i].type;
			if (dest->Write(&type, sizeof(type)) != (ssize_t)sizeof(type))
				return B_IO_ERROR;
		}
	}

	// Sezione colore del bordo di cella non predefinito, in coda (100%
	// XLSX standard compatibility, Tier 2): stesso schema esatto e
	// stessa fonte (CContainer::GetCellStyle/CellStyle::fBorderColor)
	// di ui/src/AscdIO.cpp (SaveASCD) -- scritta con i valori reali ora
	// che ParseSheet estrae davvero il colore da un <color> XLSX vero
	// (vedi ParseStyles/ResolveColorAttrs).
	{
		CellStyle defaultBorderStyle;
		std::vector<std::pair<cell, rgb_color> > borderColorsToWrite;
		CCellIterator borderColorIter(doc, NULL);
		cell bcc;
		while (borderColorIter.NextExisting(bcc))
		{
			CellStyle cs;
			doc->GetCellStyle(bcc, cs);
			if (!ColorsEqual(cs.fBorderColor, defaultBorderStyle.fBorderColor))
				borderColorsToWrite.push_back(std::make_pair(bcc, cs.fBorderColor));
		}

		int32 borderColorCount = (int32)borderColorsToWrite.size();
		if (dest->Write(&borderColorCount, sizeof(borderColorCount)) != (ssize_t)sizeof(borderColorCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < borderColorCount; i++)
		{
			int16 row = borderColorsToWrite[i].first.v, col = borderColorsToWrite[i].first.h;
			rgb_color color = borderColorsToWrite[i].second;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&color, sizeof(color)) != (ssize_t)sizeof(color))
				return B_IO_ERROR;
		}
	}

	// Sezione convalida dati, in coda (100% XLSX standard compatibility,
	// Tier 2): stesso schema esatto di ui/src/AscdIO.cpp (SaveASCD),
	// stessa fonte (CContainer::GetValidations) -- scritta con i valori
	// reali ora che ParseSheet/WriteXLSX estraggono/scrivono davvero
	// <dataValidations> (solo elenco letterale e intervallo numerico
	// "between": gli unici due tipi che questo motore modella, vedi
	// ValidationRule in Container.h).
	{
		const std::map<cell, ValidationRule>& validations = doc->GetValidations();
		int32 validationCount = (int32)validations.size();
		if (dest->Write(&validationCount, sizeof(validationCount)) != (ssize_t)sizeof(validationCount))
			return B_IO_ERROR;

		for (std::map<cell, ValidationRule>::const_iterator it = validations.begin();
			it != validations.end(); ++it)
		{
			int16 row = it->first.v, col = it->first.h;
			int8 type = (int8)it->second.type;
			int32 len = (int32)it->second.list.size();
			double min = it->second.min, max = it->second.max;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&type, sizeof(type)) != (ssize_t)sizeof(type)
				|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(it->second.list.data(), len) != len)
				return B_IO_ERROR;
			if (dest->Write(&min, sizeof(min)) != (ssize_t)sizeof(min)
				|| dest->Write(&max, sizeof(max)) != (ssize_t)sizeof(max))
				return B_IO_ERROR;
		}
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

	// Sezione titolo di grafico incorporato, in coda (vedi
	// ui/src/AscdIO.cpp, STESSO ORDINE -- ultima sezione, dopo le
	// tabelle strutturate): sempre vuota, stesso principio delle sezioni
	// "non ancora estratte" sopra (il chartCount scritto piu' sopra e'
	// gia' sempre zero per questo translator). BUG REALE senza questa
	// sezione, non solo teorico: LoadASCD (lato lettura) e' EOF-
	// tollerante SOLO quando la sezione mancante e' davvero l'ULTIMA
	// cosa nello stream -- vero per un file a un solo foglio, ma per
	// una cartella di lavoro XLSX multi-foglio (formato ASCB, piu'
	// blocchi ASCD concatenati) ogni blocco tranne l'ultimo e' seguito
	// dal blocco del foglio successivo: senza scrivere questa sezione
	// (anche vuota) per OGNI foglio, la lettura del titolo del foglio N
	// finiva per leggere i primi byte del blocco del foglio N+1 come se
	// fossero un conteggio di titoli, disallineando tutto il resto
	// della lettura -- LoadASCDBook falliva con B_BAD_DATA su
	// QUALUNQUE file XLSX con piu' di un foglio, anche senza nessun
	// grafico. Scoperto da un file reale dell'utente che non si apriva
	// piu' dopo l'aggiunta del titolo dei grafici (Fase 17).
	{
		int32 chartTitleCount = charts ? (int32)charts->size() : 0;
		if (dest->Write(&chartTitleCount, sizeof(chartTitleCount)) != (ssize_t)sizeof(chartTitleCount))
			return B_IO_ERROR;
		for (int32 i = 0; i < chartTitleCount; i++)
		{
			const std::string& title = (*charts)[i].title;
			int32 len = (int32)title.size();
			if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(title.data(), len) != len)
				return B_IO_ERROR;
		}
	}

	// Sezione area di stampa, in coda (Fase 29 di ui/src/AscdIO.cpp):
	// questo translator non legge ancora l'area di stampa/le
	// impostazioni pagina dal file XLSX originale, quindi scrive sempre
	// "assente" (has=0). Va comunque scritta per OGNI foglio, stesso
	// motivo del BUG REALE spiegato nel commento della sezione titolo
	// di grafico qui sopra: senza questi byte fissi, LoadASCD (chiamato
	// da LoadASCDBook una volta per foglio) legge l'inizio del blocco
	// del foglio successivo come se fosse questa sezione, disallineando
	// la lettura di ogni foglio dopo il primo in un file multi-foglio.
	{
		uint8 has = 0;
		int16 top = 0, left = 0, bottom = 0, right = 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
			|| dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
			|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
			|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right))
			return B_IO_ERROR;
	}

	// Sezione margini/scala di "Imposta pagina", in coda (Fase 29):
	// stesso motivo e stesso principio "sempre scritta, has=0" della
	// sezione area di stampa appena sopra.
	{
		uint8 has = 0;
		double marginTop = 2.0, marginBottom = 2.0, marginLeft = 2.0, marginRight = 2.0;
		int32 scaleMode = 0;
		double scalePercent = 100.0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&marginTop, sizeof(marginTop)) != (ssize_t)sizeof(marginTop)
			|| dest->Write(&marginBottom, sizeof(marginBottom)) != (ssize_t)sizeof(marginBottom)
			|| dest->Write(&marginLeft, sizeof(marginLeft)) != (ssize_t)sizeof(marginLeft)
			|| dest->Write(&marginRight, sizeof(marginRight)) != (ssize_t)sizeof(marginRight)
			|| dest->Write(&scaleMode, sizeof(scaleMode)) != (ssize_t)sizeof(scaleMode)
			|| dest->Write(&scalePercent, sizeof(scalePercent)) != (ssize_t)sizeof(scalePercent))
			return B_IO_ERROR;
	}

	// Sezione progetto VBA (XLSM, Fase 31), in coda: stesso schema
	// "presente si'/no" delle sezioni sopra, duplicato da
	// ui/src/AscdIO.cpp (SaveASCD) per lo stesso motivo di ogni altra
	// sezione di questo file -- vedi il commento su WriteASCD sopra.
	{
		uint8 has = (vbaProject && !vbaProject->empty()) ? 1 : 0;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has))
			return B_IO_ERROR;
		if (has)
		{
			int32 len = (int32)vbaProject->size();
			if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(vbaProject->data(), len) != len)
				return B_IO_ERROR;
		}
	}

	// Sezione celle SBLOCCATE + protezione foglio (Fase 32), in coda:
	// stesso schema di ui/src/AscdIO.cpp (SaveASCD), duplicato qui per
	// lo stesso motivo di ogni altra sezione di questo file. Il default
	// e' ora fLocked=true (vedi il
	// costruttore di CellStyle), quindi l'elenco contiene le celle
	// esplicitamente SBLOCCATE -- gia' applicate a "doc" da ParseSheet
	// (vedi <protection locked="0"/> in xl/styles.xml) quando questa
	// funzione e' chiamata per l'esportazione ASCD -> XLSX di un
	// documento riletto da un file XLSX originale.
	{
		CellStyle defaultStyle;
		std::vector<cell> unlocked;
		CCellIterator lockIter(doc, NULL);
		cell lc;
		while (lockIter.NextExisting(lc))
		{
			CellStyle cs;
			doc->GetCellStyle(lc, cs);
			if (cs.fLocked != defaultStyle.fLocked)
				unlocked.push_back(lc);
		}

		int32 unlockedCount = (int32)unlocked.size();
		if (dest->Write(&unlockedCount, sizeof(unlockedCount)) != (ssize_t)sizeof(unlockedCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < unlockedCount; i++)
		{
			int16 row = unlocked[i].v, col = unlocked[i].h;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
				return B_IO_ERROR;
		}
	}
	{
		uint8 protectedByte = (isProtected && *isProtected) ? 1 : 0;
		if (dest->Write(&protectedByte, sizeof(protectedByte)) != (ssize_t)sizeof(protectedByte))
			return B_IO_ERROR;
	}

	// Sezione intervalli con nome, in coda, ULTIMA sezione del formato:
	// stesso schema di ui/src/AscdIO.cpp (SaveASCD), duplicato qui per
	// lo stesso motivo di ogni altra sezione di questo file -- "doc" ha
	// gia' i nomi applicati da ApplyDefinedNames (vedi sopra) quando
	// questa funzione e' chiamata per l'importazione XLSX -> ASCD di un
	// file reale.
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

// Legge un flusso ASCD e ricostruisce le celle in "doc" (vuoto in
// ingresso) -- stessa logica di CsvTranslator.cpp/OdsTranslator.cpp,
// usata qui per l'esportazione (ASCD -> XLSX, la direzione opposta
// della normale importazione XLSX -> ASCD gestita da ParseSheet/
// WriteASCD sopra). "outCharts" e' opzionale (NULL per chi non ha
// bisogno dei grafici, es. l'esportazione verso il formato nativo
// stesso qualche riga piu' sotto): se presente, legge anche le
// sezioni in coda del formato ASCD fino al titolo dei grafici incluso
// (l'ULTIMA sezione, vedi SaveASCD/LoadASCD in ui/src/AscdIO.cpp),
// scartando ogni altra sezione intermedia (colori, bordi, commenti,
// ecc. -- non ancora esportati verso XLSX) MA consumandone comunque i
// byte per restare allineati fino in fondo, stesso principio
// EOF-tollerante gia' documentato li'.
static status_t ReadASCD(BPositionIO* source, CContainer* doc,
	std::vector<XlsxChartInfo>* outCharts = NULL,
	std::vector<unsigned char>* outVbaProject = NULL,
	// Protezione foglio (Fase 32): a differenza delle sezioni colori/
	// allineamento/bordi qui sotto (ancora scartate, questo export non
	// le riporta), il blocco per-cella vive gia' dentro "doc" (letto
	// direttamente da TryToParseString/NewCell sopra? no -- vedi sotto)
	// e la protezione per-foglio e' un valore a parte: catturata qui
	// SOLO se il chiamante la vuole (NULL = scartata, come le altre).
	bool* outIsProtected = NULL,
	// Blocca riquadri (100% XLSX standard compatibility, Tier 2):
	// catturati SOLO se il chiamante li vuole (NULL = scartati, come
	// isProtected sopra) -- servono all'esportazione ASCD -> XLSX per
	// scrivere un vero <pane> (vedi WriteXLSX).
	int* outFrozenRows = NULL, int* outFrozenCols = NULL)
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

	// Le formule vanno comunque calcolate prima di esportare, anche se
	// adesso l'export scrive la formula viva per le celle sullo stesso
	// foglio (vedi BuildSheetXml sotto): il valore calcolato resta
	// necessario come <v> di riserva (letto subito da chi apre il file
	// senza ricalcolare) e come unico contenuto scritto per le celle
	// con un riferimento a un altro foglio, che l'export non esporta
	// come formula viva (vedi ReferencesOtherSheet). Range completo
	// per lo stesso motivo di WriteASCD sopra.
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

	if (!outCharts)
		return B_OK;
	outCharts->clear();

	// Sezione grafici incorporati (prima sezione in coda, vedi
	// SaveASCD in ui/src/AscdIO.cpp): posizione/dimensione di ogni
	// grafico. "got == 0" qui significa "file scritto prima che questa
	// sezione esistesse", non un errore -- stesso principio
	// EOF-tollerante di ogni altra sezione qui sotto.
	{
		int32 chartCount = 0;
		ssize_t got = source->Read(&chartCount, sizeof(chartCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(chartCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < chartCount; i++)
			{
				XlsxChartInfo info;
				info.type = 0;
				float frame[4];
				if (source->Read(&info.dataLeft, sizeof(info.dataLeft)) != (ssize_t)sizeof(info.dataLeft)
					|| source->Read(&info.dataTop, sizeof(info.dataTop)) != (ssize_t)sizeof(info.dataTop)
					|| source->Read(&info.dataRight, sizeof(info.dataRight)) != (ssize_t)sizeof(info.dataRight)
					|| source->Read(&info.dataBottom, sizeof(info.dataBottom)) != (ssize_t)sizeof(info.dataBottom)
					|| source->Read(frame, sizeof(frame)) != (ssize_t)sizeof(frame))
					return B_BAD_DATA;
				info.frameLeft = frame[0];
				info.frameTop = frame[1];
				info.frameRight = frame[2];
				info.frameBottom = frame[3];
				outCharts->push_back(info);
			}
		}
	}

	// Tutte le sezioni intermedie (fra i grafici sopra e il tipo di
	// grafico piu' sotto) non riguardano ancora l'esportazione XLSX
	// (colori, bordi, commenti, tabelle, ecc.) -- vanno pero' comunque
	// lette e scartate, MAI saltate, per restare allineati fino alla
	// sezione tipo/titolo di grafico in fondo al formato (stesso
	// principio EOF-tollerante, vedi il commento su
	// project_translator_ascd_trailing_section_bug in memoria: un
	// disallineamento qui produrrebbe B_BAD_DATA o dati insensati letti
	// come se fossero il tipo/titolo del grafico). Elenco ESATTO e
	// nello STESSO ORDINE di LoadASCD in ui/src/AscdIO.cpp fra le due
	// sezioni che servono davvero qui.
	{
		// Larghezze di colonna: (int16 col, float width) per record.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 col; float width;
				if (source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&width, sizeof(width)) != (ssize_t)sizeof(width))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Colori di cella: (int16 row, int16 col, 8 byte colore) per record.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; uint8 colorBuf[8];
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(colorBuf, sizeof(colorBuf)) != (ssize_t)sizeof(colorBuf))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Colori di colonna: (int16 col, 8 byte colore) per record.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 col; uint8 colorBuf[8];
				if (source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(colorBuf, sizeof(colorBuf)) != (ssize_t)sizeof(colorBuf))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Altezze di riga: (int16 row, float height) per record.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row; float height;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&height, sizeof(height)) != (ssize_t)sizeof(height))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Blocca riquadri: due int32, MAI un conteggio davanti (sempre
		// presenti insieme, diverso da tutte le altre sezioni qui).
		int32 fr = 0, fc = 0;
		ssize_t got = source->Read(&fr, sizeof(fr));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(fr)
				|| source->Read(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
				return B_BAD_DATA;
		}
		if (outFrozenRows) *outFrozenRows = fr;
		if (outFrozenCols) *outFrozenCols = fc;
	}
	{
		// Font di cella: (int16 row, int16 col, font_family, font_style, float size).
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; font_family family; font_style style; float size;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(family, sizeof(font_family)) != (ssize_t)sizeof(font_family)
					|| source->Read(style, sizeof(font_style)) != (ssize_t)sizeof(font_style)
					|| source->Read(&size, sizeof(size)) != (ssize_t)sizeof(size))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Allineamento di cella: (int16 row, int16 col, int8 allineamento).
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; int8 alignment;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&alignment, sizeof(alignment)) != (ssize_t)sizeof(alignment))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Bordi di cella (spessore): (int16 row, int16 col, 4 byte lati).
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; uint8 sides[4];
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(sides, sizeof(sides)) != (ssize_t)sizeof(sides))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Formato numero di cella: (int16 row, int16 col, int32 formato).
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; int32 format;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&format, sizeof(format)) != (ssize_t)sizeof(format))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Sottolineato di cella: (int16 row, int16 col), nessun valore.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Testo a capo di cella: (int16 row, int16 col), nessun valore.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Celle unite: (int16 top, left, bottom, right) per record.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 top, left, bottom, right;
				if (source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
					|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right))
					return B_BAD_DATA;
			}
		}
	}
	{
		// Immagini incorporate: (int16 row, col, 4 float geom, int32
		// pngLen, poi pngLen byte del PNG).
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; float geom[4]; int32 pngLen;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(geom, sizeof(geom)) != (ssize_t)sizeof(geom)
					|| source->Read(&pngLen, sizeof(pngLen)) != (ssize_t)sizeof(pngLen))
					return B_BAD_DATA;
				if (pngLen < 0 || pngLen > 200 * 1024 * 1024)
					return B_BAD_DATA;
				if (pngLen > 0 && source->Seek(pngLen, SEEK_CUR) < 0)
					return B_BAD_DATA;
			}
		}
	}
	{
		// Visibilita' griglia: un solo byte, MAI un conteggio davanti.
		uint8 sg = 1;
		ssize_t got = source->Read(&sg, sizeof(sg));
		if (got != 0 && got != (ssize_t)sizeof(sg))
			return B_BAD_DATA;
	}
	{
		// Colore linguetta foglio: (uint8 has, poi 3 byte rgb se has != 0).
		uint8 has = 0; uint8 rgb[3];
		ssize_t got = source->Read(&has, sizeof(has));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(has)
				|| source->Read(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
				return B_BAD_DATA;
		}
	}
	{
		// Righe nascoste: un int16 per record.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row))
					return B_BAD_DATA;
			}
		}
	}
	{
		// AutoFilter: (uint8 has, poi 4 int16 range se has != 0).
		uint8 has = 0; int16 t2, l2, b2, r2;
		ssize_t got = source->Read(&has, sizeof(has));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(has)
				|| source->Read(&t2, sizeof(t2)) != (ssize_t)sizeof(t2)
				|| source->Read(&l2, sizeof(l2)) != (ssize_t)sizeof(l2)
				|| source->Read(&b2, sizeof(b2)) != (ssize_t)sizeof(b2)
				|| source->Read(&r2, sizeof(r2)) != (ssize_t)sizeof(r2))
				return B_BAD_DATA;
		}
	}
	{
		// Commenti: (int16 row, col, int32 len, poi len byte di testo).
		// Stesso schema di ui/src/AscdIO.cpp (LoadASCD) -- applicati
		// davvero a "doc" ora (100% XLSX standard compatibility, Tier 2),
		// non piu' solo scartati per restare allineati.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; int32 len;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
					return B_BAD_DATA;
				if (!cell(col, row).IsValid())
					return B_BAD_DATA;
				if (len < 0 || len > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string text;
				if (len > 0)
				{
					text.resize(len);
					if (source->Read(&text[0], len) != len)
						return B_BAD_DATA;
				}
				doc->SetComment(cell(col, row), text);
			}
		}
	}
	{
		// Collegamenti ipertestuali: stesso schema esatto dei commenti
		// sopra, ora applicati davvero a "doc" (100% XLSX standard
		// compatibility, Tier 2), non piu' solo scartati.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; int32 len;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
					return B_BAD_DATA;
				if (!cell(col, row).IsValid())
					return B_BAD_DATA;
				if (len < 0 || len > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string text;
				if (len > 0)
				{
					text.resize(len);
					if (source->Read(&text[0], len) != len)
						return B_BAD_DATA;
				}
				doc->SetHyperlink(cell(col, row), text);
			}
		}
	}

	// Sezione tipo di grafico incorporato: assegna nello STESSO ordine
	// dell'array outCharts gia' popolato piu' sopra (vedi il commento
	// gemello in LoadASCD).
	{
		int32 chartTypeCount = 0;
		ssize_t got = source->Read(&chartTypeCount, sizeof(chartTypeCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(chartTypeCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < chartTypeCount; i++)
			{
				int8 type;
				if (source->Read(&type, sizeof(type)) != (ssize_t)sizeof(type))
					return B_BAD_DATA;
				if (i < (int32)outCharts->size())
					(*outCharts)[i].type = type;
			}
		}
	}
	{
		// Colore del bordo di cella: (int16 row, col, rgb_color = 4
		// byte). Stesso schema di ui/src/AscdIO.cpp (LoadASCD) --
		// applicato davvero a "doc" ora (100% XLSX standard
		// compatibility, Tier 2), non piu' solo scartato.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; rgb_color color;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&color, sizeof(color)) != (ssize_t)sizeof(color))
					return B_BAD_DATA;
				if (!cell(col, row).IsValid())
					return B_BAD_DATA;

				cell c(col, row);
				CellStyle cs;
				doc->GetCellStyle(c, cs);
				cs.fBorderColor = color;
				doc->SetCellStyle(c, cs);
			}
		}
	}
	{
		// Convalida dati: (int16 row, col, int8 tipo, int32 len, len
		// byte elenco, 2 double min/max). Stesso schema di ui/src/
		// AscdIO.cpp (LoadASCD) -- applicata davvero a "doc" ora (100%
		// XLSX standard compatibility, Tier 2), non piu' solo scartata.
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int16 row, col; int8 type; int32 len;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&type, sizeof(type)) != (ssize_t)sizeof(type)
					|| source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
					return B_BAD_DATA;
				if (!cell(col, row).IsValid())
					return B_BAD_DATA;
				if (len < 0 || len > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string list;
				if (len > 0)
				{
					list.resize(len);
					if (source->Read(&list[0], len) != len)
						return B_BAD_DATA;
				}

				double minV, maxV;
				if (source->Read(&minV, sizeof(minV)) != (ssize_t)sizeof(minV)
					|| source->Read(&maxV, sizeof(maxV)) != (ssize_t)sizeof(maxV))
					return B_BAD_DATA;

				ValidationRule rule;
				rule.type = (ValidationType)type;
				rule.list = list;
				rule.min = minV;
				rule.max = maxV;
				doc->SetValidation(cell(col, row), rule);
			}
		}
	}
	{
		// Formattazione condizionale: (int8 tipo, int32 valueLen, valueLen
		// byte, rgb_color 4 byte, int32 rangeCount, rangeCount * 4 int16).
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int8 type; int32 valueLen;
				if (source->Read(&type, sizeof(type)) != (ssize_t)sizeof(type)
					|| source->Read(&valueLen, sizeof(valueLen)) != (ssize_t)sizeof(valueLen))
					return B_BAD_DATA;
				if (valueLen < 0 || valueLen > 16 * 1024 * 1024)
					return B_BAD_DATA;
				if (valueLen > 0 && source->Seek(valueLen, SEEK_CUR) < 0)
					return B_BAD_DATA;
				uint8 bgColorBuf[4];
				int32 rangeCount;
				if (source->Read(bgColorBuf, sizeof(bgColorBuf)) != (ssize_t)sizeof(bgColorBuf)
					|| source->Read(&rangeCount, sizeof(rangeCount)) != (ssize_t)sizeof(rangeCount))
					return B_BAD_DATA;
				for (int32 r = 0; r < rangeCount; r++)
				{
					int16 left, top, right, bottom;
					if (source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
						|| source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
						|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right)
						|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
						return B_BAD_DATA;
				}
			}
		}
	}
	{
		// Tabelle strutturate: (int32 nameLen, nameLen byte, 4 int16
		// range, int32 columnCount, per colonna: int32 colLen + colLen byte).
		int32 count = 0;
		ssize_t got = source->Read(&count, sizeof(count));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(count)) return B_BAD_DATA;
			for (int32 i = 0; i < count; i++)
			{
				int32 nameLen;
				if (source->Read(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
					return B_BAD_DATA;
				if (nameLen < 0 || nameLen > 16 * 1024 * 1024)
					return B_BAD_DATA;
				if (nameLen > 0 && source->Seek(nameLen, SEEK_CUR) < 0)
					return B_BAD_DATA;

				int16 left, top, right, bottom;
				if (source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right)
					|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
					return B_BAD_DATA;

				int32 columnCount;
				if (source->Read(&columnCount, sizeof(columnCount)) != (ssize_t)sizeof(columnCount))
					return B_BAD_DATA;
				if (columnCount < 0 || columnCount > kColCount)
					return B_BAD_DATA;
				for (int32 c = 0; c < columnCount; c++)
				{
					int32 colLen;
					if (source->Read(&colLen, sizeof(colLen)) != (ssize_t)sizeof(colLen))
						return B_BAD_DATA;
					if (colLen < 0 || colLen > 16 * 1024 * 1024)
						return B_BAD_DATA;
					if (colLen > 0 && source->Seek(colLen, SEEK_CUR) < 0)
						return B_BAD_DATA;
				}
			}
		}
	}

	// Sezione titolo di grafico incorporato: ULTIMA sezione del
	// formato, assegna nello STESSO ordine di outCharts (vedi il
	// commento gemello in LoadASCD).
	{
		int32 chartTitleCount = 0;
		ssize_t got = source->Read(&chartTitleCount, sizeof(chartTitleCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(chartTitleCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < chartTitleCount; i++)
			{
				int32 len;
				if (source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
					return B_BAD_DATA;
				if (len < 0 || len > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string title;
				if (len > 0)
				{
					title.resize(len);
					if (source->Read(&title[0], len) != len)
						return B_BAD_DATA;
				}
				if (i < (int32)outCharts->size())
					(*outCharts)[i].title = title;
			}
		}
	}

	// Sezione area di stampa, in coda (Fase 29 di ui/src/AscdIO.cpp):
	// questo translator non usa ancora l'area di stampa, ma deve
	// comunque consumare i byte scritti da WriteASCD per restare
	// allineato al blocco del foglio successivo in un file multi-foglio
	// (vedi il commento gemello nel writer qui sopra). Tollerante a EOF
	// per restare compatibile con un file scritto prima di questa fase.
	{
		uint8 has = 0;
		ssize_t got = source->Read(&has, sizeof(has));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(has))
				return B_BAD_DATA;

			int16 top, left, bottom, right;
			if (source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
				|| source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
				|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
				|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right))
				return B_BAD_DATA;
		}
	}

	// Sezione margini/scala di "Imposta pagina", in coda (Fase 29):
	// stesso motivo e stesso principio EOF-tollerante della sezione
	// area di stampa appena sopra.
	{
		uint8 has = 0;
		ssize_t got = source->Read(&has, sizeof(has));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(has))
				return B_BAD_DATA;

			double marginTop, marginBottom, marginLeft, marginRight, scalePercent;
			int32 scaleMode;
			if (source->Read(&marginTop, sizeof(marginTop)) != (ssize_t)sizeof(marginTop)
				|| source->Read(&marginBottom, sizeof(marginBottom)) != (ssize_t)sizeof(marginBottom)
				|| source->Read(&marginLeft, sizeof(marginLeft)) != (ssize_t)sizeof(marginLeft)
				|| source->Read(&marginRight, sizeof(marginRight)) != (ssize_t)sizeof(marginRight)
				|| source->Read(&scaleMode, sizeof(scaleMode)) != (ssize_t)sizeof(scaleMode)
				|| source->Read(&scalePercent, sizeof(scalePercent)) != (ssize_t)sizeof(scalePercent))
				return B_BAD_DATA;
		}
	}

	// Sezione progetto VBA (XLSM, Fase 31), in coda: stesso schema
	// EOF-tollerante delle sezioni sopra -- vedi il commento gemello in
	// ui/src/AscdIO.cpp (LoadASCD).
	{
		uint8 has = 0;
		ssize_t got = source->Read(&has, sizeof(has));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(has))
				return B_BAD_DATA;

			int32 len = 0;
			if (has)
			{
				if (source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
					return B_BAD_DATA;
				if (len < 0 || len > 64 * 1024 * 1024)
					return B_BAD_DATA;
			}

			if (has && len > 0)
			{
				std::vector<unsigned char> buf(len);
				if (source->Read(buf.data(), len) != len)
					return B_BAD_DATA;
				if (outVbaProject)
					*outVbaProject = buf;
			}
		}
	}

	// Sezione celle SBLOCCATE + protezione foglio (Fase 32), in coda. A
	// differenza delle sezioni colori/allineamento/bordi piu' sopra
	// (ancora scartate: questa esportazione non le riporta), il
	// blocco/la protezione VANNO
	// applicati a "doc"/restituiti al chiamante: e' lo scenario reale
	// piu' comune di questa funzione (MainWindow::SaveToFile per un
	// documento GIA' aperto da un vero file XLSX protetto -- vedi
	// tests/test_cell_protection.cpp), non solo un caso limite.
	{
		int32 unlockedCount = 0;
		ssize_t got = source->Read(&unlockedCount, sizeof(unlockedCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(unlockedCount))
				return B_BAD_DATA;
			for (int32 i = 0; i < unlockedCount; i++)
			{
				int16 row, col;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col))
					return B_BAD_DATA;

				cell loc(col, row);
				if (!loc.IsValid())
					return B_BAD_DATA;
				CellStyle cs;
				doc->GetCellStyle(loc, cs);
				cs.fLocked = false;
				doc->SetCellStyle(loc, cs);
			}
		}
	}
	{
		uint8 protectedByte = 0;
		ssize_t got = source->Read(&protectedByte, sizeof(protectedByte));
		if (got != 0 && got != (ssize_t)sizeof(protectedByte))
			return B_BAD_DATA;
		if (outIsProtected)
			*outIsProtected = protectedByte != 0;
	}

	// Sezione intervalli con nome, in coda, ULTIMA sezione del formato:
	// stesso schema EOF-tollerante delle sezioni sopra -- vedi il
	// commento gemello in ui/src/AscdIO.cpp (LoadASCD). A differenza
	// delle sezioni colori/allineamento/bordi piu' sopra (ancora
	// scartate), qui i nomi VANNO applicati a "doc": e' lo scenario
	// reale di questa funzione (ASCD -> XLSX, sia per un documento
	// nativo con nomi definiti sia per un file XLSX gia' importato con
	// <definedNames> e poi risalvato), non un caso limite.
	{
		int32 nameCount = 0;
		ssize_t got = source->Read(&nameCount, sizeof(nameCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(nameCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < nameCount; i++)
			{
				int32 nameLen = 0;
				if (source->Read(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
					return B_BAD_DATA;
				if (nameLen < 0 || nameLen > 1024)
					return B_BAD_DATA;

				std::string nameStr;
				if (nameLen > 0)
				{
					std::vector<char> buf(nameLen);
					if (source->Read(buf.data(), nameLen) != nameLen)
						return B_BAD_DATA;
					nameStr.assign(buf.data(), nameLen);
				}

				int16 top, left, bottom, right;
				if (source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
					|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right))
					return B_BAD_DATA;

				range r(left, top, right, bottom);
				if (!nameStr.empty() && r.IsValid())
					(*doc->GetOrCreateNameTable())[CName(nameStr.c_str())] = r;
			}
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

// Genera xl/worksheets/sheet1.xml a partire dal documento. Una cella
// con formula scrive sia <f> (la formula viva, in sintassi canonica
// ECMA-376: riferimenti A1, decimali con ".", argomenti separati da
// ",", indipendenti dalle preferenze locali dell'utente -- stesso
// principio dell'analisi di <f> in ingresso, vedi il commento in cima
// al parsing del foglio) sia <v> (il valore gia' calcolato, letto
// subito da chi apre il file senza dover ricalcolare) -- ma SOLO se
// la formula non fa riferimento a un altro foglio (vedi
// ReferencesOtherSheet): questo export scrive un solo foglio per
// file, un riferimento incrociato punterebbe a dati che nel file
// esportato non esistono affatto, quindi in quel caso resta il
// comportamento precedente (solo il valore calcolato, come CSV/ODS).
// Le stringhe sono scritte inline (t="inlineStr"/<is><t>...</t></is>)
// invece che in una tabella di stringhe condivise (xl/
// sharedStrings.xml): richiederebbe una passata separata per
// raccogliere i valori unici, complessita' non necessaria per i
// fogli tipici esportati da questo programma, ed e' comunque sintassi
// OOXML valida (Excel/LibreOffice la leggono correttamente).
static std::string BuildSheetXml(CContainer* doc, bool hasDrawing, bool isProtected,
	// Gia' pronto da WriteXLSX sopra: <dataValidations>...</dataValidations>
	// seguito da <hyperlinks>...</hyperlinks> (100% XLSX standard
	// compatibility, Tier 2), nell'ordine richiesto dallo schema OOXML
	// -- questa funzione lo inserisce solo al punto giusto, vedi sotto.
	const std::string& dataValidationAndHyperlinksXml = std::string(),
	// <sheetViews>...</sheetViews> (100% XLSX standard compatibility,
	// Tier 2), gia' pronto da WriteXLSX sopra -- va SUBITO dopo il tag
	// di apertura <worksheet>, PRIMA di <sheetData>, per lo schema
	// OOXML (CT_Worksheet); vuoto quando non c'e' nessun riquadro
	// bloccato da scrivere.
	const std::string& sheetViewsXml = std::string())
{
	range bounds;
	doc->GetBounds(bounds);

	std::string xml;
	xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	// xmlns:r sempre presente (anche senza grafici): serve solo per
	// l'attributo r:id di <drawing> sotto, innocuo da dichiarare anche
	// quando non usato.
	xml += "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
		"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";
	xml += sheetViewsXml;
	xml += "<sheetData>";

	CCellIterator iter(doc, &bounds);
	cell c;
	int curRow = -1;
	char numBuf[64];
	char nameBuf[16];
	// 16384: stessa dimensione di kMaxUnmangledFormulaLength in
	// Container.cpp (privata a quel file, non esposta da un header) --
	// generosa rispetto a qualunque formula reale.
	char formulaBuf[16384];
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

		// Blocco cella (Fase 32): "s=\"1\"" referenzia il secondo (indice
		// 1) xf di xl/styles.xml, l'UNICO che ha <protection locked="0"/>
		// -- vedi il commento su kStyles sotto in WriteXLSX. L'indice 0
		// (nessun attributo s="...") e' implicitamente bloccato, stessa
		// convenzione del default di CellStyle::fLocked.
		CellStyle cellStyle;
		doc->GetCellStyle(c, cellStyle);
		bool unlocked = !cellStyle.fLocked;

		bool writeFormula = false;
		void* rawFormula = doc->GetCellFormula(c);
		if (rawFormula)
		{
			CFormula form(rawFormula);
			if (!form.ReferencesOtherSheet())
			{
				form.UnMangle(formulaBuf, c, doc, false, '.', ',');
				writeFormula = true;
			}
		}

		if (v.fType == eNumData)
		{
			snprintf(numBuf, sizeof(numBuf), "%.15g", (double)v);
			xml += "<c r=\"";
			xml += nameBuf;
			if (unlocked)
				xml += "\" s=\"1";
			xml += "\">";
			if (writeFormula)
			{
				xml += "<f>";
				AppendXmlEscaped(xml, formulaBuf);
				xml += "</f>";
			}
			xml += "<v>";
			xml += numBuf;
			xml += "</v></c>";
		}
		else
		{
			xml += "<c r=\"";
			xml += nameBuf;
			if (unlocked)
				xml += "\" s=\"1";
			xml += "\" t=\"";
			xml += writeFormula ? "str" : "inlineStr";
			xml += "\">";
			if (writeFormula)
			{
				xml += "<f>";
				AppendXmlEscaped(xml, formulaBuf);
				xml += "</f><v>";
				AppendXmlEscaped(xml, (const char*)v);
				xml += "</v></c>";
			}
			else
			{
				xml += "<is><t>";
				AppendXmlEscaped(xml, (const char*)v);
				xml += "</t></is></c>";
			}
		}
	}
	if (curRow != -1)
		xml += "</row>";

	xml += "</sheetData>";
	// <sheetProtection/> (Fase 32): la sola presenza vuol dire "foglio
	// protetto" per Excel -- va DOPO </sheetData> e PRIMA di <drawing>
	// nell'ordine richiesto dallo schema OOXML (CT_Worksheet). Nessun
	// attributo di password: questo formato non ne ha uno da esportare
	// (l'app non protegge mai con password, solo on/off).
	if (isProtected)
		xml += "<sheetProtection sheetId=\"1\"/>";
	// <dataValidations>/<hyperlinks> (100% XLSX standard compatibility,
	// Tier 2): DOPO <sheetProtection>, PRIMA di <drawing> nell'ordine
	// richiesto dallo schema OOXML (CT_Worksheet).
	xml += dataValidationAndHyperlinksXml;
	// <drawing> e' un fratello di <sheetData> (mai al suo interno),
	// deve venire DOPO nello schema OOXML del foglio -- ancora i grafici
	// incorporati (Fase 24) tramite xl/drawings/drawing1.xml, sempre
	// r:id="rId1" perche' e' l'unica relazione aggiuntiva che questo
	// foglio puo' avere (vedi il rels scritto in WriteXLSX sotto).
	if (hasDrawing)
		xml += "<drawing r:id=\"rId1\"/>";
	xml += "</worksheet>";
	return xml;
}

// --- Esportazione dei grafici incorporati verso XLSX (Fase 24) ------
//
// Prima di questo lavoro i grafici creati in Atomo123 non venivano
// esportati affatto: WriteXLSX scriveva solo i dati delle celle,
// nessuna parte xl/charts/xl/drawings, quindi un grafico Atomo123
// spariva del tutto aprendo il file in Excel/LibreOffice. Qui sotto:
// estrazione degli stessi dati che l'app disegnerebbe (stessa logica
// di BuildChartSeries/BuildMultiChartSeries in ui/src/Chart.cpp, MAI
// quel file incluso qui, vedi XlsxChartInfo sopra) e generazione delle
// parti OOXML DrawingML necessarie (c:chart, drawing con ancoraggio
// assoluto in EMU -- ChartObject::frame e' gia' un rettangolo in
// pixel fisso, un ancoraggio assoluto evita di dover indovinare
// larghezze di colonna/altezze di riga per convertirlo in celle+
// scarto come farebbe un ancoraggio relativo).

// Lettere di colonna Excel (1 = "A", 26 = "Z", 27 = "AA", ...) --
// evita di dover ricavare la parte lettera da cell::GetName() con
// find_first_of/substr (fragile, ripetuto piu' volte sotto): qui si
// costruisce direttamente il riferimento assoluto "$COL$riga" da
// colonna/riga gia' numeriche.
static std::string ColumnLetters(int col)
{
	std::string s;
	while (col > 0)
	{
		int rem = (col - 1) % 26;
		s = char('A' + rem) + s;
		col = (col - 1) / 26;
	}
	return s;
}

static std::string AbsCellRef(int col, int row)
{
	char rowBuf[16];
	snprintf(rowBuf, sizeof(rowBuf), "%d", row);
	return "$" + ColumnLetters(col) + "$" + rowBuf;
}

// "Foglio1!$B$2:$B$5" -- lo stesso riferimento assoluto di AbsCellRef,
// esteso su un intervallo verticale (stessa colonna, righe da topRow a
// bottomRow).
static std::string AbsColumnRangeRef(const char* sheetName, int col, int topRow, int bottomRow)
{
	return std::string(sheetName) + "!" + AbsCellRef(col, topRow) + ":" + AbsCellRef(col, bottomRow);
}

// "Foglio1!$A$1:$B$2", or just "Foglio1!$A$1" for a single cell
// (matching Excel's own convention of omitting the redundant
// ":$A$1" for a 1x1 named range) -- used when writing <definedName>
// on export.
static std::string AbsRangeRef(const char* sheetName, const range& r)
{
	std::string ref = std::string(sheetName) + "!" + AbsCellRef(r.left, r.top);
	if (r.left != r.right || r.top != r.bottom)
		ref += ":" + AbsCellRef(r.right, r.bottom);
	return ref;
}

static std::string FormatChartNumber(double v)
{
	char buf[64];
	snprintf(buf, sizeof(buf), "%.15g", v);
	return buf;
}

// Etichetta testuale di una cella, stessa logica di ValueToLabel in
// ui/src/Chart.cpp (testo cosi' com'e', numero con "%g" invece della
// piena precisione usata per i VALORI veri del grafico sopra -- qui
// e' solo un'etichetta mostrata, non un dato tracciato).
static std::string ChartCellLabel(CContainer* doc, int col, int row)
{
	Value v;
	doc->GetValue(cell(col, row), v);
	if (v.fType == eTextData)
		return (const char*)v;
	if (v.fType == eNumData)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%g", (double)v);
		return buf;
	}
	return std::string();
}

// Un grafico a UNA serie (ChartObject::dataRange a due colonne:
// etichetta, valore) -- stessa identica logica di BuildChartSeries:
// ogni riga da dataTop a dataBottom e' un punto, saltato se la
// colonna valore non e' numerica (esclude gia' da sola un'eventuale
// riga di intestazione testuale, senza bisogno di rilevarla a parte).
struct SimpleChartData {
	std::vector<std::string> labels;
	std::vector<double> values;
};

static bool BuildSimpleChartData(CContainer* doc, const XlsxChartInfo& info, SimpleChartData* out)
{
	out->labels.clear();
	out->values.clear();
	if (info.dataRight - info.dataLeft != 1)
		return false;

	for (int row = info.dataTop; row <= info.dataBottom; row++)
	{
		Value vv;
		doc->GetValue(cell(info.dataLeft + 1, row), vv);
		if (vv.fType != eNumData)
			continue;
		out->labels.push_back(ChartCellLabel(doc, info.dataLeft, row));
		out->values.push_back((double)vv);
	}
	return !out->values.empty();
}

// Un grafico a PIU' serie -- stessa identica logica di
// BuildMultiChartSeries (rilevamento della riga di intestazione
// facoltativa compreso).
struct MultiChartDataXlsx {
	std::vector<std::string> categories;
	std::vector<std::string> seriesNames;
	std::vector<std::vector<double> > values; // values[serie][categoria]
	int firstDataRow;
};

static bool BuildMultiChartDataXlsx(CContainer* doc, const XlsxChartInfo& info, MultiChartDataXlsx* out)
{
	out->categories.clear();
	out->seriesNames.clear();
	out->values.clear();

	int seriesCount = info.dataRight - info.dataLeft;
	if (seriesCount < 1)
		return false;
	out->values.resize(seriesCount);

	bool hasHeader = false;
	for (int s = 0; s < seriesCount && !hasHeader; s++)
	{
		Value hv;
		doc->GetValue(cell(info.dataLeft + 1 + s, info.dataTop), hv);
		if (hv.fType == eTextData && ((const char*)hv)[0] != 0)
			hasHeader = true;
	}

	for (int s = 0; s < seriesCount; s++)
	{
		std::string name;
		if (hasHeader)
		{
			Value hv;
			doc->GetValue(cell(info.dataLeft + 1 + s, info.dataTop), hv);
			if (hv.fType == eTextData)
				name = (const char*)hv;
		}
		if (name.empty())
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "Serie %d", s + 1);
			name = buf;
		}
		out->seriesNames.push_back(name);
	}

	out->firstDataRow = hasHeader ? info.dataTop + 1 : info.dataTop;
	for (int row = out->firstDataRow; row <= info.dataBottom; row++)
	{
		std::vector<double> rowValues(seriesCount);
		bool rowOk = true;
		for (int s = 0; s < seriesCount && rowOk; s++)
		{
			Value vv;
			doc->GetValue(cell(info.dataLeft + 1 + s, row), vv);
			if (vv.fType != eNumData)
				rowOk = false;
			else
				rowValues[s] = (double)vv;
		}
		if (!rowOk)
			continue;

		out->categories.push_back(ChartCellLabel(doc, info.dataLeft, row));
		for (int s = 0; s < seriesCount; s++)
			out->values[s].push_back(rowValues[s]);
	}

	return !out->categories.empty();
}

static void AppendStrCache(std::string& xml, const std::vector<std::string>& labels)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%zu", labels.size());
	xml += "<c:strCache><c:ptCount val=\"";
	xml += buf;
	xml += "\"/>";
	for (size_t i = 0; i < labels.size(); i++)
	{
		snprintf(buf, sizeof(buf), "%zu", i);
		xml += "<c:pt idx=\"";
		xml += buf;
		xml += "\"><c:v>";
		AppendXmlEscaped(xml, labels[i].c_str());
		xml += "</c:v></c:pt>";
	}
	xml += "</c:strCache>";
}

static void AppendNumCache(std::string& xml, const std::vector<double>& values)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%zu", values.size());
	xml += "<c:numCache><c:formatCode>General</c:formatCode><c:ptCount val=\"";
	xml += buf;
	xml += "\"/>";
	for (size_t i = 0; i < values.size(); i++)
	{
		xml += "<c:pt idx=\"";
		snprintf(buf, sizeof(buf), "%zu", i);
		xml += buf;
		xml += "\"><c:v>";
		xml += FormatChartNumber(values[i]);
		xml += "</c:v></c:pt>";
	}
	xml += "</c:numCache>";
}

// Una serie completa (<c:ser>): nome letterale (mai un riferimento a
// cella, anche quando la serie ha un nome preso da un'intestazione
// vera -- evita di dover ricostruire quel riferimento per il caso,
// altrettanto comune, di un nome "Serie N" senza nessuna cella
// sorgente), categorie/valori invece SEMPRE con riferimento vivo
// (<c:f>) piu' la cache -- cosi' Excel/LibreOffice possono ricalcolare
// il grafico se i dati cambiano, non solo mostrare l'istantanea.
static void AppendSeries(std::string& xml, int idx, const std::string& seriesName,
	const std::string& catRef, const std::vector<std::string>& categories,
	const std::string& valRef, const std::vector<double>& values, bool withTx)
{
	char buf[32];
	xml += "<c:ser><c:idx val=\"";
	snprintf(buf, sizeof(buf), "%d", idx);
	xml += buf;
	xml += "\"/><c:order val=\"";
	xml += buf;
	xml += "\"/>";
	if (withTx)
	{
		xml += "<c:tx><c:v>";
		AppendXmlEscaped(xml, seriesName.c_str());
		xml += "</c:v></c:tx>";
	}
	xml += "<c:cat><c:strRef><c:f>";
	AppendXmlEscaped(xml, catRef.c_str());
	xml += "</c:f>";
	AppendStrCache(xml, categories);
	xml += "</c:strRef></c:cat>";
	xml += "<c:val><c:numRef><c:f>";
	AppendXmlEscaped(xml, valRef.c_str());
	xml += "</c:f>";
	AppendNumCache(xml, values);
	xml += "</c:numRef></c:val></c:ser>";
}

// Genera xl/charts/chartN.xml per un singolo ChartObject -- restituisce
// una stringa vuota se l'intervallo dati non produce nessun punto
// valido (stesso caso in cui l'app stessa non disegna nulla, vedi
// SheetView::Draw), il chiamante lo salta senza scrivere nessuna parte
// per quel grafico.
static std::string BuildChartXml(CContainer* doc, const XlsxChartInfo& info)
{
	static const char kSheetName[] = "Foglio1";

	bool multiSeries = (info.dataRight - info.dataLeft > 1) && info.type != 2;

	std::string plot;
	int seriesCountForLegend = 0;

	if (multiSeries)
	{
		MultiChartDataXlsx data;
		if (!BuildMultiChartDataXlsx(doc, info, &data))
			return std::string();

		std::string catRef = AbsColumnRangeRef(kSheetName, info.dataLeft,
			data.firstDataRow, info.dataBottom);

		for (int s = 0; s < (int)data.seriesNames.size(); s++)
		{
			std::string valRef = AbsColumnRangeRef(kSheetName, info.dataLeft + 1 + s,
				data.firstDataRow, info.dataBottom);
			AppendSeries(plot, s, data.seriesNames[s], catRef, data.categories,
				valRef, data.values[s], true);
		}
		seriesCountForLegend = (int)data.seriesNames.size();
	}
	else
	{
		SimpleChartData data;
		if (!BuildSimpleChartData(doc, info, &data))
			return std::string();

		std::string catRef = AbsColumnRangeRef(kSheetName, info.dataLeft,
			info.dataTop, info.dataBottom);
		std::string valRef = AbsColumnRangeRef(kSheetName, info.dataLeft + 1,
			info.dataTop, info.dataBottom);

		AppendSeries(plot, 0, "", catRef, data.labels, valRef, data.values, false);
		seriesCountForLegend = 1;
	}

	if (plot.empty())
		return std::string();

	std::string xml;
	xml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
	xml += "<c:chartSpace xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
		"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
		"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">";
	xml += "<c:chart>";

	if (info.title.empty())
		xml += "<c:autoTitleDeleted val=\"1\"/>";
	else
	{
		xml += "<c:title><c:tx><c:rich><a:bodyPr/><a:p><a:r><a:t>";
		AppendXmlEscaped(xml, info.title.c_str());
		xml += "</a:t></a:r></a:p></c:rich></c:tx><c:overlay val=\"0\"/></c:title>";
		xml += "<c:autoTitleDeleted val=\"0\"/>";
	}

	xml += "<c:plotArea><c:layout/>";

	if (info.type == 2) // torta
	{
		xml += "<c:pieChart><c:varyColors val=\"1\"/>";
		xml += plot;
		xml += "<c:firstSliceAng val=\"0\"/></c:pieChart>";
	}
	else if (info.type == 1) // linee
	{
		xml += "<c:lineChart><c:grouping val=\"standard\"/><c:varyColors val=\"0\"/>";
		xml += plot;
		xml += "<c:marker val=\"1\"/>";
		xml += "<c:axId val=\"111111111\"/><c:axId val=\"222222222\"/></c:lineChart>";
		xml += "<c:catAx><c:axId val=\"111111111\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling>"
			"<c:delete val=\"0\"/><c:axPos val=\"b\"/><c:crossAx val=\"222222222\"/></c:catAx>";
		xml += "<c:valAx><c:axId val=\"222222222\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling>"
			"<c:delete val=\"0\"/><c:axPos val=\"l\"/><c:crossAx val=\"111111111\"/></c:valAx>";
	}
	else // barre (predefinito)
	{
		xml += "<c:barChart><c:barDir val=\"col\"/><c:grouping val=\"clustered\"/><c:varyColors val=\"0\"/>";
		xml += plot;
		xml += "<c:axId val=\"111111111\"/><c:axId val=\"222222222\"/></c:barChart>";
		xml += "<c:catAx><c:axId val=\"111111111\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling>"
			"<c:delete val=\"0\"/><c:axPos val=\"b\"/><c:crossAx val=\"222222222\"/></c:catAx>";
		xml += "<c:valAx><c:axId val=\"222222222\"/><c:scaling><c:orientation val=\"minMax\"/></c:scaling>"
			"<c:delete val=\"0\"/><c:axPos val=\"l\"/><c:crossAx val=\"111111111\"/></c:valAx>";
	}

	xml += "</c:plotArea>";

	// Legenda: solo se ha davvero senso (piu' serie, o una torta le cui
	// fette si distinguono per colore -- mai per un grafico a barre/
	// linee a una sola serie, che non ne ha bisogno, stesso principio
	// gia' seguito da DrawBarChart/DrawLineChart nell'app).
	if (info.type == 2 || seriesCountForLegend > 1)
		xml += "<c:legend><c:legendPos val=\"r\"/><c:overlay val=\"0\"/></c:legend>";

	xml += "<c:plotVisOnly val=\"1\"/></c:chart></c:chartSpace>";
	return xml;
}

static status_t WriteXLSX(CContainer* doc, const std::vector<XlsxChartInfo>& charts, BPositionIO* dest,
	const std::vector<unsigned char>& vbaProject = std::vector<unsigned char>(),
	// Protezione foglio (Fase 32): vedi il commento gemello in
	// ui/src/AscdIO.h. false di default, come vbaProject sopra.
	bool isProtected = false,
	// Blocca riquadri (100% XLSX standard compatibility, Tier 2): 0,0
	// di default (nessun riquadro bloccato), come ogni altro parametro
	// opzionale qui sopra.
	int frozenRows = 0, int frozenCols = 0)
{
	// Presenza di un progetto VBA (XLSM, Fase 31): un file .xlsx puro
	// non ha mai xl/vbaProject.bin, quindi "hasMacros" e' sempre false
	// per ogni chiamante che non lo passa esplicitamente (compatibile
	// con ogni chiamata precedente a questo parametro). Il tipo MIME
	// del foglio di lavoro principale CAMBIA in presenza di macro
	// (application/vnd.ms-excel.sheet.macroEnabled.main+xml invece di
	// .../spreadsheetml.sheet.main+xml): Excel usa QUESTO per decidere
	// se fidarsi delle macro, non solo l'estensione del nome file --
	// scriverlo comunque sotto un nome ".xlsx" produrrebbe un file che
	// Excel apre riparandolo (macro perse comunque), MainWindow::
	// SaveToFile passa "vbaProject" solo quando l'utente salva con
	// estensione ".xlsm" apposta per questo.
	bool hasMacros = !vbaProject.empty();

	static const char kRootRels[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>\n"
		"</Relationships>\n";
	static const char kWorkbookHeader[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
		"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n"
		"<sheets><sheet name=\"Foglio1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>\n";

	// <definedNames> (named ranges, "100% XLSX standard compatibility"
	// plan, see ROADMAP.md): CContainer::GetNameTable(), written as
	// workbook-scoped (no localSheetId -- this export is single-sheet
	// only, nothing to disambiguate). Skipped entirely when the
	// document has no names defined, matching how every other
	// optional part of this export is only added when it has content.
	std::string definedNamesXml;
	{
		CNameTable* names = doc->GetNameTable();
		if (names && !names->empty())
		{
			definedNamesXml = "<definedNames>";
			for (CNameTable::const_iterator it = names->begin(); it != names->end(); ++it)
			{
				definedNamesXml += "<definedName name=\"";
				AppendXmlEscaped(definedNamesXml, (const char*)it->first);
				definedNamesXml += "\">" + AbsRangeRef("Foglio1", it->second) + "</definedName>";
			}
			definedNamesXml += "</definedNames>\n";
		}
	}

	std::string workbookXmlOut = std::string(kWorkbookHeader) + definedNamesXml + "</workbook>\n";

	// xl/styles.xml (rId2) e' sempre presente (vedi kStyles sotto), la
	// relazione verso xl/vbaProject.bin (rId3) va aggiunta SOLO in
	// presenza di macro: un file .xlsx normale non deve avere una
	// relazione verso una parte che non scrive.
	std::string workbookRels;
	workbookRels += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
		"<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>\n"
		"<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>\n";
	if (hasMacros)
		workbookRels += "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/vbaProject\" Target=\"vbaProject.bin\"/>\n";
	workbookRels += "</Relationships>\n";

	// Costruisce prima ogni xl/charts/chartN.xml: un ChartObject il cui
	// intervallo dati non produce nessun punto valido (stesso caso in
	// cui l'app stessa non disegnerebbe nulla) viene saltato in
	// silenzio, non e' un errore di esportazione. "usedCharts" tiene
	// l'XlsxChartInfo di ogni grafico DAVVERO scritto, nello stesso
	// ordine/indice di chartXmls -- necessario perche' uno scarto puo'
	// capitare in mezzo all'elenco, non solo in coda: usare "charts[i]"
	// direttamente piu' sotto disallineerebbe posizione/dimensione di
	// ogni grafico successivo a quello scartato.
	std::vector<std::string> chartXmls;
	std::vector<XlsxChartInfo> usedCharts;
	for (size_t i = 0; i < charts.size(); i++)
	{
		std::string xml = BuildChartXml(doc, charts[i]);
		if (!xml.empty())
		{
			chartXmls.push_back(xml);
			usedCharts.push_back(charts[i]);
		}
	}

	bool hasDrawing = !chartXmls.empty();

	// xl/comments1.xml (100% XLSX standard compatibility, Tier 2):
	// CContainer::GetComments() -> un solo <author> generico (questo
	// motore non ha un concetto di autore per commento) piu' un
	// <comment ref="A1" authorId="0"><text><t>...</t></text></comment>
	// per cella. Nessun VML legacy scritto (vedi il commento gemello
	// nel ramo di importazione, Translate sopra): il contenuto
	// sopravvive comunque, solo la posizione/visibilita' del riquadro
	// che Excel disegnerebbe in piu' non viene replicata.
	const std::map<cell, std::string>& comments = doc->GetComments();
	bool hasComments = !comments.empty();
	std::string commentsXml;
	if (hasComments)
	{
		commentsXml += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<comments xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
			"<authors><author>Atomo123</author></authors><commentList>";
		for (std::map<cell, std::string>::const_iterator it = comments.begin();
			it != comments.end(); ++it)
		{
			char ref[32];
			it->first.GetName(ref);
			commentsXml += "<comment ref=\"";
			commentsXml += ref;
			commentsXml += "\" authorId=\"0\"><text><t xml:space=\"preserve\">";
			AppendXmlEscaped(commentsXml, it->second.c_str());
			commentsXml += "</t></text></comment>";
		}
		commentsXml += "</commentList></comments>\n";
	}

	// <dataValidations> dentro <worksheet> (100% XLSX standard
	// compatibility, Tier 2): CContainer::GetValidations() -> un
	// <dataValidation> per cella (stesso principio "una voce per
	// cella" dei commenti sopra, niente raggruppamento di celle con la
	// stessa regola in un solo sqref multiplo -- piu' semplice, e
	// comunque OOXML valido). Solo le due forme che questo motore
	// modella davvero (vedi ValidationRule in Container.h): elenco
	// letterale fra virgolette per eListValidation, "whole"/operator
	// "between" per eNumberRangeValidation (Excel tratta "whole" come
	// intero, ma un limite non intero e' comunque valido da leggere;
	// non c'e' un secondo tipo "decimal" da scegliere qui perche' il
	// motore non distingue le due cose).
	const std::map<cell, ValidationRule>& validations = doc->GetValidations();
	bool hasValidations = !validations.empty();
	std::string dataValidationXml;
	if (hasValidations)
	{
		dataValidationXml = "<dataValidations count=\"";
		char countBuf[16];
		snprintf(countBuf, sizeof(countBuf), "%zu", validations.size());
		dataValidationXml += countBuf;
		dataValidationXml += "\">";
		for (std::map<cell, ValidationRule>::const_iterator it = validations.begin();
			it != validations.end(); ++it)
		{
			char ref[32];
			it->first.GetName(ref);
			const ValidationRule& rule = it->second;
			if (rule.type == eListValidation)
			{
				dataValidationXml += "<dataValidation type=\"list\" allowBlank=\"1\" "
					"showInputMessage=\"1\" showErrorMessage=\"1\" sqref=\"";
				dataValidationXml += ref;
				dataValidationXml += "\"><formula1>\"";
				AppendXmlEscaped(dataValidationXml, rule.list.c_str());
				dataValidationXml += "\"</formula1></dataValidation>";
			}
			else if (rule.type == eNumberRangeValidation)
			{
				char minBuf[64], maxBuf[64];
				snprintf(minBuf, sizeof(minBuf), "%.15g", rule.min);
				snprintf(maxBuf, sizeof(maxBuf), "%.15g", rule.max);
				dataValidationXml += "<dataValidation type=\"whole\" operator=\"between\" "
					"allowBlank=\"1\" showInputMessage=\"1\" showErrorMessage=\"1\" sqref=\"";
				dataValidationXml += ref;
				dataValidationXml += "\"><formula1>";
				dataValidationXml += minBuf;
				dataValidationXml += "</formula1><formula2>";
				dataValidationXml += maxBuf;
				dataValidationXml += "</formula2></dataValidation>";
			}
		}
		dataValidationXml += "</dataValidations>";
	}

	// <hyperlinks> dentro <worksheet> (100% XLSX standard compatibility,
	// Tier 2): CContainer::GetHyperlinks() -> un <hyperlink ref="A1"
	// r:id="rIdX"/> per cella, sempre un collegamento ESTERNO
	// (TargetMode="External" nel .rels sotto, mai "location" per un
	// riferimento interno: questo motore memorizza solo una stringa
	// per collegamento, senza distinguere le due forme). Gli Id vanno
	// assegnati DOPO quello del drawing/dei commenti (se presenti),
	// visto che sono relazioni nello stesso file .rels del foglio --
	// "hyperlinkRidStart" e' il primo libero.
	const std::map<cell, std::string>& links = doc->GetHyperlinks();
	bool hasHyperlinks = !links.empty();
	int hyperlinkRidStart = (hasDrawing ? 1 : 0) + (hasComments ? 1 : 0) + 1;
	std::string hyperlinksXml;
	if (hasHyperlinks)
	{
		hyperlinksXml = "<hyperlinks>";
		int rid = hyperlinkRidStart;
		for (std::map<cell, std::string>::const_iterator it = links.begin();
			it != links.end(); ++it, ++rid)
		{
			char ref[32];
			it->first.GetName(ref);
			char buf[16];
			snprintf(buf, sizeof(buf), "rId%d", rid);
			hyperlinksXml += "<hyperlink ref=\"";
			hyperlinksXml += ref;
			hyperlinksXml += "\" r:id=\"";
			hyperlinksXml += buf;
			hyperlinksXml += "\"/>";
		}
		hyperlinksXml += "</hyperlinks>";
	}

	// <sheetViews><sheetView><pane .../></sheetView></sheetViews> (100%
	// XLSX standard compatibility, Tier 2): solo quando c'e' davvero un
	// riquadro bloccato -- questo export non scriveva <sheetViews> per
	// nessun motivo prima d'ora. "state=\"frozen\"" e' cio' che distingue
	// un vero blocco riquadri da uno split trascinabile (dove xSplit/
	// ySplit sarebbero ventesimi di punto, non un numero di righe/
	// colonne) -- vedi il commento gemello nel ramo di importazione.
	// "topLeftCell" e' la prima cella VISIBILE nella zona scorrevole
	// (subito dopo l'ultima riga/colonna bloccata), "activePane"
	// indica quale dei quattro riquadri ha il focus, per convenzione lo
	// stesso che Excel sceglie: in basso a destra se sono bloccate sia
	// righe che colonne, altrimenti il riquadro opposto al lato bloccato.
	std::string sheetViewsXml;
	if (frozenRows > 0 || frozenCols > 0)
	{
		char topLeft[32];
		cell(frozenCols + 1, frozenRows + 1).GetName(topLeft);
		const char* activePane = (frozenCols > 0 && frozenRows > 0) ? "bottomRight"
			: (frozenCols > 0) ? "topRight" : "bottomLeft";
		char buf[256];
		snprintf(buf, sizeof(buf),
			"<sheetViews><sheetView tabSelected=\"1\" workbookViewId=\"0\">"
			"<pane xSplit=\"%d\" ySplit=\"%d\" topLeftCell=\"%s\" activePane=\"%s\" state=\"frozen\"/>"
			"</sheetView></sheetViews>",
			frozenCols, frozenRows, topLeft, activePane);
		sheetViewsXml = buf;
	}

	std::string sheet = BuildSheetXml(doc, hasDrawing, isProtected,
		dataValidationXml + hyperlinksXml, sheetViewsXml);

	// xl/styles.xml (Fase 32): finora questo export non scriveva NESSUNO
	// stile (solo valori/formule, vedi BuildSheetXml) -- una vera tabella
	// stili completa (colori/font/bordi/formati) resta fuori scopo qui,
	// ma il blocco cella e' cosi' semplice (un solo bit) da non
	// richiederla: due sole voci <xf>, la seconda (indice 1, referenziata
	// da "s=\"1\"" sulle celle sbloccate in BuildSheetXml) con
	// <protection locked="0"/>. Il boilerplate fonts/fills/borders resta
	// il minimo che Excel accetta come styles.xml valido (fills conta
	// SEMPRE almeno "none" e "gray125" anche se inutilizzati, per
	// convenzione OOXML).
	static const char kStyles[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n"
		"<fonts count=\"1\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>\n"
		"<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
		"<fill><patternFill patternType=\"gray125\"/></fill></fills>\n"
		"<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>\n"
		"<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>\n"
		"<cellXfs count=\"2\">"
		"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
		"<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"><protection locked=\"0\"/></xf>"
		"</cellXfs>\n"
		"</styleSheet>\n";

	std::string contentTypes;
	contentTypes += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
		"<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
		"<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n"
		"<Default Extension=\"xml\" ContentType=\"application/xml\"/>\n";
	if (hasMacros)
		contentTypes += "<Default Extension=\"bin\" ContentType=\"application/vnd.ms-office.vbaProject\"/>\n"
			"<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.ms-excel.sheet.macroEnabled.main+xml\"/>\n";
	else
		contentTypes += "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n";
	contentTypes += "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n";
	contentTypes += "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n";
	if (hasDrawing)
	{
		contentTypes += "<Override PartName=\"/xl/drawings/drawing1.xml\" "
			"ContentType=\"application/vnd.openxmlformats-officedocument.drawing+xml\"/>\n";
		for (size_t i = 0; i < chartXmls.size(); i++)
		{
			char buf[192];
			snprintf(buf, sizeof(buf),
				"<Override PartName=\"/xl/charts/chart%zu.xml\" "
				"ContentType=\"application/vnd.openxmlformats-officedocument.drawingml.chart+xml\"/>\n",
				i + 1);
			contentTypes += buf;
		}
	}
	if (hasComments)
		contentTypes += "<Override PartName=\"/xl/comments1.xml\" "
			"ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.comments+xml\"/>\n";
	contentTypes += "</Types>\n";

	CZipWriter zip;
	zip.Begin(dest);

	if (!zip.AddEntry("[Content_Types].xml", contentTypes.data(), contentTypes.size()))
		return B_IO_ERROR;
	if (!zip.AddEntry("_rels/.rels", kRootRels, strlen(kRootRels)))
		return B_IO_ERROR;
	if (!zip.AddEntry("xl/workbook.xml", workbookXmlOut.data(), workbookXmlOut.size()))
		return B_IO_ERROR;
	if (!zip.AddEntry("xl/_rels/workbook.xml.rels", workbookRels.data(), workbookRels.size()))
		return B_IO_ERROR;
	if (!zip.AddEntry("xl/worksheets/sheet1.xml", sheet.data(), sheet.size()))
		return B_IO_ERROR;
	if (!zip.AddEntry("xl/styles.xml", kStyles, strlen(kStyles)))
		return B_IO_ERROR;

	if (hasMacros)
	{
		if (!zip.AddEntry("xl/vbaProject.bin", vbaProject.data(), vbaProject.size()))
			return B_IO_ERROR;
	}

	if (hasDrawing || hasComments || hasHyperlinks)
	{
		// Il foglio si collega al drawing tramite rId1 (vedi
		// <drawing r:id="rId1"/> scritto da BuildSheetXml sopra, solo in
		// presenza di grafici), ai commenti tramite l'rId successivo, e
		// a ogni collegamento ipertestuale tramite gli rId da
		// "hyperlinkRidStart" in poi (stesso ordine di iterazione su
		// "links" usato sopra per costruire hyperlinksXml) -- nessun
		// elemento equivalente nel foglio stesso per i commenti, si
		// trovano solo tramite questa relazione (vedi il commento sul
		// VML legacy piu' sopra); i collegamenti invece HANNO un
		// elemento (<hyperlink r:id="..."/> in hyperlinksXml) che
		// referenzia questi stessi Id.
		std::string sheetRels;
		sheetRels += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";
		if (hasDrawing)
			sheetRels += "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing\" "
				"Target=\"../drawings/drawing1.xml\"/>\n";
		if (hasComments)
		{
			sheetRels += hasDrawing
				? "<Relationship Id=\"rId2\" "
				: "<Relationship Id=\"rId1\" ";
			sheetRels += "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments\" "
				"Target=\"../comments1.xml\"/>\n";
		}
		if (hasHyperlinks)
		{
			int rid = hyperlinkRidStart;
			for (std::map<cell, std::string>::const_iterator it = links.begin();
				it != links.end(); ++it, ++rid)
			{
				char idBuf[16];
				snprintf(idBuf, sizeof(idBuf), "rId%d", rid);
				sheetRels += "<Relationship Id=\"";
				sheetRels += idBuf;
				sheetRels += "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink\" "
					"Target=\"";
				AppendXmlEscaped(sheetRels, it->second.c_str());
				sheetRels += "\" TargetMode=\"External\"/>\n";
			}
		}
		sheetRels += "</Relationships>\n";
		if (!zip.AddEntry("xl/worksheets/_rels/sheet1.xml.rels", sheetRels.data(), sheetRels.size()))
			return B_IO_ERROR;
	}

	if (hasComments)
	{
		if (!zip.AddEntry("xl/comments1.xml", commentsXml.data(), commentsXml.size()))
			return B_IO_ERROR;
	}

	if (hasDrawing)
	{
		// xl/drawings/drawing1.xml: un ancoraggio ASSOLUTO per grafico
		// (posizione/dimensione in EMU, convertite direttamente dal
		// rettangolo in pixel di ChartObject::frame) invece di un
		// ancoraggio relativo a celle -- evita di dover conoscere
		// larghezze di colonna/altezze di riga per convertire la
		// posizione, che questo translator non ha modo di calcolare
		// correttamente qui (dipendono dal font e dalle preferenze
		// dell'utente, mai scritte nel file XLSX stesso). Entrambe le
		// forme sono OOXML valido, Excel/LibreOffice leggono
		// correttamente l'ancoraggio assoluto.
		std::string drawing;
		drawing += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
		drawing += "<xdr:wsDr xmlns:xdr=\"http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing\" "
			"xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">";

		std::string drawingRels;
		drawingRels += "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
			"<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";

		static const double kEmuPerPixelChart = 9525.0; // DrawingML, 96 DPI (predefinito Excel)
		for (size_t i = 0; i < chartXmls.size(); i++)
		{
			const XlsxChartInfo& info = usedCharts[i];
			long long x = (long long)(info.frameLeft * kEmuPerPixelChart);
			long long y = (long long)(info.frameTop * kEmuPerPixelChart);
			long long cx = (long long)((info.frameRight - info.frameLeft) * kEmuPerPixelChart);
			long long cy = (long long)((info.frameBottom - info.frameTop) * kEmuPerPixelChart);
			if (cx <= 0) cx = (long long)(400 * kEmuPerPixelChart);
			if (cy <= 0) cy = (long long)(300 * kEmuPerPixelChart);

			char buf[900];
			snprintf(buf, sizeof(buf),
				"<xdr:absoluteAnchor><xdr:pos x=\"%lld\" y=\"%lld\"/><xdr:ext cx=\"%lld\" cy=\"%lld\"/>"
				"<xdr:graphicFrame macro=\"\"><xdr:nvGraphicFramePr>"
				"<xdr:cNvPr id=\"%zu\" name=\"Grafico %zu\"/><xdr:cNvGraphicFramePr/></xdr:nvGraphicFramePr>"
				"<xdr:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/></xdr:xfrm>"
				"<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/chart\">"
				"<c:chart xmlns:c=\"http://schemas.openxmlformats.org/drawingml/2006/chart\" "
				"xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" r:id=\"rId%zu\"/>"
				"</a:graphicData></a:graphic></xdr:graphicFrame><xdr:clientData/></xdr:absoluteAnchor>",
				x, y, cx, cy, i + 1, i + 1, i + 1);
			drawing += buf;

			char relBuf[256];
			snprintf(relBuf, sizeof(relBuf),
				"<Relationship Id=\"rId%zu\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart\" "
				"Target=\"../charts/chart%zu.xml\"/>\n",
				i + 1, i + 1);
			drawingRels += relBuf;
		}

		drawing += "</xdr:wsDr>";
		drawingRels += "</Relationships>\n";

		if (!zip.AddEntry("xl/drawings/drawing1.xml", drawing.data(), drawing.size()))
			return B_IO_ERROR;
		if (!zip.AddEntry("xl/drawings/_rels/drawing1.xml.rels", drawingRels.data(), drawingRels.size()))
			return B_IO_ERROR;

		for (size_t i = 0; i < chartXmls.size(); i++)
		{
			char name[64];
			snprintf(name, sizeof(name), "xl/charts/chart%zu.xml", i + 1);
			if (!zip.AddEntry(name, chartXmls[i].data(), chartXmls[i].size()))
				return B_IO_ERROR;
		}
	}

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
	// Valori predefiniti espliciti (Fase 34): senza questi, -O2 segnala
	// correttamente -Wmaybe-uninitialized su ogni campo qui sotto --
	// ogni lettura e' gia' protetta dal suo "has*" corrispondente (mai
	// letto un rgb_color/int a caso), ma un campo POD senza inizializzo
	// resta comunque un valore indeterminato finche' non viene scritto,
	// un rischio inutile da lasciare in giro quando costa zero evitarlo.
	bool hasBg = false;
	rgb_color bg = { 0, 0, 0, 255 };
	bool hasFg = false;
	rgb_color fg = { 0, 0, 0, 255 };
	bool hasFormat = false;
	int format = 0; // valore gia' pronto per CellStyle::fFormat
	bool isDateFormat = false; // Fase 12: true se numFmtId e' un formato data/ora (vedi IsDateNumFmt) -- il valore numerico grezzo va convertito in eTimeData, non lasciato come numero
	bool hasFontStyle = false; // true solo se grassetto e/o corsivo (il Regular predefinito non serve applicarlo)
	int fontID = 0; // indice gia' risolto in gFontSizeTable, pronto per CellStyle::fFont
	bool hasAlignment = false; // true solo se diverso da eAlignGeneral (il predefinito non serve applicarlo)
	char alignment = 0; // EAlignment, pronto per CellStyle::fAlignment
	bool hasBorders = false; // true solo se almeno un lato e' impostato
	uchar borderT = 0, borderL = 0, borderB = 0, borderR = 0; // 0/1, pronti per CellStyle::fTBorderColor ecc (Fase 11: booleano per lato, non un vero colore)
	bool hasBorderColor = false; // 100% XLSX standard compatibility, Tier 2
	rgb_color borderColor = { 0, 0, 0, 255 }; // pronto per CellStyle::fBorderColor
	bool underline = false; // pronto per CellStyle::fUnderline (nessun campo "has": false coincide gia' col predefinito)
	bool wrapText = false; // pronto per CellStyle::fWrapText (nessun campo "has", stesso motivo di underline sopra)
	bool locked = true; // pronto per CellStyle::fLocked (Fase 32, nessun campo "has": true coincide gia' col predefinito)
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
	// Blocco cella (Fase 32, <protection locked="0|1"/>, figlio di
	// <xf> come <alignment>): true di default -- ECMA-376 dice che
	// un xf SENZA <protection> esplicito eredita "bloccata", stessa
	// convenzione del default di CellStyle::fLocked (vedi CellStyle.cpp).
	bool locked;
};

// Quattro lati di una voce di <borders>: presente/assente, stesso
// significato "booleano per lato" definito in Fase 11 (CellStyle::
// fTBorderColor ecc, non un vero colore/spessore nonostante il nome).
// "color"/"hasColor" (100% XLSX standard compatibility, Tier 2): il
// PRIMO <color> risolvibile fra i quattro lati -- stessa scelta di
// scope gia' fatta dal motore stesso (CellStyle::fBorderColor e' UN
// colore condiviso da tutti i lati di una cella, non un colore per
// lato, vedi il commento gemello in ui/src/AscdIO.cpp), quindi non
// serve tenere quattro colori distinti qui che nessun campo potrebbe
// mai ricevere separatamente.
struct BorderSides {
	bool top, left, bottom, right;
	bool hasColor;
	rgb_color color;
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

// Booleano XLSX (xsd:boolean, ECMA-376): sia "1"/"0" (forma usata da
// Excel) sia "true"/"false" per esteso (forma usata da LibreOffice
// Calc, riconoscibile da fileVersion appName="Calc" in workbook.xml)
// sono valide. BUG REALE trovato su un file utente vero esportato da
// Calc: diversi punti di questo file controllavano solo "0" (con
// strcmp) o usavano atoi() (che restituisce silenziosamente 0 per
// "true", non essendo una stringa numerica) -- un attributo scritto
// come "true"/"false" veniva quindi letto sbagliato ovunque tranne nei
// pochi punti che gia' controllavano esplicitamente anche "false".
// Centralizzato qui per evitare che lo stesso bug si ripresenti in un
// punto nuovo.
static bool XlsxAttrIsTrue(const char* value)
{
	return strcmp(value, "0") != 0 && strcmp(value, "false") != 0;
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
					value = XlsxAttrIsTrue(atts[i + 1]);
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
		// <color rgb="FFxxxxxx"/> (o theme="N"), figlio di <left>/
		// <right>/<top>/<bottom> (100% XLSX standard compatibility,
		// Tier 2): il PRIMO risolvibile fra i quattro lati vince, vedi
		// il commento su BorderSides sopra -- indexed non gestito (vedi
		// ResolveColorAttrs), stesso limite gia' noto per gli altri
		// colori di questo file.
		else if (strcmp(name, "color") == 0 && !ctx->borders.empty())
		{
			BorderSides& sides = ctx->borders.back();
			if (!sides.hasColor)
			{
				rgb_color c;
				if (ResolveColorAttrs(atts, *ctx->theme, &c))
				{
					sides.color = c;
					sides.hasColor = true;
				}
			}
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
			xf.locked = true;
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
				// wrapText: BUG REALE trovato su un file utente vero
				// esportato da LibreOffice Calc, escludendo solo "0" (non
				// anche "false") wrapText="false" veniva letto come vero,
				// avvolgendo su piu' righe un testo che in realta' doveva
				// restare su una riga sola e debordare nelle celle vuote
				// a destra -- vedi XlsxAttrIsTrue sopra.
				else if (strcmp(atts[i], "wrapText") == 0)
					ctx->cellXfs.back().wrapText = XlsxAttrIsTrue(atts[i + 1]);
			}
		}
		// <protection locked="0"/> e' un altro figlio di <xf>, stesso
		// principio di <alignment> sopra -- assente = resta bloccata (il
		// default gia' impostato sopra), "locked=0" e' l'UNICO modo in
		// cui un file XLSX marca una cella sbloccata.
		else if (strcmp(name, "protection") == 0 && !ctx->cellXfs.empty())
		{
			for (int i = 0; atts[i]; i += 2)
			{
				if (strcmp(atts[i], "locked") == 0)
					ctx->cellXfs.back().locked = XlsxAttrIsTrue(atts[i + 1]);
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
			if (sides.hasColor)
			{
				rs.hasBorderColor = true;
				rs.borderColor = sides.color;
			}
		}
		else
			rs.hasBorders = false;

		rs.underline = fontId >= 0 && (size_t)fontId < ctx.fontUnderline.size()
			&& ctx.fontUnderline[fontId];
		rs.wrapText = ctx.cellXfs[i].wrapText;
		rs.locked = ctx.cellXfs[i].locked;

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

// Un <hyperlink ref="A1" r:id="rIdX"/> (o, per un collegamento INTERNO
// alla stessa cartella di lavoro, <hyperlink ref="A1"
// location="Foglio2!A1"/>, senza r:id) dentro <worksheet> (100% XLSX
// standard compatibility, Tier 2) -- l'URL vero, per il caso r:id, si
// risolve solo dopo, tramite i _rels DI QUESTO foglio (stesso
// indirizzamento gia' usato per commenti/disegni/tabelle sopra), non
// qui: ParseSheet non ha accesso ai _rels, solo al testo del foglio.
struct HyperlinkRefInfo {
	std::string ref;
	std::string rId;      // non vuoto per un collegamento esterno
	std::string location; // non vuoto per un collegamento interno (nessun r:id)
};

// Un <dataValidation type="..." sqref="A1 B2:B4"><formula1>...</formula1>
// [<formula2>...</formula2>]</dataValidation>, dentro <dataValidations>
// (100% XLSX standard compatibility, Tier 2). Solo due forme si
// traducono in una ValidationRule reale (vedi Container.h): type="list"
// (formula1 e' un elenco letterale fra virgolette, es. "\"Rosso,Verde\"")
// e type="whole"/"decimal" con operator assente o "between" (formula1/2
// numerici letterali) -- ogni altra combinazione (elenco da intervallo
// di celle, operatori diversi da "between", date/orari, formula
// personalizzata) non ha un equivalente in questo motore e viene
// scartata all'applicazione, non qui: qui si raccoglie tutto cosi'
// com'e' nel file.
struct DataValidationRefInfo {
	std::string sqref;
	std::string type;
	std::string operatorAttr;
	std::string formula1;
	std::string formula2;
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
	bool* isProtected; // opzionale (NULL = non raccolto), da <sheetProtection/> (Fase 32)
	int* frozenRows; // opzionale (NULL = non raccolto), da <pane state="frozen"/> (100% XLSX standard compatibility, Tier 2)
	int* frozenCols; // idem, colonne bloccate
	std::vector<HyperlinkRefInfo>* hyperlinkRefs; // opzionale (NULL = non raccolti)
	std::vector<DataValidationRefInfo>* dataValidationRefs; // opzionale (NULL = non raccolte)
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

	// <dataValidation>/<formula1>/<formula2>: stato di parsing, stesso
	// principio di inValue/inFormula sopra ma per gli elementi di
	// convalida dati (nomi diversi per non confondersi con la formula
	// di una CELLA, un contesto completamente diverso).
	bool inDataValidation;
	bool inValidationFormula1;
	bool inValidationFormula2;
	DataValidationRefInfo currentValidation;

	// Formule array legacy (CSE, Ctrl+Maiusc+Invio): <f t="array"
	// ref="B2:D4">FORMULA</f> compare SOLO sulla cella in alto a
	// sinistra dell'intervallo -- le altre celle dell'intervallo non
	// hanno affatto un <f> proprio (niente formula, niente t="array",
	// solo <v> col valore congelato). formulaType/formulaRef leggono
	// gli attributi di <f> alla sua apertura; arrayFormulas accumula
	// (intervallo -> testo) ogni volta che una <f t="array"> con testo
	// non vuoto viene chiusa, cosi' una cella successiva SENZA <f>
	// propria puo' recuperare la formula giusta da qui (vedi "c" in
	// SheetEnd) invece di essere importata come valore statico morto.
	std::string formulaType;
	std::string formulaRef;
	std::vector<std::pair<range, std::string> > arrayFormulas;

	// Formule condivise (<f t="shared" si="N" ref="B2:B20">FORMULA</f>,
	// scritte da Excel vero quasi sempre quando si trascina una formula
	// su un intervallo): come una formula array, solo la cella ancora
	// del gruppo porta il testo -- le altre hanno solo una <f t="shared"
	// si="N"/> vuota. A differenza di una formula array pero', qui i
	// riferimenti RELATIVI vanno spostati in base alla nuova posizione
	// (un riferimento assoluto con "$" invece resta fisso): non basta
	// riusare lo stesso testo, serve compilarlo con CompileSharedFormulaAt
	// (vedi sopra SheetStart/SheetEnd). formulaSi legge l'attributo si="..."
	// di <f>; sharedFormulaAnchors mappa si -> (cella ancora, testo) ogni
	// volta che una <f t="shared"> con testo non vuoto viene chiusa.
	std::string formulaSi;
	std::map<int, std::pair<cell, std::string> > sharedFormulaAnchors;

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

// Compiles formulaText as if it had been typed into anchorLoc, but
// stores the resulting formula in targetLoc -- reproduces Excel's
// shared-formula semantics ($-absolute references stay fixed,
// everything else shifts by the offset between the two cells) without
// any manual text-level reference rewriting, by exploiting how this
// engine already encodes cell references: cell::GetFormulaCell (Cell.cpp)
// stores a reference WITHOUT "$" as a signed delta relative to the cell
// the formula is parsed against, and a reference WITH "$" as an
// absolute value regardless of that base (see the VFIXED/HFIXED flags
// in Cell.h). CContainer::CalcCell (Container.graph.cpp) always
// evaluates a formula's references against whatever cell currently
// holds its bytecode, not the cell it was originally parsed against --
// so compiling here with anchorLoc as the parse base, then writing the
// result into targetLoc, makes the SAME bytecode resolve its relative
// references correctly shifted the moment it's evaluated in place.
static bool CompileSharedFormulaAt(const std::string& formulaText,
	cell anchorLoc, cell targetLoc, CContainer* doc)
{
	// decSep='.'/listSep=',' explicit, same reasoning as every other
	// <f> text parse in this file: XLSX formula text is always in
	// canonical ECMA-376 form, independent of the user's locale prefs.
	CParser p(doc, ',', '.', 0, 0);
	if (!p.Parse(formulaText.c_str(), anchorLoc))
		return false;
	doc->NewCell(targetLoc, Value(), p.Formula().CopyString());
	return true;
}

static void XMLCALL SheetStart(void* userData, const char* name, const char** atts)
{
	SheetContext* ctx = (SheetContext*)userData;

	// <sheetView showGridLines="0" .../>, dentro <sheetViews> prima di
	// <sheetData> -- l'attributo e' assente quando la griglia e'
	// semplicemente visibile (il default di Excel, "1" implicito, mai
	// scritto esplicitamente in quel caso): solo "0"/"false" esplicito
	// la nasconde. Bug reale segnalato dall'utente confrontando un file
	// con Excel: un foglio con la griglia nascosta appositamente
	// dall'autore (un look pulito da documento ufficiale) veniva
	// comunque importato con la griglia visibile, perche' prima questo
	// translator non leggeva affatto l'attributo. Secondo bug reale,
	// trovato piu' tardi su un file esportato da LibreOffice Calc: qui
	// si usava atoi() invece di XlsxAttrIsTrue, e atoi("true") vale
	// silenziosamente 0 (non essendo una stringa numerica) -- ogni
	// foglio di un file Calc (che scrive sempre showGridLines="true"
	// esplicito, mai lo lascia implicito come Excel) risultava quindi
	// con la griglia nascosta anche quando l'originale la mostrava.
	if (strcmp(name, "sheetView") == 0 && ctx->showGrid)
	{
		bool show = true;
		for (int i = 0; atts[i]; i += 2)
			if (strcmp(atts[i], "showGridLines") == 0)
				show = XlsxAttrIsTrue(atts[i + 1]);
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
				customHeight = XlsxAttrIsTrue(atts[i + 1]);
			else if (strcmp(atts[i], "hidden") == 0)
				hidden = XlsxAttrIsTrue(atts[i + 1]);
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
	// <hyperlink ref="A1" r:id="rIdX"/> (o location="..." per un
	// collegamento interno, vedi HyperlinkRefInfo sopra), dentro
	// <hyperlinks>, fratello di <sheetData>: solo raccolto qui, risolto
	// (r:id -> URL vero) dopo, tramite i _rels del foglio (vedi Translate).
	else if (strcmp(name, "hyperlink") == 0 && ctx->hyperlinkRefs)
	{
		HyperlinkRefInfo info;
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "ref") == 0)
				info.ref = atts[i + 1];
			else if (strcmp(atts[i], "r:id") == 0)
				info.rId = atts[i + 1];
			else if (strcmp(atts[i], "location") == 0)
				info.location = atts[i + 1];
		}
		if (!info.ref.empty() && (!info.rId.empty() || !info.location.empty()))
			ctx->hyperlinkRefs->push_back(info);
	}
	// <pane xSplit="M" ySplit="N" state="frozen"/> dentro <sheetView>
	// (100% XLSX standard compatibility, Tier 2): SOLO
	// state="frozen"/"frozenSplit" vuol dire un vero blocco riquadri --
	// senza "state" (o state="split"), xSplit/ySplit sono ventesimi di
	// punto per uno split trascinabile, non un numero di righe/colonne,
	// e questa app non ha un concetto di split non bloccato da
	// rappresentare.
	else if (strcmp(name, "pane") == 0 && ctx->frozenRows && ctx->frozenCols)
	{
		std::string state;
		int xSplit = 0, ySplit = 0;
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "state") == 0)
				state = atts[i + 1];
			else if (strcmp(atts[i], "xSplit") == 0)
				xSplit = atoi(atts[i + 1]);
			else if (strcmp(atts[i], "ySplit") == 0)
				ySplit = atoi(atts[i + 1]);
		}
		if (state == "frozen" || state == "frozenSplit")
		{
			*ctx->frozenRows = ySplit;
			*ctx->frozenCols = xSplit;
		}
	}
	// <dataValidation type="..." sqref="..."> (100% XLSX standard
	// compatibility, Tier 2), dentro <dataValidations>, fratello di
	// <sheetData>: "operator" e' assente per type="list" (non si
	// applica), e vale implicitamente "between" quando assente per un
	// intervallo numerico (default OOXML) -- vedi DataValidationRefInfo
	// sopra per quali combinazioni diventano davvero una ValidationRule.
	else if (strcmp(name, "dataValidation") == 0 && ctx->dataValidationRefs)
	{
		ctx->inDataValidation = true;
		ctx->currentValidation = DataValidationRefInfo();
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "sqref") == 0)
				ctx->currentValidation.sqref = atts[i + 1];
			else if (strcmp(atts[i], "type") == 0)
				ctx->currentValidation.type = atts[i + 1];
			else if (strcmp(atts[i], "operator") == 0)
				ctx->currentValidation.operatorAttr = atts[i + 1];
		}
	}
	else if (ctx->inDataValidation && strcmp(name, "formula1") == 0)
		ctx->inValidationFormula1 = true;
	else if (ctx->inDataValidation && strcmp(name, "formula2") == 0)
		ctx->inValidationFormula2 = true;
	// <sheetProtection .../> (Fase 32, "Proteggi foglio"): la sola
	// PRESENZA dell'elemento vuol dire "foglio protetto" in Excel,
	// indipendentemente dai suoi attributi (password, quali comandi
	// restano permessi ecc. -- questo translator non li legge, solo il
	// blocco/sblocco effettivo delle celle interessa qui).
	else if (strcmp(name, "sheetProtection") == 0 && ctx->isProtected)
	{
		*ctx->isProtected = true;
	}
	else if (strcmp(name, "c") == 0)
	{
		ctx->cellRef.clear();
		ctx->cellType.clear();
		ctx->value.clear();
		ctx->formula.clear();
		ctx->formulaType.clear();
		ctx->formulaRef.clear();
		ctx->formulaSi.clear();
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
	{
		ctx->inFormula = true;
		ctx->formulaType.clear();
		ctx->formulaRef.clear();
		ctx->formulaSi.clear();
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "t") == 0)
				ctx->formulaType = atts[i + 1];
			else if (strcmp(atts[i], "ref") == 0)
				ctx->formulaRef = atts[i + 1];
			else if (strcmp(atts[i], "si") == 0)
				ctx->formulaSi = atts[i + 1];
		}
	}
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
					|| rs.hasBorders || rs.hasBorderColor || rs.underline || rs.wrapText || !rs.locked)
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
						if (rs.hasBorderColor) cs.fBorderColor = rs.borderColor;
						if (rs.underline) cs.fUnderline = true;
						if (rs.wrapText) cs.fWrapText = true;
						cs.fLocked = rs.locked;
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
	else if (strcmp(name, "formula1") == 0 && ctx->inDataValidation)
		ctx->inValidationFormula1 = false;
	else if (strcmp(name, "formula2") == 0 && ctx->inDataValidation)
		ctx->inValidationFormula2 = false;
	else if (strcmp(name, "dataValidation") == 0 && ctx->inDataValidation)
	{
		ctx->inDataValidation = false;
		if (!ctx->currentValidation.sqref.empty() && !ctx->currentValidation.formula1.empty())
			ctx->dataValidationRefs->push_back(ctx->currentValidation);
	}
	else if (strcmp(name, "f") == 0)
	{
		ctx->inFormula = false;
		// Registra la formula array (vedi il commento su
		// SheetContext::arrayFormulas sopra) solo ora che ctx->formula
		// contiene tutto il testo accumulato dal gestore di dati
		// carattere tra <f> e </f>.
		if (ctx->formulaType == "array" && !ctx->formulaRef.empty() && !ctx->formula.empty())
		{
			range r;
			if (ParseSqrefToken(ctx->formulaRef, &r))
				ctx->arrayFormulas.push_back(std::make_pair(r, ctx->formula));
		}
		// Registra l'ancora di una formula condivisa (vedi il commento
		// su SheetContext::sharedFormulaAnchors sopra): serve la cella
		// corrente (ctx->cellRef, gia' letto all'apertura di <c> --
		// <f> e' sempre annidata dentro <c>) come base per lo
		// spostamento dei riferimenti relativi di ogni cella successiva
		// con la stessa si.
		else if (ctx->formulaType == "shared" && !ctx->formulaSi.empty() && !ctx->formula.empty())
		{
			int col, row;
			if (CellRefToColRow(ctx->cellRef, col, row))
				ctx->sharedFormulaAnchors[atoi(ctx->formulaSi.c_str())] =
					std::make_pair(cell(col, row), ctx->formula);
		}
	}
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

		// Cella dentro l'intervallo di una formula array (vedi il
		// commento su SheetContext::arrayFormulas) ma senza un <f>
		// proprio: eredita la STESSA formula della cella ancora -- a
		// differenza delle formule condivise sotto, un'array formula
		// CSE mostra il testo IDENTICO in ogni cella dell'intervallo,
		// nessuno spostamento di riferimenti relativi.
		if (ctx->formula.empty())
		{
			for (size_t i = 0; i < ctx->arrayFormulas.size(); i++)
			{
				if (ctx->arrayFormulas[i].first.Contains(loc))
				{
					ctx->formula = ctx->arrayFormulas[i].second;
					break;
				}
			}
		}

		// Cella con una formula condivisa (<f t="shared" si="N"/> vuota,
		// vedi il commento su SheetContext::sharedFormulaAnchors) ma
		// senza testo proprio: a differenza di una formula array, qui i
		// riferimenti relativi vanno spostati rispetto alla nuova
		// posizione, non ripetuti identici -- CompileSharedFormulaAt
		// (sopra SheetStart) lo fa scrivendo gia' la cella qui stesso,
		// quindi il ramo generico "!text.empty()" sotto va saltato per
		// questa cella (ne scriverebbe una seconda volta, buttando via
		// la formula appena compilata e tornando al valore congelato).
		bool sharedFormulaHandled = false;
		if (ctx->formula.empty() && ctx->formulaType == "shared" && !ctx->formulaSi.empty())
		{
			std::map<int, std::pair<cell, std::string> >::iterator found =
				ctx->sharedFormulaAnchors.find(atoi(ctx->formulaSi.c_str()));
			if (found != ctx->sharedFormulaAnchors.end())
				sharedFormulaHandled = CompileSharedFormulaAt(
					found->second.second, found->second.first, loc, ctx->doc);
		}

		std::string text;
		if (sharedFormulaHandled)
		{
			// Gia' scritta sopra: "text" resta vuota apposta, cosi' il
			// blocco "!text.empty()" sotto non tocca piu' questa cella,
			// ma lo stile (vedi oltre) va comunque applicato come per
			// ogni altra.
		}
		else if (!ctx->formula.empty())
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
				|| rs.hasBorders || rs.hasBorderColor || rs.underline || rs.wrapText || !rs.locked)
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
				if (rs.hasBorderColor) cs.fBorderColor = rs.borderColor;
				if (rs.underline) cs.fUnderline = true;
				if (rs.wrapText) cs.fWrapText = true;
				cs.fLocked = rs.locked;
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
	else if (ctx->inValidationFormula1)
		ctx->currentValidation.formula1.append(s, len);
	else if (ctx->inValidationFormula2)
		ctx->currentValidation.formula2.append(s, len);
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
	bool* hasAutoFilter = NULL, range* autoFilterRange = NULL,
	bool* isProtected = NULL,
	std::vector<HyperlinkRefInfo>* hyperlinkRefs = NULL,
	std::vector<DataValidationRefInfo>* dataValidationRefs = NULL,
	int* frozenRows = NULL, int* frozenCols = NULL)
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
	ctx.isProtected = isProtected;
	ctx.hyperlinkRefs = hyperlinkRefs;
	ctx.dataValidationRefs = dataValidationRefs;
	ctx.frozenRows = frozenRows;
	ctx.frozenCols = frozenCols;
	ctx.styles = styles;
	ctx.condRules = condRules;
	ctx.date1904 = date1904;
	ctx.cellStyleIndex = -1;
	ctx.inValue = false;
	ctx.inFormula = false;
	ctx.inCfRule = false;
	ctx.inCondFormula = false;
	ctx.inDataValidation = false;
	ctx.inValidationFormula1 = false;
	ctx.inValidationFormula2 = false;

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
	return "Importa/esporta fogli di calcolo dal/al formato Excel 2007+ (XLSX), "
		"incluse le formule vive sullo stesso foglio";
}

int32 CXlsxTranslator::TranslatorVersion() const
{
	return B_TRANSLATION_MAKE_VERSION(1, 0, 0);
}

// Catalogo di localizzazione di QUESTO add-on, stesso schema/motivo di
// sCatalog in translators/csv/CsvTranslator.cpp (vedi quel commento).
static BCatalog sCatalog;
#undef B_CATALOG
#define B_CATALOG (&sCatalog)
#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "XlsxConfigView"

// Link "Offrimi un caffe'" cliccabile, stesso schema/motivo di
// CCsvCoffeeLink in translators/csv/CsvTranslator.cpp (vedi quel
// commento per il perche' non e' un ClickableStringView di ui/src/).
static const char kCoffeeUrl[] = "https://buymeacoffee.com/atomozero";

class CXlsxCoffeeLink : public BStringView {
public:
	CXlsxCoffeeLink() : BStringView("coffeeLink", B_TRANSLATE("Offrimi un caffe' \xE2\x98\x95")) {}

	virtual void AttachedToWindow()
	{
		BStringView::AttachedToWindow();
		SetHighColor(40, 80, 200);
	}

	virtual void MouseDown(BPoint)
	{
		const char* arg = kCoffeeUrl;
		be_roster->Launch("application/x-vnd.Be.URL.https", 1, const_cast<char**>(&arg));
	}

	virtual void MouseMoved(BPoint, uint32, const BMessage*)
	{
		BCursor link(B_CURSOR_ID_FOLLOW_LINK);
		SetViewCursor(&link);
	}
};

// Vista "Informazioni" mostrata dal pannello Translators di Haiku,
// stesso schema/motivo di CCsvConfigView in translators/csv/
// CsvTranslator.cpp (vedi quel commento per il perche').
class CXlsxConfigView : public BView {
public:
	CXlsxConfigView(BRect frame)
		:
		BView(frame, "XlsxConfigView", B_FOLLOW_ALL, B_WILL_DRAW)
	{
		SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

		BStringView* title = new BStringView("title", "XLSX Translator");
		title->SetFont(be_bold_font);

		int32 v = B_TRANSLATION_MAKE_VERSION(1, 0, 0);
		BString versionText;
		versionText.SetToFormat(B_TRANSLATE("Versione %d.%d.%d"),
			(int)B_TRANSLATION_MAJOR_VERSION(v), (int)B_TRANSLATION_MINOR_VERSION(v),
			(int)B_TRANSLATION_REVISION_VERSION(v));

		BFont small(be_plain_font);
		small.SetSize(be_plain_font->Size() - 1);

		BStringView* version = new BStringView("version", versionText.String());
		version->SetFont(&small);
		version->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.7));

		BStringView* info = new BStringView("info",
			B_TRANSLATE("Importa/esporta fogli di calcolo dal/al formato\n"
				"Excel 2007+ (XLSX), incluse le formule vive sullo\n"
				"stesso foglio (un riferimento a un altro foglio\n"
				"esporta solo il valore calcolato)."));

		BStringView* copyright = new BStringView("copyright",
			B_TRANSLATE("Copyright (c) 2026 Andrea Bernardi \xC2\xB7 Licenza MIT"));
		copyright->SetFont(&small);
		copyright->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.6));

		CXlsxCoffeeLink* coffeeLink = new CXlsxCoffeeLink();
		coffeeLink->SetFont(&small);

		BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS)
			.Add(title)
			.Add(version)
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(info)
			.AddGlue()
			.Add(coffeeLink)
			.Add(copyright)
		.End();
	}
};

status_t CXlsxTranslator::MakeConfigurationView(BMessage* extension, BView** _view,
	BRect* _extent)
{
	if (_view == NULL || _extent == NULL)
		return B_BAD_VALUE;

	CXlsxConfigView* view = new CXlsxConfigView(BRect(0, 0, 299, 154));
	*_view = view;
	*_extent = view->Bounds();
	return B_OK;
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

// A single <definedName> (named range, "100% XLSX standard
// compatibility" plan, ROADMAP.md): "name" and an optional
// "localSheetId" (0-based index into WorkbookContext::sheets, present
// only for a sheet-scoped name) come from <definedName>'s own
// attributes; "refText" is the raw text between its tags (e.g.
// "Foglio1!$A$1:$A$5"), resolved into an actual sheet+range later by
// ApplyDefinedNames, once every sheet's CContainer exists.
struct DefinedNameInfo {
	std::string name;
	bool hasLocalSheetId = false;
	int localSheetId = -1;
	std::string refText;
};

struct WorkbookContext {
	std::vector<WorkbookSheetInfo> sheets;
	bool inSheets;
	bool date1904; // <workbookPr date1904="1"/>: epoca Mac storica (1904-01-01) invece della predefinita (1899-12-30, Fase 12)
	std::vector<DefinedNameInfo> definedNames;
	bool inDefinedNames = false;
	bool inDefinedName = false;
	DefinedNameInfo currentName;
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
	else if (strcmp(name, "definedNames") == 0)
		ctx->inDefinedNames = true;
	else if (ctx->inDefinedNames && strcmp(name, "definedName") == 0)
	{
		ctx->inDefinedName = true;
		ctx->currentName = DefinedNameInfo();
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "name") == 0)
				ctx->currentName.name = atts[i + 1];
			else if (strcmp(atts[i], "localSheetId") == 0)
			{
				ctx->currentName.hasLocalSheetId = true;
				ctx->currentName.localSheetId = atoi(atts[i + 1]);
			}
		}
	}
	else if (strcmp(name, "workbookPr") == 0)
	{
		for (int i = 0; atts[i]; i += 2)
			if (strcmp(atts[i], "date1904") == 0)
				ctx->date1904 = XlsxAttrIsTrue(atts[i + 1]);
	}
}

static void XMLCALL WorkbookEnd(void* userData, const char* name)
{
	WorkbookContext* ctx = (WorkbookContext*)userData;
	if (strcmp(name, "sheets") == 0)
		ctx->inSheets = false;
	else if (strcmp(name, "definedNames") == 0)
		ctx->inDefinedNames = false;
	else if (strcmp(name, "definedName") == 0 && ctx->inDefinedName)
	{
		ctx->inDefinedName = false;
		if (!ctx->currentName.name.empty())
			ctx->definedNames.push_back(ctx->currentName);
	}
}

static void XMLCALL WorkbookChars(void* userData, const char* s, int len)
{
	WorkbookContext* ctx = (WorkbookContext*)userData;
	if (ctx->inDefinedName)
		ctx->currentName.refText.append(s, len);
}

static bool ParseWorkbookSheetList(const std::vector<unsigned char>& xml,
	std::vector<WorkbookSheetInfo>& out, bool* outDate1904 = NULL,
	std::vector<DefinedNameInfo>* outDefinedNames = NULL)
{
	if (xml.empty())
		return false;

	WorkbookContext ctx;
	ctx.inSheets = false;
	ctx.date1904 = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, WorkbookStart, WorkbookEnd);
	XML_SetCharacterDataHandler(parser, WorkbookChars);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK || ctx.sheets.empty())
		return false;

	out = ctx.sheets;
	if (outDate1904)
		*outDate1904 = ctx.date1904;
	if (outDefinedNames)
		*outDefinedNames = ctx.definedNames;
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
				info->showStripes = XlsxAttrIsTrue(atts[i + 1]);
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

// Applica ogni <dataValidation> raccolto da ParseSheet (100% XLSX
// standard compatibility, Tier 2) a "doc": solo due forme diventano
// davvero una ValidationRule (vedi DataValidationRefInfo sopra) --
// tutto il resto (elenco da un intervallo di celle invece che
// letterale, un operatore diverso da "between", date/orari, formula
// personalizzata) non ha equivalente in questo motore e viene
// scartato in silenzio, stesso principio di ApplyConditionalFormatting
// sopra per i tipi di regola che non sa modellare.
static void ApplyDataValidation(CContainer* doc, const std::vector<DataValidationRefInfo>& refs)
{
	for (size_t i = 0; i < refs.size(); i++)
	{
		const DataValidationRefInfo& info = refs[i];
		ValidationRule rule;

		if (info.type == "list")
		{
			const std::string& f = info.formula1;
			if (f.size() < 2 || f.front() != '"' || f.back() != '"')
				continue; // elenco da intervallo di celle: non modellabile qui
			rule.type = eListValidation;
			rule.list = f.substr(1, f.size() - 2);
		}
		else if (info.type == "whole" || info.type == "decimal")
		{
			if (!info.operatorAttr.empty() && info.operatorAttr != "between")
				continue;
			if (info.formula1.empty() || info.formula2.empty())
				continue;
			char* end1 = NULL; char* end2 = NULL;
			double v1 = strtod(info.formula1.c_str(), &end1);
			double v2 = strtod(info.formula2.c_str(), &end2);
			if (end1 == info.formula1.c_str() || *end1 != 0
				|| end2 == info.formula2.c_str() || *end2 != 0)
				continue; // riferimento a cella, non un numero letterale
			rule.type = eNumberRangeValidation;
			rule.min = v1;
			rule.max = v2;
		}
		else
			continue; // date/orari/lunghezza testo/formula personalizzata: non modellabili qui

		std::vector<range> ranges;
		ParseSqref(info.sqref, &ranges);
		for (size_t r = 0; r < ranges.size(); r++)
		{
			for (int row = ranges[r].top; row <= ranges[r].bottom; row++)
				for (int col = ranges[r].left; col <= ranges[r].right; col++)
					doc->SetValidation(cell(col, row), rule);
		}
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
	// xdr:to (Fase 25, import dei grafici): solo un xdr:twoCellAnchor
	// vero ce l'ha -- serve per calcolare la dimensione di un GRAFICO
	// (a differenza di un'immagine, un grafico non ha una "dimensione
	// naturale" di riserva quando extCxEmu/extCyEmu sono assenti/zero,
	// il caso comune per un xdr:twoCellAnchor reale: la dimensione vera
	// e' la differenza fra "from" e "to", non un <xdr:ext> che per un
	// oggetto ancorato-a-due-celle e' opzionale o assente).
	bool hasTo;
	int toCol, toRow;
	long toColOffEmu, toRowOffEmu;
	long extCxEmu, extCyEmu; // 0 = non specificato, vedi sopra
	// xdr:absoluteAnchor (Fase 25): un ancoraggio a posizione pixel
	// assoluta invece che a cella -- raro in un vero file Excel (che
	// usa quasi sempre twoCellAnchor), ma e' esattamente cio' che
	// WriteXLSX/BuildChartXml scrive per i grafici esportati da questa
	// stessa app (vedi kEmuPerPixelChart li' sopra): senza riconoscere
	// anche questo tipo di ancoraggio, un file XLSX esportato da
	// Atomo123 e poi riaperto perderebbe silenziosamente i propri
	// grafici, un'asimmetria export/import reale, non solo teorica.
	bool isAbsolute;
	long absXEmu, absYEmu;
	std::string relId;
	// true se relId viene da <c:chart r:id="..."> (grafico incorporato,
	// Fase 25) invece che da <a:blip r:embed="..."> (immagine, Fase 12)
	// -- stesso ancoraggio XLSX (xdr:twoCellAnchor/oneCellAnchor/
	// absoluteAnchor), contenuto diverso: un <xdr:graphicFrame> con
	// dentro un riferimento a xl/charts/chartN.xml invece di un
	// <xdr:pic>.
	bool isChart;

	DrawingPic() : fromCol(0), fromRow(0), fromColOffEmu(0), fromRowOffEmu(0),
		hasTo(false), toCol(0), toRow(0), toColOffEmu(0), toRowOffEmu(0),
		extCxEmu(0), extCyEmu(0), isAbsolute(false), absXEmu(0), absYEmu(0),
		isChart(false) {}
};

struct DrawingContext {
	std::vector<DrawingPic> pics;
	DrawingPic current;
	bool inAnchor;
	bool inFrom;
	bool inTo;
	// <xdr:graphicFrame> (un grafico, Fase 25) porta il SUO xfrm/ext
	// interno per il posizionamento grafico del contenuto (sempre
	// 0x0 in cio' che scrive BuildChartXml sopra -- "dipendono dal
	// font", vedi il commento li') -- un <a:ext> diverso, annidato
	// piu' in profondita', dallo <xdr:ext> a livello di ancoraggio che
	// contiene davvero la dimensione (letto sotto). Senza distinguerli,
	// lo stesso nome di tag "a:ext" veniva confuso con quello giusto e
	// lo sovrascriveva con 0x0 (bug reale, scoperto con un fprintf di
	// debug sul round-trip dei grafici esportati da questa stessa app).
	bool inGraphicFrame;
	bool captureNum;
	std::string numText;
};

static void XMLCALL DrawingStart(void* userData, const char* name, const char** atts)
{
	DrawingContext* ctx = (DrawingContext*)userData;

	if (strcmp(name, "xdr:twoCellAnchor") == 0 || strcmp(name, "xdr:oneCellAnchor") == 0
		|| strcmp(name, "xdr:absoluteAnchor") == 0)
	{
		ctx->inAnchor = true;
		ctx->current = DrawingPic();
		return;
	}
	if (!ctx->inAnchor)
		return;

	if (strcmp(name, "xdr:from") == 0)
		ctx->inFrom = true;
	else if (strcmp(name, "xdr:to") == 0)
	{
		ctx->inTo = true;
		ctx->current.hasTo = true;
	}
	else if (strcmp(name, "xdr:pos") == 0)
	{
		ctx->current.isAbsolute = true;
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "x") == 0)
				ctx->current.absXEmu = atol(atts[i + 1]);
			else if (strcmp(atts[i], "y") == 0)
				ctx->current.absYEmu = atol(atts[i + 1]);
		}
	}
	else if ((ctx->inFrom || ctx->inTo) && (strcmp(name, "xdr:col") == 0
		|| strcmp(name, "xdr:colOff") == 0 || strcmp(name, "xdr:row") == 0
		|| strcmp(name, "xdr:rowOff") == 0))
	{
		ctx->numText.clear();
		ctx->captureNum = true;
	}
	else if (strcmp(name, "xdr:graphicFrame") == 0)
		ctx->inGraphicFrame = true;
	else if (!ctx->inGraphicFrame && (strcmp(name, "xdr:ext") == 0 || strcmp(name, "a:ext") == 0))
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
	else if (strcmp(name, "c:chart") == 0)
	{
		// Grafico incorporato (Fase 25): <xdr:graphicFrame><a:graphic>
		// <a:graphicData><c:chart r:id="rIdX"/> -- r:id punta, tramite i
		// _rels di QUESTO drawing (stesso indirizzamento di a:blip
		// sopra), a xl/charts/chartN.xml.
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "r:id") == 0)
			{
				ctx->current.relId = atts[i + 1];
				ctx->current.isChart = true;
			}
		}
	}
}

static void XMLCALL DrawingEnd(void* userData, const char* name)
{
	DrawingContext* ctx = (DrawingContext*)userData;

	if (strcmp(name, "xdr:twoCellAnchor") == 0 || strcmp(name, "xdr:oneCellAnchor") == 0
		|| strcmp(name, "xdr:absoluteAnchor") == 0)
	{
		ctx->inAnchor = false;
		if (!ctx->current.relId.empty())
			ctx->pics.push_back(ctx->current);
	}
	else if (strcmp(name, "xdr:from") == 0)
		ctx->inFrom = false;
	else if (strcmp(name, "xdr:to") == 0)
		ctx->inTo = false;
	else if (strcmp(name, "xdr:graphicFrame") == 0)
		ctx->inGraphicFrame = false;
	else if (ctx->captureNum)
	{
		long value = atol(ctx->numText.c_str());
		if (ctx->inFrom)
		{
			if (strcmp(name, "xdr:col") == 0)
				ctx->current.fromCol = (int)value;
			else if (strcmp(name, "xdr:colOff") == 0)
				ctx->current.fromColOffEmu = value;
			else if (strcmp(name, "xdr:row") == 0)
				ctx->current.fromRow = (int)value;
			else if (strcmp(name, "xdr:rowOff") == 0)
				ctx->current.fromRowOffEmu = value;
		}
		else if (ctx->inTo)
		{
			if (strcmp(name, "xdr:col") == 0)
				ctx->current.toCol = (int)value;
			else if (strcmp(name, "xdr:colOff") == 0)
				ctx->current.toColOffEmu = value;
			else if (strcmp(name, "xdr:row") == 0)
				ctx->current.toRow = (int)value;
			else if (strcmp(name, "xdr:rowOff") == 0)
				ctx->current.toRowOffEmu = value;
		}
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
	ctx.inTo = false;
	ctx.inGraphicFrame = false;
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

// Un <comment> di xl/comments{N}.xml (100% XLSX standard compatibility,
// Tier 2): "ref" e' un riferimento stile "A1" (attributo diretto, non
// un ancoraggio disegno come per immagini/grafici sopra), "text" e' la
// concatenazione di ogni <t> dentro <text> (uno o piu' <r> "rich text
// run", ognuno col proprio <t> -- l'autore/formattazione per-run non
// hanno equivalente in questo motore, solo il testo sopravvive, stesso
// limite gia' documentato per il testo formattato per-carattere in una
// cella normale).
struct CommentEntry {
	std::string ref;
	std::string text;
};

struct CommentsContext {
	std::vector<CommentEntry> comments;
	CommentEntry current;
	bool inComment;
	bool captureText;
};

static void XMLCALL CommentsStart(void* userData, const char* name, const char** atts)
{
	CommentsContext* ctx = (CommentsContext*)userData;

	if (strcmp(name, "comment") == 0)
	{
		ctx->inComment = true;
		ctx->current = CommentEntry();
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "ref") == 0)
				ctx->current.ref = atts[i + 1];
		}
	}
	else if (ctx->inComment && strcmp(name, "t") == 0)
		ctx->captureText = true;
}

static void XMLCALL CommentsEnd(void* userData, const char* name)
{
	CommentsContext* ctx = (CommentsContext*)userData;

	if (strcmp(name, "comment") == 0)
	{
		ctx->inComment = false;
		if (!ctx->current.ref.empty())
			ctx->comments.push_back(ctx->current);
	}
	else if (strcmp(name, "t") == 0)
		ctx->captureText = false;
}

static void XMLCALL CommentsChars(void* userData, const char* s, int len)
{
	CommentsContext* ctx = (CommentsContext*)userData;
	if (ctx->captureText)
		ctx->current.text.append(s, len);
}

static bool ParseComments(const std::vector<unsigned char>& xml, std::vector<CommentEntry>* out)
{
	if (xml.empty())
		return false;

	CommentsContext ctx;
	ctx.inComment = false;
	ctx.captureText = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, CommentsStart, CommentsEnd);
	XML_SetCharacterDataHandler(parser, CommentsChars);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), (int)xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK)
		return false;

	*out = ctx.comments;
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

// --- Parsing di xl/charts/chartN.xml (Fase 25, importazione dei grafici) --
//
// Estrae solo cio' che serve per ricostruire un ChartObject: tipo di
// grafico, titolo opzionale, e i riferimenti di cella di categoria/
// valori. MAI i valori cache <c:numCache>/<c:strCache> -- il grafico
// dell'app legge sempre i dati DAL VIVO dal foglio al momento del
// disegno, mai da uno snapshot, stesso comportamento sia per un
// grafico creato in Atomo123 sia per uno importato da Excel; e i nomi
// delle serie (<c:tx>) non hanno campo di destinazione in ChartObject.
struct ChartXmlResult {
	int8 type;
	bool typeRecognized;
	std::string unsupportedReason; // valido solo se !typeRecognized
	std::string title;
	std::string catRef;
	std::vector<std::string> valRefs; // uno per <c:ser>, stesso ordine

	ChartXmlResult() : type(0), typeRecognized(false) {}
};

struct ChartXmlContext {
	ChartXmlResult result;
	// <c:cat> e <c:val> non si annidano mai l'uno nell'altro dentro lo
	// stesso <c:ser> -- un unico "kind piatto" (non uno stack) basta a
	// distinguerli, e resta eNone quando un <c:f> compare altrove (nel
	// titolo o nel nome di una serie), che percio' viene ignorato
	// correttamente senza bisogno di un caso speciale.
	enum { eNone, eCat, eVal } kind;
	bool inTitle;
	bool capturingF;
	bool capturingTitleText;
	std::string fText;
};

static void XMLCALL ChartXmlStart(void* userData, const char* name, const char** atts)
{
	ChartXmlContext* ctx = (ChartXmlContext*)userData;

	if (strcmp(name, "c:barChart") == 0)
	{
		ctx->result.type = 0;
		ctx->result.typeRecognized = true;
	}
	else if (strcmp(name, "c:barDir") == 0)
	{
		// Un grafico a barre ORIZZONTALI ("bar" invece di "col") non ha
		// equivalente disegnato da questa app -- solo barre verticali
		// (vedi ChartType/DrawBarChart in ui/src/Chart*).
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "val") == 0 && strcmp(atts[i + 1], "bar") == 0)
			{
				ctx->result.typeRecognized = false;
				ctx->result.unsupportedReason = "Barre orizzontali";
			}
		}
	}
	else if (strcmp(name, "c:lineChart") == 0)
	{
		ctx->result.type = 1;
		ctx->result.typeRecognized = true;
	}
	else if (strcmp(name, "c:pieChart") == 0)
	{
		ctx->result.type = 2;
		ctx->result.typeRecognized = true;
	}
	else if (!ctx->result.typeRecognized)
	{
		// Qualunque altro "c:xxxChart" (area, dispersione, radar,
		// ciambella, azionario, superficie, bolle, 3D, ecc.) non ha un
		// equivalente disegnato da questa app -- solo 3 tipi esistono
		// in ChartType (ui/src/Chart.h).
		size_t len = strlen(name);
		if (len > 5 && strncmp(name, "c:", 2) == 0 && strcmp(name + len - 5, "Chart") == 0)
			ctx->result.unsupportedReason = std::string(name).substr(2);
	}

	if (strcmp(name, "c:cat") == 0)
		ctx->kind = ChartXmlContext::eCat;
	else if (strcmp(name, "c:val") == 0)
		ctx->kind = ChartXmlContext::eVal;
	else if (strcmp(name, "c:ser") == 0)
		ctx->result.valRefs.push_back(std::string());
	else if (strcmp(name, "c:title") == 0)
		ctx->inTitle = true;
	else if (strcmp(name, "c:f") == 0)
	{
		ctx->capturingF = true;
		ctx->fText.clear();
	}
	else if (ctx->inTitle && strcmp(name, "a:t") == 0)
		ctx->capturingTitleText = true;
}

static void XMLCALL ChartXmlEnd(void* userData, const char* name)
{
	ChartXmlContext* ctx = (ChartXmlContext*)userData;

	if (strcmp(name, "c:cat") == 0 || strcmp(name, "c:val") == 0)
		ctx->kind = ChartXmlContext::eNone;
	else if (strcmp(name, "c:title") == 0)
		ctx->inTitle = false;
	else if (strcmp(name, "c:f") == 0)
	{
		ctx->capturingF = false;
		if (ctx->kind == ChartXmlContext::eCat)
		{
			if (ctx->result.catRef.empty())
				ctx->result.catRef = ctx->fText;
		}
		else if (ctx->kind == ChartXmlContext::eVal && !ctx->result.valRefs.empty())
			ctx->result.valRefs.back() = ctx->fText;
	}
	else if (strcmp(name, "a:t") == 0)
		ctx->capturingTitleText = false;
}

static void XMLCALL ChartXmlChars(void* userData, const XML_Char* s, int len)
{
	ChartXmlContext* ctx = (ChartXmlContext*)userData;
	if (ctx->capturingF)
		ctx->fText.append(s, len);
	else if (ctx->capturingTitleText)
		ctx->result.title.append(s, len);
}

static bool ParseChartXml(const std::vector<unsigned char>& xml, ChartXmlResult* out)
{
	if (xml.empty())
		return false;

	ChartXmlContext ctx;
	ctx.kind = ChartXmlContext::eNone;
	ctx.inTitle = false;
	ctx.capturingF = false;
	ctx.capturingTitleText = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, ChartXmlStart, ChartXmlEnd);
	XML_SetCharacterDataHandler(parser, ChartXmlChars);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), (int)xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK)
		return false;

	*out = ctx.result;
	return true;
}

// Converte un riferimento tipo "Foglio1!$A$2:$A$5" (o con un nome
// foglio fra apici singoli se contiene spazi, es. "'Dati mensili'!
// $B$2:$B$13") in nome del foglio + range di celle. Un riferimento a
// una sola cella (senza ":") e' valido qui, a differenza di
// ParseMergeCellRef sopra -- un grafico con un solo punto dato e'
// un caso reale. Usato solo per i riferimenti di categoria/valori di
// un grafico (<c:f> dentro <c:cat>/<c:val>), mai per formule generiche.
static bool ParseSheetRangeRef(const std::string& ref, std::string* outSheetName, range* outRange)
{
	std::string sheetName;
	size_t bang;

	if (!ref.empty() && ref[0] == '\'')
	{
		size_t end = ref.find('\'', 1);
		while (end != std::string::npos && end + 1 < ref.size() && ref[end + 1] == '\'')
			end = ref.find('\'', end + 2); // '' = un apice letterale nel nome
		if (end == std::string::npos)
			return false;

		sheetName = ref.substr(1, end - 1);
		size_t p = 0, w = 0;
		while (p < sheetName.size())
		{
			if (sheetName[p] == '\'' && p + 1 < sheetName.size() && sheetName[p + 1] == '\'')
				p++;
			sheetName[w++] = sheetName[p++];
		}
		sheetName.resize(w);

		bang = ref.find('!', end);
	}
	else
	{
		bang = ref.find('!');
		if (bang != std::string::npos)
			sheetName = ref.substr(0, bang);
	}

	if (bang == std::string::npos || bang + 1 >= ref.size())
		return false;

	std::string cellsPart = ref.substr(bang + 1);
	cellsPart.erase(std::remove(cellsPart.begin(), cellsPart.end(), '$'), cellsPart.end());

	size_t colon = cellsPart.find(':');
	int col1, row1, col2, row2;
	if (colon == std::string::npos)
	{
		if (!CellRefToColRow(cellsPart, col1, row1))
			return false;
		col2 = col1;
		row2 = row1;
	}
	else
	{
		if (!CellRefToColRow(cellsPart.substr(0, colon), col1, row1))
			return false;
		if (!CellRefToColRow(cellsPart.substr(colon + 1), col2, row2))
			return false;
	}

	*outSheetName = sheetName;
	outRange->Set(std::min(col1, col2), std::min(row1, row2),
		std::max(col1, col2), std::max(row1, row2));
	return true;
}

// Ricostruisce ChartObject::dataRange da un riferimento di categoria e
// uno o piu' riferimenti di valori (uno per serie), verificando che
// abbiano ESATTAMENTE la forma che questo stesso translator produce in
// esportazione (vedi BuildChartXml/AbsColumnRangeRef sopra): tutti
// sullo stesso foglio del grafico, colonna di categoria seguita
// immediatamente da colonne di valori contigue nello stesso ordine
// delle serie, stessa riga iniziale/finale per tutte. Un grafico che
// non rispetta questa forma (es. valori sparsi, fogli diversi, righe
// diverse) non e' rappresentabile dall'unico "range" rettangolare
// contiguo che ChartObject::dataRange richiede -- trattato come non
// supportato, stesso meccanismo di un tipo di grafico sconosciuto.
static bool ReconstructChartRange(const std::string& sheetName, const std::string& catRefText,
	const std::vector<std::string>& valRefTexts, range* outRange)
{
	if (catRefText.empty() || valRefTexts.empty())
		return false;

	std::string catSheet;
	range catRange;
	if (!ParseSheetRangeRef(catRefText, &catSheet, &catRange))
		return false;
	if (catSheet != sheetName || catRange.left != catRange.right)
		return false;

	int expectedCol = catRange.left + 1;
	for (size_t i = 0; i < valRefTexts.size(); i++)
	{
		std::string valSheet;
		range valRange;
		if (!ParseSheetRangeRef(valRefTexts[i], &valSheet, &valRange))
			return false;
		if (valSheet != sheetName || valRange.left != valRange.right)
			return false;
		if (valRange.left != expectedCol)
			return false;
		if (valRange.top != catRange.top || valRange.bottom != catRange.bottom)
			return false;
		expectedCol++;
	}

	outRange->Set(catRange.left, catRange.top, expectedCol - 1, catRange.bottom);
	return true;
}

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
	std::vector<XlsxChartInfo> charts;
	// Progetto VBA (XLSM, Fase 31): popolato SOLO sul primo foglio (un
	// progetto VBA e' un concetto per l'intera cartella di lavoro, non
	// per foglio, vedi il commento gemello su AscdSheet::vbaProject in
	// ui/src/AscdIO.h), quasi sempre vuoto.
	std::vector<unsigned char> vbaProject;
	// Protezione foglio (Fase 32): da <sheetProtection/> nel foglio
	// XLSX originale, vedi ParseSheet/SheetStart e AscdSheet::
	// isProtected in ui/src/AscdIO.h. Il blocco delle singole celle
	// (CellStyle::fLocked) vive gia' dentro "doc", nessun campo qui.
	bool isProtected = false;
	// Blocca riquadri (100% XLSX standard compatibility, Tier 2): da
	// <pane state="frozen".../> dentro <sheetView> nel foglio XLSX
	// originale, vedi ParseSheet/SheetStart. 0,0 = nessun riquadro
	// bloccato, come SheetView::fFrozenRows/fFrozenCols di default.
	int frozenRows = 0;
	int frozenCols = 0;
};

// Applies each parsed <definedName> (see WorkbookContext::definedNames
// above) to the sheet(s) it belongs to, once every sheet's CContainer
// exists ("100% XLSX standard compatibility" plan, ROADMAP.md).
// Excel's own reserved names (prefix "_xlnm.", e.g. "_xlnm.Print_Area"
// for the print area, "_xlnm.Print_Titles" for repeated print rows/
// columns) are internal bookkeeping, not real named ranges -- skipped
// here rather than polluting the name table; wiring them to this
// app's own print-area/print-settings persistence (AscdIO.h,
// unrelated to the name table) is a separate, not-yet-done follow-up,
// see ROADMAP.md. A workbook-scoped name (no localSheetId) has no
// cross-sheet equivalent in this engine -- CContainer::ResolveName
// only ever looks at its own document's name table, see the comment
// on CContainer::fNames in Container.h -- so it's added to EVERY
// sheet's own table instead: the closest approximation to "visible
// from any sheet" without a bigger cross-sheet name-resolution
// redesign. A sheet-scoped name (localSheetId present) goes only to
// that one sheet.
static void ApplyDefinedNames(const std::vector<DefinedNameInfo>& definedNames,
	std::vector<ParsedSheet>& sheets)
{
	for (size_t i = 0; i < definedNames.size(); i++)
	{
		const DefinedNameInfo& dn = definedNames[i];
		if (dn.name.compare(0, 6, "_xlnm.") == 0)
			continue;

		std::string sheetName;
		range r;
		if (!ParseSheetRangeRef(dn.refText, &sheetName, &r) || !r.IsValid())
			continue;

		if (dn.hasLocalSheetId)
		{
			if (dn.localSheetId >= 0 && (size_t)dn.localSheetId < sheets.size())
				(*sheets[dn.localSheetId].doc->GetOrCreateNameTable())[CName(dn.name.c_str())] = r;
		}
		else
		{
			for (size_t s = 0; s < sheets.size(); s++)
				(*sheets[s].doc->GetOrCreateNameTable())[CName(dn.name.c_str())] = r;
		}
	}
}

// Scrive una cartella di lavoro multi-foglio in formato "ASC2" (vedi
// il commento su kASCDBook2Magic sopra): riusa WriteASCD cosi' com'e'
// per ogni foglio, nessuna duplicazione della serializzazione per
// cella.
static status_t WriteASCDBook(const std::vector<ParsedSheet>& sheets, BPositionIO* dest)
{
	if (dest->Write(kASCDBook2Magic, 4) != 4)
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

		// Il blocco del foglio si scrive prima in memoria (per conoscerne
		// la lunghezza) e poi si riversa nello stream vero, con la sua
		// lunghezza anteposta -- vedi il commento gemello in
		// ui/src/AscdIO.cpp (SaveASCDBook).
		BMallocIO block;
		status_t err = WriteASCD(sheets[i].doc, &block, &sheets[i].colWidths, &sheets[i].images,
			&sheets[i].rowHeights, &sheets[i].showGrid,
			&sheets[i].hasTabColor, &sheets[i].tabColor,
			&sheets[i].hiddenRows, &sheets[i].hasAutoFilter, &sheets[i].autoFilterRange,
			&sheets[i].charts, &sheets[i].vbaProject, &sheets[i].isProtected,
			sheets[i].frozenRows, sheets[i].frozenCols);
		if (err != B_OK)
			return err;

		int32 blockLen = (int32)block.BufferLength();
		if (dest->Write(&blockLen, sizeof(blockLen)) != (ssize_t)sizeof(blockLen))
			return B_IO_ERROR;
		if (blockLen > 0 && dest->Write(block.Buffer(), blockLen) != blockLen)
			return B_IO_ERROR;
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
	// info puo' essere NULL (documentato nel Translation Kit: significa
	// "identifica tu stesso il formato sorgente") -- un vero crash di
	// Tracker (non solo teorico, catturato in un vero .report): il
	// thumbnail worker chiama BTranslatorRoster::Translate() cosi' per
	// ogni file mentre genera le anteprime, senza mai passare da
	// Identify() prima -- senza questo controllo, "info->type" sotto
	// leggeva un indirizzo qualunque, stesso bug corretto per gli altri
	// tre translator (CSV/XLS/ODS, vedi il commento identico li').
	translator_info localInfo;
	if (!info)
	{
		if (Identify(source, NULL, extension, &localInfo, outType) != B_OK)
			return B_NO_TRANSLATOR;
		info = &localInfo;
	}

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
		std::vector<XlsxChartInfo> charts;
		std::vector<unsigned char> vbaProject;
		bool isProtected = false;
		int frozenRows = 0, frozenCols = 0;
		status_t err = ReadASCD(source, doc, &charts, &vbaProject, &isProtected,
			&frozenRows, &frozenCols);
		if (err == B_OK)
			err = (outType == kAtomoNativeFormat) ? WriteASCD(doc, destination)
				: WriteXLSX(doc, charts, destination, vbaProject, isProtected,
					frozenRows, frozenCols);
		doc->Release();
		return err;
	}

	// Bug reale (Fase 16): engine/ e' una libreria statica, collegata
	// separatamente sia nell'eseguibile principale sia in questo
	// stesso translator .so -- ciascuno ha la propria copia
	// INDIPENDENTE di gFuncCount, mai condivisa. App::ReadyToRun()
	// inizializza solo la copia dell'eseguibile: senza questa
	// chiamata, GetFunctionNr tratta ogni funzione con nome (SUM, IF,
	// XLOOKUP, tutte) come sconosciuta SOLO quando il file passa da
	// qui (il vero BTranslatorRoster dell'app in esecuzione), mai nei
	// test che compilano questo stesso file direttamente in un
	// eseguibile di prova (che condivide un'unica copia dei globali,
	// mascherando il problema) -- vedi il commento su
	// EnsureFunctionsInitialized in FunctionUtils.h.
	EnsureFunctionsInitialized();

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
	std::vector<DefinedNameInfo> definedNames;
	std::map<std::string, std::string> relTargets;
	bool date1904 = false; // <workbookPr date1904="1"/>, Fase 12: resta false (predefinito) se il parse sotto non arriva a leggerlo
	if (zip.ReadEntry("xl/workbook.xml", workbookXml)
		&& ParseWorkbookSheetList(workbookXml, sheetList, &date1904, &definedNames)
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

	// Nomi (o tipo XML) di ogni grafico incontrato che questa app non sa
	// disegnare (Fase 25) -- accumulati qui, su tutti i fogli, e passati
	// al chiamante tramite "extension" in fondo alla funzione, MAI
	// mostrati direttamente da questo translator con una BAlert: Tracker
	// chiama Translate() in-process durante la generazione delle
	// anteprime (vedi il commento su "info" sopra), e un dialogo
	// spuntato li' dal nulla sarebbe un vero bug, non solo fastidioso.
	// MainWindow::OpenFile e' l'unico posto che deve reagire a questo.
	std::vector<std::string> unsupportedCharts;

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
		std::vector<HyperlinkRefInfo> hyperlinkRefs;
		std::vector<DataValidationRefInfo> dataValidationRefs;
		if (!ParseSheet(sheetXml, parsed.doc, sharedStrings, &parsed.colWidths, &resolvedStyles,
			&condRules, date1904, &parsed.rowHeights, &parsed.showGrid,
			&parsed.hasTabColor, &parsed.tabColor,
			&parsed.hiddenRows, &parsed.hasAutoFilter, &parsed.autoFilterRange,
			&parsed.isProtected, &hyperlinkRefs, &dataValidationRefs,
			&parsed.frozenRows, &parsed.frozenCols))
		{
			parsed.doc->Release();
			err = B_BAD_DATA;
			break;
		}

		// Collegamenti ipertestuali INTERNI (100% XLSX standard
		// compatibility, Tier 2): "location" e' gia' il riferimento
		// vero (es. "Foglio1!A1"), nessun _rels da risolvere -- quelli
		// ESTERNI (r:id) si risolvono sotto, insieme a tabelle/disegni/
		// commenti, con gli stessi _rels di questo foglio.
		for (size_t hi = 0; hi < hyperlinkRefs.size(); hi++)
		{
			if (hyperlinkRefs[hi].rId.empty() && !hyperlinkRefs[hi].location.empty())
			{
				cell c;
				c.Set(hyperlinkRefs[hi].ref.c_str());
				if (c.IsValid())
					parsed.doc->SetHyperlink(c, hyperlinkRefs[hi].location);
			}
		}

		// Convalida dati (100% XLSX standard compatibility, Tier 2):
		// nessun _rels da risolvere, applicata direttamente da quanto
		// raccolto sopra da ParseSheet.
		ApplyDataValidation(parsed.doc, dataValidationRefs);

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
				// Collegamenti ipertestuali ESTERNI: il target di una
				// relazione "hyperlink" (TargetMode="External") e' gia'
				// l'URL vero, a differenza di tabelle/disegni/commenti
				// sotto -- nessun prefisso "xl/" da ricostruire.
				for (size_t hi = 0; hi < hyperlinkRefs.size(); hi++)
				{
					if (hyperlinkRefs[hi].rId.empty())
						continue;
					std::map<std::string, std::string>::iterator rit =
						sheetRelTargets.find(hyperlinkRefs[hi].rId);
					if (rit == sheetRelTargets.end())
						continue;
					cell c;
					c.Set(hyperlinkRefs[hi].ref.c_str());
					if (c.IsValid())
						parsed.doc->SetHyperlink(c, rit->second);
				}

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
					else if (target.compare(0, 8, "comments") == 0)
					{
						// Commenti/note per cella (100% XLSX standard
						// compatibility, Tier 2): a differenza di
						// tabelle/disegni sopra/sotto, xl/comments{N}.xml
						// vive direttamente sotto "xl/", non in una
						// propria sottocartella (il target del _rels e'
						// tipicamente "../comments1.xml", non
						// "comments/comments1.xml") -- e' gia' l'elenco
						// finale (cella, testo), niente ulteriore livello
						// di _rels da risolvere. Il VML legacy che Excel
						// scrive di solito accanto (xl/drawings/
						// vmlDrawingN.vml, solo posizione/visibilita' del
						// riquadro) non serve: questa app disegna il
						// proprio indicatore da CContainer::HasComment,
						// senza leggere alcuna geometria dal file
						// originale.
						std::string commentsPath = "xl/" + target;
						std::vector<unsigned char> commentsXml;
						std::vector<CommentEntry> entries;
						if (zip.ReadEntry(commentsPath.c_str(), commentsXml)
							&& ParseComments(commentsXml, &entries))
						{
							for (size_t ci = 0; ci < entries.size(); ci++)
							{
								cell c;
								c.Set(entries[ci].ref.c_str());
								if (c.IsValid())
									parsed.doc->SetComment(c, entries[ci].text);
							}
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
							// Un anchor con isChart=true rimanda a
							// xl/charts/chartN.xml (Fase 25, gestito nel
							// ciclo dedicato subito sotto), MAI a
							// un'immagine sotto xl/media/ -- senza questo
							// filtro, il file XML del grafico veniva letto
							// come se fosse un PNG (bug reale, scoperto
							// insieme a quello sull'xdr:ext qui sopra: solo
							// dopo aver corretto quello, extCxEmu smetteva
							// di essere zero per sbaglio, e questo ramo
							// iniziava davvero a produrre un'"immagine"
							// incorporata fasulla con dentro i byte XML del
							// grafico spacciati per PNG).
							if (pics[p].isChart)
								continue;

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

						// Grafici incorporati (Fase 25): stesso file di
						// disegno, stessi _rels gia' risolti sopra per le
						// immagini -- un anchor con isChart=true rimanda,
						// tramite relId, a xl/charts/chartN.xml invece che
						// a un'immagine sotto xl/media/.
						for (size_t p = 0; p < pics.size(); p++)
						{
							if (!pics[p].isChart)
								continue;

							std::map<std::string, std::string>::iterator rit =
								drawingRelTargets.find(pics[p].relId);
							if (rit == drawingRelTargets.end())
								continue;

							std::string chartTarget = rit->second;
							if (chartTarget.compare(0, 3, "../") == 0)
								chartTarget = chartTarget.substr(3);
							std::string chartPath = "xl/" + chartTarget;

							std::vector<unsigned char> chartXml;
							ChartXmlResult chartResult;
							if (!zip.ReadEntry(chartPath.c_str(), chartXml)
								|| !ParseChartXml(chartXml, &chartResult))
								continue;

							if (!chartResult.typeRecognized)
							{
								unsupportedCharts.push_back(chartResult.unsupportedReason.empty()
									? std::string("sconosciuto") : chartResult.unsupportedReason);
								continue;
							}

							range dataRange;
							if (!ReconstructChartRange(parsed.name, chartResult.catRef,
								chartResult.valRefs, &dataRange))
							{
								unsupportedCharts.push_back("layout dati non compatibile");
								continue;
							}

							// Dimensione dell'ancoraggio in pixel: se manca
							// un <xdr:ext> esplicito (il caso comune per un
							// vero xdr:twoCellAnchor), si ricava da "to" -
							// "from" usando la larghezza/altezza PREDEFINITA
							// di colonna/riga (SheetView::kColWidth/
							// kRowHeight, 80/20 px) -- un'approssimazione: a
							// differenza delle immagini (Fase 12), qui non
							// c'e' nessuna "dimensione naturale" di riserva,
							// e le larghezze/altezze VERE della SheetView non
							// sono note in questa fase di importazione.
							static const float kDefColWidth = 80.0f, kDefRowHeight = 20.0f;
							float left, top;
							if (pics[p].isAbsolute)
							{
								// <xdr:absoluteAnchor>: posizione pixel gia'
								// assoluta (<xdr:pos>), nessuna cella di
								// ancoraggio da convertire -- il caso scritto
								// da BuildChartXml per i grafici esportati da
								// questa stessa app (vedi il commento su
								// isAbsolute in DrawingPic sopra).
								left = (float)(pics[p].absXEmu / kEmuPerPixel);
								top = (float)(pics[p].absYEmu / kEmuPerPixel);
							}
							else
							{
								left = pics[p].fromCol * kDefColWidth
									+ (float)(pics[p].fromColOffEmu / kEmuPerPixel);
								top = pics[p].fromRow * kDefRowHeight
									+ (float)(pics[p].fromRowOffEmu / kEmuPerPixel);
							}
							float width, height;
							if (pics[p].extCxEmu > 0 && pics[p].extCyEmu > 0)
							{
								width = (float)(pics[p].extCxEmu / kEmuPerPixel);
								height = (float)(pics[p].extCyEmu / kEmuPerPixel);
							}
							else if (pics[p].hasTo)
							{
								float right = pics[p].toCol * kDefColWidth
									+ (float)(pics[p].toColOffEmu / kEmuPerPixel);
								float bottom = pics[p].toRow * kDefRowHeight
									+ (float)(pics[p].toRowOffEmu / kEmuPerPixel);
								width = right - left;
								height = bottom - top;
							}
							else
							{
								width = 400.0f;
								height = 300.0f;
							}
							if (width <= 0 || height <= 0)
								continue;

							XlsxChartInfo info;
							info.dataLeft = (int16)dataRange.left;
							info.dataTop = (int16)dataRange.top;
							info.dataRight = (int16)dataRange.right;
							info.dataBottom = (int16)dataRange.bottom;
							info.frameLeft = left;
							info.frameTop = top;
							info.frameRight = left + width;
							info.frameBottom = top + height;
							info.type = chartResult.type;
							info.title = chartResult.title;
							parsed.charts.push_back(info);
						}
					}
				}
			}
		}

		sheets.push_back(parsed);
	}

	// Nomi definiti (<definedNames>, "100% XLSX standard compatibility"
	// plan): applicati ORA che ogni foglio ha gia' il suo CContainer --
	// vedi ApplyDefinedNames sopra.
	if (err == B_OK)
		ApplyDefinedNames(definedNames, sheets);

	// Progetto VBA (XLSM, Fase 31): se l'archivio originale ne ha uno,
	// lo si legge grezzo e lo si appende SOLO al primo foglio (vedi il
	// commento su ParsedSheet::vbaProject sopra) -- ne' l'importazione
	// ne' l'esportazione qui sotto lo analizzano mai, solo lo
	// trasportano cosi' com'e' cosi' che riaprire e risalvare un file
	// con macro non le distrugga piu' in silenzio.
	if (err == B_OK && !sheets.empty())
		zip.ReadEntry("xl/vbaProject.bin", sheets[0].vbaProject); // opzionale

	if (err == B_OK)
	{
		if (outType == kAtomoNativeFormat)
			err = WriteASCDBook(sheets, destination);
		else
			// L'esportazione XLSX resta a un solo foglio (quello
			// attivo, il primo qui): i writer non nativi non
			// supportano ancora piu' fogli, vedi WriteXLSX. I grafici
			// del primo foglio (Fase 25, appena ricostruiti sopra da
			// xl/charts/*.xml) vengono ripassati cosi' com'e' a
			// WriteXLSX: un vero file XLSX in ingresso riscritto in
			// uscita mantiene i suoi grafici invece di perderli.
			err = WriteXLSX(sheets[0].doc, sheets[0].charts, destination,
				sheets[0].vbaProject, sheets[0].isProtected,
				sheets[0].frozenRows, sheets[0].frozenCols);
	}

	if (err == B_OK && extension != NULL && !unsupportedCharts.empty())
	{
		// Comunica i grafici non disegnabili a MainWindow::OpenFile
		// (vedi il commento su "unsupportedCharts" sopra) senza toccare
		// il formato ASCD/ASCB -- un campo BMessage separato dal flusso
		// dati vero e proprio, stesso principio gia' seguito per ogni
		// altra informazione "fuori banda" di questo translator.
		for (size_t i = 0; i < unsupportedCharts.size(); i++)
			extension->AddString("atomo:unsupportedChart", unsupportedCharts[i].c_str());
	}

	for (size_t i = 0; i < sheets.size(); i++)
		sheets[i].doc->Release();

	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
	{
		// Vedi il commento analogo in translators/csv/CsvTranslator.cpp
		// (make_nth_translator): risolve il catalogo di questo add-on
		// dal proprio image_id, invece del catalogo dell'host.
		if (sCatalog.InitCheck() != B_OK)
		{
			image_info info;
			if (get_image_info(you, &info) == B_OK)
			{
				BEntry entry(info.name);
				entry_ref ref;
				if (entry.GetRef(&ref) == B_OK)
					sCatalog.SetTo(ref);
			}
		}
		return new CXlsxTranslator();
	}
	return NULL;
}
