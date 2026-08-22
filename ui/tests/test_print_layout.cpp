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

	// Area di stampa (Fase 27) che NON comincia dalla riga/colonna 1
	// del foglio (contentRect.left/top diversi da zero, es. un'area
	// scelta a partire dalla colonna D): la prima pagina deve iniziare
	// esattamente da contentRect.left/top vero, non da 0 (il caso
	// normale, contentRect da 0,0) ne' da un valore "relativo" alla
	// sola area -- le etichette di riga/colonna in SheetView::Draw
	// usano gia' la posizione REALE di ogni riga/colonna, quindi
	// l'unica cosa che deve cambiare qui e' da dove si comincia a
	// scorrere la vista.
	{
		BRect content(300, 150, 700, 350); // es. un'area che comincia ben oltre l'origine
		float pageWidth = 200, pageHeight = 100, headerW = 30, headerH = 20;
		std::vector<BPoint> origins = ComputePrintPageOrigins(content, pageWidth, pageHeight,
			headerW, headerH);
		Check(!origins.empty(), "un'area di stampa che non comincia da riga/colonna 1 produce comunque pagine");
		Check(origins[0] == BPoint(content.left - headerW, content.top - headerH),
			"la prima pagina di un'area di stampa che comincia a (300,150) parte da "
			"(300-headerW,150-headerH), non da (0,0)");
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

	// ComputePrintFitScale (Fase 27, vedi ROADMAP.md "v3.0
	// Consolidation"): la scala che fa stare un contenuto in una sola
	// pagina di larghezza/altezza/entrambe.
	{
		BRect content(0, 0, 600, 300);
		float scale = ComputePrintFitScale(content, 300, 1000, 30, 20, kPrintFitWidth);
		Check(scale > 0.499f && scale < 0.501f,
			"adatta alla LARGHEZZA: contenuto 600 largo (intestazione compresa) in 300 disponibili "
			"da' scala 0.5");
	}

	{
		BRect content(0, 0, 600, 300);
		float scale = ComputePrintFitScale(content, 300, 100, 30, 20, kPrintFitBoth);
		Check(scale > 0.332f && scale < 0.334f,
			"adatta a UNA PAGINA (entrambe le dimensioni): usa la piu' restrittiva delle due "
			"(altezza, 100/300=0.333), non la larghezza (300/600=0.5)");
	}

	{
		BRect content(0, 0, 600, 300);
		float scale = ComputePrintFitScale(content, 1000, 1000, 30, 20, kPrintFitWidth);
		Check(scale == 1.0f,
			"adatta non ingrandisce MAI un contenuto che gia' ci sta (scala clampata a 1.0, "
			"non 1000/600=1.667)");
	}

	{
		// Stessa area di stampa non allineata all'origine del test sopra
		// (300,150)-(700,350): la larghezza VERA da adattare e' 700-300+30
		// (intestazione compresa), non 700 da solo (che includerebbe
		// anche lo spazio PRIMA dell'area, mai stampato).
		BRect content(300, 150, 700, 350);
		float scale = ComputePrintFitScale(content, 430, 1000, 30, 20, kPrintFitWidth);
		Check(scale > 0.999f && scale < 1.001f,
			"adatta alla larghezza di un'area di stampa che non comincia da riga/colonna 1 "
			"usa la larghezza VERA dell'area (400+headerW=430), non contentRect.right da solo (700)");
	}

	// ComputePrintJobLayout (Fase 28, anteprima in "Imposta pagina"):
	// unica fonte di verita' condivisa fra PrintDocument (stampa vera)
	// e GeneratePrintPreviewPages (anteprima) -- combina conversione
	// margini cm->pixel, scelta della scala e ComputePrintPageOrigins
	// in un'unica chiamata.
	{
		// Margini di 1.27cm (0.5") a 100dpi = esattamente 50px -- verifica
		// la conversione cm->pixel, non solo che il risultato "sembri
		// ragionevole".
		BRect content(0, 0, 50, 30);
		PrintJobLayout layout = ComputePrintJobLayout(content, 400, 300, 100, 100,
			1.27, 1.27, 1.27, 1.27, 0, 50.0, 10, 5);
		Check(layout.marginTopPx > 49.9f && layout.marginTopPx < 50.1f,
			"ComputePrintJobLayout converte 1.27cm a 100dpi in esattamente 50px di margine superiore");
		Check(layout.marginLeftPx > 49.9f && layout.marginLeftPx < 50.1f,
			"...e allo stesso modo per il margine sinistro");
		Check(layout.scale > 0.499 && layout.scale < 0.501,
			"in modalita' percentuale (scaleMode 0), la scala e' esattamente scalePercent/100 (50%)");
		Check(layout.pageWidth > 599.9f && layout.pageWidth < 600.1f,
			"pageWidth e' usableWidth/scale (300/0.5=600), non usableWidth da solo");
		Check(layout.pageOrigins.size() == 1 && !layout.pageOrigins.empty()
			&& layout.pageOrigins[0] == BPoint(0, 0),
			"un contenuto che sta in una pagina produce una sola origine a (0,0), "
			"stessa identica risposta di ComputePrintPageOrigins chiamata a mano");
	}

	{
		// Stessi numeri del test "adatta alla LARGHEZZA" di
		// ComputePrintFitScale sopra (contenuto 600 largo in 300
		// disponibili da' scala 0.5): margini a 0 per isolare solo la
		// combinazione fit-mode + pagine, gia' verificata a parte.
		BRect content(0, 0, 600, 300);
		PrintJobLayout layout = ComputePrintJobLayout(content, 300, 1000, 100, 100,
			0, 0, 0, 0, kPrintFitWidth, 999.0, 30, 20);
		Check(layout.scale > 0.499 && layout.scale < 0.501,
			"in modalita' 'adatta alla larghezza', la scala usata e' esattamente quella di "
			"ComputePrintFitScale (0.5), scalePercent (999) viene ignorato");
		Check(layout.pageOrigins.size() == 1 && !layout.pageOrigins.empty()
			&& layout.pageOrigins[0] == BPoint(0, 0),
			"le origini di pagina risultanti sono le stesse di ComputePrintPageOrigins chiamata "
			"a mano con pageWidth/pageHeight derivati dalla scala 'adatta'");
	}

	{
		// Margini che da soli superano l'intera area stampabile: nessuna
		// pagina puo' contenere dati reali -- elenco vuoto (stessa
		// garanzia di sicurezza di ComputePrintPageOrigins), non un
		// crash ne' un valore a caso.
		BRect content(0, 0, 50, 30);
		PrintJobLayout layout = ComputePrintJobLayout(content, 100, 100, 100, 100,
			2.54, 2.54, 2.54, 2.54, 0, 100.0, 10, 10);
		Check(layout.pageOrigins.empty(),
			"margini che consumano tutta l'area stampabile producono un elenco di pagine vuoto");
		Check(layout.pageWidth == 0 && layout.pageHeight == 0,
			"...e pageWidth/pageHeight restano a 0, non un valore negativo o indefinito");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
