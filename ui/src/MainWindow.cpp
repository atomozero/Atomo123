/*
	MainWindow.cpp

	Vedi MainWindow.h.
*/

#include "MainWindow.h"
#include "SheetView.h"
#include "AscdIO.h"
#include "FindWindow.h"
#include "ChartWindow.h"
#include "PivotWindow.h"
#include "Chart.h"
#include "Pivot.h"
#include "RangeRef.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <Clipboard.h>
#include <Directory.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <PrintJob.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslatorRoster.h>
#include <TranslationDefs.h>

#include "CellStyle.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "Formatter.h"
#include "Range.h"

static const uint32 kMsgNew = 'anew';
static const uint32 kMsgOpen = 'aopn';
static const uint32 kMsgSaveAs = 'asva';
static const uint32 kMsgFormulaCommit = 'afml';
static const uint32 kMsgCut = 'acut';
static const uint32 kMsgCopy = 'acpy';
static const uint32 kMsgPaste = 'apst';
static const uint32 kMsgClear = 'aclr';
static const uint32 kMsgSelectAll = 'asla';
static const uint32 kMsgFillDown = 'afdn';
static const uint32 kMsgFillRight = 'afrt';
static const uint32 kMsgPrint = 'aprt';
static const uint32 kMsgFind = 'afnd';
static const uint32 kMsgSetFormat = 'stfm';
static const uint32 kMsgShowChart = 'shch';
static const uint32 kMsgShowPivot = 'shpv';

static const uint32 kAtomoNativeFormat = 'ASCD';
static const uint32 kAtomoCsvFormat = 'ACSV';

// Stessa logica di SheetView::ColumnName (vedi li' per il perche'
// della duplicazione: e' una manciata di righe, non vale la pena
// condividerla tramite un header dedicato).
static void ColumnName(int col, char* out)
{
	char buf[8];
	int n = 0;
	while (col > 0)
	{
		int rem = (col - 1) % 26;
		buf[n++] = 'A' + rem;
		col = (col - 1) / 26;
	}
	for (int i = 0; i < n; i++)
		out[i] = buf[n - 1 - i];
	out[n] = 0;
}

