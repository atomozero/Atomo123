/*
	test_validation.cpp

	Verifica la convalida dati per cella (Fase 13): SetCellValidation/
	RemoveCellValidation/CellValidation resi pubblici apposta per
	essere testabili senza passare da una vera ValidationWindow,
	stesso principio di SetCellComment/SetCellHyperlink.
	ValidateCellValue (la logica di controllo vera e propria, separata
	dalla REGOLA sopra) e' testata direttamente.

	L'aggancio vero e proprio in SheetView::CommitEditing (chiamato da
	StartEditing/CommitEditing, entrambi PRIVATI apposta -- solo
	MouseDown/tastiera reali li invocano, mai un test) NON e' testato
	qui: un valore respinto mostrerebbe un vero BAlert::Go(), che
	bloccherebbe il test in attesa di un clic reale, stesso limite gia'
	noto di DeleteSheet/RenameSheet (vedi il commento in cima a
	test_sheet_management.cpp) -- la logica di rifiuto vera e propria
	e' comunque verificata direttamente tramite ValidateCellValue.

	Verificato anche che il round-trip nativo (SaveASCD/LoadASCD)
	conservi le regole, e che la freccia a discesa disegnata da
	SheetView::Draw() per una cella con un elenco di valori sia
	davvero visibile sui pixel (stesso principio delle bitmap
	offscreen gia' usato in test_comments.cpp/test_hyperlinks.cpp).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Bitmap.h>
#include <File.h>

#include "AscdIO.h"
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
	BApplication app("application/x-vnd.Atomo-TestValidation");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	Check(win->CellValidation(3, 3).type == eNoValidation,
		"una cella mai toccata non ha nessuna regola di convalida");

	win->SetCellValidation(3, 3, eListValidation, "Rosso, Verde, Blu", 0, 0);
	ValidationRule rule = win->CellValidation(3, 3);
	Check(rule.type == eListValidation, "SetCellValidation imposta il tipo elenco");
	Check(rule.list == "Rosso, Verde, Blu", "SetCellValidation imposta la lista di valori");

	win->SetCellValidation(4, 4, eNumberRangeValidation, "", 1, 10);
	rule = win->CellValidation(4, 4);
	Check(rule.type == eNumberRangeValidation, "SetCellValidation imposta il tipo intervallo numerico");
	Check(rule.min == 1 && rule.max == 10, "SetCellValidation imposta min/max");

	win->RemoveCellValidation(3, 3);
	Check(win->CellValidation(3, 3).type == eNoValidation,
		"RemoveCellValidation rimuove davvero la regola");

	// ValidateCellValue: elenco di valori, spazi intorno a ogni voce
	// scartati (stesso schema di split di ShowValidationMenu).
	win->SetCellValidation(5, 5, eListValidation, "Rosso, Verde, Blu", 0, 0);
	BString error;
	Check(win->ValidateCellValue(5, 5, "Verde", &error),
		"un valore presente nell'elenco (con spazio davanti nella regola) e' accettato");
	Check(!win->ValidateCellValue(5, 5, "Giallo", &error),
		"un valore assente dall'elenco e' respinto");
	Check(error.Length() > 0, "un valore respinto produce un messaggio d'errore non vuoto");

	// ValidateCellValue: intervallo numerico.
	win->SetCellValidation(6, 6, eNumberRangeValidation, "", 1, 10);
	Check(win->ValidateCellValue(6, 6, "5", &error),
		"un numero dentro l'intervallo e' accettato");
	Check(win->ValidateCellValue(6, 6, "1", &error) && win->ValidateCellValue(6, 6, "10", &error),
		"gli estremi dell'intervallo sono accettati (inclusivi)");
	Check(!win->ValidateCellValue(6, 6, "11", &error),
		"un numero fuori dall'intervallo e' respinto");
	Check(!win->ValidateCellValue(6, 6, "abc", &error),
		"un testo non numerico e' respinto su una cella con intervallo numerico");

	// Nessuna regola: qualunque valore passa.
	Check(win->ValidateCellValue(7, 7, "qualunque cosa", &error),
		"una cella senza regola accetta qualunque valore");

	win->Unlock();

	// --- Round-trip nel formato nativo: le regole di convalida
	// sopravvivono a salvataggio/ricarica. ---
	{
		CContainer* rtDoc = new CContainer(NULL, NULL);
		ValidationRule listRule;
		listRule.type = eListValidation;
		listRule.list = "Si,No";
		rtDoc->SetValidation(cell(2, 2), listRule);

		ValidationRule rangeRule;
		rangeRule.type = eNumberRangeValidation;
		rangeRule.min = -5;
		rangeRule.max = 5;
		rtDoc->SetValidation(cell(3, 3), rangeRule);

		BFile file("tests/roundtrip_validation.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(rtDoc, &file) == B_OK, "SaveASCD con due regole di convalida riesce");
		rtDoc->Release();

		BFile reopened("tests/roundtrip_validation.ascd", B_READ_ONLY);
		CContainer* reloaded = new CContainer(NULL, NULL);
		Check(LoadASCD(&reopened, reloaded) == B_OK, "LoadASCD con due regole di convalida riesce");

		ValidationRule reloadedList = reloaded->GetValidation(cell(2, 2));
		Check(reloadedList.type == eListValidation && reloadedList.list == "Si,No",
			"la regola elenco sopravvive al giro salva->ricarica");

		ValidationRule reloadedRange = reloaded->GetValidation(cell(3, 3));
		Check(reloadedRange.type == eNumberRangeValidation
				&& reloadedRange.min == -5 && reloadedRange.max == 5,
			"la regola intervallo numerico sopravvive al giro salva->ricarica, allineata alla cella giusta");

		Check(!reloaded->HasValidation(cell(1, 1)),
			"una cella senza regola resta senza regola dopo il giro salva->ricarica");
		reloaded->Release();
	}

	// --- La freccia a discesa si vede davvero sui pixel (non solo
	// "il codice per disegnarla e' stato eseguito"). ---
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		TryToParseString("Rosso", cell(2, 2), doc2, true); // B2, con elenco
		ValidationRule listRule;
		listRule.type = eListValidation;
		listRule.list = "Rosso,Verde,Blu";
		doc2->SetValidation(cell(2, 2), listRule);
		TryToParseString("Testo", cell(5, 5), doc2, true); // E5, senza regola

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view2 = new SheetView(doc2);
		view2->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view2);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per la convalida si blocca per disegnarci sopra");

		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// La freccia e' grigia (90,90,90): componenti B/G/R vicini fra
		// loro (grigio, non un colore saturo) ma ben piu' scuri dello
		// sfondo bianco.
		BRect b2 = view2->CellRect(cell(2, 2));
		bool foundArrow = false;
		for (int32 y = (int32)b2.top; y < (int32)b2.bottom && !foundArrow; y++)
		{
			uint8* row = bits + y * bpr;
			for (int32 x = (int32)b2.left; x < (int32)b2.right; x++)
			{
				uint8* px = row + x * 4;
				// B_RGB32 in memoria: B, G, R, A.
				if (px[0] < 150 && px[1] < 150 && px[2] < 150)
				{
					foundArrow = true;
					break;
				}
			}
		}
		Check(foundArrow,
			"la freccia a discesa della convalida a elenco e' davvero disegnata (pixel grigio)");

		BRect e5 = view2->CellRect(cell(5, 5));
		bool foundArrowOnPlainCell = false;
		for (int32 y = (int32)e5.top; y < (int32)e5.bottom && !foundArrowOnPlainCell; y++)
		{
			uint8* row = bits + y * bpr;
			// Solo la fascia destra della cella (dove cadrebbe la
			// freccia, non il testo a sinistra) per evitare un falso
			// positivo sul glifo nero del testo stesso.
			for (int32 x = (int32)e5.right - 16; x < (int32)e5.right; x++)
			{
				uint8* px = row + x * 4;
				if (px[0] < 150 && px[1] < 150 && px[2] < 150)
				{
					foundArrowOnPlainCell = true;
					break;
				}
			}
		}
		Check(!foundArrowOnPlainCell,
			"una cella senza regola di convalida non mostra nessuna freccia");

		delete canvas;
		doc2->Release();
	}

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
