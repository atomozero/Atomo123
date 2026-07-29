/*
	OdsTranslator.cpp

	Vedi OdsTranslator.h per la descrizione generale.
*/

#include "OdsTranslator.h"
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
		kAtomoOdsFormat, kAtomoSheetGroup, 0.9f, 0.9f,
		"application/vnd.oasis.opendocument.spreadsheet",
		"OpenDocument Spreadsheet (ODS)"
	}
};

static const translation_format sOutputFormats[] = {
	{
		kAtomoNativeFormat, kAtomoSheetGroup, 1.0f, 1.0f,
		"application/x-vnd.atomo-sheet-data", "Atomo Sheet Cell Data (ASCD)"
	}
};

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
static const int32 kASCDVersion = 1;

// Stessa serializzazione ASCD degli altri translator (vedi
// translators/csv/CsvTranslator.cpp per la descrizione completa).
static status_t WriteASCD(CContainer* doc, BPositionIO* dest)
{
	range bounds;
	doc->GetBounds(bounds);

	int32 count = 0;
	CCellIterator counter(doc, &bounds);
	cell c;
	while (counter.NextExisting(c))
		count++;

	if (dest->Write(kASCDMagic, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kASCDVersion, sizeof(kASCDVersion)) != (ssize_t)sizeof(kASCDVersion))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;

	CCellIterator iter(doc, &bounds);
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

// Converte una formula ODF ("of:=[.A1]+[.B2]") nel testo di formula
// che il nostro parser si aspetta ("A1+B2"): toglie il prefisso
// "of:=" (o solo "="), e per ogni riferimento di cella tra "[." e
// "]" rimuove le parentesi e i simboli "$" (riferimenti assoluti).
// Non gestisce riferimenti a fogli diversi (es. "[Sheet2.A1]") ne'
// intervalli con ":" dentro le stesse parentesi quadre in modo
// completo -- limite noto, documentato in docs/TRANSLATORS.md.
static std::string ConvertODFFormula(const std::string& formula)
{
	std::string in = formula;
	if (in.compare(0, 4, "of:=") == 0)
		in = in.substr(4);
	else if (!in.empty() && in[0] == '=')
		in = in.substr(1);

	std::string out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); i++)
	{
		if (in[i] == '[' && i + 1 < in.size() && in[i + 1] == '.')
		{
			i += 2; // salta "[."
			while (i < in.size() && in[i] != ']')
			{
				if (in[i] != '$')
					out += in[i];
				i++;
			}
			// i punta a ']' (o a fine stringa): il for lo saltera'
			// incrementando ancora, quindi non serve altro.
		}
		else
			out += in[i];
	}
	return out;
}

// --- Parsing di content.xml ----------------------------------------------
//
// Struttura minima gestita (schema OpenDocument):
//   <office:spreadsheet>
//     <table:table table:name="Foglio1">
//       <table:table-row>
//         <table:table-cell office:value-type="float" office:value="15"/>
//         <table:table-cell office:value-type="string"><text:p>ciao</text:p></table:table-cell>
//         <table:table-cell table:formula="of:=[.A1]+[.B1]"
//             office:value-type="float" office:value="40"/>
//         <table:table-cell table:number-columns-repeated="10"/>  -- celle vuote, si saltano
//       </table:table-row>
//     </table:table>
//   </office:spreadsheet>
//
// A differenza di XLSX, le celle ODF non hanno un riferimento
// esplicito ("r=\"A1\""): la posizione si ricava contando righe e
// colonne mentre si scorre il documento, tenendo conto degli
// attributi "table:number-rows-repeated" / "table:number-columns-
// repeated" usati per comprimere intervalli di celle vuote. Si
// importa solo la prima <table:table> (primo foglio): come per
// XLSX/sheet1, i fogli successivi sono un limite noto.

struct SheetContext {
	CContainer* doc;

	int tableDepth;       // quante <table:table> aperte finora (per fermarsi alla prima)
	bool inTargetTable;    // stiamo dentro la prima <table:table>?
	bool firstTableDone;   // la prima tabella e' gia' stata chiusa?

	int curRow;
	int curCol;

