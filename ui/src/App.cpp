/*
	App.cpp

	Vedi App.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "App.h"
#include "MainWindow.h"
#include "SplashWindow.h"

#include <Entry.h>
#include <MessageRunner.h>
#include <Mime.h>
#include <Path.h>
#include <Roster.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "FileTypeIcons.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "Preferences.h"
#include "ResourceManager.h"

static const char* kAppSignature = "application/x-vnd.Atomo-Atomo123";

static const uint32 kMsgShowMainWindow = 'shmw';
// Quanto restare sullo splash prima che compaia la MainWindow (chiesto
// dall'utente): lo splash stesso resta a schermo piu' a lungo di
// cosi' (AtomGLView::kVisibleDuration + kFadeOutDuration, circa 8s) e
// continua la propria animazione/dissolvenza per conto suo, la
// MainWindow compare sotto di lui a meta' di quella durata.
static const bigtime_t kSplashDelay = 3000000; // 3s

// Stessi tipi elencati nella risorsa file_types di Atomo123.rdef (che
// serve solo a farli comparire nella lista "Apri con..." di Tracker):
// qui si decide invece se diventare l'applicazione preferita, cioe'
// quella scelta automaticamente al doppio clic.
static const char* kSupportedTypes[] = {
	"application/x-vnd.atomo-sheet-data", // .ascd nativo
	"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", // .xlsx
	"application/vnd.ms-excel.sheet.macroenabled.12", // .xlsm
	"application/vnd.ms-excel", // .xls
	"application/vnd.oasis.opendocument.spreadsheet", // .ods
	"text/csv",
	NULL
};

// Icona per tipo MIME (Fase 29, ultima voce del backlog v3.0
// "Consolidation": icone HVIF distinte per tipo di file in Tracker,
// vedi FileTypeIcons.h), stesso ordine/stessa lunghezza di
// kSupportedTypes sopra -- un array parallelo invece di una struct
// combinata per non dover toccare kSupportedTypes, gia' usato altrove
// (Atomo123.rdef elenca la stessa lista separatamente). .xlsx e .xlsm
// condividono l'icona generica con .xls: il catalogo HVIF scaricato
// (il set ufficiale Haiku, vedi FileTypeIcons.h) non ha un'icona
// "Excel" dedicata, essendo un formato proprietario Microsoft --
// limite noto, non un'omissione silenziosa.
static const IconData* kSupportedTypeIcons[] = {
	&kFileTypeIconAscd,     // .ascd nativo
	&kFileTypeIconGeneric,  // .xlsx
	&kFileTypeIconGeneric,  // .xlsm
	&kFileTypeIconGeneric,  // .xls
	&kFileTypeIconOds,      // .ods
	&kFileTypeIconCsv,      // .csv
};

App::App()
	:
	BApplication(kAppSignature),
	fSplashWindow(NULL),
	fShowMainWindowTimer(NULL)
{
	// gPrefs (Preferences.h) e' dichiarata nel motore ma non era mai
	// stata istanziata da nessuna parte della UI moderna -- restava
	// sempre NULL (bug scoperto in Fase 7 implementando la finestra
	// Preferenze). ReadPrefFile() lancia se il file non esiste ancora
	// (prima esecuzione, mai salvata nessuna preferenza): non e' un
	// errore, si parte semplicemente dai valori predefiniti di
	// ciascuna GetPref*() (gia' gestito da CPreferences stessa).
	gPrefs = new CPreferences("Atomo123_prefs");
	try
	{
		gPrefs->ReadPrefFile();
	}
	catch (CErr&)
	{
	}

	gDecimalPoint = gPrefs->GetPrefString("decimalSeparator", ".")[0];
	gListSeparator = gPrefs->GetPrefString("listSeparator", ";")[0];

	// gThousandSeparator/gCurrencySymbol (Globals.h/Formatter.cpp): come
	// sopra, mai lette da gPrefs prima d'ora -- restavano ai valori
	// scritti a mano in EngineGlobals.cpp/Formatter.cpp (bug scoperto
	// implementando la preferenza "Simbolo valuta"). Usate da
	// CContainer::GetCellResult (export CSV, elenco valori univoci del
	// filtro automatico), non dalla griglia a schermo (quella segue il
	// Locale Kit di Haiku via SheetView::fNumberFormat, indipendente da
	// questa preferenza per design -- coerente con le altre app native).
	gThousandSeparator = gPrefs->GetPrefString("thousandSeparator", ",")[0];
	// 32: stessa dimensione di gCurrencySymbol[32] in Formatter.cpp --
	// Globals.h dichiara l'array senza dimensione ("extern char
	// gCurrencySymbol[];"), quindi sizeof() non e' utilizzabile qui.
	strlcpy(gCurrencySymbol, gPrefs->GetPrefString("currencySymbol", gCurrencySymbol), 32);
}

App::~App()
{
	delete fShowMainWindowTimer;
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

	RegisterFileTypes();

	// Preferenza "Mostra lo splash screen all'avvio" (finestra
	// Preferenze, sezione Avvio): se disattivata, si salta sia lo
	// splash sia l'attesa di kSplashDelay -- chi lo disabilita lo fa
	// apposta per un avvio piu' rapido, aspettare comunque non avrebbe
	// senso.
	bool showSplash = gPrefs ? (gPrefs->GetPrefInt("showSplash", 1) != 0) : true;
	if (showSplash)
	{
		fSplashWindow = new SplashWindow();
		fSplashWindow->Show();

		// La MainWindow compare solo dopo kSplashDelay (vedi
		// MessageReceived/ShowMainWindowIfNeeded) -- MA se un file era
		// gia' pronto all'avvio (RefsReceived puo' arrivare prima o dopo
		// ReadyToRun, ordine non garantito, comportamento standard di
		// BApplication), quella finestra puo' comparire prima del timer:
		// ShowMainWindowIfNeeded() lo gestisce comunque, vedi il
		// commento li'.
		BMessage msg(kMsgShowMainWindow);
		fShowMainWindowTimer = new BMessageRunner(BMessenger(this), &msg, kSplashDelay, 1);
	}
	else
	{
		ShowMainWindowIfNeeded();
	}
}

void App::MessageReceived(BMessage* message)
{
	if (message->what == kMsgShowMainWindow)
	{
		ShowMainWindowIfNeeded();
		return;
	}

	BApplication::MessageReceived(message);
}

void App::ShowMainWindowIfNeeded()
{
	// Si cerca esplicitamente una MainWindow (non CountWindows() puro)
	// perche' lo SplashWindow e' gia' una finestra dell'app ma non va
	// mai scambiato per quella -- e RefsReceived puo' averne gia'
	// creata una nel frattempo (vedi il commento in ReadyToRun).
	bool hasMainWindow = false;
	for (int32 i = 0; i < CountWindows(); i++)
	{
		if (dynamic_cast<MainWindow*>(WindowAt(i)))
		{
			hasMainWindow = true;
			break;
		}
	}
	if (!hasMainWindow)
	{
		MainWindow* window = new MainWindow();
		window->Show();
	}

	// Show()/l'attivazione della MainWindow appena sopra la porta
	// davanti allo splash (B_NORMAL_WINDOW_FEEL per entrambe, vedi il
	// commento in SplashWindow.cpp sul perche' non e' B_FLOATING_APP_
	// WINDOW_FEEL): riattivare lo splash qui lo rimette in primo piano,
	// dove resta finche' non si chiude da solo (AtomGLView::_Tick()) --
	// ancora vivo di sicuro a questo punto, la sua durata totale (circa
	// 8s) e' piu' lunga di kSplashDelay (3s).
	if (fSplashWindow)
		fSplashWindow->Activate();
}

void App::RegisterFileTypes()
{
	for (int i = 0; kSupportedTypes[i]; i++)
	{
		BMimeType type(kSupportedTypes[i]);
		if (type.InitCheck() != B_OK)
			continue;

		// Installa il tipo nel database MIME se non c'e' gia' (un
		// pacchetto XLSX/ODS/CSV aperto per la prima volta su questo
		// sistema potrebbe non averlo ancora): senza questo passo
		// SetPreferredApp sotto fallirebbe silenziosamente su un tipo
		// sconosciuto al database.
		if (!type.IsInstalled())
			type.Install();

		char preferred[B_MIME_TYPE_LENGTH];
		// GetPreferredApp fallisce (o restituisce una stringa vuota)
		// se il tipo non ha ancora nessuna applicazione preferita:
		// solo in quel caso si imposta Atomo123, senza mai scavalcare
		// una scelta gia' fatta dall'utente o da un'altra
		// applicazione installata.
		if (type.GetPreferredApp(preferred) != B_OK || preferred[0] == 0)
			type.SetPreferredApp(kAppSignature);

		// Icona per tipo di file (Fase 29): stesso principio "non
		// scavalcare mai una scelta gia' fatta" di SetPreferredApp
		// sopra -- un'icona gia' presente (magari impostata da
		// un'altra applicazione, es. LibreOffice, o dall'utente a
		// mano) resta quella che c'era. Niente HasIcon() nell'API di
		// Haiku: GetIcon(uint8**, size_t*) e' l'unico modo per
		// scoprirlo, B_OK solo se un'icona vettoriale esiste gia' --
		// il buffer restituito va liberato subito, serve solo per il
		// controllo, non per il contenuto.
		const IconData* icon = kSupportedTypeIcons[i];
		if (icon && icon->bytes)
		{
			uint8* existing = NULL;
			size_t existingSize = 0;
			if (type.GetIcon(&existing, &existingSize) != B_OK)
				type.SetIcon(icon->bytes, icon->length);
			else
				free(existing);
		}
	}
} // App::RegisterFileTypes

void App::RefsReceived(BMessage* message)
{
	// BApplication e BWindow girano su thread (BLooper) distinti: non si
	// puo' toccare direttamente le BView di una finestra da qui senza il
	// suo lock. Si inoltra invece un B_REFS_RECEIVED per ogni ref alla
	// finestra scelta (nuova o riusata sotto): MainWindow::MessageReceived
	// gestisce gia' quel messaggio chiamando OpenFile sul thread corretto,
	// con il lock preso automaticamente dal message loop.
	//
	// Una finestra con un documento gia' aperto (anche solo aperto e mai
	// modificato) non va MAI rimpiazzata da un file successivo: se
	// l'utente ha gia' Atomo123 aperto e fa doppio clic su un secondo
	// file, vuole una finestra nuova, non perdere quella di prima.
	MainWindow* reusable = FindReusableWindow();

	entry_ref ref;
	for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
	{
		BMessage oneRef(B_REFS_RECEIVED);
		oneRef.AddRef("refs", &ref);

		MainWindow* target = reusable;
		if (target)
			// Usata: PostMessage e' asincrono, quindi lo stato della
			// finestra (fDocumentName) non si aggiorna subito -- se
			// questo messaggio contiene piu' ref (piu' file aperti in
			// un colpo solo da Tracker), i successivi devono sempre
			// aprire finestre nuove, non ricontrollare la stessa
			// finestra vergine gia' assegnata al primo file.
			reusable = NULL;
		else
		{
			target = new MainWindow();
			target->Show();
		}
		target->PostMessage(&oneRef);
	}
}

MainWindow* App::FindReusableWindow() const
{
	for (int32 i = 0; i < CountWindows(); i++)
	{
		MainWindow* window = dynamic_cast<MainWindow*>(WindowAt(i));
		if (window && window->IsUntouched())
			return window;
	}
	return NULL;
}

// tests/test_multiwindow.cpp compila una seconda copia di questo file
// (con ATOMO123_TEST_BUILD definita, vedi Makefile) per esercitare la
// vera App::RefsReceived con il proprio main() di test: senza questa
// esclusione i due main() confliggerebbero al link.
#ifndef ATOMO123_TEST_BUILD
int main()
{
	App app;
	app.Run();

	// _exit(), non "return 0": un bug pre-esistente del Locale Kit di
	// Haiku (non del codice di Atomo123, confermato riproducendolo
	// anche con una build di settimane fa) manda in crash
	// BPrivate::DefaultCatalog::~DefaultCatalog() -- chiamato dai
	// distruttori statici C++ che "return 0" innescherebbe tramite
	// __cxa_finalize/exit() -- ogni volta che il processo termina, con
	// un "General protection fault" che genera un report di crash su
	// ogni singola chiusura. _exit() salta quel giro di distruttori
	// statici del tutto (il sistema operativo recupera comunque tutta
	// la memoria del processo alla sua terminazione): nessuna perdita
	// di dati, dato che salvataggio file e preferenze avvengono gia'
	// esplicitamente PRIMA di questo punto (BFile diretto, mai
	// rimandato a un distruttore), non e' qualcosa da cui questo salto
	// dipenda.
	_exit(0);
}
#endif
