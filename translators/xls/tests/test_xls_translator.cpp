/*
	test_xls_translator.cpp

	Test del translator XLS legacy: Identify() (firma OLE2) piu' un
	test end-to-end di importazione (Translate()) sulle stringhe
	condivise BIFF8 (SST/LABELSST, vedi il commento su ReadSST in
	Excel.pass1.cpp) tramite due fixture generate con la libreria
	Python xlwt (licenza BSD, nessun problema di ridistribuzione --
	colma il gap lasciato dalla versione precedente di questo file, che
	non poteva includere il file .xls reale usato per la verifica
	manuale originale per licenza non chiara). Le fixture NON
	provengono da un file utente reale.

	Verificato anche manualmente con file .xls reali di un utente: sia
	a livello di translator (Identify/Translate) sia aprendolo dal
	vivo nell'app vera (Atomo123 file.xls) -- ha fatto emergere sei
	bug reali nel lettore BIFF/OLE2 legacy (engine/src/Excel/), tutti
	corretti, piu' la mancanza di SST/LABELSST (BIFF8/Excel97+, testata
	qui) che perdeva silenziosamente quasi tutto il testo nei file .xls
	moderni. Dettaglio completo in docs/TRANSLATORS.md e ROADMAP.md.
*/

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include <Application.h>
#include <DataIO.h>
#include <File.h>

#include "XlsTranslator.h"

static int gFailures = 0;

