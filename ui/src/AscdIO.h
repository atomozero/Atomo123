/*
	AscdIO.h

	Lettura/scrittura del formato nativo provvisorio ASCD ("Atomo
	Sheet Cell Data") usato anche dai translator (vedi
	translators/csv/CsvTranslator.cpp per la definizione originale
	del formato). L'app non e' un add-on Translation Kit, quindi
	questa e' una copia standalone della stessa logica di
	lettura/scrittura -- stesso approccio gia' seguito da ogni
	translator, che duplica WriteASCD() per non introdurre una
	dipendenza di link tra translator e app.
*/

#ifndef ASCD_IO_H
#define ASCD_IO_H

#include <vector>

#include <DataIO.h>
#include <SupportDefs.h>

#include "Chart.h"

class CContainer;

// "charts" e' opzionale (NULL = non legge/scrive nessun grafico
// incorporato, comportamento invariato per chi non ne ha bisogno,
// es. i test di round-trip gia' esistenti). Il blocco dei grafici e'
// una sezione aggiunta in coda al formato: un file ASCD scritto prima
// di questa modifica non ce l'ha affatto, e LoadASCD lo riconosce
// distinguendo "fine del file" (nessun grafico, non un errore) da un
// file davvero troncato/corrotto -- vedi il commento in AscdIO.cpp.
status_t LoadASCD(BPositionIO* source, CContainer* doc,
	std::vector<ChartObject>* charts = NULL);
status_t SaveASCD(CContainer* doc, BPositionIO* dest,
	const std::vector<ChartObject>* charts = NULL);

// Vero solo se "source" comincia con la firma nativa ASCD (riporta
// la posizione di lettura a dove si trovava prima di controllare).
// MainWindow la usa per leggere un file nativo direttamente con
// LoadASCD invece di farlo passare -- inutilmente e con perdita
// della sezione grafici incorporati -- dal Translation Kit, che per
// un file gia' ASCD lo farebbe comunque rileggere/riscrivere tramite
// la copia duplicata di ReadASCD/WriteASCD di un translator
// qualunque (vedi translators/csv/CsvTranslator.cpp), che non
// conosce quella sezione.
bool IsASCDFile(BPositionIO* source);

// Ricalcola tutte le celle con formula del documento fino a
// convergenza (o a un limite di passate). Usata da LoadASCD dopo aver
// popolato le celle: TryToParseString non calcola, quindi senza
// questo passo le celle con formula restano vuote finche' l'utente
// non le tocca a mano.
void RecalculateAll(CContainer* doc);

#endif
