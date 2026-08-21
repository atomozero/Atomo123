/*
	test_autosave.cpp

	Salvataggio automatico (Fase 23, richiesta esplicita dell'utente):
	abilitato di default ogni 5 minuti, parte SOLO dopo che il
	documento e' stato salvato manualmente la prima volta (nessun file
	dove scrivere un backup prima di allora), e scrive sempre un file
	di BACKUP ("<nome>.bak") -- mai il documento originale, stesso
	principio del backup di AutoCAD.

	MainWindow::AutoSaveBackup()/IsAutoSaveArmed() sono pubblici apposta
	per essere testabili senza aspettare un vero BMessageRunner (stesso
	principio di Save() in test_plain_save.cpp) -- qui si chiama
	AutoSaveBackup() direttamente invece di aspettare il timer vero.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <Message.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"

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

static void SaveViaRealMessage(MainWindow* win, const char* dirPath, const char* fileName)
{
	BEntry dirEntry(dirPath);
	entry_ref dirRef;
	dirEntry.GetRef(&dirRef);

	BMessage msg(B_SAVE_REQUESTED);
	msg.AddRef("directory", &dirRef);
	msg.AddString("name", fileName);
	win->MessageReceived(&msg);
}

static bool FileExists(const char* path)
{
	BEntry entry(path);
	return entry.Exists();
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestAutoSave");

	BEntry("/tmp/test_autosave.ascd.bak").Remove();
	BEntry("/tmp/test_autosave.xlsx.bak").Remove();

	// --- Un documento nuovo/mai salvato: il timer non e' mai armato ---
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		Check(win->AutoSaveEnabled() && win->AutoSaveIntervalMinutes() == 5,
			"predefinito: salvataggio automatico abilitato, ogni 5 minuti");
		Check(!win->IsAutoSaveArmed(),
			"un documento nuovo/mai salvato non ha il timer armato (nessun file dove scrivere il backup)");

		CContainer* doc = win->GetSheetView()->Document();
		TryToParseString("1", cell(1, 1), doc, true);
		SheetView* view = win->GetSheetView();
		view->SetSelection(cell(1, 1));
		view->ClearSelection(); // marca fModified, vedi test_unsaved_changes.cpp

		win->AutoSaveBackup(); // nessun file noto: non deve scrivere nulla, non deve crashare
		Check(!FileExists("/tmp/nonexistent_dir_ok.bak"), "AutoSaveBackup() senza file noto non crasha");
		win->Unlock();
	}

	// --- Dopo il primo salvataggio manuale, il timer si arma da solo -
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		CContainer* doc = win->GetSheetView()->Document();
		TryToParseString("10", cell(1, 1), doc, true); // A1

		Check(!win->IsAutoSaveArmed(), "prima del primo Salva, il timer non e' ancora armato");

		SaveViaRealMessage(win, "/tmp", "test_autosave.ascd");
		Check(win->IsAutoSaveArmed(),
			"dopo il primo salvataggio manuale, il timer si arma da solo (nessuna azione esplicita)");

		// Nessuna modifica dall'ultimo salvataggio: AutoSaveBackup() non
		// deve scrivere nulla (niente da salvare, il backup precedente
		// -- se esistesse -- resterebbe gia' accurato).
		win->AutoSaveBackup();
		Check(!FileExists("/tmp/test_autosave.ascd.bak"),
			"AutoSaveBackup() senza modifiche pendenti non scrive nessun backup");

		// Una nuova modifica dopo il salvataggio: ORA il backup si scrive.
		// A1 diventa 20 (scrittura diretta, non marca fModified da sola,
		// vedi test_unsaved_changes.cpp); C1 e' la mutazione VERA che
		// marca fModified, su una cella diversa per non disfare A1.
		TryToParseString("20", cell(1, 1), doc, true);
		SheetView* view = win->GetSheetView();
		view->SetSelection(cell(3, 1));
		view->ClearSelection();

		win->AutoSaveBackup();
		Check(FileExists("/tmp/test_autosave.ascd.bak"),
			"AutoSaveBackup() con modifiche pendenti scrive \"<nome>.ascd.bak\"");

		// Il file ORIGINALE non e' mai toccato dal backup automatico: la
		// modifica ad A1 (20) NON deve comparire li', solo nel .bak.
		MainWindow* reopenedOriginal = new MainWindow();
		reopenedOriginal->Show();
		reopenedOriginal->Lock();
		BEntry origEntry("/tmp/test_autosave.ascd");
		entry_ref origRef;
		origEntry.GetRef(&origRef);
		reopenedOriginal->OpenFile(origRef);
		Value origValue;
		reopenedOriginal->GetSheetView()->Document()->GetValue(cell(1, 1), origValue);
		Check(origValue.fType == eNumData && (double)origValue == 10.0,
			"il file ORIGINALE resta quello dell'ultimo Salva vero (A1=10), il backup non lo tocca mai");
		reopenedOriginal->Unlock();

		// Il file di BACKUP invece ha la modifica non ancora salvata.
		MainWindow* reopenedBackup = new MainWindow();
		reopenedBackup->Show();
		reopenedBackup->Lock();
		BEntry backupEntry("/tmp/test_autosave.ascd.bak");
		entry_ref backupRef;
		backupEntry.GetRef(&backupRef);
		reopenedBackup->OpenFile(backupRef);
		Value backupValue;
		reopenedBackup->GetSheetView()->Document()->GetValue(cell(1, 1), backupValue);
		Check(backupValue.fType == eNumData && (double)backupValue == 20.0,
			"il file di BACKUP ha la modifica pendente (A1=20), non ancora salvata nell'originale");
		reopenedBackup->Unlock();

		win->Unlock();
	}

	// --- Disabilitare il salvataggio automatico ferma subito il timer -
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		SaveViaRealMessage(win, "/tmp", "test_autosave_disable.ascd");
		Check(win->IsAutoSaveArmed(), "banco di prova: timer armato dopo il salvataggio");

		win->HandlePreferencesRequest(true, '.', ';', 5, true, ',', "$", false, 5);
		Check(!win->IsAutoSaveArmed(),
			"disabilitare il salvataggio automatico nelle preferenze ferma subito il timer");

		win->HandlePreferencesRequest(true, '.', ';', 5, true, ',', "$", true, 5);
		Check(win->IsAutoSaveArmed(),
			"riabilitarlo lo riarma subito (il documento ha gia' un file noto)");

		win->Unlock();
	}

	// --- Il formato del backup segue quello del documento (XLSX) -----
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		CContainer* doc = win->GetSheetView()->Document();
		TryToParseString("5", cell(1, 1), doc, true);

		SaveViaRealMessage(win, "/tmp", "test_autosave.xlsx");

		TryToParseString("9", cell(1, 1), doc, true);
		SheetView* view = win->GetSheetView();
		view->SetSelection(cell(3, 1)); // C1, non A1: marca fModified senza disfare A1=9
		view->ClearSelection();

		win->AutoSaveBackup();
		win->Unlock();

		BFile backup("/tmp/test_autosave.xlsx.bak", B_READ_ONLY);
		char magic[2] = {0, 0};
		backup.Read(magic, 2);
		Check(magic[0] == 'P' && magic[1] == 'K',
			"il backup di un documento .xlsx e' anch'esso un vero XLSX (\"PK\"), non un .ascd");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
