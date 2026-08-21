/*
	PrintLayout.h

	Calcola in quante pagine si divide la stampa di un foglio, e da dove
	(in coordinate canvas di SheetView) inizia ognuna, ripetendo la banda
	di intestazione (numeri di riga/lettere di colonna) su OGNI pagina
	invece che solo sulla prima. Funzione pura, separata da BPrintJob
	(MainWindow::PrintDocument) per poterla verificare senza una
	stampante vera configurata.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PRINT_LAYOUT_H
#define PRINT_LAYOUT_H

#include <Point.h>
#include <Rect.h>

#include <vector>

// Un'origine per pagina (angolo superiore sinistro del rettangolo da
// passare a BPrintJob::DrawView/ScrollTo, in coordinate canvas di
// SheetView -- gia' spostato indietro di headerW/headerH rispetto al
// contenuto dati vero della pagina, cosi' l'intestazione ripetuta ha
// spazio senza sovrapporsi ai dati, vedi ComputePrintPageOrigins).
std::vector<BPoint> ComputePrintPageOrigins(BRect contentRect,
	float pageWidth, float pageHeight, float headerW, float headerH);

#endif
