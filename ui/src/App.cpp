/*
	App.cpp

	Vedi App.h.
*/

#include "App.h"
#include "MainWindow.h"

#include <Entry.h>
#include <Path.h>
#include <Roster.h>

#include <cstdio>

#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "ResourceManager.h"

static const char* kAppSignature = "application/x-vnd.Atomo-Atomo123";

App::App()
	:
	BApplication(kAppSignature),
	fWindow(NULL)
{
}

void App::ReadyToRun()
{
	// Le formule con funzioni con nome (SUM, IF, ecc. -- vedi
	// engine/resources/funcs_by_nr.r) hanno bisogno che l'engine
	// carichi la tabella delle funzioni da una risorsa 'Func' legata
	// al binario in esecuzione: senza questa chiamata ogni nome di
	// funzione viene trattato come identificatore sconosciuto (vedi
	// docs/ENGINE_API.md). gAppName serve anche a LoadPlugIns() per
	// cercare add-on opzionali in Functions/ accanto al binario. Un
	// fallimento qui (binario senza risorse, percorso inatteso) non
	// deve impedire l'avvio dell'app: si degrada allo stesso
	// comportamento di prima (nessuna funzione con nome disponibile).
	app_info info;
	if (GetAppInfo(&info) == B_OK) {
		BPath path(&info.ref);
		if (path.InitCheck() == B_OK) {
			gAppName = path;
			gResourceManager.SetTo(&path);
			try {
				InitFunctions();
			} catch (CErr& e) {
				fprintf(stderr,
					"Atomo123: impossibile caricare la tabella delle "
					"funzioni con nome: %s\n", (char*)e);
			}
		}
	}

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
