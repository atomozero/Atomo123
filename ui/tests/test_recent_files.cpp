/*
	test_recent_files.cpp

	Verifica il menu File > "Apri recenti": elenco degli ultimi file
	aperti, persistito in gPrefs come 5 chiavi "recentFileN"
	(MainWindow::AddToRecentFiles/LoadRecentFiles/SaveRecentFiles).

	A differenza degli altri test UI (che lasciano gPrefs a NULL --
	vedi il commento in test_preferences.cpp) questo test imposta un
	vero CPreferences su un file di prova a parte: il valore di questa
	funzionalita' e' proprio la persistenza su disco, non testabile in
	modo significativo lasciando gPrefs NULL (in quel caso
	LoadRecentFiles/SaveRecentFiles restano no-op per progetto, cosi'
	da non far crashare i test esistenti che non se lo aspettano).

	Non copre il ramo "file non piu' disponibile" di kMsgOpenRecent:
	mostra un vero BAlert modale che bloccherebbe questo test in attesa
	di un clic reale, stesso limite gia' documentato in
	test_unsaved_changes.cpp per ConfirmDiscardChanges().
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <String.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "Preferences.h"
#include "SheetView.h"
#include "MainWindow.h"

static const uint32 kMsgOpenRecentForTest = 'aorc'; // deve combaciare con MainWindow.cpp

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

static void WriteTestFile(const char* path, const char* cellText)
{
	CContainer* doc = new CContainer(NULL, NULL);
	TryToParseString(cellText, cell(1, 1), doc, true);
	BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	SaveASCD(doc, &file);
	doc->Release();
}

// "/tmp" e' un link simbolico su Haiku (-> /boot/system/cache/tmp):
// BEntry::GetPath sul risultato di GetRef restituisce il percorso
// risolto, non il testo letterale passato in ingresso -- serve
// risolvere anche qui per confrontare le stesse stringhe che
// MainWindow::AddToRecentFiles scrive davvero in gPrefs.
static BString ResolvedPath(const char* path)
{
	entry_ref ref;
	BEntry(path).GetRef(&ref);
	BPath resolved;
	BEntry(&ref).GetPath(&resolved);
	return BString(resolved.Path());
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestRecentFiles");

	// gPrefs su un file di prova a parte, mai quello vero dell'utente
	// (Atomo123_prefs) -- ripulito prima di iniziare per una base
	// pulita indipendente da esecuzioni precedenti di questo test.
	BPath settingsDir;
	find_directory(B_USER_SETTINGS_DIRECTORY, &settingsDir, true);
	BString testPrefsPath(settingsDir.Path());
	testPrefsPath << "/test_recent_files_prefs";
	BEntry(testPrefsPath.String()).Remove();

	gPrefs = new CPreferences("test_recent_files_prefs");

	const char* path1 = "/tmp/test_recent_1.ascd";
	const char* path2 = "/tmp/test_recent_2.ascd";
	const char* path3 = "/tmp/test_recent_3.ascd";
	WriteTestFile(path1, "Uno");
	WriteTestFile(path2, "Due");
	WriteTestFile(path3, "Tre");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	entry_ref ref1, ref2, ref3;
	BEntry(path1).GetRef(&ref1);
	BEntry(path2).GetRef(&ref2);
	BEntry(path3).GetRef(&ref3);

	BString rp1 = ResolvedPath(path1);
	BString rp2 = ResolvedPath(path2);

	win->OpenFile(ref1);
	Check(BString(gPrefs->GetPrefString("recentFile0", "")) == rp1,
		"dopo la prima apertura, recentFile0 e' il file appena aperto");
	Check(strcmp(gPrefs->GetPrefString("recentFile1", ""), "") == 0,
		"recentFile1 resta vuoto dopo una sola apertura");

	win->OpenFile(ref2);
	Check(BString(gPrefs->GetPrefString("recentFile0", "")) == rp2,
		"dopo la seconda apertura, recentFile0 e' il secondo file (il piu' recente)");
	Check(BString(gPrefs->GetPrefString("recentFile1", "")) == rp1,
		"il primo file scorre a recentFile1");

	// Riaprire il primo file lo riporta in cima, senza duplicarlo (la
	// lista resta di 2 voci, non 3).
	win->OpenFile(ref1);
	Check(BString(gPrefs->GetPrefString("recentFile0", "")) == rp1,
		"riaprendo il primo file torna in cima all'elenco");
	Check(BString(gPrefs->GetPrefString("recentFile1", "")) == rp2,
		"il secondo file scorre a recentFile1");
	Check(strcmp(gPrefs->GetPrefString("recentFile2", ""), "") == 0,
		"nessuna terza voce: riaprire un file non lo duplica nell'elenco");

	// Cinque file distinti in piu' (oltre ai due gia' in elenco):
	// l'elenco resta troncato a 5, il piu' vecchio esce.
	const char* extra[5] = {
		"/tmp/test_recent_x1.ascd", "/tmp/test_recent_x2.ascd",
		"/tmp/test_recent_x3.ascd", "/tmp/test_recent_x4.ascd",
		"/tmp/test_recent_x5.ascd"
	};
	for (int i = 0; i < 5; i++)
	{
		WriteTestFile(extra[i], "X");
		entry_ref r;
		BEntry(extra[i]).GetRef(&r);
		win->OpenFile(r);
	}
	// Ordine atteso in cima (dal piu' recente): x5, x4, x3, x2, x1 --
	// path1/path2 sono usciti dall'elenco (troncato a 5 voci).
	Check(BString(gPrefs->GetPrefString("recentFile0", "")) == ResolvedPath(extra[4]),
		"dopo 5 aperture in piu' l'ultima resta recentFile0");
	Check(BString(gPrefs->GetPrefString("recentFile4", "")) == ResolvedPath(extra[0]),
		"la quinta voce piu' vecchia fra le nuove e' recentFile4, l'elenco resta a 5");
	bool path1Gone = true, path2Gone = true;
	for (int i = 0; i < 5; i++)
	{
		char key[16];
		sprintf(key, "recentFile%d", i);
		BString v(gPrefs->GetPrefString(key, ""));
		if (v == rp1) path1Gone = false;
		if (v == rp2) path2Gone = false;
	}
	Check(path1Gone && path2Gone,
		"i due file piu' vecchi escono dall'elenco quando si supera il limite di 5");

	// Riapertura tramite il messaggio del menu "Apri recenti" (invece
	// di MainWindow::OpenFile diretto, come sopra): stessa strada
	// percorsa da un vero clic su una voce del menu.
	{
		BMessage msg(kMsgOpenRecentForTest);
		msg.AddString("path", extra[0]); // il file con "X" in A1
		win->MessageReceived(&msg);

		char text[64];
		win->GetSheetView()->Document()->GetCellFormula(cell(1, 1), text, sizeof(text), false);
		Check(strcmp(text, "X") == 0,
			"selezionare una voce di \"Apri recenti\" riapre davvero quel file");
	}

	BEntry(testPrefsPath.String()).Remove();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