MainWindow::MainWindow()
	:
	BWindow(BRect(80, 80, 900, 700), "Atomo123", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
{
	fDoc = new CContainer(NULL, NULL);

	BMenuBar* menuBar = new BMenuBar("menu");
	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("Nuovo", new BMessage(kMsgNew), 'N'));
	fileMenu->AddItem(new BMenuItem("Apri" B_UTF8_ELLIPSIS, new BMessage(kMsgOpen), 'O'));
	fileMenu->AddItem(new BMenuItem("Salva con nome" B_UTF8_ELLIPSIS,
		new BMessage(kMsgSaveAs), 'S'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Stampa" B_UTF8_ELLIPSIS, new BMessage(kMsgPrint), 'P'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Esci", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(fileMenu);

	// Taglia/copia/incolla passano dal vero BClipboard di sistema (non
	// da un appunti interno all'app): il contenuto della cella (la
	// formula, come mostrata dalla barra formule) diventa testo piano
	// condivisibile anche con altre applicazioni Haiku.
	BMenu* editMenu = new BMenu("Modifica");
	editMenu->AddItem(new BMenuItem("Taglia", new BMessage(kMsgCut), 'X'));
	editMenu->AddItem(new BMenuItem("Copia", new BMessage(kMsgCopy), 'C'));
	editMenu->AddItem(new BMenuItem("Incolla", new BMessage(kMsgPaste), 'V'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Cancella", new BMessage(kMsgClear)));
	editMenu->AddSeparatorItem();
	// Niente scorciatoia Ctrl+A: su Haiku collide col byte generato da
	// Ctrl+Inizio (vedi il commento in SheetView::HandleKey) -- voce
	// di menu soltanto, come gia' fa Sum-It storico per lo stesso
	// comando (menu Modifica, "Select All", senza modificatore).
	editMenu->AddItem(new BMenuItem("Seleziona tutto", new BMessage(kMsgSelectAll)));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Trova e sostituisci" B_UTF8_ELLIPSIS,
		new BMessage(kMsgFind), 'F'));
	menuBar->AddItem(editMenu);

	// Formato numero della cella selezionata: agisce su CellStyle::fFormat
	// (letto da CContainer::GetCellResult/SheetView per la
	// visualizzazione). Valuta e percentuale sfruttano anche le
	// formattazioni dedicate del Locale Kit in SheetView::Draw.
	BMenu* formatMenu = new BMenu("Formato");
	BMessage* generalMsg = new BMessage(kMsgSetFormat);
	generalMsg->AddInt32("format", eGeneral);
	formatMenu->AddItem(new BMenuItem("Generale", generalMsg));
	BMessage* fixedMsg = new BMessage(kMsgSetFormat);
	fixedMsg->AddInt32("format", eFixed);
	formatMenu->AddItem(new BMenuItem("Numero", fixedMsg));
	BMessage* currencyMsg = new BMessage(kMsgSetFormat);
	currencyMsg->AddInt32("format", eCurrency);
	formatMenu->AddItem(new BMenuItem("Valuta", currencyMsg));
	BMessage* percentMsg = new BMessage(kMsgSetFormat);
	percentMsg->AddInt32("format", ePercent);
	formatMenu->AddItem(new BMenuItem("Percentuale", percentMsg));
	menuBar->AddItem(formatMenu);

	// Riempi in basso/a destra: copia la prima riga/colonna
	// dell'intervallo selezionato nel resto dell'intervallo,
	// aggiornando i riferimenti relativi nelle formule (vedi
	// SheetView::FillDown/FillRight). Ctrl+D/Ctrl+R come scorciatoie
	// di voce di menu (BMenuItem::SetShortcut, risolte dal BWindow
	// prima di arrivare a SheetView::KeyDown), non come casi in
	// SheetView::HandleKey: su Haiku B_END vale lo stesso byte (0x04)
	// generato da Ctrl+D (stesso problema gia' visto con Ctrl+A/
	// B_HOME per "Seleziona tutto"), quindi solo la scorciatoia di
	// menu -- risolta a un livello diverso, prima che KeyDown veda un
	// singolo byte ambiguo -- funziona in modo affidabile.
	BMenu* dataMenu = new BMenu("Dati");
	dataMenu->AddItem(new BMenuItem("Riempi in basso", new BMessage(kMsgFillDown), 'D'));
	dataMenu->AddItem(new BMenuItem("Riempi a destra", new BMessage(kMsgFillRight), 'R'));
	menuBar->AddItem(dataMenu);

	// Grafico e tabella pivot leggono un intervallo di due colonne
	// scelto dall'utente (non la sola cella selezionata, che oggi e'
	// l'unica selezione supportata dalla griglia -- vedi
	// docs/UI_ARCHITECTURE.md): l'intervallo si digita nella finestra
	// dedicata, non si trascina sulla griglia.
	BMenu* insertMenu = new BMenu("Inserisci");
	insertMenu->AddItem(new BMenuItem("Grafico a barre" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowChart)));
	insertMenu->AddItem(new BMenuItem("Tabella pivot" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowPivot)));
	menuBar->AddItem(insertMenu);

	// Barra strumenti: pulsanti di testo semplici (BButton), non
	// BToolBar -- quella classe vive solo sotto develop/headers/
	// private/shared/ su questo sistema, non nell'SDK pubblico
	// stabile, e il progetto usa solo API pubbliche. I pulsanti
	// inviano semplicemente gli stessi messaggi gia' gestiti dai
	// menu, nessuna logica nuova.
	BButton* newButton = new BButton("toolNew", "Nuovo", new BMessage(kMsgNew));
	BButton* openButton = new BButton("toolOpen", "Apri", new BMessage(kMsgOpen));
	BButton* saveButton = new BButton("toolSave", "Salva", new BMessage(kMsgSaveAs));
	BButton* printButton = new BButton("toolPrint", "Stampa", new BMessage(kMsgPrint));
	BButton* cutButton = new BButton("toolCut", "Taglia", new BMessage(kMsgCut));
	BButton* copyButton = new BButton("toolCopy", "Copia", new BMessage(kMsgCopy));
	BButton* pasteButton = new BButton("toolPaste", "Incolla", new BMessage(kMsgPaste));
	BButton* findButton = new BButton("toolFind", "Trova", new BMessage(kMsgFind));
	newButton->SetTarget(this);
	openButton->SetTarget(this);
	saveButton->SetTarget(this);
	printButton->SetTarget(this);
	cutButton->SetTarget(this);
	copyButton->SetTarget(this);
	pasteButton->SetTarget(this);
	findButton->SetTarget(this);

	fCellLabel = new BStringView("cellLabel", "A1");
	fCellLabel->SetExplicitMinSize(BSize(50, B_SIZE_UNSET));
	fCellLabel->SetExplicitMaxSize(BSize(50, B_SIZE_UNSET));
	fCellLabel->SetAlignment(B_ALIGN_CENTER);

	fFormulaBar = new BTextControl("formula", NULL, "", new BMessage(kMsgFormulaCommit));
	fFormulaBar->SetTarget(this);

	fSheetView = new SheetView(fDoc);
	fSheetView->SetCharts(&fCharts);
	BScrollView* scroll = new BScrollView("scroll", fSheetView,
		B_FOLLOW_ALL, 0, true, true);

	// BScrollView, costruita con questa forma classica, eredita di
	// default la dimensione (enorme: il canvas virtuale del foglio,
	// vedi SheetView::FullCanvasFrame) del suo target, invece di farsi
	// vincolare dal layout -- un ResizeTo() esplicito subito dopo la
	// costruzione la "sgancia" da quella dimensione ereditata, cosi'
	// il layout puo' poi ridimensionarla liberamente in base allo
	// spazio disponibile nella finestra. La dimensione qui non conta
	// molto (il layout la corregge subito al primo giro), serve solo
	// a rompere l'eredita' iniziale dal target.
	scroll->ResizeTo(400, 300);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(4, 4, 4, 4)
			.Add(newButton)
			.Add(openButton)
			.Add(saveButton)
			.Add(printButton)
			.Add(cutButton)
			.Add(copyButton)
			.Add(pasteButton)
			.Add(findButton)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(4, 4, 4, 4)
			.Add(fCellLabel)
			.Add(fFormulaBar)
		.End()
		.Add(scroll);

	fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this));
	fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this));
	fFindWindow = NULL;
	fChartWindow = NULL;
	fPivotWindow = NULL;
}

