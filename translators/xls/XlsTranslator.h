/*
	XlsTranslator.h

	Add-on Translation Kit per l'import del formato XLS legacy
	(binario BIFF/OLE2), basato sull'importer storico gia' portato ed
	estratto nel motore isolato (engine/src/Excel/, classe
	CExcel5Filter). Solo import: il motore non include ancora un
	writer per il formato binario legacy (non necessario: l'export
	verso Excel moderno passera' dal translator XLSX).

	Il "formato di uscita" e' lo stesso ASCD (Atomo Sheet Cell Data)
	gia' definito dal translator CSV (translators/csv/CsvTranslator.h),
	cosi' i due translator sono componibili: import XLS -> ASCD ->
	(in futuro) qualunque altro formato che sappia leggere ASCD.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef XLS_TRANSLATOR_H
#define XLS_TRANSLATOR_H

#include <Translator.h>
#include <TranslatorFormats.h>

const uint32 kAtomoXlsFormat = 'AXLS';
const uint32 kAtomoNativeFormat = 'ASCD';
const uint32 kAtomoSheetGroup = 'ASHT';

class CXlsTranslator : public BTranslator {
public:
	CXlsTranslator();

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
	virtual ~CXlsTranslator();
};

#endif
