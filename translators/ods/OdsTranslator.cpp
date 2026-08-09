/*
	OdsTranslator.cpp

	Vedi OdsTranslator.h per la descrizione generale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
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
#include "FunctionUtils.h"

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
	},
	{
		kAtomoOdsFormat, kAtomoSheetGroup, 0.7f, 0.7f,
		"application/vnd.oasis.opendocument.spreadsheet",
		"OpenDocument Spreadsheet (ODS)"
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
	// prodotto (stesso ragionamento del ciclo di ricalcolo sotto).
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

	return B_OK;
}

// Legge un flusso ASCD e ricostruisce le celle in "doc" (vuoto in
// ingresso) -- stessa logica di CsvTranslator.cpp, usata qui per
// l'esportazione (ASCD -> ODS, la direzione opposta della normale
// importazione ODS -> ASCD gestita da ParseContent/WriteASCD sotto).
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

	// Le formule vanno calcolate prima di esportare (l'export ODS
	// scrive solo valori, non formule -- vedi BuildContentXml sotto),
	// altrimenti una cella con formula risulterebbe vuota. Stesso
	// meccanismo "piu' passate fino a convergenza" gia' usato altrove
	// in questo progetto (vedi ui/src/AscdIO.cpp).
	//
	// L'iteratore usa il range completo del foglio (non i limiti
	// restituiti da GetBounds) perche' GetBounds esclude le celle con
	// mType eNoData -- esattamente lo stato di una formula appena
	// analizzata da TryToParseString e non ancora calcolata. Se quella
	// cella e' anche la piu' a destra/in basso del foglio, i limiti
	// calcolati la escluderebbero e non verrebbe mai visitata,
	// restando vuota per sempre. NextExisting resta comunque
	// efficiente su un range pieno: salta direttamente da una cella
	// esistente alla successiva tramite la mappa.
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

// Genera content.xml a partire dal documento: solo i valori
// calcolati (numeri/testo), non le formule -- stessa scelta gia'
// fatta per l'export CSV (vedi CsvTranslator.cpp), per non generare
// una sintassi di formula ODF potenzialmente non valida per casi non
// gestiti (l'inverso di ConvertODFFormula sotto non e' banale per
// formule arbitrarie). Limite noto, documentato in
// docs/TRANSLATORS.md.
static std::string BuildContentXml(CContainer* doc)
{
	range bounds;
	doc->GetBounds(bounds);

	std::string xml;
	xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	xml += "<office:document-content "
		"xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\" "
		"xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\" "
		"xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\" "
		"office:version=\"1.2\">"
		"<office:body><office:spreadsheet>"
		"<table:table table:name=\"Foglio1\">";

	if (bounds.right >= 1 && bounds.bottom >= 1)
	{
		char numBuf[64];
		for (int row = 1; row <= bounds.bottom; row++)
		{
			xml += "<table:table-row>";
			for (int col = 1; col <= bounds.right; col++)
			{
				Value v;
				doc->GetValue(cell(col, row), v);

				if (v.fType == eNumData && !v.IsNan())
				{
					snprintf(numBuf, sizeof(numBuf), "%.15g", (double)v);
					xml += "<table:table-cell office:value-type=\"float\" office:value=\"";
					xml += numBuf;
					xml += "\"><text:p>";
					xml += numBuf;
					xml += "</text:p></table:table-cell>";
				}
				else if (v.fType == eTextData)
				{
					xml += "<table:table-cell office:value-type=\"string\"><text:p>";
					AppendXmlEscaped(xml, (const char*)v);
					xml += "</text:p></table:table-cell>";
				}
				else
					xml += "<table:table-cell/>";
			}
			xml += "</table:table-row>";
		}
	}

	xml += "</table:table></office:spreadsheet></office:body></office:document-content>";
	return xml;
}

static status_t WriteODS(CContainer* doc, BPositionIO* dest)
{
	static const char kMimeType[] = "application/vnd.oasis.opendocument.spreadsheet";
	static const char kManifest[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<manifest:manifest xmlns:manifest=\"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0\" "
		"manifest:version=\"1.2\">\n"
		" <manifest:file-entry manifest:full-path=\"/\" manifest:version=\"1.2\" "
		"manifest:media-type=\"application/vnd.oasis.opendocument.spreadsheet\"/>\n"
		" <manifest:file-entry manifest:full-path=\"content.xml\" manifest:media-type=\"text/xml\"/>\n"
		"</manifest:manifest>\n";

	std::string content = BuildContentXml(doc);

	CZipWriter zip;
	zip.Begin(dest);

	// "mimetype" deve essere la prima voce dell'archivio (convenzione
	// OpenDocument, non solo un dettaglio implementativo: un lettore
	// ODF la usa per riconoscere il formato senza dover analizzare
	// l'intera central directory).
	if (!zip.AddEntry("mimetype", kMimeType, strlen(kMimeType)))
		return B_IO_ERROR;
	if (!zip.AddEntry("META-INF/manifest.xml", kManifest, strlen(kManifest)))
		return B_IO_ERROR;
	if (!zip.AddEntry("content.xml", content.data(), content.size()))
		return B_IO_ERROR;

	return zip.Close() ? B_OK : B_IO_ERROR;
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
	return "Importa/esporta fogli di calcolo dal/al formato OpenDocument (ODS) "
		"-- l'esportazione scrive solo i valori calcolati, non le formule";
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
	*_count = 2;
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

	// Riconosce anche un sorgente ASCD nativo: serve per l'esportazione
	// (ASCD -> ODS), la direzione opposta della normale importazione
	// gestita sotto -- stesso approccio di CsvTranslator::Identify.
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
	// info puo' essere NULL (documentato nel Translation Kit: significa
	// "identifica tu stesso il formato sorgente") -- crash reale di
	// Tracker, non solo teorico: il thumbnail worker chiama
	// BTranslatorRoster::Translate() cosi' per ogni file mentre genera
	// le anteprime, senza mai passare da Identify() prima.
	translator_info localInfo;
	if (!info)
	{
		if (Identify(source, NULL, extension, &localInfo, outType) != B_OK)
			return B_NO_TRANSLATOR;
		info = &localInfo;
	}

	if (info->type != kAtomoOdsFormat && info->type != kAtomoNativeFormat)
		return B_NO_TRANSLATOR;
	if (outType == 0)
		outType = kAtomoNativeFormat;
	if (outType != kAtomoNativeFormat && outType != kAtomoOdsFormat)
		return B_NO_TRANSLATOR;

	CContainer* doc = new CContainer(NULL, NULL);
	status_t err = B_OK;

	if (info->type == kAtomoNativeFormat)
		err = ReadASCD(source, doc);
	else
	{
		// Stesso bug reale gia' corretto per XLSX (vedi il commento su
		// EnsureFunctionsInitialized in FunctionUtils.h/.cpp): questo
		// translator .so ha la propria copia INDIPENDENTE di
		// gFuncCount, mai popolata da App::ReadyToRun -- senza questa
		// chiamata, ogni formula con funzione con nome (SUM, IF, ecc.)
		// importata da un file ODS reale resta testo grezzo.
		EnsureFunctionsInitialized();

		CZipReader zip;
		if (!zip.Open(source))
			err = B_BAD_DATA;

		std::vector<unsigned char> contentXml;
		if (err == B_OK && !zip.ReadEntry("content.xml", contentXml))
			err = B_BAD_DATA;

		if (err == B_OK && !ParseContent(contentXml, doc))
			err = B_BAD_DATA;
	}

	if (err == B_OK)
	{
		if (outType == kAtomoNativeFormat)
			err = WriteASCD(doc, destination);
		else
			err = WriteODS(doc, destination);
	}

	doc->Release();
	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
		return new COdsTranslator();
	return NULL;
}
