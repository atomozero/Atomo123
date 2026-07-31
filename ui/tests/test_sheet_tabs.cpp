/*
	test_sheet_tabs.cpp

	Verifica SheetTabView (Fase 9): la striscia di schede del foglio
	attivo che sostituisce il vecchio menu a tendina, con scorrimento
	tramite due frecce quando le schede non entrano nella larghezza
	disponibile -- il motivo per cui la vista esiste, dato che una
	cartella di lavoro reale puo' avere decine di fogli (vedi
	ROADMAP.md, Fase 9).

	Nessuna vera MainWindow: la vista si usa direttamente dentro una
	finestra di prova, stesso principio di test_resize.cpp per
	SheetView. Il bersaglio dei messaggi "cambia foglio" e' la
	finestra di prova stessa, che li intercetta in MessageReceived per
	verificare l'indice inviato -- SheetTabView::MouseDown su una
	scheda invia il messaggio in modo asincrono (BMessenger::
	SendMessage, come un vero BMenuItem), quindi va lasciato un po' di
	tempo al thread della finestra per elaborarlo dopo aver rilasciato
	il lock.
*/

#include <cstdio>
#include <vector>

#include <Application.h>
#include <Message.h>
#include <OS.h>
#include <String.h>
#include <Window.h>

#include "SheetTabView.h"

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

static const uint32 kMsgTestSwitch = 'tswt';

class TestWindow : public BWindow {
public:
	TestWindow()
		: BWindow(BRect(100, 100, 500, 200), "test-sheet-tabs", B_TITLED_WINDOW, 0),
		fLastIndex(-1)
	{
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == kMsgTestSwitch)
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				fLastIndex = index;
			return;
		}
		BWindow::MessageReceived(message);
	}

	int32 fLastIndex;
};

static BPoint RectCenter(BRect r)
{
	return BPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2);
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestSheetTabs");

	TestWindow* win = new TestWindow();
	SheetTabView* tabs = new SheetTabView("tabs", kMsgTestSwitch, win);
	tabs->MoveTo(0, 0);
	tabs->ResizeTo(300, 22);
	win->AddChild(tabs);
	win->Show();

	win->Lock();

	// --- Poche schede corte: entrano tutte, nessuno scorrimento ---
	std::vector<BString> few;
	few.push_back("Uno");
	few.push_back("Due");
	tabs->SetSheets(few, 0);
	Check(!tabs->IsScrolling(), "due schede corte entrano nella larghezza disponibile");
	Check(tabs->FirstVisibleIndex() == 0, "senza scorrimento la prima scheda visibile e' la 0");
	Check(tabs->TabRectFor(0).IsValid() && tabs->TabRectFor(1).IsValid(),
		"entrambe le schede hanno un rettangolo valido quando non si scorre");

	// --- Molte schede: non entrano tutte, serve lo scorrimento ---
	std::vector<BString> many;
	for (int i = 0; i < 20; i++)
	{
		BString name("Foglio numero ");
		name << (i + 1);
		many.push_back(name);
	}
	tabs->SetSheets(many, 0);
	Check(tabs->IsScrolling(), "venti schede non entrano nella larghezza della finestra di prova");
	Check(tabs->FirstVisibleIndex() == 0,
		"la scheda attiva (indice 0) e' visibile subito dopo SetSheets");
	Check(!tabs->TabRectFor(19).IsValid(),
		"una scheda lontana (19) non e' visibile senza aver scorso");

	// --- Le frecce scorrono senza cambiare il foglio attivo ---
	BRect rightArrow = tabs->RightArrowRect();
	tabs->MouseDown(RectCenter(rightArrow));
	Check(tabs->FirstVisibleIndex() == 1, "la freccia destra scorre di una scheda alla volta");

	win->Unlock();
	snooze(50000);
	win->Lock();
	Check(win->fLastIndex == -1, "scorrere con le frecce non invia nessun messaggio di cambio foglio");

	BRect leftArrow = tabs->LeftArrowRect();
	tabs->MouseDown(RectCenter(leftArrow));
	Check(tabs->FirstVisibleIndex() == 0, "la freccia sinistra scorre indietro");

	// --- Cambiare foglio attivo (SetSheets con un nuovo indice)
	// porta la nuova scheda attiva in vista se non lo era ---
	tabs->SetSheets(many, 19);
	Check(tabs->TabRectFor(19).IsValid(), "SetSheets con indice attivo 19 lo porta in vista");

	// --- Clic su una scheda visibile invia il messaggio con l'indice giusto ---
	tabs->SetSheets(many, 0); // torna a un punto noto (scheda 0 attiva, prima visibile)
	BRect tab1Rect = tabs->TabRectFor(1);
	Check(tab1Rect.IsValid(), "la seconda scheda (indice 1) e' visibile a partire dalla 0");
	tabs->MouseDown(RectCenter(tab1Rect));

	win->Unlock();
	snooze(50000);
	win->Lock();
	Check(win->fLastIndex == 1,
		"un clic sulla seconda scheda invia il messaggio di cambio foglio con indice 1");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
