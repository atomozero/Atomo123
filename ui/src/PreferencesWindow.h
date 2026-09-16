/*
	PreferencesWindow.h

	Finestra "Preferenze" (Fase 7, sezioni ampliate in Fase 13):
	mostra/nascondi la griglia, separatore decimale e di elenco usati
	per interpretare i numeri digitati nelle formule (CParser -- vedi
	CellParser.h), e numero di file recenti da ricordare (menu File >
	"Apri recenti", vedi MainWindow::fMaxRecentFiles). Un sottoinsieme
	volutamente ridotto di quanto esisteva in Sum-It storico: solo le
	preferenze che il motore/la UI gia' espongono tramite un punto di
	estensione pronto (gDecimalPoint/gListSeparator, letti da
	TryToParseString quando non si passa un separatore esplicito),
	una vista che il motore stesso disegna (SheetView::ShowGrid), o un
	comportamento gia' implementato ma finora con un limite fisso nel
	codice (i file recenti). Due sezioni (Generale/File) invece di un
	unico elenco di controlli, per restare leggibile ora che sono piu'
	di tre. Stessa regola sui thread di FindWindow: non tocca mai le
	preferenze applicate direttamente, inoltra solo una richiesta a
	MainWindow via BMessage.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PREFERENCES_WINDOW_H
#define PREFERENCES_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgPreferencesRequest = 'pref';

class BCheckBox;
class BMenuField;
class BTextControl;

class PreferencesWindow : public BWindow {
public:
	// Scelta lingua: 0 = sistema, 1 = italiano, 2 = English. Applicata
	// al prossimo avvio (vedi App::App), non a caldo: i cataloghi si
	// caricano all'apertura delle finestre.
	enum {
		kLangSystem = 0,
		kLangItalian = 1,
		kLangEnglish = 2
	};

	PreferencesWindow(BMessenger target);

	// Precompila i controlli con lo stato corrente (letto da
	// MainWindow prima di mostrare la finestra) -- altrimenti, alla
	// seconda apertura, mostrerebbe ancora i valori scelti la prima
	// volta invece di quelli davvero in vigore. Le 6 statistiche footer
	// sono in ordine fisso (media, conteggio, conteggio numerico,
	// minimo, massimo, somma): MainWindow le rimappa sui bit
	// kStatAverage/kStatCount/... (stesso ordine del menu contestuale
	// del footer), cosi' questa finestra non deve includere
	// MainWindow.h solo per i valori dei bit.
	void SetValues(bool showGrid, char decimalSep, char listSep, int maxRecentFiles,
		bool showSplash, char thousandSep, const char* currencySymbol,
		bool autoSaveEnabled, int autoSaveIntervalMinutes,
		bool showFormulasDefault, bool showAverage, bool showCount,
		bool showNumCount, bool showMin, bool showMax, bool showSum,
		const char* autoSaveDir, bool restoreSession, int language);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BCheckBox* fShowGridBox;
	BMenuField* fDecimalField;
	BMenuField* fListField;
	BMenuField* fRecentField;
	BCheckBox* fShowSplashBox;
	BMenuField* fThousandField;
	BTextControl* fCurrencyField;
	BCheckBox* fAutoSaveBox;
	BTextControl* fAutoSaveIntervalField;
	BCheckBox* fShowFormulasBox;
	BCheckBox* fStatAverageBox;
	BCheckBox* fStatCountBox;
	BCheckBox* fStatNumCountBox;
	BCheckBox* fStatMinBox;
	BCheckBox* fStatMaxBox;
	BCheckBox* fStatSumBox;
	BTextControl* fAutoSaveDirField;
	BCheckBox* fRestoreSessionBox;
	BMenuField* fLanguageField;
	BMessenger fTarget;
};

#endif
