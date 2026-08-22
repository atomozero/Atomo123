/*
	PageSetupWindow.h

	Finestra "Imposta pagina" (Fase 27, vedi ROADMAP.md "v3.0
	Consolidation"; anteprima aggiunta in Fase 28): margini di stampa
	(in cm, convertiti in pixel al momento della stampa vera usando la
	risoluzione reale della stampante scelta, vedi
	MainWindow::PrintDocument) e scala del contenuto -- percentuale
	scelta a mano, oppure "adatta" a larghezza/altezza/entrambe di una
	sola pagina, calcolata di volta in volta sul contenuto REALE (o
	sull'area di stampa, se impostata) invece di una percentuale fissa
	salvata una tantum. A sinistra un'anteprima vera della pagina
	stampata (PrintPreviewView), che si aggiorna ogni volta che un
	campo viene confermato -- stesso disegno usato dalla stampa vera
	(MainWindow::GeneratePrintPreviewPages), non una ricostruzione
	separata: quello che si vede qui e' quello che verra' stampato.

	Deliberatamente NIENTE controllo di orientamento/formato carta qui:
	quelli restano al dialogo di sistema che MainWindow::PrintDocument
	mostra gia' tramite BPrintJob::ConfigJob() prima di ogni stampa --
	aggiungere un secondo punto di configurazione per la stessa cosa
	sarebbe ridondante, e il dialogo di sistema dipende dal driver
	della stampante scelta (non verificabile qui senza una stampante
	vera comunque). Allo stesso modo, "Stampa..." qui sotto apre
	SEMPRE quel dialogo di sistema (dove si puo' scegliere qualunque
	stampante configurata, PDF compresa se presente) -- niente scorciatoia
	per "esportare PDF" che lo aggiri: Haiku non offre un modo pubblico
	per avviare una stampa senza mostrarlo (vedi il commento su
	MainWindow::PrintDocument).

	Stessa regola di PreferencesWindow: non tocca mai gPrefs
	direttamente, inoltra solo richieste a MainWindow via BMessage.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PAGE_SETUP_WINDOW_H
#define PAGE_SETUP_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <vector>

const uint32 kMsgPageSetupRequest = 'psrq';
// Anteprima (Fase 28): stessi sei campi di kMsgPageSetupRequest, ma i
// valori ANCORA IN MODIFICA nel dialogo (mai persistiti in gPrefs) --
// vedi MainWindow::HandlePageSetupPreviewRequest.
const uint32 kMsgPageSetupPreviewRequest = 'apsv';
// "Stampa..." dentro "Imposta pagina": applica i sei campi (come
// kMsgPageSetupRequest) POI stampa davvero, un solo clic invece di
// Applica + Archivio > Stampa separati.
const uint32 kMsgPageSetupPrintRequest = 'apsp';

class BBitmap;
class BButton;
class BRadioButton;
class BStringView;
class BTextControl;
class PrintPreviewView;

class PageSetupWindow : public BWindow {
public:
	PageSetupWindow(BMessenger target);

	// Precompila i controlli con lo stato corrente (letto da gPrefs da
	// MainWindow prima di mostrare la finestra) -- stesso motivo di
	// PreferencesWindow::SetValues.
	void SetValues(double marginTop, double marginBottom, double marginLeft,
		double marginRight, int scaleMode, double scalePercent);

	// Prende possesso delle bitmap passate (vedi PrintPreviewView::
	// SetPages) e torna a mostrare la prima pagina -- chiamata da
	// MainWindow ogni volta che l'anteprima va rigenerata: all'apertura
	// della finestra, e dopo ogni campo confermato (vedi
	// kMsgFieldChanged/kMsgScaleModeChanged sotto).
	void SetPreviewPages(std::vector<BBitmap*> pages);

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
	PrintPreviewView* fPreviewView;
	BStringView* fPageLabel;
	BButton* fPrevPageButton;
	BButton* fNextPageButton;
	BMessenger fTarget;

	// Legge e convalida i sei campi (stessi limiti gia' applicati da
	// kMsgApplyLocal: margini mai negativi, percentuale in [10,400]) in
	// un unico posto -- usata da "Applica"/"Stampa..." (persistono, "what"
	// = kMsgPageSetupRequest/kMsgPageSetupPrintRequest) e da ogni
	// cambio di campo (SOLO anteprima, "what" = kMsgPageSetupPreviewRequest,
	// mai scritto in gPrefs).
	BMessage _BuildSettingsMessage(uint32 what) const;
	void _UpdatePageNavControls();
};

#endif
