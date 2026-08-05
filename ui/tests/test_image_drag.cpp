/*
	test_image_drag.cpp

	Verifica lo spostamento con il mouse di un'immagine incorporata
	(import XLSX, EmbeddedImage.h): finora solo importate/disegnate,
	mai interattive -- richiesto dall'utente dopo aver chiesto "posso
	spostare le immagini?" e aver ricevuto risposta negativa.

	Stesso schema del ridimensionamento riga/colonna gia' testato in
	tests/test_resize.cpp (indice + punto di partenza + valore di
	partenza, armati da MouseDown, applicati da MouseMoved), qui
	sull'offset di un'immagine invece che sulla larghezza/altezza di
	una riga/colonna -- chiamate dirette a MouseDown/MouseMoved/MouseUp,
	nessun vero BMessage sintetizzato: lo stesso codice non legge mai
	Window()->CurrentMessage() per lo spostamento di un'immagine (a
	differenza, per esempio, dei modificatori di tasto per Maiusc+clic
	sulla selezione), quindi la chiamata diretta e' gia' sufficiente a
	esercitare il percorso di codice reale.
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
		: BWindow(BRect(100, 100, 900, 700), "test-image-drag", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestImageDrag");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Due immagini finte (blob PNG minimo, solo la firma -- lo
	// spostamento non decodifica mai i byte, stesso principio di
	// test_persistence.cpp): la prima ancorata a B2 con uno scarto/
	// dimensione noti, la seconda ancorata altrove e che si sovrappone
	// in parte alla prima -- serve a verificare che un clic nella zona
	// di sovrapposizione afferri quella disegnata sopra (l'ultima
	// nell'elenco), non la prima.
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
		img.anchor = cell(2, 2); // stessa cella di ancoraggio
		img.offsetX = 40; img.offsetY = 25; // si sovrappone alla prima
		img.width = 60; img.height = 40;
		img.pngData.push_back(0x89);
		images.push_back(img);
	}
	view->SetImages(&images);

	BRect frame0Before = view->ImageFrame(images[0]);
	BRect frame1Before = view->ImageFrame(images[1]);

	// --- Un clic FUORI da qualunque immagine non afferra nulla: il
	// trascinamento successivo deve estendere la selezione come al
	// solito, non spostare un'immagine a caso. ---
	BPoint farAway(500, 400);
	Check(!frame0Before.Contains(farAway) && !frame1Before.Contains(farAway),
		"il punto di controllo scelto e' davvero fuori da entrambe le immagini");
	view->MouseDown(farAway);
	view->MouseMoved(BPoint(520, 420), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(520, 420));
	Check(images[0].offsetX == 10 && images[0].offsetY == 5,
		"un clic fuori da ogni immagine non sposta la prima (resta al suo scarto originale)");
	Check(images[1].offsetX == 40 && images[1].offsetY == 25,
		"un clic fuori da ogni immagine non sposta la seconda (resta al suo scarto originale)");

	// --- Un clic nella zona di sovrapposizione afferra la seconda
	// immagine (disegnata sopra, ultima nell'elenco), non la prima. ---
	BPoint overlapPoint(frame1Before.left + 5, frame1Before.top + 5);
	Check(frame0Before.Contains(overlapPoint) && frame1Before.Contains(overlapPoint),
		"il punto di controllo scelto cade davvero dentro entrambe le immagini sovrapposte");

	view->MouseDown(overlapPoint);
	view->MouseMoved(overlapPoint + BPoint(30, 20), B_INSIDE_VIEW, NULL);
	view->MouseUp(overlapPoint + BPoint(30, 20));

	Check(images[0].offsetX == 10 && images[0].offsetY == 5,
		"la prima immagine (sotto) non si muove quando si trascina quella sopra nella zona di sovrapposizione");
	Check(images[1].offsetX == 70 && images[1].offsetY == 45,
		"la seconda immagine (sopra) si sposta esattamente dello spostamento del mouse (+30,+20)");

	// --- Trascinare per intero (non solo il punto finale) una zona
	// SENZA sovrapposizione: afferra e sposta solo quella immagine,
	// aggiornando la posizione a ogni MouseMoved, non solo all'ultimo. ---
	BRect frame0Now = view->ImageFrame(images[0]);
	BPoint insideFirst(frame0Now.left + 5, frame0Now.top + 5);
	view->MouseDown(insideFirst);
	view->MouseMoved(insideFirst + BPoint(5, 0), B_INSIDE_VIEW, NULL);
	Check(images[0].offsetX == 15 && images[0].offsetY == 5,
		"la posizione si aggiorna gia' al primo MouseMoved del trascinamento, non solo al rilascio");
	view->MouseMoved(insideFirst + BPoint(12, 8), B_INSIDE_VIEW, NULL);
	Check(images[0].offsetX == 22 && images[0].offsetY == 13,
		"un secondo MouseMoved sposta rispetto al punto di PARTENZA del trascinamento, non in modo incrementale");
	view->MouseUp(insideFirst + BPoint(12, 8));
	Check(images[1].offsetX == 70 && images[1].offsetY == 45,
		"trascinare la prima immagine non tocca la seconda");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
