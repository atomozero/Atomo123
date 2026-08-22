/*
	PrintPreviewView.cpp

	Vedi PrintPreviewView.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PrintPreviewView.h"

#include <algorithm>

#include <Bitmap.h>

PrintPreviewView::PrintPreviewView()
	:
	BView(BRect(0, 0, 320, 400), "PrintPreviewView", B_FOLLOW_ALL, B_WILL_DRAW),
	fPageIndex(0)
{
	// Grigio chiaro "fuori pagina", stesso principio visivo di Excel/
	// LibreOffice: la pagina bianca vera risalta sopra, vedi Draw().
	SetViewColor(200, 200, 200);
	// Senza una dimensione minima esplicita, BLayoutBuilder puo'
	// schiacciare l'anteprima a quasi nulla se la finestra che la
	// contiene e' troppo piccola per il resto dei controlli (stesso
	// motivo gia' documentato in ChartView).
	SetExplicitMinSize(BSize(320, 400));
}

PrintPreviewView::~PrintPreviewView()
{
	_DeletePages();
}

void PrintPreviewView::_DeletePages()
{
	for (size_t i = 0; i < fPages.size(); i++)
		delete fPages[i];
	fPages.clear();
}

void PrintPreviewView::SetPages(std::vector<BBitmap*> pages)
{
	_DeletePages();
	fPages = pages;
	fPageIndex = 0;
	Invalidate();
}

void PrintPreviewView::SetPageIndex(int index)
{
	if (index < 0 || index >= (int)fPages.size() || index == fPageIndex)
		return;
	fPageIndex = index;
	Invalidate();
}

void PrintPreviewView::Draw(BRect updateRect)
{
	BRect bounds = Bounds();
	SetHighColor(200, 200, 200);
	FillRect(bounds);

	if (fPageIndex < 0 || fPageIndex >= (int)fPages.size() || fPages[fPageIndex] == NULL)
	{
		// Nessuna pagina (ancora) generata: solo lo sfondo, senza un
		// messaggio d'errore -- MainWindow chiama SetPages() appena la
		// finestra si apre, questo stato e' troppo breve per
		// giustificare un placeholder testuale a se'.
		return;
	}

	BBitmap* page = fPages[fPageIndex];
	BRect pageBounds = page->Bounds();
	if (pageBounds.Width() <= 0 || pageBounds.Height() <= 0)
		return;

	// La bitmap mantiene gia' le proporzioni reali della carta (vedi
	// MainWindow::GeneratePrintPreviewPages): un fattore di scala
	// UNICO per entrambi gli assi, mai deformata.
	const float kMargin = 16.0f;
	float availW = bounds.Width() - kMargin * 2;
	float availH = bounds.Height() - kMargin * 2;
	if (availW <= 0 || availH <= 0)
		return;

	float scale = std::min(availW / pageBounds.Width(), availH / pageBounds.Height());
	float destW = pageBounds.Width() * scale;
	float destH = pageBounds.Height() * scale;

	BRect dest(0, 0, destW, destH);
	dest.OffsetTo(bounds.left + (bounds.Width() - destW) / 2.0f,
		bounds.top + (bounds.Height() - destH) / 2.0f);

	// Ombra sottile dietro la pagina + bordo -- l'aspetto "professionale"
	// richiesto, in stile Haiku (piatto, poco decorato) invece di
	// un'ombra sfumata vera.
	SetHighColor(150, 150, 150);
	FillRect(dest.OffsetByCopy(3, 3));

	SetHighColor(255, 255, 255);
	FillRect(dest);
	DrawBitmap(page, pageBounds, dest);

	SetHighColor(120, 120, 120);
	StrokeRect(dest);
}
