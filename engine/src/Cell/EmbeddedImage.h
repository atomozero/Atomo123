/*
	EmbeddedImage.h

	Un'immagine incorporata nel foglio (es. un logo importato da XLSX,
	xl/drawings+xl/media, Fase 12) -- ancorata a una cella (come
	l'anchor XLSX stesso, twoCellAnchor/oneCellAnchor) piu' uno scarto
	e una dimensione in pixel, non un rettangolo assoluto come
	ChartObject (vedi ui/src/Chart.h): un'immagine importata deve
	seguire la cella a cui e' ancorata se righe/colonne cambiano
	dimensione, esattamente come in Excel vero.

	Vive fuori da CContainer (come i grafici, non un campo per cella):
	non e' un dato per cella ma un oggetto flottante disegnato sopra la
	griglia. Struct dati pura, senza dipendenze dal Translation/
	Interface Kit apposta -- usata sia da translators/xlsx (che non
	linka contro l'Interface Kit, vedi engine/Makefile) sia da ui/
	(che decodifica fPngData in una BBitmap solo al disegno, mai qui).
*/

#ifndef EMBEDDED_IMAGE_H
#define EMBEDDED_IMAGE_H

#include <vector>

#include <SupportDefs.h>

#include "Cell.h"

struct EmbeddedImage {
	cell anchor;
	float offsetX, offsetY;	// scarto in pixel dall'angolo della cella di ancoraggio
	float width, height;		// dimensione visualizzata in pixel
	std::vector<uint8> pngData;	// blob PNG grezzo, cosi' come letto dal file

	EmbeddedImage() : offsetX(0), offsetY(0), width(0), height(0) {}
};

#endif
