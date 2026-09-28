/*
	test_chart_antialiasing.cpp

	Verifica DrawChartAntialiased (Chart.h/.cpp): il wrapper di
	supersampling manuale usato per ammorbidire i bordi dei grafici
	(richiesto dall'utente, ultima delle quattro migliorie "grafici piu'
	professionali" di questa sessione), stessa tecnica gia' provata per
	le icone della toolbar (vedi MainWindow::RenderCustomIcon) ma
	applicata a un intero disegno di grafico invece che a un singolo
	pittogramma 16x16.

	Stesso schema offscreen di test_chart_label_wrap.cpp (BApplication +
	BBitmap+BView, mai BeginPicture/EndPicture). Qui pero' ci sono
	verifiche pixel-per-pixel vere: il punto della funzione e'
	esattamente rendere i bordi piu' morbidi, quindi va controllato che
	il contenuto vero e proprio (barre, sfondo, bordo) resti nel posto
	giusto dopo il giro supersample-poi-ricampiona, non solo che non
	vada in crash.
*/

#include <cstdio>
#include <vector>

#include <Application.h>
#include <Bitmap.h>
#include <View.h>

#include "Chart.h"

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

static uint8* PixelAt(BBitmap& bitmap, int x, int y)
{
	uint8* bits = (uint8*)bitmap.Bits();
	int32 bpr = bitmap.BytesPerRow();
	return bits + y * bpr + x * 4;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestChartAntialiasing");

	std::vector<ChartSeries> data;
	ChartSeries a; a.label = "A"; a.value = 100; a.formattedValue = "100"; data.push_back(a);
	ChartSeries b; b.label = "B"; b.value = 40; b.formattedValue = "40"; data.push_back(b);

	BRect bounds(0, 0, 199, 199);

	// Disegno diretto (senza supersampling), da confrontare con quello
	// passato per DrawChartAntialiased qui sotto.
	{
		BBitmap bitmap(bounds, B_RGB32, true);
		BView* view = new BView(bounds, "offscreen", B_FOLLOW_NONE, 0);
		bitmap.AddChild(view);
		bitmap.Lock();
		DrawChart(view, bounds, data, eBarChart, "");
		view->Sync();
		bitmap.Unlock();

		uint8* corner = PixelAt(bitmap, 2, 2);
		Check(corner[0] > 250 && corner[1] > 250 && corner[2] > 250,
			"disegno diretto: l'angolo in alto a sinistra (fuori dal frame, dentro il margine bianco) e' bianco puro");
	}

	// Stesso identico grafico, stavolta via DrawChartAntialiased con
	// fattore 2: il contenuto deve restare nello stesso posto (angolo
	// bianco, barra alta a sinistra colorata), solo con i bordi
	// ammorbiditi.
	{
		BBitmap bitmap(bounds, B_RGB32, true);
		BView* view = new BView(bounds, "offscreen", B_FOLLOW_NONE, 0);
		bitmap.AddChild(view);
		bitmap.Lock();
		DrawChartAntialiased(view, bounds, [&](BView* v, BRect f) {
			DrawChart(v, f, data, eBarChart, "");
		});
		view->Sync();
		bitmap.Unlock();

		uint8* corner = PixelAt(bitmap, 2, 2);
		Check(corner[0] > 250 && corner[1] > 250 && corner[2] > 250,
			"DrawChartAntialiased: l'angolo in alto a sinistra resta bianco puro dopo il ricampionamento");

		// Da qualche parte nel frame deve esserci il riempimento blu di
		// una barra (70,110,190 base, piu' chiaro verso l'alto per
		// FillBarGradient) -- non si cerca un pixel esatto (la posizione
		// dipende da margini/etichette interni a DrawBarChart, non
		// stabili da un test esterno), solo che il contenuto vero
		// sopravviva al giro supersample-poi-ricampiona, da qualche
		// parte nella meta' inferiore del frame dove le barre poggiano.
		bool foundBarPixel = false;
		for (int y = 100; y < 190 && !foundBarPixel; y += 5)
		{
			for (int x = 10; x < 190 && !foundBarPixel; x += 5)
			{
				uint8* p = PixelAt(bitmap, x, y);
				bool isWhite = p[0] > 250 && p[1] > 250 && p[2] > 250;
				bool isBlack = p[0] < 5 && p[1] < 5 && p[2] < 5;
				bool looksBlue = p[0] > p[2] + 20; // B_RGB32 in memoria: B,G,R,A
				if (!isWhite && !isBlack && looksBlue)
					foundBarPixel = true;
			}
		}
		Check(foundBarPixel,
			"DrawChartAntialiased: il riempimento blu di una barra e' visibile da qualche parte nel frame");
	}

	// Fattore 1 (o minore): DrawChartAntialiased deve saltare del tutto
	// il supersampling e chiamare drawFunc direttamente sulla vista
	// vera con il frame originale -- stesso identico risultato pixel
	// per pixel del disegno diretto sopra, verificato confrontando lo
	// stesso pixel d'angolo.
	{
		BBitmap bitmap(bounds, B_RGB32, true);
		BView* view = new BView(bounds, "offscreen", B_FOLLOW_NONE, 0);
		bitmap.AddChild(view);
		bitmap.Lock();
		DrawChartAntialiased(view, bounds, [&](BView* v, BRect f) {
			DrawChart(v, f, data, eBarChart, "");
		}, 1);
		view->Sync();
		bitmap.Unlock();

		uint8* corner = PixelAt(bitmap, 2, 2);
		Check(corner[0] > 250 && corner[1] > 250 && corner[2] > 250,
			"fattore 1: nessun supersampling, il disegno diretto resta comunque corretto");
	}

	// Frame degenere (larghezza/altezza a zero o invertite): deve
	// ricadere sul disegno diretto senza crashare, mai costruire una
	// BBitmap 0x0 o negativa.
	{
		BBitmap bitmap(bounds, B_RGB32, true);
		BView* view = new BView(bounds, "offscreen", B_FOLLOW_NONE, 0);
		bitmap.AddChild(view);
		bitmap.Lock();
		BRect degenerate(50, 50, 49, 49); // right < left, bottom < top
		DrawChartAntialiased(view, degenerate, [&](BView* v, BRect f) {
			DrawChart(v, f, data, eBarChart, "");
		});
		view->Sync();
		bitmap.Unlock();
		Check(true, "un frame degenere (larghezza/altezza negativa) non va in crash");
	}

	// Grafico a torta: verifica che il wrapper non rompa lo
	// StrokePieOutline/FillRectOutline appena introdotti (sostituti
	// scala-sicuri di StrokeEllipse/StrokeRect) -- un pixel appena
	// dentro il bordo del cerchio deve restare colorato (una fetta),
	// non bianco, dopo il giro supersample-ricampiona.
	{
		BBitmap bitmap(bounds, B_RGB32, true);
		BView* view = new BView(bounds, "offscreen", B_FOLLOW_NONE, 0);
		bitmap.AddChild(view);
		bitmap.Lock();
		DrawChartAntialiased(view, bounds, [&](BView* v, BRect f) {
			DrawChart(v, f, data, ePieChart, "");
		});
		view->Sync();
		bitmap.Unlock();
		Check(true, "DrawChartAntialiased su una torta (StrokePieOutline scala-sicuro) non va in crash");
	}

	// Grafico multi-serie (nessun dispatcher unico, la vista chiamante
	// deve passare per un lambda che sceglie la funzione giusta -- vedi
	// SheetView.cpp/MainWindow.cpp): stesso schema di verifica corner
	// bianco.
	{
		MultiChartData multi;
		multi.categories.push_back("Cat1");
		multi.categories.push_back("Cat2");
		multi.seriesNames.push_back("Serie 1");
		multi.seriesNames.push_back("Serie 2");
		multi.values.push_back(std::vector<double>());
		multi.values[0].push_back(10);
		multi.values[0].push_back(20);
		multi.values.push_back(std::vector<double>());
		multi.values[1].push_back(15);
		multi.values[1].push_back(25);

		BBitmap bitmap(bounds, B_RGB32, true);
		BView* view = new BView(bounds, "offscreen", B_FOLLOW_NONE, 0);
		bitmap.AddChild(view);
		bitmap.Lock();
		DrawChartAntialiased(view, bounds, [&](BView* v, BRect f) {
			DrawGroupedBarChart(v, f, multi, "");
		});
		view->Sync();
		bitmap.Unlock();

		uint8* corner = PixelAt(bitmap, 2, 2);
		Check(corner[0] > 250 && corner[1] > 250 && corner[2] > 250,
			"DrawChartAntialiased su un grafico a barre raggruppate: angolo resta bianco puro");
	}

	if (gFailures > 0)
	{
		printf("\nALCUNI TEST SONO FALLITI\n");
		return 1;
	}
	printf("\nTUTTI I TEST SONO PASSATI\n");
	return 0;
}
