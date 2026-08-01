/*
	MainWindow.cpp

	Vedi MainWindow.h.
*/

#include "MainWindow.h"
#include "SheetView.h"
#include "SheetTabView.h"
#include "AscdIO.h"
#include "FindWindow.h"
#include "ChartWindow.h"
#include "PivotWindow.h"
#include "NameWindow.h"
#include "PasteSpecialWindow.h"
#include "GoToWindow.h"
#include "ColorWindow.h"
#include "PreferencesWindow.h"
#include "Chart.h"
#include "Pivot.h"
#include "RangeRef.h"
#include "IconCatalog.h"
#include "NameTable.h"
#include "Utils.h"
#include "FontMetrics.h"
#include "CellStyle.h"
#include "Preferences.h"
#include "MyError.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Clipboard.h>
#include <Directory.h>
#include <File.h>
#include <FilePanel.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <PrintJob.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>
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
static const uint32 kMsgUndo = 'aund';
static const uint32 kMsgRedo = 'ared';
static const uint32 kMsgCut = 'acut';
static const uint32 kMsgCopy = 'acpy';
static const uint32 kMsgPaste = 'apst';
static const uint32 kMsgClear = 'aclr';
static const uint32 kMsgSelectAll = 'asla';
static const uint32 kMsgFillDown = 'afdn';
static const uint32 kMsgFillRight = 'afrt';
static const uint32 kMsgSortAscending = 'asta';
static const uint32 kMsgSortDescending = 'astd';
static const uint32 kMsgInsertRows = 'ainr';
static const uint32 kMsgInsertColumns = 'ainc';
static const uint32 kMsgDeleteRows = 'adlr';
static const uint32 kMsgDeleteColumns = 'adlc';
static const uint32 kMsgPrint = 'aprt';
static const uint32 kMsgFind = 'afnd';
static const uint32 kMsgSwitchSheet = 'swsh';
static const uint32 kMsgSetFormat = 'stfm';
static const uint32 kMsgShowChart = 'shch';
static const uint32 kMsgShowPivot = 'shpv';
static const uint32 kMsgShowNames = 'shnm';
static const uint32 kMsgShowPasteSpecial = 'shps';
static const uint32 kMsgShowGoTo = 'shgt';
static const uint32 kMsgToggleFreeze = 'frzp';
static const uint32 kMsgToggleBold = 'tbld';
static const uint32 kMsgToggleItalic = 'tita';
static const uint32 kMsgSetAlignment = 'algn';
static const uint32 kMsgShowTextColor = 'shtc';
static const uint32 kMsgShowBgColor = 'shbc';
static const uint32 kMsgShowPreferences = 'shpr';
static const uint32 kMsgToggleBorder = 'tbrd';
static const uint32 kMsgClearBorders = 'cbrd';

static const uint32 kAtomoNativeFormat = 'ASCD';
static const uint32 kAtomoCsvFormat = 'ACSV';

// Toolbar dinamica: i pulsanti vengono generati da questa tabella
// invece che scritti uno per uno a mano (com'era prima di questo
// gruppo di modifiche, quando l'unica fonte di icone erano gli otto
// glifi disegnati a codice in ToolbarIcons.h/.cpp -- rimossi ora che
// il sito autorizzato per le icone del progetto, www.hvif-store.art,
// e' finalmente popolato, vedi Atomo123_icons/ATOMO123.md per la
// selezione ragionata e IconCatalog.h/IconData.cpp per i byte HVIF
// incorporati). Un gruppo per voce di menu principale a cui i pulsanti
// corrispondono (File/Modifica/Dati/Inserisci), con un separatore
// verticale fra un gruppo e il successivo in un'unica riga -- una
// vera BToolBar per riga/categoria non e' possibile (quella classe
// vive solo sotto develop/headers/private/shared/ su questo sistema,
// vedi il commento gia' presente piu' sotto sui BButton), ma questo
// raggruppamento visivo riprende lo stesso principio della barra
// Standard di Excel classico (icone imparentate raggruppate, separate
// da un divisore sottile) chiesto dall'utente. Solo funzioni gia'
// implementate da un comando vero (menu o scorciatoia): niente
// pulsanti per funzioni ancora "da disegnare" nel catalogo o non
// ancora presenti in Atomo123 (formattazione testo, filtro, zoom...).
struct ToolbarButtonDef {
	const char* name;
	const char* label;
	uint32 message;
	const IconData* icon;
};

struct ToolbarGroupDef {
	const ToolbarButtonDef* buttons;
	size_t count;
};

static const ToolbarButtonDef kFileToolbarButtons[] = {
	{ "toolNew", "Nuovo", kMsgNew, &kIconNew },
	{ "toolOpen", "Apri", kMsgOpen, &kIconOpen },
	{ "toolSave", "Salva", kMsgSaveAs, &kIconSave },
	{ "toolPrint", "Stampa", kMsgPrint, &kIconPrint },
};

static const ToolbarButtonDef kEditToolbarButtons[] = {
	{ "toolUndo", "Annulla", kMsgUndo, &kIconUndo },
	{ "toolRedo", "Ripeti", kMsgRedo, &kIconRedo },
	{ "toolCut", "Taglia", kMsgCut, &kIconCut },
	{ "toolCopy", "Copia", kMsgCopy, &kIconCopy },
	{ "toolPaste", "Incolla", kMsgPaste, &kIconPaste },
	{ "toolDelete", "Elimina", kMsgClear, &kIconDelete },
	{ "toolFind", "Trova", kMsgFind, &kIconFind },
};

static const ToolbarButtonDef kDataToolbarButtons[] = {
	{ "toolSortAsc", "Ordina A-Z", kMsgSortAscending, &kIconSortAscending },
	{ "toolSortDesc", "Ordina Z-A", kMsgSortDescending, &kIconSortDescending },
};

static const ToolbarButtonDef kInsertToolbarButtons[] = {
	{ "toolChart", "Grafico", kMsgShowChart, &kIconChart },
	{ "toolPivot", "Pivot", kMsgShowPivot, &kIconTable },
};

#define TOOLBAR_GROUP(buttons) { buttons, sizeof(buttons) / sizeof((buttons)[0]) }

static const ToolbarGroupDef kToolbarGroups[] = {
	TOOLBAR_GROUP(kFileToolbarButtons),
	TOOLBAR_GROUP(kEditToolbarButtons),
	TOOLBAR_GROUP(kDataToolbarButtons),
	TOOLBAR_GROUP(kInsertToolbarButtons),
};

#undef TOOLBAR_GROUP

