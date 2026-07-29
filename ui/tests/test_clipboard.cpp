/*
	test_clipboard.cpp

	Verifica che la logica di copia/incolla di MainWindow (be_clipboard
	Lock/Clear/Data/AddData/Commit per scrivere, Lock/Data/FindData per
	leggere) usi davvero gli appunti di sistema, non un buffer privato
	dell'app: senza argomenti fa un giro di scrittura+lettura
	autocontenuto; con "--write <testo>" scrive e basta, con "--read"
	legge e stampa soltanto -- cosi' si puo' incrociare con il tool a
	riga di comando "clipboard" di Haiku per provare l'interoperabilita'
	reale con il resto del sistema (vedi ROADMAP.md/docs/UI_ARCHITECTURE.md).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Clipboard.h>
#include <Message.h>

static bool WriteClipboard(const char* text)
{
	if (!be_clipboard->Lock())
		return false;

	be_clipboard->Clear();
	BMessage* clip = be_clipboard->Data();
	bool ok = clip && clip->AddData("text/plain", B_MIME_TYPE, text, strlen(text)) == B_OK;
	if (ok)
		ok = be_clipboard->Commit() == B_OK;
	be_clipboard->Unlock();
	return ok;
}

static bool ReadClipboard(char* outText, size_t maxLen)
{
	if (!be_clipboard->Lock())
		return false;

	BMessage* clip = be_clipboard->Data();
	const char* text = NULL;
	ssize_t len = 0;
	bool found = clip && clip->FindData("text/plain", B_MIME_TYPE,
		(const void**)&text, &len) == B_OK;

	if (found)
	{
		if ((size_t)len >= maxLen)
			len = maxLen - 1;
		memcpy(outText, text, len);
		outText[len] = 0;
	}

	be_clipboard->Unlock();
	return found;
}

int main(int argc, char** argv)
{
	BApplication app("application/x-vnd.Atomo-TestClipboard");

	if (argc == 3 && strcmp(argv[1], "--write") == 0)
	{
		bool ok = WriteClipboard(argv[2]);
		printf(ok ? "OK\n" : "FAIL\n");
		return ok ? 0 : 1;
	}

	if (argc == 2 && strcmp(argv[1], "--read") == 0)
	{
		char text[1024];
		if (ReadClipboard(text, sizeof(text)))
		{
			printf("%s\n", text);
			return 0;
		}
		fprintf(stderr, "FAIL (nessun testo negli appunti)\n");
		return 1;
	}

	int gFailures = 0;
	const char* sample = "Ciao Atomo123 clipboard test";

	bool wrote = WriteClipboard(sample);
	printf(wrote ? "OK   scrittura sugli appunti di sistema riuscita\n"
		: "FAIL scrittura sugli appunti di sistema riuscita\n");
	if (!wrote) gFailures++;

	char readBack[1024];
	bool read = ReadClipboard(readBack, sizeof(readBack));
	printf(read ? "OK   lettura dagli appunti di sistema riuscita\n"
		: "FAIL lettura dagli appunti di sistema riuscita\n");
	if (!read) gFailures++;

	bool matches = read && strcmp(readBack, sample) == 0;
	printf(matches ? "OK   il testo letto corrisponde a quello scritto\n"
		: "FAIL il testo letto corrisponde a quello scritto\n");
	if (!matches) gFailures++;

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
