/*
	MainWindow.h

	Finestra principale: barra dei menu, barra formula, griglia del
	foglio (SheetView) dentro una BScrollView. Apertura file passa
	dal Translation Kit (BTranslatorRoster), che sceglie
	automaticamente il translator installato adatto (CSV/XLS/XLSX/
	ODS/ASCD nativo) in base al contenuto del file.
*/

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <vector>

#include <GraphicsDefs.h>
#include <String.h>
#include <Window.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Chart.h"
#include "Container.h"

class BFilePanel;
class BMenu;
class BMenuItem;
class BTextControl;
class BStringView;
class SheetView;
class SheetTabView;
class FindWindow;
class ChartWindow;
class PivotWindow;
class NameWindow;
class PasteSpecialWindow;
class GoToWindow;
class RenameSheetWindow;
class ColorWindow;
class PreferencesWindow;

// MainWindow implementa ISheetResolver (Container.h, Fase 9) perche'
// e' lei a possedere fSheets, l'unico elenco di "nome foglio -> CContainer*"
// che esista: i CContainer dei fogli aperti si limitano a tenere un
// puntatore preso in prestito a questa interfaccia (vedi
// AttachSheetResolver sotto), mai a fSheets direttamente.
class MainWindow : public BWindow, public ISheetResolver {
public:
	MainWindow();
	virtual ~MainWindow();

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();
	// BWindow la chiama subito prima di mostrare/tracciare un menu
	// qualunque della finestra: punto comodo per ricostruire "Apri
	// recenti" con l'elenco aggiornato al momento, senza dover
	// notificare le altre finestre aperte quando una di esse apre un
	// file (l'app supporta piu' finestre, vedi il commento su fSheets
	// sotto) -- ogni finestra legge lo stato condiviso al momento
	// giusto invece di tenerne una copia da tenere sincronizzata.
	virtual void MenusBeginning();

	void OpenFile(const entry_ref& ref);
	void SelectionChanged(cell c);

	// Esposti pubblicamente apposta per essere testabili direttamente
	// (stesso principio di SheetView::SortSelection ecc. -- vedi
	// tests/test_paste_range.cpp): Taglia/Copia/Incolla e il formato
	// numerico operano su SheetView::SelectionRange(), non solo sulla
	// cella attiva, quindi la logica di conversione a/da griglia TSV e
	// di dimensionamento dell'intervallo di destinazione merita una
	// verifica diretta senza passare da un vero ciclo di dispatch dei
	// messaggi di menu.
	SheetView* GetSheetView() const { return fSheetView; }
	// Pubblico apposta per essere testabile direttamente (stesso
	// principio di GetSheetView sopra): verifica che SelectionChanged
	// anteponga davvero "=" al testo di una cella con formula (vedi il
	// commento li' sopra). Definito in MainWindow.cpp, non qui: questo
	// header dichiara solo BTextControl* fFormulaBar in avanti (vedi
	// SheetView.h), senza il tipo completo -- Text() richiede la
	// definizione vera di BTextControl.
	const char* FormulaBarText() const;
	void CopySelection(bool cut);
	void PasteSelection();
	void SetCellFormat(int32 format);

	// Pubblici per lo stesso motivo di SetCellFormat sopra -- vedi
	// tests/test_format.cpp. Grassetto/Corsivo si basano sullo stato
	// della sola cella attiva (fSheetView->Selection(), come Excel: il
	// pulsante "risulta premuto" o no in base a quella) ma si
	// applicano a tutto SelectionRange(), invertendo lo stato letto
	// dall'attiva -- se non e' in grassetto, l'intera selezione lo
	// diventa, e viceversa. SetAlignment/SetTextColor/SetBackgroundColor
	// si applicano sempre a tutto SelectionRange() direttamente, stesso
	// principio di SetCellFormat.
	void ToggleBold();
	void ToggleItalic();
	// Sottolineato (Fase 12): CellStyle::fUnderline, stesso principio
	// di ToggleBold/ToggleItalic sopra ma un booleano a parte -- BFont
	// non ha un attributo sottolineato nativo (vedi il commento sul
	// campo in CellStyle.h).
	void ToggleUnderline();
	// A capo automatico (Fase 12): CellStyle::fWrapText, stesso
	// principio di ToggleUnderline sopra, ma fa anche crescere
	// l'altezza delle righe coinvolte se serve (vedi SheetView::
	// RecalculateWrappedRowHeights).
	void ToggleWrapText();

