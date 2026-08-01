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
*/

#include <cstdio>
#include <cstring>
#include <chrono>
#include <string>

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

int main()
{
	// 40 e 60 servono a mostrare la soglia del guard di 50 passate nel
	// caso BACKWARD: N=40 converge (< 50 livelli), N=60 viene tagliato.
	const int sizes[] = { 40, 60, 500, 1000, 2000, 5000, 10000, 20000 };
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

	return 0;
}
