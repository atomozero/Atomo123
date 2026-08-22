/*
	Pivot.h

	Tabella pivot di base: raggruppa un intervallo di due o piu'
	colonne (una o piu' colonne di categoria testuale, poi una colonna
	di valore numerico) e applica un'aggregazione (somma/conteggio/
	media/minimo/massimo). Raggruppamento multi-livello (Fase 29: piu'
	colonne di categoria si comportano come piu' campi "Righe" di un
	pivot Excel, annidati nell'ordine dato), una sola misura --
	coerente con "tabelle pivot base" in ROADMAP.md, non un pivot
	multidimensionale completo come Excel (niente campi "Colonne", un
	solo valore aggregato per volta).

	Logica separata dalla finestra (PivotWindow) per essere testabile
	senza sessione grafica, stesso principio di Chart.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
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
	ePivotAverage,
	ePivotMin,
	ePivotMax
};

struct PivotRow {
	// Una voce per livello di raggruppamento (di solito una sola,
	// come prima della Fase 29) -- l'ORDINE combacia con l'ordine
	// delle colonne di categoria nell'intervallo sorgente, cosi' un
	// raggruppamento a due livelli (es. Regione, Prodotto) produce
	// righe annidate nello stesso ordine con cui l'utente le ha
	// selezionate, non riordinate a caso.
	std::vector<BString> categories;
	double aggregate; // somma -- usata anche per calcolare la media
	long count;
	double minVal;
	double maxVal;
};

// L'intervallo deve avere ALMENO due colonne: l'ULTIMA e' il valore
// numerico da aggregare, tutte le altre (una o piu') sono chiavi di
// raggruppamento, una per livello. Una riga con una qualunque chiave
// non testuale, o un valore non numerico, viene esclusa
// dall'aggregazione (comportamento invariato dalla versione
// a un solo livello). Risultato ordinato per categorie (in ordine
// lessicografico sull'intero vettore, livello per livello), cosi'
// l'output e' deterministico.
bool BuildPivotTable(CContainer* doc, const range& source,
	std::vector<PivotRow>& out);

// Scrive il risultato nel foglio a partire da "dest" (intestazioni
// nella riga di "dest" -- una per livello di raggruppamento piu' una
// per l'aggregazione -- poi una riga per gruppo sotto).
void WritePivotTable(CContainer* doc, const cell& dest,
	const std::vector<PivotRow>& rows, PivotAggFunc fn);

#endif
