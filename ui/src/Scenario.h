/*
	Scenario.h

	Uno scenario (Tier 4, "Path to full Excel parity", Gestione scenari --
	l'altra meta' di "Tabella dati", vedi ROADMAP.md): un nome piu' un
	elenco ORDINATO di valori sostitutivi per un singolo intervallo
	CONTIGUO di "celle variabili" (changing cells) -- SEMPLIFICAZIONE
	dichiarata rispetto al vero Excel (fino a 32 celle/intervalli
	ARBITRARI, anche non contigui): questa app non ha oggi ne' un parser
	di intervalli non contigui (RangeRef::ParseRangeRef accetta un solo
	token "A1" o "A1:B5") ne' un'istantanea di annullamento sparsa su un
	insieme di celle (SheetView::SaveUndoState accetta un solo "range"
	rettangolare). "values[i]" corrisponde alla i-esima cella
	dell'intervallo "changingCells" in ordine per RIGHE (stesso ordine di
	CCellIterator/FormatRangeRef), MAI per indice di colonna come
	ChartObject::valueColumns.

	Nessuna presenza sul foglio (a differenza di ChartObject/SlicerObject):
	uno scenario non ha un frame, non si disegna, non si trascina -- vive
	solo nell'elenco di MainWindow::fScenarios e nella finestra "Gestione
	scenari", SheetView non ha bisogno di saperne nulla.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef SCENARIO_H
#define SCENARIO_H

#include <String.h>
#include <vector>

#include "Range.h"

struct Scenario {
	BString name;
	// Intervallo CONTIGUO delle celle variabili (vedi il commento sopra
	// sulla semplificazione rispetto a Excel).
	range changingCells;
	// Uno per cella di "changingCells", in ordine per righe -- testo
	// grezzo (non un double), esattamente come TryToParseString si
	// aspetta: una cella variabile puo' contenere una formula/stringa,
	// non solo un numero, stesso principio delle celle input di
	// WhatIfWindow.
	std::vector<BString> values;
	// Commento libero facoltativo (come il commento del vero Excel
	// Scenario Manager) -- stringa vuota di default, nessun campo
	// booleano "ha commento" a parte: una stringa vuota e' gia' un
	// segnaposto inequivocabile ("nessun commento"), stesso principio di
	// SlicerObject::title.
	BString comment;
};

#endif
