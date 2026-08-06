#include "SplashWindow.h"

#include <Screen.h>

#include "AtomGLView.h"

// Stessa dimensione della demo standalone (dist/Atomo123Splash):
// l'utente l'ha vista e preferita alla versione ridotta provata prima.
static const float kWidth = 720.0f;
static const float kHeight = 440.0f;

SplashWindow::SplashWindow()
	:
	// B_NORMAL_WINDOW_FEEL, non B_FLOATING_APP_WINDOW_FEEL: quel feel
	// mostra la finestra solo quando una finestra "normale" della
	// stessa app e' gia' attiva, ma qui lo splash viene mostrato PRIMA
	// che la MainWindow esista -- risultato: non compariva affatto.
	// Restare sempre davanti si ottiene invece riattivandolo a mano
	// dopo aver mostrato la MainWindow, vedi App::ReadyToRun.
	BWindow(BRect(0, 0, kWidth - 1, kHeight - 1), "Atomo123",
		B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_ASYNCHRONOUS_CONTROLS)
{
	BScreen screen(this);
	BRect screenFrame = screen.Frame();
	MoveTo(screenFrame.left + (screenFrame.Width() - kWidth) / 2.0f,
		screenFrame.top + (screenFrame.Height() - kHeight) / 2.0f);

	fView = new AtomGLView(Bounds());
	AddChild(fView);
}
