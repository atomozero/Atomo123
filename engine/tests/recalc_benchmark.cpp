/*
	recalc_benchmark.cpp

	Micro-benchmark headless del ricalcolo a punto fisso di Atomo123.

	Replica ESATTAMENTE la logica di RecalculatePass/RecalculateAll di
	ui/src/AscdIO.cpp (iterazione su tutte le celle esistenti + ciclo
	"finche' nulla cambia", con il tetto di sicurezza di 50 passate),
	ma la esercita su fogli generati con dimensioni e forme diverse per
	misurare QUANDO il modello attuale inizia a soffrire.

	Scopo: fornire numeri su cui decidere se valga la pena aggiungere un
	vero grafo delle dipendenze (dirty-set + ordine topologico) al posto
	dell'iterazione a punto fisso. NON cambia il motore: lo misura.

	Tre forme di dipendenza, tutte su una singola colonna A (righe 1..N):

	  - FORWARD  (Ai = A(i-1)+1): ogni cella referenzia quella VISITATA
	    PRIMA di lei nella passata -> converge in ~1-2 passate a
	    qualunque N. E' il caso "fortunato": misura il costo puro di una
	    passata completa (N * log N).

	  - BACKWARD (Ai = A(i+1)+1): ogni cella referenzia quella VISITATA
	    DOPO di lei -> ogni passata propaga un solo livello, servono ~N
	    passate. E' il caso peggiore: mostra il fattore D. Con N > 50 il
	    guard taglia il ricalcolo PRIMA della convergenza -> il valore
	    finale e' SBAGLIATO senza alcun errore (la "scogliera" del cap).

	  - WIDE     (Ai = i+0, indipendenti): D = 1, converge in 2 passate.
	    Isola il costo per-cella slegato dalla profondita'.

	Per ogni scenario misura anche il costo REALISTICO per singola
	modifica: cambiare una cella e richiamare RecalculateAll una volta
	(cio' che CommitEditing fa a ogni conferma).

	Infine una variante "FREEZE UI": il ricalcolo gira SINCRONO sul
	thread della finestra (BLooper), quindi la sua durata e' esattamente
	il tempo per cui la UI resta bloccata a ogni conferma di cella.
	Questa variante replica RecalculateWorkbook (ricalcolo dell'INTERA
	cartella multi-foglio, il percorso piu' costoso) su cartelle di
	dimensioni realistiche -- fino al file di gara da 38 fogli che ha
	motivato la Fase 9 -- e traduce i millisecondi in un verdetto di
	percezione utente. E' un limite inferiore del freeze reale: non
	include il ridisegno via app_server che segue il ricalcolo.
*/

#include <cstdio>
#include <cstring>
#include <chrono>
#include <string>
#include <vector>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "CellIterator.h"

using Clock = std::chrono::steady_clock;

