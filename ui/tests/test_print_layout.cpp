/*
	test_print_layout.cpp

	Verifica ComputePrintPageOrigins (PrintLayout.h/.cpp): il calcolo
	puro di quante pagine servono per stampare un foglio e da dove
	inizia ognuna, ripetendo l'intestazione su OGNI pagina. Nessuna
	BPrintJob vera (serve una stampante configurata) -- solo la
	matematica del posizionamento, vedi il commento su
	MainWindow::PrintDocument per la derivazione completa.
*/

#include <cstdio>

#include "PrintLayout.h"

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
	// Contenuto che sta in una sola pagina: una sola origine, (0,0) --
	// stesso comportamento della prima pagina di sempre (nessuna
	// regressione per il caso comune, un solo foglio a una pagina).
	{
		BRect content(0, 0, 50, 30);
		std::vector<BPoint> origins = ComputePrintPageOrigins(content, 110, 55, 10, 5);
		Check(origins.size() == 1, "un contenuto che sta in una pagina produce una sola origine");
		if (origins.size() == 1)
			Check(origins[0] == BPoint(0, 0), "l'unica pagina inizia a (0,0), come sempre");
	}

	// Contenuto piu' largo/alto di una pagina: griglia 3x3 di pagine,
	// con passo (pageWidth-headerW)/(pageHeight-headerH) invece
	// dell'intera pagina -- lo scarto di headerW/headerH e' riservato
	// all'intestazione ripetuta su ogni pagina dopo la prima.
	{
		BRect content(0, 0, 250, 120);
		std::vector<BPoint> origins = ComputePrintPageOrigins(content, 110, 55, 10, 5);
		Check(origins.size() == 9, "un contenuto 250x120 con pagine 110x55 (intestazione 10x5) "
			"produce una griglia di 9 pagine (3 colonne x 3 righe)");

		BPoint expected[9] = {
			BPoint(0, 0), BPoint(100, 0), BPoint(200, 0),
			BPoint(0, 50), BPoint(100, 50), BPoint(200, 50),
			BPoint(0, 100), BPoint(100, 100), BPoint(200, 100),
		};
		bool allMatch = origins.size() == 9;
		for (size_t i = 0; allMatch && i < 9; i++)
			allMatch = (origins[i] == expected[i]);
		Check(allMatch, "le origini delle 9 pagine sono esattamente quelle attese, "
			"in ordine riga per riga (passo 100x50, non 110x55)");
	}

	// Continuita' fra pagine: il contenuto DATI (non l'intestazione) di
	// una pagina deve iniziare esattamente dove finiva quello della
	// pagina precedente sulla stessa riga -- l'origine di ogni pagina
	// e' gia' spostata indietro di headerW rispetto al dato vero
	// (origin.x + headerW), quindi la differenza fra un'origine e la
	// successiva deve essere ESATTAMENTE (pageWidth-headerW): un
	// valore diverso vorrebbe dire un buco (dati mai stampati) o una
	// sovrapposizione VISIBILE (dati ripetuti, non solo la striscia
	// coperta dall'intestazione).
	{
		BRect content(0, 0, 500, 40);
		float pageWidth = 200, pageHeight = 100, headerW = 30, headerH = 20;
		std::vector<BPoint> origins = ComputePrintPageOrigins(content, pageWidth, pageHeight,
			headerW, headerH);
		Check(origins.size() >= 2, "il contenuto di prova genera almeno due pagine in orizzontale");
		bool stepOk = true;
		for (size_t i = 1; i < origins.size(); i++)
			stepOk = stepOk && (origins[i].x - origins[i - 1].x == pageWidth - headerW);
		Check(stepOk, "ogni pagina continua esattamente dove finiva la precedente "
			"(passo == pageWidth-headerW, nessun buco ne' sovrapposizione visibile)");
	}

	// Pagina piu' stretta/bassa della sola intestazione: nessuna pagina
	// puo' contenere dati reali, elenco vuoto invece di un ciclo
	// infinito (il chiamante deve gia' escludere questo caso, ma la
	// funzione pura non deve comunque bloccarsi).
	{
		BRect content(0, 0, 500, 500);
		std::vector<BPoint> origins = ComputePrintPageOrigins(content, 10, 100, 30, 5);
		Check(origins.empty(), "una pagina piu' stretta della sola intestazione (10 < headerW 30) "
			"produce un elenco vuoto, non un ciclo infinito");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
