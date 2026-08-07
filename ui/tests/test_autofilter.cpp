/*
	test_autofilter.cpp

	Verifica l'AutoFilter (import XLSX <autoFilter ref="...">, Fase 5):
	terza delle tre migliorie di fedelta' XLSX nate confrontando un
	file reale con Excel (vedi ROADMAP.md, insieme alla griglia
	per-foglio e al colore della linguetta). A differenza di quelle
	due, qui c'e' anche una logica vera e propria da verificare (non
	solo lettura/disegno): righe nascoste, valori distinti per
	colonna, e come nascondere/mostrare un valore ricalcola tutte le
	righe filtrate insieme (AND fra colonne, come Excel vero).

	Non copre il menu a tendina vero e proprio (SheetView::
	ShowAutoFilterMenu, un vero BPopUpMenu::Go() sincrono): richiede un
	clic reale dell'utente, stesso limite gia' documentato altrove in
	questo progetto per i dialoghi modali (vedi test_selection.cpp).
	La logica che il menu richiama -- UniqueColumnValues/
	IsColumnValueVisible/SetColumnValueHidden/ClearColumnFilters -- e'
	invece testabile ed e' quella verificata qui, chiamata
	direttamente come farebbe il menu dopo un clic.
*/

#include <cstdio>
#include <cstring>
#include <vector>

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
		: BWindow(BRect(100, 100, 900, 700), "test-autofilter", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestAutoFilter");

	CContainer* doc = new CContainer(NULL, NULL);

	// Intestazione in riga 5 (A5:B5), dati nelle righe 6-10: colonna A
	// una categoria ripetuta (Nord/Sud/Nord/Est/Sud), colonna B un
	// numero qualunque -- stessa forma di una tabella reale con
	// AutoFilter (intestazione + dati sotto), non un caso sintetico
	// slegato dall'uso vero.
	TryToParseString("Zona", cell(1, 5), doc, true);
	TryToParseString("Valore", cell(2, 5), doc, true);
	TryToParseString("Nord", cell(1, 6), doc, true);
	TryToParseString("10", cell(2, 6), doc, true);
	TryToParseString("Sud", cell(1, 7), doc, true);
	TryToParseString("20", cell(2, 7), doc, true);
	TryToParseString("Nord", cell(1, 8), doc, true);
	TryToParseString("30", cell(2, 8), doc, true);
	TryToParseString("Est", cell(1, 9), doc, true);
	TryToParseString("40", cell(2, 9), doc, true);
	TryToParseString("Sud", cell(1, 10), doc, true);
	TryToParseString("50", cell(2, 10), doc, true);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(600, 500);
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	Check(!view->HasAutoFilter(), "un foglio nuovo non ha nessun AutoFilter attivo");

	// range(left, top, right, bottom): intestazione riga 5, colonne A-B
	// (top == bottom, l'intestazione e' sempre una riga sola -- vedi
	// SheetView.h).
	view->SetAutoFilter(range(1, 5, 2, 5));
	Check(view->HasAutoFilter(), "SetAutoFilter attiva l'AutoFilter");
	Check(view->AutoFilterRange().top == 5 && view->AutoFilterRange().left == 1
			&& view->AutoFilterRange().right == 2,
		"AutoFilterRange() riporta l'intervallo appena impostato");

	// --- Valori distinti, in ordine di comparsa nelle righe ---
	std::vector<BString> values = view->UniqueColumnValues(1);
	Check(values.size() == 3, "tre valori distinti nella colonna Zona (Nord/Sud/Est)");
	if (values.size() == 3)
	{
		Check(values[0] == "Nord" && values[1] == "Sud" && values[2] == "Est",
			"i valori distinti sono in ordine di prima comparsa (Nord, Sud, Est)");
	}

	Check(view->IsColumnValueVisible(1, "Nord"), "\"Nord\" e' visibile prima di ogni filtro");

	// --- Nascondere un valore nasconde SOLO le righe con quel valore ---
	view->SetColumnValueHidden(1, "Nord", true);
	Check(!view->IsColumnValueVisible(1, "Nord"), "\"Nord\" risulta non visibile dopo averlo nascosto");
	Check(view->IsRowHidden(6) && view->IsRowHidden(8),
		"le due righe con \"Nord\" (6 e 8) sono nascoste");
	Check(!view->IsRowHidden(7) && !view->IsRowHidden(9) && !view->IsRowHidden(10),
		"le righe con altri valori (Sud/Est) restano visibili");
	Check(!view->IsRowHidden(5), "la riga di intestazione stessa non viene mai nascosta");

	// --- La geometria collassa davvero: le righe nascoste occupano
	// zero pixel, la riga successiva visibile sale a occupare il loro
	// posto (esattamente come Excel), non solo lo stato interno ---
	BRect row5 = view->CellRect(cell(1, 5));
	BRect row7 = view->CellRect(cell(1, 7));
	// Celle adiacenti condividono lo stesso pixel di confine in questo
	// motore (right di una == left della prossima, senza scarto di 1 --
	// vedi lo stesso principio gia' verificato per le colonne in
	// tests/test_resize.cpp): row7.top == row5.bottom, non +1.
	Check(row7.top == row5.bottom,
		"la riga 6 (nascosta) non occupa spazio: la riga 7 sale subito sotto la 5");

	// --- Un secondo criterio, su un'altra colonna, si combina in AND
	// con il primo (come Excel vero): nascondere "20" in Valore deve
	// nascondere la riga 7 (Sud/20) SENZA far riapparire la 6/8 (Nord,
	// gia' escluse dal primo criterio) ---
	view->SetColumnValueHidden(2, "20", true);
	Check(view->IsRowHidden(7), "la riga 7 (Valore=20) e' ora nascosta dal secondo criterio");
	Check(view->IsRowHidden(6) && view->IsRowHidden(8),
		"le righe gia' nascoste dal primo criterio (Nord) restano nascoste (AND fra colonne)");
	Check(!view->IsRowHidden(9) && !view->IsRowHidden(10),
		"le righe che non corrispondono a NESSUN criterio restano visibili");

	// --- Rimuovere il primo criterio (mostra di nuovo "Nord") non
	// tocca il secondo, ancora attivo ---
	view->SetColumnValueHidden(1, "Nord", false);
	Check(view->IsColumnValueVisible(1, "Nord"), "\"Nord\" torna visibile dopo averlo ri-spuntato");
	Check(!view->IsRowHidden(6), "la riga 6 (Nord, Valore=10) torna visibile");
	Check(view->IsRowHidden(8) == false, "la riga 8 (Nord, Valore=30) torna visibile");
	Check(view->IsRowHidden(7), "la riga 7 (Sud, Valore=20) resta nascosta dal secondo criterio, ancora attivo");

	// --- "Mostra tutto" azzera ogni criterio su ogni colonna ---
	view->ClearColumnFilters();
	Check(!view->IsRowHidden(6) && !view->IsRowHidden(7) && !view->IsRowHidden(8)
			&& !view->IsRowHidden(9) && !view->IsRowHidden(10),
		"ClearColumnFilters() mostra di nuovo tutte le righe");

	// --- ClearAutoFilter spegne il filtro stesso (non solo i criteri) ---
	view->ClearAutoFilter();
	Check(!view->HasAutoFilter(), "ClearAutoFilter() disattiva l'AutoFilter");

	// --- SetHiddenRows/HiddenRows: l'API usata per la persistenza
	// (AscdIO), indipendente dai criteri per valore sopra -- imposta
	// direttamente l'elenco delle righe nascoste, come dopo aver
	// riletto un file salvato. ---
	std::vector<int> toHide;
	toHide.push_back(6);
	toHide.push_back(8);
	view->SetHiddenRows(toHide);
	Check(view->IsRowHidden(6) && view->IsRowHidden(8) && !view->IsRowHidden(7),
		"SetHiddenRows imposta esattamente le righe indicate, nessuna in piu' o in meno");
	std::vector<int> got = view->HiddenRows();
	Check(got.size() == 2 && got[0] == 6 && got[1] == 8,
		"HiddenRows() riporta lo stesso elenco appena impostato, in ordine crescente");

	// --- AutoFilterArrowRect: la freccia a discesa disegnata
	// nell'intestazione (usata sia da Draw() che da MouseDown per
	// riconoscere il clic, vedi SheetView.cpp) cade dentro il
	// rettangolo della cella di intestazione, non fuori. ---
	view->SetHiddenRows(std::vector<int>()); // pulisce, non serve piu' per questo controllo
	view->SetAutoFilter(range(1, 5, 2, 5));
	BRect headerCell = view->CellRect(cell(1, 5));
	BRect arrow = view->AutoFilterArrowRect(1);
	Check(headerCell.Contains(arrow.LeftTop()) && headerCell.Contains(arrow.RightBottom()),
		"la freccia a discesa cade dentro il rettangolo della cella di intestazione");

	win->Unlock();

	win->Lock();
	win->Quit();

	// --- Bug reale scoperto scrivendo l'AutoFilter: la freccia a
	// discesa non compariva MAI sullo schermo (ne' in una vera finestra
	// ne' su una bitmap offscreen), pur essendo il codice geometricamente
	// corretto e sicuramente raggiunto (verificato passo passo con un
	// programma di prova a parte). Causa: il ciclo che disegna il testo
	// delle celle (poco sopra in SheetView::DrawCellBand) restringe il
	// ritaglio dello schermo (ConstrainClippingRegion) al rettangolo
	// della singola cella per ogni cella non vuota, ma non lo
	// ripristinava MAI ne' fra un'iterazione e l'altra ne' all'uscita
	// dal ciclo -- il ritaglio dell'ULTIMA cella disegnata restava
	// quindi attivo per tutto cio' che veniva disegnato dopo in quella
	// stessa chiamata, la freccia compresa: mai scoperto prima perche'
	// la freccia era il primo codice a disegnare qualcosa dopo quel
	// ciclo. Verificato qui leggendo davvero i pixel attorno al centro
	// del triangolo (non solo la sua geometria, gia' controllata sopra),
	// su una bitmap offscreen con almeno una cella di testo prima della
	// freccia (senza la quale il ciclo del testo non gira affatto e il
	// bug non si manifesta).
	{
		CContainer* doc3 = new CContainer(NULL, NULL);
		TryToParseString("Zona", cell(1, 8), doc3, true);
		TryToParseString("Valore", cell(2, 8), doc3, true);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view3 = new SheetView(doc3);
		view3->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view3);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per la freccia si blocca per disegnarci sopra");

		view3->SetAutoFilter(range(1, 8, 2, 8));
		view3->Draw(canvasRect);
		view3->Sync();
		canvas->Unlock();

		BRect arrowRect = view3->AutoFilterArrowRect(1);
		BPoint arrowCenter((arrowRect.left + arrowRect.right) / 2,
			(arrowRect.top + arrowRect.bottom) / 2 + 1); // dentro il triangolo, non sulla punta in alto

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();
		uint8* px = bits + (int32)arrowCenter.y * bpr + (int32)arrowCenter.x * 4;
		// B_RGB32 in memoria: B, G, R, A (ordine gia' verificato in
		// test_image_alpha.cpp) -- il triangolo e' grigio (90,90,90),
		// non bianco.
		Check(px[0] < 150 && px[1] < 150 && px[2] < 150,
			"la freccia a discesa e' davvero disegnata (pixel grigio, non bianco) al centro del triangolo");

		delete canvas;
		doc3->Release();
	}

	// --- Bug reale scoperto DOPO aver implementato quanto sopra, su un
	// file reale con righe nascoste: RebuildRowOffsets tratta una riga
	// nascosta come alta zero (giusto), ma DUE punti del codice
	// continuavano a usare la sua altezza VERA (fRowHeights, mai
	// azzerata -- resta il valore di "ripristino" per quando torna
	// visibile, vedi il commento su fRowHidden in SheetView.h) invece
	// di trattarla come zero: l'etichetta della riga successiva
	// nell'intestazione (posizionata sommando quell'altezza vera alla
	// somma cumulativa gia' collassata) finiva spinta molto piu' in
	// basso del dovuto, e il testo della cella (DrawString, mai
	// vincolato all'altezza zero del proprio rettangolo come
	// FillRect/StrokeLine) restava comunque disegnato, sovrapposto al
	// testo della riga visibile che ne aveva preso il posto -- risultato
	// visivo: testo illeggibile, ammassato su poche righe, numeri di
	// riga fuori ordine. Riprodotto qui con una riga nascosta che
	// aveva un'altezza grande PRIMA di essere nascosta (come una riga
	// con testo a capo su piu' righe in un file reale), verificato
	// leggendo i pixel davvero disegnati su una bitmap offscreen
	// (stessa tecnica di test_image_alpha.cpp/test_sheet_tabs.cpp), non
	// solo lo stato interno.
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		TryToParseString("Prima", cell(1, 5), doc2, true); // riga 5, visibile
		TryToParseString("Nascosta", cell(1, 6), doc2, true); // riga 6, nascosta sotto
		TryToParseString("Dopo", cell(1, 7), doc2, true); // riga 7, visibile

		// La bitmap resta grande (spazio sicuro per leggere qualunque
		// pixel dopo), ma si disegna passando a Draw() un rettangolo
		// BASSO apposta (non i suoi Bounds() interi): Draw() calcola
		// firstRow/lastRow dal rettangolo ricevuto, non dalla dimensione
		// intera della vista, quindi limitarlo alle sole righe 5-7 evita
		// che una riga vera oltre la 7 (che sale a occupare lo spazio
		// lasciato libero dalla 6 nascosta) finisca per occupare
		// legittimamente la zona "fantasma" controllata piu' avanti --
		// altrimenti il controllo "deve restare bianco" sarebbe un falso
		// negativo indipendente dal bug.
		BRect canvasRect(0, 0, 799, 599);
		BRect drawRect(0, 0, 299, 149);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view2 = new SheetView(doc2);
		view2->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view2);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per la regressione si blocca per disegnarci sopra");

		// La riga 6 aveva un'altezza grande PRIMA di essere nascosta
		// (come una riga con testo a capo su piu' righe in un file
		// reale) -- SetHiddenRows sotto la nasconde SENZA azzerare
		// fRowHeights[5], esattamente come l'import XLSX reale che ha
		// scoperto il bug.
		std::vector<std::pair<int, float> > tallRow;
		tallRow.push_back(std::make_pair(6, 200.0f));
		view2->SetRowHeights(tallRow);
		std::vector<int> hidden6;
		hidden6.push_back(6);
		view2->SetHiddenRows(hidden6);

		BRect row5Rect = view2->CellRect(cell(1, 5));
		BRect row7Rect = view2->CellRect(cell(1, 7));
		Check(row7Rect.top == row5Rect.bottom,
			"la riga 7 sale subito sotto la 5 anche se la riga 6 nascosta era alta 200px prima di essere nascosta");

		// Riempie prima l'intera bitmap di bianco: limitare Draw() a
		// "drawRect" sotto (per il motivo spiegato li') significa che i
		// pixel FUORI da quel rettangolo non vengono mai toccati da
		// Draw() -- senza questo riempimento esplicito resterebbero
		// memoria non inizializzata, non necessariamente bianca, e il
		// controllo piu' sotto ("deve restare bianco") fallirebbe a
		// prescindere dal bug.
		view2->SetHighColor(255, 255, 255);
		view2->FillRect(canvasRect);

		view2->Draw(drawRect);
		view2->Sync();
		canvas->Unlock();

		// Punto esatto dove l'etichetta "fantasma" della riga 6 sarebbe
		// finita col bug: la vecchia formula la posizionava a
		// fRowOffsets[5] (collassato, cioe' row5Rect.bottom) +
		// fRowHeights[5] (l'altezza VERA, 200, mai azzerata) - 6 --
		// ben dentro la zona occupata da altre righe visibili molto
		// piu' sotto, non da row5Rect.bottom in su. Deve restare lo
		// sfondo bianco (nessuna etichetta ne' testo di cella
		// disegnati li'), non il nero del testo "6"/"Nascosta".
		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();
		// "- 6" nella formula buggy e' la distanza dalla BASE del
		// testo (DrawString piazza li' la linea di base, non il centro
		// del glifo): il corpo vero della cifra sta qualche pixel PIU'
		// IN ALTO, non esattamente sulla base -- campionato a -8
		// ulteriori pixel per cadere dentro l'inchiostro del glifo,
		// stesso principio (evitare il centro/il bordo esatto) gia'
		// imparato scrivendo test_sheet_tabs.cpp.
		// x vicino al bordo sinistro della vista (dove SheetView
		// disegna davvero l'etichetta numerica, "rowHeaderLeft + 4" nel
		// codice -- non row5Rect.left, che e' gia' dentro l'area delle
		// celle, molto piu' a destra della colonna delle intestazioni).
		BPoint ghostLabelSpot(4, row5Rect.bottom + 200 - 6 - 8);
		uint8* px = bits + (int32)ghostLabelSpot.y * bpr + (int32)ghostLabelSpot.x * 4;
		Check(px[0] == 255 && px[1] == 255 && px[2] == 255,
			"nessuna etichetta/testo fantasma della riga 6 nascosta nel punto esatto dove finiva col bug");

		delete canvas;
		doc2->Release();
	}

	// --- Bug reale scoperto durante l'audit: Ordina/Inserisci riga/
	// Elimina riga/Inserisci colonna/Elimina colonna spostano il
	// CONTENUTO delle righe senza mai far seguire fRowHidden -- una
	// riga nascosta prima restava nascosta dopo (indice giusto,
	// contenuto ormai sbagliato) invece di seguire i dati, esattamente
	// come gli intervalli uniti prima che AdjustMergedRanges fosse
	// chiamata da questi stessi comandi. ---
	{
		CContainer* doc4 = new CContainer(NULL, NULL);
		TryToParseString("Zona", cell(1, 5), doc4, true);
		TryToParseString("Valore", cell(2, 5), doc4, true);
		TryToParseString("Nord", cell(1, 6), doc4, true);
		TryToParseString("10", cell(2, 6), doc4, true);
		TryToParseString("Sud", cell(1, 7), doc4, true);
		TryToParseString("20", cell(2, 7), doc4, true);
		TryToParseString("Nord", cell(1, 8), doc4, true);
		TryToParseString("30", cell(2, 8), doc4, true);
		TryToParseString("Est", cell(1, 9), doc4, true);
		TryToParseString("40", cell(2, 9), doc4, true);
		TryToParseString("Sud", cell(1, 10), doc4, true);
		TryToParseString("50", cell(2, 10), doc4, true);

		TestWindow* win4 = new TestWindow();
		SheetView* view4 = new SheetView(doc4);
		BScrollView* scroll4 = new BScrollView("scroll4", view4, B_FOLLOW_ALL, 0, true, true);
		scroll4->ResizeTo(600, 500);
		BLayoutBuilder::Group<>(win4, B_VERTICAL, 0).Add(scroll4);
		win4->Show();
		win4->Lock();

		view4->SetAutoFilter(range(1, 5, 2, 5));
		view4->SetColumnValueHidden(1, "Nord", true);
		Check(view4->IsRowHidden(6) && view4->IsRowHidden(8) && !view4->IsRowHidden(7),
			"prima di Ordina: le righe Nord (6 e 8) sono nascoste, come nel test principale sopra");

		// Ordina crescente per Zona (Est < Nord < Sud alfabeticamente):
		// dopo l'ordinamento la riga 6 e' Est/40, 7 e 8 sono le due
		// Nord (10 e 30), 9 e 10 le due Sud (20 e 50) -- le righe
		// nascoste devono seguire "Nord" alle sue NUOVE posizioni (7 e
		// 8), non restare ferme alle vecchie (6 e 8).
		view4->SetSelection(cell(1, 6));
		view4->ExtendSelection(cell(2, 10));
		view4->SortSelection(true);

		Check(!view4->IsRowHidden(6), "dopo Ordina: la riga 6 (ora Est) NON e' piu' nascosta");
		Check(view4->IsRowHidden(7) && view4->IsRowHidden(8),
			"dopo Ordina: le righe 7 e 8 (ora le due Nord) SONO nascoste, alle nuove posizioni");
		Check(!view4->IsRowHidden(9) && !view4->IsRowHidden(10),
			"dopo Ordina: le righe 9 e 10 (ora le due Sud) restano visibili");

		// Inserire una riga sopra la zona filtrata sposta tutto in giu'
		// di uno SENZA cambiare il contenuto di ogni riga: le righe
		// nascoste devono seguire lo stesso contenuto alla sua nuova
		// posizione (7 e 8 diventano 8 e 9).
		view4->SetSelection(cell(1, 6));
		view4->ExtendSelection(cell(2, 6));
		view4->InsertRows();
		Check(!view4->IsRowHidden(7), "dopo Inserisci riga: la vecchia riga 7 (Est, spostata a 7) NON e' nascosta");
		Check(view4->IsRowHidden(8) && view4->IsRowHidden(9),
			"dopo Inserisci riga: le due Nord (spostate a 8 e 9) restano nascoste alle nuove posizioni");

		win4->Unlock();
		win4->Lock();
		win4->Quit();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
