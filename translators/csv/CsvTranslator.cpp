/*
	CsvTranslator.cpp

	Vedi CsvTranslator.h per la descrizione generale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
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
#include "FunctionUtils.h"

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
	// Range completo invece dei limiti di GetBounds: una cella con
	// formula non ancora calcolata (mType eNoData) verrebbe esclusa
	// dai limiti calcolati da GetBounds, e se e' anche la cella piu' a
	// destra/in basso del foglio sparirebbe del tutto dal file
	// prodotto (stesso ragionamento del ciclo di ricalcolo qui sotto).
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

	// TryToParseString imposta la formula/il valore di ogni cella ma
	// non la calcola: CFormula::Calculate legge i riferimenti ad
	// altre celle con una semplice GetValue (non ricorsiva), quindi
	// senza questo passo l'esportazione CSV di una formula (che deve
	// scrivere il valore calcolato, non il testo della formula)
	// risulterebbe vuota. Piu' passate finche' nessuna cella cambia
	// piu' valore propagano correttamente le dipendenze in qualunque
	// ordine siano state inserite; limite di passate per non restare
	// bloccati su un riferimento circolare.
	//
	// L'iteratore usa il range completo del foglio (non i limiti
	// restituiti da GetBounds) perche' GetBounds esclude le celle con
	// mType eNoData -- esattamente lo stato di una formula appena
	// analizzata da TryToParseString e non ancora calcolata. Se quella
	// cella e' anche la piu' a destra/in basso del foglio (nessun'altra
	// cella "reale" oltre di lei), i limiti calcolati la escluderebbero
	// e non verrebbe mai visitata da NextExisting, restando vuota per
	// sempre. NextExisting resta comunque efficiente su un range pieno:
	// salta direttamente da una cella esistente alla successiva tramite
	// la mappa, senza scandire le celle vuote in mezzo.
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

// CSV non ha una firma propria (a differenza di ASCD/XLS/XLSX/ODS):
// un formato di testo puro non ha niente all'inizio del file che lo
// distingua da un altro formato di testo. Prima di questo controllo,
// Identify() accettava incondizionatamente QUALUNQUE contenuto come
// "forse CSV" (qualita' 0.6) -- bug reale, non solo teorico: un JPEG
// (o qualunque altro binario) veniva "riconosciuto" come CSV con
// priorita' sufficiente a battere JPEGTranslator in
// BTranslatorRoster::Translate() con B_TRANSLATOR_BITMAP come uscita,
// che poi falliva con "No translator found" perche' CsvTranslator non
// sa produrre un bitmap -- scoperto verificando perche' un'immagine
// incorporata in un file XLS non veniva mai disegnata nonostante i
// byte estratti fossero un JPEG valido. Un contenuto binario (byte
// NUL, o troppi byte di controllo non tipici del testo) viene quindi
// respinto qui invece di essere accettato come "forse CSV".
static bool LooksLikeText(BPositionIO* source)
{
	off_t pos = source->Position();
	unsigned char sample[512];
	ssize_t read = source->Read(sample, sizeof(sample));
	source->Seek(pos, SEEK_SET);

	if (read <= 0)
		return true; // file vuoto: nessuna prova di essere binario

	int controlBytes = 0;
	for (ssize_t i = 0; i < read; i++)
	{
		unsigned char c = sample[i];
		if (c == 0)
			return false; // un NUL non compare mai in un file di testo genuino
		if (c < 0x20 && c != '\t' && c != '\n' && c != '\r')
			controlBytes++;
	}
	return controlBytes * 10 < read; // oltre il 10% di controllo -> binario
}

status_t CCsvTranslator::Identify(BPositionIO* source,
	const translation_format* format, BMessage* extension,
	translator_info* info, uint32 outType)
{
	off_t pos = source->Position();
	char header[4];
	ssize_t read = source->Read(header, 4);
	source->Seek(pos, SEEK_SET);

	bool isAscd = read == 4 && memcmp(header, kASCDMagic, 4) == 0;
	if (!isAscd && !LooksLikeText(source))
		return B_NO_TRANSLATOR;

	uint32 type = kAtomoCsvFormat;
	float quality = 0.6f, capability = 0.6f;
	const char* name = "Comma-Separated Values (CSV)";
	const char* mime = "text/csv";

	if (isAscd)
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
	// info puo' essere NULL (documentato nel Translation Kit: significa
	// "identifica tu stesso il formato sorgente") -- un vero crash reale
	// di Tracker, non solo teorico: il thumbnail worker chiama
	// BTranslatorRoster::Translate() cosi' per ogni file mentre genera
	// le anteprime, senza mai passare da Identify() prima. Senza questo
	// controllo, "info->type" sotto leggeva un indirizzo qualunque.
	translator_info localInfo;
	if (!info)
	{
		if (Identify(source, NULL, extension, &localInfo, outType) != B_OK)
			return B_NO_TRANSLATOR;
		info = &localInfo;
	}

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
			// Stesso bug reale gia' corretto per XLSX/ODS (vedi il
			// commento su EnsureFunctionsInitialized in FunctionUtils.h/
			// .cpp): un campo CSV che comincia per "=" e usa una
			// funzione con nome (es. "=SUM(A1:A3)") passa da CParser
			// esattamente come una formula XLSX, con lo stesso bisogno
			// della tabella delle funzioni caricata.
			EnsureFunctionsInitialized();

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
