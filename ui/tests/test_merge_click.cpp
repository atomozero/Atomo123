/*
	test_merge_click.cpp

	Verifica il comportamento delle celle unite al clic del mouse:
	segnalato dall'utente su un file reale ("perche' se ci clicco
	risulta come se non fossero mai unite") -- due bug distinti nello
	stesso screenshot.

	1. MouseDown su una cella "nascosta" sotto un intervallo unito
	   (non l'angolo in alto a sinistra) selezionava quella cella
	   fisica invece dell'angolo -- la cella nascosta non porta mai
	   contenuto/stile, quindi la barra della formula appariva vuota,
	   come se l'intervallo non fosse mai stato unito.
	2. Il riquadro di selezione disegnato in Draw() restava largo una
	   sola cella anche quando la cella attiva era l'angolo di un
	   intervallo unito, invece di estendersi a tutto l'intervallo --
	   verificato qui leggendo davvero i pixel del bordo blu su una
	   bitmap offscreen (stesso principio gia' in uso in
	   test_autofilter.cpp per lo stesso genere di bug "geometricamente
	   corretto ma mai verificato sui pixel veri").

	Corretti entrambi, un terzo bug e' emerso subito dopo (segnalato
	dall'utente con un nuovo screenshot): spostando la selezione via da
	una cella unita, il vecchio riquadro esteso restava "fantasma" sullo
	schermo -- SetSelection/ExtendSelection invalidavano ancora solo il
	rettangolo di una singola cella (PinnedCellRect grezzo), non
	l'intervallo unito davvero disegnato da Draw() (ActiveCellRect).
	Fix: la stessa ActiveCellRect (ora un metodo pubblico condiviso,
	invece di logica duplicata inline in Draw()) usata anche
	nell'invalidazione. Non testabile con lo stesso principio pixel
	delle bitmap offscreen sopra: qui Draw() viene sempre chiamata a
	mano con un rettangolo esplicito nei test, senza passare mai dal
	meccanismo reale di regione invalidata/ridisegno parziale
	dell'Interface Kit (che esiste solo per una vera finestra sullo
	schermo) -- corretto per costruzione (unione, mai una riduzione,
	dell'area gia' invalidata prima del fix).
*/

#include <cstdio>

#include <Application.h>
#include <Bitmap.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "Range.h"
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
		: BWindow(BRect(100, 100, 900, 700), "test-merge-click", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestMergeClick");

	CContainer* doc = new CContainer(NULL, NULL);
	TryToParseString("XXXXXXXXXXXX", cell(4, 4), doc, true); // D4, come nello screenshot reale
	doc->AddMergedRange(range(4, 4, 6, 4)); // D4:F4

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// --- Un clic sull'angolo D4 seleziona D4 (caso banale, nessuna
	// sorpresa). ---
	BRect rectD4 = view->CellRect(cell(4, 4));
	BPoint insideD4(rectD4.left + 5, rectD4.top + 5);
	view->MouseDown(insideD4);
	Check(view->Selection() == cell(4, 4),
		"un clic sull'angolo D4 dell'intervallo unito seleziona D4");

	// --- Un clic su F4 (dentro l'intervallo unito ma non l'angolo)
	// seleziona comunque D4, non F4: prima del fix selezionava la
	// cella fisica sotto il dito, vuota di contenuto. ---
	BRect rectF4 = view->CellRect(cell(6, 4));
	BPoint insideF4(rectF4.left + 5, rectF4.top + 5);
	view->MouseDown(insideF4);
	Check(view->Selection() == cell(4, 4),
		"un clic su F4 (nascosta sotto l'intervallo unito D4:F4) seleziona D4, non F4");

	// --- Una cella normale fuori dall'intervallo unito continua a
	// selezionare se stessa, nessuna regressione. ---
	BRect rectA1 = view->CellRect(cell(1, 1));
	BPoint insideA1(rectA1.left + 5, rectA1.top + 5);
	view->MouseDown(insideA1);
	Check(view->Selection() == cell(1, 1),
		"un clic su una cella normale (A1, fuori da qualunque intervallo unito) seleziona se stessa");

	win->Unlock();

	// --- Il riquadro di selezione disegnato si estende davvero a
	// tutto l'intervallo unito quando la cella attiva ne e' l'angolo:
	// verificato leggendo i pixel veri del bordo blu (30,100,200) su
	// una bitmap offscreen, non solo la geometria attesa. ---
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		TryToParseString("XXXXXXXXXXXX", cell(4, 4), doc2, true);
		doc2->AddMergedRange(range(4, 4, 6, 4)); // D4:F4

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view2 = new SheetView(doc2);
		view2->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view2);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per il riquadro di selezione si blocca per disegnarci sopra");

		view2->MouseDown(view2->CellRect(cell(4, 4)).LeftTop() + BPoint(5, 5));
		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		BRect fullMerge = view2->CellRect(cell(4, 4)) | view2->CellRect(cell(6, 4));
		BRect anchorOnly = view2->CellRect(cell(4, 4));

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// Punto sul bordo destro DAVVERO disegnato (angolo dell'intero
		// intervallo unito, non solo la cella D4): deve essere blu.
		BPoint edgeOfMerge(fullMerge.right, (fullMerge.top + fullMerge.bottom) / 2);
		uint8* pxMerge = bits + (int32)edgeOfMerge.y * bpr + (int32)edgeOfMerge.x * 4;
		// B_RGB32 in memoria: B, G, R, A (ordine gia' verificato in
		// test_image_alpha.cpp) -- il bordo di selezione e' blu
		// (30,100,200): componente R bassa, B alta.
		Check(pxMerge[0] > 150 && pxMerge[2] < 100,
			"il riquadro di selezione arriva davvero fino al bordo destro dell'intero intervallo unito (pixel blu)");

		// Il bordo destro della SOLA cella D4 (dentro l'intervallo
		// unito, non piu' il confine del riquadro dopo il fix) non
		// deve piu' mostrare il bordo blu -- il riquadro lo ha superato.
		BPoint edgeOfAnchorOnly(anchorOnly.right, (anchorOnly.top + anchorOnly.bottom) / 2);
		uint8* pxAnchor = bits + (int32)edgeOfAnchorOnly.y * bpr + (int32)edgeOfAnchorOnly.x * 4;
		Check(!(pxAnchor[0] > 150 && pxAnchor[2] < 100),
			"il confine della sola cella D4 non mostra piu' il bordo blu (il riquadro l'ha superato)");

		delete canvas;
		doc2->Release();
	}

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
