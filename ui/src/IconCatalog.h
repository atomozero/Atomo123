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
