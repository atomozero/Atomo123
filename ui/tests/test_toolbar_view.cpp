/*
	test_toolbar_view.cpp

	Verifica ToolbarView: i gruppi di pulsanti si affiancano
	orizzontalmente e, quando la finestra si restringe e un gruppo non
	entra piu' nella larghezza disponibile, va a capo per intero su una
	nuova riga -- mai un singolo pulsante a meta' di un gruppo (vedi
	ToolbarView.h). Nessun pulsante resta mai nascosto: a differenza del
	vecchio pulsante ">>" di troppopieno, restringere la finestra fa
	crescere il numero di righe, non sparire pulsanti. Stessa filosofia
	di test_sheet_tabs.cpp per SheetTabView: nessuna vera MainWindow, la
	vista si usa direttamente dentro una finestra di prova, e si varia
	la larghezza della vista invece che il numero di elementi per
	forzare/non forzare l'a capo.
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

// Aggiunge un gruppo di "count" pulsanti (con un separatore prima, se
// non e' il primo gruppo) -- stesso schema di kToolbarGroups in
// MainWindow.cpp, qui parametrizzato per costruire toolbar di prova di
// dimensioni diverse.
static void AddGroup(ToolbarView* toolbar, BWindow* win, int groupIndex, int count,
	bool first)
{
	if (!first)
		toolbar->AddSeparator();

	for (int i = 0; i < count; i++)
	{
		BString name("toolTest");
		name << groupIndex << "_" << i;
		BString label("Pulsante ");
		label << groupIndex << "." << i;

		BButton* button = new BButton(name.String(), NULL, new BMessage(kMsgTestButton));
		button->SetTarget(win);
		button->SetFlat(true);
		toolbar->AddButton(button, label.String());
	}
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestToolbarView");

	BWindow* win = new BWindow(BRect(100, 100, 2100, 400),
		"test-toolbar-view", B_TITLED_WINDOW, 0);

	ToolbarView* toolbar = new ToolbarView("toolbar");
	toolbar->MoveTo(0, 0);
	toolbar->ResizeTo(2000, 30);
	win->AddChild(toolbar);

	// Quattro gruppi da cinque pulsanti ciascuno (venti pulsanti in
	// tutto): abbastanza per forzare piu' di un a capo su una vista
	// stretta, qualunque sia la larghezza esatta di un singolo pulsante
	// su questo sistema.
	static const int kGroupCount = 4;
	static const int kButtonsPerGroup = 5;
	for (int g = 0; g < kGroupCount; g++)
		AddGroup(toolbar, win, g, kButtonsPerGroup, g == 0);

	win->Show();
	win->Lock();

	// --- Vista larga: tutti i gruppi entrano in una sola riga ---
	toolbar->ResizeTo(2000, 30);
	toolbar->Layout();
	Check(toolbar->RowCount() == 1,
		"una vista larga (2000px) per quattro gruppi sta tutta su una sola riga");
	Check(toolbar->ButtonCount() == kGroupCount * kButtonsPerGroup,
		"tutti e venti i pulsanti esistono (nessun troppopieno, nessun pulsante nascosto)");

	// --- Vista stretta: non tutti i gruppi entrano in una riga, va a
	// capo su piu' righe ---
	toolbar->ResizeTo(150, 30);
	toolbar->Layout();
	int narrowRows = toolbar->RowCount();
	Check(narrowRows > 1,
		"una vista stretta (150px) per quattro gruppi va a capo su piu' righe");
	Check(toolbar->ButtonCount() == kGroupCount * kButtonsPerGroup,
		"restringendo la vista nessun pulsante sparisce (a differenza del vecchio troppopieno)");

	// --- Nessun pulsante di un gruppo finisce su una riga diversa dai
	// suoi compagni di gruppo: un gruppo va a capo per intero, mai a
	// meta' -- il punto centrale di quello che l'utente ha chiesto.
	bool anyGroupSplit = false;
	for (int g = 0; g < kGroupCount; g++)
	{
		float firstY = -1;
		for (int i = 0; i < kButtonsPerGroup; i++)
		{
			BString name("toolTest");
			name << g << "_" << i;
			BButton* button = dynamic_cast<BButton*>(win->FindView(name.String()));
			if (!button)
				continue;
			float y = button->Frame().top;
			if (firstY < 0)
				firstY = y;
			else if (y != firstY)
				anyGroupSplit = true;
		}
	}
	Check(!anyGroupSplit,
		"nella vista stretta nessun gruppo e' spezzato fra due righe: i suoi pulsanti "
		"restano sempre tutti sulla stessa riga");

	// --- Riallargare la vista fa tornare tutto su una riga sola ---
	toolbar->ResizeTo(2000, 30);
	toolbar->Layout();
	Check(toolbar->RowCount() == 1,
		"riallargando la vista i gruppi tornano tutti su una sola riga");

	// --- Una vista ancora piu' stretta di prima (ogni gruppo sulla
	// propria riga) fa crescere ulteriormente il numero di righe ---
	toolbar->ResizeTo(60, 30);
	toolbar->Layout();
	Check(toolbar->RowCount() >= narrowRows,
		"restringendo ulteriormente la vista il numero di righe non diminuisce mai");
	Check(toolbar->ButtonCount() == kGroupCount * kButtonsPerGroup,
		"anche nella vista strettissima nessun pulsante sparisce");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