	// Celle unite (Fase 12): un rettangolo per foglio (CContainer::
	// AddMergedRange), non un campo per cella -- vedi il commento
	// sull'implementazione in MainWindow.cpp per cosa succede al
	// contenuto delle celle diverse da quella in alto a sinistra.
	void MergeCells();
	void UnmergeCells();
	void SetAlignment(char alignment);
	void SetTextColor(rgb_color color);
	void SetBackgroundColor(rgb_color color);

	// Bordi di cella (Fase 11), stesso principio di ToggleBold/
	// ToggleItalic sopra -- vedi tests/test_borders.cpp. "side": 0 =
	// superiore, 1 = sinistro, 2 = inferiore, 3 = destro (stesso
	// ordine di CellStyle::fTBorderColor/fLBorderColor/fBBorderColor/
	// fRBorderColor). ClearBorders toglie tutti e quattro i lati in
	// un colpo solo, sempre su tutto SelectionRange() direttamente
	// (nessuno stato "di partenza" da invertire, a differenza di
	// ToggleBorder).
	void ToggleBorder(int side);
	void ClearBorders();

	// Pubblico per lo stesso motivo di SetCellFormat sopra -- vedi
	// tests/test_preferences.cpp. showGrid si applica alla sola
	// SheetView di questa finestra; decimalSeparator/listSeparator
	// sono globali al motore (gDecimalPoint/gListSeparator, lette da
	// TryToParseString quando non si passa un separatore esplicito),
	// quindi valgono per l'intera applicazione, non solo per il
	// documento corrente -- stesso comportamento di Sum-It storico.
	void HandlePreferencesRequest(bool showGrid, char decimalSep, char listSep);

	// Chiamato da SheetView (che possiede fDoc solo indirettamente,
	// tramite il puntatore che MainWindow gli passa) ogni volta che
	// una delle sue operazioni muta il documento -- stesso principio
	// di SelectionChanged/NotifySelectionChanged, per il titolo della
	// finestra e l'avviso prima di scartare modifiche non salvate
	// (Nuovo/Apri/Esci).
	void DocumentChanged();

	// Pubblici per lo stesso motivo di CopySelection/PasteSelection/
	// SetCellFormat sopra -- vedi tests/test_unsaved_changes.cpp.
	// ConfirmDiscardChanges() restituisce true senza mostrare nessun
	// BAlert quando non ci sono modifiche in sospeso (l'unico ramo
	// testabile in automatico: un vero clic su un BAlert non lo e',
	// stesso limite gia' documentato altrove in questo progetto per i
	// dialoghi modali interattivi).
	bool IsModified() const { return fModified; }
	bool ConfirmDiscardChanges();

	// Usato da App::RefsReceived per decidere se riusare questa finestra
	// per un file in arrivo invece di aprirne una nuova (vedi App.cpp) --
	// vera solo per una finestra "vergine" come quella creata da
	// ReadyToRun all'avvio: mai nessuna modifica e mai nessun file
	// aperto con successo (OpenFile imposta fDocumentName solo a
	// caricamento riuscito).
	bool IsUntouched() const { return !fModified && fDocumentName.Length() == 0; }

	// Pubblici per lo stesso motivo di CopySelection/PasteSelection
	// sopra -- vedi tests/test_multisheet.cpp. SheetCount()/SheetName()
	// permettono di verificare l'elenco dei fogli letti da un file
	// senza dover aprire il menu a tendina vero e proprio.
	int SheetCount() const { return (int)fSheets.size(); }
	int ActiveSheetIndex() const { return fActiveSheetIndex; }
	const char* SheetName(int index) const { return fSheets[index].name.String(); }
	void SwitchToSheet(int index);