MainWindow::~MainWindow()
{
	delete fOpenPanel;
	delete fSavePanel;
	if (fFindWindow)
	{
		// FindWindow::QuitRequested() la nasconde soltanto (per
		// restare riusabile finche' l'app e' viva): qui invece va
		// davvero distrutta, quindi si chiama Quit() direttamente
		// (non tramite il messaggio B_QUIT_REQUESTED che passerebbe
		// da quell'hook).
		fFindWindow->Lock();
		fFindWindow->Quit();
	}
	if (fChartWindow)
	{
		fChartWindow->Lock();
		fChartWindow->Quit();
	}
	if (fPivotWindow)
	{
		fPivotWindow->Lock();
		fPivotWindow->Quit();
	}
	if (fDoc)
		fDoc->Release();
}

void MainWindow::NewDocument()
{
	CContainer* newDoc = new CContainer(NULL, NULL);
	if (fDoc)
		fDoc->Release();
	fDoc = newDoc;
	fCharts.clear();
	fSheetView->SetDocument(fDoc);
	fFormulaBar->SetText("");
}

void MainWindow::OpenFile(const entry_ref& ref)
{
	BFile file(&ref, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
	{
		BAlert* alert = new BAlert("Errore",
			"Impossibile aprire il file selezionato.", "OK");
		alert->Go();
		return;
	}

	CContainer* newDoc = new CContainer(NULL, NULL);
	std::vector<ChartObject> newCharts;
	status_t err;

	if (IsASCDFile(&file))
	{
		// File gia' nel formato nativo: si legge direttamente, senza
		// passare dal Translation Kit -- che per un file gia' ASCD lo
		// farebbe comunque rileggere/riscrivere tramite la copia
		// duplicata di ReadASCD/WriteASCD di un translator qualunque
		// (vedi translators/csv/CsvTranslator.cpp), perdendo la
		// sezione dei grafici incorporati che quella copia non
		// conosce.
		err = LoadASCD(&file, newDoc, &newCharts);
	}
	else
	{
		// BTranslatorRoster sceglie automaticamente il translator
		// installato adatto (CSV/XLS/XLSX/ODS) in base al contenuto
		// reale del file, non all'estensione. Nessuno di questi
		// formati porta grafici incorporati: newCharts resta vuoto.
		BMallocIO ascd;
		err = BTranslatorRoster::Default()->Translate(&file, NULL, NULL,
			&ascd, kAtomoNativeFormat);
		if (err != B_OK)
		{
			newDoc->Release();
			BAlert* alert = new BAlert("Errore",
				"Formato file non riconosciuto da nessun translator installato.",
				"OK");
			alert->Go();
			return;
		}

		ascd.Seek(0, SEEK_SET);
		err = LoadASCD(&ascd, newDoc);
	}

	if (err != B_OK)
	{
		newDoc->Release();
		BAlert* alert = new BAlert("Errore",
			"Il file e' stato tradotto ma i dati risultanti non sono validi.",
			"OK");
		alert->Go();
		return;
	}

	fCharts = newCharts;
	if (fDoc)
		fDoc->Release();
	fDoc = newDoc;
	fSheetView->SetDocument(fDoc);
}

void MainWindow::SaveToFile(const entry_ref& dir, const char* name)
{
	// Il formato di destinazione si sceglie dall'estensione del nome
	// scelto nel BFilePanel (".csv" esporta in CSV, qualunque altra
	// estensione o nessuna resta sul formato nativo ASCD) -- non c'e'
	// ancora un selettore di formato dedicato nel pannello di salvataggio.
	BString nameStr(name);
	uint32 outType = kAtomoNativeFormat;
	int32 csvPos = nameStr.IFindLast(".csv");
	if (csvPos >= 0 && csvPos == nameStr.Length() - 4)
		outType = kAtomoCsvFormat;

	BDirectory directory(&dir);
	BFile file(&directory, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
	{
		BAlert* alert = new BAlert("Errore", "Impossibile creare il file.", "OK");
		alert->Go();
		return;
	}

	if (outType == kAtomoNativeFormat)
	{
		status_t err = SaveASCD(fDoc, &file, &fCharts);
		if (err != B_OK)
		{
			BAlert* alert = new BAlert("Errore", "Scrittura del file fallita.", "OK");
			alert->Go();
		}
		return;
	}

	// Per qualunque formato non nativo si passa dal Translation Kit:
	// si serializza prima il documento in ASCD in memoria, poi si
	// lascia che BTranslatorRoster trovi un translator installato che
	// sappia leggere ASCD e scrivere il formato scelto (il translator
	// CSV lo fa gia' in entrambe le direzioni; XLS/XLSX/ODS per ora
	// importano soltanto, quindi qui la Translate fallisce per loro
	// finche' non avranno anche un writer).
	BMallocIO ascd;
	status_t err = SaveASCD(fDoc, &ascd);
	if (err != B_OK)
	{
		BAlert* alert = new BAlert("Errore", "Serializzazione del documento fallita.", "OK");
		alert->Go();
		return;
	}

	ascd.Seek(0, SEEK_SET);
	err = BTranslatorRoster::Default()->Translate(&ascd, NULL, NULL, &file, outType);
	if (err != B_OK)
	{
		BAlert* alert = new BAlert("Errore",
			"Nessun translator installato sa esportare in questo formato.", "OK");
		alert->Go();
	}
}

void MainWindow::CopySelection(bool cut)
{
	if (!fDoc)
		return;

	cell sel = fSheetView->Selection();
	char text[512];
	fDoc->GetCellFormula(sel, text, false);

	if (be_clipboard->Lock())
	{
		be_clipboard->Clear();
		BMessage* clip = be_clipboard->Data();
		if (clip)
			clip->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
		be_clipboard->Commit();
		be_clipboard->Unlock();
	}

	if (cut)
	{
		fDoc->DisposeCell(sel);
		RecalculateAll(fDoc);
		fSheetView->Invalidate();
		SelectionChanged(sel);
	}
}

void MainWindow::PasteSelection()
{
	if (!fDoc)
		return;

	cell sel = fSheetView->Selection();

	if (!be_clipboard->Lock())
		return;

	BMessage* clip = be_clipboard->Data();
	const char* text = NULL;
	ssize_t len = 0;
	bool found = clip && clip->FindData("text/plain", B_MIME_TYPE,
		(const void**)&text, &len) == B_OK;

	if (found)
	{
		BString pasted(text, len);
		try
		{
			TryToParseString(pasted.String(), sel, fDoc, true);
		}
		catch (...)
		{
		}
		RecalculateAll(fDoc);
		fSheetView->Invalidate();
		SelectionChanged(sel);
	}

	be_clipboard->Unlock();
}

void MainWindow::DeleteSelection()
{
	if (!fDoc)
		return;

	// Cancella tutte le celle nell'intervallo selezionato (non solo la
	// cella attiva) e ricalcola gia' da sola.
	fSheetView->ClearSelection();
	SelectionChanged(fSheetView->Selection());
}

void MainWindow::ShowFindWindow()
{
	if (!fFindWindow)
		fFindWindow = new FindWindow(BMessenger(this));

	if (fFindWindow->IsHidden())
		fFindWindow->Show();
	fFindWindow->Activate();
}

void MainWindow::ShowChartWindow()
{
	if (!fChartWindow)
		fChartWindow = new ChartWindow(BMessenger(this));

	if (fChartWindow->IsHidden())
		fChartWindow->Show();
	fChartWindow->Activate();
}

void MainWindow::ShowPivotWindow()
{
	if (!fPivotWindow)
		fPivotWindow = new PivotWindow(BMessenger(this));

	if (fPivotWindow->IsHidden())
		fPivotWindow->Show();
	fPivotWindow->Activate();
}

// Legge l'intervallo dati richiesto da ChartWindow e manda indietro i
// dati estratti via BMessage: sia la lettura del documento sia la
// costruzione del messaggio di risposta girano sul thread di
// MainWindow (che possiede fDoc), mai su quello di ChartWindow.
void MainWindow::HandleChartRequest(const char* rangeText)
{
	if (!fDoc || !fChartWindow)
		return;

	range r;
	std::vector<ChartSeries> series;
	if (!ParseRangeRef(rangeText, r) || !BuildChartSeries(fDoc, r, series))
	{
		BAlert* alert = new BAlert("Grafico",
			"Intervallo non valido: serve esattamente due colonne "
			"(etichette, valori) con almeno una riga numerica, es. A1:B5.",
			"OK");
		alert->Go();
		return;
	}

	BMessage data(kMsgChartData);
	for (size_t i = 0; i < series.size(); i++)
	{
		data.AddString("label", series[i].label);
		data.AddDouble("value", series[i].value);
	}
	BMessenger(fChartWindow).SendMessage(&data);
}

// A differenza di HandleChartRequest (sola anteprima in ChartWindow),
// qui il grafico entra a far parte del documento: aggiunto a fCharts
// e disegnato da SheetView (vedi Chart.h/ChartObject) da questo
// momento in poi, sopravvive al salvataggio/ricaricamento nel
// formato nativo (vedi AscdIO.cpp) e legge i dati dal vivo ogni volta
// che si ridisegna, non un'istantanea fissa come l'anteprima.
void MainWindow::HandleChartInsert(const char* rangeText, const char* destText)
{
	if (!fDoc)
		return;

	range dataRange;
	std::vector<ChartSeries> series;
	cell dest;
	if (!ParseRangeRef(rangeText, dataRange) || !BuildChartSeries(fDoc, dataRange, series)
		|| !cell::GetCell(destText, dest))
	{
		BAlert* alert = new BAlert("Grafico",
			"Intervallo dati o cella di destinazione non validi: l'intervallo "
			"deve avere due colonne (etichette, valori) con almeno una riga "
			"numerica, es. A1:B5.", "OK");
		alert->Go();
		return;
	}

	// Dimensione fissa di default (non ridimensionabile in questa
	// prima versione): abbastanza per leggere etichette/barre senza
	// essere invasiva sulla griglia.
	BPoint origin = fSheetView->CellOrigin(dest);
	ChartObject obj;
	obj.dataRange = dataRange;
	obj.frame.Set(origin.x, origin.y, origin.x + 300, origin.y + 180);
	fCharts.push_back(obj);

	fSheetView->Invalidate();
}

// Come sopra: legge/scrive fDoc sul thread di MainWindow, poi
// aggiorna direttamente la griglia (stesso thread, sicuro).
void MainWindow::HandlePivotRequest(const char* sourceText, const char* destText,
	int32 agg)
{
	if (!fDoc)
		return;

	range source;
	cell dest;
	if (!ParseRangeRef(sourceText, source) || !cell::GetCell(destText, dest))
	{
		BAlert* alert = new BAlert("Tabella pivot",
			"Intervallo dati o cella di destinazione non validi.", "OK");
		alert->Go();
		return;
	}

	std::vector<PivotRow> rows;
	if (!BuildPivotTable(fDoc, source, rows))
	{
		BAlert* alert = new BAlert("Tabella pivot",
			"Nessun dato valido nell'intervallo (servono due colonne: "
			"categoria testuale, valore numerico).", "OK");
		alert->Go();
		return;
	}

	// La destinazione (intestazioni + una riga per categoria) non deve
	// sovrapporsi ai dati sorgente, altrimenti la scrittura riga per
	// riga li corromperebbe mentre li si sta ancora leggendo.
	range destRange(dest.h, dest.v, dest.h + 1, dest.v + (int)rows.size());
	if (destRange.left <= source.right && destRange.right >= source.left
		&& destRange.top <= source.bottom && destRange.bottom >= source.top)
	{
		BAlert* alert = new BAlert("Tabella pivot",
			"La cella di destinazione si sovrappone all'intervallo dati: "
			"scegline una fuori dai dati sorgente.", "OK");
		alert->Go();
		return;
	}

	WritePivotTable(fDoc, dest, rows, (PivotAggFunc)agg);
	fSheetView->Invalidate();

	BString msg;
	msg << (int32)rows.size() << " categoria/e trovate.";
	BAlert* alert = new BAlert("Tabella pivot", msg.String(), "OK");
	alert->Go();
}

void MainWindow::FindNext(const char* searchText)
{
	if (!fDoc || !searchText || !searchText[0])
		return;

	BString needle(searchText);
	needle.ToLower();

	range bounds;
	fDoc->GetBounds(bounds);
	if (bounds.right < 1 || bounds.bottom < 1)
		return;

	cell start = fSheetView->Selection();

	// Scansione completa a ogni ricerca (nessun iteratore persistito
	// fra una chiamata e l'altra): per le dimensioni di foglio di
	// questa prima versione dell'app e' un compromesso semplice e
	// robusto, senza il rischio di un iteratore invalidato da una
	// modifica del documento fra due "Trova successivo".
	CCellIterator iter(fDoc, &bounds);
	cell c;
	cell wrapMatch(0, 0);
	bool haveWrapMatch = false;
	cell nextMatch(0, 0);
	bool haveNextMatch = false;

	while (iter.NextExisting(c))
	{
		char text[512];
		fDoc->GetCellFormula(c, text, false);

		BString hay(text);
		hay.ToLower();
		if (hay.FindFirst(needle) < 0)
			continue;

		if (!haveWrapMatch)
		{
			wrapMatch = c;
			haveWrapMatch = true;
		}

		bool after = (c.v > start.v) || (c.v == start.v && c.h > start.h);
		if (after && !haveNextMatch)
		{
			nextMatch = c;
			haveNextMatch = true;
		}
	}

	if (haveNextMatch)
		fSheetView->SetSelection(nextMatch);
	else if (haveWrapMatch)
		fSheetView->SetSelection(wrapMatch);
	// Nessun risultato: nessuna azione, niente BAlert invasivo per
	// ogni ricerca senza esito (coerente con un dialogo "Trova" che
	// resta aperto per piu' tentativi).
}

// Sostituisce tutte le occorrenze di "search" con "replace" dentro
// "text" (senza distinguere maiuscole/minuscole nella ricerca, ma
// inserendo "replace" cosi' come scritto, non nella capitalizzazione
// del testo originale).
static BString ReplaceAllCaseInsensitive(const BString& text,
	const BString& search, const BString& replace)
{
	BString result(text);
	if (search.Length() == 0)
		return result;

	int32 pos = 0;
	while (true)
	{
		int32 found = result.IFindFirst(search, pos);
		if (found < 0)
			break;
		result.Remove(found, search.Length());
		result.Insert(replace, found);
		pos = found + replace.Length();
	}
	return result;
}

void MainWindow::ReplaceCurrent(const char* searchText, const char* replaceText)
{
	if (!fDoc || !searchText || !searchText[0])
		return;

	cell sel = fSheetView->Selection();
	char text[512];
	fDoc->GetCellFormula(sel, text, false);

	BString original(text);
	if (original.IFindFirst(searchText) < 0)
		return; // la cella selezionata non contiene il testo cercato

	BString replaced = ReplaceAllCaseInsensitive(original, searchText, replaceText);

	try
	{
		TryToParseString(replaced.String(), sel, fDoc, true);
	}
	catch (...)
	{
	}
	RecalculateAll(fDoc);
	fSheetView->Invalidate();
	SelectionChanged(sel);

	FindNext(searchText);
}

void MainWindow::ReplaceAll(const char* searchText, const char* replaceText)
{
	if (!fDoc || !searchText || !searchText[0])
		return;

	range bounds;
	fDoc->GetBounds(bounds);
	if (bounds.right < 1 || bounds.bottom < 1)
		return;

	// Prima si raccolgono le celle da modificare, poi si modificano:
	// CCellIterator scorre la mappa interna del documento, che non va
	// alterata (TryToParseString puo' aggiungere/rimuovere celle)
	// mentre la si sta iterando.
	std::vector<cell> matches;
	CCellIterator iter(fDoc, &bounds);
	cell c;
	while (iter.NextExisting(c))
	{
		char text[512];
		fDoc->GetCellFormula(c, text, false);
		if (BString(text).IFindFirst(searchText) >= 0)
			matches.push_back(c);
	}

	for (size_t i = 0; i < matches.size(); i++)
	{
		char text[512];
		fDoc->GetCellFormula(matches[i], text, false);
		BString replaced = ReplaceAllCaseInsensitive(text, searchText, replaceText);
		try
		{
			TryToParseString(replaced.String(), matches[i], fDoc, true);
		}
		catch (...)
		{
		}
	}

	// Un solo ricalcolo completo dopo tutte le sostituzioni, non uno
	// per cella modificata: piu' efficiente ed equivalente (vedi
	// AscdIO.h per il perche' non basta CalcCell sulla sola cella
	// toccata).
	RecalculateAll(fDoc);
	fSheetView->Invalidate();
	SelectionChanged(fSheetView->Selection());

	BString msg;
	msg << (int32)matches.size() << " cella/e sostituita/e.";
	BAlert* alert = new BAlert("Sostituisci tutto", msg.String(), "OK");
	alert->Go();
}

void MainWindow::SetCellFormat(int32 format)
{
	if (!fDoc)
		return;

	cell sel = fSheetView->Selection();
	CellStyle cs;
	fDoc->GetCellStyle(sel, cs);
	cs.fFormat = format;
	fDoc->SetCellStyle(sel, cs);
	fSheetView->Invalidate();
}

void MainWindow::PrintDocument()
{
	if (!fDoc)
		return;

	BPrintJob printJob("Atomo123");

	// ConfigJob mostra il dialogo di stampa di sistema (scelta
	// stampante/opzioni): se l'utente annulla, o non c'e' nessuna
	// stampante configurata, restituisce un errore e non si stampa
	// nulla.
	if (printJob.ConfigJob() != B_OK)
		return;

	printJob.BeginJob();

	BRect printableRect = printJob.PrintableRect();
	BRect contentRect = fSheetView->ContentRect();
	float pageWidth = printableRect.Width();
	float pageHeight = printableRect.Height();

	if (pageWidth <= 0 || pageHeight <= 0)
	{
		printJob.CancelJob();
		return;
	}

	// Si stampa solo l'area del foglio che contiene dati
	// (SheetView::ContentRect), suddivisa in tante pagine quante ne
	// servono in base all'area stampabile della stampante scelta —
	// non l'intero intervallo virtuale del motore (702x16384 celle).
	// Limite noto: le intestazioni di riga/colonna, disegnate da
	// SheetView::Draw solo nella banda 0-kHeaderWidth/kHeaderHeight,
	// compaiono quindi solo sulla prima pagina (in alto a sinistra),
	// non ripetute su ogni pagina.
	for (float y = 0; y <= contentRect.bottom && printJob.CanContinue(); y += pageHeight)
	{
		for (float x = 0; x <= contentRect.right && printJob.CanContinue(); x += pageWidth)
		{
			BRect pageSlice(x, y, x + pageWidth, y + pageHeight);
			printJob.DrawView(fSheetView, pageSlice, BPoint(0, 0));
			printJob.SpoolPage();
		}
	}

	if (printJob.CanContinue())
		printJob.CommitJob();
	else
		printJob.CancelJob();
}

void MainWindow::CommitFormulaBar()
{
	if (!fDoc)
		return;

	cell sel = fSheetView->Selection();
	const char* text = fFormulaBar->Text();

	try
	{
		TryToParseString(text, sel, fDoc, true);
	}
	catch (...)
	{
	}
	RecalculateAll(fDoc);
	fSheetView->Invalidate();
}

void MainWindow::SelectionChanged(cell c)
{
	char name[32];
	ColumnName(c.h, name);
	int len = strlen(name);
	snprintf(name + len, sizeof(name) - len, "%d", c.v);

	// Con piu' di una cella selezionata (trascinamento, Maiusc+frecce,
	// Ctrl+A) l'indirizzo mostrato diventa "A1:B5" (angolo in alto a
	// sinistra : angolo in basso a destra, sempre in quest'ordine
	// indipendentemente da quale angolo sia la cella attiva) invece
	// della sola cella attiva -- come Excel/LibreOffice Calc, cosi' si
	// vede a colpo d'occhio quante celle sono selezionate.
	range sel = fSheetView->SelectionRange();
	if (sel.left != sel.right || sel.top != sel.bottom)
	{
		char topLeft[16], botRight[16];
		ColumnName(sel.left, topLeft);
		len = strlen(topLeft);
		snprintf(topLeft + len, sizeof(topLeft) - len, "%d", sel.top);

		ColumnName(sel.right, botRight);
		len = strlen(botRight);
		snprintf(botRight + len, sizeof(botRight) - len, "%d", sel.bottom);

		snprintf(name, sizeof(name), "%s:%s", topLeft, botRight);
	}
	fCellLabel->SetText(name);

	char formula[512];
	if (fDoc)
		fDoc->GetCellFormula(c, formula, false);
	else
		formula[0] = 0;
	fFormulaBar->SetText(formula);
}

void MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgNew:
			NewDocument();
			break;

		case kMsgOpen:
			fOpenPanel->Show();
			break;

		case kMsgSaveAs:
			fSavePanel->Show();
			break;

		case B_REFS_RECEIVED:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				OpenFile(ref);
			break;
		}

		case B_SAVE_REQUESTED:
		{
			entry_ref dir;
			BString name;
			if (message->FindRef("directory", &dir) == B_OK
				&& message->FindString("name", &name) == B_OK)
				SaveToFile(dir, name.String());
			break;
		}

		case kMsgFormulaCommit:
			CommitFormulaBar();
			break;

		case kMsgCut:
			CopySelection(true);
			break;

		case kMsgCopy:
			CopySelection(false);
			break;

		case kMsgPaste:
			PasteSelection();
			break;

		case kMsgClear:
			DeleteSelection();
			break;

		case kMsgSelectAll:
			fSheetView->SelectAll();
			break;

		case kMsgFillDown:
			fSheetView->FillDown();
			break;

		case kMsgFillRight:
			fSheetView->FillRight();
			break;

		case kMsgPrint:
			PrintDocument();
			break;

		case kMsgFind:
			ShowFindWindow();
			break;

		case kMsgFindNext:
		{
			BString text;
			if (message->FindString("text", &text) == B_OK)
				FindNext(text.String());
			break;
		}

		case kMsgReplaceCurrent:
		{
			BString text, replace;
			if (message->FindString("text", &text) == B_OK
				&& message->FindString("replace", &replace) == B_OK)
				ReplaceCurrent(text.String(), replace.String());
			break;
		}

		case kMsgReplaceAll:
		{
			BString text, replace;
			if (message->FindString("text", &text) == B_OK
				&& message->FindString("replace", &replace) == B_OK)
				ReplaceAll(text.String(), replace.String());
			break;
		}

		case kMsgSetFormat:
		{
			int32 format;
			if (message->FindInt32("format", &format) == B_OK)
				SetCellFormat(format);
			break;
		}

		case kMsgShowChart:
			ShowChartWindow();
			break;

		case kMsgShowPivot:
			ShowPivotWindow();
			break;

		case kMsgChartRequest:
		{
			BString rangeText;
			if (message->FindString("range", &rangeText) == B_OK)
				HandleChartRequest(rangeText.String());
			break;
		}

		case kMsgChartInsert:
		{
			BString rangeText, destText;
			if (message->FindString("range", &rangeText) == B_OK
				&& message->FindString("dest", &destText) == B_OK)
				HandleChartInsert(rangeText.String(), destText.String());
			break;
		}

		case kMsgPivotRequest:
		{
			BString sourceText, destText;
			int32 agg;
			if (message->FindString("source", &sourceText) == B_OK
				&& message->FindString("dest", &destText) == B_OK
				&& message->FindInt32("agg", &agg) == B_OK)
				HandlePivotRequest(sourceText.String(), destText.String(), agg);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

bool MainWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}