	// attributi/contenuto della cella corrente
	std::string valueType;
	std::string value;      // office:value / office:boolean-value / office:string-value
	std::string formula;    // table:formula
	int columnsRepeated;
	int rowsRepeated;

	bool inTextP;
	std::string textContent; // testo dentro <text:p>, per le celle stringa
};

static const char* FindAttr(const char** atts, const char* name)
{
	for (int i = 0; atts[i]; i += 2)
	{
		if (strcmp(atts[i], name) == 0)
			return atts[i + 1];
	}
	return NULL;
}

static void ImportCell(SheetContext* ctx)
{
	if (ctx->curCol <= 0 || ctx->curRow <= 0)
		return;

	std::string text;
	if (!ctx->formula.empty())
		text = "=" + ConvertODFFormula(ctx->formula);
	else if (ctx->valueType == "string")
		text = ctx->textContent;
	else if (!ctx->value.empty())
		text = ctx->value;
	else if (!ctx->textContent.empty())
		text = ctx->textContent; // fallback: valore solo come testo visualizzato

	if (text.empty())
		return;

	cell loc(ctx->curCol, ctx->curRow);
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

static void XMLCALL ContentStart(void* userData, const char* name, const char** atts)
{
	SheetContext* ctx = (SheetContext*)userData;

	if (strcmp(name, "table:table") == 0)
	{
		if (ctx->tableDepth == 0 && !ctx->firstTableDone)
			ctx->inTargetTable = true;
		ctx->tableDepth++;
	}
	else if (!ctx->inTargetTable)
		return;
	else if (strcmp(name, "table:table-row") == 0)
	{
		ctx->curCol = 1;
		ctx->rowsRepeated = 1;
		const char* rep = FindAttr(atts, "table:number-rows-repeated");
		if (rep)
			ctx->rowsRepeated = atoi(rep);
		if (ctx->rowsRepeated < 1)
			ctx->rowsRepeated = 1;
	}
	else if (strcmp(name, "table:table-cell") == 0
		|| strcmp(name, "table:covered-table-cell") == 0)
	{
		ctx->valueType.clear();
		ctx->value.clear();
		ctx->formula.clear();
		ctx->textContent.clear();
		ctx->columnsRepeated = 1;

		const char* vt = FindAttr(atts, "office:value-type");
		if (vt)
			ctx->valueType = vt;

		const char* v = FindAttr(atts, "office:value");
		if (!v)
			v = FindAttr(atts, "office:boolean-value");
		if (!v)
			v = FindAttr(atts, "office:string-value");
		if (v)
			ctx->value = v;

		const char* f = FindAttr(atts, "table:formula");
		if (f)
			ctx->formula = f;

		const char* rep = FindAttr(atts, "table:number-columns-repeated");
		if (rep)
			ctx->columnsRepeated = atoi(rep);
		if (ctx->columnsRepeated < 1)
			ctx->columnsRepeated = 1;
	}
	else if (strcmp(name, "text:p") == 0)
		ctx->inTextP = true;
}

static void XMLCALL ContentEnd(void* userData, const char* name)
{
	SheetContext* ctx = (SheetContext*)userData;

	if (strcmp(name, "table:table") == 0)
	{
		ctx->tableDepth--;
		if (ctx->tableDepth == 0 && ctx->inTargetTable)
		{
			ctx->inTargetTable = false;
			ctx->firstTableDone = true;
		}
		return;
	}

	if (!ctx->inTargetTable)
		return;

	if (strcmp(name, "table:table-row") == 0)
		ctx->curRow += ctx->rowsRepeated;
	else if (strcmp(name, "table:table-cell") == 0
		|| strcmp(name, "table:covered-table-cell") == 0)
	{
		// Le celle vuote ripetute (spesso migliaia, fino al margine
		// del foglio) non contengono nulla da importare: si avanza
		// solo il contatore di colonna, senza chiamare ImportCell.
		bool hasContent = !ctx->formula.empty() || !ctx->valueType.empty()
			|| !ctx->textContent.empty();
		if (hasContent)
			ImportCell(ctx);
		ctx->curCol += ctx->columnsRepeated;
	}
	else if (strcmp(name, "text:p") == 0)
		ctx->inTextP = false;
}

static void XMLCALL ContentChars(void* userData, const char* s, int len)
{
	SheetContext* ctx = (SheetContext*)userData;
	if (ctx->inTextP)
		ctx->textContent.append(s, len);
}

static bool ParseContent(const std::vector<unsigned char>& xml, CContainer* doc)
{
	SheetContext ctx;
	ctx.doc = doc;
	ctx.tableDepth = 0;
	ctx.inTargetTable = false;
	ctx.firstTableDone = false;
	ctx.curRow = 1;
	ctx.curCol = 1;
	ctx.columnsRepeated = 1;
	ctx.rowsRepeated = 1;
	ctx.inTextP = false;

	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, &ctx);
	XML_SetElementHandler(parser, ContentStart, ContentEnd);
	XML_SetCharacterDataHandler(parser, ContentChars);

	XML_Status status = XML_Parse(parser, (const char*)xml.data(), xml.size(), 1);
	XML_ParserFree(parser);

	return status == XML_STATUS_OK;
}

