/*
	test_image_alpha.cpp

	Bug reale segnalato dall'utente: "se un'immagine ha lo sfondo
	trasparente, riusciamo a garantire anche su Atomo123 la
	trasparenza dell'immagine?" -- la risposta era no. Le immagini
	incorporate (Fase 12, vedi EmbeddedImage.h) vengono decodificate
	correttamente con canale alpha (il PNGTranslator produce un
	BBitmap B_RGBA32 se il sorgente ne ha uno), e l'alpha sopravvive
	al salvataggio nel formato nativo .ascd (blob grezzo, vedi
	test_persistence.cpp) -- ma SheetView::Draw() chiamava DrawBitmap
	sulle immagini incorporate senza impostare una modalita' di
	disegno alpha-aware: restava attiva B_OP_COPY (l'impostazione
	predefinita di BView), che copia i byte RGB del sorgente cosi'
	come sono ignorando il canale alpha -- uno sfondo "trasparente"
	appariva quindi con il colore pieno del pixel sorgente invece
	dello sfondo della cella sottostante.

	Non basta un PNG finto come in test_persistence.cpp (li' la
	persistenza trasporta i byte cosi' come sono, senza mai
	decodificarli): qui serve un vero PNG con canale alpha, capace di
	essere davvero decodificato dal Translation Kit, per esercitare
	il percorso di codice reale (DecodeImageBytes + DrawBitmap) che
	ha causato il bug -- codificato al volo da una BBitmap B_RGBA32
	tramite lo stesso BTranslatorRoster che l'app usa per decodificare
	(BBitmapStream come sorgente, formato di uscita B_PNG_FORMAT).

	Per verificare il risultato visivo serve leggere i pixel
	realmente disegnati, non solo lo stato interno (un controllo tipo
	"DrawingMode() dopo Draw()" non avrebbe scoperto il bug: il
	cambio di modalita' e il suo ripristino avvengono entrambi dentro
	la stessa chiamata a Draw(), invisibili dall'esterno a
	chiamata conclusa). SheetView viene percio' disegnata su una vera
	BBitmap offscreen (creata con "accetta viste", B_BITMAP_ACCEPTS_
	VIEWS tramite il parametro acceptsViews del costruttore) invece
	che su una finestra visibile in schermo: la vista aggiunta con
	AddChild diventa figlia di una finestra offscreen privata gestita
	da app_server, con Draw() che scrive davvero nella memoria della
	bitmap (leggibile poi con Bits()) -- stessa tecnica standard di
	Haiku per rendering headless, mai usata finora nei test di questo
	progetto (gia' verificata a parte con un piccolo programma di
	prova prima di scrivere questo test, compreso il fatto che senza
	il fix la meta' "trasparente" dell'immagine risultava rosso
	pieno invece di mostrare lo sfondo bianco della cella, esattamente
	il bug segnalato).
*/

#include <cstdio>
#include <vector>

#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <TranslationDefs.h>
#include <TranslatorRoster.h>
#include <View.h>

#include "Cell.h"
#include "Container.h"
#include "EmbeddedImage.h"
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

// Codifica una BBitmap B_RGBA32 in un vero PNG con canale alpha,
// usando lo stesso Translation Kit che SheetView usa per decodificarlo.
static bool EncodePngWithAlpha(BBitmap* source, std::vector<uint8>& out)
{
	BBitmapStream stream(source); // adotta 'source', lo cancella lei
	BMallocIO dest;
	status_t err = BTranslatorRoster::Default()->Translate(&stream, NULL, NULL,
		&dest, B_PNG_FORMAT);
	if (err != B_OK)
		return false;
	out.assign((const uint8*)dest.Buffer(), (const uint8*)dest.Buffer() + dest.BufferLength());
	return true;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestImageAlpha");

	CContainer* doc = new CContainer(NULL, NULL);

	// Immagine 8x8: meta' sinistra rosso pieno (alpha 255), meta'
	// destra completamente trasparente (alpha 0, ma con lo stesso
	// rosso nei canali RGB -- cosi' se il bug fosse ancora presente
	// (canale alpha ignorato) il risultato sarebbe comunque rosso
	// pieno, non un colore neutro che nasconderebbe la differenza).
	const int32 kSize = 8;
	BBitmap* srcBitmap = new BBitmap(BRect(0, 0, kSize - 1, kSize - 1), B_RGBA32);
	uint8* srcBits = (uint8*)srcBitmap->Bits();
	int32 srcBpr = srcBitmap->BytesPerRow();
	for (int32 y = 0; y < kSize; y++)
	{
		for (int32 x = 0; x < kSize; x++)
		{
			uint8* px = srcBits + y * srcBpr + x * 4;
			bool leftHalf = x < kSize / 2;
			px[0] = 0;						// B
			px[1] = 0;						// G
			px[2] = 255;					// R
			px[3] = leftHalf ? 255 : 0;	// A
		}
	}

	std::vector<uint8> pngData;
	Check(EncodePngWithAlpha(srcBitmap, pngData),
		"un vero PNG con canale alpha viene codificato dal Translation Kit");
	Check(pngData.size() > 8
			&& pngData[0] == 0x89 && pngData[1] == 'P' && pngData[2] == 'N' && pngData[3] == 'G',
		"i byte codificati iniziano con la firma PNG reale (non un blob finto)");

	std::vector<EmbeddedImage> images;
	EmbeddedImage img;
	img.anchor = cell(2, 3); // B3, mai selezionata
	img.offsetX = 0;
	img.offsetY = 0;
	img.width = kSize * 4;   // ingrandita: ogni meta' logica dell'immagine
	img.height = kSize * 4;  // copre piu' pixel schermo, piu' facile da
	                          // campionare senza cadere su un bordo
	img.pngData = pngData;
	images.push_back(img);

	BRect canvasRect(0, 0, 799, 599);
	BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true); // accetta viste (rendering offscreen)
	SheetView* view = new SheetView(doc);
	view->ResizeTo(canvasRect.Width(), canvasRect.Height());
	view->SetImages(&images);
	canvas->AddChild(view);

	bool locked = canvas->Lock();
	Check(locked, "la bitmap offscreen si blocca per disegnarci sopra");
	view->Draw(canvasRect);
	view->Sync();
	canvas->Unlock();

	BRect cellRect = view->CellRect(cell(2, 3));
	BPoint opaquePt(cellRect.left + 4, cellRect.top + kSize * 2);
	BPoint transparentPt(cellRect.left + kSize * 3 + 4, cellRect.top + kSize * 2);

	uint8* cbits = (uint8*)canvas->Bits();
	int32 cbpr = canvas->BytesPerRow();
	uint8* opq = cbits + (int32)opaquePt.y * cbpr + (int32)opaquePt.x * 4;
	uint8* trn = cbits + (int32)transparentPt.y * cbpr + (int32)transparentPt.x * 4;

	// B_RGB32/B_RGBA32 in memoria: B, G, R, A (ordine verificato con
	// il programma di prova preliminare).
	Check(opq[0] == 0 && opq[1] == 0 && opq[2] == 255,
		"la meta' opaca dell'immagine (alpha 255) e' rossa piena, come il sorgente");
	Check(trn[0] == 255 && trn[1] == 255 && trn[2] == 255,
		"la meta' trasparente dell'immagine (alpha 0) mostra lo sfondo bianco della cella, non rosso pieno");

	delete canvas;

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
