/*
	ColorWindow.h

	Finestra "Colore testo"/"Colore sfondo"/"Colore bordo" (Fase 7,
	bordo aggiunto in Fase 13): un BColorControl piu' un pulsante
	Applica. Un'unica finestra riusata per tutte e tre le scelte
	(SetMode cambia titolo e colore iniziale) invece di classi quasi
	identiche. Stessa regola sui thread di FindWindow: non tocca mai
	CellStyle direttamente, inoltra solo una richiesta a MainWindow via
	BMessage.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef COLOR_WINDOW_H
#define COLOR_WINDOW_H

#include <GraphicsDefs.h>
#include <Messenger.h>
#include <Window.h>

const uint32 kMsgColorRequest = 'colr';

enum ColorTarget {
	eTextColor,
	eBackgroundColor,
	eBorderColor,
	eTabColor,
	// Colore di una serie di grafico (ChartWindow, Fase colori): a
	// differenza dei quattro bersagli sopra (sempre diretti a
	// MainWindow), questo e' pensato per una finestra ChartWindow che
	// possiede una PROPRIA istanza di ColorWindow puntata a se stessa
	// (vedi SetTarget sotto) -- il round trip non passa mai da
	// MainWindow per questo bersaglio.
	eSeriesColor,
	// Colore di un RUOLO della tavolozza tema (Tier 4, "Named cell
	// styles + live theme palette", NamedStyleWindow): stesso principio
	// esatto di eSeriesColor sopra -- NamedStyleWindow possiede la
	// PROPRIA istanza di ColorWindow (mai quella condivisa di
	// MainWindow), il round trip non passa mai da MainWindow per
	// questo bersaglio.
	eThemeColor
};

class BColorControl;

class ColorWindow : public BWindow {
public:
	ColorWindow(BMessenger target);

	// target sceglie testo/sfondo/bordo -- cambia titolo e messaggio
	// inviato da Applica, initial e' il colore da mostrare gia'
	// selezionato (letto dalla cella attiva da MainWindow prima di
	// mostrare la finestra).
	void SetMode(ColorTarget target, rgb_color initial);
	// Ripunta il round trip verso un bersaglio diverso da quello passato
	// al costruttore -- serve solo a ChartWindow, che possiede una
	// propria istanza (vedi il commento su eSeriesColor sopra) e la
	// punta sempre a se stessa; MainWindow non la chiama mai (la sua
	// unica istanza resta sempre puntata a BMessenger(this), come da
	// sempre). Va chiamata PRIMA di SetMode/Show per il nuovo utilizzo.
	void SetTarget(BMessenger target) { fTarget = target; }
	// Indice della serie per cui si sta scegliendo il colore -- viaggia
	// nel round trip (campo "seriesIndex") insieme al colore scelto,
	// innocuo/mai letto per i quattro bersagli storici sopra. Va
	// chiamata PRIMA di Show() per il nuovo utilizzo, come SetTarget.
	void SetSeriesIndex(int index) { fSeriesIndex = index; }
	// Ruolo della tavolozza tema (un ThemeColorRole di NamedStyle.h,
	// passato come plain int per non aggiungere qui una dipendenza
	// dall'header -- stesso principio di fSeriesIndex sopra) per cui si
	// sta scegliendo il colore -- viaggia nel round trip (campo
	// "themeRole"). Va chiamata PRIMA di Show(), come SetSeriesIndex.
	void SetThemeRole(int role) { fThemeRole = role; }

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BColorControl* fColorControl;
	BMessenger fTarget;
	ColorTarget fColorTarget;
	int fSeriesIndex;
	int fThemeRole;
};

#endif
