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

#include <String.h>
#include <Window.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Chart.h"
#include "Container.h"

class BFilePanel;
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
	void CopySelection(bool cut);
	void PasteSelection();
	void SetCellFormat(int32 format);

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
	SheetView* fSheetView;
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
	std::vector<ChartObject> fCharts;

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
};

#endif