	// Nuovo/Elimina/Rinomina foglio (Fase 13): pubblici per lo stesso
	// motivo di CopySelection/PasteSelection sopra -- testabili
	// direttamente senza passare da un vero clic destro sulla scheda.
	// NewSheet() sceglie da sola un nome libero ("Foglio1", "Foglio2",
	// ... come Excel/LibreOffice Calc, mai un nome gia' usato).
	// DeleteSheet() chiede conferma con un vero BAlert (stesso schema
	// di ConfirmDiscardChanges) prima di chiamare DeleteSheetNoConfirm
	// sotto -- quest'ultima e' pubblica APPOSTA per essere testabile
	// senza mostrare un vero BAlert, che bloccherebbe un test
	// automatico in attesa di un clic reale (stesso limite noto gia'
	// documentato in tests/test_unsaved_changes.cpp per
	// ConfirmDiscardChanges). RenameSheet() rifiuta un nome gia' usato
	// da un altro foglio (stesso vincolo di Excel), con un BAlert solo
	// in quel caso di errore (non nel percorso normale, quindi
	// testabile senza problemi).
	void NewSheet();
	void DeleteSheet(int index);
	void DeleteSheetNoConfirm(int index);
	void RenameSheet(int index, const char* newName);
	BString UniqueSheetName(const char* prefix) const;

	// ISheetResolver (Fase 9): risolve "NomeFoglio!Cella" verso il
	// CContainer corrispondente in fSheets, per nome. Pubblico perche'
	// la classe lo espone come override di un'interfaccia pubblica, non
	// per uso diretto dall'esterno -- vedi AttachSheetResolver sotto
	// per come i CContainer dei fogli ne ricevono il puntatore.
	CContainer* ResolveSheetByName(const char* inName);

	// Pubblico per lo stesso motivo di CopySelection/PasteSelection
	// sopra -- vedi tests/test_xsheet.cpp. Ricalcola l'intera cartella
	// di lavoro (tutti i fogli, non solo quello attivo) se ne esiste
	// piu' di uno, altrimenti il solo foglio attivo (RecalculateAll,
	// piu' economico) -- serve perche' una formula in un foglio puo'
	// referenziarne un altro (vedi AscdIO.h), quindi modificare un
	// foglio puo' richiedere di ricalcolare anche gli altri. Sostituisce
	// RecalculateAll(fDoc) in tutti i punti che gia' lo chiamavano dopo
	// una modifica del documento.
	void RecalculateActiveWorkbook();

	// Pubblici per lo stesso motivo di CopySelection/PasteSelection
	// sopra -- vedi tests/test_names.cpp. Definire/eliminare un nome
	// ricalcola l'intera cartella di lavoro (una ridefinizione puo'
	// cambiare qualunque formula in qualunque foglio, stesso motivo di
	// RecalculateActiveWorkbook sopra); "Vai a" sposta la selezione
	// della SheetView attiva sull'intervallo risolto.
	void HandleDefineName(const char* name, const char* rangeText);
	void HandleDeleteName(const char* name);
	void HandleGoToName(const char* name);

	// Pubblico per lo stesso motivo di CopySelection/PasteSelection
	// sopra -- vedi tests/test_paste_special.cpp. content/operation/
	// transpose vengono da PasteSpecialWindow (vedi il commento sopra
	// la sua implementazione in MainWindow.cpp per il significato dei
	// due codici numerici).
	void HandlePasteSpecialRequest(int32 content, int32 operation, bool transpose);

	// Pubblico per lo stesso motivo di CopySelection/PasteSelection
	// sopra -- vedi tests/test_goto.cpp. "range" accetta sia una sola
	// cella ("C15") sia un intervallo ("A1:B5"), stessa sintassi di
	// RangeRef::ParseRangeRef; un testo non valido non fa nulla
	// (nessuno spostamento, nessun crash).
	void HandleGoToRequest(const char* rangeText);

private:
	// Elenco degli ultimi file aperti (menu File > "Apri recenti"),
	// persistito in gPrefs come 5 chiavi "recentFileN" (N=0..4, la piu'
	// recente per prima) -- CPreferences non ha un tipo lista, solo
	// stringa/intero/double (vedi Preferences.h), da qui lo slot fisso
	// invece di una singola stringa con separatore.
	static const int kMaxRecentFiles = 5;
	static std::vector<BString> LoadRecentFiles();
	static void SaveRecentFiles(const std::vector<BString>& paths);
	void AddToRecentFiles(const entry_ref& ref);
	void RebuildRecentMenu();

