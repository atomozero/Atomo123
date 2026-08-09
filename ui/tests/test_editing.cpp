/*
	test_editing.cpp

	Verifica l'editing in-cella: digitare direttamente su una cella
	selezionata apre l'editor (SheetView::KeyDown, caso "default" per
	un carattere stampabile), e confermare scrive il valore nel
	documento. Bug segnalato dall'utente: "se scrivo un numero in una
	cella e premo invio non succede nulla" -- il valore veniva in
	realta' scritto correttamente (CommitEditing gia' funzionava), ma
	la selezione restava ferma sulla stessa cella invece di avanzare a
	quella sotto come in Excel/LibreOffice Calc, dando l'impressione
	che il tasto Invio non facesse nulla.

	Aziona KeyDown() e MessageReceived() direttamente (entrambi
	pubblici) invece di passare dal vero dispatch della tastiera --
	stesso principio di test_scroll.cpp: piu' affidabile di
	un'iniezione di input reale (vedi ROADMAP.md/docs/UI_ARCHITECTURE.md
	per i problemi gia' documentati con hey/Pippo, incluso un episodio
	in questa stessa sessione in cui l'iniezione e' finita per errore
	su una finestra di un'altra sessione concorrente sullo stesso
	desktop condiviso). kMsgCellEditCommit e' privato/statico in
	SheetView.cpp (valore 'cedt'): riprodotto qui letteralmente perche'
	non e' esportato nell'header pubblico.
*/

#include <cstdio>

#include <Application.h>
#include <Window.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"

static const uint32 kMsgCellEditCommit = 'cedt';

static int gFailures = 0;

