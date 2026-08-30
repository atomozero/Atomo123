/*
	test_chart_label_wrap.cpp

	Verifica che le etichette lunghe (nomi di categoria, voci di
	legenda) NON sconfinino piu' fuori dal grafico -- bug segnalato
	dall'utente: BView::DrawString da solo non tronca ne' va a capo, un
	nome lungo ("United States of America" nella legenda della torta,
	o una categoria lunga sotto una barra stretta) veniva disegnato per
	intero, anche oltre il bordo del grafico.

	A differenza di test_chart.cpp (puro calcolo geometrico, nessun
	BView), qui serve una vera misura del testo (BView::StringWidth),
	quindi una vera BApplication e un vero BView -- ma agganciato a una
	BBitmap offscreen (mai BeginPicture/EndPicture: vedi il commento in
	memoria di progetto sul crash reale di app_server con quella
	tecnica, mai usata in questo progetto). Non ci sono asserzioni
	pixel-per-pixel (nessun precedente in questo file per le funzioni
	Draw*, solo le Compute* pure sono verificate a fondo altrove): qui
	si verifica che il disegno con etichette deliberatamente
	problematiche (nomi lunghi, un'unica parola lunghissima senza
	spazi, uno slot strettissimo) non vada mai in crash, sulle stesse
	identiche funzioni che l'app vera chiama per disegnare un grafico
	incorporato o nella finestra di anteprima.
*/

#include <cstdio>

#include <Application.h>
#include <Bitmap.h>
#include <View.h>

