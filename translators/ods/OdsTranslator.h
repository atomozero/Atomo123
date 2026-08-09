/*
	OdsTranslator.h

	Add-on Translation Kit per l'import del formato ODS
	(OpenDocument Spreadsheet, LibreOffice/OpenOffice Calc). Come
	XLSX, un file ODS e' un archivio ZIP contenente XML: si riusa lo
	stesso lettore ZIP minimale (MiniZip.h) e lo stesso parser expat,
	ma con lo schema OpenDocument (content.xml) invece di OOXML.

	Solo import per ora: il motore non include ancora un writer per
	ODS. Il formato di uscita e' lo stesso ASCD gia' definito dal
	translator CSV.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef ODS_TRANSLATOR_H
#define ODS_TRANSLATOR_H

#include <Translator.h>
#include <TranslatorFormats.h>

const uint32 kAtomoOdsFormat = 'AODS';
const uint32 kAtomoNativeFormat = 'ASCD';
const uint32 kAtomoSheetGroup = 'ASHT';

class COdsTranslator : public BTranslator {
public:
	COdsTranslator();

	virtual const char* TranslatorName() const;
	virtual const char* TranslatorInfo() const;
	virtual int32 TranslatorVersion() const;

	virtual const translation_format* InputFormats(int32* _count) const;
	virtual const translation_format* OutputFormats(int32* _count) const;

	virtual status_t Identify(BPositionIO* source,
		const translation_format* format, BMessage* extension,
		translator_info* info, uint32 outType);

	virtual status_t Translate(BPositionIO* source,
		const translator_info* info, BMessage* extension, uint32 outType,
		BPositionIO* destination);

protected:
	virtual ~COdsTranslator();
};

#endif
