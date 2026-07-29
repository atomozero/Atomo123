/*
	AscdIO.h

	Lettura/scrittura del formato nativo provvisorio ASCD ("Atomo
	Sheet Cell Data") usato anche dai translator (vedi
	translators/csv/CsvTranslator.cpp per la definizione originale
	del formato). L'app non e' un add-on Translation Kit, quindi
	questa e' una copia standalone della stessa logica di
	lettura/scrittura -- stesso approccio gia' seguito da ogni
	translator, che duplica WriteASCD() per non introdurre una
	dipendenza di link tra translator e app.
*/

#ifndef ASCD_IO_H
#define ASCD_IO_H

#include <DataIO.h>
#include <SupportDefs.h>

class CContainer;

status_t LoadASCD(BPositionIO* source, CContainer* doc);
status_t SaveASCD(CContainer* doc, BPositionIO* dest);

#endif
