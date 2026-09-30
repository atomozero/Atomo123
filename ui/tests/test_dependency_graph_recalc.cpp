/*
	test_dependency_graph_recalc.cpp

	Verifica il RICALCOLO vero basato sul grafo delle dipendenze
	(roadmap "Path to full Excel parity" Tier 3, Fase 1 -- il primo
	vero consumatore del grafo di Fase 0, vedi RecalculateMinimal in
	AscdIO.h/.cpp): a differenza di test_dependency_graph_build.cpp
	(solo struttura, nessun ricalcolo), qui si verifica che
	RecalculateMinimal produca risultati IDENTICI a RecalculateAll/
	RecalculateWorkbook per ogni caso che quei due gia' coprono
	(inclusi tutti i casi di test_circular_reference.cpp, portati qui
	uno a uno), PIU' la vera novita': un riferimento circolare che
	attraversa due fogli diversi, che il vecchio rilevatore DFS
	(GetPrecedents, solo stesso foglio) non puo' vedere.

	Fase 1 e' ancora in modalita' "ombra" (MainWindow::
	ShadowVerifyDependencyGraph esegue RecalculateMinimal DOPO il
	vecchio percorso, confronta, e ripristina il valore vecchio in
	caso di disaccordo -- l'utente non vede mai un risultato calcolato
	da RecalculateMinimal fino al taglio della Fase 3): questo file
	testa RecalculateMinimal DIRETTAMENTE, non attraverso quella rete
	di sicurezza.
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <Path.h>
#include <Roster.h>

#include "AscdIO.h"
#include "Cell.h"
#include "CellIterator.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MainWindow.h"
#include "MyError.h"
#include "ResourceManager.h"
#include "SheetView.h"

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

static bool IsCircularMarker(CContainer* doc, cell c)
{
	Value v;
	doc->GetValue(c, v);
	return v.fType == eTextData && strcmp((const char*)v, "#CIRCULAR!") == 0;
}

static double NumValue(CContainer* doc, cell c)
{
	Value v;
	doc->GetValue(c, v);
	return (double)v;
}

// Stessa identica logica di CollectFormulaCells (AscdIO.cpp, file-locale
// li' -- non raggiungibile da qui): ogni cella con formula di "doc",
// qualificata con "doc" stesso, pensata per seminare RecalculateMinimal
// con lo stesso insieme che RecalculateAll gia' elabora per intero, cosi'
// da confrontare i due algoritmi a parita' di condizioni.
static void SeedAllFormulaCells(CContainer* doc, std::vector<QualifiedCell>& out)
{
	CCellIterator iter(doc, NULL);
	cell c;
	while (iter.NextExisting(c))
	{
		if (doc->GetCellFormula(c))
		{
			QualifiedCell qc;
			qc.container = doc;
			qc.loc = c;
			out.push_back(qc);
		}
	}
}

class TestResolver : public ISheetResolver {
public:
	CContainer* sheet1;
	CContainer* sheet2;

	TestResolver() : sheet1(NULL), sheet2(NULL) {}

	CContainer* ResolveSheetByName(const char* name) override
	{
		if (sheet1 && strcmp(name, "Sheet1") == 0) return sheet1;
		if (sheet2 && strcmp(name, "Sheet2") == 0) return sheet2;
		return NULL;
	}
	CContainer* FindSheetWithTable(const std::string&) override { return NULL; }
	const NamedStyleTable* GetNamedStyleTable() const override { return NULL; }
	const ThemePalette* GetThemePalette() const override { return NULL; }
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestDependencyGraphRecalc");

	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	// Parte 1: catena lineare stesso-foglio A1->B1->C1.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), doc, true);          // A1
		TryToParseString("=A1+1", cell(2, 1), doc, true);        // B1
		TryToParseString("=B1+1", cell(3, 1), doc, true);        // C1

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);

		Check(NumValue(doc, cell(2, 1)) == 11, "catena lineare: B1=A1+1 vale 11");
		Check(NumValue(doc, cell(3, 1)) == 12, "catena lineare: C1=B1+1 vale 12");

		// Modifica A1 e riesegue -- deve propagarsi di nuovo correttamente.
		TryToParseString("100", cell(1, 1), doc, true);
		seeds.clear();
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);

		Check(NumValue(doc, cell(2, 1)) == 101, "catena lineare dopo modifica: B1 vale 101");
		Check(NumValue(doc, cell(3, 1)) == 102, "catena lineare dopo modifica: C1 vale 102");

		doc->Release();
	}

	// Parte 2: diamante A1->B1,A1->C1,B1->D1,C1->D1 -- D1 deve essere
	// calcolata esattamente UNA volta per modifica (vedi
	// CContainer::sCalcCellCallCount), non due (una per ciascun
	// genitore che la "raggiunge" durante la BFS).
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), doc, true);            // A1
		TryToParseString("=A1*2", cell(2, 1), doc, true);        // B1
		TryToParseString("=A1*3", cell(3, 1), doc, true);        // C1
		TryToParseString("=B1+C1", cell(4, 1), doc, true);       // D1

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);

		Check(NumValue(doc, cell(4, 1)) == 25, "diamante: D1=B1+C1=10+15=25");

		TryToParseString("10", cell(1, 1), doc, true);
		seeds.clear();
		SeedAllFormulaCells(doc, seeds);

		long before = CContainer::sCalcCellCallCount;
		RecalculateMinimal(seeds);
		long calls = CContainer::sCalcCellCallCount - before;

		Check(NumValue(doc, cell(4, 1)) == 50, "diamante dopo modifica: D1=20+30=50");
		// 3 celle formula (B1,C1,D1) nell'insieme raggiunto da A1 (A1
		// stessa e' letterale, nessuna formula, CalcCell su di lei
		// sarebbe comunque un no-op se capitasse -- ma non e' fra i
		// "seeds" qui, solo le celle CON formula lo sono): esattamente
		// 3 chiamate, non 4+ (che rivelerebbe D1 ricalcolata piu' di
		// una volta).
		Check(calls == 3, "diamante: esattamente 3 chiamate a CalcCell (B1,C1,D1 una volta ciascuna)");

		doc->Release();
	}

	// Parte 3: catena CROSS-FOGLIO Sheet1!A1 -> Sheet2!B1 -> Sheet1!C1.
	{
		CContainer* sheet1 = new CContainer(NULL, NULL);
		CContainer* sheet2 = new CContainer(NULL, NULL);
		TestResolver resolver;
		resolver.sheet1 = sheet1;
		resolver.sheet2 = sheet2;
		sheet1->SetSheetResolver(&resolver);
		sheet2->SetSheetResolver(&resolver);

		TryToParseString("7", cell(1, 1), sheet1, true);                 // Sheet1!A1
		TryToParseString("=Sheet1!A1+1", cell(2, 1), sheet2, true);      // Sheet2!B1
		TryToParseString("=Sheet2!B1+1", cell(3, 1), sheet1, true);      // Sheet1!C1

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(sheet1, seeds);
		SeedAllFormulaCells(sheet2, seeds);
		RecalculateMinimal(seeds);

		Check(NumValue(sheet2, cell(2, 1)) == 8, "catena cross-foglio: Sheet2!B1 = Sheet1!A1+1 = 8");
		Check(NumValue(sheet1, cell(3, 1)) == 9, "catena cross-foglio: Sheet1!C1 = Sheet2!B1+1 = 9");

		sheet1->Release();
		sheet2->Release();
	}

	// Parte 4: parita' di rilevamento dei riferimenti circolari con
	// test_circular_reference.cpp -- stessi tre casi, stesso risultato
	// atteso, ma attraverso RecalculateMinimal invece di RecalculateAll/
	// RecalculateWorkbook.
	{
		// Caso diretto: A4="=A1+A2+A3+A4".
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("100", cell(1, 1), doc, true);
		TryToParseString("200", cell(1, 2), doc, true);
		TryToParseString("300", cell(1, 3), doc, true);
		TryToParseString("=A1+A2+A3+A4", cell(1, 4), doc, true);

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);

		Check(IsCircularMarker(doc, cell(1, 4)),
			"parita' circolare diretta: A4 auto-referenziata viene marcata #CIRCULAR!");
		Check(NumValue(doc, cell(1, 1)) == 100,
			"parita' circolare diretta: A1 (letterale, fuori dal ciclo) resta 100");

		doc->Release();
	}
	{
		// Caso indiretto: B1="=B2+1", B2="=B1+1".
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("=B2+1", cell(2, 1), doc, true);
		TryToParseString("=B1+1", cell(2, 2), doc, true);

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);

		Check(IsCircularMarker(doc, cell(2, 1)), "parita' circolare indiretta: B1 marcata");
		Check(IsCircularMarker(doc, cell(2, 2)), "parita' circolare indiretta: B2 marcata anch'essa");

		doc->Release();
	}
	{
		// Non-partecipante: A1="=A1" (circolare), B1="=A1*2" (dipende
		// dal ciclo ma non ne fa parte -- NON deve essere marcata).
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("=A1", cell(1, 1), doc, true);
		TryToParseString("=A1*2", cell(2, 1), doc, true);

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);

		Check(IsCircularMarker(doc, cell(1, 1)), "parita' non-partecipante: A1=\"=A1\" marcata");
		Check(!IsCircularMarker(doc, cell(2, 1)),
			"parita' non-partecipante: B1=\"=A1*2\" NON marcata essa stessa (stesso comportamento "
			"del vecchio rilevatore DFS)");

		doc->Release();
	}

	// Parte 5: NOVITA' reale -- ciclo circolare fra DUE FOGLI diversi,
	// che GetPrecedents (solo stesso foglio) non puo' vedere, ma
	// GetQualifiedPrecedents si'. Sheet1!A1 = "=Sheet2!B1", Sheet2!B1 =
	// "=Sheet1!A1": nessuna delle due celle referenzia se stessa o
	// un'altra cella del PROPRIO foglio, quindi il vecchio rilevatore
	// (usato da RecalculateAll/RecalculateWorkbook oggi) non troverebbe
	// nessun ciclo qui -- il nuovo, si'.
	{
		CContainer* sheet1 = new CContainer(NULL, NULL);
		CContainer* sheet2 = new CContainer(NULL, NULL);
		TestResolver resolver;
		resolver.sheet1 = sheet1;
		resolver.sheet2 = sheet2;
		sheet1->SetSheetResolver(&resolver);
		sheet2->SetSheetResolver(&resolver);

		TryToParseString("=Sheet2!B1", cell(1, 1), sheet1, true);   // Sheet1!A1
		TryToParseString("=Sheet1!A1", cell(2, 1), sheet2, true);   // Sheet2!B1

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(sheet1, seeds);
		SeedAllFormulaCells(sheet2, seeds);
		RecalculateMinimal(seeds);

		Check(IsCircularMarker(sheet1, cell(1, 1)),
			"NUOVA capacita': Sheet1!A1 (ciclo cross-foglio con Sheet2!B1) viene marcata #CIRCULAR!");
		Check(IsCircularMarker(sheet2, cell(2, 1)),
			"NUOVA capacita': Sheet2!B1 marcata anch'essa -- rilevamento impossibile per il "
			"vecchio DFS stesso-foglio");

		sheet1->Release();
		sheet2->Release();
	}

	// Parte 6: seme multiplo (simula un incolla/riempimento che tocca
	// piu' celle in un colpo solo) -- A1 e A2 modificate insieme, B1 e
	// B2 (ciascuna dipendente dalla propria) devono aggiornarsi entrambe
	// in una sola chiamata, senza ricalcoli ridondanti.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), doc, true);        // A1
		TryToParseString("2", cell(1, 2), doc, true);        // A2
		TryToParseString("=A1*10", cell(2, 1), doc, true);   // B1
		TryToParseString("=A2*10", cell(2, 2), doc, true);   // B2

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);

		TryToParseString("5", cell(1, 1), doc, true);
		TryToParseString("6", cell(1, 2), doc, true);

		QualifiedCell a1, a2;
		a1.container = doc; a1.loc = cell(1, 1);
		a2.container = doc; a2.loc = cell(1, 2);
		std::vector<QualifiedCell> multiSeed;
		multiSeed.push_back(a1);
		multiSeed.push_back(a2);
		RecalculateMinimal(multiSeed);

		Check(NumValue(doc, cell(2, 1)) == 50, "seme multiplo: B1=A1*10=50 dopo modifica simultanea");
		Check(NumValue(doc, cell(2, 2)) == 60, "seme multiplo: B2=A2*10=60 dopo modifica simultanea");

		doc->Release();
	}

	// Parte 7: la verifica "ombra" (MainWindow::ShadowVerifyDependencyGraph,
	// chiamata da RecalculateActiveWorkbook dopo il vecchio percorso) non
	// deve alterare il risultato ne' bloccarsi su una sequenza di editing
	// del tutto ordinaria -- prova indiretta che il confronto non scatta
	// falsi positivi sull'uso comune (un vero falso allarme sarebbe
	// altrettanto dannoso di uno mancato per il periodo di collaudo).
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();

		CContainer* doc = win->GetSheetView()->Document();
		TryToParseString("3", cell(1, 1), doc, true);
		TryToParseString("=A1*4", cell(2, 1), doc, true);
		win->RecalculateActiveWorkbook();

		Check(NumValue(doc, cell(2, 1)) == 12,
			"verifica ombra: RecalculateActiveWorkbook produce ancora il risultato corretto "
			"(B1=A1*4=12) dopo l'aggiunta della verifica ombra");

		TryToParseString("=A1+A1", cell(1, 1), doc, true); // A1 autoreferenziata
		win->RecalculateActiveWorkbook();
		Check(IsCircularMarker(doc, cell(1, 1)),
			"verifica ombra: un vero ciclo resta rilevato dal vecchio percorso "
			"(la rete di sicurezza non nasconde un errore reale)");

		win->Unlock();
		win->Lock();
		win->Quit();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
