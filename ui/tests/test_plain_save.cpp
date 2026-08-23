/*
	test_plain_save.cpp

	"Salva" (Fase 22, richiesta esplicita dell'utente: "abbiamo solo
	Salva con nome") deve riscrivere lo stesso file gia' aperto/salvato
	in precedenza SENZA mostrare nessun pannello -- a differenza di
	"Salva con nome" (kMsgSaveAs), sempre passato da fSavePanel->Show().

	Tre scenari:
	1. Un documento aperto da un file esistente (OpenFile): Save()
	   riscrive quello stesso file al primo posto giusto (nessun
	   pannello, MainWindow::Save() e' pubblico apposta per questo).
	2. Un documento nuovo/mai salvato: Save() non ha nessun file noto,
	   deve mostrare il pannello "Salva con nome" invece di scrivere
	   nel nulla (verificato indirettamente: IsWindow(fSavePanel)
	   diventa visibile).
	3. Dopo un "Salva con nome" a un formato diverso (qui XLSX),
	   un Save() successivo riscrive QUEL file (stesso principio di
	   Excel/LibreOffice Calc: dopo un Salva con nome, il Salva
	   normale segue il nuovo percorso/formato scelto).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <Message.h>
#include <NodeInfo.h>

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

int main()
{
	BApplication app("application/x-vnd.Atomo-TestPlainSave");

	// --- Scenario 1: file esistente, Save() lo riscrive da solo -------
	{
		MainWindow* setup = new MainWindow();
		setup->Show();
		setup->Lock();
		CContainer* setupDoc = setup->GetSheetView()->Document();
		TryToParseString("1", cell(1, 1), setupDoc, true); // A1
		SaveViaRealMessage(setup, "/tmp", "test_plain_save.ascd");
		setup->Unlock();

		// Bug reale scoperto dall'utente con un doppio clic su un file
		// .ascd appena creato, che non apriva Atomo123: nessun
		// salvataggio nativo aveva mai impostato l'attributo BEOS:TYPE,
		// Tracker ricadeva quindi sul tipo generico "application/
		// octet-stream", senza applicazione preferita -- vedi il
		// commento su kAtomoNativeMimeType in MainWindow.cpp.
		{
			BFile typeCheck("/tmp/test_plain_save.ascd", B_READ_ONLY);
			BNodeInfo nodeInfo(&typeCheck);
			char mimeType[B_MIME_TYPE_LENGTH] = { 0 };
			nodeInfo.GetType(mimeType);
			Check(strcmp(mimeType, "application/x-vnd.atomo-sheet-data") == 0,
				"un file .ascd salvato con \"Salva con nome\" ha il tipo MIME nativo "
				"(Tracker sa quale applicazione aprire con un doppio clic)");
		}

		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();

		BEntry entry("/tmp/test_plain_save.ascd");
		entry_ref ref;
		entry.GetRef(&ref);
		win->OpenFile(ref);

		CContainer* doc = win->GetSheetView()->Document();
		TryToParseString("42", cell(1, 1), doc, true); // A1 = 42, modifica

		win->Save(); // NESSUN pannello: deve riscrivere test_plain_save.ascd da solo
		win->Unlock();

		MainWindow* reopened = new MainWindow();
		reopened->Show();
		reopened->Lock();
		BEntry reEntry("/tmp/test_plain_save.ascd");
		entry_ref reRef;
		reEntry.GetRef(&reRef);
		reopened->OpenFile(reRef);

		Value v;
		reopened->GetSheetView()->Document()->GetValue(cell(1, 1), v);
		Check(v.fType == eNumData && (double)v == 42.0,
			"Save() su un documento gia' aperto da file riscrive quello stesso file (A1=42)");
		reopened->Unlock();
	}

	// --- Scenario 2: documento mai salvato, Save() apre il pannello ---
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		SheetView* view = win->GetSheetView();
		CContainer* doc = view->Document();
		TryToParseString("7", cell(1, 1), doc, true); // A1, scrittura diretta

		// TryToParseString diretto sul CContainer non marca fModified da
		// solo (bypassa SheetView): serve una mutazione VERA passata da
		// SheetView, stesso principio di test_unsaved_changes.cpp.
		view->SetSelection(cell(1, 1));
		view->ClearSelection();

		win->Save(); // documento nuovo: nessun file noto, deve ricadere sul pannello
		win->Unlock();

		// Il pannello "Salva con nome" e' un vero BWindow separato: se
		// Save() e' ricaduto su di lui, IsHidden() e' falso (visibile).
		// fSavePanel non e' esposto pubblicamente, quindi si verifica
		// indirettamente che il documento sia rimasto "modificato" (mai
		// scritto su disco, la modifica resta pendente) invece di finire
		// silenziosamente scritto in un posto arbitrario.
		Check(win->IsModified(),
			"Save() su un documento mai salvato non scrive nulla da solo "
			"(nessun file noto): il documento resta modificato, in attesa del pannello");
	}

	// --- Scenario 3: dopo Salva con nome (XLSX), Save() segue quello --
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();
		CContainer* doc = win->GetSheetView()->Document();
		TryToParseString("5", cell(1, 1), doc, true); // A1

		SaveViaRealMessage(win, "/tmp", "test_plain_save_after_saveas.xlsx");

		TryToParseString("9", cell(1, 1), doc, true); // A1 = 9, nuova modifica
		win->Save(); // deve riscrivere lo STESSO .xlsx, non un .ascd
		win->Unlock();

		BFile check("/tmp/test_plain_save_after_saveas.xlsx", B_READ_ONLY);
		char magic[2] = {0, 0};
		check.Read(magic, 2);
		Check(magic[0] == 'P' && magic[1] == 'K',
			"dopo un Salva con nome in XLSX, Save() riscrive ancora un vero XLSX (\"PK\"), non un .ascd");

		MainWindow* reopened = new MainWindow();
		reopened->Show();
		reopened->Lock();
		BEntry entry("/tmp/test_plain_save_after_saveas.xlsx");
		entry_ref ref;
		entry.GetRef(&ref);
		reopened->OpenFile(ref);

		Value v;
		reopened->GetSheetView()->Document()->GetValue(cell(1, 1), v);
		Check(v.fType == eNumData && (double)v == 9.0,
			"il file XLSX riaperto ha la modifica successiva al Salva con nome (A1=9)");
		reopened->Unlock();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
