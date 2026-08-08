/*
	test_scrollbars_visible.cpp

	Verifica che le due barre di scorrimento della griglia siano
	posizionate DAVVERO dentro l'area visibile della finestra, non
	fuori schermo -- bug reale segnalato dall'utente ("non si vedono
	le barre di scorrimento").

	Causa reale: le barre automatiche di una BScrollView si
	posizionano in base al Frame() del bersaglio (SheetView, che
	copre l'intero canvas virtuale del foglio, ~56000 x 328000 pixel,
	vedi SheetView::FullCanvasFrame), non in base alla propria area
	visibile -- finivano ancorate a quel bordo enorme, mai visibili
	ne' utilizzabili (confermato dal vivo interrogando Frame() delle
	barre via "hey Atomo123 get Frame..."). Corretto smettendo di
	usare le barre automatiche di BScrollView (costruita con
	horizontal=false, vertical=false) e creando due BScrollBar
	indipendenti (MainWindow::fVScrollBar/fHScrollBar), fratelli di
	"scroll" nel layout della finestra invece che suoi figli privati:
	BLayoutBuilder le posiziona come qualunque altra view, sempre
	dentro l'area visibile vera.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <ScrollBar.h>

#include "Cell.h"
#include "Range.h"
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

int main()
{
	BApplication app("application/x-vnd.Atomo-TestScrollbarsVisible");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();

	BScrollBar* hsb = view->ScrollBar(B_HORIZONTAL);
	BScrollBar* vsb = view->ScrollBar(B_VERTICAL);
	Check(hsb != NULL, "SheetView ha una barra di scorrimento orizzontale collegata");
	Check(vsb != NULL, "SheetView ha una barra di scorrimento verticale collegata");

	if (hsb && vsb)
	{
		BRect hFrame = hsb->Frame();
		BRect vFrame = vsb->Frame();

		// Limite di controllo ampiamente sotto le dimensioni del canvas
		// virtuale (~56000 x 328000, vedi sopra) ma ampiamente sopra
		// qualunque finestra reale: una barra fuori da questo intervallo
		// e' fuori schermo per costruzione, non solo "poco probabile".
		const float kSaneLimit = 5000;

		Check(hFrame.right < kSaneLimit && hFrame.bottom < kSaneLimit,
			"la barra orizzontale e' dentro un'area visibile ragionevole, non nel canvas virtuale (~56000x328000)");
		Check(vFrame.right < kSaneLimit && vFrame.bottom < kSaneLimit,
			"la barra verticale e' dentro un'area visibile ragionevole, non nel canvas virtuale (~56000x328000)");

		// Le due barre devono avere una dimensione reale (non
		// collassate a zero da un calcolo sbagliato).
		Check(hFrame.Width() > 10 && hFrame.Height() > 5 && hFrame.Height() < 30,
			"la barra orizzontale ha una larghezza sostanziale e un'altezza da barra sottile, non collassata a zero");
		Check(vFrame.Height() > 10 && vFrame.Width() > 5 && vFrame.Width() < 30,
			"la barra verticale ha un'altezza sostanziale e una larghezza da barra sottile, non collassata a zero");
	}

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
