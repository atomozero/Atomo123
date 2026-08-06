#include "SplashWindow.h"

#include <Screen.h>

#include "AtomGLView.h"

// Versione piccola: 720x440 originale ridotta del 66% mantenendo le
// proporzioni (18:11), pensata per non dominare lo schermo all'avvio.
static const float kWidth = 480.0f;
static const float kHeight = 293.0f;

SplashWindow::SplashWindow()
	:
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
