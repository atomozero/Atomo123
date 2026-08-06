/*
	test_multiwindow.cpp

	Verifica che aprire un file mentre Atomo123 e' gia' in esecuzione con
	un documento apra una finestra NUOVA invece di rimpiazzarlo: prima di
	questo, App::RefsReceived inoltrava sempre B_REFS_RECEIVED alla
	stessa, unica finestra (fWindow), perdendo il documento gia' aperto
	(segnalato dall'utente dopo la Fase 9).

	Copre anche lo stesso identico bug per il percorso "drop diretto su
	una finestra" (trascinare un'icona di file da Tracker dentro una
	finestra di Atomo123 gia' aperta, non sull'icona dell'applicazione):
	MainWindow::MessageReceived gestiva B_REFS_RECEIVED chiamando sempre
	OpenFile() su se stessa, sostituendo un documento gia' aperto invece
	di aprirne uno nuovo (segnalato dall'utente, Fase 13) -- ora segue
	la stessa politica di App::RefsReceived (IsUntouched() decide se
	riusare la finestra o aprirne una nuova).

	Usa la vera classe App (non una BApplication generica) per esercitare
	la logica reale di selezione della finestra (App::RefsReceived/
	FindReusableWindow), non una sua reimplementazione qui -- il target
	test-multiwindow nel Makefile ricompila App.cpp con
	-DATOMO123_TEST_BUILD apposta per escluderne il main() (che
	confliggerebbe con quello di questo file) senza toccare il binario
	vero.

	Non chiama mai app.Run(): come le altre finestre di test in questo
	progetto (vedi test_multisheet.cpp), le finestre vengono manipolate
	direttamente (Lock/OpenFile/Unlock) sul thread del test. Per questo
	le uniche verifiche fatte sono quelle raggiungibili in modo
	deterministico:
	- il conteggio di MainWindow davvero aperte (CountMainWindows() sotto,
	  non BApplication::CountWindows() da sola: ogni MainWindow crea
	  anche due BFilePanel per "Apri"/"Salva con nome", che sono anch'essi
	  BWindow e si registrerebbero presso l'app gonfiando il conteggio --
	  scoperto proprio scrivendo questo test) per verificare quando viene
	  creata una finestra nuova e quando ne viene riusata una esistente.
	  Una BWindow si registra presso la BApplication nel proprio
	  costruttore, in modo sincrono, quindi il conteggio e' corretto
	  subito, senza bisogno di app.Run();
	- il contenuto della finestra il cui documento e' stato aperto con
	  una chiamata diretta a OpenFile() (sincrona), non tramite
	  PostMessage/B_REFS_RECEIVED (che verrebbe elaborato in modo
	  asincrono sul thread della finestra, quindi non e' possibile
	  controllarne l'esito subito dopo senza introdurre una corsa fra
	  thread nel test).
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

// Prima MainWindow diversa da "known" fra quelle registrate presso app.
static MainWindow* OtherMainWindow(BApplication* app, MainWindow* known)
{
	for (int32 i = 0; i < app->CountWindows(); i++)
	{
		MainWindow* w = dynamic_cast<MainWindow*>(app->WindowAt(i));
		if (w && w != known)
			return w;
	}
	return NULL;
}

int main()
{
	App app;

	const char* path1 = "/tmp/test_multiwindow_1.ascd";
	const char* path2 = "/tmp/test_multiwindow_2.ascd";

	CContainer* doc1 = new CContainer(NULL, NULL);
	TryToParseString("Primo file", cell(1, 1), doc1, true);
	{
		BFile file(path1, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(doc1, &file) == B_OK, "preparazione del primo file di prova riuscita");
	}
	doc1->Release();

	CContainer* doc2 = new CContainer(NULL, NULL);
	TryToParseString("Secondo file", cell(1, 1), doc2, true);
	{
		BFile file(path2, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		Check(SaveASCD(doc2, &file) == B_OK, "preparazione del secondo file di prova riuscita");
	}
	doc2->Release();

	entry_ref ref1, ref2;
	Check(BEntry(path1).GetRef(&ref1) == B_OK, "entry_ref del primo file di prova ottenuto correttamente");
	Check(BEntry(path2).GetRef(&ref2) == B_OK, "entry_ref del secondo file di prova ottenuto correttamente");

	Check(CountMainWindows(&app) == 0, "nessuna MainWindow prima di qualunque evento (app.Run() non e' mai chiamato)");

	// Una finestra col suo documento gia' aperto (chiamata diretta,
	// sincrona: nessuna corsa con l'elaborazione asincrona di
	// B_REFS_RECEIVED) -- simula "Atomo123 e' gia' aperto con un file".
	MainWindow* win1 = new MainWindow();
	win1->Show();
	win1->Lock();
	win1->OpenFile(ref1);
	win1->Unlock();
	Check(CountMainWindows(&app) == 1, "la finestra creata manualmente si registra da sola presso App");
	Check(!win1->IsUntouched(), "dopo OpenFile la finestra non e' piu' vergine");

	// Una singola MainWindow crea anche due BFilePanel (Apri/Salva),
	// anch'essi BWindow: BApplication::CountWindows() grezzo conta tutti
	// e tre, non solo la MainWindow. MainWindow::QuitRequested() deve
	// filtrare per tipo (come CountMainWindows() sopra) prima di decidere
	// se e' rimasta l'ultima finestra -- un CountWindows() grezzo
	// resterebbe sempre sopra 1 e l'app non terminerebbe mai chiudendo
	// l'ultima MainWindow (bug scoperto scrivendo questo test).
	Check(app.CountWindows() > CountMainWindows(&app),
		"CountWindows() grezzo (che include i BFilePanel) e' maggiore del conteggio filtrato delle sole MainWindow");

	// Lo scenario chiesto dall'utente: arriva un secondo file mentre
	// l'unica finestra esistente ha gia' un documento aperto -- deve
	// aprirne una NUOVA, non sostituire quella di prima.
	BMessage refs2(B_REFS_RECEIVED);
	refs2.AddRef("refs", &ref2);
	app.RefsReceived(&refs2);
	Check(CountMainWindows(&app) == 2,
		"un secondo file, a finestra gia' occupata, apre una finestra nuova invece di sostituirla");

	MainWindow* win2 = OtherMainWindow(&app, win1);
	Check(win2 != NULL && win2 != win1, "la nuova finestra e' un'istanza distinta dalla prima");

	// La prima finestra non deve aver perso il suo documento: e' il
	// sintomo diretto del comportamento segnalato ("perdere il primo
	// file aperto e aprirci sopra il secondo").
	win1->Lock();
	char text[512];
	win1->GetSheetView()->Document()->GetCellFormula(cell(1, 1), text, sizeof(text), false);
	Check(strcmp(text, "Primo file") == 0,
		"la prima finestra mostra ancora il suo documento originale dopo l'apertura del secondo file");
	win1->Unlock();

	// L'altra meta' della logica: una finestra "vergine" (mai toccata,
	// come quella creata da App::ReadyToRun all'avvio) viene invece
	// RIUSATA per il file successivo, non raddoppiata -- caso "avvio
	// con un file gia' pronto" descritto in App::ReadyToRun.
	MainWindow* win3 = new MainWindow();
	win3->Show();
	Check(CountMainWindows(&app) == 3, "una terza finestra vergine, creata a parte, si registra anch'essa");

	BMessage refs1Again(B_REFS_RECEIVED);
	refs1Again.AddRef("refs", &ref1);
	app.RefsReceived(&refs1Again);
	Check(CountMainWindows(&app) == 3,
		"un file in arrivo mentre esiste una finestra vergine la riusa, non ne apre una quarta");

	// Trascinare un file DIRETTAMENTE su una finestra gia' occupata
	// (non sull'icona dell'applicazione, gia' verificato sopra) deve
	// aprire una finestra nuova, non sostituire il documento della
	// finestra bersaglio -- stesso bug segnalato dall'utente
	// ("trascino un file dentro l'applicazione"), ma per il percorso
	// "drop diretto sulla finestra" (MainWindow::MessageReceived,
	// caso B_REFS_RECEIVED) invece di "drop sull'icona dell'app"
	// (App::RefsReceived, gia' testato sopra). MessageReceived
	// chiamato direttamente, non tramite PostMessage: stesso motivo
	// gia' spiegato in cima al file per app.RefsReceived (la
	// creazione della finestra e' sincrona, solo il caricamento del
	// file al suo interno resta asincrono).
	BMessage refs2OnWindow(B_REFS_RECEIVED);
	refs2OnWindow.AddRef("refs", &ref2);
	win1->Lock();
	win1->MessageReceived(&refs2OnWindow);
	win1->Unlock();
	Check(CountMainWindows(&app) == 4,
		"trascinare un file direttamente su una finestra gia' occupata apre una finestra nuova, non la sostituisce");

	win1->Lock();
	win1->GetSheetView()->Document()->GetCellFormula(cell(1, 1), text, sizeof(text), false);
	Check(strcmp(text, "Primo file") == 0,
		"la finestra su cui e' stato trascinato il file mantiene il proprio documento originale");
	win1->Unlock();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