// Costruisce l'intera riga della toolbar dalla tabella sopra: un
// BButton per voce, con la sua icona HVIF (IconCatalog::Render --
// SetIcon ne copia i bit al suo interno, quindi il BBitmap temporaneo
// va eliminato subito dopo, altrimenti perde solo memoria senza
// benefici), e un separatore verticale fra un gruppo e il successivo.
// "target" riceve i messaggi di tutti i pulsanti (sempre "this" per
// MainWindow, passato esplicitamente solo per non legare questa
// funzione libera a una particolare istanza).
static BView* BuildToolbar(BHandler* target)
{
	BGroupView* row = new BGroupView(B_HORIZONTAL, 4);
	row->GroupLayout()->SetInsets(4, 4, 4, 4);

	size_t groupCount = sizeof(kToolbarGroups) / sizeof(kToolbarGroups[0]);
	for (size_t g = 0; g < groupCount; g++)
	{
		if (g > 0)
			row->AddChild(new BSeparatorView(B_VERTICAL));

		const ToolbarGroupDef& group = kToolbarGroups[g];
		for (size_t i = 0; i < group.count; i++)
		{
			const ToolbarButtonDef& def = group.buttons[i];
			// Nessuna etichetta visibile sul pulsante (solo l'icona):
			// il testo compare come tooltip al passaggio del mouse
			// (SetToolTip), risparmiando spazio orizzontale invece di
			// tenerlo sempre disegnato accanto a ogni icona -- chiesto
			// dall'utente dopo aver visto la toolbar coi soli quattro
			// pulsanti File gia' quasi al limite della larghezza
			// predefinita della finestra.
			BButton* button = new BButton(def.name, NULL, new BMessage(def.message));
			button->SetToolTip(def.label);
			button->SetTarget(target);

			BBitmap* icon = IconCatalog::Render(*def.icon);
			if (icon)
			{
				button->SetIcon(icon);
				delete icon;
			}

			row->AddChild(button);
		}
	}

	row->GroupLayout()->AddItem(BSpaceLayoutItem::CreateGlue());
	return row;
}

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
	// Niente B_QUIT_ON_WINDOW_CLOSE: con piu' finestre possibili (vedi
	// App.h/App.cpp) chiudere QUESTA finestra non deve terminare l'intera
	// applicazione se ce ne sono altre aperte -- ci pensa esplicitamente
	// QuitRequested() sotto, solo quando e' rimasta l'ultima.
	BWindow(BRect(80, 80, 900, 700), "Atomo123", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS)
{
	// fSheetView/fSheetTabView vanno azzerati ESPLICITAMENTE prima di
	// ResetWorkbook() qui sotto: sono puntatori membro senza un
	// inizializzatore nella lista sopra, quindi contengono spazzatura
	// indeterminata finche' non vengono creati piu' avanti in questo
	// costruttore -- ResetWorkbook() li controlla con "if (fSheetView)"
	// prima di usarli (per essere richiamabile anche piu' tardi, es. da
	// NewDocument(), quando esistono gia' entrambi), ma letta PRIMA di
	// questo azzeramento quella condizione varrebbe su un puntatore a
	// caso, non su "non ancora creato" -- bug reale scoperto scrivendo
	// tests/test_multisheet.cpp: un puntatore a caso che capita non-NULL
	// mandava SetDocument/SetCharts su un oggetto inesistente, con esito
	// imprevedibile (a volte un blocco indefinito, non un crash pulito).
	fSheetView = NULL;
	fSheetTabView = NULL;
	fFreezeMenuItem = NULL; // stesso motivo di fSheetView/fSheetTabView sopra
	fActiveSheetIndex = -1; // ResetWorkbook() sotto lo imposta a 0
	ResetWorkbook("Foglio1");
	fModified = false;

	BMenuBar* menuBar = new BMenuBar("menu");
	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("Nuovo", new BMessage(kMsgNew), 'N'));
	fileMenu->AddItem(new BMenuItem("Apri" B_UTF8_ELLIPSIS, new BMessage(kMsgOpen), 'O'));
	fileMenu->AddItem(new BMenuItem("Salva con nome" B_UTF8_ELLIPSIS,
		new BMessage(kMsgSaveAs), 'S'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Stampa" B_UTF8_ELLIPSIS, new BMessage(kMsgPrint), 'P'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Preferenze" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowPreferences)));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Esci", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(fileMenu);

	// Taglia/copia/incolla passano dal vero BClipboard di sistema (non
	// da un appunti interno all'app): il contenuto della cella (la
	// formula, come mostrata dalla barra formule) diventa testo piano
	// condivisibile anche con altre applicazioni Haiku.
	BMenu* editMenu = new BMenu("Modifica");
	// Ctrl+Z/Ctrl+Y come scorciatoie dirette (non solo di menu): a
	// differenza di Ctrl+A/Ctrl+D/Ctrl+End sopra, questi due byte
	// (0x1a e 0x19) non corrispondono a nessun altro tasto speciale
	// gia' gestito da SheetView::HandleKey, quindi non c'e' ambiguita'
	// da aggirare (vedi InterfaceDefs.h: B_SUBSTITUTE = 0x1a per
	// Ctrl+Z, nessun nome dedicato per Ctrl+Y). La scorciatoia di menu
	// basta comunque a farli funzionare, risolta dal BWindow.
	editMenu->AddItem(new BMenuItem("Annulla", new BMessage(kMsgUndo), 'Z'));
	editMenu->AddItem(new BMenuItem("Ripeti", new BMessage(kMsgRedo), 'Y'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Taglia", new BMessage(kMsgCut), 'X'));
	editMenu->AddItem(new BMenuItem("Copia", new BMessage(kMsgCopy), 'C'));
	editMenu->AddItem(new BMenuItem("Incolla", new BMessage(kMsgPaste), 'V'));
	editMenu->AddItem(new BMenuItem("Incolla speciale" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowPasteSpecial)));
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
	editMenu->AddItem(new BMenuItem("Vai a" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowGoTo), 'G'));
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
	formatMenu->AddSeparatorItem();
	// Grassetto/Corsivo: agiscono su CellStyle::fFont (un indice in
	// gFontSizeTable, mai un flag a parte -- vedi MainWindow::ToggleBold/
	// ToggleItalic). Allinea: CellStyle::fAlignment, letto da
	// SheetView::Draw per posizionare il testo dentro la cella.
	formatMenu->AddItem(new BMenuItem("Grassetto", new BMessage(kMsgToggleBold), 'B'));
	formatMenu->AddItem(new BMenuItem("Corsivo", new BMessage(kMsgToggleItalic), 'I'));
	formatMenu->AddSeparatorItem();
	BMessage* alignLeftMsg = new BMessage(kMsgSetAlignment);
	alignLeftMsg->AddInt32("alignment", eAlignLeft);
	formatMenu->AddItem(new BMenuItem("Allinea a sinistra", alignLeftMsg));
	BMessage* alignCenterMsg = new BMessage(kMsgSetAlignment);
	alignCenterMsg->AddInt32("alignment", eAlignCenter);
	formatMenu->AddItem(new BMenuItem("Allinea al centro", alignCenterMsg));
	BMessage* alignRightMsg = new BMessage(kMsgSetAlignment);
	alignRightMsg->AddInt32("alignment", eAlignRight);
	formatMenu->AddItem(new BMenuItem("Allinea a destra", alignRightMsg));
	formatMenu->AddSeparatorItem();
	formatMenu->AddItem(new BMenuItem("Colore testo" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowTextColor)));
	formatMenu->AddItem(new BMenuItem("Colore sfondo" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowBgColor)));
	formatMenu->AddSeparatorItem();
	// Bordi di cella (Fase 11): un lato alla volta, come Grassetto/
	// Corsivo sopra -- vedi MainWindow::ToggleBorder ("side" nello
	// stesso ordine di CellStyle::fTBorderColor/fLBorderColor/
	// fBBorderColor/fRBorderColor).
	BMessage* topBorderMsg = new BMessage(kMsgToggleBorder);
	topBorderMsg->AddInt32("side", 0);
	formatMenu->AddItem(new BMenuItem("Bordo superiore", topBorderMsg));
	BMessage* leftBorderMsg = new BMessage(kMsgToggleBorder);
	leftBorderMsg->AddInt32("side", 1);
	formatMenu->AddItem(new BMenuItem("Bordo sinistro", leftBorderMsg));
	BMessage* bottomBorderMsg = new BMessage(kMsgToggleBorder);
	bottomBorderMsg->AddInt32("side", 2);
	formatMenu->AddItem(new BMenuItem("Bordo inferiore", bottomBorderMsg));
	BMessage* rightBorderMsg = new BMessage(kMsgToggleBorder);
	rightBorderMsg->AddInt32("side", 3);
	formatMenu->AddItem(new BMenuItem("Bordo destro", rightBorderMsg));
	formatMenu->AddItem(new BMenuItem("Nessun bordo", new BMessage(kMsgClearBorders)));
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
	dataMenu->AddSeparatorItem();
	// Ordina per righe intere, confrontando la colonna piu' a sinistra
	// dell'intervallo selezionato (vedi SheetView::SortSelection).
	dataMenu->AddItem(new BMenuItem("Ordina crescente", new BMessage(kMsgSortAscending)));
	dataMenu->AddItem(new BMenuItem("Ordina decrescente", new BMessage(kMsgSortDescending)));
	dataMenu->AddSeparatorItem();
	// Il numero di righe/colonne e il punto vengono dalla selezione
	// corrente (SheetView::SelectionRange()), non da una selezione di
	// intestazione -- Atomo123 non ha ancora un modo per selezionare
	// un'intera riga/colonna cliccando l'intestazione. Quattro voci
	// separate ed esplicite invece dell'unico comando "Inserisci"/
	// "Elimina" di Sum-It storico (che inferiva riga o colonna dal
	// fatto che la selezione coprisse un'intera riga/colonna): senza
	// quel gesto sarebbe ambiguo qui.
	dataMenu->AddItem(new BMenuItem("Inserisci riga", new BMessage(kMsgInsertRows)));
	dataMenu->AddItem(new BMenuItem("Inserisci colonna", new BMessage(kMsgInsertColumns)));
	dataMenu->AddItem(new BMenuItem("Elimina riga", new BMessage(kMsgDeleteRows)));
	dataMenu->AddItem(new BMenuItem("Elimina colonna", new BMessage(kMsgDeleteColumns)));
	dataMenu->AddSeparatorItem();
	// Blocca tutto cio' che sta sopra/a sinistra della cella attiva
	// (come Excel): voce con segno di spunta, sincronizzato a ogni
	// attivazione/cambio foglio -- vedi SheetView::ToggleFreezePanes.
	fFreezeMenuItem = new BMenuItem("Blocca riquadri", new BMessage(kMsgToggleFreeze));
	dataMenu->AddItem(fFreezeMenuItem);
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
	insertMenu->AddItem(new BMenuItem("Intervalli con nome" B_UTF8_ELLIPSIS,
		new BMessage(kMsgShowNames)));
	menuBar->AddItem(insertMenu);

	// Barra strumenti: pulsanti di testo semplici (BButton), non
	// BToolBar -- quella classe vive solo sotto develop/headers/
	// private/shared/ su questo sistema, non nell'SDK pubblico
	// stabile, e il progetto usa solo API pubbliche. I pulsanti
	// inviano semplicemente gli stessi messaggi gia' gestiti dai
	// menu, nessuna logica nuova -- costruiti da BuildToolbar() sopra
	// a partire da kToolbarGroups invece che uno per uno a mano.
	BView* toolbar = BuildToolbar(this);

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

	// Striscia di schede del foglio attivo, come Excel/LibreOffice
	// Calc, in basso sotto la griglia -- sostituisce il menu a tendina
	// iniziale su richiesta dell'utente. Scorre con due frecce quando
	// le schede non entrano tutte nella larghezza della finestra
	// (una cartella di lavoro reale puo' averne decine, vedi Fase 9),
	// invece di tagliarle silenziosamente o far crescere la finestra
	// all'infinito. Ripopolata da RebuildSheetTabs() ogni volta che
	// cambia l'elenco dei fogli o il foglio attivo (Nuovo, Apri, cambio
	// scheda, in futuro Inserisci/Elimina/Rinomina foglio).
	fSheetTabView = new SheetTabView("sheetTabs", kMsgSwitchSheet, this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.Add(toolbar)
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(4, 4, 4, 4)
			.Add(fCellLabel)
			.Add(fFormulaBar)
		.End()
		.Add(scroll)
		.Add(fSheetTabView);

	RebuildSheetTabs();

	fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this));
	fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this));
	fFindWindow = NULL;
	fChartWindow = NULL;
	fPivotWindow = NULL;
	fNameWindow = NULL;
	fPasteSpecialWindow = NULL;
	fGoToWindow = NULL;
	fColorWindow = NULL;
	fPreferencesWindow = NULL;

	UpdateTitle();
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
	if (fNameWindow)
	{
		fNameWindow->Lock();
		fNameWindow->Quit();
	}
	if (fPasteSpecialWindow)
	{
		fPasteSpecialWindow->Lock();
		fPasteSpecialWindow->Quit();
	}
	if (fGoToWindow)
	{
		fGoToWindow->Lock();
		fGoToWindow->Quit();
	}
	if (fColorWindow)
	{
		fColorWindow->Lock();
		fColorWindow->Quit();
	}
	if (fPreferencesWindow)
	{
		fPreferencesWindow->Lock();
		fPreferencesWindow->Quit();
	}
	// fDoc e' sempre lo stesso puntatore di fSheets[fActiveSheetIndex]
	// .doc (mai un CContainer a parte): rilasciare solo fDoc
	// perderebbe (memory leak, non un doppio Release) tutti gli altri
	// fogli della cartella di lavoro.
	for (size_t i = 0; i < fSheets.size(); i++)
		fSheets[i].doc->Release();
}

