/*
	XlsxTranslator.cpp

	Vedi XlsxTranslator.h per la descrizione generale.
*/

#include "XlsxTranslator.h"
#include "MiniZip.h"

#include <algorithm>
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
static const int32 kASCDVersion = 1;
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
	const std::vector<std::pair<int, float> >* colWidths = NULL)
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

		if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_IO_ERROR;
		if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_IO_ERROR;
		if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
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

	// Sezione altezze di riga, Blocca riquadri, allineamento di cella,
	// bordi di cella, tutte in coda (Fase 10/11 di ui/src/AscdIO.cpp):
	// sempre scritte vuote/a zero qui, questo translator non estrae
	// ancora nessuna di queste informazioni dal file XLSX originale (a
	// differenza di larghezza di colonna, colori e -- sotto -- font
	// sopra). Il campo va comunque scritto sempre, mai omesso, per lo
	// stesso motivo della sezione grafici sopra: LoadASCD (in ui/src/
	// AscdIO.cpp, che legge questo stesso flusso) si aspetta ORA tutte
	// queste sezioni in coda a ogni blocco ASCD -- ometterle
	// disallineerebbe la lettura del blocco successivo in una cartella
	// di lavoro multi-foglio, esattamente come il bug gia' descritto
	// sopra per i grafici (bug reale scoperto aprendo di nuovo lo
	// stesso file .xlsm da 38 fogli dopo l'aggiunta di quelle sezioni
	// in Fase 10/11: leggeva byte del foglio successivo come se
	// fossero l'altezza di una riga del foglio corrente).
	int32 rowHeightCount = 0;
	if (dest->Write(&rowHeightCount, sizeof(rowHeightCount)) != (ssize_t)sizeof(rowHeightCount))
		return B_IO_ERROR;

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

	int32 alignCount = 0;
	if (dest->Write(&alignCount, sizeof(alignCount)) != (ssize_t)sizeof(alignCount))
		return B_IO_ERROR;

	int32 borderCount = 0;
	if (dest->Write(&borderCount, sizeof(borderCount)) != (ssize_t)sizeof(borderCount))
		return B_IO_ERROR;

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
	if (version != kASCDVersion)
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

		char text[4096];
		if (len < 0 || len >= (int32)sizeof(text))
			return B_BAD_DATA;
		if (len > 0 && source->Read(text, len) != len)
			return B_BAD_DATA;
		text[len] = 0;

		cell c(col, row);
		try
		{
			TryToParseString(text, c, doc, true);
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
	bool hasFontStyle; // true solo se grassetto e/o corsivo (il Regular predefinito non serve applicarlo)
	int fontID; // indice gia' risolto in gFontSizeTable, pronto per CellStyle::fFont
};

enum StylesSection { kStylesNone, kStylesNumFmts, kStylesFills, kStylesFonts, kStylesCellXfs };

// (fontId, fillId, numFmtId) per indice s= di una cella/colonna.
struct XfInfo {
	int fontId;
	int fillId;
	int numFmtId;
};

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
	std::vector<float> fontSize; // -1 = non specificata nel font XLSX (<sz> assente)

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

static void XMLCALL StylesStart(void* userData, const char* name, const char** atts)
{
	StylesContext* ctx = (StylesContext*)userData;

	if (strcmp(name, "numFmts") == 0) { ctx->section = kStylesNumFmts; return; }
	if (strcmp(name, "fills") == 0) { ctx->section = kStylesFills; return; }
	if (strcmp(name, "fonts") == 0) { ctx->section = kStylesFonts; return; }
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
	}
	else if (ctx->section == kStylesCellXfs)
	{
		if (strcmp(name, "xf") == 0)
		{
			XfInfo xf;
			xf.fontId = 0;
			xf.fillId = 0;
			xf.numFmtId = 0;
			for (int i = 0; atts[i]; i += 2)
			{
				if (strcmp(atts[i], "fontId") == 0)
					xf.fontId = atoi(atts[i + 1]);
				else if (strcmp(atts[i], "fillId") == 0)
					xf.fillId = atoi(atts[i + 1]);
				else if (strcmp(atts[i], "numFmtId") == 0)
					xf.numFmtId = atoi(atts[i + 1]);
			}
			ctx->cellXfs.push_back(xf);
		}
	}
}

static void XMLCALL StylesEnd(void* userData, const char* name)
{
	StylesContext* ctx = (StylesContext*)userData;
	if (strcmp(name, "numFmts") == 0 || strcmp(name, "fills") == 0
		|| strcmp(name, "fonts") == 0 || strcmp(name, "cellXfs") == 0)
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
		(*out)[i] = rs;
	}
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

struct SheetContext {
	CContainer* doc;
	const std::vector<std::string>* sharedStrings;
	std::vector<std::pair<int, float> >* colWidths; // opzionale (NULL = non raccolte)
	const std::vector<ResolvedStyle>* styles; // opzionale (NULL = non applica colori)

