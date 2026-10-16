/*
	EmbeddedControl.h

	Un controllo ActiveX (Forms 2.0) incorporato nel foglio -- import
	XLSX da xl/worksheets/sheetN.xml <controls> (NON xl/drawings/
	drawingN.xml: quel file ha solo segnaposto vuoti, nascosti, per
	questi stessi controlli) piu' xl/activeX/activeXN.xml/.bin. Stesso
	principio di ancoraggio di EmbeddedImage (vedi EmbeddedImage.h): una
	cella piu' uno scarto e una dimensione in pixel, non un rettangolo
	assoluto, cosi' segue la cella di ancoraggio se righe/colonne
	cambiano dimensione, esattamente come in Excel vero.

	SOLO CommandButton (classid {D7053240-CE69-11CD-A777-00DD01143C57},
	Forms 2.0) e' davvero supportato in rendering/hit-test/dispatch VBA
	in questa fase -- "classid" resta comunque un campo esplicito (non
	un bool "e' un CommandButton") per restare in avanti compatibile: un
	controllo di un altro tipo si importa comunque (ancoraggio/nome),
	ma chi lo consuma lo ignora finche' non viene davvero supportato,
	stesso principio "non pretendere completezza" di ogni altro limite
	dichiarato di questo codice (vedi ROADMAP.md).

	Vive fuori da CContainer (come immagini e grafici, non un campo per
	cella): non e' un dato per cella ma un oggetto flottante disegnato
	sopra la griglia. Struct dati pura, senza dipendenze dal
	Translation/Interface Kit apposta -- stesso motivo di EmbeddedImage
	(usata sia da translators/xlsx che da ui/).
*/

#ifndef EMBEDDED_CONTROL_H
#define EMBEDDED_CONTROL_H

#include <string>

#include "Cell.h"

struct EmbeddedControl {
	cell anchor;
	float offsetX, offsetY;	// scarto in pixel dall'angolo della cella di ancoraggio
	float width, height;		// dimensione visualizzata in pixel

	// Nome VBA del controllo (es. "CommandButton1", "cb0"), l'attributo
	// "name" di <control> -- la chiave di ricerca per i gestori evento
	// "<name>_Click"/"<name>_MouseDown"/"<name>_MouseUp" (vedi
	// MainWindow::RunControlEvent).
	std::string name;

	// Didascalia visualizzata, estratta con un'euristica best-effort
	// dallo stream binario MS-OFORMS (vedi ScanActiveXCaptionHeuristic
	// in XlsxTranslator.cpp) -- NON conformita' piena alla specifica
	// [MS-OFORMS], un'approssimazione dichiarata come ogni altro limite
	// onesto di questo translator. Uguale a "name" quando l'euristica
	// non trova nulla di plausibile nello stream.
	std::string caption;

	// Identificatore a 16 byte del tipo di controllo ActiveX, in forma
	// testuale GUID standard (es. "{D7053240-CE69-11CD-A777-00DD01143C57}"
	// per Forms 2.0 CommandButton) -- vedi il commento sopra la struct
	// sul perche' questo e' un campo esplicito e non un enum/bool.
	std::string classid;

	EmbeddedControl() : offsetX(0), offsetY(0), width(0), height(0) {}
};

#endif
