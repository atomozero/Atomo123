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

	Fase 3 (taglio): RecalculateMinimal e' ora l'UNICO percorso di
	ricalcolo visto dall'utente reale (MainWindow::
	RecalculateActiveWorkbook lo chiama direttamente, niente piu' rete
	di sicurezza "ombra" ne' vecchio ciclo a punto fisso nel percorso
	live) -- la Parte 7 sotto, che durante il periodo di collaudo
	verificava che la rete ombra non alterasse un uso comune, ora
	verifica lo stesso comportamento sul percorso di produzione vero.
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

	// Parte 8: riferimento a colonna intera (fColumnDependents, Fase 2) --
	// modificare QUALUNQUE cella della colonna osservata deve pianificare
	// la formula grezza; una modifica in un'altra colonna non deve
	// toccarla per niente (il seme non la raggiungerebbe nemmeno).
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), doc, true);       // A1
		TryToParseString("2", cell(1, 2), doc, true);       // A2
		TryToParseString("=SUM(A:A)", cell(2, 1), doc, true); // B1

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);
		Check(NumValue(doc, cell(2, 1)) == 3, "colonna intera: B1=SUM(A:A)=1+2=3 al primo giro");

		TryToParseString("10", cell(1, 2), doc, true); // A2 cambia, e' nella colonna osservata
		QualifiedCell a2; a2.container = doc; a2.loc = cell(1, 2);
		std::vector<QualifiedCell> seedA2(1, a2);
		RecalculateMinimal(seedA2);
		Check(NumValue(doc, cell(2, 1)) == 11,
			"colonna intera: modificare A2 (dentro la colonna osservata) aggiorna B1 a 1+10=11");

		TryToParseString("99", cell(2, 2), doc, true); // B2, colonna B, non osservata da nessuno
		QualifiedCell b2; b2.container = doc; b2.loc = cell(2, 2);
		std::vector<QualifiedCell> seedB2(1, b2);
		RecalculateMinimal(seedB2);
		Check(NumValue(doc, cell(2, 1)) == 11,
			"colonna intera: modificare B2 (colonna NON osservata) non tocca B1, resta 11");

		doc->Release();
	}

	// Parte 9: dipendente di una cella spillata (fSpillOwnerOf +
	// RedirectSpillMember, Fase 2) -- "=B5*2" dipende in realta'
	// dall'owner del suo spill (B3), non da B5 stessa: modificare B3
	// (l'unica cella con una formula propria nello spill) deve
	// ripercuotersi sul dipendente.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("=SEQUENCE(3,1)", cell(2, 3), doc, true, '.', ','); // B3:B5
		TryToParseString("=B5*2", cell(4, 1), doc, true); // D1, B5 e' gia' spillata quando D1 nasce

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);
		Check(NumValue(doc, cell(2, 5)) == 3, "spill: B5 (terza cella spillata) vale 3");
		Check(NumValue(doc, cell(4, 1)) == 6, "spill: D1=B5*2=6, dipendente redirected sull'owner B3");

		doc->Release();
	}

	// Parte 10: caso limite REALE (vedi il commento in
	// CContainer::ApplySpill) -- un dipendente scritto PRIMA che la sua
	// cella diventi membro di uno spill deve migrare a dipendere
	// dall'owner, cosi' una futura crescita dello spill (per un cambio
	// di un argomento a monte, non della formula dell'owner stessa) lo
	// raggiunge comunque.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("3", cell(3, 1), doc, true); // C1 = 3 (argomento di SEQUENCE)
		TryToParseString("=SEQUENCE(C1,1)", cell(2, 3), doc, true, '.', ','); // B3:B5, C1 righe
		// D1 dipende da B6, che NON e' ancora membro dello spill (lo
		// spill iniziale e' solo B3:B5, 3 righe) -- fDependents[B6] nasce
		// puntando a D1 direttamente, non redirected su B3.
		TryToParseString("=B6*2", cell(4, 1), doc, true); // D1

		std::vector<QualifiedCell> seeds;
		SeedAllFormulaCells(doc, seeds);
		RecalculateMinimal(seeds);
		Check(NumValue(doc, cell(4, 1)) == 0,
			"crescita spill: prima della crescita B6 e' vuota, D1=B6*2=0");

		// C1 cresce da 3 a 4: lo spill di B3 si estende a B3:B6, B6
		// diventa membro per la prima volta (valore atteso 4, quarto
		// elemento di SEQUENCE(4,1)).
		TryToParseString("4", cell(3, 1), doc, true);
		QualifiedCell c1; c1.container = doc; c1.loc = cell(3, 1);
		std::vector<QualifiedCell> seedC1(1, c1);
		RecalculateMinimal(seedC1);

		Check(NumValue(doc, cell(2, 6)) == 4, "crescita spill: B6 e' ora membro dello spill, vale 4");
		Check(NumValue(doc, cell(4, 1)) == 8,
			"crescita spill: D1=B6*2=8 si aggiorna SUBITO, provando che il dipendente scritto "
			"prima della crescita e' stato migrato su fDependents[owner] (senza la migrazione "
			"in CContainer::ApplySpill, D1 resterebbe fermo a 0: il seme parte da C1, arriva a "
			"B3 (owner), ma senza la migrazione non raggiungerebbe D1, ancora agganciato a B6)");

		doc->Release();
	}

	// Parte 7 (Fase 3, percorso di produzione reale): MainWindow::
	// RecalculateActiveWorkbook() con seme "largo" (nessuna cella
	// passata) deve produrre lo stesso risultato corretto di sempre su
	// una sequenza di editing ordinaria, e un vero ciclo resta rilevato
	// -- nessuna rete di sicurezza ombra di mezzo, questo E' il percorso
	// che l'utente reale esercita ora.
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();

		CContainer* doc = win->GetSheetView()->Document();
		TryToParseString("3", cell(1, 1), doc, true);
		TryToParseString("=A1*4", cell(2, 1), doc, true);
		win->RecalculateActiveWorkbook();

		Check(NumValue(doc, cell(2, 1)) == 12,
			"percorso di produzione: RecalculateActiveWorkbook produce il risultato corretto "
			"(B1=A1*4=12)");

		TryToParseString("=A1+A1", cell(1, 1), doc, true); // A1 autoreferenziata
		win->RecalculateActiveWorkbook();
		Check(IsCircularMarker(doc, cell(1, 1)),
			"percorso di produzione: un vero ciclo viene rilevato e marcato");

		win->Unlock();
		win->Lock();
		win->Quit();
	}

	// Parte 11 (Fase 3): banco di prova di prestazioni -- la prova
	// concreta che l'intero lavoro sul grafo delle dipendenze valeva la
	// pena. Un solo foglio con: 20000 celle letterali di riempimento
	// (nessuna relazione con nulla), un input condiviso S1, 500 formule a
	// ventaglio che dipendono TUTTE da S1 (F1..F500 = S1*2), e una catena
	// INVERTITA di 40 celle (Chain1=Chain2+1, ..., Chain39=Chain40+1,
	// Chain40=A1+1) -- invertita apposta: CCellIterator scandisce in
	// ordine di riga crescente, quindi Chain1 (che dipende da Chain2,
	// scandita DOPO nella stessa passata) legge il valore vecchio di
	// Chain2 nella stessa passata in cui e' cambiato, serve una passata
	// in piu' per ogni anello della catena per propagare all'indietro --
	// esattamente il caso reale che la roadmap descrive ("fino a 50
	// passate"), non il caso migliore (una catena diretta nello stesso
	// verso dell'iterazione converge gia' in 1 passata, come una prima
	// versione di questo banco di prova aveva scoperto per errore).
	// Si modifica S1 UNA volta e si cronometra il ricalcolo intero con
	// (a) il vecchio ciclo a punto fisso (RecalculateAll, fino a 50
	// passate su OGNI cella esistente, letterali comprese) e (b)
	// RecalculateMinimal seminato con la sola S1 (che scopre da solo,
	// tramite il grafo, i soli ~540 dipendenti reali, in UNA passata
	// ordinata topologicamente). RecalculateAll resta nel codice apposta
	// (non e' morto: altri test lo chiamano direttamente) proprio per
	// rendere possibile questo confronto onesto a parita' di documento.
	{
		CContainer* doc = new CContainer(NULL, NULL);

		cell s1(1, 1); // A1, l'input condiviso
		TryToParseString("1", s1, doc, true);

		const int kPadding = 20000;
		const int kFanout = 500;
		const int kChain = 40;

		// Riempimento: colonne D in poi, 1000 righe per colonna -- pura
		// zavorra, nessuna formula, nessuna relazione con S1.
		{
			int written = 0;
			for (int col = 4; written < kPadding; col++)
			{
				for (int row = 1; row <= 1000 && written < kPadding; row++)
				{
					char buf[16];
					snprintf(buf, sizeof(buf), "%d", written);
					TryToParseString(buf, cell(col, row), doc, true);
					written++;
				}
			}
		}

		// Ventaglio: colonna B (indice 2), righe 1..500, tutte "=A1*2".
		for (int row = 1; row <= kFanout; row++)
			TryToParseString("=A1*2", cell(2, row), doc, true);

		// Catena INVERTITA: colonna C (indice 3). Chain(kChain) e' la
		// prima a dipendere da A1; ogni cella precedente dipende dalla
		// SUCCESSIVA (numero di riga piu' alto), il contrario dell'ordine
		// di scansione di CCellIterator.
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "=A1+1");
			TryToParseString(buf, cell(3, kChain), doc, true);
			for (int row = kChain - 1; row >= 1; row--)
			{
				snprintf(buf, sizeof(buf), "=C%d+1", row + 1);
				TryToParseString(buf, cell(3, row), doc, true);
			}
		}

		// Porta il documento a un primo stato coerente (non cronometrato:
		// e' il costo "una tantum" dell'apertura file, non quello che
		// questa fase ottimizza) prima di misurare l'effetto di UNA
		// modifica interattiva.
		RecalculateAll(doc);
		Check(NumValue(doc, cell(2, 1)) == 2, "banco di prova: stato iniziale coerente (F1=A1*2=2)");
		Check(NumValue(doc, cell(3, 1)) == 1 + kChain,
			"banco di prova: stato iniziale coerente (inizio catena, propagato dalla fine)");

		// (a) vecchio percorso: modifica A1, cronometra RecalculateAll.
		TryToParseString("2", s1, doc, true);
		bigtime_t oldStart = system_time();
		RecalculateAll(doc);
		bigtime_t oldElapsed = system_time() - oldStart;

		Check(NumValue(doc, cell(2, 1)) == 4, "banco di prova (vecchio): F1=A1*2=4 dopo la modifica");
		Check(NumValue(doc, cell(3, 1)) == 2 + kChain,
			"banco di prova (vecchio): inizio catena propagato correttamente entro 50 passate");

		// (b) nuovo percorso: un'altra modifica reale ad A1 (non un
		// ricalcolo a vuoto di un valore gia' corretto -- stesso tipo di
		// lavoro del punto (a), per un confronto onesto), cronometra
		// RecalculateMinimal seminato con la sola A1.
		TryToParseString("3", s1, doc, true);
		QualifiedCell seed;
		seed.container = doc;
		seed.loc = s1;
		std::vector<QualifiedCell> seeds(1, seed);

		bigtime_t newStart = system_time();
		RecalculateMinimal(seeds);
		bigtime_t newElapsed = system_time() - newStart;

		Check(NumValue(doc, cell(2, 1)) == 6, "banco di prova (nuovo): F1=A1*2=6 dopo la modifica");
		Check(NumValue(doc, cell(3, 1)) == 3 + kChain,
			"banco di prova (nuovo): inizio catena propagato correttamente in una sola passata");

		double speedup = (newElapsed > 0) ? (double)oldElapsed / (double)newElapsed : 0.0;
		printf("\nBANCO DI PROVA PRESTAZIONI (foglio singolo, ~%d celle, ~%d dipendenti reali di A1, "
			"catena invertita di %d anelli):\n"
			"  RecalculateAll (vecchio):      %8lld us\n"
			"  RecalculateMinimal (nuovo):    %8lld us\n"
			"  Accelerazione:                 %.1fx\n\n",
			kPadding + kFanout + kChain + 1, kFanout + kChain, kChain,
			(long long)oldElapsed, (long long)newElapsed, speedup);

		Check(speedup >= 10.0,
			"banco di prova: il nuovo percorso e' almeno un ordine di grandezza piu' veloce "
			"del vecchio per una singola modifica su questo documento sintetico");

		doc->Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
