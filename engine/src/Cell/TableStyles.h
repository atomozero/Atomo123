/*
	TableStyles.h

	Piccolo elenco di stili tabella con nome di Excel (Path to full Excel
	parity Tier 4, "named table styles"): solo il colore della banda
	alternata per un sottoinsieme rappresentativo (8) dei ~60 stili reali
	di Excel ("TableStyleLight1".."TableStyleDark11"), non tutti -- uno
	stile riconosciuto qui usa il suo vero colore approssimato, uno NON
	riconosciuto (o nessuno) resta con la banda grigio chiaro neutra di
	sempre (comportamento gia' esistente, invariato). Colori scelti a
	occhio per essere ben distinguibili l'uno dall'altro, NON estratti
	pixel per pixel da un vero file Excel -- stessa approssimazione
	dichiarata gia' presente nel commento di ApplyTableBanding in
	XlsxTranslator.cpp per il colore di banda in generale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef TABLE_STYLES_H
#define TABLE_STYLES_H

#include <string>

#include <GraphicsDefs.h>

#include "Range.h"

class CContainer;

struct TableStyleDef {
	const char* name;
	rgb_color bandColor;
};

// Ordine qualunque: FindTableStyleBandColor sotto fa una ricerca lineare,
// una manciata di voci non giustifica una mappa.
extern const TableStyleDef kTableStyles[];
extern const int kTableStyleCount;

// Vero solo se "name" corrisponde (case-sensitive, esattamente come gli
// altri nomi di questo formato -- nomi di tabella/colonna Excel sono gia'
// confrontati cosi' altrove in questo motore) a uno degli stili
// riconosciuti sopra; "outColor" resta invariato altrimenti (il chiamante
// decide il proprio fallback, oggi il grigio neutro di sempre).
bool FindTableStyleBandColor(const std::string& name, rgb_color* outColor);

// Applica la banda alle righe dati dispari di "tableRange" (la prima riga
// e' l'intestazione, esclusa dalla banda, e le eventuali "totalsRowCount"
// righe finali sono anch'esse escluse), solo alle celle senza gia' un
// colore di sfondo esplicito -- vedi il commento piu' lungo in
// TableStyles.cpp. Condivisa fra l'importazione XLSX
// (XlsxTranslator.cpp) e la UI (un cambio di stile da menu su una
// tabella gia' registrata), cosi' le due strade producono esattamente
// la stessa banda invece di due implementazioni che potrebbero
// disallinearsi nel tempo.
void ApplyTableStyleBanding(CContainer* doc, const range& tableRange,
	int totalsRowCount, const std::string& styleName);

#endif
