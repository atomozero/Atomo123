/*
	ToolbarIcons.cpp

	Vedi ToolbarIcons.h.
*/

#include "ToolbarIcons.h"

#include <cstring>

#include <Bitmap.h>
#include <View.h>

namespace {

const int kIconSize = 16;

// Disegna in un BBitmap offscreen 16x16 B_RGBA32 (acceptsViews=true,
// necessario per potergli agganciare una BView e disegnarci sopra con
// le normali funzioni di BView): sfondo azzerato a mano (trasparente,
// non lo sfondo bianco della finestra) prima di disegnare, cosi' il
// bottone che la mostra non porta con se' un riquadro visibile
// intorno al tratto dell'icona.
template<typename Draw>
BBitmap* MakeIcon(Draw draw)
{
	BBitmap* bitmap = new BBitmap(BRect(0, 0, kIconSize - 1, kIconSize - 1), B_RGBA32, true);
	memset(bitmap->Bits(), 0, bitmap->BitsLength());

	BView* view = new BView(bitmap->Bounds(), "icon", B_FOLLOW_NONE, B_WILL_DRAW);
	bitmap->AddChild(view);
	bitmap->Lock();
	view->SetDrawingMode(B_OP_COPY);
	view->SetHighColor(60, 60, 60, 255);
	view->SetPenSize(1);
	draw(view);
	view->Sync();
	bitmap->Unlock();
	bitmap->RemoveChild(view);
	delete view;

	return bitmap;
}

} // namespace

BBitmap* ToolbarIcons::New()
{
	// Pagina con l'angolo in alto a destra piegato, piu' un "+" al
	// centro (nuovo documento vuoto).
	return MakeIcon([](BView* v) {
		v->StrokeLine(BPoint(4, 2), BPoint(9, 2));
		v->StrokeLine(BPoint(9, 2), BPoint(12, 5));
		v->StrokeLine(BPoint(12, 5), BPoint(12, 14));
		v->StrokeLine(BPoint(12, 14), BPoint(4, 14));
		v->StrokeLine(BPoint(4, 14), BPoint(4, 2));
		v->StrokeLine(BPoint(9, 2), BPoint(9, 5));
		v->StrokeLine(BPoint(9, 5), BPoint(12, 5));
		v->StrokeLine(BPoint(8, 8), BPoint(8, 12));
		v->StrokeLine(BPoint(6, 10), BPoint(10, 10));
	});
}

BBitmap* ToolbarIcons::Open()
{
	// Cartella: corpo piu' linguetta in alto a sinistra.
	return MakeIcon([](BView* v) {
		v->StrokeRect(BRect(2, 6, 13, 12));
		v->StrokeLine(BPoint(2, 6), BPoint(2, 4));
		v->StrokeLine(BPoint(2, 4), BPoint(6, 4));
		v->StrokeLine(BPoint(6, 4), BPoint(7, 6));
	});
}

BBitmap* ToolbarIcons::Save()
{
	// Dischetto: corpo, sportello metallico in alto (pieno), etichetta
	// in basso.
	return MakeIcon([](BView* v) {
		v->StrokeRect(BRect(2, 2, 13, 13));
		v->FillRect(BRect(5, 2, 10, 5));
		v->StrokeRect(BRect(4, 8, 11, 12));
	});
}

BBitmap* ToolbarIcons::Print()
{
	// Stampante: foglio sopra, corpo al centro, vassoio sotto.
	return MakeIcon([](BView* v) {
		v->StrokeRect(BRect(5, 1, 10, 5));
		v->StrokeRect(BRect(3, 5, 12, 9));
		v->StrokeRect(BRect(4, 9, 11, 13));
	});
}

BBitmap* ToolbarIcons::Cut()
{
	// Forbici: due anelli in basso, lame incrociate verso l'alto.
	return MakeIcon([](BView* v) {
		v->StrokeEllipse(BPoint(4, 11), 2, 2);
		v->StrokeEllipse(BPoint(4, 4), 2, 2);
		v->StrokeLine(BPoint(6, 9), BPoint(13, 3));
		v->StrokeLine(BPoint(6, 6), BPoint(13, 12));
	});
}

BBitmap* ToolbarIcons::Copy()
{
	// Due pagine sovrapposte, sfalsate.
	return MakeIcon([](BView* v) {
		v->StrokeRect(BRect(2, 5, 9, 12));
		v->StrokeRect(BRect(5, 2, 12, 9));
	});
}

BBitmap* ToolbarIcons::Paste()
{
	// Appunti: tavoletta, fermaglio in cima, righe di testo.
	return MakeIcon([](BView* v) {
		v->StrokeRect(BRect(3, 3, 12, 14));
		v->StrokeRect(BRect(6, 1, 9, 3));
		v->StrokeLine(BPoint(5, 6), BPoint(10, 6));
		v->StrokeLine(BPoint(5, 8), BPoint(10, 8));
		v->StrokeLine(BPoint(5, 10), BPoint(10, 10));
	});
}

BBitmap* ToolbarIcons::Find()
{
	// Lente d'ingrandimento: cerchio piu' manico.
	return MakeIcon([](BView* v) {
		v->StrokeEllipse(BPoint(6, 6), 4, 4);
		v->StrokeLine(BPoint(9, 9), BPoint(13, 13));
	});
}
