/*
	ToolbarView.h

	Contenitore per i pulsanti della toolbar (BuildToolbar in
	MainWindow.cpp) con gestione del troppopieno quando la finestra si
	restringe: i pulsanti che non entrano piu' nella larghezza
	disponibile vengono nascosti e sostituiti da un unico pulsante
	">>" che apre un BPopUpMenu con le stesse voci (etichetta e
	comando) dei pulsanti nascosti -- stesso spirito di SheetTabView
	(scorrimento con due frecce quando le schede non entrano tutte),
	ma qui i pulsanti restano veri BButton (icona, tooltip, stato
	premuto gestiti dalla classe stessa) invece di una vista disegnata
	interamente a mano: bastano posizione/visibilita' gestite da
	questo contenitore, il resto lo fa gia' BButton.
*/

#ifndef TOOLBAR_VIEW_H
#define TOOLBAR_VIEW_H

#include <String.h>
#include <View.h>

#include <vector>

class BButton;

class ToolbarView : public BView {
public:
	ToolbarView(const char* name);
	virtual ~ToolbarView();

	virtual void AttachedToWindow();
	virtual void FrameResized(float width, float height);

	virtual BSize MinSize();
	virtual BSize MaxSize();
	virtual BSize PreferredSize();

	// Aggiunge un pulsante gia' costruito (icona/messaggio/target gia'
	// impostati da BuildToolbar) come prossimo elemento orizzontale.
	// "label" e' lo stesso testo gia' passato a SetToolTip dal
	// chiamante: serve anche come etichetta della voce equivalente nel
	// menu di troppopieno, se il pulsante finisce nascosto.
	void AddButton(BButton* button, const char* label);
	// Segna un separatore verticale nella posizione corrente, fra due
	// gruppi di pulsanti.
	void AddSeparator();

	// Ricalcola posizione/visibilita' di ogni elemento dalla larghezza
	// corrente (Bounds().Width()): pubblico apposta per essere
	// richiamabile direttamente dai test subito dopo un ResizeTo(),
	// senza dipendere dal giro di andata/ritorno col app_server che
	// invoca FrameResized() -- non garantito sincrono rispetto al
	// thread di chi chiama ResizeTo(), a differenza di una chiamata
	// diretta (stesso motivo per cui SheetTabView::SetSheets() chiama
	// Layout() da se', invece di aspettare un giro di disegno).
	void Layout();

	// Esposti pubblicamente per essere testabili direttamente (stesso
	// principio di SheetTabView::IsScrolling()/TabRectFor()): permette
	// di verificare quanti pulsanti restano visibili e se il
	// troppopieno e' attivo senza dover simulare un vero
	// ridimensionamento a schermo ne' un vero clic.
	bool IsOverflowing() const { return fOverflowing; }
	int VisibleButtonCount() const;
	int HiddenButtonCount() const;
	BButton* OverflowButton() const { return fOverflowButton; }

	// Costruisce e mostra il BPopUpMenu con le voci equivalenti dei
	// pulsanti attualmente nascosti dal troppopieno. Pubblico (oltre
	// che richiamato dal pulsante ">>" stesso) apposta per essere
	// testabile/richiamabile direttamente, e per non dover passare da
	// BInvoker::SetTarget/BMessenger per il clic interno -- vedi
	// OverflowButton.
	void ShowOverflowMenu();

private:
	struct Item {
		BView* view;       // BButton* o BSeparatorView*
		bool isSeparator;
		BString label;     // vuota per i separatori
	};

	std::vector<Item> fItems;
	BButton* fOverflowButton;
	bool fOverflowing;
	float fRowHeight;
	float fNaturalWidth;
};

#endif