COdsTranslator::COdsTranslator()
	: BTranslator()
{
}

COdsTranslator::~COdsTranslator()
{
}

const char* COdsTranslator::TranslatorName() const
{
	return "ODS Translator";
}

const char* COdsTranslator::TranslatorInfo() const
{
	return "Importa fogli di calcolo dal formato OpenDocument (ODS)";
}

int32 COdsTranslator::TranslatorVersion() const
{
	return B_TRANSLATION_MAKE_VERSION(1, 0, 0);
}

const translation_format* COdsTranslator::InputFormats(int32* _count) const
{
	*_count = 1;
	return sInputFormats;
}

const translation_format* COdsTranslator::OutputFormats(int32* _count) const
{
	*_count = 1;
	return sOutputFormats;
}

status_t COdsTranslator::Identify(BPositionIO* source,
	const translation_format* format, BMessage* extension,
	translator_info* info, uint32 outType)
{
	off_t pos = source->Position();
	unsigned char header[4];
	ssize_t read = source->Read(header, 4);
	source->Seek(pos, SEEK_SET);

	// Firma ZIP locale ("PK\x03\x04"): necessaria ma non sufficiente
	// -- si verifica anche la presenza di content.xml e del
	// manifest, che solo i pacchetti OpenDocument hanno (e non
	// XLSX, che ha invece [Content_Types].xml).
	static const unsigned char kZipSig[4] = { 'P', 'K', 0x03, 0x04 };
	if (read != 4 || memcmp(header, kZipSig, 4) != 0)
		return B_NO_TRANSLATOR;

	CZipReader zip;
	if (!zip.Open(source) || !zip.HasEntry("content.xml")
		|| !zip.HasEntry("META-INF/manifest.xml"))
	{
		source->Seek(pos, SEEK_SET);
		return B_NO_TRANSLATOR;
	}
	source->Seek(pos, SEEK_SET);

	info->type = kAtomoOdsFormat;
	info->group = kAtomoSheetGroup;
	info->quality = 0.9f;
	info->capability = 0.9f;
	strlcpy(info->name, "OpenDocument Spreadsheet (ODS)", sizeof(info->name));
	strlcpy(info->MIME, "application/vnd.oasis.opendocument.spreadsheet",
		sizeof(info->MIME));

	return B_OK;
}

status_t COdsTranslator::Translate(BPositionIO* source,
	const translator_info* info, BMessage* extension, uint32 outType,
	BPositionIO* destination)
{
	if (info->type != kAtomoOdsFormat)
		return B_NO_TRANSLATOR;
	if (outType != 0 && outType != kAtomoNativeFormat)
		return B_NO_TRANSLATOR;

	CZipReader zip;
	if (!zip.Open(source))
		return B_BAD_DATA;

	std::vector<unsigned char> contentXml;
	if (!zip.ReadEntry("content.xml", contentXml))
		return B_BAD_DATA;

	CContainer* doc = new CContainer(NULL, NULL);
	status_t err = B_OK;

	if (!ParseContent(contentXml, doc))
		err = B_BAD_DATA;

	if (err == B_OK)
		err = WriteASCD(doc, destination);

	doc->Release();
	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
		return new COdsTranslator();
	return NULL;
}
