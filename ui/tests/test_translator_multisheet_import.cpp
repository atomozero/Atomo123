/*
	test_translator_multisheet_import.cpp

	Regressione per un bug reale (Fase 29): translators/xlsx/
	XlsxTranslator.cpp scrive un formato ASCD duplicato dal proprio (mai
	linkato contro ui/src/AscdIO.cpp, per non introdurre una dipendenza
	di link fra il translator e l'app) -- quando quel duplicato smette
	di essere allineato al formato vero letto da AscdIO.cpp::LoadASCD
	(qui, le sezioni area di stampa/impostazioni di stampa aggiunte in
	AscdIO.cpp ma dimenticate nel translator), un file XLSX con PIU' di
	un foglio smette di aprirsi: LoadASCDBook legge il primo foglio
	correttamente (l'EOF-tolleranza di LoadASCD nasconde la sezione
	mancante quando e' l'ultima cosa nello stream) ma disallinea la
	lettura di ogni foglio successivo, perche' quella stessa sezione
	mancante non e' piu' l'ultima cosa nello stream -- il foglio dopo
	viene letto a partire dai byte sbagliati.

	A differenza di ui/tests/test_multisheet.cpp e test_ascd_book.cpp
	(che costruiscono la cartella di lavoro chiamando direttamente
	SaveASCDBook/LoadASCDBook, mai il translator), questo test passa
	DAVVERO per il translator XLSX installato (BTranslatorRoster,
	stesso percorso di MainWindow::OpenFile) cosi' da rilevare un
	disallineamento fra i due formati duplicati -- l'unico modo per
	riprodurre davvero questo bug.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <File.h>
#include <TranslatorRoster.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"

static const uint32 kAtomoNativeFormat = 'ASCD';

static int gFailures = 0;

static void Check(bool condition, const char* description)
{
	printf("%s   %s\n", condition ? "OK" : "FAIL", description);
	if (!condition)
		gFailures++;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestTranslatorMultisheetImport");

	BFile srcFile("tests/sample_multisheet.xlsx", B_READ_ONLY);
	Check(srcFile.InitCheck() == B_OK, "apertura di tests/sample_multisheet.xlsx riuscita");

	BMallocIO ascd;
	BMessage ext;
	status_t translateErr = BTranslatorRoster::Default()->Translate(&srcFile, NULL, &ext, &ascd,
		kAtomoNativeFormat);
	Check(translateErr == B_OK, "Translate() del file XLSX a tre fogli riesce");

	ascd.Seek(0, SEEK_SET);
	bool isBook = IsASCDBookFile(&ascd);
	Check(isBook, "l'output tradotto e' riconosciuto come cartella di lavoro multi-foglio (ASCB)");

	std::vector<AscdSheet> sheets;
	status_t loadErr = isBook ? LoadASCDBook(&ascd, &sheets) : B_BAD_DATA;
	Check(loadErr == B_OK,
		"LoadASCDBook legge l'intera cartella tradotta senza errori (il bug reale falliva qui "
		"con B_BAD_DATA dal secondo foglio in poi)");
	Check(sheets.size() == 3, "LoadASCDBook restituisce tutti e tre i fogli, non solo il primo");

	if (sheets.size() == 3)
	{
		Check(sheets[0].name == "Primo", "il nome del primo foglio e' preservato");
		Check(sheets[1].name == "Secondo",
			"il nome del secondo foglio e' preservato (il bug reale lo leggeva come testo "
			"corrotto, dentro ai byte disallineati dell'area di stampa mancante del primo foglio)");
		Check(sheets[2].name == "Terzo", "il nome del terzo foglio e' preservato");

		Value v;
		sheets[0].doc->GetValue(cell(1, 1), v);
		Check(v.fType == eNumData && (double)v == 111, "A1 del primo foglio vale 111");

		sheets[1].doc->GetValue(cell(1, 1), v);
		Check(v.fType == eNumData && (double)v == 222,
			"A1 del secondo foglio vale 222, non un valore corrotto dal disallineamento");

		sheets[2].doc->GetValue(cell(1, 1), v);
		Check(v.fType == eNumData && (double)v == 333,
			"A1 del terzo foglio vale 333, non un valore corrotto dal disallineamento");
	}

	for (size_t i = 0; i < sheets.size(); i++)
		sheets[i].doc->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
