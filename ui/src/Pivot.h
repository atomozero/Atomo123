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

	PivotAggFunc/PivotRow/PivotTableObject vivono ormai in Container.h
	(motore), non piu' qui: un futuro importatore XLSX (che include
	solo Container.h, mai questo header della UI) deve poter costruire
	quei tipi direttamente -- vedi il commento su PivotTableObject in
	Container.h per il ragionamento completo. Questo file resta con la
	sola LOGICA (BuildPivotTable/WritePivotTable), che opera su quei
	tipi ma non li possiede.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PIVOT_H
#define PIVOT_H

#include <vector>

#include "Container.h"

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

// Vero quando "pivot" usa il campo Colonne e/o 2+ misure esplicite,
// cioe' richiede il percorso 2D (BuildPivotTable2D/WritePivotTable2D)
// invece del vecchio percorso piatto sopra -- unico punto di verita' per
// questa decisione, usato da MainWindow (quale coppia Build.../Write...
// chiamare) e dal translator XLSX (quale forma OOXML scrivere/accettare).
inline bool PivotIsMultiDimensional(const PivotTableObject& pivot)
{
	return pivot.columnFieldCol != -1 || pivot.measures.size() > 1;
}

// Raggruppa "source" su DUE assi: le colonne chiave di riga (auto-
// derivate: ogni colonna di "source" TRANNE "columnFieldCol", se >= 0, e
// tranne ogni PivotMeasure::sourceCol in "measures" -- stesso principio
// del vettore ChartObject::valueColumns, l'insieme escluso e' esplicito,
// il resto e' implicito) e il campo Colonne esplicito (valori distinti
// ordinati lessicograficamente in "outColumnValues"). Fallisce (false)
// se non resta nessuna colonna chiave di riga.
//
// Una riga sorgente con una qualunque chiave di riga non testuale, o
// con il valore del campo Colonne non testuale (quando columnFieldCol
// >= 0), viene esclusa del tutto. Per ogni misura, un valore non
// numerico nella sua sourceCol esclude SOLO quella misura per quella
// riga (le altre misure della stessa riga sorgente restano valide) --
// a differenza di BuildPivotTable sopra, che scarta l'intera riga se
// l'unica misura non e' numerica.
bool BuildPivotTable2D(CContainer* doc, const range& source, int columnFieldCol,
	const std::vector<PivotMeasure>& measures,
	std::vector<BString>* outColumnValues, std::vector<PivotRow2D>* outRows);

// Scrive il grigliato 2D a partire da "dest": SEMPRE due righe di
// intestazione -- riga 1 = valore del campo Colonne (ripetuto su tutte
// le colonne misura di quel valore, vuoto sotto le colonne chiave di
// riga e vuoto ovunque quando columnFieldCol == -1), riga 2 = etichetta
// di ogni misura. Layout colonne: prima le colonne chiave di riga, poi
// un blocco di measures.size() colonne per ogni valore distinto del
// campo Colonne (o un solo blocco quando non c'e' campo Colonne).
void WritePivotTable2D(CContainer* doc, const cell& dest,
	const std::vector<BString>& columnValues, const std::vector<PivotMeasure>& measures,
	const std::vector<PivotRow2D>& rows);

// Calcola l'intervallo che WritePivotTable2D scriverebbe per questo
// risultato, SENZA scrivere nulla -- usato per il controllo di
// sovrapposizione sorgente/destinazione prima di scrivere davvero.
range PivotTable2DDestRange(const cell& dest, int numRowKeyCols,
	const std::vector<BString>& columnValues, const std::vector<PivotMeasure>& measures,
	size_t rowCount);

#endif
