/*
	test_names.cpp

	Verifica gli intervalli con nome (Fase 7) su una vera MainWindow,
	non solo la logica isolata del motore (vedi
	engine/tests/named_ranges_test.cpp per quella): MainWindow::
	HandleDefineName/HandleDeleteName/HandleGoToName sono i metodi
	pubblici richiamati da NameWindow tramite BMessage (vedi
	MainWindow::MessageReceived, casi kMsgDefineName/kMsgDeleteName/
	kMsgGoToName) -- chiamati qui direttamente, come CopySelection/
	PasteSelection in tests/test_paste_range.cpp, per non dover
	gestire un vero giro di dispatch dei messaggi in un test headless.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"

static int gFailures = 0;

static void Check(bool condition, const char *what)
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
	BApplication app("application/x-vnd.Atomo-TestNames");

	MainWindow *win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer *doc = win->GetSheetView()->Document();
	TryToParseString("10", cell(1, 1), doc, true); // A1
	TryToParseString("20", cell(1, 2), doc, true); // A2

	// Definire "Totale" su A1 tramite HandleDefineName (stesso
	// percorso di NameWindow che preme "Aggiungi/Aggiorna").
	win->HandleDefineName("Totale", "A1");

	TryToParseString("=Totale*2", cell(2, 1), doc, true); // B1
	// IsConstant() e' false per un valName (Fase 7): TryToParseString
	// non calcola subito, la formula resta viva finche' qualcosa la
	// calcola davvero -- stesso principio gia' verificato in
	// engine/tests/named_ranges_test.cpp.
	doc->CalcCell(cell(2, 1));
	Value v;
	doc->GetValue(cell(2, 1), v);
	Check((double)v == 20.0,
		"=Totale*2 con Totale=A1 (10), definito tramite HandleDefineName, calcola 20");

	// Ridefinire il nome su A2 tramite un secondo HandleDefineName
	// (stesso pulsante "Aggiungi/Aggiorna", chiamato di nuovo) propaga
	// alla formula esistente tramite il ricalcolo dell'intera cartella
	// di lavoro (RecalculateActiveWorkbook, chiamato dentro
	// HandleDefineName -- una ridefinizione puo' cambiare qualunque
	// formula, non solo quelle toccate dopo).
	win->HandleDefineName("Totale", "A2");
	doc->GetValue(cell(2, 1), v);
	Check((double)v == 40.0,
		"dopo HandleDefineName(\"Totale\", \"A2\"), B1 ricalcola subito a 40 (20*2), "
		"senza bisogno di un CalcCell esplicito");

	// "Vai a" sposta la selezione della SheetView attiva sull'intervallo
	// risolto -- qui una sola cella (A2).
	win->HandleGoToName("Totale");
	Check(win->GetSheetView()->Selection() == cell(1, 2),
		"HandleGoToName(\"Totale\") sposta la selezione su A2, dove Totale e' ridefinito");

	// Un intervallo multi-cella: "Vai a" estende la selezione, non solo
	// l'angolo in alto a sinistra.
	win->HandleDefineName("Intervallo", "A1:A2");
	win->HandleGoToName("Intervallo");
	range selRange = win->GetSheetView()->SelectionRange();
	Check(selRange.TopLeft() == cell(1, 1) && selRange.BotRight() == cell(1, 2),
		"HandleGoToName(\"Intervallo\") su A1:A2 estende la selezione a entrambe le celle");

	// Eliminare il nome tramite HandleDeleteName (pulsante "Elimina" di
	// NameWindow): il nome non e' piu' risolvibile, ma la formula resta
	// viva (non crasha, si limita a non risolversi -- stesso principio
	// gia' verificato per i riferimenti fra fogli in xsheet_test.cpp).
	win->HandleDeleteName("Totale");
	bool threw = false;
	try { doc->ResolveName("Totale"); }
	catch (...) { threw = true; }
	Check(threw, "dopo HandleDeleteName(\"Totale\"), ResolveName(\"Totale\") non trova piu' nulla");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
