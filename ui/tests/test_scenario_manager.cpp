/*
	test_scenario_manager.cpp

	Verifica "Gestione scenari" (Tier 4, "Path to full Excel parity" --
	l'altra meta' di "Tabella dati", vedi ROADMAP.md e Scenario.h).
	Stesso schema di test_chart_series_colors.cpp: MainWindow vera con
	HandleDefineScenario/HandleDeleteScenario/HandleShowScenario chiamati
	direttamente (pubblici apposta per essere testabili), poi un giro
	AscdIO in memoria/su file.

	Punto piu' importante di questo file: "Mostra scenario" scrive
	VERAMENTE i valori nelle celle variabili (mutazione permanente, non
	auto-ripristinata come le Tabelle dati) -- deve quindi essere
	annullabile con Annulla/Ripristina come ogni altra mutazione, ed e'
	verificato esplicitamente qui, non solo assunto. Aggiungere/
	aggiornare/eliminare uno scenario invece NON e' annullabile in questa
	fase (stesso limite dichiarato di NameWindow -- HandleDefineName/
	HandleDeleteName non chiamano mai SaveUndoState nemmeno loro).
*/

#include <cstdio>

#include <Application.h>
#include <File.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "Scenario.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "AscdIO.h"

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

static double CellNumber(CContainer* doc, cell c)
{
	Value v;
	doc->GetValue(c, v);
	return (double)v;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestScenarioManager");

	// --- Parte 1: MainWindow vera, Aggiungi/Aggiorna/Mostra/Annulla/Elimina ---

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	CContainer* doc = win->GetSheetView()->Document();

	// Celle variabili B2:B3, con una formula dipendente altrove (A5) per
	// verificare che "Mostra scenario" ricalcoli davvero, non solo
	// scriva le celle.
	TryToParseString("10", cell(2, 2), doc, true); // B2
	TryToParseString("20", cell(2, 3), doc, true); // B3
	TryToParseString("=B2+B3", cell(1, 5), doc, true); // A5

	win->HandleDefineScenario("Ottimistico", "B2:B3", "100\n200", "Caso migliore");
	Check(win->Scenarios().size() == 1, "HandleDefineScenario aggiunge uno scenario");
	if (win->Scenarios().size() == 1)
	{
		Check(win->Scenarios()[0].values.size() == 2, "2 valori per l'intervallo B2:B3");
		Check(win->Scenarios()[0].comment == "Caso migliore", "il commento e' preservato");
	}

	win->HandleDefineScenario("Pessimistico", "B2:B3", "1\n2", "");
	Check(win->Scenarios().size() == 2, "un secondo scenario si aggiunge senza toccare il primo");

	// Ridefinire un nome esistente sostituisce sul posto, non ne aggiunge
	// un terzo (stesso principio di "Aggiungi/Aggiorna" in NameWindow).
	win->HandleDefineScenario("Ottimistico", "B2:B3", "150\n250", "Aggiornato");
	Check(win->Scenarios().size() == 2, "ridefinire un nome esistente sostituisce, non aggiunge");
	if (win->Scenarios().size() == 2)
		Check(win->Scenarios()[0].values[0] == "150", "il valore aggiornato e' quello nuovo");

	// Validazione: un numero di valori diverso dal numero di celle
	// dell'intervallo non aggiunge nulla (mostra un BAlert bloccante in
	// un vero desktop -- qui verifichiamo solo che il modello non
	// cambi, stesso limite gia' documentato per gli altri BAlert di
	// convalida in questo progetto).
	size_t beforeInvalid = win->Scenarios().size();
	win->HandleDefineScenario("Rotto", "B2:B3", "solo-uno", "");
	Check(win->Scenarios().size() == beforeInvalid,
		"un numero di valori diverso dal numero di celle non aggiunge lo scenario");

	// Validazione: un intervallo non analizzabile non aggiunge nulla.
	win->HandleDefineScenario("RottoAnche", "!!!", "1\n2", "");
	Check(win->Scenarios().size() == beforeInvalid, "un intervallo non valido non aggiunge lo scenario");

	// --- Mostra scenario: mutazione VERA + ricalcolo. ---
	win->HandleShowScenario("Pessimistico");
	Check(CellNumber(doc, cell(2, 2)) == 1.0, "Mostra scenario scrive davvero il primo valore in B2");
	Check(CellNumber(doc, cell(2, 3)) == 2.0, "Mostra scenario scrive davvero il secondo valore in B3");
	Check(CellNumber(doc, cell(1, 5)) == 3.0, "A5 (=B2+B3) si ricalcola con i nuovi valori (1+2=3)");

	// --- Annullabile. ---
	Check(win->GetSheetView()->CanUndo(), "Mostra scenario e' annullabile");
	win->GetSheetView()->Undo();
	Check(CellNumber(doc, cell(2, 2)) == 10.0, "Annulla ripristina B2 al valore originale (10)");
	Check(CellNumber(doc, cell(2, 3)) == 20.0, "Annulla ripristina B3 al valore originale (20)");
	Check(CellNumber(doc, cell(1, 5)) == 30.0, "Annulla ricalcola anche A5 (10+20=30)");

	Check(win->GetSheetView()->CanRedo(), "dopo Annulla, Ripristina e' disponibile");
	win->GetSheetView()->Redo();
	Check(CellNumber(doc, cell(2, 2)) == 1.0, "Ripristina riapplica lo scenario appena annullato");
	Check(CellNumber(doc, cell(1, 5)) == 3.0, "Ripristina ricalcola anche A5");

	// --- Elimina. ---
	win->HandleDeleteScenario("Pessimistico");
	Check(win->Scenarios().size() == 1, "HandleDeleteScenario rimuove lo scenario dall'elenco");
	if (win->Scenarios().size() == 1)
		Check(win->Scenarios()[0].name == "Ottimistico", "lo scenario rimasto e' quello giusto");

	// Nome/indice inesistente: nessun crash, nessuna modifica.
	win->HandleDeleteScenario("NonEsiste");
	Check(win->Scenarios().size() == 1, "eliminare un nome inesistente non tocca l'elenco");
	win->HandleShowScenario("NonEsiste");
	Check(true, "mostrare uno scenario inesistente non va in crash");

	win->Unlock();

	win->Lock();
	win->Quit();

	// --- Parte 2: round-trip AscdIO (compreso un file scritto PRIMA di questa sezione) ---

	{
		CContainer& scenDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(2, 2), &scenDoc, true);

		std::vector<Scenario> saved;
		Scenario s1;
		s1.name = "Caso A";
		s1.changingCells.Set(2, 2, 2, 3); // B2:B3
		s1.values.push_back("100");
		s1.values.push_back("200");
		s1.comment = "Nota di prova";
		saved.push_back(s1);

		// Nessun commento -- deve restare vuoto dopo il giro, non
		// ereditare quello del primo scenario.
		Scenario s2;
		s2.name = "Caso B";
		s2.changingCells.Set(2, 2, 2, 3);
		s2.values.push_back("1");
		s2.values.push_back("2");
		saved.push_back(s2);

		BFile scenFile("tests/roundtrip_scenarios.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCD(&scenDoc, &scenFile,
			NULL, // charts
			NULL, // colWidths
			NULL, // rowHeights
			NULL, // frozenRows
			NULL, // frozenCols
			NULL, // images
			NULL, // showGrid
			NULL, // hasTabColor
			NULL, // tabColor
			NULL, // hiddenRows
			NULL, // hasAutoFilter
			NULL, // autoFilterRange
			NULL, // hasPrintArea
			NULL, // printArea
			NULL, // printSettings
			NULL, // vbaProject
			NULL, // isProtected
			NULL, // protection
			NULL, // filterHiddenValues
			NULL, // slicers
			&saved); // scenarios
		Check(err == B_OK, "SaveASCD con scenari riesce");
		scenDoc.Release();

		BFile scenReopened("tests/roundtrip_scenarios.ascd", B_READ_ONLY);
		CContainer& scenReloaded = *new CContainer(NULL, NULL);
		std::vector<Scenario> loaded;
		err = LoadASCD(&scenReopened, &scenReloaded,
			NULL, // charts
			NULL, // colWidths
			NULL, // rowHeights
			NULL, // frozenRows
			NULL, // frozenCols
			NULL, // images
			NULL, // showGrid
			NULL, // hasTabColor
			NULL, // tabColor
			NULL, // hiddenRows
			NULL, // hasAutoFilter
			NULL, // autoFilterRange
			NULL, // hasPrintArea
			NULL, // printArea
			NULL, // printSettings
			false, // skipInitialRecalc
			NULL, // vbaProject
			NULL, // isProtected
			false, // skipVbaAndProtectionSections
			NULL, // protection
			NULL, // filterHiddenValues
			NULL, // slicers
			&loaded); // scenarios
		Check(err == B_OK, "LoadASCD con scenari riesce");
		Check(loaded.size() == 2, "entrambi gli scenari sopravvivono al giro salva->ricarica");
		if (loaded.size() == 2)
		{
			Check(loaded[0].name == "Caso A", "il nome del primo scenario e' preservato");
			Check(loaded[0].changingCells.left == 2 && loaded[0].changingCells.top == 2
					&& loaded[0].changingCells.right == 2 && loaded[0].changingCells.bottom == 3,
				"l'intervallo delle celle variabili e' preservato");
			Check(loaded[0].values.size() == 2 && loaded[0].values[0] == "100"
					&& loaded[0].values[1] == "200",
				"i valori del primo scenario sono preservati, nello stesso ordine");
			Check(loaded[0].comment == "Nota di prova", "il commento e' preservato");
			Check(loaded[1].comment == "",
				"il secondo scenario (senza commento) resta vuoto, non eredita quello del primo");
		}
		scenReloaded.Release();

		// --- Compatibilita' con un file scritto PRIMA di questa sezione ---
		//
		// La sezione scenari NON e' piu' l'ultima cosa scritta da
		// SaveASCD (lo era quando questo test e' stato scritto): la Fase
		// "Named cell styles + live theme palette" ne ha aggiunte altre
		// due in coda dopo di essa (vedi AscdIO.cpp/NamedStyle.h e
		// test_named_styles.cpp per quella prova dedicata), e la Fase 7
		// ("asse secondario / trendline / barre d'errore") ne ha appesa
		// una quinta dopo ancora (sezione opzioni per serie di grafico).
		// Per un file senza scenari, senza stili con nome e senza
		// "charts" passato (NULL, come in questa chiamata), il totale
		// finale e' 14 byte: 4 (conteggio scenari, qui 0) + 4 (conteggio
		// celle con stile con nome, qui 0) + 1 (hasNamedStyles) + 1
		// (hasThemePalette) + 4 (contatore grafici della sezione opzioni
		// per serie, 0 qui perche' "charts" e' NULL). Troncare quei 14
		// byte equivale quindi a un file scritto da una build PRIMA che
		// TUTTE E TRE le sezioni esistessero -- prova CONCRETA (non solo
		// un ragionamento sul codice) che LoadASCD resta EOF-tollerante
		// con un file vecchio. (Nota per la prossima fase che aggiunge
		// una sezione in coda: questo conteggio andra' aggiornato di
		// nuovo, stessa manutenzione gia' successa qui due volte.)
		CContainer& oldDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), &oldDoc, true);
		BFile oldFile("tests/roundtrip_scenarios_old.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		err = SaveASCD(&oldDoc, &oldFile);
		Check(err == B_OK, "SaveASCD di riferimento (nessuno scenario) riesce");
		oldDoc.Release();

		off_t size = 0;
		oldFile.GetSize(&size);
		Check(size > 14, "il file di riferimento e' abbastanza grande da poter troncare 14 byte");
		oldFile.SetSize(size - 14);

		BFile oldReopened("tests/roundtrip_scenarios_old.ascd", B_READ_ONLY);
		CContainer& oldReloaded = *new CContainer(NULL, NULL);
		std::vector<Scenario> oldLoaded;
		err = LoadASCD(&oldReopened, &oldReloaded,
			NULL, // charts
			NULL, // colWidths
			NULL, // rowHeights
			NULL, // frozenRows
			NULL, // frozenCols
			NULL, // images
			NULL, // showGrid
			NULL, // hasTabColor
			NULL, // tabColor
			NULL, // hiddenRows
			NULL, // hasAutoFilter
			NULL, // autoFilterRange
			NULL, // hasPrintArea
			NULL, // printArea
			NULL, // printSettings
			false, // skipInitialRecalc
			NULL, // vbaProject
			NULL, // isProtected
			false, // skipVbaAndProtectionSections
			NULL, // protection
			NULL, // filterHiddenValues
			NULL, // slicers
			&oldLoaded); // scenarios
		Check(err == B_OK, "un file troncato (che simula una build precedente a questa sezione) si carica comunque");
		Check(oldLoaded.empty(), "un file senza questa sezione restituisce un elenco scenari vuoto, non un errore");
		oldReloaded.Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
