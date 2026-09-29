/*
	ScenarioWindow.h

	Finestra "Gestione scenari" (Tier 4, "Path to full Excel parity" --
	l'altra meta' di "Tabella dati", vedi ROADMAP.md e Scenario.h): elenco
	degli scenari gia' definiti (BListView, un nome per riga), un campo
	nome, un campo per l'intervallo delle celle variabili (con lo stesso
	pulsante "..." selettore di intervallo gia' usato da ChartWindow --
	vedi kMsgScenarioRangePickRequest sotto), un campo di testo
	multi-riga per i valori sostitutivi (un valore per riga, nello stesso
	ordine per righe dell'intervallo) e un campo di commento facoltativo,
	piu' tre pulsanti (Aggiungi/Aggiorna, Elimina, Mostra). Stessa regola
	di ChartWindow/NameWindow: non tocca mai il documento direttamente,
	manda le richieste a MainWindow via BMessage e riceve indietro solo
	dati gia' estratti (mai un puntatore al documento). MainWindow
	richiama SetScenarios() prima di Show() e dopo ogni Aggiungi/
	Aggiorna/Elimina/Mostra per tenere l'elenco allineato a
	MainWindow::Scenarios().

	SEMPLIFICAZIONE dichiarata rispetto al vero Excel (fino a 32 celle/
	intervalli ARBITRARI, anche non contigui, per scenario): qui le
	"celle variabili" sono un SINGOLO intervallo contiguo -- vedi
	Scenario.h sul perche' (RangeRef::ParseRangeRef e
	SheetView::SaveUndoState non supportano oggi nulla di non contiguo).
	"Genera riepilogo scenari" (un nuovo foglio di confronto statico fra
	tutti gli scenari) e' esplicitamente FUORI da questa fase.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef SCENARIO_WINDOW_H
#define SCENARIO_WINDOW_H

#include <Messenger.h>
#include <String.h>
#include <Window.h>
#include <vector>

#include "Scenario.h"

const uint32 kMsgDefineScenario = 'dfsc';
const uint32 kMsgDeleteScenario = 'dlsc';
const uint32 kMsgShowScenario = 'shsn';
// ScenarioWindow non tocca mai SheetView direttamente (vedi il commento
// in cima al file): il pulsante "..." manda questo a fTarget
// (MainWindow), che inoltra a SheetView::StartRangePicker/
// CancelRangePicker -- stesso schema esatto di kMsgChartRangePickRequest/
// kMsgRangePicked in ChartWindow.h, ma con una coppia di costanti
// PARALLELA e indipendente invece di condividere quella: l'unico riuso
// disponibile e' gia' dentro SheetView stessa (StartRangePicker e' gia'
// generico), condividere anche la costante accoppierebbe inutilmente lo
// spazio dei messaggi di due finestre altrimenti indipendenti.
const uint32 kMsgScenarioRangePickRequest = 'scrp';
const uint32 kMsgScenarioRangePicked = 'scrk';

class BButton;
class BListView;
class BTextControl;
class BTextView;

class ScenarioWindow : public BWindow {
public:
	ScenarioWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

	// Chiamata da MainWindow prima di Show() e dopo ogni Aggiungi/
	// Aggiorna/Elimina/Mostra per tenere l'elenco allineato a
	// MainWindow::Scenarios() -- stesso schema di NameWindow::SetNames.
	void SetScenarios(const std::vector<Scenario>& scenarios);

private:
	BListView* fScenarioList;
	BTextControl* fNameField;
	BTextControl* fRangeField;
	// Pulsante "..." accanto a fRangeField, stesso principio esatto di
	// ChartWindow::fRangePickButton (vedi il commento li').
	BButton* fRangePickButton;
	bool fPickingRange;
	// Un valore per riga, in ordine per righe (stesso ordine di
	// Scenario::values) -- niente griglia vera (nessun widget a griglia
	// editabile esiste gia' in questa codebase fuori da SheetView
	// stessa): l'utente inserisce tanti valori quante celle copre
	// l'intervallo, uno per riga, e un numero sbagliato viene rifiutato
	// con un avviso da MainWindow::HandleDefineScenario.
	BTextView* fValuesField;
	BTextControl* fCommentField;
	std::vector<Scenario> fScenarios;
	BMessenger fTarget;

	void SendScenarioMessage(uint32 what);
	// Disarma fRangePickButton/manda "start"=false a fTarget se un
	// selettore era rimasto armato da una sessione precedente -- stesso
	// principio esatto di ChartWindow::CancelPickingIfArmed.
	void CancelPickingIfArmed();
};

#endif
