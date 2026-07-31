/*
	test_ascd_book.cpp

	Verifica il formato "cartella di lavoro" multi-foglio
	(SaveASCDBook/LoadASCDBook), primo passo del supporto multi-foglio
	(Fase 9): un header "ASCB" piu' il numero di fogli, seguiti per
	ciascuno da nome + un blocco ASCD completo (la stessa identica cosa
	di SaveASCD/LoadASCD, riusata cosi' com'e', non duplicata).

	Stesso motivo di BApplication di test_ascd_io.cpp: GetCellFormula
	su una formula passa da BFont::StringWidth, che senza un'app
	registrata resta bloccato in attesa dell'app_server.
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <File.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"

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
	BApplication app("application/x-vnd.Atomo-TestAscdBook");

	// Due fogli distinti, con nomi e contenuti diversi.
	CContainer* doc1 = new CContainer(NULL, NULL);
	TryToParseString("10", cell(1, 1), doc1, true); // A1
	TryToParseString("=A1*2", cell(2, 1), doc1, true); // B1
	doc1->CalcCell(cell(2, 1));

	CContainer* doc2 = new CContainer(NULL, NULL);
	TryToParseString("Ciao dal secondo foglio", cell(1, 1), doc2, true);

	std::vector<AscdSheet> sheets;
	AscdSheet sheet1;
	sheet1.name = "Foglio1";
	sheet1.doc = doc1;
	sheets.push_back(sheet1);

	AscdSheet sheet2;
	sheet2.name = "Secondo foglio";
	sheet2.doc = doc2;
	sheets.push_back(sheet2);

	BFile file("/tmp/test_ascd_book.tmp", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	Check(file.InitCheck() == B_OK, "creazione del file di test riuscita");

	status_t saveErr = SaveASCDBook(sheets, &file);
	Check(saveErr == B_OK, "SaveASCDBook riesce");

	file.Unset();
	BFile reopened("/tmp/test_ascd_book.tmp", B_READ_ONLY);
	Check(reopened.InitCheck() == B_OK, "riapertura del file salvato riuscita");

	Check(IsASCDBookFile(&reopened),
		"IsASCDBookFile riconosce il file appena scritto come cartella di lavoro");

	std::vector<AscdSheet> loaded;
	status_t loadErr = LoadASCDBook(&reopened, &loaded);
	Check(loadErr == B_OK, "LoadASCDBook riesce");
	Check(loaded.size() == 2, "LoadASCDBook restituisce due fogli");

	if (loaded.size() == 2)
	{
		Check(loaded[0].name == "Foglio1", "il nome del primo foglio e' preservato");
		Check(loaded[1].name == "Secondo foglio", "il nome del secondo foglio e' preservato");

		Value a1;
		loaded[0].doc->GetValue(cell(1, 1), a1);
		Check(a1.fType == eNumData && (double)a1 == 10.0,
			"A1 del primo foglio vale 10 dopo il giro completo");

		Value b1;
		loaded[0].doc->GetValue(cell(2, 1), b1);
		Check(b1.fType == eNumData && (double)b1 == 20.0,
			"B1 (=A1*2) del primo foglio ricalcola a 20 dopo il giro completo");

		char text2[512];
		loaded[1].doc->GetCellFormula(cell(1, 1), text2, false);
		Check(strcmp(text2, "Ciao dal secondo foglio") == 0,
			"il testo del secondo foglio e' preservato");

		for (size_t i = 0; i < loaded.size(); i++)
			loaded[i].doc->Release();
	}

	// Un vecchio file .ascd a un solo foglio (senza il wrapper "ASCB")
	// non deve essere scambiato per una cartella di lavoro.
	CContainer* legacyDoc = new CContainer(NULL, NULL);
	TryToParseString("42", cell(1, 1), legacyDoc, true);

	BFile legacyFile("/tmp/test_ascd_legacy.tmp", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	SaveASCD(legacyDoc, &legacyFile);
	legacyFile.Unset();

	BFile reopenedLegacy("/tmp/test_ascd_legacy.tmp", B_READ_ONLY);
	Check(!IsASCDBookFile(&reopenedLegacy),
		"un vecchio file .ascd a un solo foglio non viene riconosciuto come cartella di lavoro");
	Check(IsASCDFile(&reopenedLegacy),
		"lo stesso file resta pero' riconoscibile come ASCD a un solo foglio");

	legacyDoc->Release();
	doc1->Release();
	doc2->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
