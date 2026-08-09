/*
	PasteSpecialWindow.h

	Finestra "Incolla speciale" (Fase 7, sul modello di Sum-It storico
	PasteSpecialDialog, con un perimetro ridotto -- niente formato,
	dato che Atomo123 non lo copia ancora negli appunti, ne'
	"References"/link, un'opzione di nicchia): cosa incollare (Tutto o
	Solo valori), come combinarlo con la cella di destinazione
	(Sovrascrivi o un'operazione aritmetica), e se trasporre righe e
	colonne. Stessa regola sui thread di FindWindow/PivotWindow: non
	tocca mai il documento direttamente, inoltra solo una richiesta a
	MainWindow via BMessage.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PASTE_SPECIAL_WINDOW_H
#define PASTE_SPECIAL_WINDOW_H

#include <Messenger.h>
#include <Window.h>

const uint32 kMsgPasteSpecialRequest = 'pstr';

class BCheckBox;
class BMenuField;

class PasteSpecialWindow : public BWindow {
public:
	PasteSpecialWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	BMenuField* fContentField;
	BMenuField* fOperationField;
	BCheckBox* fTransposeBox;
	BMessenger fTarget;
};

#endif
