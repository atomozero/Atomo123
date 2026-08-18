/*
	XlsxTranslator.h

	Add-on Translation Kit per l'import del formato XLSX (Excel 2007+,
	OOXML). Un file XLSX e' un archivio ZIP contenente XML: si legge
	con MiniZip.h (lettore ZIP minimale scritto per questo progetto,
	vedi quel file per il perche') e si fa il parsing con expat (gia'
	installato su questo sistema, non serve libxml2 completa).

	Import ed export: scrive anche formule vive (non solo il valore
	gia' calcolato) per le celle che non referenziano un altro foglio
	(vedi CFormula::ReferencesOtherSheet in engine/), con sintassi
	canonica ECMA-376 indipendente dalle preferenze locali dell'utente.
	Il formato intermedio verso/da ASCD e' lo stesso gia' definito dal
	translator CSV.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef XLSX_TRANSLATOR_H
#define XLSX_TRANSLATOR_H

#include <Rect.h>
#include <Translator.h>
#include <TranslatorFormats.h>

class BView;

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

	virtual status_t MakeConfigurationView(BMessage* extension, BView** _view,
		BRect* _extent);

protected:
	virtual ~CXlsxTranslator();
};

#endif
