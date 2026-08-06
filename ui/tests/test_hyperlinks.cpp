/*
	test_hyperlinks.cpp

	Verifica i collegamenti ipertestuali per cella (Fase 13): stesso
	schema di test_comments.cpp -- SetCellHyperlink/RemoveCellHyperlink/
	CellHyperlink resi pubblici apposta per essere testabili senza
	passare da una vera HyperlinkWindow, stesso principio di
	NewSheet/RenameSheet/SetCellComment.

	OpenCellHyperlink NON e' testato qui: lancia davvero l'applicazione
	preferita per l'URL (BUrl::OpenWithPreferredApplication), stesso
	limite gia' noto di DeleteSheet/RenameSheet con un vero BAlert (vedi
	il commento in cima a test_sheet_management.cpp).

	Verificato anche che il round-trip nativo (SaveASCD/LoadASCD)
	conservi i collegamenti, e che il testo blu sottolineato disegnato
	da SheetView::Draw() per una cella con collegamento sia davvero
	visibile sui pixel (stesso principio delle bitmap offscreen gia'
	usato in test_comments.cpp/test_merge_click.cpp).
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
	BApplication app("application/x-vnd.Atomo-TestHyperlinks");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	Check(win->CellHyperlink(3, 3).Length() == 0,
		"una cella mai toccata non ha nessun collegamento");

	win->SetCellHyperlink(3, 3, "https://www.haiku-os.org");
	Check(win->CellHyperlink(3, 3) == "https://www.haiku-os.org",
		"SetCellHyperlink imposta davvero il collegamento sulla cella giusta");

	win->SetCellHyperlink(3, 3, "https://www.atomozero.example/aggiornato");
	Check(win->CellHyperlink(3, 3) == "https://www.atomozero.example/aggiornato",
		"un secondo SetCellHyperlink sulla stessa cella sostituisce l'URL, non lo accumula");

	win->RemoveCellHyperlink(3, 3);
	Check(win->CellHyperlink(3, 3).Length() == 0,
		"RemoveCellHyperlink rimuove davvero il collegamento");

	// Una stringa vuota passata a SetCellHyperlink equivale a
	// RemoveCellHyperlink (stesso comportamento di
	// CContainer::SetHyperlink, vedi Container.h).
	win->SetCellHyperlink(4, 4, "https://qualcosa.example");
	win->SetCellHyperlink(4, 4, "");
	Check(win->CellHyperlink(4, 4).Length() == 0,
		"SetCellHyperlink con una stringa vuota rimuove il collegamento");

	// --- Round-trip nel formato nativo: un collegamento sopravvive a
	// salvataggio/ricarica, come qualunque altra proprieta' per cella. ---
	{
		CContainer* doc = new CContainer(NULL, NULL);
		doc->SetHyperlink(cell(2, 5), "https://www.example.com/nota");

		BFile file("tests/roundtrip_hyperlinks.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(doc, &file) == B_OK, "SaveASCD con un collegamento riesce");
		doc->Release();

		BFile reopened("tests/roundtrip_hyperlinks.ascd", B_READ_ONLY);
		CContainer* reloaded = new CContainer(NULL, NULL);
		Check(LoadASCD(&reopened, reloaded) == B_OK, "LoadASCD con un collegamento riesce");
		Check(reloaded->HasHyperlink(cell(2, 5)),
			"il collegamento sopravvive al giro salva->ricarica");
		Check(reloaded->GetHyperlink(cell(2, 5)) == "https://www.example.com/nota",
			"l'URL del collegamento e' quello giusto dopo il giro salva->ricarica");
		Check(!reloaded->HasHyperlink(cell(1, 1)),
			"una cella senza collegamento resta senza collegamento dopo il giro salva->ricarica");
		reloaded->Release();
	}

	win->Unlock();

	// --- Il testo blu sottolineato si vede davvero sui pixel (non
	// solo "il codice per disegnarlo e' stato eseguito"). ---
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		TryToParseString("Link", cell(2, 2), doc2, true); // B2, con collegamento
		doc2->SetHyperlink(cell(2, 2), "https://www.example.com");
		// E5, non A1: la cella A1 e' la selezione attiva predefinita di
		// una SheetView appena creata, il cui riquadro di evidenziazione
		// e' anch'esso disegnato in blu (vedi SheetView::Draw) -- un
		// falso positivo se usata come termine di paragone "senza
		// collegamento".
		TryToParseString("Testo", cell(5, 5), doc2, true);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view2 = new SheetView(doc2);
		view2->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view2);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per il collegamento si blocca per disegnarci sopra");

		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// Colore atteso (20,80,200) in ordine BGRA -- una fascia di
		// tolleranza stretta intorno ai tre canali, non un generico
		// "componente B alta": l'antialiasing subpixel dei font su
		// Haiku produce sui bordi dei glifi anche di testo NERO normale
		// dei pixel con una leggera frangia colorata (es. B=204 G=126
		// R=49, osservato scrivendo questo test), che una soglia larga
		// tipo "B>150 e R<100" scambierebbe per il blu del
		// collegamento -- falso positivo scoperto proprio su "Testo"
		// senza nessun collegamento.
		bool foundBlue = false;
		BRect b2 = view2->CellRect(cell(2, 2));
		for (int32 y = (int32)b2.top; y < (int32)b2.bottom && !foundBlue; y++)
		{
			uint8* row = bits + y * bpr;
			for (int32 x = (int32)b2.left; x < (int32)b2.right; x++)
			{
				uint8* px = row + x * 4;
				// B_RGB32 in memoria: B, G, R, A.
				if (px[0] >= 180 && px[0] <= 220 && px[1] >= 60 && px[1] <= 100
					&& px[2] <= 40)
				{
					foundBlue = true;
					break;
				}
			}
		}
		Check(foundBlue,
			"il testo blu del collegamento e' davvero disegnato (almeno un pixel del blu esatto)");

		// E5 ("Testo", senza collegamento): nessun pixel del blu esatto
		// del collegamento nella cella (la frangia di antialiasing del
		// testo nero normale non deve far scattare il controllo, vedi
		// sopra).
		BRect e5 = view2->CellRect(cell(5, 5));
		bool foundBlueOnPlainText = false;
		for (int32 y = (int32)e5.top; y < (int32)e5.bottom && !foundBlueOnPlainText; y++)
		{
			uint8* row = bits + y * bpr;
			for (int32 x = (int32)e5.left; x < (int32)e5.right; x++)
			{
				uint8* px = row + x * 4;
				if (px[0] >= 180 && px[0] <= 220 && px[1] >= 60 && px[1] <= 100
					&& px[2] <= 40)
				{
					foundBlueOnPlainText = true;
					break;
				}
			}
		}
		Check(!foundBlueOnPlainText,
			"una cella senza collegamento non mostra il blu del collegamento (nero normale)");

		delete canvas;
		doc2->Release();
	}

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