	SheetView* fSheetView;
	BMenuItem* fFreezeMenuItem;
	BMenu* fRecentMenu;
	BTextControl* fFormulaBar;
	BStringView* fCellLabel;
	CContainer* fDoc;
	BFilePanel* fOpenPanel;
	BFilePanel* fSavePanel;
	FindWindow* fFindWindow;
	ChartWindow* fChartWindow;
	PivotWindow* fPivotWindow;
	NameWindow* fNameWindow;
	PasteSpecialWindow* fPasteSpecialWindow;
	GoToWindow* fGoToWindow;
	RenameSheetWindow* fRenameSheetWindow;
	ColorWindow* fColorWindow;
	PreferencesWindow* fPreferencesWindow;
	std::vector<ChartObject> fCharts;
	std::vector<EmbeddedImage> fImages;

	// Cartella di lavoro multi-foglio (Fase 9): fSheets tiene un
	// AscdSheet (nome + documento + grafici) per ogni foglio, in
	// ordine di tabulazione; fDoc/fCharts sopra restano il "foglio
	// attivo" -- letti/scritti direttamente da tutte le operazioni
	// gia' esistenti (Taglia/Copia/Incolla, Trova e sostituisci,
	// grafici, tabelle pivot...), che quindi non hanno bisogno di
	// sapere nulla dei fogli multipli: SwitchToSheet() si limita a
	// ripuntare fDoc al CContainer del nuovo foglio attivo (nessuna
	// copia: e' lo stesso puntatore gia' tenuto in fSheets) e a
	// risincronizzare fCharts (un vector per valore, non un
	// puntatore, quindi va ricopiato avanti e indietro esplicitamente
	// a ogni cambio). fSheets[fActiveSheetIndex].doc rimane sempre lo
	// stesso oggetto puntato da fDoc mentre quel foglio e' attivo.
	std::vector<AscdSheet> fSheets;
	int fActiveSheetIndex;
	SheetTabView* fSheetTabView;

	// Sostituisce l'intera cartella di lavoro con un solo foglio
	// vuoto di nome "name" -- usato da NewDocument() e come base
	// prima di popolare i fogli letti da un file. Rilascia tutti i
	// CContainer dei fogli precedenti (Release(), mai delete diretto).
	void ResetWorkbook(const char* name);
	void RebuildSheetTabs();

	// Collega questa MainWindow (come ISheetResolver) a ogni
	// CContainer in fSheets, cosi' le formule possono referenziare un
	// foglio diverso dal proprio (Fase 9) -- chiamato da ResetWorkbook
	// e da OpenFile ogni volta che fSheets cambia. Il puntatore e'
	// preso in prestito (mai posseduto dai CContainer, vedi il
	// commento su ISheetResolver in Container.h): non serve nessuno
	// scollegamento esplicito quando un foglio viene rilasciato, dato
	// che a quel punto il puntatore stesso smette di essere usato.
	void AttachSheetResolver();

	// Nome del file corrente (solo il nome, non il percorso completo:
	// basta per il titolo -- vedi UpdateTitle) e se il documento ha
	// modifiche non ancora salvate da quando e' stato aperto/creato/
	// salvato l'ultima volta. Vuoto = documento nuovo, mai salvato.
	BString fDocumentName;
	bool fModified;

	void UpdateTitle();
	void MarkModified();

	void NewDocument();
	void CommitFormulaBar();
	void SaveToFile(const entry_ref& dir, const char* name);
	void DeleteSelection();
	void PrintDocument();
	void ShowFindWindow();
	void FindNext(const char* searchText);
	void ReplaceCurrent(const char* searchText, const char* replaceText);
	void ReplaceAll(const char* searchText, const char* replaceText);
	void ShowChartWindow();
	void ShowPivotWindow();
	void HandleChartRequest(const char* rangeText);
	void HandleChartInsert(const char* rangeText, const char* destText);
	void HandlePivotRequest(const char* sourceText, const char* destText, int32 agg);
	void ShowNameWindow();
	void RefreshNameWindow();
	void ShowPasteSpecialWindow();
	void ShowGoToWindow();
	void ShowColorWindow(bool background);
	void ShowPreferencesWindow();
};

#endif
