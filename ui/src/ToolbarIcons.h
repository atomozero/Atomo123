/*
	ToolbarIcons.h

	Icone monocromatiche disegnate a codice per i pulsanti della
	toolbar (Nuovo/Apri/Salva/Stampa/Taglia/Copia/Incolla/Trova).

	Non derivano da HVIF (a differenza dell'icona dell'applicazione,
	vedi Atomo123.rdef/icons/atomo123.hvif): il sito autorizzato per le
	icone di questo progetto (www.hvif-store.art) risultava ancora
	vuoto ("0 icons found") al momento in cui sono state aggiunte
	queste, per la terza volta di seguito -- l'utente ha scelto di
	disegnarle direttamente, senza aspettare oltre ne' passare da
	Icon-O-Matic. Ogni funzione restituisce un BBitmap 16x16 B_RGBA32
	con sfondo trasparente, di proprieta' del chiamante (che deve
	cancellarlo).
*/

#ifndef TOOLBAR_ICONS_H
#define TOOLBAR_ICONS_H

class BBitmap;

namespace ToolbarIcons {
	BBitmap* New();
	BBitmap* Open();
	BBitmap* Save();
	BBitmap* Print();
	BBitmap* Cut();
	BBitmap* Copy();
	BBitmap* Paste();
	BBitmap* Find();
}

#endif
