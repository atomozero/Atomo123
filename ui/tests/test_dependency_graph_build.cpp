/*
	test_dependency_graph_build.cpp

	Verifica la COSTRUZIONE del grafo delle dipendenze (roadmap "Path to
	full Excel parity" Tier 3, "un vero grafo delle dipendenze" -- Fase
	0, gruppo di strutture dati, vedi CContainer::fDependents/
	fColumnDependents/fRowDependents/GetQualifiedPrecedents in
	Container.h): solo la struttura dati e il suo aggiornamento
	incrementale (NewCell/DisposeCell/ClearCellContent/CopyCell/
	MoveCell), NESSUNA asserzione sul ricalcolo vero -- quello e'
	compito della Fase 1, il primo vero CONSUMATORE del grafo, che deve
	ancora essere scritta. Qui si verifica solo che i bordi compaiano/
	spariscano correttamente quando le formule cambiano.
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <Path.h>
#include <Roster.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "NamedStyle.h"
#include "ResourceManager.h"

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

// Risolutore di test per i casi cross-foglio: a differenza del
// "TestResolver" di test_named_styles.cpp (sempre NULL, non gli serviva
// una vera risoluzione), qui ResolveSheetByName risolve DAVVERO due
// CContainer distinti per nome, il caso che questo test deve coprire.
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

static bool HasDependent(CContainer* precedentContainer, cell precedentLoc,
	CContainer* dependentContainer, cell dependentLoc)
{
	const std::map<cell, std::set<QualifiedCell> >& deps = precedentContainer->GetDependentsMap();
	std::map<cell, std::set<QualifiedCell> >::const_iterator it = deps.find(precedentLoc);
	if (it == deps.end())
		return false;
	for (std::set<QualifiedCell>::const_iterator d = it->second.begin(); d != it->second.end(); d++)
	{
		if (d->container == dependentContainer && d->loc == dependentLoc)
			return true;
	}
	return false;
}

static bool HasColumnDependent(CContainer* watchedContainer, int column,
	CContainer* dependentContainer, cell dependentLoc)
{
	const std::map<int, std::set<QualifiedCell> >& deps = watchedContainer->GetColumnDependentsMap();
	std::map<int, std::set<QualifiedCell> >::const_iterator it = deps.find(column);
	if (it == deps.end())
		return false;
	for (std::set<QualifiedCell>::const_iterator d = it->second.begin(); d != it->second.end(); d++)
	{
		if (d->container == dependentContainer && d->loc == dependentLoc)
			return true;
	}
	return false;
}

// Gemella di HasColumnDependent sopra, per fRowDependents.
static bool HasRowDependent(CContainer* watchedContainer, int row,
	CContainer* dependentContainer, cell dependentLoc)
{
	const std::map<int, std::set<QualifiedCell> >& deps = watchedContainer->GetRowDependentsMap();
	std::map<int, std::set<QualifiedCell> >::const_iterator it = deps.find(row);
	if (it == deps.end())
		return false;
	for (std::set<QualifiedCell>::const_iterator d = it->second.begin(); d != it->second.end(); d++)
	{
		if (d->container == dependentContainer && d->loc == dependentLoc)
			return true;
	}
	return false;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestDependencyGraphBuild");

	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	// Parte 1: bordo base, stesso foglio -- A1="=B1+1" referenzia B1.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("5", cell(2, 1), doc, true); // B1
		TryToParseString("=B1+1", cell(1, 1), doc, true); // A1

		Check(HasDependent(doc, cell(2, 1), doc, cell(1, 1)),
			"A1=\"=B1+1\" registra A1 come dipendente di B1");

		doc->Release();
	}

	// Parte 2: modificare la formula sposta il bordo -- A1 passa da
	// referenziare B1 a referenziare C1.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("5", cell(2, 1), doc, true); // B1
		TryToParseString("7", cell(3, 1), doc, true); // C1
		TryToParseString("=B1+1", cell(1, 1), doc, true); // A1
		Check(HasDependent(doc, cell(2, 1), doc, cell(1, 1)),
			"prima della modifica, A1 dipende da B1");

		TryToParseString("=C1+1", cell(1, 1), doc, true); // A1 ora referenzia C1
		Check(!HasDependent(doc, cell(2, 1), doc, cell(1, 1)),
			"dopo la modifica, A1 NON dipende piu' da B1");
		Check(HasDependent(doc, cell(3, 1), doc, cell(1, 1)),
			"dopo la modifica, A1 dipende da C1");

		doc->Release();
	}

	// Parte 3: cancellare la cella dipendente (DisposeCell) rimuove i
	// suoi bordi uscenti.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("5", cell(2, 1), doc, true); // B1
		TryToParseString("=B1+1", cell(1, 1), doc, true); // A1
		Check(HasDependent(doc, cell(2, 1), doc, cell(1, 1)),
			"prima della cancellazione, A1 dipende da B1");

		doc->DisposeCell(cell(1, 1));
		Check(!HasDependent(doc, cell(2, 1), doc, cell(1, 1)),
			"dopo DisposeCell(A1), B1 non ha piu' A1 come dipendente");

		doc->Release();
	}

	// Parte 3b: stesso principio di Parte 3, ma con ClearCellContent
	// (Canc/Backspace) invece di DisposeCell -- un secondo punto di
	// scrittura diretta trovato durante l'implementazione, non elencato
	// nella ricerca originale.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("5", cell(2, 1), doc, true); // B1
		TryToParseString("=B1+1", cell(1, 1), doc, true); // A1
		Check(HasDependent(doc, cell(2, 1), doc, cell(1, 1)),
			"prima di Canc, A1 dipende da B1");

		doc->ClearCellContent(cell(1, 1));
		Check(!HasDependent(doc, cell(2, 1), doc, cell(1, 1)),
			"dopo ClearCellContent(A1) (Canc/Backspace), B1 non ha piu' A1 come dipendente");

		doc->Release();
	}

	// Parte 4: catena a piu' celle, stesso foglio -- A1 -> B1 -> C1.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), doc, true); // A1
		TryToParseString("=A1+1", cell(2, 1), doc, true); // B1
		TryToParseString("=B1+1", cell(3, 1), doc, true); // C1

		Check(HasDependent(doc, cell(1, 1), doc, cell(2, 1)), "B1 dipende da A1");
		Check(HasDependent(doc, cell(2, 1), doc, cell(3, 1)), "C1 dipende da B1");
		Check(!HasDependent(doc, cell(1, 1), doc, cell(3, 1)),
			"C1 NON dipende direttamente da A1 (solo indirettamente via B1)");

		doc->Release();
	}

	// Parte 5: cross-foglio -- Sheet2!A1 = "=Sheet1!A1+1".
	{
		CContainer* sheet1 = new CContainer(NULL, NULL);
		CContainer* sheet2 = new CContainer(NULL, NULL);
		TestResolver resolver;
		resolver.sheet1 = sheet1;
		resolver.sheet2 = sheet2;
		sheet1->SetSheetResolver(&resolver);
		sheet2->SetSheetResolver(&resolver);

		TryToParseString("10", cell(1, 1), sheet1, true); // Sheet1!A1
		TryToParseString("=Sheet1!A1+1", cell(1, 1), sheet2, true); // Sheet2!A1

		Check(HasDependent(sheet1, cell(1, 1), sheet2, cell(1, 1)),
			"Sheet2!A1 e' registrata come dipendente su Sheet1!A1 (bordo qualificato fra fogli)");
		Check(!HasDependent(sheet2, cell(1, 1), sheet2, cell(1, 1)),
			"nessun bordo spurio stesso-foglio per un riferimento incrociato");

		// Un nome di foglio non risolvibile e' un bordo in meno, non un
		// errore -- stesso comportamento di CFormula::Calculate.
		TryToParseString("=FoglioInesistente!A1+1", cell(2, 1), sheet2, true); // Sheet2!B1
		Check(!HasDependent(sheet1, cell(1, 1), sheet2, cell(2, 1)),
			"un riferimento a un foglio inesistente non produce nessun bordo (permissivo, non un errore)");

		sheet1->Release();
		sheet2->Release();
	}

	// Parte 6: inserimento/cancellazione di righe -- MoveCell con
	// split!=noSplit deve rimappare l'identita' della cella dipendente
	// (il caso a rischio piu' alto del piano) e aggiornare i riferimenti
	// che si spostano SENZA che la formula stessa cambi cella.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		// A1=1, A2="=A1+1" (dipende da A1). Inseriamo una riga sopra
		// entrambe: A1->A2, A2->A3, e la formula di quella che era A2
		// (ora A3) deve continuare a referenziare quella che era A1
		// (ora A2) -- stesso identico principio di InsertRows in
		// SheetView.cpp, replicato qui a mano senza bisogno di
		// MainWindow/SheetView per restare un test di solo motore.
		TryToParseString("1", cell(1, 1), doc, true); // A1
		TryToParseString("=A1+1", cell(1, 2), doc, true); // A2
		Check(HasDependent(doc, cell(1, 1), doc, cell(1, 2)),
			"prima dell'inserimento, A2 dipende da A1");

		// Scandisce dal basso verso l'alto, stesso ordine di
		// SheetView::InsertRows (evita di sovrascrivere celle non ancora
		// spostate).
		doc->MoveCell(doc, cell(1, 2), cell(1, 3), vSplit, 1, 1); // A2 -> A3
		doc->MoveCell(doc, cell(1, 1), cell(1, 2), vSplit, 1, 1); // A1 -> A2

		Check(!HasDependent(doc, cell(1, 1), doc, cell(1, 2)),
			"dopo l'inserimento, il vecchio bordo (A1 come precedente di A2) e' sparito");
		Check(HasDependent(doc, cell(1, 2), doc, cell(1, 3)),
			"dopo l'inserimento, la cella ora in A3 (era A2) dipende dalla cella ora in A2 (era A1)");

		doc->Release();
	}

	// Parte 6b: split in place (srcLoc == destLoc) -- una formula non si
	// sposta lei stessa, ma i SUOI riferimenti si aggiornano perche' una
	// riga e' stata inserita/cancellata nelle vicinanze.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), doc, true); // A1
		TryToParseString("5", cell(1, 2), doc, true); // A2
		TryToParseString("=A2+1", cell(3, 1), doc, true); // C1, resta ferma, referenzia A2
		Check(HasDependent(doc, cell(1, 2), doc, cell(3, 1)), "prima dello split, C1 dipende da A2");

		// Riga inserita sopra la riga 2: C1 non si sposta (resta C1), ma
		// il suo riferimento ad A2 deve diventare A3.
		doc->MoveCell(doc, cell(3, 1), cell(3, 1), vSplit, 2, 1);

		Check(!HasDependent(doc, cell(1, 2), doc, cell(3, 1)),
			"dopo lo split, C1 non dipende piu' dalla vecchia posizione (A2)");
		Check(HasDependent(doc, cell(1, 3), doc, cell(3, 1)),
			"dopo lo split, C1 dipende dalla nuova posizione del suo riferimento (A3)");

		doc->Release();
	}

	// Parte 7: cancellazione di un foglio -- PurgeDependenciesOn deve
	// togliere ogni bordo che PUNTA al foglio che sta per sparire (come
	// QualifiedCell::container di un dipendente), altrimenti quel
	// puntatore penderebbe. Il caso davvero a rischio e' quando il
	// bordo vive sul foglio SUPERSTITE: qui sheet1 (che sta per essere
	// cancellato) DIPENDE da sheet2 (che resta vivo), quindi il bordo
	// e' registrato dentro sheet2->fDependents[...] con
	// QualifiedCell::container=sheet1 -- esattamente il puntatore che
	// altrimenti penderebbe dopo la cancellazione di sheet1.
	{
		CContainer* sheet1 = new CContainer(NULL, NULL);
		CContainer* sheet2 = new CContainer(NULL, NULL);
		TestResolver resolver;
		resolver.sheet1 = sheet1;
		resolver.sheet2 = sheet2;
		sheet1->SetSheetResolver(&resolver);
		sheet2->SetSheetResolver(&resolver);

		TryToParseString("10", cell(1, 1), sheet2, true); // Sheet2!A1
		TryToParseString("=Sheet2!A1+1", cell(2, 1), sheet1, true); // Sheet1!B1, dipende da Sheet2!A1
		Check(HasDependent(sheet2, cell(1, 1), sheet1, cell(2, 1)),
			"prima della cancellazione di sheet1, sheet2 porta un bordo verso Sheet1!B1");

		// sheet1 sta per essere cancellato: ogni ALTRO foglio ancora
		// vivo (qui solo sheet2) deve togliere i bordi che puntano a
		// lui, esattamente come MainWindow::DeleteSheetNoConfirm fa
		// PRIMA di Release() sul foglio in uscita.
		sheet2->PurgeDependenciesOn(sheet1);

		Check(!HasDependent(sheet2, cell(1, 1), sheet1, cell(2, 1)),
			"dopo PurgeDependenciesOn(sheet1) su sheet2, il bordo verso il foglio cancellato e' sparito");

		sheet1->Release();
		sheet2->Release();
	}

	// Parte 8: riferimento a colonna intera -- "=SUM(A:A)" produce UN
	// solo bordo grezzo in fColumnDependents, non migliaia di voci
	// individuali in fDependents.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), doc, true); // A1
		TryToParseString("2", cell(1, 2), doc, true); // A2
		TryToParseString("=SUM(A:A)", cell(2, 1), doc, true); // B1

		Check(HasColumnDependent(doc, 1, doc, cell(2, 1)),
			"B1=\"=SUM(A:A)\" registra un bordo grezzo su fColumnDependents[1] (colonna A)");
		Check(!HasDependent(doc, cell(1, 1), doc, cell(2, 1)),
			"nessun bordo individuale su A1 (assorbito dal bordo grezzo di colonna)");
		Check(!HasDependent(doc, cell(1, 2), doc, cell(2, 1)),
			"nessun bordo individuale su A2 (assorbito dal bordo grezzo di colonna)");

		doc->Release();
	}

	// Parte 9: gemella della Parte 8 per una riga intera. Il parser di
	// questo motore non accetta la sintassi letterale "1:1" (un numero
	// puro non e' un token di riferimento, a differenza di una lettera
	// di colonna -- confermato da un vero CParseErr durante lo sviluppo
	// di questo test), quindi si scrive un intervallo esplicito che
	// copre l'intera larghezza del foglio (A1:ZZ1, ZZ = kColCount = 702):
	// range::IsWholeRow() lo riconosce comunque come riga intera per
	// approssimazione prudente (vedi il commento su IsWholeRow in
	// Range.h), stesso trattamento grezzo di "1:1" se il parser lo
	// avesse accettato.
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), doc, true); // A1
		TryToParseString("2", cell(2, 1), doc, true); // B1
		TryToParseString("=SUM(A1:ZZ1)", cell(1, 2), doc, true); // A2

		Check(HasRowDependent(doc, 1, doc, cell(1, 2)),
			"A2=\"=SUM(A1:ZZ1)\" registra un bordo grezzo su fRowDependents[1] (riga 1)");
		Check(!HasDependent(doc, cell(1, 1), doc, cell(1, 2)),
			"nessun bordo individuale su A1 (assorbito dal bordo grezzo di riga)");
		Check(!HasDependent(doc, cell(2, 1), doc, cell(1, 2)),
			"nessun bordo individuale su B1 (assorbito dal bordo grezzo di riga)");

		doc->Release();
	}

	// Parte 10: riferimento a colonna intera CROSS-FOGLIO -- Sheet2 ha
	// "=SUM(Sheet1!A:A)", il bordo grezzo deve comparire su
	// fColumnDependents di Sheet1 (il foglio osservato), qualificato con
	// Sheet2 come dipendente -- stessa idea della Parte 5 (cross-foglio
	// puntuale) applicata al caso grezzo della Parte 8.
	{
		CContainer* sheet1 = new CContainer(NULL, NULL);
		CContainer* sheet2 = new CContainer(NULL, NULL);
		TestResolver resolver;
		resolver.sheet1 = sheet1;
		resolver.sheet2 = sheet2;
		sheet1->SetSheetResolver(&resolver);
		sheet2->SetSheetResolver(&resolver);

		TryToParseString("1", cell(1, 1), sheet1, true); // Sheet1!A1
		TryToParseString("2", cell(1, 2), sheet1, true); // Sheet1!A2
		TryToParseString("=SUM(Sheet1!A:A)", cell(1, 1), sheet2, true); // Sheet2!A1

		Check(HasColumnDependent(sheet1, 1, sheet2, cell(1, 1)),
			"Sheet2!A1=\"=SUM(Sheet1!A:A)\" registra un bordo grezzo su fColumnDependents "
			"di Sheet1 (colonna A), qualificato con Sheet2");
		Check(!HasDependent(sheet1, cell(1, 1), sheet2, cell(1, 1)),
			"nessun bordo individuale su Sheet1!A1 (assorbito dal bordo grezzo di colonna)");

		sheet1->Release();
		sheet2->Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
