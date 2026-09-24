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
#include "Pivot.h"

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

	// --- Stesso comportamento "vivo" per un pivot 2D (campo Colonne +
	// misure multiple) costruito tramite la firma estesa di
	// HandlePivotRequest -- verifica che PivotIsMultiDimensional() lo
	// riconosca e RefreshAllPivotTables() lo aggiorni per davvero
	// tramite BuildPivotTable2D/WritePivotTable2D, non il vecchio
	// percorso 1D. ---
	doc->NewCell(cell(1, 10), Value("Nord"), NULL); // A10
	doc->NewCell(cell(2, 10), Value("Q1"), NULL);   // B10
	doc->NewCell(cell(3, 10), Value(100.0), NULL);  // C10
	doc->NewCell(cell(1, 11), Value("Nord"), NULL); // A11
	doc->NewCell(cell(2, 11), Value("Q2"), NULL);   // B11
	doc->NewCell(cell(3, 11), Value(200.0), NULL);  // C11
	doc->NewCell(cell(1, 12), Value("Sud"), NULL);  // A12
	doc->NewCell(cell(2, 12), Value("Q1"), NULL);   // B12
	doc->NewCell(cell(3, 12), Value(50.0), NULL);   // C12
	doc->NewCell(cell(1, 13), Value("Sud"), NULL);  // A13
	doc->NewCell(cell(2, 13), Value("Q2"), NULL);   // B13
	doc->NewCell(cell(3, 13), Value(80.0), NULL);   // C13

	std::vector<int32> measureCols2D, measureAggs2D;
	std::vector<BString> measureLabels2D;
	measureCols2D.push_back(3); // C
	measureAggs2D.push_back((int32)ePivotSum);
	win->HandlePivotRequest("A10:C13", "E10", (int32)ePivotSum,
		2 /* columnFieldCol = B */, measureCols2D, measureAggs2D, measureLabels2D);

	Check(doc->GetPivotTables().size() == 2,
		"HandlePivotRequest persiste anche il pivot 2D (ora due tabelle in totale)");
	if (doc->GetPivotTables().size() == 2)
	{
		const PivotTableObject& p2D = doc->GetPivotTables()[1];
		Check(PivotIsMultiDimensional(p2D),
			"PivotIsMultiDimensional riconosce il pivot appena creato come 2D");
		Check(p2D.columnFieldCol == 2 && p2D.measures.size() == 1 && p2D.measures[0].sourceCol == 3,
			"il pivot 2D persistito ha il campo Colonne e la misura giusti");
	}

	Value nordQ1Before, sudQ1Before;
	doc->GetValue(cell(6, 12), nordQ1Before); // F12: Nord/Q1
	doc->GetValue(cell(6, 13), sudQ1Before);  // F13: Sud/Q1
	Check((double)nordQ1Before == 100.0 && (double)sudQ1Before == 50.0,
		"il grigliato 2D e' scritto correttamente subito dopo la creazione (Nord/Q1=100, Sud/Q1=50)");

	// Cambia un dato sorgente direttamente: stesso principio del blocco
	// 1D sopra, il grigliato 2D deve restare fermo finche' non arriva
	// un Aggiorna esplicito.
	doc->NewCell(cell(3, 12), Value(999.0), NULL); // Sud/Q1: 50 -> 999

	Value sudQ1Frozen;
	doc->GetValue(cell(6, 13), sudQ1Frozen);
	Check((double)sudQ1Frozen == 50.0,
		"cambiare un dato sorgente NON aggiorna da solo il grigliato 2D (cache ferma)");
	Check(doc->GetPivotTables()[1].cachedRows2D[1].cells[0][0].aggregate == 50.0,
		"la cache 2D persistita resta ferma anch'essa, non solo le celle");

	win->RefreshAllPivotTables();

	Value sudQ1After;
	doc->GetValue(cell(6, 13), sudQ1After);
	Check((double)sudQ1After == 999.0,
		"RefreshAllPivotTables aggiorna davvero le celle del grigliato 2D (Sud/Q1 -> 999)");
	Check(doc->GetPivotTables()[1].cachedRows2D[1].cells[0][0].aggregate == 999.0,
		"RefreshAllPivotTables aggiorna anche la cache 2D persistita");

	// Il pivot 1D (indice 0) non deve aver risentito in alcun modo del
	// refresh del pivot 2D accanto ad esso.
	Value melaStillOk;
	doc->GetValue(cell(5, 2), melaStillOk);
	Check((double)melaStillOk == 110.0,
		"il pivot 1D accanto resta corretto dopo il refresh del pivot 2D (nessuna interferenza)");

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
