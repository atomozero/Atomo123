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
#include <Bitmap.h>
#include <Message.h>
#include <OS.h>
#include <String.h>
#include <View.h>
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

	// --- Colore della scheda (import XLSX, <sheetPr><tabColor>): una
	// barra d'accento sul bordo inferiore, MAI un riempimento a piena
	// scheda, la stessa sia per la scheda ATTIVA sia per quella NON
	// attiva (Fase 34, BControlLook -- vedi il commento in
	// SheetTabView::Draw()). Il resto della scheda usa il disegno
	// nativo di BControlLook (DrawActiveTab/DrawInactiveTab), la cui
	// sfumatura esatta dipende dal tema dell'utente: qui si verifica
	// solo cio' che questa vista controlla davvero (la barra
	// d'accento), non un colore di sfondo che potrebbe cambiare con
	// ogni tema. Verificato leggendo i pixel davvero disegnati su una
	// bitmap offscreen ("accetta viste"), stessa tecnica di
	// ui/tests/test_image_alpha.cpp: un controllo sullo stato interno
	// non basterebbe, qui conta il colore che arriva davvero sullo
	// schermo.
	{
		std::vector<BString> two;
		two.push_back("Rossa");
		two.push_back("Normale");
		std::vector<bool> hasColor;
		hasColor.push_back(true);
		hasColor.push_back(false);
		std::vector<rgb_color> colorList;
		rgb_color red = { 255, 0, 0, 255 };
		colorList.push_back(red);
		colorList.push_back(red); // ignorato: hasColor[1] e' false

		BRect canvasRect(0, 0, 299, 21);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetTabView* offTabs = new SheetTabView("offtabs", kMsgTestSwitch, win);
		offTabs->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(offTabs);

		// SetSheets (GetFont/Invalidate) va chiamato con la bitmap
		// offscreen gia' bloccata, non solo Draw(): chiamato prima del
		// Lock() va in crash, verificato a parte con un piccolo
		// programma di prova (nessuna vera finestra dietro la vista
		// finche' non si blocca la bitmap che la ospita).
		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per le schede si blocca per disegnarci sopra");

		// Scheda 0 (rossa) NON attiva: la barra d'accento sul bordo
		// inferiore e' rossa, il resto della scheda NON e' piu' rosso a
		// tutta area (comportamento pre-BControlLook).
		offTabs->SetSheets(two, 1, &hasColor, &colorList);
		BRect tab0Rect = offTabs->TabRectFor(0);
		// Vicino all'angolo in alto a sinistra, non al centro: il nome
		// della scheda e' disegnato centrato verticalmente e a ridosso
		// del bordo sinistro (kTabPadding), un pixel preso li' in mezzo
		// rischia di cadere sull'antialiasing del testo invece che sul
		// riempimento di sfondo.
		BPoint inside0(tab0Rect.left + 3, tab0Rect.top + 3);
		BPoint bottom0(tab0Rect.left + tab0Rect.Width() / 2, tab0Rect.bottom - 1);

		offTabs->Draw(canvasRect);
		offTabs->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();
		// B_RGB32 in memoria: B, G, R, A (ordine verificato in
		// test_image_alpha.cpp).
		uint8* px = bits + (int32)inside0.y * bpr + (int32)inside0.x * 4;
		Check(!(px[0] == 0 && px[1] == 0 && px[2] == 255),
			"la scheda 0 (rossa, NON attiva) NON e' piu' disegnata rossa a tutta area");

		uint8* pxBottomInactive = bits + (int32)bottom0.y * bpr + (int32)bottom0.x * 4;
		Check(pxBottomInactive[0] == 0 && pxBottomInactive[1] == 0 && pxBottomInactive[2] == 255,
			"la scheda 0 (rossa) NON attiva mostra comunque la barra rossa sul bordo inferiore");

		// Scheda 0 (rossa) ATTIVA: stessa barra d'accento, nessun
		// riempimento a piena scheda nemmeno qui. La scheda 1 (senza
		// colore) e' ora quella NON attiva: nessuna barra colorata sul
		// suo bordo inferiore.
		locked = canvas->Lock();
		offTabs->SetSheets(two, 0, &hasColor, &colorList);
		tab0Rect = offTabs->TabRectFor(0);
		bottom0 = BPoint(tab0Rect.left + tab0Rect.Width() / 2, tab0Rect.bottom - 1);
		BRect tab1Rect = offTabs->TabRectFor(1);
		BPoint bottom1(tab1Rect.left + tab1Rect.Width() / 2, tab1Rect.bottom - 1);

		offTabs->Draw(canvasRect);
		offTabs->Sync();
		canvas->Unlock();

		uint8* pxBottomActive = bits + (int32)bottom0.y * bpr + (int32)bottom0.x * 4;
		Check(pxBottomActive[0] == 0 && pxBottomActive[1] == 0 && pxBottomActive[2] == 255,
			"la scheda 0 (rossa) ATTIVA mostra comunque una barra rossa sul bordo inferiore");

		uint8* px1Bottom = bits + (int32)bottom1.y * bpr + (int32)bottom1.x * 4;
		Check(!(px1Bottom[0] == 0 && px1Bottom[1] == 0 && px1Bottom[2] == 255),
			"la scheda 1 (senza colore, ora NON attiva) non ha nessuna barra colorata");

		delete canvas;
	}

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
