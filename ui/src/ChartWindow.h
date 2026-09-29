/*
	ChartWindow.h

	Finestra "Grafico": un campo di testo per l'intervallo dati (es.
	"A1:B5", due colonne: etichette e valori), un selettore del tipo
	(Barre/Linee/Torta/Area/Dispersione/Combinato), un'anteprima
	(ChartView, "Disegna") e un
	secondo campo con la cella di destinazione per incorporare davvero
	il grafico nel foglio ("Inserisci nel foglio", vedi ChartObject in
	Chart.h). Stessa regola sui thread di FindWindow (vedi
	FindWindow.h): non tocca mai il documento direttamente, manda le
	richieste a MainWindow via BMessage e riceve indietro solo dati
	gia' estratti (mai un puntatore al documento), sempre via
	BMessage.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef CHART_WINDOW_H
#define CHART_WINDOW_H

#include <vector>

#include <Messenger.h>
#include <Window.h>

#include "Chart.h"

const uint32 kMsgChartRequest = 'chrq';
const uint32 kMsgChartData = 'chdt';
const uint32 kMsgChartInsert = 'chin';
// Risposta di HandleChartRequest quando l'intervallo ha piu' di due
// colonne (serie multiple, Fase 17) -- vedi il commento su
// MultiChartData in Chart.h e il gestore in ChartWindow.cpp.
const uint32 kMsgChartDataMulti = 'chdm';
// Risposta di HandleChartRequest per un grafico a dispersione (Fase
// 35): il tipo scelto viaggia DENTRO kMsgChartRequest stesso (vedi
// RequestDraw), cosi' MainWindow sa quale dei tre percorsi (singola
// serie/multi/dispersione) costruire senza doverlo indovinare dalla
// sola forma dell'intervallo.
const uint32 kMsgChartDataScatter = 'chds';
// Conferma di ChartWindow quando fEditingChartIndex >= 0 (vedi
// LoadForEdit): stesso schema di kMsgChartInsert, ma porta anche
// "index" (il grafico esistente da aggiornare) e NON porta "dest" --
// la posizione di un grafico gia' incorporato non cambia editandolo.
const uint32 kMsgChartUpdate = 'chup';
// ChartWindow non tocca mai SheetView direttamente (vedi il commento in
// cima al file): il pulsante "..." manda questo a fTarget (MainWindow),
// che inoltra a SheetView::StartRangePicker/CancelRangePicker. Porta un
// campo bool "start" (true = arma la selezione, false = disarma senza
// scegliere nulla -- un secondo clic sullo stesso pulsante mentre e'
// gia' armato).
const uint32 kMsgChartRangePickRequest = 'crpr';
// Risposta di SheetView (via MainWindow, BMessenger(chartWindow) passato
// come target a StartRangePicker) quando l'utente completa un clic o un
// trascinamento sul foglio: porta "range", il testo gia' formattato da
// FormatRangeRef, pronto per fRangeField->SetText().
const uint32 kMsgRangePicked = 'rpkd';

class BBox;
class BButton;
class BCheckBox;
class BMenu;
class BMenuField;
class BTextControl;
class ChartView;
class ChartColorSwatch;
class ChartSeriesOptionsWindow;
class ColorWindow;

class ChartWindow : public BWindow {
public:
	ChartWindow(BMessenger target);

	// Precompila il campo Intervallo e disegna subito l'anteprima --
	// usata da MainWindow::ShowChartWindow quando il foglio ha gia'
	// una selezione di piu' di una cella al momento dell'apertura,
	// cosi' l'utente non deve ridigitare come testo un intervallo gia'
	// selezionato sul foglio (Fase 18). Esce SEMPRE dalla modalita' di
	// modifica (vedi LoadForEdit sotto): un "Inserisci grafico" aperto
	// dopo aver modificato un grafico esistente non deve restare
	// agganciato a quello vecchio.
	void LoadRange(const char* rangeText);

	// Precompila intervallo/tipo/titolo con le impostazioni ATTUALI di
	// un grafico gia' incorporato e passa in modalita' "modifica": il
	// pulsante in basso diventa "Aggiorna" invece di "Inserisci nel
	// foglio" e il campo destinazione (che non ha senso per un grafico
	// che esiste gia' da qualche parte) si nasconde. Alla conferma
	// (kMsgInsertLocal) la finestra manda kMsgChartUpdate con "index"
	// invece di kMsgChartInsert -- vedi MainWindow::EditChart/
	// HandleChartUpdate.
	void LoadForEdit(int chartIndex, const char* rangeText, const char* title, ChartType type,
		bool rowOriented, const std::vector<rgb_color>& seriesColors,
		const std::vector<ChartSeriesOptions>& seriesOptions = std::vector<ChartSeriesOptions>());

	// Usata da MainWindow::ShowChartWindow PRIMA di ogni altra cosa,
	// incondizionatamente: senza multi-selezione attiva LoadRange sopra
	// non verrebbe chiamata, e la finestra resterebbe agganciata
	// all'ultimo grafico modificato (fEditingChartIndex) invece di
	// tornare a "crea nuovo grafico".
	void ExitEditMode() { SetEditingChartIndex(-1); }

	// Pubblico apposta per essere testabile (stesso principio di
	// MainWindow::GetChartWindow): verifica che le 7 voci del menu Tipo
	// esistano davvero dopo la costruzione, senza dover ispezionare
	// pixel -- vedi tests/test_edit_chart.cpp.
	BMenu* TypeMenu() const;

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

private:
	// Comune a LoadRange/LoadForEdit: azzera o imposta la modalita' di
	// modifica (etichetta del pulsante, visibilita' del campo
	// destinazione).
	void SetEditingChartIndex(int chartIndex);

	BTextControl* fTitleField;
	BTextControl* fRangeField;
	// Pulsante "..." accanto a fRangeField (Fase selettore di
	// intervallo): manda kMsgChartRangePickRequest a fTarget invece di
	// toccare SheetView direttamente (stesso principio di ogni altra
	// richiesta di questa finestra). fPickingRange traccia solo
	// l'etichetta/lo stato del pulsante in questa finestra -- lo stato
	// "davvero armato" vive in SheetView (fRangePickerActive), non qui:
	// un secondo clic su questo pulsante manda semplicemente "start"
	// false per disarmarlo.
	BButton* fRangePickButton;
	bool fPickingRange;
	BMenuField* fTypeField;
	// "Scambia righe/colonne" (Fase colori/orientamento, richiesta
	// esplicita dell'utente): espone SOLO il caso semplice di Excel
	// (reinterpreta lo stesso intervallo rettangolare con l'asse
	// opposto come categorie, righe contigue implicite) -- l'elenco di
	// righe non contigue (ChartObject::valueRows quando popolato
	// dall'importazione XLSX) resta un campo di sola importazione, mai
	// esposto qui, stesso limite dichiarato di valueColumns.
	BCheckBox* fRowOrientedCheckbox;
	BTextControl* fDestField;
	BButton* fInsertButton;
	ChartView* fChartView;
	BMessenger fTarget;
	// -1 = sto creando un grafico nuovo (comportamento di sempre,
	// kMsgChartInsert); >= 0 = sto modificando il grafico a quell'indice
	// in MainWindow::fCharts (kMsgChartUpdate). Vedi LoadForEdit sopra.
	int fEditingChartIndex;
	// Riquadro (con titolo + suggerimento) che contiene la riga di
	// checkbox "mostra i valori", una voce per serie (Fase 19). Creato
	// una sola volta nel costruttore -- solo fSeriesCheckboxRow al suo
	// interno viene svuotato/ripopolato a ogni richiesta, cosi' titolo
	// e suggerimento non vengono mai distrutti e ricreati. Nascosto per
	// intero (Hide/Show) quando l'ultimo grafico richiesto e' a singola
	// serie, cosi' non resta un riquadro vuoto che sembra un controllo
	// "sparito" (vedi ClearSeriesCheckboxes/RebuildSeriesCheckboxes).
	BBox* fSeriesCheckboxBox;
	BView* fSeriesCheckboxRow;
	std::vector<BCheckBox*> fSeriesCheckboxes;
	// Riquadretti di colore, uno per serie, affiancati alle checkbox
	// sopra nella stessa riga -- stesso ciclo di vita esatto
	// (ricostruiti da RebuildSeriesCheckboxes/svuotati da
	// ClearSeriesCheckboxes insieme alle checkbox). Un clic su uno apre
	// fSeriesColorWindow per quella sola serie.
	std::vector<ChartColorSwatch*> fSeriesColorSwatches;
	// Pulsante "Opzioni..." per serie (Fase 7), affiancato allo swatch di
	// colore nella stessa riga -- stesso ciclo di vita esatto (vedi il
	// commento su fSeriesColorSwatches sopra). Un clic apre
	// fSeriesOptionsWindow per quella sola serie (linea di
	// tendenza/barre d'errore: troppa configurazione per stare in riga,
	// vedi ChartSeriesOptionsWindow.h).
	std::vector<BButton*> fSeriesOptionsButtons;
	// Colori scelti dall'utente per serie (Fase colori): indicizzato
	// come fSeriesCheckboxes/fMultiData.seriesNames, preservato fra una
	// richiesta e l'altra per NOME di serie in RebuildSeriesCheckboxes
	// (stesso principio dello stato spuntata/non spuntata delle
	// checkbox). alpha 0 = "non ancora scelto per questa serie", vedi
	// SeriesColor() in Chart.h.
	std::vector<rgb_color> fSeriesColorOverrides;
	// Opzioni per serie (Fase 7, "asse secondario / trendline / barre
	// d'errore"): stesso ciclo di vita/stessa preservazione per NOME di
	// serie di fSeriesColorOverrides sopra -- vedi RebuildSeriesCheckboxes.
	// Nessun controllo la mostra ancora (solo plumbing per ora, i
	// controlli veri arrivano con le fasi 7b/7c/7d), quindi resta sempre
	// una lista di ChartSeriesOptions() di default finche' non si
	// aggiungono i controlli.
	std::vector<ChartSeriesOptions> fSeriesOptions;
	// Riquadretto di colore per un grafico a SINGOLA serie (barre/linee/
	// area/barre orizzontali/dispersione): un solo colore per l'intero
	// grafico, mostrato vicino al campo Tipo invece che nella riga serie
	// (che per un grafico a singola serie resta nascosta). alpha 0 =
	// "non scelto", stesso principio di fSeriesColorOverrides sopra.
	ChartColorSwatch* fChartColorSwatch;
	rgb_color fChartColor;
	// Finestra Colore di PROPRIETA' di questa finestra (non quella
	// condivisa di MainWindow, sempre puntata a se stessa via
	// BMessenger(this)) -- creata al volo al primo clic su uno
	// swatch, riusata per ogni scelta successiva. Vedi
	// ColorWindow::SetTarget/SetSeriesIndex per il perche' serve
	// un'istanza propria invece di condividere quella di MainWindow.
	ColorWindow* fSeriesColorWindow;
	// Finestra Opzioni serie (Fase 7) di PROPRIETA' di questa finestra --
	// stesso principio esatto di fSeriesColorWindow sopra, creata al
	// volo al primo clic su un pulsante "Opzioni...", riusata per ogni
	// serie successiva (SetSeriesIndex/SetOptions la ripuntano prima di
	// ogni Show()).
	ChartSeriesOptionsWindow* fSeriesOptionsWindow;

	ChartType SelectedType() const;
	// Corpo comune di kMsgDrawLocal e LoadRange sopra: applica il
	// titolo corrente all'anteprima e richiede a MainWindow i dati
	// dell'intervallo attualmente in fRangeField.
	void RequestDraw();
	// Ricostruisce fSeriesCheckboxRow per i dati appena ricevuti,
	// preservando lo stato di una checkbox il cui nome di serie
	// combacia con quello di prima (stessa serie tra una richiesta e
	// l'altra) e imposta data->showValues di conseguenza -- chiamata
	// PRIMA di ChartView::SetMultiData, cosi' il grafico si disegna
	// gia' con le visibilita' corrette al primo giro, non solo dopo
	// che l'utente tocca una checkbox.
	void RebuildSeriesCheckboxes(MultiChartData* data);
	// Nessuna checkbox da mostrare per un grafico a singola serie.
	void ClearSeriesCheckboxes();
	// Apre (creando fSeriesColorWindow al volo se serve) il selettore di
	// colore per la serie "index", o per l'intero grafico se index < 0
	// (vedi il commento su kMsgChartColorButtonLocal in ChartWindow.cpp).
	void ShowSeriesColorPicker(int index);
	// Apre (creando fSeriesOptionsWindow al volo se serve) il pop-up
	// linea di tendenza/barre d'errore per la serie "index" -- vedi
	// ChartSeriesOptionsWindow.h e il commento su
	// kMsgSeriesOptionsButtonLocal in ChartWindow.cpp.
	void ShowSeriesOptionsPopup(int index);
	// Disarma fRangePickButton/manda "start"=false a fTarget se un
	// selettore era rimasto armato da una sessione precedente -- usato
	// da LoadRange/LoadForEdit/QuitRequested, gli unici tre punti in cui
	// la finestra puo' "ripartire da capo" mentre il pulsante era ancora
	// in attesa di un clic sul foglio. Non fa nulla se non era armato.
	void CancelPickingIfArmed();
};

#endif
