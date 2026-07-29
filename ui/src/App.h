/*
	App.h

	BApplication dell'app Atomo123: crea la finestra principale e
	inoltra i file aperti da Tracker/riga di comando (B_REFS_RECEIVED)
	alla finestra tramite MainWindow::OpenFile.
*/

#ifndef APP_H
#define APP_H

#include <Application.h>

class MainWindow;

class App : public BApplication {
public:
	App();

	virtual void ReadyToRun();
	virtual void RefsReceived(BMessage* message);

private:
	MainWindow* fWindow;
};

#endif
