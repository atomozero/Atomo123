/*
	Slicer.h

	Uno slicer (Tier 4, "Path to full Excel parity"): un riquadro con un
	elenco di pulsanti disegnato sopra la griglia (come ChartObject/
	EmbeddedImage) che permette di includere/escludere valori di UNA
	colonna dell'AutoFilter attivo del foglio con un clic sul pulsante,
	invece di aprire il menu a tendina dell'intestazione -- stessa
	identica funzione (SheetView::SetColumnValueHidden), solo
	un'interfaccia diversa, sempre visibile.

	Riusa l'AutoFilter gia' esistente del foglio (SheetView::
	AutoFilterRange/UniqueColumnValues/IsColumnValueVisible) invece di
	avere un proprio intervallo/stato di filtro separato: un foglio ha un
	solo AutoFilter alla volta (vedi SheetView::SetAutoFilter), quindi
	uno slicer indica solo QUALE colonna di quell'AutoFilter mostrare
	come pulsanti -- se l'AutoFilter viene rimosso, lo slicer resta
	nell'elenco ma non disegna ne' risponde a nulla (nessun valore da
	elencare), limite dichiarato coerente con come un vero slicer Excel
	orfano della sua tabella si comporta.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef SLICER_H
#define SLICER_H

#include <Rect.h>
#include <String.h>

struct SlicerObject {
	SlicerObject() : frame(0, 0, 140, 160), columnIndex(0) {}

	BRect frame;
	// Colonna assoluta (1-based, stesso sistema di range::left/right in
	// engine/src/Cell/Range.h) dell'AutoFilter del foglio che questo
	// slicer controlla.
	int columnIndex;
	// Titolo mostrato nella barra superiore del riquadro (il
	// trascinamento avviene afferrando questa barra, vedi
	// SheetView::SlicerTitleBarRect -- il corpo sotto e' tutto pulsanti,
	// non trascinabile) -- di solito il testo dell'intestazione della
	// colonna al momento della creazione, ma libero, non risincronizzato
	// se l'intestazione cambia dopo.
	BString title;
};

#endif