void MainWindow::UpdateTitle()
{
	BString title;
	if (fModified)
		title << "* ";
	title << (fDocumentName.Length() > 0 ? fDocumentName : BString("Nuovo documento"));
	title << " \xE2\x80\x94 Atomo123"; // em dash (U+2014) in UTF-8
	SetTitle(title.String());
}

void MainWindow::MarkModified()
{
	if (!fModified)
	{
		fModified = true;
		UpdateTitle();
	}
}

void MainWindow::DocumentChanged()
{
	MarkModified();
}

bool MainWindow::ConfirmDiscardChanges()
{
	if (!fModified)
		return true;

	BAlert* alert = new BAlert("Modifiche non salvate",
		"Le modifiche non salvate andranno perse. Continuare?",
		"Annulla", "Continua senza salvare", NULL,
		B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	return alert->Go() == 1;
}

void MainWindow::ResetWorkbook(const char* name)
{
	// Rilascia tutti i fogli precedenti (Release(), mai delete diretto
	// -- CContainer e' un BLocker con conteggio di riferimenti).
	for (size_t i = 0; i < fSheets.size(); i++)
		fSheets[i].doc->Release();
	fSheets.clear();

	AscdSheet sheet;
	sheet.name = name;
	sheet.doc = new CContainer(NULL, NULL);
	fSheets.push_back(sheet);

	fActiveSheetIndex = 0;
	fDoc = fSheets[0].doc;
	fCharts.clear();

	if (fSheetView)
	{
		fSheetView->SetDocument(fDoc);
		fSheetView->SetCharts(&fCharts);
		fSheetView->SetColumnWidths(fSheets[0].colWidths);
		fSheetView->SetRowHeights(fSheets[0].rowHeights);
		// Un foglio nuovo (fSheets[0] appena creato da ResetWorkbook)
		// non ha mai nulla di bloccato: frozenRows/frozenCols sono gia'
		// 0 di default (vedi AscdSheet in AscdIO.h), letti comunque da
		// li' invece di un letterale 0,0 per restare corretto anche se
		// in futuro ResetWorkbook ricevesse fogli non vuoti.
		fSheetView->SetFreezePanes(fSheets[0].frozenRows, fSheets[0].frozenCols);
		if (fFreezeMenuItem)
			fFreezeMenuItem->SetMarked(false);
	}
	if (fSheetTabView)
		RebuildSheetTabs();

	AttachSheetResolver();
}

// ISheetResolver (Fase 9): cerca per nome esatto (case-sensitive) fra
// i fogli aperti -- vedi il commento su ISheetResolver in Container.h
// sul perche' la risoluzione e' per nome e non per indice.
CContainer* MainWindow::ResolveSheetByName(const char* inName)
{
	for (size_t i = 0; i < fSheets.size(); i++)
		if (fSheets[i].name == inName)
			return fSheets[i].doc;
	return NULL;
}

void MainWindow::AttachSheetResolver()
{
	for (size_t i = 0; i < fSheets.size(); i++)
		fSheets[i].doc->SetSheetResolver(this);
}

void MainWindow::RecalculateActiveWorkbook()
{
	if (fSheets.size() > 1)
		RecalculateWorkbook(fSheets);
	else
		RecalculateAll(fDoc);
}

void MainWindow::SwitchToSheet(int index)
{
	if (index < 0 || index >= (int)fSheets.size() || index == fActiveSheetIndex)
		return;

	// fDoc e' gia' lo stesso puntatore di fSheets[fActiveSheetIndex].doc
	// (mai copiato: le mutazioni fatte finora sono gia' scritte
	// direttamente li'), quindi non c'e' nulla da salvare per il
	// documento -- solo fCharts e le larghezze di colonna, entrambi un
	// vector per valore invece che un puntatore (SheetView ne tiene
	// solo l'indirizzo/li tiene internamente, non un riferimento al
	// foglio: vedi il commento sui campi in MainWindow.h), vanno
	// risincronizzati esplicitamente in entrambe le direzioni.
	fSheets[fActiveSheetIndex].charts = fCharts;
	fSheets[fActiveSheetIndex].colWidths = fSheetView->CustomColumnWidths();
	fSheets[fActiveSheetIndex].rowHeights = fSheetView->CustomRowHeights();
	fSheets[fActiveSheetIndex].frozenRows = fSheetView->FrozenRows();
	fSheets[fActiveSheetIndex].frozenCols = fSheetView->FrozenCols();

	fActiveSheetIndex = index;
	fDoc = fSheets[index].doc;
	fCharts = fSheets[index].charts;

	fSheetView->SetDocument(fDoc);
	fSheetView->SetColumnWidths(fSheets[index].colWidths);
	fSheetView->SetRowHeights(fSheets[index].rowHeights);
	fSheetView->SetFreezePanes(fSheets[index].frozenRows, fSheets[index].frozenCols);
	fFreezeMenuItem->SetMarked(fSheetView->HasFreezePanes());
	fFormulaBar->SetText("");
	RebuildSheetTabs();
}

void MainWindow::RebuildSheetTabs()
{
	if (!fSheetTabView)
		return;

	std::vector<BString> names;
	for (size_t i = 0; i < fSheets.size(); i++)
		names.push_back(fSheets[i].name);

	fSheetTabView->SetSheets(names, fActiveSheetIndex);
}

void MainWindow::NewDocument()
{
	if (!ConfirmDiscardChanges())
		return;

	ResetWorkbook("Foglio1");
	fFormulaBar->SetText("");

	fDocumentName = "";
	fModified = false;
	UpdateTitle();
}

// Legge un singolo blocco ASCD (un solo foglio, formato "ASCD") in un
// AscdSheet nuovo di nome "Foglio1" -- usato sia per un vecchio file
// .ascd nativo a un solo foglio, sia per l'output di un translator che
// non produce ancora una cartella di lavoro multi-foglio (CSV/ODS/XLS,
// e XLSX per i formati diversi da .xlsx/.xlsm con piu' fogli -- vedi
// il commento in XlsxTranslator.cpp). Restituisce false (e rilascia il
// documento) se la lettura fallisce.
static bool ReadSingleSheetASCD(BPositionIO* source, AscdSheet* outSheet)
{
	outSheet->name = "Foglio1";
	outSheet->doc = new CContainer(NULL, NULL);
	status_t err = LoadASCD(source, outSheet->doc, &outSheet->charts, &outSheet->colWidths);
	if (err != B_OK)
	{
		outSheet->doc->Release();
		return false;
	}
	return true;
}

void MainWindow::OpenFile(const entry_ref& ref)
{
	if (!ConfirmDiscardChanges())
		return;

	BFile file(&ref, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
	{
		BAlert* alert = new BAlert("Errore",
			"Impossibile aprire il file selezionato.", "OK");
		alert->Go();
		return;
	}

	std::vector<AscdSheet> newSheets;
	bool ok = true;

	if (IsASCDBookFile(&file))
	{
		// Cartella di lavoro nativa multi-foglio (Fase 9): si legge
		// direttamente, senza passare dal Translation Kit -- stesso
		// motivo di IsASCDFile sotto.
		ok = LoadASCDBook(&file, &newSheets) == B_OK;
	}
	else if (IsASCDFile(&file))
	{
		// Vecchio file .ascd a un solo foglio (senza il wrapper "ASCB"):
		// si legge direttamente, senza passare dal Translation Kit --
		// che per un file gia' ASCD lo farebbe comunque rileggere/
		// riscrivere tramite la copia duplicata di ReadASCD/WriteASCD
		// di un translator qualunque, perdendo la sezione dei grafici
		// incorporati che quella copia non conosce.
		AscdSheet sheet;
		ok = ReadSingleSheetASCD(&file, &sheet);
		if (ok)
			newSheets.push_back(sheet);
	}
	else
	{
		// BTranslatorRoster sceglie automaticamente il translator
		// installato adatto (CSV/XLS/XLSX/ODS) in base al contenuto
		// reale del file, non all'estensione. Solo XlsxTranslator (per
		// .xlsx/.xlsm con piu' fogli) produce una cartella di lavoro
		// "ASCB"; gli altri restano a un solo foglio, senza grafici.
		BMallocIO ascd;
		status_t translateErr = BTranslatorRoster::Default()->Translate(&file, NULL, NULL,
			&ascd, kAtomoNativeFormat);
		if (translateErr != B_OK)
		{
			BAlert* alert = new BAlert("Errore",
				"Formato file non riconosciuto da nessun translator installato.",
				"OK");
			alert->Go();
			return;
		}

		ascd.Seek(0, SEEK_SET);
		if (IsASCDBookFile(&ascd))
			ok = LoadASCDBook(&ascd, &newSheets) == B_OK;
		else
		{
			AscdSheet sheet;
			ok = ReadSingleSheetASCD(&ascd, &sheet);
			if (ok)
				newSheets.push_back(sheet);
		}
	}

	if (!ok || newSheets.empty())
	{
		for (size_t i = 0; i < newSheets.size(); i++)
			newSheets[i].doc->Release();
		BAlert* alert = new BAlert("Errore",
			"Il file e' stato tradotto ma i dati risultanti non sono validi.",
			"OK");
		alert->Go();
		return;
	}

	for (size_t i = 0; i < fSheets.size(); i++)
		fSheets[i].doc->Release();
	fSheets = newSheets;
	fActiveSheetIndex = 0;
	fDoc = fSheets[0].doc;
	fCharts = fSheets[0].charts;
	fSheetView->SetDocument(fDoc);
	fSheetView->SetCharts(&fCharts);
	fSheetView->SetColumnWidths(fSheets[0].colWidths);
	fSheetView->SetRowHeights(fSheets[0].rowHeights);
	fSheetView->SetFreezePanes(fSheets[0].frozenRows, fSheets[0].frozenCols);
	fFreezeMenuItem->SetMarked(fSheetView->HasFreezePanes());
	RebuildSheetTabs();

	// Collega il resolver PRIMA di ricalcolare: ogni foglio e' stato
	// letto (e gia' ricalcolato una prima volta) da LoadASCDBook/
	// LoadASCD singolarmente, quando gli altri fogli della stessa
	// cartella di lavoro non erano ancora tutti presenti -- un
	// riferimento incrociato (Fase 9, "NomeFoglio!Cella") non poteva
	// quindi risolversi in quella prima passata. Questo ricalcolo
	// dell'intera cartella, con tutti i fogli gia' collegati fra loro,
	// e' il primo punto in cui puo' farlo correttamente.
	AttachSheetResolver();
	RecalculateWorkbook(fSheets);

	fDocumentName = ref.name;
	fModified = false;
	UpdateTitle();
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
		// Salva l'intera cartella di lavoro (tutti i fogli, non solo
		// quello attivo) nel formato "ASCB" -- fCharts e' la copia di
		// lavoro del foglio attivo (vedi il commento sui campi in
		// MainWindow.h), va risincronizzata su fSheets prima di
		// scrivere, altrimenti l'ultima modifica ai grafici/alle
		// larghezze di colonna del foglio attivo non verrebbe salvata.
		fSheets[fActiveSheetIndex].charts = fCharts;
		fSheets[fActiveSheetIndex].colWidths = fSheetView->CustomColumnWidths();
		fSheets[fActiveSheetIndex].rowHeights = fSheetView->CustomRowHeights();
		fSheets[fActiveSheetIndex].frozenRows = fSheetView->FrozenRows();
		fSheets[fActiveSheetIndex].frozenCols = fSheetView->FrozenCols();

		status_t err = SaveASCDBook(fSheets, &file);
		if (err != B_OK)
		{
			BAlert* alert = new BAlert("Errore", "Scrittura del file fallita.", "OK");
			alert->Go();
			return;
		}
		fDocumentName = name;
		fModified = false;
		UpdateTitle();
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
		return;
	}

	fDocumentName = name;
	fModified = false;
	UpdateTitle();
}

void MainWindow::CopySelection(bool cut)
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	int numRows = sel.bottom - sel.top + 1;
	int numCols = sel.right - sel.left + 1;

	// Una sola cella resta testo semplice (compatibile con qualunque
	// altra applicazione Haiku). Un intervallo piu' grande usa il
	// formato TSV classico -- colonne separate da tabulazione, righe
	// da ritorno a capo -- lo stesso capito da Excel/LibreOffice
	// Calc, cosi' copiare/incollare fra Atomo123 e loro tramite gli
	// appunti di sistema funziona gia' da solo, senza bisogno di un
	// formato proprietario.
	// "text/x-atomo-values" affianca lo stesso testo ma con
	// GetCellResult (il risultato calcolato, es. "30") al posto di
	// GetCellFormula (il testo della formula, es. "=A1+B1") -- solo
	// per Incolla speciale > Solo valori (vedi HandlePasteSpecialRequest
	// sotto), che deve convertire una formula copiata nel suo valore
	// statico invece di ricopiare la formula stessa. Un secondo campo
	// dati sullo stesso BMessage degli appunti, non un secondo giro di
	// Lock/Clear/Commit.
	BString clipText, valuesText;
	for (int i = 0; i < numRows; i++)
	{
		if (i > 0)
		{
			clipText << "\n";
			valuesText << "\n";
		}
		for (int j = 0; j < numCols; j++)
		{
			if (j > 0)
			{
				clipText << "\t";
				valuesText << "\t";
			}
			cell src(sel.left + j, sel.top + i);
			char text[4096];
			fDoc->GetCellFormula(src, text, sizeof(text), false);
			clipText << text;
			fDoc->GetCellResult(src, text, sizeof(text), true);
			valuesText << text;
		}
	}

	if (be_clipboard->Lock())
	{
		be_clipboard->Clear();
		BMessage* clip = be_clipboard->Data();
		if (clip)
		{
			clip->AddData("text/plain", B_MIME_TYPE, clipText.String(), clipText.Length());
			clip->AddData("text/x-atomo-values", B_MIME_TYPE,
				valuesText.String(), valuesText.Length());
		}
		be_clipboard->Commit();
		be_clipboard->Unlock();
	}

	if (cut)
	{
		fSheetView->SaveUndoState(sel);
		for (int i = 0; i < numRows; i++)
			for (int j = 0; j < numCols; j++)
				fDoc->DisposeCell(cell(sel.left + j, sel.top + i));
		RecalculateActiveWorkbook();
		fSheetView->Invalidate();
		SelectionChanged(fSheetView->Selection());
		MarkModified();
	}
}

