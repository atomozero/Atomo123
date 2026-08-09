/*
	RangeRef.h

	Analizza un riferimento a intervallo digitato dall'utente (es.
	"A1:B5", o anche solo "A1" per un intervallo di una sola cella) in
	un range del motore. Condiviso fra ChartWindow e PivotWindow, le
	prime due funzionalita' che hanno bisogno di un intervallo scelto
	dall'utente invece che dalla sola cella selezionata.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef RANGE_REF_H
#define RANGE_REF_H

class range;

bool ParseRangeRef(const char* text, range& outRange);

#endif
