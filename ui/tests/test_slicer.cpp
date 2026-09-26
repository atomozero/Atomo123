/*
	test_slicer.cpp

	Verifica lo slicer (Tier 4, "Path to full Excel parity"): stesso
	schema di test_chart_drag.cpp/test_chart_resize.cpp/
	test_chart_delete.cpp per trascinamento (solo dalla barra del
	titolo)/ridimensionamento/cancellazione+Annulla, PIU' la parte
	davvero nuova rispetto a un grafico -- un clic su un pulsante deve
	nascondere/mostrare le righe corrispondenti esattamente come il menu
	a tendina dell'AutoFilter (SetColumnValueHidden).
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
#include "Slicer.h"
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
		: BWindow(BRect(100, 100, 900, 700), "test-slicer", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestSlicer");

	CContainer* doc = new CContainer(NULL, NULL);
	// Colonna A: intestazione + tre valori (righe 2-4) -- lo stesso
	// AutoFilter/dati che un vero slicer dovrebbe controllare.
	TryToParseString("Colore", cell(1, 1), doc, true);
	TryToParseString("Rosso", cell(1, 2), doc, true);
	TryToParseString("Verde", cell(1, 3), doc, true);
	TryToParseString("Blu", cell(1, 4), doc, true);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(700, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	view->SetAutoFilter(range(1, 1, 1, 1)); // A1, colonna 1

	std::vector<SlicerObject> slicers;
	{
		SlicerObject obj;
		obj.frame = BRect(100, 100, 250, 220);
		obj.columnIndex = 1;
		obj.title = "Colore";
		slicers.push_back(obj);
	}
	{
		SlicerObject obj;
		obj.frame = BRect(200, 150, 350, 270); // si sovrappone al primo
		obj.columnIndex = 1;
		obj.title = "Colore (copia)";
		slicers.push_back(obj);
	}
	view->SetSlicers(&slicers);

	// --- Un clic FUORI da qualunque slicer non afferra nulla. ---
	BPoint farAway(500, 400);
	Check(!slicers[0].frame.Contains(farAway) && !slicers[1].frame.Contains(farAway),
		"il punto di controllo scelto e' davvero fuori da entrambi gli slicer");
	BRect frame0Before = slicers[0].frame;
	BRect frame1Before = slicers[1].frame;
	view->MouseDown(farAway);
	view->MouseMoved(BPoint(520, 420), B_INSIDE_VIEW, NULL);
	view->MouseUp(BPoint(520, 420));
	Check(slicers[0].frame == frame0Before && slicers[1].frame == frame1Before,
		"un clic fuori da ogni slicer non sposta nessuno dei due");

	// --- Trascinamento SOLO dalla barra del titolo del secondo slicer.
	// x scelto apposta OLTRE il bordo destro del primo slicer (right=250):
	// un punto piu' a sinistra cadrebbe dentro l'area pulsanti del primo
	// slicer nella zona di sovrapposizione, facendo scattare un clic sul
	// pulsante invece del trascinamento (il clic pulsante e' controllato
	// PRIMA del trascinamento in MouseDown). Il corpo sotto la barra del
	// titolo e' tutto pulsanti, non trascina. ---
	BPoint titleBarPoint(slicers[1].frame.left + 90, slicers[1].frame.top + 10);
	view->MouseDown(titleBarPoint);
	view->MouseMoved(titleBarPoint + BPoint(30, 15), B_INSIDE_VIEW, NULL);
	view->MouseUp(titleBarPoint + BPoint(30, 15));
	Check(slicers[0].frame == frame0Before,
		"il primo slicer (sotto) non si muove quando si trascina la barra del titolo del secondo");
	Check(slicers[1].frame == frame1Before.OffsetByCopy(30, 15),
		"il secondo slicer (sopra) si sposta esattamente dello spostamento del mouse (+30,+15)");

	Check(view->CanUndo(), "dopo aver trascinato uno slicer, Annulla e' disponibile");
	view->Undo();
	Check(slicers[1].frame == frame1Before,
		"Annulla riporta il secondo slicer al frame di prima del trascinamento");
	view->Redo();
	Check(slicers[1].frame == frame1Before.OffsetByCopy(30, 15),
		"Ripristina riapplica lo spostamento appena annullato");

	// --- Ridimensionamento dalla maniglia (angolo in basso a destra). ---
	BRect frame1AfterDrag = slicers[1].frame;
	BPoint handlePoint(frame1AfterDrag.right - 3, frame1AfterDrag.bottom - 3);
	view->MouseDown(handlePoint);
	view->MouseMoved(handlePoint + BPoint(20, 10), B_INSIDE_VIEW, NULL);
	view->MouseUp(handlePoint + BPoint(20, 10));
	Check(slicers[1].frame.right == frame1AfterDrag.right + 20
			&& slicers[1].frame.bottom == frame1AfterDrag.bottom + 10
			&& slicers[1].frame.left == frame1AfterDrag.left
			&& slicers[1].frame.top == frame1AfterDrag.top,
		"il ridimensionamento cambia solo right/bottom, l'angolo in alto a sinistra resta fermo");
	view->Undo(); // torna a frame1AfterDrag, cosi' i pulsanti sotto restano a una posizione nota

	// --- Clic su un pulsante: nasconde/mostra esattamente come il menu
	// a tendina dell'AutoFilter (IsColumnValueVisible/SetColumnValueHidden).
	// Usa il PRIMO slicer (non spostato/ridimensionato, posizione nota). ---
	Check(view->IsColumnValueVisible(1, "Verde"),
		"\"Verde\" e' visibile prima di qualunque clic sullo slicer");

	std::vector<BRect> buttonRects;
	std::vector<BString> buttonValues;
	view->SlicerButtonRects(slicers[0], &buttonRects, &buttonValues);
	Check(buttonValues.size() == 3, "lo slicer elenca i tre valori distinti della colonna (Rosso/Verde/Blu)");

	int verdeButtonIndex = -1;
	for (size_t i = 0; i < buttonValues.size(); i++)
		if (buttonValues[i] == "Verde")
			verdeButtonIndex = (int)i;
	Check(verdeButtonIndex >= 0, "\"Verde\" e' uno dei pulsanti dello slicer");

	if (verdeButtonIndex >= 0)
	{
		BPoint verdeButtonCenter = buttonRects[verdeButtonIndex].LeftTop()
			+ BPoint(10, 5);
		view->MouseDown(verdeButtonCenter);
		view->MouseUp(verdeButtonCenter);
		Check(!view->IsColumnValueVisible(1, "Verde"),
			"un clic sul pulsante \"Verde\" lo esclude, esattamente come deselezionarlo nel menu a tendina");
		Check(view->IsRowHidden(3),
			"la riga 3 (\"Verde\") risulta nascosta dopo il clic sul pulsante");

		// Un secondo clic sullo stesso pulsante lo rimostra (interruttore).
		view->MouseDown(verdeButtonCenter);
		view->MouseUp(verdeButtonCenter);
		Check(view->IsColumnValueVisible(1, "Verde"),
			"un secondo clic sullo stesso pulsante rimostra \"Verde\"");
		Check(!view->IsRowHidden(3), "la riga 3 torna visibile dopo il secondo clic");
	}

	// --- Cancellazione + Annulla/Ripristina (Canc sullo slicer
	// selezionato dal clic sul pulsante appena sopra). ---
	Check(view->HasSelectedSlicer(), "lo slicer resta selezionato dopo un clic su un suo pulsante");
	int selectedBefore = view->SelectedSlicerIndex();
	size_t countBefore = slicers.size();
	view->DeleteSelectedSlicer();
	Check(slicers.size() == countBefore - 1, "Elimina slicer rimuove davvero uno slicer dal vettore");
	Check(!view->HasSelectedSlicer(), "nessuno slicer resta selezionato dopo la cancellazione");

	view->Undo();
	Check(slicers.size() == countBefore, "Annulla la cancellazione reinserisce lo slicer");
	Check((int)slicers.size() > selectedBefore
			&& slicers[selectedBefore].columnIndex == 1,
		"lo slicer reinserito e' alla sua posizione originale nel vettore");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
