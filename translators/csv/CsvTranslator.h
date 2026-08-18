/*
	CsvTranslator.h

	Add-on Translation Kit per l'import/export CSV, basato sul motore
	di calcolo isolato (engine/). Converte tra testo CSV e un formato
	nativo minimo (ASCD, "Atomo Sheet Cell Data") che serializza le
	celle non vuote di un CContainer preservando le formule.

	Il formato ASCD è una base di partenza pensata per essere ripresa
	e superata dal vero formato documento nativo dell'app quando la
	Fase 4 (UI) definirà come l'app salva i propri file — per ora
	serve a dimostrare in modo concreto e testabile che il translator
	funziona end-to-end tramite il framework BTranslator di Haiku.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef CSV_TRANSLATOR_H
#define CSV_TRANSLATOR_H

#include <Rect.h>
#include <Translator.h>
#include <TranslatorFormats.h>

class BView;

// Codici di formato custom (nessun gruppo "spreadsheet" predefinito
// nel Translation Kit: si registrano come tipi MIME/type_code propri,
// come previsto dal framework per formati non standard).
const uint32 kAtomoCsvFormat = 'ACSV';
const uint32 kAtomoNativeFormat = 'ASCD';
const uint32 kAtomoSheetGroup = 'ASHT';

class CCsvTranslator : public BTranslator {
public:
	CCsvTranslator();

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
	virtual ~CCsvTranslator();
};

#endif