static void Check(bool condition, const char *what)
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
	BApplication app("application/x-vnd.Atomo-TestXlsTranslator");

	BTranslator *translator = make_nth_translator(0, 0, 0);
	Check(translator != NULL, "make_nth_translator crea il translator");

	// Dati chiaramente non-OLE2: Identify deve rifiutarli.
	const char *notXls = "questo non e' un file XLS";
	BMemoryIO notXlsIn(notXls, strlen(notXls));
	translator_info info;
	status_t err = translator->Identify(&notXlsIn, NULL, NULL, &info, 0);
	Check(err != B_OK, "Identify rifiuta dati senza firma OLE2");

	// Firma OLE2 valida seguita da dati non significativi: Identify
	// deve riconoscere il formato (anche se poi Translate() fallirebbe
	// nel parsing BIFF vero e proprio, che richiede un file reale).
	unsigned char ole2Header[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
	char fakeXls[64];
	memcpy(fakeXls, ole2Header, 8);
	memset(fakeXls + 8, 0, sizeof(fakeXls) - 8);
	BMemoryIO fakeXlsIn(fakeXls, sizeof(fakeXls));

	err = translator->Identify(&fakeXlsIn, NULL, NULL, &info, 0);
	Check(err == B_OK, "Identify riconosce la firma OLE2");
	Check(info.type == kAtomoXlsFormat, "Identify classifica il tipo come XLS");

	// Translate() su un OLE2 con firma valida ma contenuto BIFF non
	// significativo deve fallire in modo pulito (eccezione catturata,
	// nessun blocco), non necessariamente produrre dati corretti: qui
	// si verifica solo la robustezza, non l'interpretazione reale.
	fakeXlsIn.Seek(0, SEEK_SET);
	BMallocIO ascdOut;
	err = translator->Translate(&fakeXlsIn, &info, NULL, kAtomoNativeFormat, &ascdOut);
	Check(err != B_OK, "Translate su OLE2 senza contenuto BIFF valido fallisce in modo pulito (non si blocca)");

	// Stringhe condivise BIFF8 (SST/LABELSST): tests/sample_sst.xls ha
	// A1/B1 ("Ciao mondo"/"Seconda stringa"), A2/A3 duplicate
	// ("Ripetuta", deve deduplicare in un'unica voce SST ma comparire
	// in entrambe le celle), B2 (42, un numero puro che non passa da
	// SST) e A4 una stringa lunga (oltre 250 caratteri, entro un solo
	// record SST) -- generata con xlwt (Python, licenza BSD), non un
	// file utente reale.
	{
		BFile sstFile("tests/sample_sst.xls", B_READ_ONLY);
		Check(sstFile.InitCheck() == B_OK, "apertura di tests/sample_sst.xls riuscita");

		translator_info sstInfo;
		status_t sstErr = translator->Identify(&sstFile, NULL, NULL, &sstInfo, 0);
		Check(sstErr == B_OK, "Identify riconosce sample_sst.xls");

		sstFile.Seek(0, SEEK_SET);
		BMallocIO sstOut;
		sstErr = translator->Translate(&sstFile, &sstInfo, NULL, kAtomoNativeFormat, &sstOut);
		Check(sstErr == B_OK, "Translate di sample_sst.xls riesce");

		if (sstErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)sstOut.Buffer();
			size_t len = sstOut.BufferLength();
			Check(len > 12 && memcmp(data, "ASCD", 4) == 0,
				"l'output di Translate di sample_sst.xls e' un ASCD valido");

			int32 count = 0;
			if (len > 12)
				memcpy(&count, data + 8, 4);
			Check(count == 6, "l'ASCD contiene le 6 celle di sample_sst.xls");

			bool foundA1 = false, foundB1 = false, foundA2 = false, foundA3 = false,
				foundB2 = false, foundA4Long = false;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= len; i++)
			{
				short row, col;
				int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len)
					break;
				std::string text((const char *)data + pos, l);
				pos += l;

				if (row == 1 && col == 1 && text == "Ciao mondo") foundA1 = true;
				if (row == 1 && col == 2 && text == "Seconda stringa") foundB1 = true;
				if (row == 2 && col == 1 && text == "Ripetuta") foundA2 = true;
				if (row == 3 && col == 1 && text == "Ripetuta") foundA3 = true;
				if (row == 2 && col == 2 && text == "42") foundB2 = true;
				if (row == 4 && col == 1 && text.size() > 250
					&& text.compare(0, 13, "Testo lungo: ") == 0)
					foundA4Long = true;
			}

			Check(foundA1, "A1 (\"Ciao mondo\", SST/LABELSST) importato correttamente");
			Check(foundB1, "B1 (\"Seconda stringa\", SST/LABELSST) importato correttamente");
			Check(foundA2 && foundA3,
				"A2/A3 (stessa stringa \"Ripetuta\", una sola voce SST) importate in entrambe le celle");
			Check(foundB2, "B2 (42, numero puro, non SST) resta un numero, non tocca fSST");
			Check(foundA4Long, "A4 (stringa lunga oltre 250 caratteri) importata per intero, non troncata");
		}
	}

	// Stessa cosa ma con una SST abbastanza grande (400 stringhe unite
	// oltre 12KB) da superare la dimensione massima di un record BIFF
	// (~8224 byte) e proseguire in uno o piu' record CONTINUE -- vedi
	// il commento su ReadSST in Excel.pass1.cpp per la stranezza BIFF8
	// per cui un CONTINUE che interrompe una stringa a meta' ricomincia
	// con un byte "grbit" tutto suo. Le stringhe sono generate con lo
	// stesso schema deterministico usato per costruire la fixture
	// ("Voce numero %04d di prova lunga", indice 0..399), cosi' il
	// test puo' ricostruirsi l'elenco atteso senza un file a parte.
	{
		BFile largeFile("tests/sample_sst_large.xls", B_READ_ONLY);
		Check(largeFile.InitCheck() == B_OK, "apertura di tests/sample_sst_large.xls riuscita");

		translator_info largeInfo;
		status_t largeErr = translator->Identify(&largeFile, NULL, NULL, &largeInfo, 0);
		Check(largeErr == B_OK, "Identify riconosce sample_sst_large.xls");

		largeFile.Seek(0, SEEK_SET);
		BMallocIO largeOut;
		largeErr = translator->Translate(&largeFile, &largeInfo, NULL, kAtomoNativeFormat, &largeOut);
		Check(largeErr == B_OK, "Translate di sample_sst_large.xls riesce");

		if (largeErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)largeOut.Buffer();
			size_t len = largeOut.BufferLength();
			int32 count = 0;
			if (len > 12)
				memcpy(&count, data + 8, 4);
			Check(count == 400, "l'ASCD contiene le 400 celle di sample_sst_large.xls");

			std::set<std::string> got;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= len; i++)
			{
				short row, col;
				int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len)
					break;
				got.insert(std::string((const char *)data + pos, l));
				pos += l;
			}

			int missing = 0;
			for (int i = 0; i < 400; i++)
			{
				char expected[64];
				snprintf(expected, sizeof(expected), "Voce numero %04d di prova lunga", i);
				if (got.find(expected) == got.end())
					missing++;
			}
			Check(missing == 0,
				"tutte le 400 stringhe di sample_sst_large.xls sopravvivono, attraversando i record CONTINUE della SST");
		}
	}

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
