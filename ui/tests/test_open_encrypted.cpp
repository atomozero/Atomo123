/*
	test_open_encrypted.cpp

	Verifica l'apertura di un vero workbook open-password (Agile
	Encryption, seconda meta' di "gestione completa delle password" --
	Path to full Excel parity, solo lettura) attraverso il VERO percorso
	interattivo: MainWindow::OpenFileAsync su
	translators/xlsx/tests/sample_encrypted.xlsx (password "test123",
	vedi il commento in cima a engine/tests/office_crypto_test.cpp per
	come e' stato generato). A differenza di quel test (che verifica solo
	l'algoritmo di decifratura in isolamento), questo copre l'intero giro:
	riconoscimento del contenitore OLE2/CFB cifrato in OpenFileAsync,
	comparsa del prompt della password (simulato con lo stesso principio
	di CompletePasswordDialog in test_cell_protection.cpp: un
	kMsgPasswordCommit inviato direttamente, senza toccare davvero la
	finestra/BTextControl), la decifratura in memoria, e il caricamento
	del vero contenuto attraverso il percorso NORMALE di OpenFileThreadEntry
	(BTranslatorRoster -> XlsxTranslator, come se non fosse mai stato
	cifrato).

	Niente BAlert::Go() qui: la password sbagliata E' testata (rifiuto
	pulito, nessun caricamento), ma SENZA passare dal vero comando 'prsh'
	della finestra reale -- si chiama OpenFileAsync/MessageReceived
	direttamente, quindi lo stesso avviso "Password errata" di
	MainWindow::MessageReceived (kMsgPasswordCommit, ramo file-open)
	scatterebbe comunque un vero BAlert::Go() bloccante. Per evitarlo
	senza saltare la verifica, il test della password sbagliata usa
	direttamente OfficeCrypto (stesso principio del "ramo negato" mai
	esercitato via messaggio in test_cell_protection.cpp) -- la parte
	VIA MESSAGGIO qui sotto copre solo il caso della password corretta.
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <Entry.h>
#include <File.h>
#include <OS.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "PasswordWindow.h"
#include "OfficeCrypto.h"

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
	BApplication app("application/x-vnd.Atomo-TestOpenEncrypted");

	const char* path = "../translators/xlsx/tests/sample_encrypted.xlsx";
	entry_ref ref;
	BEntry entry(path);
	Check(entry.Exists() && entry.GetRef(&ref) == B_OK,
		"il file di prova cifrato esiste e il suo entry_ref si ottiene correttamente");

	// Verifica indipendente (nessun MainWindow coinvolto) che la
	// password sbagliata sia rifiutata: copre lo stesso ramo che, dentro
	// MainWindow::MessageReceived, mostrerebbe un BAlert::Go() bloccante
	// -- vedi il commento in cima al file sul perche' questo pezzo non
	// passa dal vero messaggio.
	{
		BFile file(&ref, B_READ_ONLY);
		off_t size = 0;
		file.GetSize(&size);
		std::vector<uint8> data((size_t)size);
		file.Read(&data[0], (size_t)size);
		std::vector<uint8> plain;
		Check(!DecryptAgileEncryptedXlsx(&data[0], data.size(), "password sbagliata", &plain),
			"una password sbagliata viene rifiutata anche passando dal file reale su disco");
	}

	MainWindow* win = new MainWindow();
	win->Show();

	win->Lock();
	Check(!win->IsOpeningFile(), "IsOpeningFile() e' falso prima di qualunque apertura");
	win->OpenFileAsync(ref);
	// Un file cifrato NON avvia subito il thread di caricamento: prima
	// serve la password, quindi IsOpeningFile() deve restare falso finche'
	// non arriva un kMsgPasswordCommit valido (a differenza del caso non
	// cifrato di test_open_async.cpp, dove torna vero immediatamente).
	Check(!win->IsOpeningFile(),
		"OpenFileAsync su un file cifrato NON avvia il thread di caricamento subito "
		"(mostra prima il prompt della password)");

	// Simula la sottomissione della password corretta esattamente come
	// CompletePasswordDialog in test_cell_protection.cpp: un
	// kMsgPasswordCommit diretto, senza toccare PasswordWindow/BTextControl.
	BMessage commit(kMsgPasswordCommit);
	commit.AddBool("setMode", false);
	commit.AddString("password", "test123");
	win->MessageReceived(&commit);

	bool wasOpeningRightAfterCommit = win->IsOpeningFile();
	win->Unlock();
	Check(wasOpeningRightAfterCommit,
		"dopo una password corretta, il thread di caricamento parte (IsOpeningFile() torna vero)");

	bigtime_t start = system_time();
	bool finished = false;
	while (system_time() - start < 10000000) // 10s, ampio margine per un file di prova minuscolo
	{
		snooze(10000);
		win->Lock();
		finished = !win->IsOpeningFile();
		win->Unlock();
		if (finished)
			break;
	}
	Check(finished, "il caricamento del contenuto decifrato finisce entro il timeout di 10s");

	win->Lock();
	Check(win->SheetCount() == 1, "il workbook decifrato ha il suo unico foglio");
	if (win->SheetCount() == 1)
	{
		CContainer* doc = win->GetSheetView()->Document();
		Value v;
		doc->GetValue(cell(1, 1), v);
		Check(v.fType == eTextData && strcmp((const char*)v, "Segreto") == 0,
			"A1 del foglio decifrato vale davvero \"Segreto\" (il contenuto reale, non spazzatura)");
		doc->GetValue(cell(2, 1), v);
		Check(v.fType == eNumData && (double)v == 42,
			"B1 del foglio decifrato vale davvero 42");
	}
	win->Unlock();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
