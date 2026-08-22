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
