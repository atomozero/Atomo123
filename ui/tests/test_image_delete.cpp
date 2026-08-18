/*
	test_image_delete.cpp

	Verifica la cancellazione di un'immagine incorporata (Fase 16,
	richiesta esplicita dell'utente dopo il fix del ridisegno mancante
	post-ricalcolo): un clic seleziona l'immagine (fSelectedImageIndex,
	riquadro blu in Draw()), Canc/Backspace la rimuove dal vettore.
	Selezionare una cella (SetSelection/ExtendSelection) deve invece
	deselezionare l'immagine, come in Excel -- Canc dopo un clic su una
	cella deve tornare a cancellare il contenuto della cella, non
	un'immagine ormai deselezionata.

	Stesso schema di test_image_drag.cpp (BScrollView reale, chiamate
	dirette a MouseDown/HandleKey senza un vero ciclo dei messaggi).
*/

#include <cstdio>
#include <vector>

#include <Application.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "EmbeddedImage.h"
#include "SheetView.h"

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

class TestWindow : public BWindow {
public:
	TestWindow()
		: BWindow(BRect(100, 100, 900, 700), "test-image-delete", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestImageDelete");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	std::vector<EmbeddedImage> images;
	{
		EmbeddedImage img;
		img.anchor = cell(2, 2); // B2
		img.offsetX = 10; img.offsetY = 5;
		img.width = 60; img.height = 40;
		img.pngData.push_back(0x89);
		images.push_back(img);
	}
	{
		EmbeddedImage img;
		img.anchor = cell(6, 10); // F10, lontana dalla prima
		img.offsetX = 0; img.offsetY = 0;
		img.width = 50; img.height = 30;
		img.pngData.push_back(0x89);
		images.push_back(img);
	}
	view->SetImages(&images);

	Check(!view->HasSelectedImage(), "nessuna immagine selezionata all'avvio");

	// --- Canc SENZA aver mai cliccato un'immagine cancella la cella
	// selezionata come sempre, non fa nulla alle immagini. ---
	view->HandleKey(B_DELETE, false, false);
	Check(images.size() == 2, "Canc senza selezione d'immagine non tocca il vettore immagini");

	// --- Un clic dentro la prima immagine la seleziona (senza
	// spostarla, MouseUp nello stesso punto). ---
	BRect frame0 = view->ImageFrame(images[0]);
	BPoint insideFirst(frame0.left + 5, frame0.top + 5);
	view->MouseDown(insideFirst);
	view->MouseUp(insideFirst);
	Check(view->HasSelectedImage() && view->SelectedImageIndex() == 0,
		"un clic dentro un'immagine la seleziona (indice 0)");

	// --- Selezionare una cella (non un'immagine) deseleziona
	// l'immagine, come in Excel. ---
	view->SetSelection(cell(1, 1));
	Check(!view->HasSelectedImage(),
		"selezionare una cella deseleziona l'immagine appena selezionata");

	// --- Ricliccare la prima immagine e premere Canc la rimuove dal
	// vettore, senza toccare la seconda. ---
	view->MouseDown(insideFirst);
	view->MouseUp(insideFirst);
	Check(view->HasSelectedImage() && view->SelectedImageIndex() == 0,
		"riclic sulla prima immagine: di nuovo selezionata");
	bool handled = view->HandleKey(B_DELETE, false, false);
	Check(handled, "Canc su un'immagine selezionata e' un tasto gestito");
	Check(images.size() == 1, "Canc rimuove l'immagine selezionata dal vettore");
	Check(images[0].anchor.h == 6 && images[0].anchor.v == 10,
		"la seconda immagine (F10) e' l'unica rimasta dopo la cancellazione della prima");
	Check(!view->HasSelectedImage(),
		"dopo la cancellazione, nessuna immagine resta selezionata");

	// --- Annulla/Ripristina: la cancellazione deve essere annullabile
	// come ogni altra mutazione (stesso principio gia' testato in
	// test_image_drag.cpp per lo spostamento). ---
	Check(view->CanUndo(), "dopo aver cancellato un'immagine, Annulla e' disponibile");
	view->Undo();
	Check(images.size() == 2, "Annulla ripristina l'immagine cancellata nel vettore");
	Check(images[0].anchor.h == 2 && images[0].anchor.v == 2,
		"l'immagine ripristinata torna alla sua posizione originale (indice 0, ancora B2)");
	Check(images[0].offsetX == 10 && images[0].offsetY == 5 && images[0].width == 60,
		"l'immagine ripristinata mantiene scarto/dimensione originali, non solo l'ancora");
	Check(images[0].pngData.size() == 1 && images[0].pngData[0] == 0x89,
		"l'immagine ripristinata mantiene i dati PNG originali");

	Check(view->CanRedo(), "dopo Annulla, Ripristina e' disponibile");
	view->Redo();
	Check(images.size() == 1, "Ripristina rimuove di nuovo l'immagine");
	Check(images[0].anchor.h == 6 && images[0].anchor.v == 10,
		"dopo Ripristina resta solo la seconda immagine (F10)");

	// --- Dopo Ripristina, un ulteriore Annulla deve ancora funzionare
	// (la ri-cancellazione ha ricatturato dati completi, non solo
	// l'indice). ---
	view->Undo();
	Check(images.size() == 2,
		"un secondo Annulla dopo Ripristina funziona ancora (dati completi ricatturati da Redo)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
