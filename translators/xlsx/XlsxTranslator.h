/*
	XlsxTranslator.h

	Add-on Translation Kit per l'import del formato XLSX (Excel 2007+,
	OOXML). Un file XLSX e' un archivio ZIP contenente XML: si legge
	con MiniZip.h (lettore ZIP minimale scritto per questo progetto,
	vedi quel file per il perche') e si fa il parsing con expat (gia'
	installato su questo sistema, non serve libxml2 completa).

	Come per XLS legacy, solo import per ora: il motore non include
	ancora un writer per XLSX. Il formato di uscita e' lo stesso ASCD
	gia' definito dal translator CSV.
*/

#ifndef XLSX_TRANSLATOR_H
#define XLSX_TRANSLATOR_H

#include <Translator.h>
#include <TranslatorFormats.h>

const uint32 kAtomoXlsxFormat = 'AXSX';
const uint32 kAtomoNativeFormat = 'ASCD';
const uint32 kAtomoSheetGroup = 'ASHT';

class CXlsxTranslator : public BTranslator {
public:
	CXlsxTranslator();

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
	virtual ~CXlsxTranslator();
};

#endif
