/*
	Pivot.h

	Tabella pivot di base: raggruppa un intervallo di due colonne
	(categoria testuale, valore numerico) per categoria e applica
	un'aggregazione (somma/conteggio/media). Un solo livello di
	raggruppamento, una sola misura -- coerente con "tabelle pivot
	base" in ROADMAP.md, non un pivot multidimensionale come Excel.

	Logica separata dalla finestra (PivotWindow) per essere testabile
	senza sessione grafica, stesso principio di Chart.h.
*/

#ifndef PIVOT_H
#define PIVOT_H

#include <vector>

#include <String.h>

class CContainer;
class range;
struct cell;

enum PivotAggFunc {
	ePivotSum,
	ePivotCount,
	ePivotAverage
};

struct PivotRow {
	BString category;
	double aggregate;
	long count;
};

// L'intervallo deve avere esattamente due colonne (categoria,
// valore). Righe con valore non numerico nella seconda colonna
// vengono escluse dall'aggregazione (ma non dal conteggio delle
// categorie se la categoria stessa e' comunque testo valido).
// Risultato ordinato per categoria, cosi' l'output e' deterministico.
bool BuildPivotTable(CContainer* doc, const range& source,
	std::vector<PivotRow>& out);

// Scrive il risultato nel foglio a partire da "dest" (intestazioni
// nella riga di "dest", poi una riga per categoria sotto).
void WritePivotTable(CContainer* doc, const cell& dest,
	const std::vector<PivotRow>& rows, PivotAggFunc fn);

#endif
