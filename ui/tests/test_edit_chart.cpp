/*
	test_edit_chart.cpp

	Verifica MainWindow::EditChart/HandleChartUpdate: riaprire un
	grafico gia' incorporato per modificarne intervallo/tipo/titolo
	invece di doverlo cancellare e ricrearne uno nuovo -- richiesta
	esplicita dell'utente ("si potrebbe avere un editor dei grafici dopo
	averli creati?"), motivata da un file reale con grafici mal
	configurati (etichette sovrapposte, tipo sbagliato) che prima si
	potevano solo cancellare e ricreare da zero.

	Stesso schema di test_insert_chart.cpp (MainWindow vera, nessuna
	finestra di dialogo reale coinvolta: HandleChartUpdate e' pubblico
	apposta per essere testabile direttamente, stesso principio).

	Limite noto (non testabile in automatico, stesso limite gia'
	documentato in test_image_export_drag.cpp per "buttons"): il
	DOPPIO CLIC che apre l'editor (SheetView::MouseDown, lettura di
	"clicks" da Window()->CurrentMessage()) richiede un vero messaggio
	B_MOUSE_DOWN di sistema, non riproducibile chiamando MouseDown()
	come una normale funzione C++ -- verificato a mano nell'app vera.
	Qui si verifica solo la logica di aggiornamento vera e propria
	(MainWindow::EditChart/HandleChartUpdate), che il doppio clic si
	limita a invocare.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Menu.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "ChartWindow.h"
#include "Chart.h"

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
	BApplication app("application/x-vnd.Atomo-TestEditChart");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer* doc = win->GetSheetView()->Document();

	TryToParseString("Gen", cell(1, 1), doc, true);
	TryToParseString("10", cell(2, 1), doc, true);
	TryToParseString("Feb", cell(1, 2), doc, true);
	TryToParseString("20", cell(2, 2), doc, true);
	TryToParseString("Mar", cell(1, 3), doc, true);
	TryToParseString("30", cell(2, 3), doc, true);

	win->HandleChartInsert("A1:B3", "E1", eBarChart, "Titolo originale");
	Check(win->Charts().size() == 1, "un grafico esiste prima di modificarlo");
	BRect originalFrame = win->Charts()[0].frame;

	// Modifica valida: tipo e titolo cambiano, l'intervallo resta lo
	// stesso testo (A1:B3) -- caso comune motivante la richiesta
	// dell'utente, "ho sbagliato il tipo di grafico".
	win->HandleChartUpdate(0, "A1:B3", ePieChart, "Titolo corretto");
	Check(win->Charts().size() == 1, "HandleChartUpdate non aggiunge un nuovo grafico, modifica quello esistente");
	if (win->Charts().size() == 1)
	{
		Check(win->Charts()[0].type == ePieChart, "il tipo e' stato aggiornato (torta)");
		Check(win->Charts()[0].title == "Titolo corretto", "il titolo e' stato aggiornato");
		Check(win->Charts()[0].frame == originalFrame,
			"la posizione/dimensione del grafico NON cambia editandolo");
	}

	// Annullabile: Undo riporta tipo/titolo/intervallo com'erano prima.
	win->GetSheetView()->Undo();
	Check(win->Charts().size() == 1, "Annulla non rimuove il grafico, lo riporta com'era");
	if (win->Charts().size() == 1)
	{
		Check(win->Charts()[0].type == eBarChart, "Annulla ripristina il tipo originale (barre)");
		Check(win->Charts()[0].title == "Titolo originale", "Annulla ripristina il titolo originale");
	}

	// Ripeti: Ripristina riapplica la modifica annullata.
	win->GetSheetView()->Redo();
	Check(win->Charts().size() == 1 && win->Charts()[0].type == ePieChart
			&& win->Charts()[0].title == "Titolo corretto",
		"Ripristina riapplica la modifica appena annullata");

	// Intervallo non valido: nessuna modifica applicata, il grafico
	// resta esattamente com'era (stesso limite di HandleChartInsert:
	// mostra un vero BAlert, non verificabile qui, solo che il modello
	// non cambi). NOTA: come test_insert_chart.cpp, questa riga mostra
	// un vero BAlert bloccante -- in QUESTO sandbox senza un utente
	// reale che lo chiuda, il test si blocca qui per sempre (stesso
	// limite pre-esistente e gia' documentato per test_insert_chart/
	// test_find_replace, non una novita' di questo file). Serve un vero
	// desktop Haiku interattivo per farlo girare fino in fondo.
	ChartObject beforeInvalid = win->Charts()[0];
	win->HandleChartUpdate(0, "A1:A3", eBarChart, "Non dovrebbe applicarsi");
	Check(win->Charts()[0].type == beforeInvalid.type && win->Charts()[0].title == beforeInvalid.title,
		"un intervallo dati non valido (una sola colonna) non modifica il grafico");

	// Indice fuori dai limiti: nessun crash, nessuna modifica.
	win->HandleChartUpdate(5, "A1:B3", eBarChart, "Indice inesistente");
	Check(win->Charts().size() == 1, "un indice di grafico inesistente non va in crash ne' aggiunge nulla");

	// EditChart popola davvero ChartWindow (creata al volo se serve) con
	// le impostazioni attuali -- non verificabile pixel per pixel senza
	// una vera finestra visibile, ma deve almeno non crashare ed
	// esistere dopo la chiamata.
	win->EditChart(0);
	Check(true, "EditChart su un indice valido non va in crash");
	win->EditChart(99);
	Check(true, "EditChart su un indice fuori dai limiti non va in crash (nessun effetto)");

	// Le 7 voci del menu Tipo esistono davvero (icone comprese: ognuna
	// e' un ChartTypeMenuItem costruito con un'icona renderizzata da
	// IconCatalog::RenderCustom) -- non verifica i pixel dell'icona, ma
	// esercita l'intero percorso di costruzione senza crash.
	BMenu* typeMenu = win->GetChartWindow() ? win->GetChartWindow()->TypeMenu() : NULL;
	Check(typeMenu != NULL, "ChartWindow espone un menu Tipo dopo EditChart");
	if (typeMenu)
	{
		Check(typeMenu->CountItems() == 7, "il menu Tipo ha le 7 voci previste");
		for (int32 i = 0; i < typeMenu->CountItems(); i++)
			Check(typeMenu->ItemAt(i) != NULL, "ogni voce del menu Tipo esiste (icona compresa)");
	}

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
