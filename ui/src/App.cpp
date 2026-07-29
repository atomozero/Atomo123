/*
	App.cpp

	Vedi App.h.
*/

#include "App.h"
#include "MainWindow.h"

#include <Entry.h>

static const char* kAppSignature = "application/x-vnd.Atomo-Atomo123";

App::App()
	:
	BApplication(kAppSignature),
	fWindow(NULL)
{
}

void App::ReadyToRun()
{
	fWindow = new MainWindow();
	fWindow->Show();
}

void App::RefsReceived(BMessage* message)
{
	// BApplication e BWindow girano su thread (BLooper) distinti:
	// non si puo' chiamare direttamente un metodo che tocca le BView
	// della finestra da qui senza il lock della finestra. Si inoltra
	// invece il messaggio al BWindow stesso (MainWindow::MessageReceived
	// gestisce gia' B_REFS_RECEIVED), cosi' viene elaborato sul thread
	// corretto con il lock preso automaticamente dal message loop.
	if (fWindow)
		fWindow->PostMessage(message);
}

int main()
{
	App app;
	app.Run();
	return 0;
}
