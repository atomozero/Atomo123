/*
	test_comments.cpp

	Verifica i commenti/note per cella (Fase 13): SetCellComment/
	RemoveCellComment/CellComment sono pubblici apposta per essere
	testabili senza passare da una vera CommentWindow (stesso principio
	di NewSheet/RenameSheet in test_sheet_management.cpp) -- richiede
	una vera MainWindow.

	Verificato anche che il round-trip nativo (SaveASCD/LoadASCD)
	conservi i commenti, e che il riquadro rosso disegnato da
	SheetView::Draw() per una cella commentata sia davvero visibile sui
	pixel (stesso principio delle bitmap offscreen già usato in
	test_autofilter.cpp/test_merge_click.cpp per lo stesso genere di
	bug "geometricamente corretto ma mai verificato sui pixel veri").
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Bitmap.h>
#include <File.h>

#include "AscdIO.h"
#include "Cell.h"
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
	BApplication app("application/x-vnd.Atomo-TestComments");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	Check(win->CellComment(3, 3).Length() == 0,
		"una cella mai toccata non ha nessun commento");

	win->SetCellComment(3, 3, "Verificare questo totale con l'ufficio contabilità");
	Check(win->CellComment(3, 3) == "Verificare questo totale con l'ufficio contabilità",
		"SetCellComment imposta davvero il commento sulla cella giusta");

	win->SetCellComment(3, 3, "Testo aggiornato");
	Check(win->CellComment(3, 3) == "Testo aggiornato",
		"un secondo SetCellComment sulla stessa cella sostituisce il testo, non lo accumula");

	win->RemoveCellComment(3, 3);
	Check(win->CellComment(3, 3).Length() == 0,
		"RemoveCellComment rimuove davvero il commento");

	// Una stringa vuota passata a SetCellComment equivale a
	// RemoveCellComment (stesso comportamento di
	// CContainer::SetComment, vedi Container.h).
	win->SetCellComment(4, 4, "qualcosa");
	win->SetCellComment(4, 4, "");
	Check(win->CellComment(4, 4).Length() == 0,
		"SetCellComment con una stringa vuota rimuove il commento");

	// --- Round-trip nel formato nativo: un commento sopravvive a
	// salvataggio/ricarica, come qualunque altra proprietà per cella. ---
	{
		CContainer* doc = new CContainer(NULL, NULL);
		doc->SetComment(cell(2, 5), "Nota di prova");

		BFile file("tests/roundtrip_comments.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(doc, &file) == B_OK, "SaveASCD con un commento riesce");
		doc->Release();

		BFile reopened("tests/roundtrip_comments.ascd", B_READ_ONLY);
		CContainer* reloaded = new CContainer(NULL, NULL);
		Check(LoadASCD(&reopened, reloaded) == B_OK, "LoadASCD con un commento riesce");
		Check(reloaded->HasComment(cell(2, 5)),
			"il commento sopravvive al giro salva->ricarica");
		Check(reloaded->GetComment(cell(2, 5)) == "Nota di prova",
			"il testo del commento e' quello giusto dopo il giro salva->ricarica");
		Check(!reloaded->HasComment(cell(1, 1)),
			"una cella senza commento resta senza commento dopo il giro salva->ricarica");
		reloaded->Release();
	}

	win->Unlock();

	// --- Il riquadro rosso di indicazione si vede davvero sui pixel
	// (non solo "il codice per disegnarlo e' stato eseguito"). ---
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		doc2->SetComment(cell(2, 2), "Commento su B2");

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view2 = new SheetView(doc2);
		view2->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view2);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per il commento si blocca per disegnarci sopra");

		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		BRect b2 = view2->CellRect(cell(2, 2));
		// Punto dentro il triangolo (angolo in alto a destra della
		// cella, vedi SheetView::Draw): a meta' strada fra i due lati
		// corti, un paio di pixel dentro dal bordo destro/superiore.
		BPoint inTriangle(b2.right - 2, b2.top + 3);
		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();
		uint8* px = bits + (int32)inTriangle.y * bpr + (int32)inTriangle.x * 4;
		// B_RGB32 in memoria: B, G, R, A (ordine gia' verificato in
		// test_image_alpha.cpp) -- il triangolo e' rosso (200,40,40):
		// componente R alta, B bassa.
		Check(px[2] > 150 && px[0] < 100,
			"il triangolo rosso del commento e' davvero disegnato (pixel rosso, non bianco)");

		// Una cella SENZA commento non mostra nessun triangolo: il
		// punto equivalente su A1 resta bianco.
		BRect a1 = view2->CellRect(cell(1, 1));
		BPoint noTriangle(a1.right - 2, a1.top + 3);
		uint8* px2 = bits + (int32)noTriangle.y * bpr + (int32)noTriangle.x * 4;
		Check(px2[2] > 200 && px2[0] > 200,
			"una cella senza commento non mostra nessun triangolo (pixel bianco)");

		delete canvas;
		doc2->Release();
	}

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
