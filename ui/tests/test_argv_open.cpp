/*
	test_argv_open.cpp

	Verifica App::ArgvReceived (Fase 34, "Path to full Excel parity"
	Tier 1): "atomo123 file.ascd" da riga di comando non apriva affatto
	il file (nessun override esisteva) -- l'app si limitava ad aprire un
	documento nuovo vuoto, l'argomento passato veniva semplicemente
	ignorato. App::RefsReceived (drag-and-drop, Tracker "Apri con...")
	gia' funzionava correttamente: ArgvReceived ne riusa la stessa
	funzione App::OpenOneRef per la scelta della finestra (nuova o
	riusata), gia' verificata a fondo in test_multiwindow.cpp -- qui si
	verifica solo la parte DAVVERO nuova, la conversione di argv in
	entry_ref: argv[0] (il percorso dell'eseguibile) va sempre
	ignorato, un percorso inesistente va ignorato in silenzio, e piu'
	argomenti nella stessa chiamata aprono piu' finestre.

	Stessa tecnica di test_multiwindow.cpp: usa la vera classe App (non
	una BApplication generica), ricompilando App.cpp con
	-DATOMO123_TEST_BUILD per escluderne il main(). Non chiama mai
	app.Run(): B_REFS_RECEIVED (inoltrato da ArgvReceived tramite
	PostMessage) viene elaborato in modo asincrono sul thread della
	finestra, quindi non se ne puo' verificare l'esito (il documento
	caricato) senza introdurre una corsa fra thread -- le uniche
	verifiche fatte sono sul conteggio di MainWindow create (una
	BWindow si registra presso la BApplication nel proprio costruttore,
	in modo sincrono, quindi il conteggio e' sempre corretto subito).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <Message.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "App.h"

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

static int CountMainWindows(BApplication* app)
{
	int count = 0;
	for (int32 i = 0; i < app->CountWindows(); i++)
	{
		if (dynamic_cast<MainWindow*>(app->WindowAt(i)))
			count++;
	}
	return count;
}

int main()
{
	App app;

	const char* path1 = "/tmp/test_argv_open_1.ascd";
	const char* path2 = "/tmp/test_argv_open_2.ascd";

	CContainer* doc1 = new CContainer(NULL, NULL);
	TryToParseString("Primo argomento", cell(1, 1), doc1, true);
	{
		BFile file(path1, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(doc1, &file) == B_OK, "preparazione del primo file di prova riuscita");
	}
	doc1->Release();

	CContainer* doc2 = new CContainer(NULL, NULL);
	TryToParseString("Secondo argomento", cell(1, 1), doc2, true);
	{
		BFile file(path2, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(doc2, &file) == B_OK, "preparazione del secondo file di prova riuscita");
	}
	doc2->Release();

	Check(CountMainWindows(&app) == 0, "nessuna MainWindow prima di qualunque evento (app.Run() non e' mai chiamato)");

	// Lo scenario reale: "atomo123 file.ascd" -- argv[0] e' il percorso
	// dell'eseguibile stesso (come in ogni programma C), MAI un file da
	// aprire, l'argomento vero e' argv[1]. Nessuna finestra esiste
	// ancora, quindi la prima ne crea sicuramente una nuova (nessuna
	// finestra "vergine" da riusare, a prescindere da IsUntouched()).
	{
		char arg0[] = "/Magazzino/Atomo123/ui/Atomo123";
		char arg1[] = "/tmp/test_argv_open_1.ascd";
		char* argv[] = { arg0, arg1 };
		app.ArgvReceived(2, argv);
	}
	Check(CountMainWindows(&app) == 1,
		"ArgvReceived con un percorso vero apre esattamente una MainWindow (argv[0] ignorato)");

	// Un argomento che non esiste (percorso inventato, mai un file
	// vero) va ignorato in silenzio, non trattato come un errore fatale
	// -- stesso principio permissivo gia' usato per un formato senza
	// translator installato: nessuna finestra nuova per QUESTO
	// argomento, ma neppure un crash o un blocco dell'intera chiamata.
	{
		char arg0[] = "/Magazzino/Atomo123/ui/Atomo123";
		char argBad[] = "/tmp/questo_file_non_esiste_davvero_12345.ascd";
		char* argv[] = { arg0, argBad };
		app.ArgvReceived(2, argv);
	}
	Check(CountMainWindows(&app) == 1,
		"un argomento che punta a un file inesistente viene ignorato, nessuna finestra in piu'");

	// Piu' argomenti in una sola chiamata (come Tracker che passa piu'
	// ref in un solo B_REFS_RECEIVED): il primo riusa la finestra
	// "vergine" gia' aperta sopra (il suo B_REFS_RECEIVED e' solo in
	// coda, mai ancora elaborato -- IsUntouched() e' ancora vero, non
	// e' un bug del test, e' la stessa asincronia gia' documentata in
	// test_multiwindow.cpp), il secondo ne apre una davvero nuova.
	{
		char arg0[] = "/Magazzino/Atomo123/ui/Atomo123";
		char arg1[] = "/tmp/test_argv_open_1.ascd";
		char arg2[] = "/tmp/test_argv_open_2.ascd";
		char* argv[] = { arg0, arg1, arg2 };
		app.ArgvReceived(3, argv);
	}
	Check(CountMainWindows(&app) == 2,
		"due argomenti validi in una sola chiamata: il primo riusa la finestra vergine, il secondo ne apre una nuova");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
