/*
	PrintLayout.cpp

	Vedi PrintLayout.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PrintLayout.h"

// Posizione canvas (assoluta) da cui comincia davvero il contenuto da
// stampare -- condivisa fra ComputePrintPageOrigins e
// ComputePrintFitScale sotto, MAI duplicata a mano: un'incongruenza
// fra le due produrrebbe pagine che non si allineano con la scala
// calcolata per "adattare" il contenuto. Vedi il commento completo su
// ComputePrintPageOrigins per il ragionamento.
static float PrintContentStartX(BRect contentRect, float headerW)
{
	return (contentRect.left > headerW) ? contentRect.left : headerW;
}

static float PrintContentStartY(BRect contentRect, float headerH)
{
	return (contentRect.top > headerH) ? contentRect.top : headerH;
}

std::vector<BPoint> ComputePrintPageOrigins(BRect contentRect,
	float pageWidth, float pageHeight, float headerW, float headerH)
{
	std::vector<BPoint> origins;

	// Una pagina piu' stretta/bassa della sola banda di intestazione
	// non puo' contenere nessun dato reale (il passo del ciclo sotto
	// sarebbe <= 0, loop infinito) -- il chiamante deve gia' averlo
	// escluso, ma un elenco vuoto e' comunque una risposta sicura.
	if (pageWidth <= headerW || pageHeight <= headerH)
		return origins;

	// dataX/dataY sono la posizione canvas (assoluta, stesso spazio di
	// SheetView::ContentRect) del primo pixel di dati VERI mostrato su
	// questa pagina -- partono da contentRect.left/top, MAI meno di
	// headerW/headerH: per il caso comune (contentRect da 0,0, tutto il
	// foglio) coincide con l'inizio del canvas, dove la colonna 1
	// comincia gia' di suo (kHeaderWidth e' una riserva fissa una
	// tantum all'inizio del canvas, non per pagina). Un'AREA DI STAMPA
	// che non comincia dalla riga/colonna 1 (Fase 27, vedi
	// MainWindow::SetPrintArea) parte invece da contentRect.left/top
	// vero -- le etichette di riga/colonna di SheetView::Draw usano
	// gia' la posizione REALE di ogni riga/colonna (mai relativa a
	// "prima riga/colonna della pagina"), quindi ripetere le
	// intestazioni funziona identico anche per un'area che comincia,
	// ad esempio, dalla colonna D o dalla riga 5.
	//
	// Ogni pagina successiva avanza di (pageWidth-headerW), non
	// dell'intera pagina: la sua STESSA intestazione ripetuta occupa i
	// primi headerW pixel di quella pagina, quindi la dose di dati
	// NUOVI che ci sta e' ridotta della stessa banda.
	//
	// L'origine della pagina (quella restituita, e quella passata a
	// SheetView::ScrollTo/BPrintJob::DrawView) e' dataX-headerW, non
	// dataX: disegnare l'intera pagina a partire da li' ridisegna
	// anche l'ultima striscia headerW-larga della pagina PRECEDENTE
	// (SheetView::Draw non sa "saltare" quella banda), ma quella
	// striscia duplicata finisce comunque coperta dall'intestazione
	// ridisegnata sopra di lei (SheetView::Draw la disegna DOPO i dati,
	// vedi il commento gemello in MainWindow::PrintDocument) -- nessun
	// dato visibile ne' perso ne' duplicato nella stampa finale.
	float startX = PrintContentStartX(contentRect, headerW);
	float startY = PrintContentStartY(contentRect, headerH);

	for (float dataY = startY; dataY < contentRect.bottom; dataY += (pageHeight - headerH))
	{
		for (float dataX = startX; dataX < contentRect.right; dataX += (pageWidth - headerW))
			origins.push_back(BPoint(dataX - headerW, dataY - headerH));
	}

	return origins;
}

BRect PrintPageContentExtent(BPoint pageOrigin, float pageW, float pageH,
	BRect contentRect, float headerW, float headerH)
{
	BRect dataArea(pageOrigin.x + headerW, pageOrigin.y + headerH,
		pageOrigin.x + pageW, pageOrigin.y + pageH);
	if (!dataArea.IsValid())
		return dataArea;
	return dataArea & contentRect;
}

float ComputePrintFitScale(BRect contentRect, float usableWidth, float usableHeight,
	float headerW, float headerH, int fitMode)
{
	// Larghezza/altezza VERE (intestazione compresa) che il contenuto
	// occupa a partire da dove comincia davvero (PrintContentStartX/Y
	// sopra, la STESSA posizione usata da ComputePrintPageOrigins per
	// la prima pagina) -- non contentRect.right/bottom da soli, che per
	// un'area di stampa che non parte dalla riga/colonna 1 (Fase 27)
	// includerebbero anche lo spazio PRIMA dell'area, mai stampato.
	float totalWidth = contentRect.right - PrintContentStartX(contentRect, headerW) + headerW;
	float totalHeight = contentRect.bottom - PrintContentStartY(contentRect, headerH) + headerH;

	if (totalWidth <= 0 || totalHeight <= 0 || usableWidth <= 0 || usableHeight <= 0)
		return 1.0f;

	float fitWidthScale = usableWidth / totalWidth;
	float fitHeightScale = usableHeight / totalHeight;

	float scale;
	if (fitMode == kPrintFitWidth)
		scale = fitWidthScale;
	else if (fitMode == kPrintFitHeight)
		scale = fitHeightScale;
	else
		scale = (fitWidthScale < fitHeightScale) ? fitWidthScale : fitHeightScale;

	// "Adatta" restringe soltanto: un contenuto che gia' ci sta resta
	// alla sua dimensione naturale (100%), non viene ingrandito, come
	// in Excel.
	return (scale > 1.0f) ? 1.0f : scale;
}

float ComputePrintFitScaleToPages(BRect contentRect, float usableWidth, float usableHeight,
	float headerW, float headerH, int wide, int tall)
{
	if (wide < 1) wide = 1;
	if (tall < 1) tall = 1;

	// Stessa base di ComputePrintFitScale sopra (mai duplicata a mano
	// qui dentro oltre le due righe di misura): il contenuto occupa
	// totalWidth x totalHeight a partire da dove comincia davvero.
	float totalWidth = contentRect.right - PrintContentStartX(contentRect, headerW) + headerW;
	float totalHeight = contentRect.bottom - PrintContentStartY(contentRect, headerH) + headerH;

	if (totalWidth <= 0 || totalHeight <= 0 || usableWidth <= 0 || usableHeight <= 0)
		return 1.0f;

	// wide pagine di larghezza usableWidth ciascuna (e tall di altezza):
	// la scala che ci sta e' il minimo fra le due dimensioni, mai oltre
	// 1.0 come sopra.
	float scale = usableWidth * wide / totalWidth;
	float heightScale = usableHeight * tall / totalHeight;
	if (heightScale < scale)
		scale = heightScale;
	return (scale > 1.0f) ? 1.0f : scale;
}

PrintJobLayout ComputePrintJobLayout(BRect contentRect,
	float printableWidth, float printableHeight, int32 xDPI, int32 yDPI,
 double marginTopCm, double marginBottomCm, double marginLeftCm, double marginRightCm,
	int scaleMode, double scalePercent, float headerW, float headerH,
	int fitWide, int fitTall, bool centerH, bool centerV)
{
	PrintJobLayout layout;

	// 1 pollice = 2.54cm -- xDPI/yDPI sono la risoluzione VERA del
	// dispositivo (BPrintJob::GetResolution), non un valore fisso a
	// 72dpi: printableWidth/Height sono gia' espresse in pixel del
	// dispositivo (BPrintJob::PrintableRect()).
	layout.marginTopPx = (float)(marginTopCm / 2.54 * yDPI);
	layout.marginLeftPx = (float)(marginLeftCm / 2.54 * xDPI);
	float marginBottomPx = (float)(marginBottomCm / 2.54 * yDPI);
	float marginRightPx = (float)(marginRightCm / 2.54 * xDPI);

	float usableWidth = printableWidth - layout.marginLeftPx - marginRightPx;
	float usableHeight = printableHeight - layout.marginTopPx - marginBottomPx;

	if (usableWidth <= headerW || usableHeight <= headerH)
	{
		layout.pageWidth = layout.pageHeight = 0;
		layout.scale = 1.0;
		return layout;
	}

	// Scala: o una percentuale fissa scelta dall'utente (scaleMode 0),
	// o calcolata per adattare il contenuto alla larghezza/altezza/
	// entrambe di una sola pagina (ComputePrintFitScale sopra) o a
	// fitWide x fitTall pagine (ComputePrintFitScaleToPages) -- i valori
	// di scaleMode 1/2/3/4 coincidono apposta con
	// kPrintFitWidth/kPrintFitHeight/kPrintFitBoth/kPrintFitPages.
	if (scaleMode == kPrintFitWidth || scaleMode == kPrintFitHeight || scaleMode == kPrintFitBoth)
		layout.scale = ComputePrintFitScale(contentRect, usableWidth, usableHeight,
			headerW, headerH, scaleMode);
	else if (scaleMode == kPrintFitPages)
		layout.scale = ComputePrintFitScaleToPages(contentRect, usableWidth, usableHeight,
			headerW, headerH, fitWide, fitTall);
	else
		layout.scale = scalePercent / 100.0;

	if (layout.scale <= 0.0)
		layout.scale = 1.0;

	// pageWidth/pageHeight sono la porzione di CANVAS (coordinate
	// logiche, non scalate, di SheetView) che sta in una pagina fisica
	// -- non usableWidth/usableHeight direttamente: BView::SetScale
	// ingrandisce/rimpicciolisce ogni operazione di disegno della vista
	// in modo trasparente al codice di disegno interno. Per riempire la
	// STESSA area fisica usableWidth x usableHeight con una scala < 1
	// serve percio' PIU' canvas logico, cioe' usableWidth/scale: la
	// larghezza fisica totale per pagina resta invariata a usableWidth
	// qualunque sia la scala, per costruzione (pageWidth*scale ==
	// usableWidth).
	layout.pageWidth = (float)(usableWidth / layout.scale);
	layout.pageHeight = (float)(usableHeight / layout.scale);

	layout.pageOrigins = ComputePrintPageOrigins(contentRect, layout.pageWidth, layout.pageHeight,
		headerW, headerH);

	// Centratura per pagina: un offset per pageOrigins (stesso indice),
	// in pixel del dispositivo come marginLeftPx/marginTopPx -- il
	// chiamante lo somma alla destinazione. Solo le pagine parziali si
	// muovono davvero: a pagina piena l'estensione coincide con l'area
	// dati e lo spazio residuo e' nullo.
	layout.pageOffsets.assign(layout.pageOrigins.size(), BPoint(0, 0));
	if (centerH || centerV)
	{
		for (size_t i = 0; i < layout.pageOrigins.size(); i++)
		{
			BRect extent = PrintPageContentExtent(layout.pageOrigins[i],
				layout.pageWidth, layout.pageHeight, contentRect, headerW, headerH);
			float offX = 0, offY = 0;
			if (extent.IsValid())
			{
				// Il blocco centrato e' bande di intestazione + dati
				// (headerW/H sono gia' 0 senza intestazioni): centrare i
				// soli dati sposterebbe anche le intestazioni fuori asse.
				if (centerH)
				{
					float slack = usableWidth
						- (headerW + extent.Width()) * (float)layout.scale;
					if (slack > 0)
						offX = slack / 2;
				}
				if (centerV)
				{
					float slack = usableHeight
						- (headerH + extent.Height()) * (float)layout.scale;
					if (slack > 0)
						offY = slack / 2;
				}
			}
			layout.pageOffsets[i] = BPoint(offX, offY);
		}
	}

	return layout;
}
