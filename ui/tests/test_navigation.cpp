/*
	test_navigation.cpp

	Verifica le scorciatoie da tastiera in stile Excel/LibreOffice
	Calc aggiunte su richiesta esplicita dell'utente ("vorrei
	replicare tutti i comandi da tastiera di Excel"): Inizio/Ctrl+
	Inizio, Ctrl+Fine, PagSu/PagGiu, Maiusc+Tab, Maiusc+Invio.

	Usa SheetView::HandleKey(char, bool ctrl, bool shift) invece di
	KeyDown() direttamente: KeyDown() legge Ctrl/Maiusc dal vero
	messaggio B_KEY_DOWN corrente (Window()->CurrentMessage()), che
	qui sarebbe sempre NULL/senza modificatori perche' non si passa
	da un vero ciclo di dispatch della tastiera (stesso principio
	di test_scroll.cpp/test_editing.cpp: piu' affidabile di
	un'iniezione di input reale -- vedi ROADMAP.md/
	docs/UI_ARCHITECTURE.md per i problemi gia' documentati con
	hey/Pippo, incluso un episodio in questa stessa sessione in cui
	l'iniezione e' finita per errore su una finestra di un'altra
	sessione concorrente sullo stesso desktop condiviso).
*/

#include <cstdio>

#include <Application.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <ScrollView.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
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
		: BWindow(BRect(100, 100, 700, 500), "test-navigation", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestNavigation");

	CContainer* doc = new CContainer(NULL, NULL);
	// Dati d'uso fino a E10 (colonna 5, riga 10): definisce l'angolo
	// che Ctrl+Fine deve raggiungere.
	TryToParseString("dato", cell(5, 10), doc, true);

	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
	scroll->ResizeTo(400, 300);	// vedi test_scroll.cpp per il perche'
	BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
	win->Show();

	win->Lock();

	// Inizio (senza Ctrl): va alla colonna A della riga corrente.
	view->SetSelection(cell(6, 4));	// F4
	view->HandleKey(B_HOME, false, false);
	cell afterHome = view->Selection();
	Check(afterHome.h == 1 && afterHome.v == 4,
		"Inizio va alla colonna A restando sulla stessa riga (F4 -> A4)");

	// Ctrl+Inizio: va sempre ad A1, indipendentemente da dov'eravamo.
	view->SetSelection(cell(6, 20));
	view->HandleKey(B_HOME, true, false);
	cell afterCtrlHome = view->Selection();
	Check(afterCtrlHome.h == 1 && afterCtrlHome.v == 1,
		"Ctrl+Inizio va sempre ad A1");

	// Ctrl+Fine: va all'angolo in basso a destra dei dati (E10, per
	// via della cella scritta all'inizio).
	view->SetSelection(cell(1, 1));
	view->HandleKey(B_END, true, false);
	cell afterCtrlEnd = view->Selection();
	Check(afterCtrlEnd.h == 5 && afterCtrlEnd.v == 10,
		"Ctrl+Fine va all'ultima cella con contenuto (E10)");

	// PagGiu/PagSu: si spostano di un numero di righe pari a quante
	// ne stanno nell'area visibile (non un numero fisso arbitrario).
	view->SetSelection(cell(1, 1));
	view->HandleKey(B_PAGE_DOWN, false, false);
	cell afterPageDown = view->Selection();
	Check(afterPageDown.v > 1, "PagGiu sposta la selezione piu' in basso");

	view->HandleKey(B_PAGE_UP, false, false);
	cell afterPageUp = view->Selection();
	Check(afterPageUp.v == 1, "PagSu riporta la selezione dov'era prima del PagGiu");

	// Maiusc+Tab: sposta a sinistra invece che a destra.
	view->SetSelection(cell(3, 1));	// C1
	view->HandleKey(B_TAB, false, true);
	cell afterShiftTab = view->Selection();
	Check(afterShiftTab.h == 2 && afterShiftTab.v == 1,
		"Maiusc+Tab sposta a sinistra (C1 -> B1)");

	// Tab (senza Maiusc): sposta a destra come prima -- non una
	// regressione del comportamento gia' esistente.
	view->HandleKey(B_TAB, false, false);
	cell afterTab = view->Selection();
	Check(afterTab.h == 3 && afterTab.v == 1,
		"Tab senza Maiusc sposta ancora a destra come prima (B1 -> C1)");

	// Maiusc+Invio: sposta in alto invece che in basso.
	view->SetSelection(cell(1, 5));	// A5
	view->HandleKey(B_RETURN, false, true);
	cell afterShiftReturn = view->Selection();
	Check(afterShiftReturn.h == 1 && afterShiftReturn.v == 4,
		"Maiusc+Invio sposta in alto (A5 -> A4)");

	// Invio (senza Maiusc): sposta in basso come prima.
	view->HandleKey(B_RETURN, false, false);
	cell afterReturn = view->Selection();
	Check(afterReturn.h == 1 && afterReturn.v == 5,
		"Invio senza Maiusc sposta ancora in basso come prima (A4 -> A5)");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
