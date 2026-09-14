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
#include <String.h>
#include <SupportDefs.h>

#include <vector>

// Un'origine per pagina (angolo superiore sinistro del rettangolo da
// passare a BPrintJob::DrawView/ScrollTo, in coordinate canvas di
// SheetView -- gia' spostato indietro di headerW/headerH rispetto al
// contenuto dati vero della pagina, cosi' l'intestazione ripetuta ha
// spazio senza sovrapporsi ai dati, vedi ComputePrintPageOrigins).
std::vector<BPoint> ComputePrintPageOrigins(BRect contentRect,
	float pageWidth, float pageHeight, float headerW, float headerH,
	float footerH = 0, bool acrossFirst = false);

// Porzione di contentRect davvero presente sulla pagina che comincia a
// pageOrigin (una delle origini di ComputePrintPageOrigins sopra):
// intersezione fra l'area DATI della pagina (headerW/headerH iniziali
// esclusi, riservati all'intestazione ripetuta) e contentRect. Rettangolo
// non valido se la pagina non contiene dati -- non succede con origini
// calcolate da ComputePrintPageOrigins, ma il chiamante non deve fidarsi
// (una pagina vuota centra "niente", offset nullo, mai un valore a caso).
BRect PrintPageContentExtent(BPoint pageOrigin, float pageWidth, float pageHeight,
	BRect contentRect, float headerW, float headerH);

// Modalita' di "adatta" per ComputePrintFitScale sotto -- stessi tre
// scelte di Excel (Pagina Larghezza/Altezza/entrambe), piu' "N x M
// pagine" (kPrintFitPages, con wide/tall da AscdPrintSettings::fitWide/
// fitTall) invece della sola pagina singola.
enum {
	kPrintFitWidth = 1,
	kPrintFitHeight = 2,
	kPrintFitBoth = 3,
	kPrintFitPages = 4
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
	float headerW, float headerH, int fitMode, float footerH = 0);

// Come ComputePrintFitScale in modalita' kPrintFitBoth, ma su wide x tall
// pagine invece di una sola: la scala (mai oltre 1.0, stesso principio)
// che fa stare contentRect in wide pagine di larghezza e tall di altezza.
// wide/tall < 1 vengono trattati come 1 (mai divisione per zero o scala
// infinita da valori corrotti). kPrintFitBoth e' il caso 1x1 di questa.
float ComputePrintFitScaleToPages(BRect contentRect, float usableWidth, float usableHeight,
	float headerW, float headerH, int wide, int tall, float footerH = 0);

// Risultato completo del calcolo di un lavoro di stampa (Fase 28,
// anteprima in "Imposta pagina"): unica fonte di verita' condivisa fra
// MainWindow::PrintDocument (stampa vera) e
// MainWindow::GeneratePrintPreviewPages (anteprima) -- un'incongruenza
// fra le due produrrebbe un'anteprima che non corrisponde a quello che
// viene davvero stampato.
struct PrintJobLayout {
	std::vector<BPoint> pageOrigins;
	// Spostamento DEST (in pixel del dispositivo, stessa unita' di
	// marginLeftPx/marginTopPx) di ogni pagina per la centratura:
	// (0,0) senza centratura o a pagina piena, altrimenti lo spazio
	// residuo dimezzato -- un elemento per pageOrigins, stesso indice.
	std::vector<BPoint> pageOffsets;
	float pageWidth;
	float pageHeight;
	float marginLeftPx;
	float marginTopPx;
	double scale;
};

// Margini in centimetri -> pixel (usando la risoluzione VERA del
// dispositivo, xDPI/yDPI da BPrintJob::GetResolution), poi scala
// (percentuale fissa o "adatta", vedi ComputePrintFitScale sopra) e
// origini di pagina (vedi ComputePrintPageOrigins sopra), in un'unica
// chiamata. printableWidth/printableHeight sono
// BPrintJob::PrintableRect().Width()/Height() (l'area stampabile PRIMA
// dei margini). pageOrigins vuoto se i margini lasciano meno spazio
// utile della sola banda di intestazione (stessa condizione di
// sicurezza gia' in ComputePrintPageOrigins) -- il chiamante deve
// trattarlo come "niente da stampare/mostrare", non come un errore.
// fitWide/fitTall servono solo con scaleMode kPrintFitPages (quante
// pagine di larghezza/altezza), ignorati negli altri modi. centerH/
// centerV spostano il contenuto al centro dell'area utile di OGNI pagina
// (solo le pagine parziali si muovono davvero, vedi pageOffsets sopra).
// titleRowsH/titleColsW sono le bande dei titoli di stampa (righe/colonne
// da ripetere): riservate su ogni pagina in impaginazione (origini) ma
// NON nei totali di scala -- sono celle di contenuto, gia' dentro
// contentRect, sommarle li' le conterebbe due volte.
PrintJobLayout ComputePrintJobLayout(BRect contentRect,
	float printableWidth, float printableHeight, int32 xDPI, int32 yDPI,
	double marginTopCm, double marginBottomCm, double marginLeftCm, double marginRightCm,
	int scaleMode, double scalePercent, float headerW, float headerH,
	int fitWide = 1, int fitTall = 1, bool centerH = false, bool centerV = false,
	float footerH = 0, bool acrossFirst = false,
	float titleRowsH = 0, float titleColsW = 0);

// Espande i codici di intestazione/pie' di pagina nel testo del modello:
// &P numero di pagina, &N pagine totali, &D data corrente (GG.MM.AAAA),
// && una e-commerciale letterale. Un codice sconosciuto (&X) resta com'e'
// ("&X"), mai perso in silenzio -- il modello resta leggibile anche se
// un codice non e' supportato. Funzione pura (stesso testo in anteprima
// e stampa vera), testabile senza stampante.
BString ExpandPrintHeaderCodes(const char* templ, int page, int pages);

// Nomi di colonna 1-based ("A".."ZZ", come SheetView::ColumnName ma qui
// come funzione pura riusabile anche dal dialogo): "out" deve contenere
// almeno 8 byte. Nessun controllo sui limiti (il chiamante valida).
void PrintColumnName(int col, char* out, size_t outSize);

// Intervalli di titoli di stampa ("Imposta pagina", righe/colonne da
// ripetere su OGNI pagina): "1:3" o "2" per le righe, "A:C" o "B" per le
// colonne (maiuscole/minuscole indifferenti, spazi tollerati). Riempiono
// first/last (1-based, first<=last) e tornano true; testo vuoto = nessun
// titolo (first=last=0, true); formato errato o fuori [1, maxRow/maxCol]
// = false (il dialogo azzera, mai valori a meta'). Funzioni pure,
// testabili senza stampante ne' vista.
bool ParsePrintTitleRows(const char* text, int maxRow, int* first, int* last);
bool ParsePrintTitleCols(const char* text, int maxCol, int* first, int* last);

#endif
