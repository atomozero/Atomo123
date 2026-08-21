/*
	test_range_ref.cpp

	Verifica FormatRangeRef (RangeRef.h/.cpp), l'inversa di
	ParseRangeRef -- usata per precompilare il campo Intervallo di
	ChartWindow con la selezione corrente del foglio (Fase 18, vedi
	MainWindow::ShowChartWindow). Nessuna sessione grafica: solo
	l'engine per il tipo "range".
*/

#include <cstdio>
#include <cstring>

#include "Range.h"
#include "RangeRef.h"

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
	char buf[32];

	// Una sola cella -> "A1", non "A1:A1" (stessa convenzione del
	// footer, vedi MainWindow::SelectionChanged).
	range singleCell(1, 1, 1, 1);
	FormatRangeRef(singleCell, buf, sizeof(buf));
	Check(strcmp(buf, "A1") == 0, "una sola cella (A1) si formatta come \"A1\", non \"A1:A1\"");

	// Un intervallo di piu' celle -> "A1:B5" (angolo in alto a
	// sinistra : angolo in basso a destra).
	range multiCell(1, 1, 2, 5);
	FormatRangeRef(multiCell, buf, sizeof(buf));
	Check(strcmp(buf, "A1:B5") == 0, "un intervallo di piu' celle si formatta come \"A1:B5\"");

	// Colonne oltre Z (AA, AB, ...) -- stesso algoritmo di ColumnName
	// gia' usato dal footer/toolbar, verificato qui per la prima volta.
	range wideCell(27, 3, 28, 3);
	FormatRangeRef(wideCell, buf, sizeof(buf));
	Check(strcmp(buf, "AA3:AB3") == 0, "le colonne oltre Z si formattano come AA, AB, ... non A27/A28");

	// Round-trip: ParseRangeRef(FormatRangeRef(r)) == r, per un
	// intervallo qualunque -- le due funzioni devono restare inverse
	// l'una dell'altra.
	range original(3, 2, 6, 9);
	FormatRangeRef(original, buf, sizeof(buf));
	range roundTrip;
	bool parsed = ParseRangeRef(buf, roundTrip);
	Check(parsed, "il testo prodotto da FormatRangeRef si rianalizza correttamente con ParseRangeRef");
	if (parsed)
	{
		Check(roundTrip.left == original.left && roundTrip.right == original.right
				&& roundTrip.top == original.top && roundTrip.bottom == original.bottom,
			"il giro FormatRangeRef->ParseRangeRef riproduce esattamente l'intervallo di partenza");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