// Divide un testo TSV (tabulazioni fra colonne, ritorni a capo fra
// righe -- lo stesso formato scritto da CopySelection, capito anche
// da Excel/LibreOffice Calc) in una griglia di stringhe. Condivisa da
// PasteSelection e HandlePasteSpecialRequest sotto: entrambe leggono
// un campo dati degli appunti e lo trasformano nella stessa griglia,
// solo la sorgente del testo (text/plain o text/x-atomo-values) e il
// modo in cui la griglia viene poi scritta nel documento cambiano.
static void ParseTSVGrid(const char* text, ssize_t len,
	std::vector<std::vector<BString> >& grid)
{
	BString pasted(text, len);

	int32 rowStart = 0;
	while (rowStart <= pasted.Length())
	{
		int32 rowEnd = pasted.FindFirst('\n', rowStart);
		if (rowEnd < 0)
			rowEnd = pasted.Length();
		BString rowText;
		pasted.CopyInto(rowText, rowStart, rowEnd - rowStart);

		std::vector<BString> cols;
		int32 colStart = 0;
		while (colStart <= rowText.Length())
		{
			int32 colEnd = rowText.FindFirst('\t', colStart);
			if (colEnd < 0)
				colEnd = rowText.Length();
			BString cellText;
			rowText.CopyInto(cellText, colStart, colEnd - colStart);
			cols.push_back(cellText);
			if (colEnd == rowText.Length())
				break;
			colStart = colEnd + 1;
		}
		grid.push_back(cols);

		if (rowEnd == pasted.Length())
			break;
		rowStart = rowEnd + 1;
	}
}

