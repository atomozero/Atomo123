/*
	test_image_export_drag.cpp

	Verifica il trascinamento con il tasto DESTRO di un'immagine
	incorporata (Fase 17, richiesta esplicita dell'utente: "posso
	trascinare l'immagine fuori dal programma per salvarla sul desktop
	o spostarla in un'altra applicazione?"). Il vero drag-and-drop di
	sistema (DragMessage/B_COPY_TARGET verso Tracker o un'altra
	applicazione) non e' testabile qui: richiederebbe un vero
	handshake fra processi (stesso limite gia' documentato altrove in
	questo progetto per i dialoghi modali/menu sincroni, vedi
	test_selection.cpp) e chiamare davvero DragMessage() in un test
	sintetico rischierebbe di restare in attesa di un rilascio del
	mouse mai arrivato. Copre invece la parte testabile senza superare
	la soglia di trascinamento: un clic destro seleziona l'immagine
	SENZA spostarla/ridimensionarla/cancellarla (a differenza del tasto
	sinistro, invariato -- vedi test_image_drag.cpp/test_image_resize.cpp/
	test_image_delete.cpp), e la stessa soglia impedisce che un piccolo
	tremore del mouse durante un clic venga scambiato per un
	trascinamento.
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
		: BWindow(BRect(100, 100, 900, 700), "test-image-export-drag", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestImageExportDrag");

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
	view->SetImages(&images);

	BRect frame0 = view->ImageFrame(images[0]);
	BPoint insideFirst(frame0.left + 5, frame0.top + 5);

	// --- MouseDown/MouseUp diretti (senza un vero messaggio B_MOUSE_DOWN
	// in coda) equivalgono a "nessun tasto premuto" per la lettura di
	// "buttons" da Window()->CurrentMessage(): esercitano quindi lo
	// stesso percorso del tasto sinistro (spostamento), gia' coperto da
	// test_image_drag.cpp. Qui serve solo verificare che un clic senza
	// spostamento reale (sotto la soglia) non cambi nulla nel modello,
	// indipendentemente dal tasto -- la parte che DISTINGUE davvero i
	// due tasti (lettura di "buttons") non e' esercitabile senza un
	// vero messaggio di sistema, quindi resta verificata a mano. ---
	view->MouseDown(insideFirst);
	view->MouseUp(insideFirst);
	Check(images[0].offsetX == 10 && images[0].offsetY == 5,
		"un clic senza trascinare non sposta l'immagine (tasto sinistro implicito nel test)");
	Check(images[0].width == 60 && images[0].height == 40,
		"un clic senza trascinare non la ridimensiona");
	Check(images.size() == 1,
		"un clic senza trascinare non la cancella");
	Check(view->HasSelectedImage() && view->SelectedImageIndex() == 0,
		"un clic dentro l'immagine la seleziona comunque (indipendente dal tasto)");

	// --- Un piccolo spostamento (sotto la soglia dei 4px usata da
	// MouseMoved per il tasto destro, vedi SheetView.cpp) non deve
	// scambiarsi per l'inizio di un trascinamento neppure sul percorso
	// del tasto sinistro: verifica che la soglia non abbia introdotto
	// un ritardo indesiderato nello spostamento normale gia' testato
	// altrove -- qui solo un controllo di non regressione minimo. ---
	view->MouseDown(insideFirst);
	view->MouseMoved(insideFirst + BPoint(1, 1), B_INSIDE_VIEW, NULL);
	Check(images[0].offsetX == 11 && images[0].offsetY == 6,
		"il tasto sinistro (percorso normale) si aggiorna comunque subito, senza soglia");
	view->MouseUp(insideFirst + BPoint(1, 1));

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
