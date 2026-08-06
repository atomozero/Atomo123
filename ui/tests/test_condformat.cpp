/*
	test_condformat.cpp

	Verifica la formattazione condizionale VIVA per cella (Fase 13):
	MainWindow::ApplyConditionalFormatToSelection/
	RemoveAllConditionalFormatRules resi pubblici apposta per essere
	testabili senza passare da una vera ConditionalFormatWindow, stesso
	principio di SetCellValidation/RemoveCellValidation.

	La logica di valutazione vera e propria (CContainer::
	EvaluateConditionalFormatting, quali celle corrispondono a quali
	regole) e' gia' testata a fondo in ui/tests/test_ascd_io.cpp
	(compresa la prova del "viva": una rivalutazione dopo aver
	cambiato un valore riflette il cambiamento da sola) -- qui invece
	si verifica che ApplyConditionalFormatToSelection costruisca
	davvero una regola con l'intervallo della selezione corrente, e
	che il colore che ne risulta sia DAVVERO disegnato sui pixel da
	SheetView::Draw (stesso principio delle bitmap offscreen gia' usato
	in test_comments.cpp/test_borders.cpp).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Bitmap.h>

#include "Cell.h"
#include "Value.h"
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
	BApplication app("application/x-vnd.Atomo-TestCondFormat");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	Check(doc->GetConditionalFormatRules().empty(),
		"un documento appena creato non ha nessuna regola di formattazione condizionale");

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 3)); // A1:A3

	rgb_color red = { 255, 199, 206, 255 };
	win->ApplyConditionalFormatToSelection(0 /* eCondCellIsEqual */, "Mancante", red);

	const std::vector<ConditionalFormatRule>& rules = doc->GetConditionalFormatRules();
	Check(rules.size() == 1, "ApplyConditionalFormatToSelection aggiunge una regola");
	if (rules.size() == 1)
	{
		Check(rules[0].type == eCondCellIsEqual, "la regola ha il tipo scelto (uguale a un letterale)");
		Check(rules[0].compareValue == "Mancante", "la regola ha il valore di confronto scelto");
		Check(rules[0].bgColor.red == 255 && rules[0].bgColor.green == 199
				&& rules[0].bgColor.blue == 206,
			"la regola ha il colore scelto");
		Check(rules[0].ranges.size() == 1 && rules[0].ranges[0].left == 1
				&& rules[0].ranges[0].top == 1 && rules[0].ranges[0].right == 1
				&& rules[0].ranges[0].bottom == 3,
			"la regola si applica esattamente alla selezione corrente (A1:A3), non a tutto il foglio");
	}

	// Una seconda regola su un'altra selezione si AGGIUNGE, non
	// sostituisce la prima -- un foglio reale ha in genere piu' di una
	// regola di formattazione condizionale contemporaneamente.
	view->SetSelection(cell(2, 1));
	view->ExtendSelection(cell(2, 3)); // B1:B3
	rgb_color yellow = { 255, 235, 156, 255 };
	win->ApplyConditionalFormatToSelection(1 /* eCondDuplicateValues */, "", yellow);
	Check(doc->GetConditionalFormatRules().size() == 2,
		"una seconda regola su un'altra selezione si aggiunge, non sostituisce la prima");

	win->RemoveAllConditionalFormatRules();
	Check(doc->GetConditionalFormatRules().empty(),
		"RemoveAllConditionalFormatRules toglie davvero tutte le regole insieme");

	win->Unlock();

	// --- Il colore si vede davvero sui pixel (non solo "il codice per
	// disegnarlo e' stato eseguito"), e si ricalcola da solo -- non
	// scrive mai in CellStyle. ---
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		TryToParseString("Mancante", cell(1, 1), doc2, true); // A1, corrisponde
		TryToParseString("OK", cell(1, 2), doc2, true);       // A2, non corrisponde

		ConditionalFormatRule rule;
		rule.type = eCondCellIsEqual;
		rule.compareValue = "Mancante";
		rule.bgColor = red;
		rule.ranges.push_back(range(1, 1, 1, 2)); // A1:A2
		doc2->AddConditionalFormatRule(rule);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view2 = new SheetView(doc2);
		view2->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view2);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per la formattazione condizionale si blocca per disegnarci sopra");

		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// B_RGB32 in memoria: B, G, R, A -- il colore atteso e'
		// (255,199,206) in R,G,B, quindi (206,199,255) in ordine BGRA.
		BRect a1 = view2->CellRect(cell(1, 1));
		uint8* pxA1 = bits + (int32)(a1.top + 3) * bpr + (int32)(a1.left + 3) * 4;
		Check(pxA1[0] > 190 && pxA1[1] > 180 && pxA1[2] > 240,
			"A1 (\"Mancante\", corrisponde alla regola) ha davvero lo sfondo colorato sui pixel");

		BRect a2 = view2->CellRect(cell(1, 2));
		uint8* pxA2 = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.left + 3) * 4;
		Check(pxA2[0] > 250 && pxA2[1] > 250 && pxA2[2] > 250,
			"A2 (\"OK\", non corrisponde) resta bianca");

		// Cambia il valore di A2 DOPO il primo disegno, senza toccare
		// ne' la regola ne' CellStyle -- un secondo Draw() deve
		// riflettere il nuovo valore da solo.
		TryToParseString("Mancante", cell(1, 2), doc2, true);
		canvas->Lock();
		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		uint8* pxA2After = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.left + 3) * 4;
		Check(pxA2After[0] > 190 && pxA2After[1] > 180 && pxA2After[2] > 240,
			"dopo aver cambiato A2 in \"Mancante\", un nuovo Draw() la colora da solo (e' questo che la rende viva)");

		delete canvas;
		doc2->Release();
	}

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
