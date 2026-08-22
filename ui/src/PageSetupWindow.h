/*
	PageSetupWindow.h

	Finestra "Imposta pagina" (Fase 27, vedi ROADMAP.md "v3.0
	Consolidation"): margini di stampa (in cm, convertiti in pixel al
	momento della stampa vera usando la risoluzione reale della
	stampante scelta, vedi MainWindow::PrintDocument) e scala del
	contenuto -- percentuale scelta a mano, oppure "adatta" a
	larghezza/altezza/entrambe di una sola pagina, calcolata di volta
	in volta sul contenuto REALE (o sull'area di stampa, se impostata)
	invece di una percentuale fissa salvata una tantum.

	Deliberatamente NIENTE controllo di orientamento/formato carta qui:
	quelli restano al dialogo di sistema che MainWindow::PrintDocument
	mostra gia' tramite BPrintJob::ConfigJob() prima di ogni stampa --
	aggiungere un secondo punto di configurazione per la stessa cosa
	sarebbe ridondante, e il dialogo di sistema dipende dal driver
	della stampante scelta (non verificabile qui senza una stampante
	vera comunque).

	Stessa regola di PreferencesWindow: non tocca mai gPrefs
	direttamente, inoltra solo una richiesta a MainWindow via BMessage.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PAGE_SETUP_WINDOW_H
#define PAGE_SETUP_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgPageSetupRequest = 'psrq';

class BRadioButton;
class BTextControl;

class PageSetupWindow : public BWindow {
public:
	PageSetupWindow(BMessenger target);

	// Precompila i controlli con lo stato corrente (letto da gPrefs da
	// MainWindow prima di mostrare la finestra) -- stesso motivo di
	// PreferencesWindow::SetValues.
	void SetValues(double marginTop, double marginBottom, double marginLeft,
		double marginRight, int scaleMode, double scalePercent);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BTextControl* fMarginTopField;
	BTextControl* fMarginBottomField;
	BTextControl* fMarginLeftField;
	BTextControl* fMarginRightField;
	BRadioButton* fScalePercentRadio;
	BRadioButton* fScaleFitWidthRadio;
	BRadioButton* fScaleFitHeightRadio;
	BRadioButton* fScaleFitBothRadio;
	BTextControl* fScalePercentField;
	BMessenger fTarget;
};

#endif
