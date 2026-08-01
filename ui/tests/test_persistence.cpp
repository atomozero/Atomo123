/*
	test_persistence.cpp

	Verifica la persistenza nel formato nativo (Fase 10) di Blocca
	riquadri e altezza di riga -- le due funzionalita' rimaste "solo
	per la sessione corrente" dopo la Fase 7, ora salvate/ricaricate
	tramite una sezione opzionale in coda al formato ASCD, stesso
	principio gia' usato per larghezza di colonna e colori (vedi il
	commento in AscdIO.h). Non copre font/allineamento (ancora un
	limite noto, vedi ROADMAP.md Fase 10 -- fFont e' un indice
	volatile in gFontSizeTable, richiede una sezione a parte non
	ancora scritta).

	Stesso motivo di BApplication di test_ascd_io.cpp/test_ascd_book.cpp:
	GetCellFormula su una formula passa da BFont::StringWidth, che
	senza un'app registrata resta bloccato in attesa dell'app_server.
*/

#include <cstdio>
#include <vector>

#include <Application.h>
#include <File.h>

#include "AscdIO.h"
#include "Cell.h"
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
	BApplication app("application/x-vnd.Atomo-TestPersistence");

	const char* path = "/tmp/test_persistence.ascd";

	CContainer* doc = new CContainer(NULL, NULL);
	TryToParseString("10", cell(1, 1), doc, true); // A1

	std::vector<std::pair<int, float> > rowHeights;
	rowHeights.push_back(std::make_pair(1, 40.0f)); // riga 1 alta il doppio
	rowHeights.push_back(std::make_pair(3, 10.0f)); // riga 3 al minimo

	int frozenRows = 2, frozenCols = 1;

	{
		BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCD(doc, &file, NULL, NULL, &rowHeights, &frozenRows, &frozenCols);
		Check(err == B_OK, "SaveASCD con altezze di riga e Blocca riquadri riesce");
	}
	doc->Release();

	CContainer* reloaded = new CContainer(NULL, NULL);
	std::vector<std::pair<int, float> > loadedHeights;
	int loadedFrozenRows = -1, loadedFrozenCols = -1;

	{
		BFile file(path, B_READ_ONLY);
		status_t err = LoadASCD(&file, reloaded, NULL, NULL,
			&loadedHeights, &loadedFrozenRows, &loadedFrozenCols);
		Check(err == B_OK, "LoadASCD dallo stesso file riesce");
	}

	Check(loadedFrozenRows == 2 && loadedFrozenCols == 1,
		"Blocca riquadri (2 righe, 1 colonna) sopravvive al giro di salvataggio/ricarica");

	Check(loadedHeights.size() == 2, "entrambe le altezze di riga personalizzate sopravvivono");
	bool foundRow1 = false, foundRow3 = false;
	for (size_t i = 0; i < loadedHeights.size(); i++)
	{
		if (loadedHeights[i].first == 1 && loadedHeights[i].second == 40.0f)
			foundRow1 = true;
		if (loadedHeights[i].first == 3 && loadedHeights[i].second == 10.0f)
			foundRow3 = true;
	}
	Check(foundRow1, "l'altezza della riga 1 (40) e' quella corretta dopo il giro");
	Check(foundRow3, "l'altezza della riga 3 (10) e' quella corretta dopo il giro");

	// Un file scritto SENZA queste sezioni (chiamante che passa NULL,
	// come tutte le chiamate a SaveASCD/LoadASCD esistenti prima di
	// questa fase) resta leggibile: nessuna sezione, nessun errore,
	// il chiamante che chiede i nuovi campi riceve semplicemente i
	// valori "nessun blocco/nessuna riga personalizzata".
	const char* oldPath = "/tmp/test_persistence_old.ascd";
	CContainer* oldStyleDoc = new CContainer(NULL, NULL);
	TryToParseString("5", cell(1, 1), oldStyleDoc, true);
	{
		BFile file(oldPath, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		SaveASCD(oldStyleDoc, &file); // nessuna delle sezioni nuove
	}
	oldStyleDoc->Release();

	CContainer* oldStyleReloaded = new CContainer(NULL, NULL);
	int oldFrozenRows = -1, oldFrozenCols = -1;
	std::vector<std::pair<int, float> > oldHeights;
	{
		BFile file(oldPath, B_READ_ONLY);
		status_t err = LoadASCD(&file, oldStyleReloaded, NULL, NULL,
			&oldHeights, &oldFrozenRows, &oldFrozenCols);
		Check(err == B_OK, "un file senza le nuove sezioni resta leggibile");
	}
	Check(oldFrozenRows == 0 && oldFrozenCols == 0 && oldHeights.empty(),
		"e chi chiede i nuovi campi riceve i valori predefiniti (nessun blocco/altezza)");

	doc = NULL; // gia' rilasciato sopra
	reloaded->Release();
	oldStyleReloaded->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
