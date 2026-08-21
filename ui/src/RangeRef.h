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

#include <cstddef>

class range;

bool ParseRangeRef(const char* text, range& outRange);

// Inversa di ParseRangeRef (Fase 18): "A1" per un intervallo di una
// sola cella, "A1:B5" per piu' celle -- usata per precompilare il
// campo Intervallo di ChartWindow con la selezione corrente del
// foglio quando si apre la finestra Grafico, vedi
// MainWindow::ShowChartWindow.
void FormatRangeRef(const range& r, char* out, size_t outSize);

#endif
