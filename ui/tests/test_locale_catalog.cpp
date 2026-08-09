/*
	test_locale_catalog.cpp

	Verifica il catalogo inglese incorporato nel binario (locales/en.catkeys,
	incorporato con "linkcatkeys -tr" -- vedi Makefile, target $(APP)):
	carica il catalogo direttamente dal file eseguibile con la lingua
	forzata a "en" (costruttore BCatalog(entry_ref, lingua)), invece di
	passare dalla lingua di sistema -- non serve un vero cambio di lingua
	dell'ambiente Haiku per eseguire questo test in automatico.

	Non e' un test esaustivo di tutte le ~250 chiavi (ci pensa il
	confronto riga per riga fra locales/it.catkeys e locales/en.catkeys
	fatto a mano quando si aggiungono nuove stringhe): controlla solo un
	campione che copre le classi di casi piu' delicate -- parole semplici,
	un B_TRANSLATE_COMMENT usato per disambiguare due significati diversi
	della stessa parola italiana ("Annulla" = Undo o Cancel a seconda del
	contesto), e una stringa con specificatori di formato (%s/%d/%g).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <cstring>

#include <Catalog.h>
#include <Entry.h>
#include <Path.h>

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
	// Nessuna vera BApplication: BCatalog(entry_ref, lingua) legge il
	// catalogo incorporato direttamente dal file indicato, senza
	// passare da BLocaleRoster::Default() (che dipenderebbe dalla
	// lingua di sistema effettiva) -- vedi il commento in cima al file.
	BEntry appEntry("Atomo123");
	entry_ref appRef;
	status_t err = appEntry.GetRef(&appRef);
	Check(err == B_OK, "il binario Atomo123 (con catalogo incorporato) esiste in questa directory");
	if (err != B_OK)
	{
		printf("Esegui questo test da ui/ dopo \"make\" (che incorpora locales/en.catkeys).\n");
		return 1;
	}

	BCatalog catalog(appRef, "en");
	Check(catalog.InitCheck() == B_OK, "il catalogo inglese incorporato si apre correttamente");

	Check(strcmp(catalog.GetString("Nuovo", "MainWindow"), "New") == 0,
		"\"Nuovo\" (MainWindow) traduce in \"New\"");
	Check(strcmp(catalog.GetString("Salva", "MainWindow"), "Save") == 0,
		"\"Salva\" (MainWindow) traduce in \"Save\"");
	Check(strcmp(catalog.GetString("Preferenze", "PreferencesWindow"), "Preferences") == 0,
		"\"Preferenze\" (PreferencesWindow) traduce in \"Preferences\"");
	Check(strcmp(catalog.GetString("Mostra lo splash screen all'avvio", "PreferencesWindow"),
			"Show the splash screen at startup") == 0,
		"la voce di preferenza dello splash screen traduce correttamente");

	// Stessa parola italiana "Annulla", due significati diversi a
	// seconda del contesto/commento (vedi il fix nella cronologia di
	// questo file per il bug reale corretto): senza il B_TRANSLATE_
	// COMMENT di disambiguazione, entrambe le chiamate avrebbero
	// condiviso un'unica voce di catalogo e una delle due traduzioni
	// sarebbe stata sbagliata.
	Check(strcmp(catalog.GetString("Annulla", "MainWindow"), "Undo") == 0,
		"\"Annulla\" (voce di menu Modifica) traduce in \"Undo\"");
	Check(strcmp(catalog.GetString("Annulla", "MainWindow",
			"Pulsante di annullamento in una finestra di conferma (\"Cancel\"), "
			"non la voce di menu Annulla/Undo"), "Cancel") == 0,
		"\"Annulla\" (pulsante di conferma, disambiguato dal commento) traduce in \"Cancel\"");

	// Stessa disambiguazione per l'indicatore di modalita' nel footer
	// ("Modifica" = Edit come voce di menu, "Editing" come stato).
	Check(strcmp(catalog.GetString("Modifica", "MainWindow"), "Edit") == 0,
		"\"Modifica\" (voce di menu) traduce in \"Edit\"");
	Check(strcmp(catalog.GetString("Modifica", "MainWindow",
			"Indicatore di modalita' nel footer: modifica di una cella in corso, "
			"non la voce di menu \"Modifica\""), "Editing") == 0,
		"\"Modifica\" (indicatore del footer, disambiguato dal commento) traduce in \"Editing\"");

	// Stringhe con specificatori di formato: la traduzione deve
	// conservare %s/%d/%g nell'ordine giusto, non solo il testo
	// circostante.
	Check(strcmp(catalog.GetString("%d cella/e sostituita/e.", "MainWindow"),
			"%d cell(s) replaced.") == 0,
		"il messaggio di Sostituisci tutto conserva %d nella traduzione");
	Check(strcmp(catalog.GetString("\"%s\" non è un numero compreso fra %g e %g.", "MainWindow"),
			"\"%s\" is not a number between %g and %g.") == 0,
		"il messaggio di errore della convalida numerica conserva %s/%g nella traduzione");

	if (gFailures == 0)
		printf("\nTUTTI I TEST SONO PASSATI\n");
	else
		printf("\n%d TEST FALLITI\n", gFailures);

	return gFailures == 0 ? 0 : 1;
}
