/*
	IconCatalog.h

	Icone HVIF vere (non piu' disegnate a codice, vedi ToolbarIcons.h/
	.cpp) per i pulsanti della toolbar, ora che il sito autorizzato
	(www.hvif-store.art) risulta finalmente popolato -- vedi
	Atomo123_icons/ATOMO123.md per la selezione ragionata e
	LICENSES.md per le licenze (tutte MIT). I byte grezzi vivono in
	IconData.cpp (generati dai file .hvif del catalogo, incorporati
	come array C invece che come file separati da distribuire a parte,
	stesso principio gia' scelto per l'icona dell'applicazione).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef ICON_CATALOG_H
#define ICON_CATALOG_H

#include <SupportDefs.h>

class BBitmap;

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
}

#endif
