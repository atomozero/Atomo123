/*
	test_selection_stats.cpp

	Verifica il footer Somma/Media/Massimo/Conteggio della selezione
	(MainWindow::SelectionChanged), aggiunto su richiesta dell'utente
	("vorrei avere un footer dove posso vedere la somma delle celle
	selezionate o la media o il valore max come in Excel").

	Solo le celle NUMERICHE contano, esattamente come lo status bar di
	Excel: testo/vuoto/errore vengono ignorati, mai contati come zero
	(altrimenti la media sarebbe sbagliata). Vuoto (nessun testo) se la
	selezione non contiene nessun valore numerico, per non mostrare
	"Somma: 0" su una selezione di sole etichette testuali.
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

	view->SetSelection(cell(3, 11)); // C11 da sola
	Check(strcmp(win->SelectionStatsText(), "Somma: 28   Media: 28   Massimo: 28   Conteggio: 1") == 0,
		"una sola cella numerica (C11=28) mostra Somma/Media/Massimo uguali, Conteggio 1");

	view->SetSelection(cell(3, 11)); // C11
	view->ExtendSelection(cell(4, 11)); // esteso a C11:D11
	Check(strcmp(win->SelectionStatsText(), "Somma: 189   Media: 94.5   Massimo: 161   Conteggio: 2") == 0,
		"C11:D11 (28, 161) mostra Somma 189, Media 94.5, Massimo 161, Conteggio 2");

	view->SetSelection(cell(3, 11)); // C11
	view->ExtendSelection(cell(5, 11)); // esteso a C11:E11, include il testo E11
	Check(strcmp(win->SelectionStatsText(), "Somma: 189   Media: 94.5   Massimo: 161   Conteggio: 2") == 0,
		"C11:E11 (28, 161, \"Ciao\") ignora la cella di testo, stessa statistica di prima");

	view->SetSelection(cell(5, 11)); // E11, solo testo
	Check(strcmp(win->SelectionStatsText(), "") == 0,
		"una selezione di sole celle di testo (E11) lascia il footer vuoto, non \"Somma: 0\"");

	view->SetSelection(cell(9, 9)); // I9, cella vuota
	Check(strcmp(win->SelectionStatsText(), "") == 0,
		"una cella vuota lascia il footer vuoto");

	view->SetSelection(cell(3, 12)); // C12, formula calcolata (=C11+D11 = 189)
	Check(strcmp(win->SelectionStatsText(), "Somma: 189   Media: 189   Massimo: 189   Conteggio: 1") == 0,
		"una cella con formula gia' calcolata conta per il suo valore numerico (189), non per il testo della formula");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