void MainWindow::PasteSelection()
{
	if (!fDoc)
		return;

	if (!be_clipboard->Lock())
		return;

	BMessage* clip = be_clipboard->Data();
	const char* text = NULL;
	ssize_t len = 0;
	bool found = clip && clip->FindData("text/plain", B_MIME_TYPE,
		(const void**)&text, &len) == B_OK;

	if (found)
	{
		std::vector<std::vector<BString> > grid;
		ParseTSVGrid(text, len, grid);

		int numRows = (int)grid.size();
		int numCols = 0;
		for (size_t i = 0; i < grid.size(); i++)
			numCols = std::max(numCols, (int)grid[i].size());

		range sel = fSheetView->SelectionRange();
		cell anchor = sel.TopLeft();

		range destRange;
		if (numRows == 1 && numCols == 1 &&
			(sel.right > sel.left || sel.bottom > sel.top))
		{
			// Un solo valore incollato su un intervallo piu' grande di
			// una cella: riempie tutto l'intervallo selezionato (come
			// Excel/LibreOffice Calc), non solo la cella attiva.
			destRange = sel;
		}
		else
		{
			// Incollato ancorato all'angolo in alto a sinistra della
			// selezione corrente, esteso alla dimensione del blocco
			// incollato (non a quella della selezione corrente, che
			// puo' anche essere una sola cella).
			destRange = range(anchor.h, anchor.v,
				anchor.h + numCols - 1, anchor.v + numRows - 1);
		}

		fSheetView->SaveUndoState(destRange);

		for (int row = destRange.top; row <= destRange.bottom; row++)
		{
			int srcRow = std::min(row - destRange.top, numRows - 1);
			for (int col = destRange.left; col <= destRange.right; col++)
			{
				int srcCol = std::min(col - destRange.left, numCols - 1);
				const char* fieldText = "";
				if (srcRow < (int)grid.size() && srcCol < (int)grid[srcRow].size())
					fieldText = grid[srcRow][srcCol].String();

				cell dest(col, row);
				if (fieldText[0] == 0)
					fDoc->DisposeCell(dest);
				else
				{
					try
					{
						TryToParseString(fieldText, dest, fDoc, true);
					}
					catch (...)
					{
					}
				}
			}
		}

		RecalculateActiveWorkbook();
		fSheetView->Invalidate();
		fSheetView->SetSelection(destRange.TopLeft());
		fSheetView->ExtendSelection(destRange.BotRight());
		MarkModified();
	}

	be_clipboard->Unlock();
}

