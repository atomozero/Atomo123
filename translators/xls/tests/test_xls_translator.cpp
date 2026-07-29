/*
	test_xls_translator.cpp

	Test minimo del translator XLS legacy: verifica che Identify()
	riconosca correttamente la firma OLE2 e rifiuti dati che non ce
	l'hanno. Un test di importazione end-to-end (Translate() su un
	file .xls reale, verifica dei valori/formule importati) richiede
	un file di esempio autentico generato da Excel o LibreOffice: non
	incluso qui, va aggiunto quando se ne ha uno a disposizione (vedi
	nota "Test di congruita'" nella Fase 3 di ROADMAP.md).
*/

#include <cstdio>
#include <cstring>

#include <DataIO.h>

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

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	printf("NOTA: questo test copre solo il riconoscimento del formato (Identify).\n");
	printf("Manca ancora un test di importazione end-to-end con un file .xls reale.\n");
	return gFailures == 0 ? 0 : 1;
}
