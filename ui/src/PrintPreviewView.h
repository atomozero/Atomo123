/*
	PrintPreviewView.h

	Vista dell'anteprima di stampa incorporata in "Imposta pagina"
	(Fase 28): disegna UNA pagina alla volta come un rettangolo
	"carta" bianco con un bordo/ombra sottile, su sfondo grigio
	chiaro -- stile Excel/LibreOffice, ma un puro visualizzatore, non
	genera nulla da sola. Le bitmap arrivano gia' pronte da
	MainWindow::GeneratePrintPreviewPages, che le rende con lo STESSO
	codice di disegno usato dalla stampa vera (MainWindow::PrintDocument),
	garanzia che l'anteprima corrisponda davvero a quello che verra'
	stampato.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PRINT_PREVIEW_VIEW_H
#define PRINT_PREVIEW_VIEW_H

#include <View.h>

#include <vector>

class BBitmap;

class PrintPreviewView : public BView {
public:
	PrintPreviewView();
	virtual ~PrintPreviewView();

	// Prende possesso delle bitmap passate (eliminate alla chiamata
	// successiva o nel distruttore) -- a differenza di
	// ChartView::SetData, qui serve vera ownership: le pagine sono
	// bitmap, troppo costose da ricopiare per ogni aggiornamento
	// dell'anteprima.
	void SetPages(std::vector<BBitmap*> pages);
	void SetPageIndex(int index);
	int PageIndex() const { return fPageIndex; }
	int PageCount() const { return (int)fPages.size(); }

	virtual void Draw(BRect updateRect);

private:
	std::vector<BBitmap*> fPages;
	int fPageIndex;

	void _DeletePages();
};

#endif