	std::string cellRef;
	std::string cellType; // valore dell'attributo t="..." (puo' essere vuoto)
	std::string value;    // testo dentro <v>
	std::string formula;  // testo dentro <f>
	int cellStyleIndex;   // valore di s="..." sulla cella corrente, -1 se assente
	bool inValue;
	bool inFormula;
};

// La larghezza in <col width="..."> e' nell'unita' di misura di Excel
// ("numero di caratteri '0' del carattere piu' largo del font
// predefinito", non pixel): la formula esatta dipende dal font/DPI del
// documento originale (vedi lo standard ECMA-376, 18.3.1.13), che
// questo progetto non replica. Approssimazione ampiamente usata da
// importatori piu' semplici (basata su un carattere largo circa 7
// pixel per il font predefinito Calibri 11, piu' un margine fisso di
// 5 pixel) -- visivamente ragionevole, non un valore esatto pixel per
// pixel come lo mostrerebbe Excel stesso.
static float ExcelColWidthToPixels(double charWidth)
{
	return (float)(charWidth * 7.0 + 5.0);
}

static void XMLCALL SheetStart(void* userData, const char* name, const char** atts)
{
	SheetContext* ctx = (SheetContext*)userData;

	if (strcmp(name, "c") == 0)
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
				if (rs.hasBg || rs.hasFg || rs.hasFormat || rs.hasFontStyle)
				{
					for (int col = min; col <= clampedMax; col++)
					{
						CellStyle cs;
						ctx->doc->GetColumnStyle(col, cs);
						if (rs.hasBg) cs.fLowColor = rs.bg;
						if (rs.hasFg) cs.fHighColor = rs.fg;
						if (rs.hasFormat) cs.fFormat = rs.format;
						if (rs.hasFontStyle) cs.fFont = rs.fontID;
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

		if (!text.empty())
		{
			try
			{
				TryToParseString(text.c_str(), loc, ctx->doc, false);
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
			if (rs.hasBg || rs.hasFg || rs.hasFormat || rs.hasFontStyle)
			{
				CellStyle cs;
				ctx->doc->GetCellStyle(loc, cs);
				if (rs.hasBg) cs.fLowColor = rs.bg;
				if (rs.hasFg) cs.fHighColor = rs.fg;
				if (rs.hasFormat) cs.fFormat = rs.format;
				if (rs.hasFontStyle) cs.fFont = rs.fontID;
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
}

static bool ParseSheet(const std::vector<unsigned char>& xml, CContainer* doc,
	const std::vector<std::string>& sharedStrings,
	std::vector<std::pair<int, float> >* colWidths,
	const std::vector<ResolvedStyle>* styles)
{
	SheetContext ctx;
	ctx.doc = doc;
	ctx.sharedStrings = &sharedStrings;
	ctx.colWidths = colWidths;
	ctx.styles = styles;
	ctx.cellStyleIndex = -1;
	ctx.inValue = false;
	ctx.inFormula = false;

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
}

static void XMLCALL WorkbookEnd(void* userData, const char* name)
{
	WorkbookContext* ctx = (WorkbookContext*)userData;
	if (strcmp(name, "sheets") == 0)
		ctx->inSheets = false;
}

static bool ParseWorkbookSheetList(const std::vector<unsigned char>& xml,
	std::vector<WorkbookSheetInfo>& out)
{
	if (xml.empty())
		return false;

	WorkbookContext ctx;
	ctx.inSheets = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, WorkbookStart, WorkbookEnd);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	if (status != XML_STATUS_OK || ctx.sheets.empty())
		return false;

	out = ctx.sheets;
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

// Un foglio gia' analizzato, pronto per essere scritto in formato
// ASCD/ASCB: nome, documento, e le sole colonne con una larghezza
// esplicita nel file XLSX originale (vedi ParseSheet/SheetStart).
struct ParsedSheet {
	std::string name;
	CContainer* doc;
	std::vector<std::pair<int, float> > colWidths;
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

		status_t err = WriteASCD(sheets[i].doc, dest, &sheets[i].colWidths);
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

	std::vector<std::pair<std::string, std::string> > sheetsToRead; // (nome, percorso XML)

	std::vector<unsigned char> workbookXml, relsXml;
	std::vector<WorkbookSheetInfo> sheetList;
	std::map<std::string, std::string> relTargets;
	if (zip.ReadEntry("xl/workbook.xml", workbookXml)
		&& ParseWorkbookSheetList(workbookXml, sheetList)
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
		if (!ParseSheet(sheetXml, parsed.doc, sharedStrings, &parsed.colWidths, &resolvedStyles))
		{
			parsed.doc->Release();
			err = B_BAD_DATA;
			break;
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
