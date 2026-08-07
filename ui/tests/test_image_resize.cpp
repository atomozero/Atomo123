/*
	test_image_resize.cpp

	Verifica il ridimensionamento con il mouse di un'immagine incorporata
	(import XLSX, EmbeddedImage.h): estensione naturale dello spostamento
	gia' testato in test_image_drag.cpp, richiesta dall'utente dopo aver
	chiesto "posso ridimensionare le immagini?" e aver ricevuto risposta
	affermativa.

	Stesso schema di test_image_drag.cpp (indice + punto di partenza +
	valore di partenza, armati da MouseDown, applicati da MouseMoved),
	qui su width/height invece di offsetX/offsetY, e con un test in piu'
	per verificare che la maniglia (angolo in basso a destra) abbia la
	precedenza sul corpo dell'immagine quando i due si sovrappongono.
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
		: BWindow(BRect(100, 100, 900, 700), "test-image-resize", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestImageResize");

	CContainer* doc = new CContainer(NULL, NULL);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Due immagini finte (blob PNG minimo, solo la firma -- il
	// ridimensionamento non decodifica mai i byte, stesso principio di
	// test_image_drag.cpp): la prima ancorata a B2, la seconda ancorata
	// altrove e ben separata dalla prima, cosi' da verificare che
	// ridimensionare una non tocchi l'altra.
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
		img.anchor = cell(2, 10); // molto piu' in basso, nessuna sovrapposizione
		img.offsetX = 10; img.offsetY = 5;
		img.width = 60; img.height = 40;
		img.pngData.push_back(0x89);
		images.push_back(img);
	}
	view->SetImages(&images);

	BRect handle0Before = view->ImageResizeHandle(images[0]);
	BRect frame0Before = view->ImageFrame(images[0]);

	// --- Un clic FUORI dalla maniglia e dal corpo dell'immagine non
	// ridimensiona ne' sposta nulla. ---
	BPoint farAway(500, 400);
	Check(!frame0Before.Contains(farAway),
		"il punto di controllo scelto e' davvero fuori dall'immagine");
	view->MouseDown(farAway);
	view->MouseMoved(BPoint(520, 420), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(520, 420));
	Check(images[0].width == 60 && images[0].height == 40,
		"un clic fuori dall'immagine non la ridimensiona (resta alla sua dimensione originale)");

	// --- Un clic sulla maniglia (angolo in basso a destra) avvia il
	// ridimensionamento, non lo spostamento: dopo il trascinamento
	// width/height cambiano ma offsetX/offsetY restano gli stessi. ---
	BPoint onHandle(handle0Before.left + 2, handle0Before.top + 2);
	Check(frame0Before.Contains(onHandle),
		"la maniglia scelta cade davvero dentro il corpo dell'immagine (dove si sovrappongono)");

	view->MouseDown(onHandle);
	view->MouseMoved(onHandle + BPoint(15, 10), B_INSIDE_VIEW, NULL);
	Check(images[0].width == 75 && images[0].height == 50,
		"la larghezza/altezza si aggiorna gia' al primo MouseMoved del trascinamento, non solo al rilascio");
	Check(images[0].offsetX == 10 && images[0].offsetY == 5,
		"trascinare la maniglia cambia solo width/height, non offsetX/offsetY (niente spostamento)");

	view->MouseMoved(onHandle + BPoint(25, 5), B_INSIDE_VIEW, NULL);
	Check(images[0].width == 85 && images[0].height == 45,
		"un secondo MouseMoved ridimensiona rispetto al punto di PARTENZA del trascinamento, non in modo incrementale");

	view->MouseUp(onHandle + BPoint(25, 5));
	Check(images[1].width == 60 && images[1].height == 40
		&& images[1].offsetX == 10 && images[1].offsetY == 5,
		"ridimensionare la prima immagine non tocca la seconda");

	// --- Annulla/Ripristina (bug reale trovato durante l'audit di
	// Annulla: ridimensionare un'immagine non lasciava nessuna traccia
	// nella pila di Annulla, stesso problema dello spostamento gia'
	// coperto in test_image_drag.cpp). Il blocco sopra e' un UNICO
	// trascinamento (un solo MouseDown/MouseUp, i due MouseMoved
	// intermedi non creano istantanee separate): Annulla riporta quindi
	// alla dimensione di PRIMA del MouseDown (60x40), non a quella
	// intermedia (75x50). ---
	Check(view->CanUndo(), "dopo aver ridimensionato un'immagine, Annulla e' disponibile");
	view->Undo();
	Check(images[0].width == 60 && images[0].height == 40,
		"Annulla riporta la prima immagine alla dimensione di PRIMA dell'intero ridimensionamento (60x40), non a una intermedia");
	Check(view->CanRedo(), "dopo Annulla, Ripristina e' disponibile");
	view->Redo();
	Check(images[0].width == 85 && images[0].height == 45,
		"Ripristina riapplica il ridimensionamento appena annullato");

	// --- Il ridimensionamento non puo' andare sotto un minimo: trascinare
	// la maniglia verso l'alto/sinistra oltre il minimo blocca width/
	// height al minimo invece di farle diventare negative o nulle. ---
	BRect handle0Now = view->ImageResizeHandle(images[0]);
	BPoint onHandleNow(handle0Now.left + 2, handle0Now.top + 2);
	view->MouseDown(onHandleNow);
	view->MouseMoved(onHandleNow + BPoint(-500, -500), B_INSIDE_VIEW, NULL);
	Check(images[0].width >= 10 && images[0].height >= 10,
		"il ridimensionamento non scende mai sotto il minimo, anche trascinando molto oltre");
	view->MouseUp(onHandleNow + BPoint(-500, -500));

	// --- Un clic dentro il corpo dell'immagine ma FUORI dalla maniglia
	// continua a spostarla (comportamento gia' testato in
	// test_image_drag.cpp), non a ridimensionarla: verifica solo che i
	// due percorsi restino distinti dopo l'aggiunta della maniglia. ---
	images[0].width = 60; images[0].height = 40;
	images[0].offsetX = 10; images[0].offsetY = 5;
	BRect frame0Reset = view->ImageFrame(images[0]);
	BPoint insideNotHandle(frame0Reset.left + 3, frame0Reset.top + 3);
	Check(!view->ImageResizeHandle(images[0]).Contains(insideNotHandle),
		"il punto di controllo scelto cade dentro l'immagine ma fuori dalla maniglia");
	view->MouseDown(insideNotHandle);
	view->MouseMoved(insideNotHandle + BPoint(7, 3), B_INSIDE_VIEW, NULL);
	view->MouseUp(insideNotHandle + BPoint(7, 3));
	Check(images[0].width == 60 && images[0].height == 40,
		"un clic fuori dalla maniglia (ma dentro l'immagine) sposta, non ridimensiona: width/height invariate");
	Check(images[0].offsetX == 17 && images[0].offsetY == 8,
		"un clic fuori dalla maniglia (ma dentro l'immagine) sposta: offsetX/offsetY cambiano");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