#include "Cell.h"
#include "Chart.h"
#include "Container.h"
#include "Range.h"
#include "Value.h"

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
	BApplication app("application/x-vnd.Atomo-TestChartLabelWrap");

	BRect bounds(0, 0, 300, 300);
	BBitmap bitmap(bounds, B_RGB32, true);
	BView* view = new BView(bounds, "offscreen", B_FOLLOW_NONE, 0);
	bitmap.AddChild(view);
	bool locked = bitmap.Lock();
	Check(locked, "la bitmap offscreen si blocca (serve per disegnare senza una vera finestra)");
	if (!locked)
	{
		printf("\nALCUNI TEST SONO FALLITI\n");
		return 1;
	}

	// Serie con nomi di categoria lunghi, alcuni piu' larghi di un
	// singolo carattere pur senza spazi -- esattamente il caso reale
	// (dataset Financial Sample) che ha fatto scoprire il bug
	// all'utente.
	std::vector<ChartSeries> longLabels;
	ChartSeries s1; s1.label = "United States of America"; s1.value = 100; longLabels.push_back(s1);
	ChartSeries s2; s2.label = "Channel Partners"; s2.value = 50; longLabels.push_back(s2);
	ChartSeries s3; s3.label = "UnaSolaParolaLunghissimaSenzaSpazi"; s3.value = -20; longLabels.push_back(s3);
	ChartSeries s4; s4.label = ""; s4.value = 10; longLabels.push_back(s4); // etichetta vuota, caso limite

	BRect wideFrame(0, 0, 280, 280);
	DrawChart(view, wideFrame, longLabels, eBarChart, "Un titolo di grafico estremamente lungo che non dovrebbe mai sconfinare fuori dal frame");
	Check(true, "DrawBarChart con etichette lunghe/vuote e un titolo lunghissimo non va in crash");

	DrawChart(view, wideFrame, longLabels, eLineChart, "");
	Check(true, "DrawLineChart con le stesse etichette non va in crash");

	DrawChart(view, wideFrame, longLabels, ePieChart, "");
	Check(true, "DrawPieChart (legenda con nomi lunghi + percentuale) non va in crash");

	DrawChart(view, wideFrame, longLabels, eAreaChart, "");
	Check(true, "DrawAreaChart (Fase 35) con le stesse etichette non va in crash");

	// A differenza dei controlli "non va in crash" sopra (nessun
	// precedente pixel-per-pixel in questo file per le funzioni Draw*),
	// qui si verifica per davvero che il riempimento sotto la linea
	// esista: un solo valore molto alto in un frame quadrato riempie
	// quasi tutta l'altezza del grafico, cosi' il centro esatto cade
	// quasi sicuramente dentro al poligono riempito (FillPolygon +
	// B_OP_ALPHA), mentre l'angolo in alto a sinistra del frame resta
	// sempre fuori da plotArea (margini/assi) e deve restare bianco
	// puro, mai toccato dal riempimento.
	{
		// Due punti (non uno solo): il riempimento richiede un
		// poligono di almeno due vertici della spezzata (vedi
		// DrawAreaChart), un singolo punto non forma un'area da
		// riempire, solo un pallino -- scoperto scrivendo proprio
		// questo test, che con un solo punto trovava sempre il centro
		// del frame bianco.
		std::vector<ChartSeries> twoTallPoints;
		ChartSeries tall1; tall1.label = "Uno"; tall1.value = 1000; twoTallPoints.push_back(tall1);
		ChartSeries tall2; tall2.label = "Due"; tall2.value = 1000; twoTallPoints.push_back(tall2);
		BRect areaFrame(0, 0, 199, 199);
		DrawChart(view, areaFrame, twoTallPoints, eAreaChart, "");
		view->Sync();

		uint8* bits = (uint8*)bitmap.Bits();
		int32 bpr = bitmap.BytesPerRow();
		// B_RGB32 in memoria: B, G, R, A. (5,5): dentro il margine di
		// 10px di DrawAreaChart ma fuori dal bordo nero disegnato
		// esattamente sul perimetro del frame (StrokeRect(frame), che
		// passerebbe proprio per (0,0) se campionato li').
		uint8* margin = bits + 5 * bpr + 5 * 4;
		Check(margin[0] > 250 && margin[1] > 250 && margin[2] > 250,
			"il margine del frame (fuori da plotArea, non sul bordo) resta bianco puro, "
			"il riempimento non sconfina");

		uint8* center = bits + 100 * bpr + 100 * 4; // circa al centro del frame
		bool centerTinted = !(center[0] > 250 && center[1] > 250 && center[2] > 250);
		Check(centerTinted,
			"il centro del frame, sotto due valori molto alti, e' colorato dal riempimento dell'area "
			"(non e' rimasto bianco come lo sfondo)");
	}

	// Frame MOLTO piccolo: lo slot per categoria diventa strettissimo
	// (pochi pixel), il caso peggiore per l'algoritmo di andata a capo
	// -- deve comunque troncare con l'ellissi, non crashare ne' entrare
	// in un ciclo infinito.
	BRect tinyFrame(0, 0, 40, 40);
	DrawChart(view, tinyFrame, longLabels, eBarChart, "");
	Check(true, "DrawBarChart in un frame minuscolo (slot strettissimo) non va in crash");

	// Grafico multi-serie: stessa legenda a striscia fissa (110px) di
	// DrawPieChart, stesso rischio di sconfinamento per un nome di
	// serie lungo.
	MultiChartData multi;
	multi.categories.push_back("Categoria con un nome piuttosto lungo");
	multi.categories.push_back("Breve");
	multi.seriesNames.push_back("Una serie con un nome lunghissimo che dovrebbe andare a capo nella legenda");
	multi.seriesNames.push_back("Serie 2");
	multi.values.push_back(std::vector<double>());
	multi.values[0].push_back(10);
	multi.values[0].push_back(-5);
	multi.values.push_back(std::vector<double>());
	multi.values[1].push_back(20);
	multi.values[1].push_back(15);

	DrawGroupedBarChart(view, wideFrame, multi, "");
	Check(true, "DrawGroupedBarChart con categorie/nomi di serie lunghi non va in crash");

	DrawMultiLineChart(view, wideFrame, multi, "");
	Check(true, "DrawMultiLineChart con le stesse categorie/serie non va in crash");

	DrawMultiAreaChart(view, wideFrame, multi, "");
	Check(true, "DrawMultiAreaChart (Fase 35) con le stesse categorie/serie non va in crash");

	bitmap.Unlock();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
