/*
	NameWindow.h

	Finestra "Intervalli con nome": elenco dei nomi gia' definiti
	(BListView), due campi (nome, intervallo "A1" o "A1:B5") e tre
	pulsanti (Aggiungi/Aggiorna, Elimina, Vai a). Stessa regola sui
	thread di FindWindow/PivotWindow: non tocca mai CNameTable
	direttamente (appartiene al CContainer del foglio attivo, su
	MainWindow), inoltra solo richieste via BMessage. MainWindow
	richiama SetNames() prima di Show() e dopo ogni Aggiungi/
	Aggiorna/Elimina per tenere l'elenco allineato a CNameTable.
*/

#ifndef NAME_WINDOW_H
#define NAME_WINDOW_H

#include <Messenger.h>
#include <String.h>
#include <Window.h>
#include <vector>

const uint32 kMsgDefineName = 'dfnm';
const uint32 kMsgDeleteName = 'dlnm';
const uint32 kMsgGoToName = 'gtnm';

class BListView;
class BTextControl;

class NameWindow : public BWindow {
public:
	NameWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

	// names/ranges devono avere la stessa lunghezza (ranges[i] e'
	// gia' formattato come testo, es. "A1:B5" -- vedi
	// range::GetRCName, la stessa sintassi accettata da
	// RangeRef::ParseRangeRef per il campo Intervallo).
	void SetNames(const std::vector<BString>& names, const std::vector<BString>& ranges);

private:
	BListView* fNameList;
	BTextControl* fNameField;
	BTextControl* fRangeField;
	BMessenger fTarget;
	std::vector<BString> fRanges;

	void SendNameMessage(uint32 what);
};

#endif
