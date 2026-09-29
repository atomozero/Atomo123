/*
	NamedStyleWindow.h

	Finestra "Gestione stili cella" (Tier 4, "Path to full Excel parity",
	Fase C -- UI di "Named cell styles + live theme palette", vedi
	NamedStyle.h): elenco degli stili con nome (BListView, built-in
	compresi), un campo nome (usato solo per "Nuovo dalla selezione"),
	sfondo/testo (un BMenuField per scegliere fra un colore letterale o
	un ruolo della tavolozza tema, piu' un riquadretto di anteprima),
	grassetto/corsivo/sottolineato, quattro pulsanti (Applica/Nuovo
	dalla selezione/Aggiorna/Elimina), e una striscia di 8 riquadretti
	per i colori della tavolozza tema stessa.

	Stessa regola di ogni altra finestra di questo progetto (vedi il
	commento in cima a ChartWindow.h): non tocca mai il documento
	direttamente, manda le richieste a MainWindow via BMessage. "Nuovo
	dalla selezione" e' l'UNICO modo per creare uno stile personalizzato
	in questa fase (niente editor "da zero"): la finestra manda solo il
	nome scelto, MainWindow cattura l'aspetto GIA' RISOLTO della cella
	attiva e lo salva come nuova definizione -- corrisponde al flusso
	piu' comune del vero Excel ("formatta una cella come vuoi, poi
	salvala come stile").

	MainWindow richiama SetStyles()/SetTheme() prima di Show() e dopo
	ogni Applica/Nuovo/Aggiorna/Elimina/cambio tema per tenere l'elenco e
	l'anteprima allineati al proprio stato.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef NAMED_STYLE_WINDOW_H
#define NAMED_STYLE_WINDOW_H

#include <Messenger.h>
#include <String.h>
#include <Window.h>
#include <vector>

#include "NamedStyle.h"

// Applica lo stile "styleID" alla selezione corrente del foglio.
const uint32 kMsgApplyNamedStyle = 'apns';
// Cattura l'aspetto GIA' RISOLTO della cella attiva come un nuovo
// stile personalizzato chiamato "name" -- vedi il commento in cima al
// file sul perche' e' l'UNICO modo per creare uno stile in questa
// fase.
const uint32 kMsgCreateNamedStyleFromSelection = 'crns';
// Ridefinisce lo stile "styleID" (built-in o personalizzato) con i
// campi sfondo/testo/grassetto/corsivo/sottolineato correnti della
// finestra -- IL momento "vivo": ogni cella che referenzia questo ID
// cambia aspetto subito, senza toccare nessuna cella.
const uint32 kMsgRedefineNamedStyle = 'rdns';
// Elimina lo stile personalizzato "styleID" (nessun effetto sui
// built-in, mai eliminabili).
const uint32 kMsgDeleteNamedStyle = 'dlns';
// Cambia il colore del ruolo tema "role" (vedi ThemeColorRole in
// NamedStyle.h) -- IL momento "vivo" per il tema: ogni stile che
// referenzia questo ruolo cambia aspetto subito in ogni cella che lo
// usa, senza toccare ne' lo stile ne' la cella.
const uint32 kMsgSetThemeColor = 'sthc';

class BButton;
class BCheckBox;
class BListView;
class BMenuField;
class BTextControl;
class ColorWindow;
class SwatchView;

class NamedStyleWindow : public BWindow {
public:
	NamedStyleWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

	// Chiamata da MainWindow prima di Show() e dopo ogni
	// Applica/Nuovo/Aggiorna/Elimina per tenere l'elenco allineato a
	// MainWindow::NamedStyles() -- stesso schema di
	// ScenarioWindow::SetScenarios.
	void SetStyles(const NamedStyleTable& styles);
	// Chiamata insieme a SetStyles() (stesso ciclo di aggiornamento) e
	// da sola dopo ogni cambio di un colore tema -- serve per calcolare
	// l'anteprima risolta (NamedStyleDef::Resolve) senza mai toccare il
	// documento.
	void SetTheme(const ThemePalette& theme);

private:
	BListView* fStyleList;
	// fStyleList[i] (0-based) corrisponde allo stile con ID
	// fListIDs[i] -- necessario perche' gli slot rimossi (tombstone,
	// vedi NamedStyleTable::IsRemovedAt) sono saltati dall'elenco ma
	// non dagli ID stessi, quindi indice di riga e ID NON coincidono in
	// generale.
	std::vector<int> fListIDs;
	NamedStyleTable fStyles;
	ThemePalette fTheme;

	BTextControl* fNameField;

	BMenuField* fBackgroundRoleField;
	SwatchView* fBackgroundSwatch;
	BMenuField* fTextRoleField;
	SwatchView* fTextSwatch;
	BCheckBox* fBoldCheckbox;
	BCheckBox* fItalicCheckbox;
	BCheckBox* fUnderlineCheckbox;

	std::vector<SwatchView*> fThemeSwatches; // kThemeColorRoleCount voci, indice = ruolo

	// Colori LETTERALI scelti a mano quando il rispettivo BMenuField e'
	// su "Colore personalizzato" -- preservati anche quando l'utente
	// passa temporaneamente su un ruolo tema e poi torna indietro,
	// stesso principio di fChartColor in ChartWindow.h.
	rgb_color fBackgroundLiteral;
	rgb_color fTextLiteral;

	// Finestra Colore di PROPRIETA' di questa finestra (mai quella
	// condivisa di MainWindow), stesso principio esatto di
	// ChartWindow::fSeriesColorWindow -- creata al volo al primo clic
	// su uno qualunque dei riquadretti (letterale sfondo/testo, o uno
	// degli 8 del tema).
	ColorWindow* fColorWindow;
	// -1 = nessuna scelta di colore in corso; 0 = sfondo letterale;
	// 1 = testo letterale; altrimenti (kind >= 2) un ruolo tema, vedi
	// ShowColorPicker.
	int fColorPickKind;

	BMessenger fTarget;

	void RebuildStyleList();
	// Popola i controlli sfondo/testo/grassetto/corsivo/sottolineato
	// dallo stile con questo ID (chiamata alla selezione in
	// fStyleList) -- niente comportamento se styleID non esiste piu'
	// (stile rimosso da un'altra finestra/sessione).
	void LoadStyleIntoControls(int styleID);
	// Aggiorna i DUE riquadretti di anteprima (sfondo/testo) risolvendo
	// lo stato CORRENTE dei controlli contro fTheme -- chiamata dopo
	// ogni modifica ai BMenuField/riquadretti letterali, cosi'
	// l'anteprima segue sempre quello che si vedrebbe premendo
	// "Aggiorna" in quel momento.
	void RefreshPreview();
	// Apre (creando fColorWindow al volo se serve) il selettore di
	// colore per kind (0=sfondo letterale, 1=testo letterale,
	// >=2=ruolo tema, vedi fColorPickKind sopra).
	void ShowColorPicker(int kind);
};

#endif
