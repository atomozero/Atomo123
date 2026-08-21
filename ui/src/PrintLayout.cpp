/*
	PrintLayout.cpp

	Vedi PrintLayout.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PrintLayout.h"

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
	// questa pagina -- dataX parte da headerW perche' la colonna 1 del
	// foglio comincia gia' li' nel layout di SheetView (kHeaderWidth e'
	// una riserva fissa una tantum all'inizio del canvas, non per
	// pagina). Ogni pagina successiva avanza di (pageWidth-headerW), non
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
	for (float dataY = headerH; dataY < contentRect.bottom; dataY += (pageHeight - headerH))
	{
		for (float dataX = headerW; dataX < contentRect.right; dataX += (pageWidth - headerW))
			origins.push_back(BPoint(dataX - headerW, dataY - headerH));
	}

	return origins;
}
