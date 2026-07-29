/*
	CsvTranslator.cpp

	Vedi CsvTranslator.h per la descrizione generale.
*/

#include "CsvTranslator.h"

#include <cstdio>
#include <cstring>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "CellIterator.h"
#include "Text2Cells.h"

static const translation_format sInputFormats[] = {
	{
		kAtomoCsvFormat, kAtomoSheetGroup, 0.8f, 0.8f,
		"text/csv", "Comma-Separated Values (CSV)"
	},
	{
		kAtomoNativeFormat, kAtomoSheetGroup, 1.0f, 1.0f,
		"application/x-vnd.atomo-sheet-data", "Atomo Sheet Cell Data (ASCD)"
	}
};

static const translation_format sOutputFormats[] = {
	{
		kAtomoCsvFormat, kAtomoSheetGroup, 0.8f, 0.8f,
		"text/csv", "Comma-Separated Values (CSV)"
	},
	{
		kAtomoNativeFormat, kAtomoSheetGroup, 1.0f, 1.0f,
		"application/x-vnd.atomo-sheet-data", "Atomo Sheet Cell Data (ASCD)"
	}
};

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
static const int32 kASCDVersion = 1;

// Scrive in "dest" tutte le celle non vuote di "doc" nel formato ASCD:
// magic(4) + versione(int32) + numero celle(int32), poi per ciascuna
// cella: riga(int16) colonna(int16) lunghezza testo(int32) testo.
// Il testo è quanto restituito da GetCellFormula: la formula con "="
// se presente, altrimenti il valore formattato — cosi' una formula
// sopravvive al round-trip nativo (a differenza dell'export CSV, dove
// e' corretto che venga "appiattita" al valore calcolato).
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

// Legge un flusso ASCD e ricostruisce le celle in "doc" (vuoto in
// ingresso), riusando TryToParseString che gia' distingue formule da
// valori costanti.
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

	return B_OK;
}

CCsvTranslator::CCsvTranslator()
	: BTranslator()
{
}

CCsvTranslator::~CCsvTranslator()
{
}

const char* CCsvTranslator::TranslatorName() const
{
	return "CSV Translator";
}

const char* CCsvTranslator::TranslatorInfo() const
{
	return "Converte fogli di calcolo tra CSV e il formato dati nativo Atomo Sheet";
}

int32 CCsvTranslator::TranslatorVersion() const
{
	return B_TRANSLATION_MAKE_VERSION(1, 0, 0);
}

const translation_format* CCsvTranslator::InputFormats(int32* _count) const
{
	*_count = 2;
	return sInputFormats;
}

const translation_format* CCsvTranslator::OutputFormats(int32* _count) const
{
	*_count = 2;
	return sOutputFormats;
}

status_t CCsvTranslator::Identify(BPositionIO* source,
	const translation_format* format, BMessage* extension,
	translator_info* info, uint32 outType)
{
	off_t pos = source->Position();
	char header[4];
	ssize_t read = source->Read(header, 4);
	source->Seek(pos, SEEK_SET);

	uint32 type = kAtomoCsvFormat;
	float quality = 0.6f, capability = 0.6f;
	const char* name = "Comma-Separated Values (CSV)";
	const char* mime = "text/csv";

	if (read == 4 && memcmp(header, kASCDMagic, 4) == 0)
	{
		type = kAtomoNativeFormat;
		quality = capability = 1.0f;
		name = "Atomo Sheet Cell Data (ASCD)";
		mime = "application/x-vnd.atomo-sheet-data";
	}

	info->type = type;
	info->group = kAtomoSheetGroup;
	info->quality = quality;
	info->capability = capability;
	strlcpy(info->name, name, sizeof(info->name));
	strlcpy(info->MIME, mime, sizeof(info->MIME));

	return B_OK;
}

status_t CCsvTranslator::Translate(BPositionIO* source,
	const translator_info* info, BMessage* extension, uint32 outType,
	BPositionIO* destination)
{
	if (outType == 0)
		outType = kAtomoNativeFormat;

	if (outType != kAtomoCsvFormat && outType != kAtomoNativeFormat)
		return B_NO_TRANSLATOR;

	CContainer* doc = new CContainer(NULL, NULL);
	status_t err = B_OK;

	try
	{
		if (info->type == kAtomoNativeFormat)
			err = ReadASCD(source, doc);
		else
		{
			range r(1, 1, 1, 1);
			CTextConverter conv(*source, doc);
			conv.SetFieldSeparator(',');
			conv.SetQuoteTextFields(true);
			conv.ConvertFromText(r, NULL, true);
		}
	}
	catch (...)
	{
		err = B_ERROR;
	}

	if (err == B_OK)
	{
		try
		{
			if (outType == kAtomoNativeFormat)
				err = WriteASCD(doc, destination);
			else
			{
				CTextConverter conv(*destination, doc);
				conv.SetFieldSeparator(',');
				conv.SetQuoteTextFields(true);
				conv.ConvertToText(NULL, NULL);
			}
		}
		catch (...)
		{
			err = B_ERROR;
		}
	}

	doc->Release();
	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
		return new CCsvTranslator();
	return NULL;
}
