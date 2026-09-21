/*
	test_pivot_refresh.cpp

	Verifica che una tabella pivot creata tramite
	MainWindow::HandlePivotRequest resti "viva" davvero: persistita nel
	documento (CContainer::AddPivotTable), FERMA sulla sua cache finche'
	i dati sorgente cambiano (stesso comportamento del vero "Aggiorna"
	di Excel, non un ricalcolo automatico a ogni modifica), e solo
	MainWindow::RefreshAllPivotTables la fa aggiornare per davvero,
	sia nelle celle scritte sul foglio sia nella cache stessa.

	La logica di raggruppamento/aggregazione (BuildPivotTable/
	WritePivotTable) e' gia' testata a fondo in tests/test_pivot.cpp;
	il round-trip di salvataggio/ricarica dell'oggetto persistito in
	tests/test_ascd_io.cpp. Qui si verifica solo il comportamento
	"vivo" end-to-end, stesso principio di test_condformat.cpp.
*/

#include <cstdio>

#include <Application.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
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
	BApplication app("application/x-vnd.Atomo-TestPivotRefresh");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// Frutta/quantita', con "Mela" ripetuta due volte -- stesso caso
	// base di test_pivot.cpp.
	doc->NewCell(cell(1, 1), Value("Mela"), NULL);
	doc->NewCell(cell(2, 1), Value(10.0), NULL);
	doc->NewCell(cell(1, 2), Value("Pera"), NULL);
	doc->NewCell(cell(2, 2), Value(5.0), NULL);
	doc->NewCell(cell(1, 3), Value("Mela"), NULL);
	doc->NewCell(cell(2, 3), Value(20.0), NULL);

	win->HandlePivotRequest("A1:B3", "D1", (int32)ePivotSum);

	Check(doc->GetPivotTables().size() == 1,
		"HandlePivotRequest persiste una tabella pivot");

	Value melaBefore;
	doc->GetValue(cell(5, 2), melaBefore); // E2, aggregazione di Mela
	Check((double)melaBefore == 30.0, "Mela aggregata a 30 (10+20) subito dopo la creazione");

	// Cambia un dato SORGENTE direttamente (non tramite la finestra
	// Pivot): la cache/le celle di destinazione NON devono seguire da
	// sole -- prova che il pivot persistito resta fermo sulla sua
	// cache finche' non arriva un comando esplicito, esattamente come
	// una vera pivot table Excel prima di "Aggiorna".
	doc->NewCell(cell(2, 3), Value(100.0), NULL); // Mela, riga 3: 20 -> 100

	Value melaFrozen;
	doc->GetValue(cell(5, 2), melaFrozen);
	Check((double)melaFrozen == 30.0,
		"cambiare un dato sorgente NON aggiorna da solo le celle della pivot (cache ferma)");
	Check(doc->GetPivotTables()[0].cachedRows.size() == 2
			&& doc->GetPivotTables()[0].cachedRows[0].aggregate == 30.0,
		"la cache persistita resta ferma anch'essa, non solo le celle");

	// Ora il comando esplicito: le celle E la cache devono riflettere
	// il nuovo totale (10+100=110).
	win->RefreshAllPivotTables();

	Value melaAfter;
	doc->GetValue(cell(5, 2), melaAfter);
	Check((double)melaAfter == 110.0,
		"RefreshAllPivotTables aggiorna davvero le celle di destinazione (10+100=110)");
	Check(doc->GetPivotTables()[0].cachedRows.size() == 2
			&& doc->GetPivotTables()[0].cachedRows[0].aggregate == 110.0,
		"RefreshAllPivotTables aggiorna anche la cache persistita");

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
