/*
	test_borders.cpp

	Verifica i bordi di cella (Fase 11, MainWindow::ToggleBorder/
	ClearBorders): CellStyle::fTBorderColor/fLBorderColor/
	fBBorderColor/fRBorderColor, mai implementati ne' nel Sum-It
	storico ne' nel motore moderno prima d'ora (vedi ROADMAP.md Fase
	11) -- qui trattati come un byte di SPESSORE per lato (0 = nessun
	bordo, 1/2/3 = sottile/medio/spesso dalla Fase 13 in poi, prima
	solo 0/1). ToggleBorder si applica a tutto SelectionRange()
	invertendo lo stato letto dalla sola cella attiva, stesso
	principio di ToggleBold/ToggleItalic (Fase 7). Richiede una vera
	MainWindow (i metodi sono pubblici apposta per essere testabili,
	stesso principio di CopySelection/PasteSelection).

	Fase 13 aggiunge anche il COLORE del bordo (CellStyle::
	fBorderColor, condiviso da tutti e quattro i lati, predefinito
	nero) e lo SPESSORE regolabile (MainWindow::SetBorderThickness):
	verificati sia sul dato (CellStyle dopo la chiamata) sia sul pixel
	davvero disegnato (bitmap offscreen, stesso principio gia' usato
	in test_comments.cpp/test_hyperlinks.cpp per non dare per scontato
	che "il codice per disegnare e' stato eseguito" equivalga a "si
	vede davvero").

	Include anche il sottolineato (Fase 12, MainWindow::
	ToggleUnderline): CellStyle::fUnderline, un booleano a parte
	(BFont non ha un attributo sottolineato nativo), stesso principio
	di ToggleBorder sopra -- aggiunto qui invece che in un file a
	parte per non duplicare il setup di MainWindow/documento.
*/

#include <cstdio>
#include <utility>
#include <vector>