static double MsSince(Clock::time_point t0)
{
	return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// --- Replica fedele di ui/src/AscdIO.cpp -------------------------------

static bool RecalculatePass(CContainer* doc)
{
	bool changed = false;
	CCellIterator iter(doc, NULL);
	cell c;
	while (iter.NextExisting(c))
	{
		if (doc->CalcCell(c))
			changed = true;
	}
	return changed;
}

// Ritorna il numero di passate effettuate (per capire se ha raggiunto
// la convergenza o e' stato tagliato dal guard di 50).
static int RecalculateAll(CContainer* doc)
{
	bool changed = true;
	int guard = 0;
	while (changed && guard < 50)
	{
		changed = RecalculatePass(doc);
		guard++;
	}
	return guard;
}

// Replica di RecalculateWorkbook (ui/src/AscdIO.cpp): a ogni passata
// ri-scansiona TUTTI i fogli, non uno alla volta, perche' una formula
// puo' referenziare un altro foglio (Fase 9). E' il percorso che
// RecalculateActiveWorkbook prende su una cartella multi-foglio -- il
// piu' costoso, ed e' cio' che gira sincrono sul thread della finestra
// a ogni conferma di cella. Ritorna il numero di passate.
static int RecalculateWorkbook(std::vector<CContainer*>& sheets)
{
	bool changed = true;
	int guard = 0;
	while (changed && guard < 50)
	{
		changed = false;
		for (size_t i = 0; i < sheets.size(); i++)
		{
			if (RecalculatePass(sheets[i]))
				changed = true;
		}
		guard++;
	}
	return guard;
}

// --- Generatori di fogli ----------------------------------------------

enum Shape { kForward, kBackward, kWide };

static CContainer* BuildSheet(int n, Shape shape)
{
	CContainer* doc = new CContainer(NULL, NULL);
	char buf[64];

	for (int i = 1; i <= n; i++)
	{
		cell c(1, i); // colonna A (h=1), riga i (v=i)
		switch (shape)
		{
			case kForward:
				if (i == 1) strcpy(buf, "1");
				else        snprintf(buf, sizeof(buf), "=A%d+1", i - 1);
				break;
			case kBackward:
				if (i == n) strcpy(buf, "1");
				else        snprintf(buf, sizeof(buf), "=A%d+1", i + 1);
				break;
			case kWide:
				snprintf(buf, sizeof(buf), "=%d+0", i);
				break;
		}
		TryToParseString(buf, c, doc, true);
	}
	return doc;
}

// Valore atteso a convergenza per la cella "sonda" di coda.
static double ExpectedTail(int n, Shape shape, cell& probe)
{
	switch (shape)
	{
		case kForward:  probe = cell(1, n); return (double)n;        // An = n
		case kBackward: probe = cell(1, 1); return (double)n;        // A1 = n
		case kWide:     probe = cell(1, n); return (double)n;        // An = n
	}
	probe = cell(1, 1);
	return 0;
}

static const char* ShapeName(Shape s)
{
	switch (s) {
		case kForward:  return "FORWARD ";
		case kBackward: return "BACKWARD";
		case kWide:     return "WIDE    ";
	}
	return "?";
}

// --- Un caso di misura -------------------------------------------------

static void RunCase(int n, Shape shape)
{
	Clock::time_point t0 = Clock::now();
	CContainer* doc = BuildSheet(n, shape);
	double buildMs = MsSince(t0);

	// Ricalcolo completo "a freddo" (prima apertura / caricamento file).
	t0 = Clock::now();
	int passes = RecalculateAll(doc);
	double coldMs = MsSince(t0);

	// Correttezza: la coda ha davvero convergiuto al valore atteso?
	cell probe(1, 1);
	double expected = ExpectedTail(n, shape, probe);
	Value v;
	doc->GetValue(probe, v);
	double got = (double)v;
	bool converged = (got == expected);

	// Costo REALISTICO per singola modifica: cambio una costante alla
	// radice della catena e richiamo RecalculateAll una volta sola,
	// come fa CommitEditing a ogni conferma di cella.
	cell root = (shape == kBackward) ? cell(1, n) : cell(1, 1);
	TryToParseString("2", root, doc, true);
	t0 = Clock::now();
	int editPasses = RecalculateAll(doc);
	double editMs = MsSince(t0);

	printf("  %s  N=%6d | build %8.2f ms | ricalc a freddo %8.2f ms (%2d passate)"
	       " | 1 modifica %8.2f ms (%2d passate) | coda %s\n",
	       ShapeName(shape), n, buildMs, coldMs, passes, editMs, editPasses,
	       converged ? "OK" : "*** NON CONVERGE (cap 50) ***");

	doc->Release();
}

// --- Variante: freeze percepito della UI ------------------------------

// Traduce una durata in millisecondi nella sensazione all'utente. Il
// ricalcolo gira SINCRONO sul thread della finestra (BLooper): per
// tutto questo tempo la UI non ridisegna, non risponde al mouse/
// tastiera, non si puo' annullare -> e' esattamente la durata del
// "freeze".
static const char* FreezeVerdict(double ms)
{
	if (ms < 16.0)   return "impercettibile (< 1 frame a 60 fps)";
	if (ms < 100.0)  return "fluido";
	if (ms < 1000.0) return "lag visibile";
	return "FREEZE evidente (UI bloccata > 1 s)";
}

// Simula il costo di UNA modifica di cella su una cartella multi-foglio,
// cioe' cio' che RecalculateActiveWorkbook -> RecalculateWorkbook fa
// sincrono sul thread della finestra dopo ogni conferma. Ogni foglio e'
// una catena FORWARD (caso realistico e "ben formato": converge in
// poche passate, quindi il costo NON viene dai riferimenti circolari ma
// dal semplice ri-scansionare tutte le celle di tutti i fogli).
//
// Il valore misurato e' un LIMITE INFERIORE del freeze reale: l'app,
// subito dopo, chiama anche fSheetView->Invalidate() (ridisegno via
// app_server), non misurabile headless, che si somma a questo tempo.
static void RunFreezeCase(int nSheets, int nPerSheet)
{
	std::vector<CContainer*> sheets;
	sheets.reserve(nSheets);
	for (int s = 0; s < nSheets; s++)
		sheets.push_back(BuildSheet(nPerSheet, kForward));

	// Convergenza iniziale (apertura file), non cronometrata qui.
	RecalculateWorkbook(sheets);

	// Una singola modifica utente: cambio la radice del primo foglio e
	// misuro il ricalcolo sincrono che ne segue.
	TryToParseString("2", cell(1, 1), sheets[0], true);

	Clock::time_point t0 = Clock::now();
	int passes = RecalculateWorkbook(sheets);
	double freezeMs = MsSince(t0);

	long totalCells = (long)nSheets * nPerSheet;
	printf("  %2d fogli x %6d celle (= %8ld totali) | freeze %9.2f ms"
	       " (%2d passate) | %s\n",
	       nSheets, nPerSheet, totalCells, freezeMs, passes, FreezeVerdict(freezeMs));

	for (size_t i = 0; i < sheets.size(); i++)
		sheets[i]->Release();
}

int main()
{
	// 40 e 60 servono a mostrare la soglia del guard di 50 passate nel
	// caso BACKWARD: N=40 converge (< 50 livelli), N=60 viene tagliato.
	// Il massimo e' 16000: kRowCount (Config/Constants.h) limita un
	// foglio a 16384 righe, e queste catene stanno tutte sulla colonna A.
	const int sizes[] = { 40, 60, 500, 1000, 2000, 5000, 10000, 16000 };
	const int nSizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

	printf("Benchmark ricalcolo a punto fisso (replica di RecalculateAll)\n");
	printf("=============================================================\n\n");

	printf("WIDE: N formule indipendenti (profondita' D=1) -- costo di una passata\n");
	for (int i = 0; i < nSizes; i++) RunCase(sizes[i], kWide);

	printf("\nFORWARD: catena Ai=A(i-1)+1, ordine di visita = ordine di dipendenza\n");
	printf("         (caso fortunato: converge subito, D non pesa)\n");
	for (int i = 0; i < nSizes; i++) RunCase(sizes[i], kForward);

	printf("\nBACKWARD: catena Ai=A(i+1)+1, ordine di visita = INVERSO alle dipendenze\n");
	printf("          (caso peggiore: una passata per livello -> fattore D)\n");
	for (int i = 0; i < nSizes; i++) RunCase(sizes[i], kBackward);

	printf("\nNota: nelle catene BACKWARD con N>50 il ciclo si ferma al guard di\n");
	printf("50 passate PRIMA della convergenza: il valore di coda resta sbagliato\n");
	printf("senza segnalare alcun errore. E' la scogliera di correttezza del\n");
	printf("modello a punto fisso descritta nell'analisi.\n");

	printf("\nFREEZE UI: durata del ricalcolo SINCRONO sul thread finestra dopo\n");
	printf("          UNA modifica su una cartella multi-foglio (fogli ben\n");
	printf("          formati, catene FORWARD). E' il tempo per cui la UI resta\n");
	printf("          bloccata a ogni conferma di cella (limite inferiore: non\n");
	printf("          include il ridisegno via app_server che segue).\n");
	RunFreezeCase(1,  5000);
	RunFreezeCase(1,  16000);  // foglio singolo quasi pieno (max kRowCount)
	RunFreezeCase(10, 2000);
	RunFreezeCase(38, 1000);   // il file di gara reale che ha motivato la Fase 9
	RunFreezeCase(38, 5000);
	printf("\nIl ricalcolo gira sul thread della finestra (BLooper), quindi la\n");
	printf("durata sopra e' esattamente il tempo di freeze percepito. Spostarlo\n");
	printf("su un thread worker (l'inutilizzato CCalcThread) toglie il freeze\n");
	printf("senza cambiare i risultati; un grafo delle dipendenze ne abbatte\n");
	printf("anche la durata ricalcolando solo le celle a valle della modifica.\n");

	return 0;
}
