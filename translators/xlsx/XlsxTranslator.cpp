/*
	XlsxTranslator.cpp

	Vedi XlsxTranslator.h per la descrizione generale.
*/

#include "XlsxTranslator.h"
#include "MiniZip.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <expat.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"

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

// Stessa serializzazione ASCD degli altri translator (vedi
// translators/csv/CsvTranslator.cpp per la descrizione completa).
static status_t WriteASCD(CContainer* doc, BPositionIO* dest)
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
		char text[512];
		doc->GetCellFormula(c, text, false);

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

		char text[512];
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

	std::string cellRef;
	std::string cellType; // valore dell'attributo t="..." (puo' essere vuoto)
	std::string value;    // testo dentro <v>
	std::string formula;  // testo dentro <f>
	bool inValue;
	bool inFormula;
};

static void XMLCALL SheetStart(void* userData, const char* name, const char** atts)
{
	SheetContext* ctx = (SheetContext*)userData;

	if (strcmp(name, "c") == 0)
	{
		ctx->cellRef.clear();
		ctx->cellType.clear();
		ctx->value.clear();
		ctx->formula.clear();
		for (int i = 0; atts[i]; i += 2)
		{
			if (strcmp(atts[i], "r") == 0)
				ctx->cellRef = atts[i + 1];
			else if (strcmp(atts[i], "t") == 0)
				ctx->cellType = atts[i + 1];
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

		if (text.empty())
			return;

		cell loc(col, row);
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
	const std::vector<std::string>& sharedStrings)
{
	SheetContext ctx;
	ctx.doc = doc;
	ctx.sharedStrings = &sharedStrings;
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

	CContainer* doc = new CContainer(NULL, NULL);
	status_t err = B_OK;

	if (info->type == kAtomoNativeFormat)
		err = ReadASCD(source, doc);
	else
	{
		CZipReader zip;
		if (!zip.Open(source))
			err = B_BAD_DATA;

		// Il primo foglio e' sempre xl/worksheets/sheet1.xml nei
		// pacchetti generati da strumenti standard (l'ordine reale dei
		// fogli e' tecnicamente definito da xl/workbook.xml + i _rels,
		// ma per un documento con un solo foglio -- il caso comune --
		// sheet1.xml e' sempre quello giusto).
		if (err == B_OK && !zip.HasEntry("xl/worksheets/sheet1.xml"))
			err = B_BAD_DATA;

		std::vector<unsigned char> sharedStringsXml;
		if (err == B_OK)
			zip.ReadEntry("xl/sharedStrings.xml", sharedStringsXml); // opzionale

		std::vector<std::string> sharedStrings;
		if (err == B_OK && !ParseSharedStrings(sharedStringsXml, sharedStrings))
			err = B_BAD_DATA;

		std::vector<unsigned char> sheetXml;
		if (err == B_OK && !zip.ReadEntry("xl/worksheets/sheet1.xml", sheetXml))
			err = B_BAD_DATA;

		if (err == B_OK && !ParseSheet(sheetXml, doc, sharedStrings))
			err = B_BAD_DATA;
	}

	if (err == B_OK)
	{
		if (outType == kAtomoNativeFormat)
			err = WriteASCD(doc, destination);
		else
			err = WriteXLSX(doc, destination);
	}

	doc->Release();
	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
		return new CXlsxTranslator();
	return NULL;
}
