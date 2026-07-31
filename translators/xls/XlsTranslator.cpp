/*
	XlsTranslator.cpp

	Vedi XlsTranslator.h per la descrizione generale.
*/

#include "XlsTranslator.h"

#include <cstring>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellIterator.h"
#include "Excel.h"
#include "EngineViewStub.h"

static const translation_format sInputFormats[] = {
	{
		kAtomoXlsFormat, kAtomoSheetGroup, 0.8f, 0.8f,
		"application/vnd.ms-excel", "Microsoft Excel 97-2003 (XLS)"
	}
};

static const translation_format sOutputFormats[] = {
	{
		kAtomoNativeFormat, kAtomoSheetGroup, 1.0f, 1.0f,
		"application/x-vnd.atomo-sheet-data", "Atomo Sheet Cell Data (ASCD)"
	}
};

// Firma standard degli OLE2 Compound File Binary (usati da XLS legacy,
// DOC, e altri formati Microsoft Office pre-2007): 8 byte fissi.
static const unsigned char kOLE2Signature[8] =
	{ 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
static const int32 kASCDVersion = 1;

// Stessa serializzazione ASCD del translator CSV (vedi
// translators/csv/CsvTranslator.cpp per la descrizione completa):
// duplicata qui deliberatamente, per mantenere ogni translator un
// add-on autonomo senza dipendenze incrociate. Se in futuro servono
// piu' translator con lo stesso formato di uscita, vale la pena
// estrarla in una libreria condivisa.
static status_t WriteASCD(CContainer* doc, BPositionIO* dest)
{
	// Range completo invece dei limiti di GetBounds: una cella con
	// formula non ancora calcolata (mType eNoData) verrebbe esclusa
	// dai limiti calcolati da GetBounds, e se e' anche la cella piu' a
	// destra/in basso del foglio sparirebbe del tutto dal file
	// prodotto -- bug scoperto e corretto costruendo l'export ODS
	// (vedi ROADMAP.md, Fase 5).
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

CXlsTranslator::CXlsTranslator()
	: BTranslator()
{
}

CXlsTranslator::~CXlsTranslator()
{
}

const char* CXlsTranslator::TranslatorName() const
{
	return "XLS Legacy Translator";
}

const char* CXlsTranslator::TranslatorInfo() const
{
	return "Importa fogli di calcolo dal formato binario Excel 97-2003 (XLS)";
}

int32 CXlsTranslator::TranslatorVersion() const
{
	return B_TRANSLATION_MAKE_VERSION(1, 0, 0);
}

const translation_format* CXlsTranslator::InputFormats(int32* _count) const
{
	*_count = 1;
	return sInputFormats;
}

const translation_format* CXlsTranslator::OutputFormats(int32* _count) const
{
	*_count = 1;
	return sOutputFormats;
}

status_t CXlsTranslator::Identify(BPositionIO* source,
	const translation_format* format, BMessage* extension,
	translator_info* info, uint32 outType)
{
	off_t pos = source->Position();
	unsigned char header[8];
	ssize_t read = source->Read(header, 8);
	source->Seek(pos, SEEK_SET);

	if (read != 8 || memcmp(header, kOLE2Signature, 8) != 0)
		return B_NO_TRANSLATOR;

	info->type = kAtomoXlsFormat;
	info->group = kAtomoSheetGroup;
	info->quality = 0.8f;
	info->capability = 0.8f;
	strlcpy(info->name, "Microsoft Excel 97-2003 (XLS)", sizeof(info->name));
	strlcpy(info->MIME, "application/vnd.ms-excel", sizeof(info->MIME));

	return B_OK;
}

status_t CXlsTranslator::Translate(BPositionIO* source,
	const translator_info* info, BMessage* extension, uint32 outType,
	BPositionIO* destination)
{
	if (info->type != kAtomoXlsFormat)
		return B_NO_TRANSLATOR;
	if (outType != 0 && outType != kAtomoNativeFormat)
		return B_NO_TRANSLATOR;

	CContainer* doc = new CContainer(NULL, NULL);
	status_t err = B_OK;

	try
	{
		// cellView=NULL: nessuna UI collegata (translator headless).
		// Il costruttore legge subito il flusso e popola doc; alcuni
		// metadati (nomi di intervallo, larghezze colonna/altezze
		// riga) vengono scartati in questa modalita' -- vedi la nota
		// nello stub in engine/src/Stubs/EngineViewStub.h.
		CExcel5Filter filter(*source, NULL, doc);
		filter.Translate();
	}
	catch (...)
	{
		err = B_BAD_DATA;
	}

	if (err == B_OK)
		err = WriteASCD(doc, destination);

	doc->Release();
	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
		return new CXlsTranslator();
	return NULL;
}