// Incolla speciale (Fase 7, sul modello di Sum-It storico
// PasteSpecialDialog): content sceglie il campo dati sorgente
// (0 = "text/plain", lo stesso testo di un Incolla normale, puo'
// contenere formule; 1 = "text/x-atomo-values", il risultato gia'
// calcolato -- vedi il commento in CopySelection sopra), operation
// sceglie come combinare il valore incollato con quello gia' presente
// nella cella di destinazione (0 = Sovrascrivi, lo stesso di Incolla;
// 1..4 = Somma/Sottrai/Moltiplica/Dividi, sempre su valori, mai su
// formule -- come Excel, il risultato di un'operazione aritmetica e'
// sempre una cella statica, non conta se la sorgente era una
// formula), transpose scambia righe e colonne del blocco incollato
// prima di scriverlo.
void MainWindow::HandlePasteSpecialRequest(int32 content, int32 operation, bool transpose)
{
	if (!fDoc)
		return;

	if (!be_clipboard->Lock())
		return;

	BMessage* clip = be_clipboard->Data();
	const char* text = NULL;
	ssize_t len = 0;
	bool found = false;
	if (content == 1 && clip)
		found = clip->FindData("text/x-atomo-values", B_MIME_TYPE,
			(const void**)&text, &len) == B_OK;
	if (!found && clip)
		found = clip->FindData("text/plain", B_MIME_TYPE,
			(const void**)&text, &len) == B_OK;

	if (found)
	{
		std::vector<std::vector<BString> > grid;
		ParseTSVGrid(text, len, grid);

		if (transpose)
		{
			size_t rows = grid.size();
			size_t cols = 0;
			for (size_t i = 0; i < rows; i++)
				cols = std::max(cols, grid[i].size());

			std::vector<std::vector<BString> > t(cols);
			for (size_t c = 0; c < cols; c++)
			{
				t[c].resize(rows);
				for (size_t r = 0; r < rows; r++)
					t[c][r] = (c < grid[r].size()) ? grid[r][c] : BString();
			}
			grid = t;
		}

		int numRows = (int)grid.size();
		int numCols = 0;
		for (size_t i = 0; i < grid.size(); i++)
			numCols = std::max(numCols, (int)grid[i].size());

		range sel = fSheetView->SelectionRange();
		cell anchor = sel.TopLeft();

		range destRange;
		if (numRows == 1 && numCols == 1 &&
			(sel.right > sel.left || sel.bottom > sel.top))
			destRange = sel; // stesso comportamento di PasteSelection
		else
			destRange = range(anchor.h, anchor.v,
				anchor.h + numCols - 1, anchor.v + numRows - 1);

		fSheetView->SaveUndoState(destRange);

		for (int row = destRange.top; row <= destRange.bottom; row++)
		{
			int srcRow = std::min(row - destRange.top, numRows - 1);
			for (int col = destRange.left; col <= destRange.right; col++)
			{
				int srcCol = std::min(col - destRange.left, numCols - 1);
				const char* fieldText = "";
				if (srcRow < (int)grid.size() && srcCol < (int)grid[srcRow].size())
					fieldText = grid[srcRow][srcCol].String();

				cell dest(col, row);

				if (operation == 0)
				{
					if (fieldText[0] == 0)
						fDoc->DisposeCell(dest);
					else
					{
						try { TryToParseString(fieldText, dest, fDoc, true); }
						catch (...) { }
					}
				}
				else if (fieldText[0] != 0)
				{
					double srcValue = atof_i(fieldText);
					Value destVal;
					double destValue = (fDoc->GetValue(dest, destVal)
						&& destVal.fType == eNumData && !destVal.IsNan())
						? (double)destVal : 0.0;

					double result = srcValue;
					switch (operation)
					{
						case 1: result = destValue + srcValue; break;
						case 2: result = destValue - srcValue; break;
						case 3: result = destValue * srcValue; break;
						case 4:
							if (srcValue != 0.0)
								result = destValue / srcValue;
							break;
					}

					fDoc->NewCell(dest, Value(result), NULL);
				}
			}
		}

		RecalculateActiveWorkbook();
		fSheetView->Invalidate();
		fSheetView->SetSelection(destRange.TopLeft());
		fSheetView->ExtendSelection(destRange.BotRight());
		MarkModified();
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

// Ricostruisce l'elenco di NameWindow a partire da fDoc->GetOrCreateNameTable()
// -- chiamato prima di Show() e di nuovo dopo ogni Aggiungi/Aggiorna/Elimina
// (HandleDefineName/HandleDeleteName), cosi' la finestra riflette sempre lo
// stato vero di CNameTable invece di tenerne una copia che puo' disallinearsi.
void MainWindow::RefreshNameWindow()
{
	if (!fNameWindow)
		return;

	std::vector<BString> names, ranges;
	CNameTable* table = fDoc->GetOrCreateNameTable();
	for (CNameTable::iterator i = table->begin(); i != table->end(); ++i)
	{
		char rangeText[32];
		range r = i->second;
		r.GetRCName(rangeText);
		names.push_back(BString((const char*)i->first));
		ranges.push_back(BString(rangeText));
	}
	fNameWindow->SetNames(names, ranges);
}

void MainWindow::ShowNameWindow()
{
	if (!fNameWindow)
		fNameWindow = new NameWindow(BMessenger(this));

	RefreshNameWindow();

	if (fNameWindow->IsHidden())
		fNameWindow->Show();
	fNameWindow->Activate();
}

void MainWindow::ShowPasteSpecialWindow()
{
	if (!fPasteSpecialWindow)
		fPasteSpecialWindow = new PasteSpecialWindow(BMessenger(this));

	if (fPasteSpecialWindow->IsHidden())
		fPasteSpecialWindow->Show();
	fPasteSpecialWindow->Activate();
}

void MainWindow::ShowGoToWindow()
{
	if (!fGoToWindow)
		fGoToWindow = new GoToWindow(BMessenger(this));

	if (fGoToWindow->IsHidden())
		fGoToWindow->Show();
	fGoToWindow->Activate();
}

void MainWindow::ShowColorWindow(bool background)
{
	if (!fColorWindow)
		fColorWindow = new ColorWindow(BMessenger(this));

	CellStyle cs;
	if (fDoc)
		fDoc->GetCellStyle(fSheetView->Selection(), cs);
	fColorWindow->SetMode(background, background ? cs.fLowColor : cs.fHighColor);

	if (fColorWindow->IsHidden())
		fColorWindow->Show();
	fColorWindow->Activate();
}

void MainWindow::ShowPreferencesWindow()
{
	if (!fPreferencesWindow)
		fPreferencesWindow = new PreferencesWindow(BMessenger(this));

	fPreferencesWindow->SetValues(fSheetView->ShowGrid(), gDecimalPoint, gListSeparator);

	if (fPreferencesWindow->IsHidden())
		fPreferencesWindow->Show();
	fPreferencesWindow->Activate();
}

void MainWindow::HandlePreferencesRequest(bool showGrid, char decimalSep, char listSep)
{
	fSheetView->SetShowGrid(showGrid);
	gDecimalPoint = decimalSep;
	gListSeparator = listSep;

	// gPrefs (Preferences.h) puo' essere NULL in un test che non passa
	// da App::App() (vedi il commento li'): l'effetto in memoria sopra
	// resta comunque valido e testabile, solo la persistenza su disco
	// si salta.
	if (gPrefs)
	{
		char decStr[2] = { decimalSep, 0 };
		char listStr[2] = { listSep, 0 };
		gPrefs->SetPrefString("decimalSeparator", decStr);
		gPrefs->SetPrefString("listSeparator", listStr);
		gPrefs->SetPrefInt("showGrid", showGrid ? 1 : 0);
		try { gPrefs->WritePrefFile(); }
		catch (CErr&) { }
	}
}

// Legge famiglia/stile/dimensione/colore di un font registrato in
// gFontSizeTable, con un ripiego sicuro se l'indice non e' (ancora)
// valido -- CContainer::CContainer registra un font predefinito in
// gFontSizeTable SOLO se costruito con un CCellView non nullo
// (inPane), ma la UI moderna passa sempre NULL (CCellView e' uno
// stub permanente, vedi EngineViewStub.h -- stessa classe di bug gia'
// trovata e corretta per le formule fra fogli e gli intervalli con
// nome in questa stessa fase): un documento nuovo, mai passato da
// un'importazione XLSX (che registra font propri in Excel.pass1.cpp),
// ha quindi gFontSizeTable completamente vuota. CFontSizeTable::
// GetFontInfo non controlla i limiti (a differenza di SetFontID):
// chiamarla con un indice fuori dai limiti e' un accesso non valido
// alla memoria (bug scoperto scrivendo tests/test_format.cpp: il
// primo tentativo, senza questo controllo, restava bloccato senza
// mai stampare nulla). Il ripiego usa il font di sistema, lo stesso
// aspetto che una cella "senza font personalizzato" ha gia' oggi.
static void GetCellFontInfo(int fontID, font_family* family, font_style* style,
	float* size, rgb_color* color)
{
	if (fontID >= 0 && (unsigned long)fontID < gFontSizeTable.Count())
	{
		gFontSizeTable.GetFontInfo(fontID, family, style, size, color);
		return;
	}

	be_plain_font->GetFamilyAndStyle(family, style);
	*size = be_plain_font->Size();
	if (color)
	{
		color->red = color->green = color->blue = 0;
		color->alpha = 255;
	}
}

// Applica un nuovo font (famiglia/stile/dimensione/colore) a tutte le
// celle di SelectionRange() -- CellStyle::fFont e' un indice in
// gFontSizeTable (mai un colore/stile diretto), quindi si registra
// prima l'eventuale nuova combinazione (GetFontID deduplica, non crea
// un doppione se gia' esiste) e poi si scrive quell'indice nella
// mappa di stile del documento, stesso principio di SetCellFormat.
static void ApplyFontToRange(CContainer* doc, range sel,
	const char* family, const char* style, float size, rgb_color color)
{
	int newFontID = (int)gFontSizeTable.GetFontID(family, style, size, color);
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			doc->GetCellStyle(c, cs);
			cs.fFont = newFontID;
			doc->SetCellStyle(c, cs);
		}
}

void MainWindow::ToggleBold()
{
	if (!fDoc)
		return;

	CellStyle cs;
	fDoc->GetCellStyle(fSheetView->Selection(), cs);
	font_family family;
	font_style style;
	float size;
	rgb_color color;
	GetCellFontInfo(cs.fFont, &family, &style, &size, &color);

	// Lo stato di partenza (grassetto o no) si legge dalla sola cella
	// attiva -- come il pulsante "risulta premuto" o no di Excel -- ma
	// lo stato OPPOSTO si applica a tutto SelectionRange(): se
	// l'attiva non e' in grassetto, l'intera selezione lo diventa, e
	// viceversa.
	BString styleStr(style);
	bool wasBold = styleStr.IFindFirst("Bold") >= 0;
	bool isItalic = styleStr.IFindFirst("Italic") >= 0;

	char newStyle[64];
	if (!wasBold && isItalic)
		strlcpy(newStyle, "Bold Italic", sizeof(newStyle));
	else if (!wasBold)
		strlcpy(newStyle, "Bold", sizeof(newStyle));
	else if (isItalic)
		strlcpy(newStyle, "Italic", sizeof(newStyle));
	else
		strlcpy(newStyle, "Regular", sizeof(newStyle));

	ApplyFontToRange(fDoc, fSheetView->SelectionRange(), family, newStyle, size, color);
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::ToggleItalic()
{
	if (!fDoc)
		return;

	CellStyle cs;
	fDoc->GetCellStyle(fSheetView->Selection(), cs);
	font_family family;
	font_style style;
	float size;
	rgb_color color;
	GetCellFontInfo(cs.fFont, &family, &style, &size, &color);

	BString styleStr(style);
	bool isBold = styleStr.IFindFirst("Bold") >= 0;
	bool wasItalic = styleStr.IFindFirst("Italic") >= 0;

	char newStyle[64];
	if (isBold && !wasItalic)
		strlcpy(newStyle, "Bold Italic", sizeof(newStyle));
	else if (!wasItalic)
		strlcpy(newStyle, "Italic", sizeof(newStyle));
	else if (isBold)
		strlcpy(newStyle, "Bold", sizeof(newStyle));
	else
		strlcpy(newStyle, "Regular", sizeof(newStyle));

	ApplyFontToRange(fDoc, fSheetView->SelectionRange(), family, newStyle, size, color);
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::SetAlignment(char alignment)
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fAlignment = alignment;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::SetTextColor(rgb_color color)
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fHighColor = color;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::SetBackgroundColor(rgb_color color)
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fLowColor = color;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

// Legge/scrive il campo di CellStyle corrispondente a "side" (0..3 =
// superiore/sinistro/inferiore/destro, stesso ordine di
// ToggleBorder/ClearBorders in MainWindow.h) -- un unico punto invece
// di quattro rami quasi identici ripetuti in ToggleBorder/ClearBorders
// sotto.
static uchar& BorderField(CellStyle& cs, int side)
{
	switch (side)
	{
		case 1: return cs.fLBorderColor;
		case 2: return cs.fBBorderColor;
		case 3: return cs.fRBorderColor;
		default: return cs.fTBorderColor;
	}
}

void MainWindow::ToggleBorder(int side)
{
	if (!fDoc)
		return;

	// Stato di partenza dalla sola cella attiva (come Grassetto/
	// Corsivo): se non ha gia' un bordo su quel lato, l'intera
	// selezione lo ottiene; se ce l'ha gia', lo perde.
	CellStyle activeStyle;
	fDoc->GetCellStyle(fSheetView->Selection(), activeStyle);
	bool hadBorder = BorderField(activeStyle, side) != 0;
	uchar newValue = hadBorder ? 0 : 1;

	range sel = fSheetView->SelectionRange();
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			BorderField(cs, side) = newValue;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::ClearBorders()
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fTBorderColor = cs.fLBorderColor = cs.fBBorderColor = cs.fRBorderColor = 0;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::HandleGoToRequest(const char* rangeText)
{
	range r;
	if (!ParseRangeRef(rangeText, r))
		return;

	fSheetView->SetSelection(r.TopLeft());
	if (!(r.TopLeft() == r.BotRight()))
		fSheetView->ExtendSelection(r.BotRight());
}

void MainWindow::HandleDefineName(const char* name, const char* rangeText)
{
	if (!name || !name[0])
		return;

	range r;
	if (!ParseRangeRef(rangeText, r))
		return;

	(*fDoc->GetOrCreateNameTable())[name] = r;

	// Una ridefinizione puo' cambiare il risultato di qualunque
	// formula che usa quel nome, in qualunque foglio -- stesso motivo
	// per cui una modifica cross-foglio (Fase 9) ricalcola l'intera
	// cartella di lavoro, non solo il foglio attivo.
	RecalculateActiveWorkbook();
	RefreshNameWindow();
}

void MainWindow::HandleDeleteName(const char* name)
{
	if (!name || !name[0])
		return;

	fDoc->GetOrCreateNameTable()->erase(name);
	RecalculateActiveWorkbook();
	RefreshNameWindow();
}

void MainWindow::HandleGoToName(const char* name)
{
	if (!name || !name[0])
		return;

	try
	{
		range r = fDoc->ResolveName(name);
		fSheetView->SetSelection(r.TopLeft());
		if (!(r.TopLeft() == r.BotRight()))
			fSheetView->ExtendSelection(r.BotRight());
	}
	catch (...)
	{
		// Nome non definito: nessuno spostamento, nessun crash.
	}
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
	MarkModified();
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
	MarkModified();

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
		char text[4096];
		fDoc->GetCellFormula(c, text, sizeof(text), false);

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
	char text[4096];
	fDoc->GetCellFormula(sel, text, sizeof(text), false);

	BString original(text);
	if (original.IFindFirst(searchText) < 0)
		return; // la cella selezionata non contiene il testo cercato

	BString replaced = ReplaceAllCaseInsensitive(original, searchText, replaceText);

	fSheetView->SaveUndoState(sel);
	try
	{
		TryToParseString(replaced.String(), sel, fDoc, true);
	}
	catch (...)
	{
	}
	RecalculateActiveWorkbook();
	fSheetView->Invalidate();
	SelectionChanged(sel);
	MarkModified();

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
		char text[4096];
		fDoc->GetCellFormula(c, text, sizeof(text), false);
		if (BString(text).IFindFirst(searchText) >= 0)
			matches.push_back(c);
	}

	if (matches.empty())
		return;

	// Un'istantanea sola per il rettangolo che racchiude tutte le
	// celle toccate (non un intervallo sparso, che SaveUndoState non
	// sa rappresentare): le celle nel mezzo non modificate vengono
	// comunque incluse, ma annullare le riscrive con lo stesso testo
	// che avevano gia', quindi resta corretto anche se non minimale.
	range affected(matches[0].h, matches[0].v, matches[0].h, matches[0].v);
	for (size_t i = 1; i < matches.size(); i++)
	{
		affected.left = std::min(affected.left, matches[i].h);
		affected.right = std::max(affected.right, matches[i].h);
		affected.top = std::min(affected.top, matches[i].v);
		affected.bottom = std::max(affected.bottom, matches[i].v);
	}
	fSheetView->SaveUndoState(affected);

	for (size_t i = 0; i < matches.size(); i++)
	{
		char text[4096];
		fDoc->GetCellFormula(matches[i], text, sizeof(text), false);
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
	RecalculateActiveWorkbook();
	fSheetView->Invalidate();
	SelectionChanged(fSheetView->Selection());
	MarkModified();

	BString msg;
	msg << (int32)matches.size() << " cella/e sostituita/e.";
	BAlert* alert = new BAlert("Sostituisci tutto", msg.String(), "OK");
	alert->Go();
}

void MainWindow::SetCellFormat(int32 format)
{
	if (!fDoc)
		return;

	// Applica a tutte le celle di SelectionRange(), non solo alla
	// cella attiva -- SetCellStyle crea la voce nella mappa interna
	// del documento solo se serve davvero (stile diverso da quello di
	// colonna/predefinito), quindi non "sporca" con voci vuote le
	// celle dell'intervallo che restano senza contenuto.
	range sel = fSheetView->SelectionRange();
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fFormat = format;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
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
	RecalculateActiveWorkbook();
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

	char formula[4096];
	if (fDoc)
		fDoc->GetCellFormula(c, formula, sizeof(formula), false);
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

		case kMsgSwitchSheet:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				SwitchToSheet(index);
			break;
		}

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

		case kMsgUndo:
			fSheetView->Undo();
			break;

		case kMsgRedo:
			fSheetView->Redo();
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

		case kMsgSortAscending:
			fSheetView->SortSelection(true);
			break;

		case kMsgSortDescending:
			fSheetView->SortSelection(false);
			break;

		case kMsgInsertRows:
			fSheetView->InsertRows();
			break;

		case kMsgInsertColumns:
			fSheetView->InsertColumns();
			break;

		case kMsgDeleteRows:
			fSheetView->DeleteRows();
			break;

		case kMsgDeleteColumns:
			fSheetView->DeleteColumns();
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

		case kMsgShowNames:
			ShowNameWindow();
			break;

		case kMsgShowPasteSpecial:
			ShowPasteSpecialWindow();
			break;

		case kMsgPasteSpecialRequest:
		{
			int32 content = 0, operation = 0;
			bool transpose = false;
			message->FindInt32("content", &content);
			message->FindInt32("operation", &operation);
			message->FindBool("transpose", &transpose);
			HandlePasteSpecialRequest(content, operation, transpose);
			break;
		}

		case kMsgShowGoTo:
			ShowGoToWindow();
			break;

		case kMsgToggleFreeze:
			fSheetView->ToggleFreezePanes();
			fFreezeMenuItem->SetMarked(fSheetView->HasFreezePanes());
			break;

		case kMsgToggleBold:
			ToggleBold();
			break;

		case kMsgToggleItalic:
			ToggleItalic();
			break;

		case kMsgSetAlignment:
		{
			int32 alignment;
			if (message->FindInt32("alignment", &alignment) == B_OK)
				SetAlignment((char)alignment);
			break;
		}

		case kMsgShowTextColor:
			ShowColorWindow(false);
			break;

		case kMsgShowBgColor:
			ShowColorWindow(true);
			break;

		case kMsgColorRequest:
		{
			rgb_color* color = NULL;
			ssize_t size = 0;
			bool background = false;
			message->FindBool("background", &background);
			if (message->FindData("color", B_RGB_COLOR_TYPE,
					(const void**)&color, &size) == B_OK && color)
			{
				if (background)
					SetBackgroundColor(*color);
				else
					SetTextColor(*color);
			}
			break;
		}

		case kMsgShowPreferences:
			ShowPreferencesWindow();
			break;

		case kMsgPreferencesRequest:
		{
			bool showGrid = true;
			int8 decimalSep = '.', listSep = ';';
			message->FindBool("showGrid", &showGrid);
			message->FindInt8("decimalSeparator", &decimalSep);
			message->FindInt8("listSeparator", &listSep);
			HandlePreferencesRequest(showGrid, (char)decimalSep, (char)listSep);
			break;
		}

		case kMsgToggleBorder:
		{
			int32 side = 0;
			if (message->FindInt32("side", &side) == B_OK)
				ToggleBorder(side);
			break;
		}

		case kMsgClearBorders:
			ClearBorders();
			break;

		case kMsgGoToRequest:
		{
			BString rangeText;
			if (message->FindString("range", &rangeText) == B_OK)
				HandleGoToRequest(rangeText.String());
			break;
		}

		case kMsgDefineName:
		{
			BString name, rangeText;
			if (message->FindString("name", &name) == B_OK
				&& message->FindString("range", &rangeText) == B_OK)
				HandleDefineName(name.String(), rangeText.String());
			break;
		}

		case kMsgDeleteName:
		{
			BString name;
			if (message->FindString("name", &name) == B_OK)
				HandleDeleteName(name.String());
			break;
		}

		case kMsgGoToName:
		{
			BString name;
			if (message->FindString("name", &name) == B_OK)
				HandleGoToName(name.String());
			break;
		}

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
	if (!ConfirmDiscardChanges())
		return false;

	// Senza B_QUIT_ON_WINDOW_CLOSE (rimosso dal costruttore, vedi sopra)
	// chiudere questa finestra non chiude piu' l'app da solo: tocca a
	// questo hook farlo, ma SOLO quando e' rimasta l'ultima MainWindow.
	// be_app->CountWindows() da solo non basta: conta anche fOpenPanel/
	// fSavePanel (i BFilePanel di Apri/Salva, BWindow a loro volta),
	// quindi resterebbe sempre sopra 1 anche con una sola MainWindow
	// aperta, e l'app non terminerebbe mai chiudendo l'ultima -- bug
	// scoperto scrivendo tests/test_multiwindow.cpp.
	int mainWindows = 0;
	for (int32 i = 0; i < be_app->CountWindows(); i++)
	{
		if (dynamic_cast<MainWindow*>(be_app->WindowAt(i)))
			mainWindows++;
	}
	if (mainWindows <= 1)
		be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}
