/*
	PreferencesWindow.h

	Finestra "Preferenze" (Fase 7): mostra/nascondi la griglia,
	separatore decimale e di elenco usati per interpretare i numeri
	digitati nelle formule (CParser -- vedi CellParser.h). Un
	sottoinsieme volutamente ridotto di quanto esisteva in Sum-It
	storico: solo le preferenze che il motore gia' espone tramite un
	punto di estensione pronto (gDecimalPoint/gListSeparator, letti da
	TryToParseString quando non si passa un separatore esplicito) o
	una vista che il motore stesso disegna (SheetView::ShowGrid).
	Stessa regola sui thread di FindWindow: non tocca mai le
	preferenze applicate direttamente, inoltra solo una richiesta a
	MainWindow via BMessage.
*/

#ifndef PREFERENCES_WINDOW_H
#define PREFERENCES_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgPreferencesRequest = 'pref';

class BCheckBox;
class BMenuField;

class PreferencesWindow : public BWindow {
public:
	PreferencesWindow(BMessenger target);

	// Precompila i controlli con lo stato corrente (letto da
	// MainWindow prima di mostrare la finestra) -- altrimenti, alla
	// seconda apertura, mostrerebbe ancora i valori scelti la prima
	// volta invece di quelli davvero in vigore.
	void SetValues(bool showGrid, char decimalSep, char listSep);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BCheckBox* fShowGridBox;
	BMenuField* fDecimalField;
	BMenuField* fListField;
	BMessenger fTarget;
};

#endif
