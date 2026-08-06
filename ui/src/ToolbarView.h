/*
	ToolbarView.h

	Contenitore per i pulsanti della toolbar (BuildToolbar in
	MainWindow.cpp), organizzati in gruppi (un separatore verticale fra
	un gruppo e il successivo). I gruppi si affiancano orizzontalmente e,
	quando la finestra si restringe e un gruppo non entra piu' nella
	larghezza disponibile, va a capo per intero su una nuova riga -- MAI
	un singolo pulsante a meta' di un gruppo, che resterebbe spezzato fra
	due righe senza motivo (chiesto esplicitamente dall'utente dopo aver
	visto la toolbar su righe fisse per categoria: un gruppo mezzo su una
	riga e mezzo sulla successiva era peggio della singola riga
	originale). L'altezza della vista cresce con le righe che servono
	(GetHeightForWidth, protocollo standard del Layout Kit per una vista
	la cui altezza dipende dalla larghezza assegnata), cosi' il resto
	della finestra si sposta di conseguenza -- non serve piu' un
	pulsante ">>" di troppopieno, ogni pulsante resta sempre visibile da
	qualche parte nella toolbar.
*/

#ifndef TOOLBAR_VIEW_H
#define TOOLBAR_VIEW_H

#include <String.h>
#include <View.h>

#include <vector>

class BButton;
class BSeparatorView;

class ToolbarView : public BView {
public:
	ToolbarView(const char* name);
	virtual ~ToolbarView();

	virtual void AttachedToWindow();
	virtual void FrameResized(float width, float height);

	virtual BSize MinSize();
	virtual BSize MaxSize();
	virtual BSize PreferredSize();

	// Protocollo "altezza in funzione della larghezza" del Layout Kit:
	// senza questo, un BGroupLayout (vedi BuildToolbar) non saprebbe mai
	// che restringere la finestra puo' far crescere l'altezza di questa
	// vista (piu' righe), e continuerebbe a darle sempre l'altezza di
	// una singola riga indipendentemente da quante gliene servono.
	virtual bool HasHeightForWidth();
	virtual void GetHeightForWidth(float width, float* min, float* max,
		float* preferred);

	// Aggiunge un pulsante gia' costruito (icona/messaggio/target gia'
	// impostati da BuildToolbar) al gruppo corrente. "label" e' lo
	// stesso testo gia' passato a SetToolTip dal chiamante.
	void AddButton(BButton* button, const char* label);
	// Chiude il gruppo corrente e ne apre uno nuovo (separato dal
	// precedente da un separatore verticale, nascosto quando il nuovo
	// gruppo finisce per essere il primo della sua riga -- un
	// separatore appena dopo l'a capo non separerebbe piu' niente).
	void AddSeparator();

	// Ricalcola posizione/visibilita'/riga di ogni gruppo dalla
	// larghezza corrente (Bounds().Width()): pubblico apposta per
	// essere richiamabile direttamente dai test subito dopo un
	// ResizeTo(), senza dipendere dal giro di andata/ritorno col
	// app_server che invoca FrameResized() -- non garantito sincrono
	// rispetto al thread di chi chiama ResizeTo(), a differenza di una
	// chiamata diretta (stesso motivo per cui SheetTabView::SetSheets()
	// chiama Layout() da se', invece di aspettare un giro di disegno).
	void Layout();

	// Esposti pubblicamente per essere testabili direttamente (stesso
	// principio di SheetTabView::IsScrolling()/TabRectFor()): quante
	// righe servono alla larghezza corrente e quanti pulsanti in totale
	// (sempre tutti visibili, a differenza del vecchio troppopieno).
	int RowCount() const { return fRowCount; }
	int ButtonCount() const;

private:
	struct ButtonItem {
		BButton* view;
		BString label;
	};

	struct Group {
		std::vector<ButtonItem> buttons;
		BSeparatorView* separator; // NULL per il primo gruppo
	};

	// Impacchetta i gruppi in righe per la larghezza data, senza
	// toccare nessuna BView (usato sia da Layout(), che poi posiziona
	// davvero i pulsanti, sia da GetHeightForWidth(), a cui interessa
	// solo quante righe risulterebbero). "row" nell'array di uscita e'
	// la riga (0-based) assegnata al gruppo con lo stesso indice.
	int _PackGroups(float availWidth, std::vector<int>* outRows) const;
	float _GroupWidth(const Group& group) const;

	std::vector<Group> fGroups;
	int fRowCount;
	float fRowHeight;
	float fNaturalWidth; // larghezza totale se tutti i gruppi stessero su una riga sola
};

#endif
