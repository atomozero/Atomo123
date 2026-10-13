/*
	test_goal_seek.cpp

	Verifica Ricerca obiettivo (Tier 3, "Path to full Excel parity" --
	Goal Seek): MainWindow::ApplyGoalSeek, chiamata direttamente (e'
	pubblica, stesso principio di ApplyWhatIfDataTable in
	test_whatif.cpp). Copre un caso lineare, uno non lineare (cubo, per
	verificare che il metodo delle secanti funzioni davvero e non sia
	solo un caso fortunato di algebra lineare), ogni forma di input non
	valido, un obiettivo irraggiungibile (non converge, la cella da
	cambiare torna al contenuto originale), e Annulla sul solo caso
	convergente.
*/

#include <cstdio>
#include <cstring>

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
#include "ResourceManager.h"
#include "SheetView.h"
#include "MainWindow.h"

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

static bool NearlyEqual(double a, double b, double tolerance = 0.001)
{
	double diff = a - b;
	if (diff < 0)
		diff = -diff;
	return diff <= tolerance;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestGoalSeek");

	app_info info;
	if (app.GetAppInfo(&info) == B_OK) {
		BPath path(&info.ref);
		if (path.InitCheck() == B_OK) {
			gAppName = path;
			gResourceManager.SetTo(&path);
			try {
				InitFunctions();
			} catch (CErr& e) {
				printf("FAIL impossibile caricare la tabella funzioni: %s\n", (char*)e);
				return 1;
			}
		}
	}

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// --- Caso lineare: B1 = A1*3+1, obiettivo 100 cambiando A1. ---
	TryToParseString("5", cell(1, 1), doc, true);       // A1: valore di partenza
	TryToParseString("=A1*3+1", cell(2, 1), doc, true); // B1: cella formula

	double achieved = 0.0;
	MainWindow::GoalSeekOutcome outcome =
		win->ApplyGoalSeek("B1", "100", "A1", &achieved);

	Check(outcome == MainWindow::eGoalSeekConverged,
		"caso lineare: la ricerca converge su un'equazione di primo grado");
	Check(NearlyEqual(CellNumber(doc, cell(1, 1)), 33.0),
		"caso lineare: A1 diventa 33 (33*3+1 = 100)");
	Check(NearlyEqual(CellNumber(doc, cell(2, 1)), 100.0),
		"caso lineare: B1 raggiunge davvero 100 dopo il ricalcolo");
	Check(NearlyEqual(achieved, 100.0),
		"caso lineare: outAchievedValue riporta il valore davvero raggiunto");

	Check(view->CanUndo(), "caso lineare: dopo una convergenza, Annulla e' disponibile");
	view->Undo();
	Check(NearlyEqual(CellNumber(doc, cell(1, 1)), 5.0),
		"caso lineare: Annulla riporta A1 al valore originale (5)");
	// Ripristina lo stato "risolto" per i controlli successivi (il
	// resto del file non dipende da questo, ma resta piu' chiaro).
	TryToParseString("=A1*3+1", cell(2, 1), doc, true);

	// --- Caso non lineare: D1 = C1^3, obiettivo 1000 cambiando C1
	// (C1 = 10, non un caso risolvibile per via puramente lineare --
	// prova vera del metodo delle secanti, non solo algebra lineare). ---
	TryToParseString("2", cell(3, 1), doc, true);           // C1: valore di partenza
	TryToParseString("=C1*C1*C1", cell(4, 1), doc, true);   // D1: cella formula

	outcome = win->ApplyGoalSeek("D1", "1000", "C1", &achieved);
	Check(outcome == MainWindow::eGoalSeekConverged,
		"caso cubico: la ricerca converge anche su un'equazione non lineare");
	Check(NearlyEqual(CellNumber(doc, cell(3, 1)), 10.0, 0.01),
		"caso cubico: C1 converge a 10 (10^3 = 1000)");
	Check(NearlyEqual(CellNumber(doc, cell(4, 1)), 1000.0, 1.0),
		"caso cubico: D1 raggiunge davvero circa 1000 dopo il ricalcolo");

	// --- Obiettivo irraggiungibile: F1 = 5 (una formula che NON
	// dipende affatto dalla cella da cambiare, E1) -- ogni tentativo da'
	// sempre lo stesso residuo, il metodo delle secanti si ferma subito
	// (f1==f0) invece di proseguire all'infinito. E1 deve tornare al suo
	// contenuto originale, non restare all'ultimo valore di tentativo.
	// "=G1" (non "=5" letterale): un'espressione COSTANTE si riduce a
	// un valore letterale gia' in fase di analisi (ReduceToValue in
	// CellParser.cpp), quindi non resterebbe affatto una formula vera
	// (GetCellFormula tornerebbe NULL) -- un riferimento a cella vero,
	// anche se a un'altra costante, e' l'unico modo di avere una
	// formula autentica che pero' non dipenda da E1. ---
	TryToParseString("777", cell(5, 1), doc, true); // E1: cella da cambiare, valore "a riposo"
	TryToParseString("5", cell(8, 1), doc, true);   // H1: valore fisso indipendente
	TryToParseString("=H1", cell(6, 1), doc, true); // F1: formula vera, ma non dipende da E1
	// Calcolata una prima volta esplicitamente: F1 non dipende da E1,
	// quindi ApplyGoalSeek sotto (che ricalcola solo a partire da E1)
	// non la tocca mai da sola -- esattamente come un vero foglio,
	// dove ogni cella mostra gia' il proprio valore calcolato PRIMA che
	// un utente apra Ricerca obiettivo, non come una formula
	// appena scritta e mai ancora calcolata.
	doc->CalcCell(cell(6, 1));

	outcome = win->ApplyGoalSeek("F1", "10", "E1", &achieved);
	Check(outcome == MainWindow::eGoalSeekNotConverged,
		"obiettivo irraggiungibile: la ricerca NON converge (F1 non puo' mai valere 10)");
	Check(NearlyEqual(CellNumber(doc, cell(5, 1)), 777.0),
		"obiettivo irraggiungibile: E1 torna al valore originale (777), non resta a un tentativo");
	Check(NearlyEqual(achieved, 5.0),
		"obiettivo irraggiungibile: outAchievedValue riporta il valore vero (5) mai modificabile a 10");

	// --- Input non validi: ognuno restituisce eGoalSeekBadInput, mai un
	// crash ne' una modifica silenziosa al documento. ---
	BString errorDetail;
	outcome = win->ApplyGoalSeek("", "100", "A1", NULL, &errorDetail);
	Check(outcome == MainWindow::eGoalSeekBadInput,
		"campo \"Imposta\" vuoto restituisce eGoalSeekBadInput");
	Check(errorDetail.Length() > 0, "...con un messaggio di errore non vuoto");

	outcome = win->ApplyGoalSeek("B1", "", "A1");
	Check(outcome == MainWindow::eGoalSeekBadInput, "campo \"Al valore\" vuoto restituisce eGoalSeekBadInput");

	outcome = win->ApplyGoalSeek("B1", "100", "");
	Check(outcome == MainWindow::eGoalSeekBadInput,
		"campo \"Cambiando la cella\" vuoto restituisce eGoalSeekBadInput");

	outcome = win->ApplyGoalSeek("NonUnaCella!!!", "100", "A1");
	Check(outcome == MainWindow::eGoalSeekBadInput,
		"riferimento \"Imposta\" non valido restituisce eGoalSeekBadInput, non un crash");

	outcome = win->ApplyGoalSeek("B1", "100", "NonUnaCella!!!");
	Check(outcome == MainWindow::eGoalSeekBadInput,
		"riferimento della cella da cambiare non valido restituisce eGoalSeekBadInput");

	outcome = win->ApplyGoalSeek("B1", "non-un-numero", "A1");
	Check(outcome == MainWindow::eGoalSeekBadInput,
		"valore obiettivo non numerico restituisce eGoalSeekBadInput");

	// G1 e' un valore LETTERALE (nessuna formula): la cella "Imposta"
	// deve davvero contenere una formula, altrimenti cambiare un'altra
	// cella non potrebbe mai spostarne il risultato.
	TryToParseString("42", cell(7, 1), doc, true); // G1: letterale, non una formula
	outcome = win->ApplyGoalSeek("G1", "100", "A1");
	Check(outcome == MainWindow::eGoalSeekBadInput,
		"la cella \"Imposta\" senza una formula vera restituisce eGoalSeekBadInput");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
