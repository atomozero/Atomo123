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

#include <utility>
#include <vector>

#include <DataIO.h>
#include <String.h>
#include <SupportDefs.h>

#include "Chart.h"

class CContainer;

// "charts"/"colWidths" sono opzionali (NULL = non legge/scrive nulla
// di quella sezione, comportamento invariato per chi non ne ha
// bisogno, es. i test di round-trip gia' esistenti) -- entrambe
// sezioni aggiunte in coda al formato: un file ASCD scritto prima di
// una di queste modifiche semplicemente non ce l'ha, e LoadASCD lo
// riconosce distinguendo "fine del file" (sezione assente, non un
// errore) da un file davvero troncato/corrotto -- vedi il commento in
// AscdIO.cpp. I byte di ciascuna sezione vengono comunque sempre
// consumati dallo stream se presenti, anche passando NULL: altrimenti
// una chiamata con NULL lascerebbe la posizione di lettura sbagliata
// per chi legge subito dopo (es. LoadASCDBook, che concatena piu'
// blocchi ASCD sullo stesso flusso).
//
// "colWidths" e' l'elenco (colonna 1-based, larghezza in pixel) delle
// SOLE colonne con una larghezza diversa da quella predefinita
// (SheetView::kColWidth) -- letta da un file importato (vedi
// XlsxTranslator) o da un ridimensionamento fatto a mano
// (SheetView::CustomColumnWidths), non un array denso su tutte le
// kColCount colonne.
status_t LoadASCD(BPositionIO* source, CContainer* doc,
	std::vector<ChartObject>* charts = NULL,
	std::vector<std::pair<int, float> >* colWidths = NULL);
status_t SaveASCD(CContainer* doc, BPositionIO* dest,
	const std::vector<ChartObject>* charts = NULL,
	const std::vector<std::pair<int, float> >* colWidths = NULL);

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

// Un foglio di una cartella di lavoro multi-foglio: nome piu'
// documento (creato da LoadASCDBook, di proprieta' del chiamante --
// va rilasciato con Release() quando non serve piu', stesso discorso
// gia' valido per qualunque CContainer). "charts"/"colWidths"
// seguono lo stesso significato di LoadASCD/SaveASCD sopra.
struct AscdSheet {
	BString name;
	CContainer* doc;
	std::vector<ChartObject> charts;
	std::vector<std::pair<int, float> > colWidths;
};

// Vero solo se "source" comincia con la firma di una cartella di
// lavoro multi-foglio (riporta la posizione di lettura a dove si
// trovava prima di controllare) -- distingue un file "ASCB" da un
// vecchio ".ascd" a un solo foglio ("ASCD", senza wrapper), che resta
// leggibile con IsASCDFile/LoadASCD come prima.
bool IsASCDBookFile(BPositionIO* source);

// Formato "cartella di lavoro" (piu' fogli): firma "ASCB" piu' il
// numero di fogli, seguiti per ciascuno dal nome e da un blocco ASCD
// completo -- la stessa identica cosa che scrivono/leggono
// SaveASCD/LoadASCD sopra, riusate cosi' come sono (nessuna
// duplicazione del formato per cella) invece di reinventare la
// serializzazione: ogni blocco ASCD e' gia' auto-delimitante tramite
// i propri conteggi interni, quindi concatenarne N in sequenza sullo
// stesso stream e rileggerli con lo stesso numero di chiamate a
// LoadASCD funziona senza bisogno di offset o lunghezze esplicite fra
// un foglio e il successivo.
status_t SaveASCDBook(const std::vector<AscdSheet>& sheets, BPositionIO* dest);
status_t LoadASCDBook(BPositionIO* source, std::vector<AscdSheet>* outSheets);

// Ricalcola tutte le celle con formula del documento fino a
// convergenza (o a un limite di passate). Usata da LoadASCD dopo aver
// popolato le celle (TryToParseString non calcola, quindi senza
// questo passo le celle con formula restano vuote finche' l'utente
// non le tocca a mano) e, soprattutto, da MainWindow/SheetView dopo
// OGNI modifica di una cella (editing, taglia/incolla/cancella,
// trova e sostituisci) invece del solo CalcCell() sulla cella
// toccata: CContainer non tiene un grafo delle dipendenze, quindi
// CalcCell() su una sola cella non si propaga a chi la referenzia in
// formula altrove -- bug scoperto in Fase 6 ("se modifico A1, B1
// (=A1+5) resta al valore vecchio finche' non lo tocco anch'esso").
// Il costo (piu' passate su tutte le celle CON CONTENUTO, non
// sull'intero foglio virtuale -- vedi GetBounds()) resta accettabile
// anche su fogli grandi: e' lo stesso meccanismo gia' usato e
// verificato al caricamento file.
void RecalculateAll(CContainer* doc);

#endif
