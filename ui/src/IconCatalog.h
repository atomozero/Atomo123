/*
	IconCatalog.h

	Icone HVIF vere (non piu' disegnate a codice, vedi ToolbarIcons.h/
	.cpp) per i pulsanti della toolbar, ora che il sito autorizzato
	(www.hvif-store.art) risulta finalmente popolato -- vedi
	docs/ICONS.md per la selezione ragionata e docs/ICON_LICENSES.md
	per le licenze (MIT o DSL a seconda dell'icona, mai copyleft). I
	byte grezzi vivono in IconData.cpp (generati dai file .hvif del
	catalogo scaricato, incorporati come array C invece che come file
	separati da distribuire a parte, stesso principio gia' scelto per
	l'icona dell'applicazione) -- il catalogo scaricato stesso (~1281
	icone, materiale di sola selezione) non e' piu' nel repository, solo
	le ~35 icone davvero usate qui e la documentazione in docs/.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef ICON_CATALOG_H
#define ICON_CATALOG_H

#include <SupportDefs.h>

class BBitmap;
class BView;

struct IconData {
	const uint8* bytes;
	size_t length;
};

extern const IconData kIconNew;
extern const IconData kIconOpen;
extern const IconData kIconSave;
extern const IconData kIconPrint;
extern const IconData kIconUndo;
extern const IconData kIconRedo;
extern const IconData kIconCut;
extern const IconData kIconCopy;
extern const IconData kIconPaste;
extern const IconData kIconDelete;
extern const IconData kIconFind;
extern const IconData kIconSortAscending;
extern const IconData kIconSortDescending;
extern const IconData kIconChart;
extern const IconData kIconTable;
extern const IconData kIconBold;
extern const IconData kIconItalic;
extern const IconData kIconUnderline;
extern const IconData kIconAlignLeft;
extern const IconData kIconAlignCenter;
extern const IconData kIconAlignRight;
extern const IconData kIconWrapText;
extern const IconData kIconTextColor;
extern const IconData kIconHighlight;
extern const IconData kIconHyperlink;
extern const IconData kIconComment;
extern const IconData kIconNamedRange;
extern const IconData kIconGoTo;
extern const IconData kIconBorderColor;
// Seconda ondata (stesso catalogo MIT, vedi IconData.cpp): pulsanti
// Celle/Regole/Numeri promossi da voci di menu. In attesa delle icone
// disegnate a mano in Icon-O-Matic per le lacune vere (Sostituisci,
// Bordi, Formato numero, fx, varianti grafico...), queste riusano le
// piu' vicine semanticamente fra quelle esistenti.
extern const IconData kIconInsertRow;
extern const IconData kIconInsertCol;
extern const IconData kIconMerge;
extern const IconData kIconFreeze;
extern const IconData kIconValidate;
extern const IconData kIconCondFormat;
extern const IconData kIconAutoSum;
extern const IconData kIconCurrency;

namespace IconCatalog {
	// Renderizza "icon" in un BBitmap 16x16 B_RGBA32 di proprieta' del
	// chiamante (stessa convenzione di ToolbarIcons: va cancellato
	// subito dopo BButton::SetIcon, che ne copia i bit al suo interno).
	// Ritorna NULL se il rendering vettoriale fallisce (dati HVIF
	// corrotti/incompatibili) -- il chiamante deve gestire questo caso
	// senza chiamare SetIcon su NULL.
	BBitmap* Render(const IconData& icon);

	// Disegna un pittogramma 16x16 a codice invece che da un vero file
	// HVIF -- per le lacune del catalogo (vedi docs/ICONS.md "icone da
	// disegnare", es. i tipi di grafico) dove nessuna icona del sito
	// autorizzato e' un candidato adatto. Disegna a una risoluzione 4x
	// piu' grande (BView::SetScale) e ricampiona con una media pesata
	// sull'alpha: un antialiasing fatto a mano, perche' questo app_server
	// disegna le forme dal vivo senza sfumare i bordi alla griglia dei
	// pixel (persino con B_SUBPIXEL_PRECISE). Stessa convenzione di
	// proprieta' di Render sopra: il chiamante cancella il BBitmap subito
	// dopo BButton::SetIcon/BMenuItem::SetIcon, che ne copiano i bit al
	// loro interno. "draw" e' un puntatore a funzione semplice (non
	// std::function) perche' i disegni sono sempre funzioni libere senza
	// stato catturato, stesso principio dei pittogrammi della toolbar
	// gia' esistenti prima di questa estrazione.
	BBitmap* RenderCustom(void (*draw)(BView*));
}

#endif