#include <Application.h>
#include <Bitmap.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
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
	BApplication app("application/x-vnd.Atomo-TestBorders");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("1", cell(1, 1), doc, true); // A1
	TryToParseString("2", cell(2, 1), doc, true); // B1

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 1)); // A1:B1

	// Bordo superiore: nessuna cella ce l'ha all'inizio (attiva = B1),
	// quindi l'intera selezione lo ottiene.
	win->ToggleBorder(0);
	CellStyle cs;
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fTBorderColor != 0, "ToggleBorder(superiore) su A1:B1 lo applica ad A1, non solo all'attiva");
	doc->GetCellStyle(cell(2, 1), cs);
	Check(cs.fTBorderColor != 0, "e anche a B1 (l'attiva)");

	// Un secondo ToggleBorder sullo stesso lato/stessa selezione lo
	// toglie di nuovo da entrambe.
	win->ToggleBorder(0);
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fTBorderColor == 0, "un secondo ToggleBorder(superiore) lo toglie da tutta la selezione");

	// I quattro lati sono indipendenti: bordo sinistro su A1 non tocca
	// gli altri tre lati.
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));
	win->ToggleBorder(1); // sinistro
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fLBorderColor != 0 && cs.fTBorderColor == 0
		&& cs.fBBorderColor == 0 && cs.fRBorderColor == 0,
		"ToggleBorder(sinistro) tocca solo il lato sinistro, gli altri tre restano senza bordo");

	// Bordo inferiore e destro, aggiunti separatamente sulla stessa
	// cella: si accumulano, non si sostituiscono a vicenda.
	win->ToggleBorder(2); // inferiore
	win->ToggleBorder(3); // destro
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fLBorderColor != 0 && cs.fBBorderColor != 0 && cs.fRBorderColor != 0,
		"tre lati diversi aggiunti in sequenza convivono sulla stessa cella");

	// ClearBorders toglie tutti e quattro i lati in un colpo solo.
	win->ClearBorders();
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fTBorderColor == 0 && cs.fLBorderColor == 0
		&& cs.fBBorderColor == 0 && cs.fRBorderColor == 0,
		"ClearBorders toglie tutti e quattro i lati insieme");

	// Spessore del bordo (Fase 13): SetBorderThickness cambia solo i
	// lati GIA' presenti, non ne attiva di nuovi.
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));
	win->ToggleBorder(0); // superiore, nessun altro lato
	win->SetBorderThickness(2); // medio
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fTBorderColor == 2, "SetBorderThickness cambia lo spessore del lato gia' presente");
	Check(cs.fLBorderColor == 0 && cs.fBBorderColor == 0 && cs.fRBorderColor == 0,
		"SetBorderThickness non attiva un lato che non c'era");

	win->SetBorderThickness(3); // spesso
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fTBorderColor == 3, "un secondo SetBorderThickness cambia di nuovo lo spessore");

	win->ClearBorders();

	// Colore del bordo (Fase 13): CellStyle::fBorderColor, condiviso
	// da tutti e quattro i lati -- si applica anche a una cella senza
	// nessun bordo attivo (il colore resta impostato, pronto per
	// quando un lato verra' attivato in seguito).
	rgb_color red = { 220, 40, 40, 255 };
	win->SetBorderColor(red);
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fBorderColor.red == 220 && cs.fBorderColor.green == 40 && cs.fBorderColor.blue == 40,
		"SetBorderColor imposta CellStyle::fBorderColor sulla selezione");

	// Sottolineato (Fase 12): stesso principio di ToggleBorder sopra,
	// ma un booleano semplice (nessun "lato").
	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(2, 1)); // A1:B1
	win->ToggleUnderline();
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fUnderline, "ToggleUnderline su A1:B1 lo applica ad A1, non solo all'attiva");
	doc->GetCellStyle(cell(2, 1), cs);
	Check(cs.fUnderline, "e anche a B1 (l'attiva)");

	win->ToggleUnderline();
	doc->GetCellStyle(cell(1, 1), cs);
	Check(!cs.fUnderline, "un secondo ToggleUnderline lo toglie da tutta la selezione");

	// A capo automatico (Fase 12): stesso principio di ToggleUnderline
	// sopra, ma con l'effetto collaterale di far crescere l'altezza
	// della riga se il testo non entra piu' su una riga sola alla
	// larghezza di colonna corrente.
	{
		std::vector<std::pair<int, float> > narrowCol;
		narrowCol.push_back(std::make_pair(1, 40.0f)); // colonna 1 stretta
		view->SetColumnWidths(narrowCol);

		TryToParseString("Testo lungo che sicuramente non entra su una riga sola",
			cell(1, 3), doc, true); // A3

		view->SetSelection(cell(1, 3));
		view->ExtendSelection(cell(1, 3));
		win->ToggleWrapText();
		doc->GetCellStyle(cell(1, 3), cs);
		Check(cs.fWrapText, "ToggleWrapText imposta CellStyle::fWrapText su A3");

		std::vector<std::pair<int, float> > heights = view->CustomRowHeights();
		bool row3Grew = false;
		for (size_t i = 0; i < heights.size(); i++)
			if (heights[i].first == 3 && heights[i].second > 20.0f)
				row3Grew = true;
		Check(row3Grew,
			"l'altezza della riga 3 cresce oltre il predefinito per contenere il testo a capo");
	}

	// Celle unite (Fase 12): MergeCells/UnmergeCells (MainWindow),
	// GetMergedRange (CContainer).
	{
		view->SetSelection(cell(2, 5));
		view->ExtendSelection(cell(4, 5)); // B5:D5

		win->MergeCells();
		Check(doc->GetMergedRanges().size() == 1, "MergeCells su B5:D5 crea un intervallo unito");

		range found;
		bool inMiddle = doc->GetMergedRange(cell(3, 5), &found); // C5, in mezzo all'intervallo
		Check(inMiddle && found.left == 2 && found.top == 5 && found.right == 4 && found.bottom == 5,
			"GetMergedRange riconosce una cella in mezzo all'intervallo e restituisce l'intervallo intero");

		bool outside = doc->GetMergedRange(cell(5, 5), &found); // E5, fuori dall'intervallo
		Check(!outside, "una cella fuori dall'intervallo unito non risulta unita");

		// Selezionare una sola cella non fa nulla (niente da unire).
		view->SetSelection(cell(6, 6));
		view->ExtendSelection(cell(6, 6));
		win->MergeCells();
		Check(doc->GetMergedRanges().size() == 1,
			"MergeCells su una sola cella non crea un secondo intervallo");

		// Unire di nuovo B5:D5 (gia' unita) non duplica l'intervallo.
		view->SetSelection(cell(2, 5));
		view->ExtendSelection(cell(4, 5));
		win->MergeCells();
		Check(doc->GetMergedRanges().size() == 1,
			"unire di nuovo lo stesso intervallo non lo duplica");

		win->UnmergeCells();
		Check(doc->GetMergedRanges().size() == 0, "UnmergeCells toglie l'intervallo unito");
	}

	// A capo automatico dentro una cella unita su piu' colonne (Fase
	// 5/12): il calcolo dell'a capo (RecalculateWrappedRowHeights)
	// deve usare la larghezza dell'INTERO intervallo unito, non solo
	// la prima colonna -- bug reale scoperto confrontando con Excel
	// vero una fattura reale: una descrizione unita su sei colonne
	// (comune per un'intestazione descrittiva in una colonna A
	// stretta) andava a capo su molte piu' righe del necessario
	// contando solo quella prima colonna, gonfiando l'altezza di riga
	// ben oltre quella che mostra Excel (dove il testo entra su una
	// riga sola nella larghezza combinata).
	{
		std::vector<std::pair<int, float> > narrowCols;
		narrowCols.push_back(std::make_pair(7, 40.0f)); // colonna G stretta
		narrowCols.push_back(std::make_pair(8, 40.0f)); // colonna H stretta
		narrowCols.push_back(std::make_pair(9, 40.0f)); // colonna I stretta
		view->SetColumnWidths(narrowCols);

		TryToParseString("Voce di test", cell(7, 10), doc, true); // G10

		view->SetSelection(cell(7, 10));
		view->ExtendSelection(cell(9, 10)); // G10:I10
		win->MergeCells();

		view->SetSelection(cell(7, 10));
		view->ExtendSelection(cell(7, 10));
		win->ToggleWrapText();
		doc->GetCellStyle(cell(7, 10), cs);
		Check(cs.fWrapText, "ToggleWrapText imposta CellStyle::fWrapText su G10 (la cella in alto a sinistra dell'intervallo unito)");

		std::vector<std::pair<int, float> > heights = view->CustomRowHeights();
		float row10Height = 20.0f;
		for (size_t i = 0; i < heights.size(); i++)
			if (heights[i].first == 10)
				row10Height = heights[i].second;
		Check(row10Height <= 40.0f,
			"riga 10 (testo a capo dentro una cella unita su tre colonne strette) resta bassa (al piu' due righe), non gonfiata contando solo la larghezza della prima colonna");
	}

	// --- BorderWindow (Fase 13): HandleBorderFormatRequest imposta
	// lati/spessore/colore insieme, in un solo passo di Annulla -- a
	// differenza di ToggleBorder/SetBorderThickness/SetBorderColor
	// sopra (un lato/aspetto alla volta). ---
	{
		view->SetSelection(cell(9, 12));
		view->ExtendSelection(cell(9, 12)); // I12, mai toccata finora

		rgb_color green = { 40, 160, 40, 255 };
		win->HandleBorderFormatRequest(true, false, true, false, 3, green);

		CellStyle cs;
		doc->GetCellStyle(cell(9, 12), cs);
		Check(cs.fTBorderColor == 3 && cs.fBBorderColor == 3,
			"HandleBorderFormatRequest attiva superiore/inferiore allo spessore scelto (3)");
		Check(cs.fLBorderColor == 0 && cs.fRBorderColor == 0,
			"HandleBorderFormatRequest non tocca i lati non selezionati (sinistro/destro restano a 0)");
		Check(cs.fBorderColor.green == 160 && cs.fBorderColor.red == 40,
			"HandleBorderFormatRequest imposta anche il colore condiviso dei lati");

		// Un solo Annulla ripristina tutti e quattro i lati insieme (un
		// solo passo, non quattro): la cella torna com'era prima, senza
		// bordi.
		view->Undo();
		doc->GetCellStyle(cell(9, 12), cs);
		Check(cs.fTBorderColor == 0 && cs.fLBorderColor == 0
				&& cs.fBBorderColor == 0 && cs.fRBorderColor == 0,
			"Annulla dopo HandleBorderFormatRequest toglie tutti e quattro i lati in un solo passo");

		// Una seconda chiamata SOSTITUISCE lo stato dei lati invece di
		// alternarlo (a differenza di ToggleBorder): partendo da
		// superiore/inferiore gia' attivi, scegliere solo sinistro li
		// spegne entrambi e accende solo quello.
		win->HandleBorderFormatRequest(true, false, true, false, 1, green);
		win->HandleBorderFormatRequest(false, true, false, false, 1, green);
		doc->GetCellStyle(cell(9, 12), cs);
		Check(cs.fLBorderColor != 0 && cs.fTBorderColor == 0 && cs.fBBorderColor == 0,
			"una seconda HandleBorderFormatRequest sostituisce lo stato dei lati, non lo alterna");
	}

	win->Unlock();

	// --- Il bordo rosso spesso si vede davvero sui pixel (non solo
	// "il codice per disegnarlo e' stato eseguito"). ---
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		CellStyle cs2;
		doc2->GetCellStyle(cell(3, 3), cs2); // C3
		cs2.fTBorderColor = 3; // spesso
		rgb_color borderRed = { 220, 40, 40, 255 };
		cs2.fBorderColor = borderRed;
		doc2->SetCellStyle(cell(3, 3), cs2);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view2 = new SheetView(doc2);
		view2->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view2);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per il bordo si blocca per disegnarci sopra");

		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// Scandisce una fascia orizzontale di qualche pixel intorno al
		// bordo superiore atteso, non un unico pixel esatto: uno
		// spessore di penna maggiore di 1 viene centrato sulla
		// coordinata della linea (meta' sopra, meta' sotto), la
		// posizione esatta non e' un dettaglio da fissare qui.
		BRect c3 = view2->CellRect(cell(3, 3));
		bool foundRed = false;
		for (int32 y = (int32)c3.top - 2; y <= (int32)c3.top + 2 && !foundRed; y++)
		{
			uint8* row = bits + y * bpr;
			for (int32 x = (int32)c3.left + 2; x < (int32)c3.right - 2; x++)
			{
				uint8* px = row + x * 4;
				// B_RGB32 in memoria: B, G, R, A.
				if (px[2] > 180 && px[0] < 100 && px[1] < 100)
				{
					foundRed = true;
					break;
				}
			}
		}
		Check(foundRed,
			"il bordo superiore rosso spesso e' davvero disegnato (pixel rosso lungo il bordo)");

		delete canvas;
		doc2->Release();
	}

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
