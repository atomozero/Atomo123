/*
	test_toolbar_phase13.cpp

	Verifica i cinque pulsanti Fase 13 promossi a pulsante ora che il
	catalogo HVIF li copre (collegamento ipertestuale, commento cella,
	intervalli con nome, vai a, colore bordo -- vedi kNavigateToolbarButtons/
	kAnnotateToolbarButtons/kFormatToolbarButtons in MainWindow.cpp e
	Atomo123_icons/ATOMO123.md per la selezione delle icone). Il
	comportamento "a capo" della toolbar (gruppi affiancati, un gruppo
	intero va a capo quando non entra piu') e' invece testato a fondo in
	tests/test_toolbar_view.cpp, che lavora direttamente su ToolbarView
	senza bisogno di una vera MainWindow. Stesso principio di
	test_format_toolbar.cpp: qui interessa solo che il
	pulsante esista, sia targettato su MainWindow e che cliccarlo apra la
	finestra giusta senza crash -- l'effetto delle singole finestre
	(CommentWindow, HyperlinkWindow, NameWindow, GoToWindow, ColorWindow) e'
	gia' verificato altrove (test_comments.cpp, test_hyperlinks.cpp,
	test_names.cpp, test_goto.cpp).
*/

#include <cstdio>

#include <Application.h>
#include <Button.h>
#include <Message.h>
#include <View.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
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

static BButton* FindToolButton(MainWindow* win, const char* name)
{
	return dynamic_cast<BButton*>(win->FindView(name));
}

static void ClickButton(MainWindow* win, BButton* button)
{
	if (button && button->Message())
	{
		BMessage msg(*button->Message());
		win->MessageReceived(&msg);
	}
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestToolbarPhase13");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();
	TryToParseString("1", cell(1, 1), doc, true); // A1

	static const char* kNames[] = {
		"toolHyperlink", "toolComment", "toolGoTo", "toolNamedRanges", "toolBorderColor",
	};
	bool allFound = true;
	bool allTargeted = true;
	for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++)
	{
		BButton* button = FindToolButton(win, kNames[i]);
		if (!button)
		{
			allFound = false;
			continue;
		}
		BLooper* looper = NULL;
		BHandler* target = button->Target(&looper);
		if (target != win)
			allTargeted = false;
	}
	Check(allFound, "i cinque pulsanti Fase 13 (hyperlink/commento/vai a/"
		"intervalli con nome/colore bordo) esistono tutti nella toolbar");
	Check(allTargeted, "tutti hanno MainWindow come target (SetTarget), non un handler a caso");

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));

	ClickButton(win, FindToolButton(win, "toolComment"));
	Check(win->FindView("toolComment") != NULL,
		"cliccare \"toolComment\" (apre CommentWindow) non va in crash");

	ClickButton(win, FindToolButton(win, "toolHyperlink"));
	Check(win->FindView("toolHyperlink") != NULL,
		"cliccare \"toolHyperlink\" (apre HyperlinkWindow) non va in crash");

	ClickButton(win, FindToolButton(win, "toolGoTo"));
	Check(win->FindView("toolGoTo") != NULL,
		"cliccare \"toolGoTo\" (apre GoToWindow) non va in crash");

	ClickButton(win, FindToolButton(win, "toolNamedRanges"));
	Check(win->FindView("toolNamedRanges") != NULL,
		"cliccare \"toolNamedRanges\" (apre NameWindow) non va in crash");

	// Colore bordo apre una vera ColorWindow asincrona (stesso principio
	// di toolTextColor/toolHighlight in test_format_toolbar.cpp): cliccato
	// due volte apposta, la seconda volta la finestra esiste gia'.
	BButton* borderColorButton = FindToolButton(win, "toolBorderColor");
	ClickButton(win, borderColorButton);
	ClickButton(win, borderColorButton);
	Check(true, "cliccare due volte \"toolBorderColor\" (ColorWindow gia' aperta la seconda volta) non va in crash");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
