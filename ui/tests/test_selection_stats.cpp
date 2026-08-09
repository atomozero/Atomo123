/*
	test_selection_stats.cpp

	Verifica il footer con le statistiche della selezione
	(MainWindow::SelectionChanged), aggiunto su richiesta dell'utente
	("vorrei avere un footer dove posso vedere la somma delle celle
	selezionate o la media o il valore max come in Excel") e poi reso
	personalizzabile (Fase 17, "il footer lo vorrei che si comportasse
	esattamente come excel"): quali statistiche compaiono e' scelto
	dall'utente col tasto destro (MainWindow::ToggleFooterStat), non
	piu' fisso -- il default riproduce esattamente il default reale di
	Excel (Media/Conteggio/Somma accesi, Conteggio numerico/Minimo/
	Massimo spenti).

	Media/Minimo/Massimo/Conteggio numerico/Somma contano solo le celle
	NUMERICHE, esattamente come lo status bar di Excel: testo/vuoto/
	errore vengono ignorati, mai contati come zero (altrimenti la media
	sarebbe sbagliata). Conteggio invece conta ogni cella non vuota
	della selezione, testo compreso -- anche questo comportamento reale
	di Excel, non un'approssimazione.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
#include "Range.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"

// kMsgCellEditCommit/kMsgCellEditCancel sono privati/statici in
// SheetView.cpp: riprodotti qui letteralmente ('cedt'/'cedc'), stesso
// principio gia' documentato in tests/test_editing.cpp.
static const uint32 kMsgCellEditCommit = 'cedt';
static const uint32 kMsgCellEditCancel = 'cedc';

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

int main()
{
	BApplication app("application/x-vnd.Atomo-TestSelectionStats");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("28", cell(3, 11), doc, true);  // C11
	TryToParseString("161", cell(4, 11), doc, true); // D11
	TryToParseString("Ciao", cell(5, 11), doc, true); // E11, testo (ignorato)
	TryToParseString("=C11+D11", cell(3, 12), doc, true); // C12, formula = 189
	doc->CalcCell(cell(3, 12));

	// Default = esattamente il default reale di Excel: Media/Conteggio/
	// Somma accesi, Conteggio numerico/Minimo/Massimo spenti.
	view->SetSelection(cell(3, 11)); // C11 da sola
	Check(strcmp(win->SelectionStatsText(), "Media: 28   Conteggio: 1   Somma: 28") == 0,
		"una sola cella numerica (C11=28) mostra Media/Conteggio/Somma (default di Excel)");

	view->SetSelection(cell(3, 11)); // C11
	view->ExtendSelection(cell(4, 11)); // esteso a C11:D11
	Check(strcmp(win->SelectionStatsText(), "Media: 94.5   Conteggio: 2   Somma: 189") == 0,
		"C11:D11 (28, 161) mostra Media 94.5, Conteggio 2, Somma 189");

	view->SetSelection(cell(3, 11)); // C11
	view->ExtendSelection(cell(5, 11)); // esteso a C11:E11, include il testo E11
	Check(strcmp(win->SelectionStatsText(), "Media: 94.5   Conteggio: 3   Somma: 189") == 0,
		"C11:E11 (28, 161, \"Ciao\") ignora il testo per Media/Somma, ma lo conta in Conteggio (come Excel)");

	view->SetSelection(cell(5, 11)); // E11, solo testo
	Check(strcmp(win->SelectionStatsText(), "Conteggio: 1") == 0,
		"una selezione di sole celle di testo (E11) mostra comunque Conteggio: 1 (come Excel), non Somma/Media");

	view->SetSelection(cell(9, 9)); // I9, cella vuota
	Check(strcmp(win->SelectionStatsText(), "") == 0,
		"una cella vuota lascia il footer vuoto");

	view->SetSelection(cell(3, 12)); // C12, formula calcolata (=C11+D11 = 189)
	Check(strcmp(win->SelectionStatsText(), "Media: 189   Conteggio: 1   Somma: 189") == 0,
		"una cella con formula gia' calcolata conta per il suo valore numerico (189), non per il testo della formula");

	// Personalizzazione (Fase 17, "il footer lo vorrei che si comportasse
	// esattamente come excel"): ToggleFooterStat accende/spegne una
	// statistica per volta, esattamente come il vero menu contestuale
	// del tasto destro sullo status bar di Excel.
	view->SetSelection(cell(3, 11)); // C11 = 28, da sola
	win->ToggleFooterStat(MainWindow::kStatMax);
	win->ToggleFooterStat(MainWindow::kStatMin);
	win->ToggleFooterStat(MainWindow::kStatNumCount);
	Check(strcmp(win->SelectionStatsText(),
			"Media: 28   Conteggio: 1   Conteggio numerico: 1   Minimo: 28   Massimo: 28   Somma: 28") == 0,
		"accendendo Minimo/Massimo/Conteggio numerico compaiono nell'ordine del menu di Excel");

	win->ToggleFooterStat(MainWindow::kStatAverage);
	win->ToggleFooterStat(MainWindow::kStatCount);
	win->ToggleFooterStat(MainWindow::kStatSum);
	Check(strcmp(win->SelectionStatsText(), "Conteggio numerico: 1   Minimo: 28   Massimo: 28") == 0,
		"spegnendo Media/Conteggio/Somma restano solo le tre appena accese");

	// Riporta la maschera allo stato di default per non alterare gPrefs
	// per gli altri test/l'uso reale dell'app dopo questo test.
	win->ToggleFooterStat(MainWindow::kStatMax);
	win->ToggleFooterStat(MainWindow::kStatMin);
	win->ToggleFooterStat(MainWindow::kStatNumCount);
	win->ToggleFooterStat(MainWindow::kStatAverage);
	win->ToggleFooterStat(MainWindow::kStatCount);
	win->ToggleFooterStat(MainWindow::kStatSum);
	Check(win->FooterStatsMask() == (MainWindow::kStatAverage | MainWindow::kStatCount | MainWindow::kStatSum),
		"la maschera torna al default di Excel dopo aver invertito ogni bit due volte");

	// Indicatore di modalita' ("Pronto"/"Modifica"), come Excel: acceso
	// dall'editing in-cella e spento di nuovo confermando o annullando.
	// KeyDown()/MessageReceived() (entrambi pubblici) invece di
	// StartEditing/CommitEditing diretti (privati) -- stesso principio
	// di tests/test_editing.cpp.
	view->SetSelection(cell(3, 11)); // C11 = 28
	Check(strcmp(win->CellModeText(), "Pronto") == 0,
		"il footer parte in modalita' \"Pronto\", nessun editing in corso");

	char digit = '9';
	view->KeyDown(&digit, 1);
	Check(strcmp(win->CellModeText(), "Modifica") == 0,
		"digitare su una cella (SheetView::StartEditing) porta il footer in modalita' \"Modifica\"");

	// Da qui in poi il documento e' davvero modificato (MainWindow::
	// MarkModified, chiamata da CommitEditing tramite NotifyDocument
	// Changed/DocumentChanged): il footer lo segnala con lo stesso "* "
	// del titolo della finestra (vedi RefreshCellModeText).
	BMessage commitMsg(kMsgCellEditCommit);
	view->MessageReceived(&commitMsg);
	Check(strcmp(win->CellModeText(), "* Pronto") == 0,
		"confermando l'editing (Invio) il footer torna a \"Pronto\", con \"* \" perche' il documento e' ora modificato");

	char digit2 = '1';
	view->KeyDown(&digit2, 1);
	BMessage cancelMsg(kMsgCellEditCancel);
	view->MessageReceived(&cancelMsg);
	Check(strcmp(win->CellModeText(), "* Pronto") == 0,
		"annullando l'editing (Escape) il footer torna comunque a \"* Pronto\" (il documento resta modificato da prima)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
