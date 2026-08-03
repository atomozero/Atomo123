/*
	test_toolbar_view.cpp

	Verifica ToolbarView: quando la finestra si restringe e i
	pulsanti non entrano piu' tutti nella larghezza disponibile,
	quelli in eccesso vengono nascosti e sostituiti da un unico
	pulsante ">>" -- stesso principio di test_sheet_tabs.cpp per
	SheetTabView (nessuna vera MainWindow, la vista si usa
	direttamente dentro una finestra di prova, e si varia la
	larghezza della vista invece che il numero di elementi per
	forzare/non forzare il troppopieno).
*/

#include <cstdio>

#include <Application.h>
#include <Button.h>
#include <Message.h>
#include <Window.h>

#include "ToolbarView.h"

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

static const uint32 kMsgTestButton = 'tbtn';

int main()
{
	BApplication app("application/x-vnd.Atomo-TestToolbarView");

	BWindow* win = new BWindow(BRect(100, 100, 2100, 200),
		"test-toolbar-view", B_TITLED_WINDOW, 0);

	ToolbarView* toolbar = new ToolbarView("toolbar");
	toolbar->MoveTo(0, 0);
	toolbar->ResizeTo(2000, 30);
	win->AddChild(toolbar);

	// Venti pulsanti, con un separatore ogni cinque (come i gruppi
	// veri della toolbar di MainWindow) -- abbastanza da non entrare
	// mai tutti in una vista stretta, qualunque sia la larghezza
	// esatta di un singolo pulsante su questo sistema.
	static const int kButtonCount = 20;
	for (int i = 0; i < kButtonCount; i++)
	{
		if (i > 0 && i % 5 == 0)
			toolbar->AddSeparator();

		BString name("toolTest");
		name << i;
		BString label("Pulsante ");
		label << i;

		BButton* button = new BButton(name.String(), NULL, new BMessage(kMsgTestButton));
		button->SetTarget(win);
		button->SetFlat(true);
		toolbar->AddButton(button, label.String());
	}

	win->Show();
	win->Lock();

	// --- Vista larga: tutti i pulsanti entrano, nessun troppopieno ---
	toolbar->ResizeTo(2000, 30);
	toolbar->Layout();
	Check(!toolbar->IsOverflowing(),
		"una vista larga (2000px) per venti pulsanti non attiva il troppopieno");
	Check(toolbar->VisibleButtonCount() == kButtonCount,
		"senza troppopieno tutti e venti i pulsanti sono visibili");
	Check(toolbar->HiddenButtonCount() == 0,
		"senza troppopieno nessun pulsante e' nascosto");
	Check(toolbar->OverflowButton()->IsHidden(),
		"il pulsante \">>\" resta nascosto quando non serve");

	// --- Vista stretta: non entrano tutti, scatta il troppopieno ---
	toolbar->ResizeTo(150, 30);
	toolbar->Layout();
	Check(toolbar->IsOverflowing(),
		"una vista stretta (150px) per venti pulsanti attiva il troppopieno");
	Check(toolbar->VisibleButtonCount() > 0,
		"col troppopieno resta comunque visibile almeno qualche pulsante");
	Check(toolbar->VisibleButtonCount() < kButtonCount,
		"col troppopieno non tutti i venti pulsanti restano visibili");
	Check(toolbar->HiddenButtonCount() == kButtonCount - toolbar->VisibleButtonCount(),
		"visibili + nascosti tornano sempre al totale di venti pulsanti");
	Check(!toolbar->OverflowButton()->IsHidden(),
		"il pulsante \">>\" appare quando scatta il troppopieno");

	// --- Riallargare la vista fa ricomparire tutti i pulsanti e
	// nascondere di nuovo il pulsante ">>" ---
	toolbar->ResizeTo(2000, 30);
	toolbar->Layout();
	Check(!toolbar->IsOverflowing(),
		"riallargando la vista il troppopieno si disattiva di nuovo");
	Check(toolbar->VisibleButtonCount() == kButtonCount,
		"riallargando la vista tutti i pulsanti tornano visibili");
	Check(toolbar->OverflowButton()->IsHidden(),
		"riallargando la vista il pulsante \">>\" torna nascosto");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
