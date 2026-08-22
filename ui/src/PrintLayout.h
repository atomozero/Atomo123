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

// Modalita' di "adatta" per ComputePrintFitScale sotto -- stessi tre
// scelte di Excel (Pagina Larghezza/Altezza/entrambe).
enum {
	kPrintFitWidth = 1,
	kPrintFitHeight = 2,
	kPrintFitBoth = 3
};

// Scala (mai oltre 1.0: "adatta" restringe soltanto, non ingrandisce
// mai un contenuto che gia' ci sta, come in Excel) che fa stare
// contentRect (intestazione compresa) nella larghezza/altezza/
// entrambe di una sola pagina fisica usableWidth x usableHeight (gia'
// al netto dei margini, vedi MainWindow::PrintDocument). Usa la
// STESSA logica di "da dove comincia il contenuto vero" di
// ComputePrintPageOrigins sopra (mai duplicata a mano nel chiamante):
// un'incongruenza fra le due produrrebbe un "adatta a una pagina" che
// in realta' non ci sta.
float ComputePrintFitScale(BRect contentRect, float usableWidth, float usableHeight,
	float headerW, float headerH, int fitMode);

#endif
