/*
	BorderWindow.h

	Finestra "Bordo cella" (Fase 13): raccoglie in un unico posto lati,
	spessore e colore del bordo -- come la scheda "Bordo" di Formato
	celle in Excel -- invece dei quattro comandi Bordo superiore/
	sinistro/inferiore/destro, del sottomenu Spessore bordo e di Colore
	bordo separati (rimasti nel menu Formato come scorciatoie rapide
	per un singolo lato, non sostituiti: la scelta rapida resta comoda
	per un cambio al volo, questa finestra e' per quando si vogliono
	scegliere lati/spessore/colore insieme in un solo colpo, con
	un'anteprima di come verra' la cella). Applica tutto in una sola
	chiamata a MainWindow (un solo passo di Annulla, non quattro).
	Stessa regola sui thread di ColorWindow/FindWindow: non tocca mai
	CellStyle direttamente, inoltra solo una richiesta a MainWindow via
	BMessage.
*/

#ifndef BORDER_WINDOW_H
#define BORDER_WINDOW_H

#include <GraphicsDefs.h>
#include <Messenger.h>
#include <Window.h>

const uint32 kMsgBorderFormatRequest = 'bfmt';

class BCheckBox;
class BColorControl;
class BMenuField;
class BorderPreviewView;

class BorderWindow : public BWindow {
public:
	BorderWindow(BMessenger target);

	// Precompila i controlli con lo stato del bordo della cella attiva
	// (letto da MainWindow prima di mostrare la finestra) -- stesso
	// principio di PreferencesWindow::SetValues/ColorWindow::SetMode.
	void SetInitial(bool top, bool left, bool bottom, bool right,
		int thickness, rgb_color color);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	void _UpdatePreview();
	int _SelectedThickness() const;

	BCheckBox* fTopBox;
	BCheckBox* fLeftBox;
	BCheckBox* fBottomBox;
	BCheckBox* fRightBox;
	BMenuField* fThicknessField;
	BColorControl* fColorControl;
	BorderPreviewView* fPreview;
	BMessenger fTarget;
};

#endif
