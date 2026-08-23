/*
	FileTypeIcons.h

	Icone HVIF per i tipi di file gestiti da Atomo123 in Tracker (Fase
	29, ultima voce del backlog v3.0 "Consolidation"): oggi ogni foglio
	.xlsx/.csv/.ods/.xls appare con l'icona generica del sistema, non
	distinguibile a colpo d'occhio come fanno Excel/LibreOffice Calc.
	Registrate su BMimeType (vedi App::RegisterFileTypes in App.cpp),
	non incorporate come risorsa dell'applicazione: un'icona per TIPO
	MIME, non per l'app stessa (quella resta l'unica in Atomo123.rdef).

	Byte grezzi presi dal catalogo scaricato da www.hvif-store.art (sito
	autorizzato per questo progetto -- vedi Atomo123_icons/LICENSES.md),
	tutte MIT, "Haiku, Inc." (il set ufficiale di icone FileTypes di
	Haiku stesso). Stesso principio di incorporamento come array C gia'
	scelto per le icone della toolbar (vedi IconCatalog.h/IconData.cpp):
	kFileTypeIconOds = 256_opendocument-spreadsheet-file.hvif (corri-
	spondenza esatta per .ods); kFileTypeIconCsv = 259_text-file.hvif
	(un CSV e' testo, nessuna icona "CSV" dedicata nel catalogo);
	kFileTypeIconGeneric = 222_generic-file.hvif, usata sia per XLSX che
	per XLS -- il catalogo scaricato (il set ufficiale Haiku) non
	contiene un'icona "Excel" dedicata, essendo un formato proprietario
	Microsoft: limite noto, non un'omissione silenziosa (vedi il
	commento su App::RegisterFileTypes).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef FILE_TYPE_ICONS_H
#define FILE_TYPE_ICONS_H

#include "IconCatalog.h"

extern const IconData kFileTypeIconOds;
extern const IconData kFileTypeIconCsv;
extern const IconData kFileTypeIconGeneric;
// L'icona dell'app stessa (ui/icons/atomo123.hvif, gia' incorporata
// come risorsa BEOS:ICON in Atomo123.rdef): riusata qui anche per il
// tipo MIME nativo .ascd, dato che nessun'altra applicazione lo
// gestisce -- non c'e' un'icona "presa in prestito" da nessun'altra
// parte da confondere con la nostra.
extern const IconData kFileTypeIconAscd;

#endif