static void Check(bool condition, const char* what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

class TestWindow : public BWindow {
public:
	TestWindow()
		: BWindow(BRect(100, 100, 700, 500), "test-editing", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestEditing");

	CContainer* doc = new CContainer(NULL, NULL);
	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	win->AddChild(view);
	win->Show();

	win->Lock();

	// Digitare "4" su A1 (selezionata di default) apre l'editor con
	// quel carattere gia' dentro -- lo stesso percorso di
	// StartEditing usato per la digitazione diretta sulla griglia.
	char digit = '4';
	view->KeyDown(&digit, 1);

	// Confermare (lo stesso messaggio che BTextControl manda da solo
	// al proprio target premendo Invio al suo interno) deve scrivere
	// il valore in A1.
	BMessage commit(kMsgCellEditCommit);
	view->MessageReceived(&commit);

	Value v;
	doc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 4.0,
		"digitare un numero su una cella e confermare scrive il valore nel documento");

	cell afterCommit = view->Selection();
	Check(afterCommit.h == 1 && afterCommit.v == 2,
		"confermando con Invio la selezione avanza alla cella sotto (come Excel/LibreOffice Calc)");

	// Stesso giro su A2 (dove siamo finiti): confermare con Invio deve
	// avanzare ancora, non restare ferma indefinitamente sulla stessa
	// riga dopo la prima conferma.
	char digit2 = '7';
	view->KeyDown(&digit2, 1);
	view->MessageReceived(&commit);

	doc->GetValue(cell(1, 2), v);
	Check(v.fType == eNumData && (double)v == 7.0,
		"il secondo valore digitato viene scritto nella cella corretta (A2, dove si era avanzati)");

	cell afterSecondCommit = view->Selection();
	Check(afterSecondCommit.h == 1 && afterSecondCommit.v == 3,
		"la selezione avanza di nuovo dopo la seconda conferma (A2 -> A3)");

	// Annullare con Escape (CommitEditing(true), lo stesso messaggio
	// che CellEditEscapeFilter manda) non deve invece avanzare la
	// selezione ne' scrivere nulla -- comportamento diverso da Invio,
	// coerente con Excel/LibreOffice Calc.
	static const uint32 kMsgCellEditCancel = 'cedc';
	char digit3 = '9';
	view->KeyDown(&digit3, 1);
	BMessage cancel(kMsgCellEditCancel);
	view->MessageReceived(&cancel);

	doc->GetValue(cell(1, 3), v);
	Check(v.fType != eNumData,
		"annullare con Escape non scrive nessun valore nella cella");

	cell afterCancel = view->Selection();
	Check(afterCancel.h == 1 && afterCancel.v == 3,
		"annullare con Escape non fa avanzare la selezione (resta su A3)");

	// Tab durante l'editing in-cella (bug segnalato dall'utente: Tab
	// non confermava affatto il valore, la BTextView interna lo
	// trattava come normale spostamento del fuoco tastiera -- vedi
	// CellEditKeyFilter): come Invio scrive il valore, ma sposta la
	// selezione a destra invece che in basso.
	static const uint32 kMsgCellEditCommitTabRight = 'cetr';
	static const uint32 kMsgCellEditCommitTabLeft = 'cetl';
	view->SetSelection(cell(1, 5));	// A5
	char digitTab = '8';
	view->KeyDown(&digitTab, 1);
	BMessage tabRight(kMsgCellEditCommitTabRight);
	view->MessageReceived(&tabRight);

	doc->GetValue(cell(1, 5), v);
	Check(v.fType == eNumData && (double)v == 8.0,
		"Tab durante l'editing scrive il valore nella cella, non solo Invio");

	cell afterTab = view->Selection();
	Check(afterTab.h == 2 && afterTab.v == 5,
		"confermando con Tab la selezione avanza a destra (A5 -> B5), non in basso");

	// Maiusc+Tab sposta a sinistra invece che a destra, stessa
	// direzione gia' usata da Tab fuori dall'editing in-cella.
	char digitShiftTab = '3';
	view->KeyDown(&digitShiftTab, 1);
	BMessage tabLeft(kMsgCellEditCommitTabLeft);
	view->MessageReceived(&tabLeft);

	doc->GetValue(cell(2, 5), v);
	Check(v.fType == eNumData && (double)v == 3.0,
		"Maiusc+Tab durante l'editing scrive comunque il valore (in B5, dove si era avanzati con Tab)");

	cell afterShiftTab = view->Selection();
	Check(afterShiftTab.h == 1 && afterShiftTab.v == 5,
		"confermando con Maiusc+Tab la selezione torna a sinistra (B5 -> A5)");

	// Propagazione a celle dipendenti: CContainer non tiene un grafo
	// delle dipendenze, quindi modificare una cella referenziata da
	// una formula altrove (qui B1="=A1+5") deve ricalcolare anche
	// quella formula, non solo la cella appena modificata -- bug
	// scoperto in Fase 6 ("ottimizzazione ricalcolo su fogli grandi":
	// il problema reale non era la velocita', ma che le formule
	// dipendenti restavano non aggiornate finche' non venivano
	// toccate a mano). Fix: CommitEditing chiama RecalculateAll(fDoc)
	// invece del solo CalcCell(editedCell).
	view->SetSelection(cell(1, 4));	// A4
	char digit4 = '1';
	view->KeyDown(&digit4, 1);
	view->MessageReceived(&commit);	// A4 = 1

	TryToParseString("=A4+5", cell(2, 4), doc, true);	// B4 = "=A4+5"
	doc->CalcCell(cell(2, 4));

	Value b4;
	doc->GetValue(cell(2, 4), b4);
	Check(b4.fType == eNumData && (double)b4 == 6.0,
		"B4 (=A4+5) calcola 6 col primo valore di A4 (1)");

	view->SetSelection(cell(1, 4));	// torna su A4
	char digit5 = '2';
	view->KeyDown(&digit5, 1);
	view->MessageReceived(&commit);	// A4 = 2, non piu' "1"

	doc->GetValue(cell(2, 4), b4);
	Check(b4.fType == eNumData && (double)b4 == 7.0,
		"modificando A4 (senza toccare B4 direttamente), B4 si aggiorna da solo a 7 "
		"(non resta fermo al valore vecchio, 6)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
