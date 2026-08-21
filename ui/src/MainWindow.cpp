/*
	MainWindow.cpp

	Vedi MainWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "MainWindow.h"
#include "SheetView.h"
#include "SheetTabView.h"
#include "ToolbarView.h"
#include "AscdIO.h"
#include "FindWindow.h"
#include "ChartWindow.h"
#include "PivotWindow.h"
#include "NameWindow.h"
#include "PasteSpecialWindow.h"
#include "GoToWindow.h"
#include "RenameSheetWindow.h"
#include "CommentWindow.h"
#include "HyperlinkWindow.h"
#include "ValidationWindow.h"
#include "ConditionalFormatWindow.h"
#include "ColorWindow.h"
#include "PreferencesWindow.h"
#include "BorderWindow.h"
#include "AboutWindow.h"
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
#include <cstdlib>
#include <cstring>
#include <vector>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <Clipboard.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <PrintJob.h>
#include <ScrollView.h>
#include <SeparatorView.h>
#include <SpaceLayoutItem.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslatorRoster.h>
#include <TranslationDefs.h>
#include <Url.h>

#include "CellStyle.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "Formatter.h"
#include "Range.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "MainWindow"

static const uint32 kMsgNew = 'anew';
static const uint32 kMsgOpen = 'aopn';
static const uint32 kMsgOpenRecent = 'aorc';
static const uint32 kMsgSave = 'asve';
static const uint32 kMsgSaveAs = 'asva';
// Voci del sottomenu "Salva con nome" (Fase 21, richiesta esplicita
// dell'utente: "non posso selezionare il formato del file finale"):
// ognuna precompila il pannello di salvataggio con l'estensione giusta
// prima di mostrarlo, cosi' il formato scelto qui e' visibile a colpo
// d'occhio nel nome del file -- SaveToFile continua a scegliere il
// formato dall'estensione digitata (vedi il commento li'), non serve
// nessun campo nuovo per "ricordare" la scelta.
static const uint32 kMsgSaveAsAscd = 'svas';
static const uint32 kMsgSaveAsCsv = 'svcs';
static const uint32 kMsgSaveAsXlsx = 'svxl';
static const uint32 kMsgSaveAsOds = 'svod';
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
// Un solo colpo, poco dopo Show(): vedi MainWindow::Show() per il perche'.
static const uint32 kMsgFixupScrollBars = 'fscb';
static const uint32 kMsgNewSheet = 'nwsh';
static const uint32 kMsgSetFormat = 'stfm';
static const uint32 kMsgShowChart = 'shch';
static const uint32 kMsgShowPivot = 'shpv';
static const uint32 kMsgShowNames = 'shnm';
static const uint32 kMsgShowPasteSpecial = 'shps';
static const uint32 kMsgShowGoTo = 'shgt';
static const uint32 kMsgToggleFreeze = 'frzp';
static const uint32 kMsgToggleBold = 'tbld';
static const uint32 kMsgToggleItalic = 'tita';
static const uint32 kMsgToggleUnderline = 'tund';
static const uint32 kMsgToggleWrapText = 'twrp';
static const uint32 kMsgMergeCells = 'mrgc';
static const uint32 kMsgShowCommentWindow = 'shcw';
static const uint32 kMsgShowHyperlinkWindow = 'shlw';
static const uint32 kMsgShowValidationWindow = 'shvw';
static const uint32 kMsgShowConditionalFormatWindow = 'shcf';
static const uint32 kMsgUnmergeCells = 'umrg';
static const uint32 kMsgSetAlignment = 'algn';
static const uint32 kMsgShowTextColor = 'shtc';
static const uint32 kMsgShowBgColor = 'shbc';
static const uint32 kMsgShowPreferences = 'shpr';
static const uint32 kMsgAutoSaveTick = 'atck';
static const uint32 kMsgToggleBorder = 'tbrd';
static const uint32 kMsgClearBorders = 'cbrd';
static const uint32 kMsgShowBorderColor = 'shbd';
static const uint32 kMsgSetBorderThickness = 'sbth';
static const uint32 kMsgShowBorderWindow = 'shbw';
static const uint32 kMsgFormulaModified = 'afmd';
static const uint32 kMsgToggleFooterStat = 'tfst';

// Footer stile Excel (Fase 17): un bit per statistica (MainWindow::
// FooterStat, nell'header -- serve anche ai test), personalizzabile col
// tasto destro sul footer (vedi FooterStatsView sotto) -- stesso elenco
// e stesso ordine del vero menu contestuale dello status bar di Excel.
// "Conteggio" conta ogni cella non vuota della selezione (anche testo),
// "Conteggio numerico" solo quelle con un valore numerico: distinzione
// reale di Excel, non ridondante (una selezione di sole etichette
// testuali mostra un Conteggio ma nessun Conteggio numerico).
//
// Default = esattamente il default reale di Excel (Media/Conteggio/
// Somma attivi, Conteggio numerico/Minimo/Massimo disattivi finche'
// l'utente non li accende dal menu contestuale).
static const int kDefaultFooterStats =
	MainWindow::kStatAverage | MainWindow::kStatCount | MainWindow::kStatSum;

// Aggiunge "Etichetta: valore" al testo del footer, con "   " come
// separatore fra una statistica e la successiva (solo se il testo non
// e' gia' vuoto) -- stesso identico spaziamento del vecchio testo fisso
// "Somma: %g   Media: %g   ..." che questa funzione sostituisce.
static void AppendFooterStat(BString& text, const char* label, double value)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "%s%s: %g", text.IsEmpty() ? "" : "   ", label, value);
	text << buf;
}

static void AppendFooterStatInt(BString& text, const char* label, int value)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "%s%s: %d", text.IsEmpty() ? "" : "   ", label, value);
	text << buf;
}

// BStringView del footer con le statistiche di selezione: a differenza
// di ClickableStringView (clic sinistro, apre un link) risponde al
// tasto DESTRO con un menu contestuale per accendere/spegnere ogni
// statistica -- esattamente il comportamento del vero status bar di
// Excel (mai fisso, sempre personalizzabile). "fTarget" e' la
// MainWindow proprietaria: unica fonte di verita' per la maschera
// corrente (FooterStatsMask) e destinataria di kMsgToggleFooterStat.
class FooterStatsView : public BStringView {
public:
	FooterStatsView(const char* name, MainWindow* target)
		:
		BStringView(name, ""),
		fTarget(target)
	{
	}

	virtual void MouseDown(BPoint where)
	{
		uint32 buttons = 0;
		if (Window() != NULL && Window()->CurrentMessage() != NULL)
			Window()->CurrentMessage()->FindInt32("buttons", (int32*)&buttons);

		if (!(buttons & B_SECONDARY_MOUSE_BUTTON) || fTarget == NULL)
			return;

		const struct { int bit; const char* label; } kItems[] = {
			{ MainWindow::kStatAverage,  B_TRANSLATE("Media") },
			{ MainWindow::kStatCount,    B_TRANSLATE("Conteggio") },
			{ MainWindow::kStatNumCount, B_TRANSLATE("Conteggio numerico") },
			{ MainWindow::kStatMin,      B_TRANSLATE("Minimo") },
			{ MainWindow::kStatMax,      B_TRANSLATE("Massimo") },
			{ MainWindow::kStatSum,      B_TRANSLATE("Somma") },
		};

		BPopUpMenu* menu = new BPopUpMenu("footerStatsMenu", false, false);
		int mask = fTarget->FooterStatsMask();
		for (size_t i = 0; i < sizeof(kItems) / sizeof(kItems[0]); i++)
		{
			BMessage* itemMsg = new BMessage(kMsgToggleFooterStat);
			itemMsg->AddInt32("bit", kItems[i].bit);
			BMenuItem* item = new BMenuItem(kItems[i].label, itemMsg);
			item->SetMarked((mask & kItems[i].bit) != 0);
			menu->AddItem(item);
		}
		menu->SetTargetForItems(fTarget);

		ConvertToScreen(&where);
		menu->Go(where, true, true, true);
	}

private:
	MainWindow* fTarget;
};

static const uint32 kAtomoNativeFormat = 'ASCD';
static const uint32 kAtomoCsvFormat = 'ACSV';
// Stessi FourCC dei rispettivi translator (vedi translators/xlsx/
// XlsxTranslator.h, translators/ods/OdsTranslator.h) -- ripetuti qui
// come kAtomoCsvFormat sopra, invece di includere l'header privato di
// un translator dalla UI: entrambe le parti concordano solo sul
// valore del FourCC, non condividono altro.
static const uint32 kAtomoXlsxFormat = 'AXSX';
static const uint32 kAtomoOdsFormat = 'AODS';

// Toolbar dinamica: i pulsanti vengono generati da questa tabella
// invece che scritti uno per uno a mano (com'era prima di questo
// gruppo di modifiche, quando l'unica fonte di icone erano gli otto
// glifi disegnati a codice in ToolbarIcons.h/.cpp -- rimossi ora che
// il sito autorizzato per le icone del progetto, www.hvif-store.art,
// e' finalmente popolato, vedi Atomo123_icons/ATOMO123.md per la
// selezione ragionata e IconCatalog.h/IconData.cpp per i byte HVIF
// incorporati). Un gruppo per voce di menu principale a cui i pulsanti
// corrispondono (File/Modifica/Dati/Inserisci/Formato), con un
// separatore verticale fra un gruppo e il successivo in un'unica riga
// -- una vera BToolBar non e' disponibile su questo sistema (quella
// classe vive solo sotto develop/headers/private/shared/, la sua
// implementazione non e' nemmeno presente in libbe.so qui: non solo
// "sconsigliata", proprio non linkabile). Bottoni BButton normali,
// resi piatti con SetFlat() (pubblica, vedi il commento piu' sotto) e
// una riga di separazione orizzontale subito sotto (vedi
// MainWindow::MainWindow) riprendono lo stesso aspetto di una vera
// BToolBar (Terminal, WebPositive...) senza quella classe. Il
// raggruppamento per categoria riprende invece lo stesso principio
// della barra Standard di Excel classico (icone imparentate
// raggruppate, separate da un divisore sottile) chiesto dall'utente.
// Solo funzioni gia' implementate da un comando vero (menu o
// scorciatoia): niente pulsanti per funzioni ancora "da disegnare"
// nel catalogo (bordi cella, unisci celle, inserisci riga/colonna...)
// o non ancora presenti in Atomo123 (filtro, zoom...).
struct ToolbarButtonDef {
	const char* name;
	const char* label;
	uint32 message;
	const IconData* icon;
	// Parametro int32 opzionale nel BMessage (es. "alignment" per i tre
	// pulsanti di allineamento, che condividono lo stesso kMsgSetAlignment
	// e si distinguono solo per questo valore -- vedi i tre BMenuItem
	// equivalenti piu' sotto in questo stesso costruttore). NULL per i
	// pulsanti che non ne hanno bisogno: le voci esistenti sopra questo
	// commento non lo specificano, quindi restano NULL/0 per
	// inizializzazione automatica degli aggregati.
	const char* paramName;
	int32 paramValue;
};

struct ToolbarGroupDef {
	const ToolbarButtonDef* buttons;
	size_t count;
};

// Le etichette qui sotto sono marcate con B_TRANSLATE_MARK (identita' a
// tempo di compilazione, vedi Catalog.h) invece che tradotte sul posto
// con B_TRANSLATE: queste tabelle sono oggetti "static" a livello di
// file, inizializzati PRIMA di main() (quindi prima che BApplication/
// BLocaleRoster esistano) -- chiamare davvero il catalogo qui
// crasherebbe o restituirebbe l'italiano non tradotto. La traduzione
// vera avviene a runtime in BuildToolbar() sotto, nel punto in cui
// def.label viene davvero usato.
static const ToolbarButtonDef kFileToolbarButtons[] = {
	{ "toolNew", B_TRANSLATE_MARK("Nuovo"), kMsgNew, &kIconNew },
	{ "toolOpen", B_TRANSLATE_MARK("Apri"), kMsgOpen, &kIconOpen },
	{ "toolSave", B_TRANSLATE_MARK("Salva"), kMsgSave, &kIconSave },
	{ "toolPrint", B_TRANSLATE_MARK("Stampa"), kMsgPrint, &kIconPrint },
};

static const ToolbarButtonDef kEditToolbarButtons[] = {
	{ "toolUndo", B_TRANSLATE_MARK("Annulla"), kMsgUndo, &kIconUndo },
	{ "toolRedo", B_TRANSLATE_MARK("Ripeti"), kMsgRedo, &kIconRedo },
	{ "toolCut", B_TRANSLATE_MARK("Taglia"), kMsgCut, &kIconCut },
	{ "toolCopy", B_TRANSLATE_MARK("Copia"), kMsgCopy, &kIconCopy },
	{ "toolPaste", B_TRANSLATE_MARK("Incolla"), kMsgPaste, &kIconPaste },
	{ "toolDelete", B_TRANSLATE_MARK("Elimina"), kMsgClear, &kIconDelete },
	{ "toolFind", B_TRANSLATE_MARK("Trova"), kMsgFind, &kIconFind },
};

static const ToolbarButtonDef kDataToolbarButtons[] = {
	{ "toolSortAsc", B_TRANSLATE_MARK("Ordina A-Z"), kMsgSortAscending, &kIconSortAscending },
	{ "toolSortDesc", B_TRANSLATE_MARK("Ordina Z-A"), kMsgSortDescending, &kIconSortDescending },
};

// Vai a/Intervalli con nome (gia' voci di menu) promossi a pulsante ora
// che il catalogo HVIF li copre (compass/tag, vedi IconData.cpp):
// gruppo separato da Ordina A-Z/Z-A qui sopra perche' sono comandi di
// navigazione, non di riordino dati (vedi kToolbarGroups piu' sotto).
static const ToolbarButtonDef kNavigateToolbarButtons[] = {
	{ "toolGoTo", B_TRANSLATE_MARK("Vai a"), kMsgShowGoTo, &kIconGoTo },
	{ "toolNamedRanges", B_TRANSLATE_MARK("Intervalli con nome"), kMsgShowNames, &kIconNamedRange },
};

static const ToolbarButtonDef kInsertToolbarButtons[] = {
	{ "toolChart", B_TRANSLATE_MARK("Grafico"), kMsgShowChart, &kIconChart },
	{ "toolPivot", B_TRANSLATE_MARK("Pivot"), kMsgShowPivot, &kIconTable },
};

// Commento cella/Collegamento ipertestuale (Fase 13, gia' voci del menu
// Formato): promossi a pulsante nello stesso momento dei due sopra,
// stessa ragione (icona HVIF ora disponibile) -- restano un gruppo a
// parte perche' sono annotazioni sulla singola cella attiva, non
// inserimenti di un intero oggetto come grafico/pivot.
static const ToolbarButtonDef kAnnotateToolbarButtons[] = {
	{ "toolHyperlink", B_TRANSLATE_MARK("Collegamento ipertestuale"), kMsgShowHyperlinkWindow, &kIconHyperlink },
	{ "toolComment", B_TRANSLATE_MARK("Commento cella"), kMsgShowCommentWindow, &kIconComment },
};

// Stessi comandi dei BMenuItem del menu Formato piu' sotto in questo
// costruttore (kMsgToggleBold/kMsgToggleItalic/kMsgToggleUnderline/
// kMsgSetAlignment/kMsgToggleWrapText/kMsgShowTextColor/kMsgShowBgColor):
// promossi a pulsante ora che il catalogo HVIF li copre tutti (vedi
// IconData.cpp), rimasti solo voci di menu fino ad ora per questo
// motivo, non per mancanza della funzione stessa.
static const ToolbarButtonDef kFormatToolbarButtons[] = {
	{ "toolBold", B_TRANSLATE_MARK("Grassetto"), kMsgToggleBold, &kIconBold },
	{ "toolItalic", B_TRANSLATE_MARK("Corsivo"), kMsgToggleItalic, &kIconItalic },
	{ "toolUnderline", B_TRANSLATE_MARK("Sottolineato"), kMsgToggleUnderline, &kIconUnderline },
	{ "toolAlignLeft", B_TRANSLATE_MARK("Allinea a sinistra"), kMsgSetAlignment, &kIconAlignLeft, "alignment", eAlignLeft },
	{ "toolAlignCenter", B_TRANSLATE_MARK("Allinea al centro"), kMsgSetAlignment, &kIconAlignCenter, "alignment", eAlignCenter },
	{ "toolAlignRight", B_TRANSLATE_MARK("Allinea a destra"), kMsgSetAlignment, &kIconAlignRight, "alignment", eAlignRight },
	{ "toolWrapText", B_TRANSLATE_MARK("A capo automatico"), kMsgToggleWrapText, &kIconWrapText },
	{ "toolTextColor", B_TRANSLATE_MARK("Colore testo"), kMsgShowTextColor, &kIconTextColor },
	{ "toolHighlight", B_TRANSLATE_MARK("Colore sfondo"), kMsgShowBgColor, &kIconHighlight },
	// Colore bordo (Fase 13): stesso gruppo "Colore" degli altri due
	// sopra, icona a tavolozza (kIconBorderColor) per distinguerla dai
	// pittogrammi "A"/pennello gia' usati da testo/sfondo.
	// Icona di "colore bordo" riusata: apre pero' la finestra completa
	// (lati/spessore/colore insieme), non solo il colore -- vedi
	// BorderWindow.h e il commento su kMsgShowBorderColor piu' sotto.
	{ "toolBorderColor", B_TRANSLATE_MARK("Bordo cella"), kMsgShowBorderWindow, &kIconBorderColor },
};

#define TOOLBAR_GROUP(buttons) { buttons, sizeof(buttons) / sizeof((buttons)[0]) }

// Un unico ToolbarView con tutti i gruppi affiancati in ordine: e' lui
// stesso a mandare a capo un gruppo intero su una nuova riga quando non
// entra piu' nella larghezza disponibile (vedi ToolbarView.h) -- niente
// piu' righe fisse per categoria (provate prima, scartate: un gruppo
// tagliato a meta' fra una riga e la successiva era peggio di una sola
// riga).
static const ToolbarGroupDef kToolbarGroups[] = {
	TOOLBAR_GROUP(kFileToolbarButtons),
	TOOLBAR_GROUP(kEditToolbarButtons),
	TOOLBAR_GROUP(kDataToolbarButtons),
	TOOLBAR_GROUP(kNavigateToolbarButtons),
	TOOLBAR_GROUP(kInsertToolbarButtons),
	TOOLBAR_GROUP(kAnnotateToolbarButtons),
	TOOLBAR_GROUP(kFormatToolbarButtons),
};

#undef TOOLBAR_GROUP

// Costruisce l'intera toolbar dalla tabella sopra: un BButton per voce,
// con la sua icona HVIF (IconCatalog::Render -- SetIcon ne copia i bit
// al suo interno, quindi il BBitmap temporaneo va eliminato subito
// dopo, altrimenti perde solo memoria senza benefici), e un gruppo
// nuovo (AddSeparator) per ogni voce di kToolbarGroups. "target" riceve
// i messaggi di tutti i pulsanti (sempre "this" per MainWindow, passato
// esplicitamente solo per non legare questa funzione libera a una
// particolare istanza).
static BView* BuildToolbar(BHandler* target)
{
	ToolbarView* toolbar = new ToolbarView("toolbar");

	size_t groupCount = sizeof(kToolbarGroups) / sizeof(kToolbarGroups[0]);
	for (size_t g = 0; g < groupCount; g++)
	{
		if (g > 0)
			toolbar->AddSeparator();

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
			BMessage* message = new BMessage(def.message);
			if (def.paramName)
				message->AddInt32(def.paramName, def.paramValue);

			const char* label = B_TRANSLATE(def.label);
			BButton* button = new BButton(def.name, NULL, message);
			button->SetToolTip(label);
			button->SetTarget(target);
			// Bottone piatto (bordo visibile solo al passaggio del mouse/
			// alla pressione, non sempre come un BButton normale): stesso
			// aspetto dei pulsanti di una vera BToolBar (Terminal,
			// WebPositive, Tracker...), quella classe non e' pero'
			// disponibile su questo sistema -- vedi il commento in cima
			// al file. SetFlat() da sola e' pubblica (Button.h) e basta a
			// ottenere lo stesso stile visivo senza la classe intera.
			button->SetFlat(true);

			BBitmap* icon = IconCatalog::Render(*def.icon);
			if (icon)
			{
				button->SetIcon(icon);
				delete icon;
			}

			toolbar->AddButton(button, label);
		}
	}

	return toolbar;
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
	fRecentMenu = NULL; // stesso motivo, azzerato prima di essere creato piu' sotto
	// Letto da gPrefs PRIMA di RebuildRecentMenu() piu' sotto (usa
	// fMaxRecentFiles per decidere quante voci mostrare): gPrefs puo'
	// essere NULL in un test senza una vera App::App(), stesso motivo di
	// LoadRecentFiles/SaveRecentFiles sotto.
	fMaxRecentFiles = gPrefs ? gPrefs->GetPrefInt("maxRecentFiles", 5) : 5;
	if (fMaxRecentFiles < 1)
		fMaxRecentFiles = 1;
	if (fMaxRecentFiles > kMaxRecentFilesLimit)
		fMaxRecentFiles = kMaxRecentFilesLimit;
	// Salvataggio automatico (Fase 23): abilitato ogni 5 minuti per
	// default, richiesta esplicita dell'utente. fAutoSaveRunner parte
	// solo piu' avanti (StartOrUpdateAutoSaveRunner), quando/se
	// fDocumentName smette di essere vuoto.
	fAutoSaveEnabled = gPrefs ? (gPrefs->GetPrefInt("autoSaveEnabled", 1) != 0) : true;
	fAutoSaveIntervalMinutes = gPrefs ? gPrefs->GetPrefInt("autoSaveInterval", 5) : 5;
	if (fAutoSaveIntervalMinutes < 1)
		fAutoSaveIntervalMinutes = 1;
	if (fAutoSaveIntervalMinutes > 120)
		fAutoSaveIntervalMinutes = 120;
	fAutoSaveRunner = NULL;
	fActiveSheetIndex = -1; // ResetWorkbook() sotto lo imposta a 0
	ResetWorkbook("Foglio1");
	fModified = false;
	fEditingCell = false;

	BMenuBar* menuBar = new BMenuBar("menu");
	BMenu* fileMenu = new BMenu(B_TRANSLATE("File"));
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Nuovo"), new BMessage(kMsgNew), 'N'));
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Apri" B_UTF8_ELLIPSIS), new BMessage(kMsgOpen), 'O'));
	fRecentMenu = new BMenu(B_TRANSLATE("Apri recenti"));
	fileMenu->AddItem(new BMenuItem(fRecentMenu));
	RebuildRecentMenu(); // popolato subito, non solo alla prima apertura del menu
	// "Salva" (Fase 22, richiesta esplicita dell'utente: "abbiamo solo
	// Salva con nome") scrive direttamente sul file gia' aperto/salvato
	// in precedenza (fFileDirRef/fDocumentName), senza mostrare nessun
	// pannello -- SOLO se il documento ha gia' un percorso noto; un
	// documento nuovo/mai salvato ricade sullo stesso pannello di "Salva
	// con nome" (nessun percorso da riusare). L'acceleratore 'S' vive
	// qui, non piu' sulla voce del sottomenu sotto.
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Salva"), new BMessage(kMsgSave), 'S'));
	// Sottomenu invece di una singola voce (Fase 21): prima il formato
	// di destinazione si sceglieva solo digitando l'estensione a mano
	// nel pannello di salvataggio, senza nessun indizio che fosse
	// possibile -- ogni voce qui precompila il nome col formato scelto
	// prima di mostrare il pannello (vedi i gestori kMsgSaveAsXxx
	// sotto). Nessun acceleratore su queste voci (ne' prima ne' ora):
	// "Salva con nome" mostra sempre il pannello per scelta, non ha
	// bisogno di una scorciatoia da tastiera dedicata.
	BMenu* saveAsMenu = new BMenu(B_TRANSLATE("Salva con nome" B_UTF8_ELLIPSIS));
	saveAsMenu->AddItem(new BMenuItem(B_TRANSLATE("Formato nativo (.ascd)"),
		new BMessage(kMsgSaveAsAscd)));
	saveAsMenu->AddItem(new BMenuItem(B_TRANSLATE("CSV (.csv)"), new BMessage(kMsgSaveAsCsv)));
	saveAsMenu->AddItem(new BMenuItem(B_TRANSLATE("Excel (.xlsx)"), new BMessage(kMsgSaveAsXlsx)));
	saveAsMenu->AddItem(new BMenuItem(B_TRANSLATE("OpenDocument (.ods)"), new BMessage(kMsgSaveAsOds)));
	fileMenu->AddItem(new BMenuItem(saveAsMenu));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Stampa" B_UTF8_ELLIPSIS), new BMessage(kMsgPrint), 'P'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Preferenze" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowPreferences)));
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Informazioni su Atomo123" B_UTF8_ELLIPSIS),
		new BMessage(B_ABOUT_REQUESTED)));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem(B_TRANSLATE("Esci"), new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(fileMenu);

	// Taglia/copia/incolla passano dal vero BClipboard di sistema (non
	// da un appunti interno all'app): il contenuto della cella (la
	// formula, come mostrata dalla barra formule) diventa testo piano
	// condivisibile anche con altre applicazioni Haiku.
	BMenu* editMenu = new BMenu(B_TRANSLATE("Modifica"));
	// Ctrl+Z/Ctrl+Y come scorciatoie dirette (non solo di menu): a
	// differenza di Ctrl+A/Ctrl+D/Ctrl+End sopra, questi due byte
	// (0x1a e 0x19) non corrispondono a nessun altro tasto speciale
	// gia' gestito da SheetView::HandleKey, quindi non c'e' ambiguita'
	// da aggirare (vedi InterfaceDefs.h: B_SUBSTITUTE = 0x1a per
	// Ctrl+Z, nessun nome dedicato per Ctrl+Y). La scorciatoia di menu
	// basta comunque a farli funzionare, risolta dal BWindow.
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Annulla"), new BMessage(kMsgUndo), 'Z'));
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Ripeti"), new BMessage(kMsgRedo), 'Y'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Taglia"), new BMessage(kMsgCut), 'X'));
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Copia"), new BMessage(kMsgCopy), 'C'));
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Incolla"), new BMessage(kMsgPaste), 'V'));
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Incolla speciale" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowPasteSpecial)));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Cancella"), new BMessage(kMsgClear)));
	editMenu->AddSeparatorItem();
	// Niente scorciatoia Ctrl+A: su Haiku collide col byte generato da
	// Ctrl+Inizio (vedi il commento in SheetView::HandleKey) -- voce
	// di menu soltanto, come gia' fa Sum-It storico per lo stesso
	// comando (menu Modifica, "Select All", senza modificatore).
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Seleziona tutto"), new BMessage(kMsgSelectAll)));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Trova e sostituisci" B_UTF8_ELLIPSIS),
		new BMessage(kMsgFind), 'F'));
	editMenu->AddItem(new BMenuItem(B_TRANSLATE("Vai a" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowGoTo), 'G'));
	menuBar->AddItem(editMenu);

	// Formato numero della cella selezionata: agisce su CellStyle::fFormat
	// (letto da CContainer::GetCellResult/SheetView per la
	// visualizzazione). Valuta e percentuale sfruttano anche le
	// formattazioni dedicate del Locale Kit in SheetView::Draw.
	BMenu* formatMenu = new BMenu(B_TRANSLATE("Formato"));
	BMessage* generalMsg = new BMessage(kMsgSetFormat);
	generalMsg->AddInt32("format", eGeneral);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Generale"), generalMsg));
	BMessage* fixedMsg = new BMessage(kMsgSetFormat);
	fixedMsg->AddInt32("format", eFixed);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Numero"), fixedMsg));
	BMessage* currencyMsg = new BMessage(kMsgSetFormat);
	currencyMsg->AddInt32("format", eCurrency);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Valuta"), currencyMsg));
	BMessage* percentMsg = new BMessage(kMsgSetFormat);
	percentMsg->AddInt32("format", ePercent);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Percentuale"), percentMsg));
	formatMenu->AddSeparatorItem();
	// Grassetto/Corsivo: agiscono su CellStyle::fFont (un indice in
	// gFontSizeTable, mai un flag a parte -- vedi MainWindow::ToggleBold/
	// ToggleItalic). Allinea: CellStyle::fAlignment, letto da
	// SheetView::Draw per posizionare il testo dentro la cella.
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Grassetto"), new BMessage(kMsgToggleBold), 'B'));
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Corsivo"), new BMessage(kMsgToggleItalic), 'I'));
	// Sottolineato (Fase 12): CellStyle::fUnderline, un booleano a
	// parte -- BFont non ha un attributo sottolineato nativo (vedi il
	// commento sul campo in CellStyle.h), quindi non puo' viaggiare
	// nella tripla famiglia/stile/dimensione di fFont come Grassetto/
	// Corsivo sopra.
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Sottolineato"), new BMessage(kMsgToggleUnderline), 'U'));
	formatMenu->AddSeparatorItem();
	BMessage* alignLeftMsg = new BMessage(kMsgSetAlignment);
	alignLeftMsg->AddInt32("alignment", eAlignLeft);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Allinea a sinistra"), alignLeftMsg));
	BMessage* alignCenterMsg = new BMessage(kMsgSetAlignment);
	alignCenterMsg->AddInt32("alignment", eAlignCenter);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Allinea al centro"), alignCenterMsg));
	BMessage* alignRightMsg = new BMessage(kMsgSetAlignment);
	alignRightMsg->AddInt32("alignment", eAlignRight);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Allinea a destra"), alignRightMsg));
	// A capo automatico (Fase 12): CellStyle::fWrapText, stesso
	// principio di ToggleBold/ToggleUnderline sopra (stato letto dalla
	// cella attiva, applicato a tutta la selezione) ma con un effetto
	// collaterale in piu' -- fa anche crescere l'altezza delle righe
	// coinvolte se serve, vedi MainWindow::ToggleWrapText.
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("A capo automatico"), new BMessage(kMsgToggleWrapText)));
	formatMenu->AddSeparatorItem();
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Colore testo" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowTextColor)));
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Colore sfondo" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowBgColor)));
	// Colore del bordo (Fase 13): condiviso da tutti e quattro i lati
	// di una cella (CellStyle::fBorderColor), stesso principio di
	// scope gia' scelto per i grafici -- vedi ROADMAP.md.
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Colore bordo" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowBorderColor)));
	formatMenu->AddSeparatorItem();
	// Bordo cella (Fase 13): lati/spessore/colore insieme, con
	// anteprima -- vedi BorderWindow.h. Le voci sotto (un lato/aspetto
	// alla volta) restano come scorciatoie rapide, non sostituite.
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Bordo cella" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowBorderWindow)));
	formatMenu->AddSeparatorItem();
	// Bordi di cella (Fase 11): un lato alla volta, come Grassetto/
	// Corsivo sopra -- vedi MainWindow::ToggleBorder ("side" nello
	// stesso ordine di CellStyle::fTBorderColor/fLBorderColor/
	// fBBorderColor/fRBorderColor).
	BMessage* topBorderMsg = new BMessage(kMsgToggleBorder);
	topBorderMsg->AddInt32("side", 0);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Bordo superiore"), topBorderMsg));
	BMessage* leftBorderMsg = new BMessage(kMsgToggleBorder);
	leftBorderMsg->AddInt32("side", 1);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Bordo sinistro"), leftBorderMsg));
	BMessage* bottomBorderMsg = new BMessage(kMsgToggleBorder);
	bottomBorderMsg->AddInt32("side", 2);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Bordo inferiore"), bottomBorderMsg));
	BMessage* rightBorderMsg = new BMessage(kMsgToggleBorder);
	rightBorderMsg->AddInt32("side", 3);
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Bordo destro"), rightBorderMsg));
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Nessun bordo"), new BMessage(kMsgClearBorders)));
	// Spessore del bordo (Fase 13): cambia lo spessore dei lati GIA'
	// presenti sulla selezione, non ne attiva di nuovi -- vedi
	// MainWindow::SetBorderThickness. Corrispondenza posizionale con
	// CellStyle::fTBorderColor ecc: 1/2/3 = sottile/medio/spesso.
	BMenu* borderThicknessMenu = new BMenu(B_TRANSLATE("Spessore bordo"));
	BMessage* thinMsg = new BMessage(kMsgSetBorderThickness);
	thinMsg->AddInt32("thickness", 1);
	borderThicknessMenu->AddItem(new BMenuItem(B_TRANSLATE("Sottile"), thinMsg));
	BMessage* mediumMsg = new BMessage(kMsgSetBorderThickness);
	mediumMsg->AddInt32("thickness", 2);
	borderThicknessMenu->AddItem(new BMenuItem(B_TRANSLATE("Medio"), mediumMsg));
	BMessage* thickMsg = new BMessage(kMsgSetBorderThickness);
	thickMsg->AddInt32("thickness", 3);
	borderThicknessMenu->AddItem(new BMenuItem(B_TRANSLATE("Spesso"), thickMsg));
	formatMenu->AddItem(borderThicknessMenu);
	formatMenu->AddSeparatorItem();
	// Celle unite (Fase 12): un rettangolo per foglio (CContainer::
	// AddMergedRange), non un campo per cella -- vedi MainWindow::
	// MergeCells/UnmergeCells.
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Unisci celle"), new BMessage(kMsgMergeCells)));
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Dividi celle"), new BMessage(kMsgUnmergeCells)));
	formatMenu->AddSeparatorItem();
	// Formattazione condizionale VIVA (Fase 13): si applica a tutta la
	// selezione, vedi MainWindow::ApplyConditionalFormatToSelection --
	// questa e' l'unica delle voci "Fase 13" qui sotto che e' davvero
	// formattazione (le altre tre -- Commento cella/Collegamento
	// ipertestuale/Convalida dati -- sono annotazioni o integrita' dei
	// dati, non aspetto visivo: spostate in Inserisci/Dati, stesso
	// posto in cui le mette Excel).
	formatMenu->AddItem(new BMenuItem(B_TRANSLATE("Formattazione condizionale" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowConditionalFormatWindow)));
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
	BMenu* dataMenu = new BMenu(B_TRANSLATE("Dati"));
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Riempi in basso"), new BMessage(kMsgFillDown), 'D'));
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Riempi a destra"), new BMessage(kMsgFillRight), 'R'));
	dataMenu->AddSeparatorItem();
	// Ordina per righe intere, confrontando la colonna piu' a sinistra
	// dell'intervallo selezionato (vedi SheetView::SortSelection).
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Ordina crescente"), new BMessage(kMsgSortAscending)));
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Ordina decrescente"), new BMessage(kMsgSortDescending)));
	dataMenu->AddSeparatorItem();
	// Convalida dati (Fase 13, spostata qui da Formato -- vedi il
	// commento li'): si applica a tutta la selezione, come il vero
	// "Convalida dati" nella scheda Dati di Excel, non solo alla cella
	// attiva -- vedi MainWindow::ApplyValidationToSelection.
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Convalida dati" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowValidationWindow)));
	dataMenu->AddSeparatorItem();
	// Il numero di righe/colonne e il punto vengono dalla selezione
	// corrente (SheetView::SelectionRange()), non da una selezione di
	// intestazione -- Atomo123 non ha ancora un modo per selezionare
	// un'intera riga/colonna cliccando l'intestazione. Quattro voci
	// separate ed esplicite invece dell'unico comando "Inserisci"/
	// "Elimina" di Sum-It storico (che inferiva riga o colonna dal
	// fatto che la selezione coprisse un'intera riga/colonna): senza
	// quel gesto sarebbe ambiguo qui.
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Inserisci riga"), new BMessage(kMsgInsertRows)));
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Inserisci colonna"), new BMessage(kMsgInsertColumns)));
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Elimina riga"), new BMessage(kMsgDeleteRows)));
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Elimina colonna"), new BMessage(kMsgDeleteColumns)));
	dataMenu->AddSeparatorItem();
	// Elimina/Rinomina foglio non compaiono qui apposta: si scelgono
	// col tasto destro sulla scheda del foglio (SheetTabView), dove il
	// foglio bersaglio e' scelto dal clic stesso -- un elenco/indice da
	// scegliere qui sarebbe ridondante e piu' scomodo. "Nuovo foglio"
	// invece non ha un bersaglio da scegliere (aggiunge sempre in
	// coda), quindi qui nel menu ha senso quanto la maniglia "+" di
	// Excel/LibreOffice Calc dopo l'ultima scheda -- non implementata
	// per restare nello scope di questo punto della Fase 13. Ripetuta
	// anche nel menu Inserisci sotto (bug segnalato dall'utente: "su
	// insert non posso inserire un nuovo foglio" -- in Excel "Inserisci
	// foglio" vive li', non insieme a riga/colonna): stesso
	// kMsgNewSheet, nessuna logica duplicata.
	dataMenu->AddItem(new BMenuItem(B_TRANSLATE("Nuovo foglio"), new BMessage(kMsgNewSheet)));
	dataMenu->AddSeparatorItem();
	// Blocca tutto cio' che sta sopra/a sinistra della cella attiva
	// (come Excel): voce con segno di spunta, sincronizzato a ogni
	// attivazione/cambio foglio -- vedi SheetView::ToggleFreezePanes.
	fFreezeMenuItem = new BMenuItem(B_TRANSLATE("Blocca riquadri"), new BMessage(kMsgToggleFreeze));
	dataMenu->AddItem(fFreezeMenuItem);
	menuBar->AddItem(dataMenu);

	// Grafico e tabella pivot leggono un intervallo di due colonne
	// scelto dall'utente (non la sola cella selezionata, che oggi e'
	// l'unica selezione supportata dalla griglia -- vedi
	// docs/UI_ARCHITECTURE.md): l'intervallo si digita nella finestra
	// dedicata, non si trascina sulla griglia.
	BMenu* insertMenu = new BMenu(B_TRANSLATE("Inserisci"));
	// "Nuovo foglio" in cima: stesso comando di kMsgNewSheet gia' nel
	// menu Dati sopra (vedi il commento li'), ripetuto qui perche' e'
	// dove Excel lo mette e dove l'utente lo ha cercato per primo.
	insertMenu->AddItem(new BMenuItem(B_TRANSLATE("Nuovo foglio"), new BMessage(kMsgNewSheet)));
	insertMenu->AddSeparatorItem();
	insertMenu->AddItem(new BMenuItem(B_TRANSLATE("Grafico a barre" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowChart)));
	insertMenu->AddItem(new BMenuItem(B_TRANSLATE("Tabella pivot" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowPivot)));
	insertMenu->AddSeparatorItem();
	// Commento cella/Collegamento ipertestuale (Fase 13, spostate qui
	// da Formato -- vedi il commento li'): entrambe operano sempre
	// sulla sola cella attiva (fSelection), mai sull'intero intervallo
	// selezionato come Taglia/Copia/Incolla/Formato -- un'annotazione o
	// un URL ha senso su UNA cella precisa, non ripetuto su piu' celle
	// in un colpo solo. Stesso posto in cui Excel mette "Collegamento"
	// (scheda Inserisci); "Commento cella" li' vive nella scheda
	// Revisione, non presente qui, Inserisci resta il posto migliore.
	insertMenu->AddItem(new BMenuItem(B_TRANSLATE("Commento cella" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowCommentWindow)));
	insertMenu->AddItem(new BMenuItem(B_TRANSLATE("Collegamento ipertestuale" B_UTF8_ELLIPSIS),
		new BMessage(kMsgShowHyperlinkWindow)));
	insertMenu->AddSeparatorItem();
	insertMenu->AddItem(new BMenuItem(B_TRANSLATE("Intervalli con nome" B_UTF8_ELLIPSIS),
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
	// "Modifica" nel footer appena si digita nella barra formula (senza
	// dover prima fare doppio clic sulla cella) -- BTextControl invia
	// questo messaggio a ogni tasto premuto, a differenza del messaggio
	// normale (solo su Invio/Tab), esattamente per questo scopo.
	fFormulaBar->SetModificationMessage(new BMessage(kMsgFormulaModified));

	fSheetView = new SheetView(fDoc);
	fSheetView->SetCharts(&fCharts);
	fSheetView->SetImages(&fImages);

	// horizontal=false, vertical=false: NON le barre automatiche di
	// BScrollView (usata qui solo per il bordo e il ritaglio). Le due
	// barre create da BScrollView sarebbero SUE figlie private,
	// posizionate in base al Frame() (l'intero canvas virtuale, vedi
	// SheetView::FullCanvasFrame) del bersaglio invece che alla vera
	// area visibile -- bug reale segnalato dall'utente ("non si vedono
	// le barre di scorrimento"): le barre finivano fuori schermo (o,
	// anche riposizionate a mano, sempre RIDISEGNATE SOPRA da
	// SheetView, che le considera comunque dentro al proprio Frame()
	// enorme). Le due barre VERE sono costruite subito sotto, come
	// view indipendenti nel layout della finestra (mai figlie di
	// "scroll"), cosi' l'ordine di disegno fra fratelli le mette
	// correttamente sopra, non sotto, alla griglia.
	BScrollView* scroll = new BScrollView("scroll", fSheetView,
		B_FOLLOW_ALL, 0, false, false);
	scroll->ResizeTo(400, 300);

	// BScrollBar::BScrollBar(name, target, min, max, orientation)
	// registra "fSheetView" come bersaglio indipendentemente da chi
	// sia il genitore nella gerarchia delle view (vedi BView::
	// fVerScroller/fHorScroller) -- SheetView::FixupScrollBars()
	// continua a trovarle tramite ScrollBar(B_HORIZONTAL/VERTICAL)
	// esattamente come se fossero ancora figlie automatiche di una
	// BScrollView.
	fVScrollBar = new BScrollBar("vscroll", fSheetView, 0, 0, B_VERTICAL);
	fHScrollBar = new BScrollBar("hscroll", fSheetView, 0, 0, B_HORIZONTAL);

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

	// Indicatore di modalita' ("Pronto"/"Modifica") a sinistra del
	// footer, come Excel: aggiornato da SetCellMode, chiamata da
	// SheetView::StartEditing/CommitEditing (editing in-cella) e dal
	// messaggio di modifica della barra formula sotto.
	fCellMode = new BStringView("cellMode", B_TRANSLATE("Pronto"));

	// Footer con le statistiche della selezione corrente, come Excel:
	// vuoto con una sola cella selezionata senza valore numerico dentro
	// (o senza contenuto per Conteggio), aggiornato da SelectionChanged
	// sotto insieme a fCellLabel/fFormulaBar. Quali statistiche
	// compaiono e' scelto dall'utente col tasto destro (FooterStatsView
	// sopra), non piu' fisso: fFooterStatsMask persiste in gPrefs.
	fFooterStatsMask = gPrefs ? gPrefs->GetPrefInt("footerStats", kDefaultFooterStats) : kDefaultFooterStats;
	fSelectionStats = new FooterStatsView("selectionStats", this);
	fSelectionStats->SetAlignment(B_ALIGN_RIGHT);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.Add(toolbar)
		// Riga di separazione sotto la toolbar: l'altra meta' dell'aspetto
		// di una vera BToolBar (vedi il commento sopra BuildToolbar), non
		// disponibile su questo sistema.
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(4, 4, 4, 4)
			.Add(fCellLabel)
			.Add(fFormulaBar)
		.End()
		.AddGroup(B_HORIZONTAL, 0)
			.AddGroup(B_VERTICAL, 0)
				.Add(scroll)
				.Add(fHScrollBar)
			.End()
			.Add(fVScrollBar)
		.End()
		.Add(fSheetTabView)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(4, 2, 4, 2)
			.Add(fCellMode)
			.AddGlue()
			.Add(fSelectionStats)
		.End();

	RebuildSheetTabs();

	fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this));
	fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this));
	fFindWindow = NULL;
	fChartWindow = NULL;
	fPivotWindow = NULL;
	fNameWindow = NULL;
	fPasteSpecialWindow = NULL;
	fGoToWindow = NULL;
	fRenameSheetWindow = NULL;
	fCommentWindow = NULL;
	fHyperlinkWindow = NULL;
	fValidationWindow = NULL;
	fConditionalFormatWindow = NULL;
	fColorWindow = NULL;
	fPreferencesWindow = NULL;
	fBorderWindow = NULL;

	UpdateTitle();
}

MainWindow::~MainWindow()
{
	delete fAutoSaveRunner;
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
	if (fRenameSheetWindow)
	{
		fRenameSheetWindow->Lock();
		fRenameSheetWindow->Quit();
	}
	if (fCommentWindow)
	{
		fCommentWindow->Lock();
		fCommentWindow->Quit();
	}
	if (fHyperlinkWindow)
	{
		fHyperlinkWindow->Lock();
		fHyperlinkWindow->Quit();
	}
	if (fValidationWindow)
	{
		fValidationWindow->Lock();
		fValidationWindow->Quit();
	}
	if (fConditionalFormatWindow)
	{
		fConditionalFormatWindow->Lock();
		fConditionalFormatWindow->Quit();
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
	if (fBorderWindow)
	{
		fBorderWindow->Lock();
		fBorderWindow->Quit();
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
	title << (fDocumentName.Length() > 0 ? fDocumentName : BString(B_TRANSLATE("Nuovo documento")));
	title << " \xE2\x80\x94 Atomo123"; // em dash (U+2014) in UTF-8
	SetTitle(title.String());

	// Stesso "* " del titolo, anche nell'indicatore di modalita' del
	// footer (vedi RefreshCellModeText): unico punto di richiamo invece
	// di toccare ognuno dei punti sparsi che scrivono fModified
	// direttamente, perche' UpdateTitle() e' gia' chiamata da tutti
	// loro per il titolo.
	RefreshCellModeText();
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

	BAlert* alert = new BAlert(B_TRANSLATE("Modifiche non salvate"),
		B_TRANSLATE("Le modifiche non salvate andranno perse. Continuare?"),
		B_TRANSLATE_COMMENT("Annulla", "Pulsante di annullamento in una finestra di conferma (\"Cancel\"), non la voce di menu Annulla/Undo"),
		B_TRANSLATE("Continua senza salvare"), NULL,
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
	// Un foglio nuovo parte dalla preferenza generale dell'utente (lo
	// stesso interruttore che SheetView legge nel proprio costruttore
	// per il primissimo foglio all'avvio, vedi SheetView.cpp) -- non
	// piu' l'unica fonte di verita' pero': una volta creato, questo
	// valore vive sul foglio stesso (fSheets[i].showGrid) e puo' essere
	// sovrascritto aprendo un documento che ne porta uno proprio
	// (nativo .ascd o import XLSX).
	sheet.showGrid = gPrefs ? (gPrefs->GetPrefInt("showGrid", 1) != 0) : true;
	fSheets.push_back(sheet);

	fActiveSheetIndex = 0;
	fDoc = fSheets[0].doc;
	fCharts.clear();
	fImages.clear();

	if (fSheetView)
	{
		fSheetView->SetDocument(fDoc);
		fSheetView->SetCharts(&fCharts);
		fSheetView->SetImages(&fImages);
		fSheetView->SetColumnWidths(fSheets[0].colWidths);
		fSheetView->SetRowHeights(fSheets[0].rowHeights);
		// Un foglio nuovo (fSheets[0] appena creato da ResetWorkbook)
		// non ha mai nulla di bloccato: frozenRows/frozenCols sono gia'
		// 0 di default (vedi AscdSheet in AscdIO.h), letti comunque da
		// li' invece di un letterale 0,0 per restare corretto anche se
		// in futuro ResetWorkbook ricevesse fogli non vuoti.
		fSheetView->SetFreezePanes(fSheets[0].frozenRows, fSheets[0].frozenCols);
		fSheetView->SetShowGrid(fSheets[0].showGrid);
		fSheetView->SetHiddenRows(fSheets[0].hiddenRows);
		if (fSheets[0].hasAutoFilter)
			fSheetView->SetAutoFilter(fSheets[0].autoFilterRange);
		else
			fSheetView->ClearAutoFilter();
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

// ISheetResolver (Fase 15): a differenza di ResolveSheetByName sopra
// (cerca per nome DI FOGLIO), qui si cerca per nome DI TABELLA fra le
// tabelle registrate su ciascun foglio (CContainer::GetTables) -- il
// primo foglio che ce l'ha, dato che il nome di una tabella e' unico
// in tutta la cartella di lavoro (come in Excel).
CContainer* MainWindow::FindSheetWithTable(const std::string& tableName)
{
	for (size_t i = 0; i < fSheets.size(); i++)
		if (fSheets[i].doc->GetTables().count(tableName))
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
	fSheets[fActiveSheetIndex].images = fImages;
	fSheets[fActiveSheetIndex].colWidths = fSheetView->CustomColumnWidths();
	fSheets[fActiveSheetIndex].rowHeights = fSheetView->CustomRowHeights();
	fSheets[fActiveSheetIndex].frozenRows = fSheetView->FrozenRows();
	fSheets[fActiveSheetIndex].frozenCols = fSheetView->FrozenCols();
	fSheets[fActiveSheetIndex].showGrid = fSheetView->ShowGrid();
	fSheets[fActiveSheetIndex].hiddenRows = fSheetView->HiddenRows();
	fSheets[fActiveSheetIndex].hasAutoFilter = fSheetView->HasAutoFilter();
	fSheets[fActiveSheetIndex].autoFilterRange = fSheetView->AutoFilterRange();
	// Posizione di scorrimento (bug reale segnalato dall'utente: uscire
	// da un foglio scorso e passare a un altro mostrava lo stesso punto
	// scorso invece dell'angolo in alto a sinistra o di dove l'utente
	// l'aveva lasciato -- un solo SheetView e' condiviso da tutti i
	// fogli, la sua posizione di scorrimento non seguiva mai il foglio
	// da solo). Vedi il commento sul campo in AscdIO.h.
	fSheets[fActiveSheetIndex].scrollPosition = fSheetView->Bounds().LeftTop();

	fActiveSheetIndex = index;
	fDoc = fSheets[index].doc;
	fCharts = fSheets[index].charts;
	fImages = fSheets[index].images;

	fSheetView->SetDocument(fDoc);
	fSheetView->SetColumnWidths(fSheets[index].colWidths);
	fSheetView->SetRowHeights(fSheets[index].rowHeights);
	// Testo a capo (Fase 12): il documento porta gia' con se' quali
	// celle hanno fWrapText (persistito o importato), qui si ricalcola
	// solo l'altezza di riga necessaria a contenerle alla larghezza di
	// colonna corrente di QUESTO foglio.
	fSheetView->RecalculateWrappedRowHeights();
	fSheetView->SetFreezePanes(fSheets[index].frozenRows, fSheets[index].frozenCols);
	fSheetView->SetShowGrid(fSheets[index].showGrid);
	fSheetView->SetHiddenRows(fSheets[index].hiddenRows);
	if (fSheets[index].hasAutoFilter)
		fSheetView->SetAutoFilter(fSheets[index].autoFilterRange);
	else
		fSheetView->ClearAutoFilter();
	fFreezeMenuItem->SetMarked(fSheetView->HasFreezePanes());
	fFormulaBar->SetText("");
	// Ripristina lo scorrimento di QUESTO foglio (catturato sopra
	// l'ultima volta che era attivo, (0,0) se non lo e' mai stato) --
	// dopo tutte le SetXxx sopra, che possono a loro volta scorrere la
	// vista (es. SetFreezePanes/SetColumnWidths che ridisegnano il
	// canvas), cosi' non viene subito sovrascritto.
	fSheetView->ScrollTo(fSheets[index].scrollPosition);
	RebuildSheetTabs();
}

void MainWindow::RebuildSheetTabs()
{
	if (!fSheetTabView)
		return;

	std::vector<BString> names;
	std::vector<bool> hasColor;
	std::vector<rgb_color> colors;
	for (size_t i = 0; i < fSheets.size(); i++)
	{
		names.push_back(fSheets[i].name);
		hasColor.push_back(fSheets[i].hasTabColor);
		colors.push_back(fSheets[i].tabColor);
	}

	fSheetTabView->SetSheets(names, fActiveSheetIndex, &hasColor, &colors);
}

BString MainWindow::UniqueSheetName(const char* prefix) const
{
	// "Foglio1", "Foglio2", ... come Excel/LibreOffice Calc: il primo
	// numero libero, non necessariamente fSheets.size()+1 (un foglio
	// di mezzo puo' essere stato rinominato o eliminato, lasciando
	// "buchi" nella numerazione).
	for (int n = 1; ; n++)
	{
		BString candidate;
		candidate << prefix << n;
		bool taken = false;
		for (size_t i = 0; i < fSheets.size(); i++)
		{
			if (fSheets[i].name == candidate)
			{
				taken = true;
				break;
			}
		}
		if (!taken)
			return candidate;
	}
}

void MainWindow::NewSheet()
{
	AscdSheet sheet;
	sheet.name = UniqueSheetName("Foglio");
	sheet.doc = new CContainer(NULL, NULL);
	sheet.showGrid = gPrefs ? (gPrefs->GetPrefInt("showGrid", 1) != 0) : true;
	fSheets.push_back(sheet);

	// SwitchToSheet sincronizza gia' da sola lo stato UI del foglio
	// PRECEDENTEMENTE attivo in fSheets prima di passare al nuovo,
	// esattamente come un cambio scheda scelto dall'utente -- nessuna
	// duplicazione di quella logica necessaria qui.
	SwitchToSheet((int)fSheets.size() - 1);
	AttachSheetResolver();
	MarkModified();
}

void MainWindow::DeleteSheet(int index)
{
	if (index < 0 || index >= (int)fSheets.size())
		return;

	if (fSheets.size() <= 1)
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Elimina foglio"),
			B_TRANSLATE("Non è possibile eliminare l'unico foglio della cartella di lavoro."),
			B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
		alert->Go();
		return;
	}

	BString msg;
	msg.SetToFormat(B_TRANSLATE("Eliminare il foglio \"%s\"? L'operazione non può essere annullata."),
		fSheets[index].name.String());
	BAlert* alert = new BAlert(B_TRANSLATE("Elimina foglio"), msg.String(),
		B_TRANSLATE_COMMENT("Annulla", "Pulsante di annullamento in una finestra di conferma (\"Cancel\"), non la voce di menu Annulla/Undo"),
		B_TRANSLATE("Elimina"), NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	if (alert->Go() != 1)
		return;

	DeleteSheetNoConfirm(index);
}

void MainWindow::DeleteSheetNoConfirm(int index)
{
	if (index < 0 || index >= (int)fSheets.size() || fSheets.size() <= 1)
		return;

	if (index == fActiveSheetIndex)
	{
		// Passa a un altro foglio PRIMA di eliminare quello attivo,
		// cosi' SwitchToSheet gestisce gia' da sola tutta la
		// sincronizzazione UI -> fSheets (il risultato scritto per
		// l'indice che stiamo per eliminare viene scartato subito
		// dopo, ma e' innocuo): a destra se c'e' un foglio dopo,
		// altrimenti a sinistra (era l'ultimo della lista).
		int target = (index + 1 < (int)fSheets.size()) ? index + 1 : index - 1;
		SwitchToSheet(target);
	}

	fSheets[index].doc->Release();
	fSheets.erase(fSheets.begin() + index);
	// Ogni indice DOPO quello appena eliminato scala di uno per via
	// dell'erase(): se il foglio attivo era dopo, il suo indice va
	// aggiornato di conseguenza (se era proprio quello eliminato, il
	// blocco sopra l'ha gia' spostato altrove PRIMA dell'erase, quindi
	// a questo punto e' sempre != index).
	if (fActiveSheetIndex > index)
		fActiveSheetIndex--;

	RebuildSheetTabs();
	MarkModified();
}

void MainWindow::RenameSheet(int index, const char* newName)
{
	if (index < 0 || index >= (int)fSheets.size() || !newName || !newName[0])
		return;

	// Stesso vincolo di Excel/LibreOffice Calc: due fogli con lo
	// stesso nome nella stessa cartella di lavoro renderebbero
	// ambiguo "NomeFoglio!Cella" (ISheetResolver cerca per nome
	// esatto, vedi ResolveSheetByName sopra).
	for (size_t i = 0; i < fSheets.size(); i++)
	{
		if ((int)i != index && fSheets[i].name == newName)
		{
			BAlert* alert = new BAlert(B_TRANSLATE("Rinomina foglio"),
				B_TRANSLATE("Esiste già un foglio con questo nome."),
				B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT);
			alert->Go();
			return;
		}
	}

	fSheets[index].name = newName;
	RebuildSheetTabs();
	MarkModified();
}

std::vector<BString> MainWindow::LoadRecentFiles()
{
	std::vector<BString> result;
	if (!gPrefs) // vedi il commento sui test senza una vera App::App() altrove in questo file
		return result;

	for (int i = 0; i < kMaxRecentFilesLimit; i++)
	{
		char key[16];
		sprintf(key, "recentFile%d", i);
		BString path(gPrefs->GetPrefString(key, ""));
		if (path.Length() > 0)
			result.push_back(path);
	}
	return result;
}

void MainWindow::SaveRecentFiles(const std::vector<BString>& paths)
{
	if (!gPrefs)
		return;

	for (int i = 0; i < kMaxRecentFilesLimit; i++)
	{
		char key[16];
		sprintf(key, "recentFile%d", i);
		gPrefs->SetPrefString(key, (size_t)i < paths.size() ? paths[i].String() : "");
	}
	try { gPrefs->WritePrefFile(); } catch (...) { /* preferenze non salvabili: non bloccante */ }
}

// Aggiunge/sposta "ref" in cima all'elenco (piu' recente per primo),
// senza duplicati (confronto sul percorso completo, non sul solo nome:
// due file con lo stesso nome in cartelle diverse restano voci
// separate) e troncato a fMaxRecentFiles (scelto dall'utente in
// Preferenze, non piu' un numero fisso).
void MainWindow::AddToRecentFiles(const entry_ref& ref)
{
	BPath path;
	if (BEntry(&ref).GetPath(&path) != B_OK)
		return;

	std::vector<BString> recent = LoadRecentFiles();
	for (size_t i = 0; i < recent.size(); i++)
	{
		if (recent[i] == path.Path())
		{
			recent.erase(recent.begin() + i);
			break;
		}
	}
	recent.insert(recent.begin(), path.Path());
	if (recent.size() > (size_t)fMaxRecentFiles)
		recent.resize(fMaxRecentFiles);

	SaveRecentFiles(recent);
	RebuildRecentMenu();
}

void MainWindow::RebuildRecentMenu()
{
	if (!fRecentMenu)
		return;

	while (BMenuItem* item = fRecentMenu->RemoveItem((int32)0))
		delete item;

	std::vector<BString> recent = LoadRecentFiles();
	// Troncato a fMaxRecentFiles anche qui, non solo in AddToRecentFiles:
	// se l'utente ha appena abbassato il numero in Preferenze, l'elenco
	// gia' salvato su disco puo' averne ancora di piu' finche' non si
	// apre un altro file (che lo tronca da solo sopra) -- qui si vede
	// subito l'effetto della nuova preferenza, senza aspettare quel giro.
	if (recent.size() > (size_t)fMaxRecentFiles)
		recent.resize(fMaxRecentFiles);
	if (recent.empty())
	{
		BMenuItem* none = new BMenuItem(B_TRANSLATE("(nessuno)"), NULL);
		none->SetEnabled(false);
		fRecentMenu->AddItem(none);
		return;
	}

	for (size_t i = 0; i < recent.size(); i++)
	{
		BPath path(recent[i].String());
		BMessage* msg = new BMessage(kMsgOpenRecent);
		msg->AddString("path", recent[i]);
		fRecentMenu->AddItem(new BMenuItem(path.Leaf(), msg));
	}
}

void MainWindow::MenusBeginning()
{
	RebuildRecentMenu();
	BWindow::MenusBeginning();
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
	StopAutoSaveRunner(); // documento nuovo: nessun file dove scrivere un backup
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
	// Tutti gli argomenti opzionali di LoadASCD, non solo i primi tre:
	// mancavano altezza di riga/blocca riquadri/immagini incorporate
	// (aggiunti a LoadASCD in Fase 10/12, mai riportati qui) -- bug
	// reale, non solo teorico per XLS: un file XLSX a un solo foglio
	// con un'immagine incorporata la perdeva silenziosamente allo
	// stesso modo, dato che passa dalla stessa funzione. LoadASCDBook
	// (poco sotto) li passa gia' tutti correttamente.
	status_t err = LoadASCD(source, outSheet->doc, &outSheet->charts, &outSheet->colWidths,
		&outSheet->rowHeights, &outSheet->frozenRows, &outSheet->frozenCols, &outSheet->images,
		&outSheet->showGrid, &outSheet->hasTabColor, &outSheet->tabColor,
		&outSheet->hiddenRows, &outSheet->hasAutoFilter, &outSheet->autoFilterRange);
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
		BAlert* alert = new BAlert(B_TRANSLATE("Errore"),
			B_TRANSLATE("Impossibile aprire il file selezionato."), B_TRANSLATE("OK"));
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
			BAlert* alert = new BAlert(B_TRANSLATE("Errore"),
				B_TRANSLATE("Formato file non riconosciuto da nessun translator installato."),
				B_TRANSLATE("OK"));
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
		BAlert* alert = new BAlert(B_TRANSLATE("Errore"),
			B_TRANSLATE("Il file e' stato tradotto ma i dati risultanti non sono validi."),
			B_TRANSLATE("OK"));
		alert->Go();
		return;
	}

	for (size_t i = 0; i < fSheets.size(); i++)
		fSheets[i].doc->Release();
	fSheets = newSheets;
	fActiveSheetIndex = 0;
	fDoc = fSheets[0].doc;
	fCharts = fSheets[0].charts;
	fImages = fSheets[0].images;
	fSheetView->SetDocument(fDoc);
	fSheetView->SetCharts(&fCharts);
	fSheetView->SetImages(&fImages);
	fSheetView->SetColumnWidths(fSheets[0].colWidths);
	fSheetView->SetRowHeights(fSheets[0].rowHeights);
	// Testo a capo (Fase 12): copre sia i documenti nativi (fWrapText
	// gia' persistito per cella) sia quelli importati da un translator
	// (XLSX imposta fWrapText, ma non puo' calcolare l'altezza senza
	// una view/font vivi -- questa chiamata lo fa qui, con entrambi
	// disponibili).
	fSheetView->RecalculateWrappedRowHeights();
	fSheetView->SetFreezePanes(fSheets[0].frozenRows, fSheets[0].frozenCols);
	fSheetView->SetShowGrid(fSheets[0].showGrid);
	fSheetView->SetHiddenRows(fSheets[0].hiddenRows);
	if (fSheets[0].hasAutoFilter)
		fSheetView->SetAutoFilter(fSheets[0].autoFilterRange);
	else
		fSheetView->ClearAutoFilter();
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
	{
		BEntry openedEntry(&ref);
		BEntry parentEntry;
		if (openedEntry.GetParent(&parentEntry) == B_OK)
			parentEntry.GetRef(&fFileDirRef);
	}
	fModified = false;
	UpdateTitle();
	StartOrUpdateAutoSaveRunner();

	AddToRecentFiles(ref);
}

// Nome senza estensione, usato per precompilare il pannello di
// salvataggio con l'estensione giusta quando si sceglie un formato dal
// sottomenu "Salva con nome" (vedi i gestori kMsgSaveAsXxx sotto) --
// un documento nuovo/mai salvato non ha ancora un nome, ne usa uno
// generico invece di lasciare il campo vuoto.
static BString BaseNameWithoutExtension(const BString& name)
{
	if (name.Length() == 0)
		return BString(B_TRANSLATE("Senza titolo"));
	int32 dot = name.FindLast('.');
	if (dot > 0)
		return BString(name.String(), dot);
	return name;
}

// "Salva" (Fase 22, richiesta esplicita dell'utente: "abbiamo solo
// Salva con nome"): riscrive direttamente lo stesso file gia'
// aperto/salvato in precedenza (fFileDirRef/fDocumentName), senza
// mostrare nessun pannello -- un documento mai salvato (fDocumentName
// vuoto, vedi IsUntouched) non ha ancora nessun file da riscrivere,
// ricade sullo stesso pannello di "Salva con nome".
void MainWindow::Save()
{
	if (fDocumentName.Length() > 0)
		SaveToFile(fFileDirRef, fDocumentName.String());
	else
		fSavePanel->Show();
}

void MainWindow::SaveToFile(const entry_ref& dir, const char* name)
{
	// Il formato di destinazione si sceglie dall'estensione del nome
	// scelto nel BFilePanel (".csv"/.xlsx"/".ods" esportano nel
	// translator corrispondente, qualunque altra estensione o nessuna
	// resta sul formato nativo ASCD) -- il sottomenu "Salva con nome"
	// (Fase 21) precompila gia' l'estensione giusta prima di mostrare
	// il pannello, ma resta comunque possibile digitarne una diversa a
	// mano, che vince sempre su quella precompilata. ".xls" non e'
	// qui apposta: quel translator sa solo importare, mai scrivere
	// (vedi XlsTranslator.cpp) -- ricade sul ramo generico piu' sotto,
	// che mostra l'errore "Nessun translator installato sa esportare
	// in questo formato" quando la Translate fallisce, invece di
	// scrivere silenziosamente byte ASCD sotto un nome ".xls".
	BString nameStr(name);
	uint32 outType = kAtomoNativeFormat;
	int32 csvPos = nameStr.IFindLast(".csv");
	int32 xlsxPos = nameStr.IFindLast(".xlsx");
	int32 odsPos = nameStr.IFindLast(".ods");
	if (csvPos >= 0 && csvPos == nameStr.Length() - 4)
		outType = kAtomoCsvFormat;
	else if (xlsxPos >= 0 && xlsxPos == nameStr.Length() - 5)
		outType = kAtomoXlsxFormat;
	else if (odsPos >= 0 && odsPos == nameStr.Length() - 4)
		outType = kAtomoOdsFormat;

	BDirectory directory(&dir);
	BFile file(&directory, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Errore"), B_TRANSLATE("Impossibile creare il file."), B_TRANSLATE("OK"));
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
		fSheets[fActiveSheetIndex].images = fImages;
		fSheets[fActiveSheetIndex].colWidths = fSheetView->CustomColumnWidths();
		fSheets[fActiveSheetIndex].rowHeights = fSheetView->CustomRowHeights();
		fSheets[fActiveSheetIndex].frozenRows = fSheetView->FrozenRows();
		fSheets[fActiveSheetIndex].frozenCols = fSheetView->FrozenCols();
		fSheets[fActiveSheetIndex].showGrid = fSheetView->ShowGrid();
		fSheets[fActiveSheetIndex].hiddenRows = fSheetView->HiddenRows();
		fSheets[fActiveSheetIndex].hasAutoFilter = fSheetView->HasAutoFilter();
		fSheets[fActiveSheetIndex].autoFilterRange = fSheetView->AutoFilterRange();

		status_t err = SaveASCDBook(fSheets, &file);
		if (err != B_OK)
		{
			BAlert* alert = new BAlert(B_TRANSLATE("Errore"), B_TRANSLATE("Scrittura del file fallita."), B_TRANSLATE("OK"));
			alert->Go();
			return;
		}
		fDocumentName = name;
		fFileDirRef = dir;
		fModified = false;
		UpdateTitle();
		StartOrUpdateAutoSaveRunner();
		return;
	}

	// Per qualunque formato non nativo si passa dal Translation Kit:
	// si serializza prima il documento in ASCD in memoria, poi si
	// lascia che BTranslatorRoster trovi un translator installato che
	// sappia leggere ASCD e scrivere il formato scelto (CSV/XLSX/ODS
	// lo fanno tutti e tre; XLS resta solo import -- vedi il commento
	// su outType sopra -- quindi qui la Translate fallisce per un nome
	// ".xls", mostrando l'errore sotto invece di un'estensione senza
	// il translator giusto).
	BMallocIO ascd;
	status_t err = SaveASCD(fDoc, &ascd);
	if (err != B_OK)
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Errore"), B_TRANSLATE("Serializzazione del documento fallita."), B_TRANSLATE("OK"));
		alert->Go();
		return;
	}

	// Sceglie a mano il translator installato che dichiara outType fra
	// i propri formati di uscita, invece di lasciare che
	// BTranslatorRoster lo indovini da solo (con info=NULL, o anche
	// passando outType come "wantType" a Identify(), che qui non
	// cambia la scelta): un sorgente ASCD generico (non un vero file
	// XLSX/ODS/XLS con una firma distintiva) viene riconosciuto allo
	// stesso identico modo da TUTTI i translator installati (tutti
	// concordano su kAtomoNativeFormat), quindi Identify() ne sceglie
	// sempre lo stesso indipendentemente dal formato di uscita
	// richiesto -- se quello non e' il translator giusto, il suo
	// stesso Translate() rifiuta correttamente l'outType e l'intera
	// chiamata fallisce con B_NO_TRANSLATOR, anche col translator
	// giusto gia' installato. Bug reale scoperto scrivendo il test di
	// questa funzionalita'.
	translator_id chosenId = 0;
	translator_id* allIds = NULL;
	int32 idCount = 0;
	if (BTranslatorRoster::Default()->GetAllTranslators(&allIds, &idCount) == B_OK)
	{
		for (int32 i = 0; i < idCount && chosenId == 0; i++)
		{
			const translation_format* formats = NULL;
			int32 numFormats = 0;
			if (BTranslatorRoster::Default()->GetOutputFormats(allIds[i], &formats, &numFormats)
					!= B_OK)
				continue;
			for (int32 j = 0; j < numFormats; j++)
			{
				if (formats[j].type == outType)
				{
					chosenId = allIds[i];
					break;
				}
			}
		}
	}

	ascd.Seek(0, SEEK_SET);
	if (chosenId != 0)
		err = BTranslatorRoster::Default()->Translate(chosenId, &ascd, NULL, &file, outType);
	else
		err = BTranslatorRoster::Default()->Translate(&ascd, NULL, NULL, &file, outType);
	if (err != B_OK)
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Errore"),
			B_TRANSLATE("Nessun translator installato sa esportare in questo formato."), B_TRANSLATE("OK"));
		alert->Go();
		return;
	}

	fDocumentName = name;
	fFileDirRef = dir;
	fModified = false;
	UpdateTitle();
	StartOrUpdateAutoSaveRunner();
}

// Nome/formato del backup automatico (Fase 23): stessa estensione di
// fDocumentName, con ".bak" in coda -- MAI il nome del file originale
// (richiesta esplicita dell'utente: "come fa AutoCAD", che scrive un
// file di recupero a parte invece di toccare il disegno aperto). Il
// FORMATO del contenuto segue comunque quello del file vero (stessa
// estensione riconosciuta da SaveToFile sopra): un documento aperto
// come .xlsx produce un backup .xlsx.bak scritto dal translator XLSX,
// non un .ascd sotto mentite spoglie.
void MainWindow::AutoSaveBackup()
{
	// Niente da salvare (documento intatto dall'ultimo salvataggio) o
	// nessun file conosciuto (documento mai salvato manualmente): in
	// entrambi i casi StartOrUpdateAutoSaveRunner non avrebbe nemmeno
	// dovuto avviare il timer, ma il controllo resta qui per sicurezza
	// (es. l'utente disabilita "Salva" mentre il timer e' gia' armato).
	if (!fModified || fDocumentName.Length() == 0)
		return;

	BString nameStr(fDocumentName);
	uint32 outType = kAtomoNativeFormat;
	int32 csvPos = nameStr.IFindLast(".csv");
	int32 xlsxPos = nameStr.IFindLast(".xlsx");
	int32 odsPos = nameStr.IFindLast(".ods");
	if (csvPos >= 0 && csvPos == nameStr.Length() - 4)
		outType = kAtomoCsvFormat;
	else if (xlsxPos >= 0 && xlsxPos == nameStr.Length() - 5)
		outType = kAtomoXlsxFormat;
	else if (odsPos >= 0 && odsPos == nameStr.Length() - 4)
		outType = kAtomoOdsFormat;

	BString backupName(fDocumentName);
	backupName << ".bak";

	BDirectory directory(&fFileDirRef);
	BFile file(&directory, backupName.String(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return; // silenzioso: un salvataggio automatico non deve mai interrompere l'utente con un BAlert

	if (outType == kAtomoNativeFormat)
	{
		fSheets[fActiveSheetIndex].charts = fCharts;
		fSheets[fActiveSheetIndex].images = fImages;
		fSheets[fActiveSheetIndex].colWidths = fSheetView->CustomColumnWidths();
		fSheets[fActiveSheetIndex].rowHeights = fSheetView->CustomRowHeights();
		fSheets[fActiveSheetIndex].frozenRows = fSheetView->FrozenRows();
		fSheets[fActiveSheetIndex].frozenCols = fSheetView->FrozenCols();
		fSheets[fActiveSheetIndex].showGrid = fSheetView->ShowGrid();
		fSheets[fActiveSheetIndex].hiddenRows = fSheetView->HiddenRows();
		fSheets[fActiveSheetIndex].hasAutoFilter = fSheetView->HasAutoFilter();
		fSheets[fActiveSheetIndex].autoFilterRange = fSheetView->AutoFilterRange();
		SaveASCDBook(fSheets, &file); // esito ignorato, vedi il commento sopra su file.InitCheck()
		return;
	}

	BMallocIO ascd;
	if (SaveASCD(fDoc, &ascd) != B_OK)
		return;

	translator_id chosenId = 0;
	translator_id* allIds = NULL;
	int32 idCount = 0;
	if (BTranslatorRoster::Default()->GetAllTranslators(&allIds, &idCount) == B_OK)
	{
		for (int32 i = 0; i < idCount && chosenId == 0; i++)
		{
			const translation_format* formats = NULL;
			int32 numFormats = 0;
			if (BTranslatorRoster::Default()->GetOutputFormats(allIds[i], &formats, &numFormats) != B_OK)
				continue;
			for (int32 j = 0; j < numFormats; j++)
			{
				if (formats[j].type == outType)
				{
					chosenId = allIds[i];
					break;
				}
			}
		}
	}

	ascd.Seek(0, SEEK_SET);
	if (chosenId != 0)
		BTranslatorRoster::Default()->Translate(chosenId, &ascd, NULL, &file, outType);
	else
		BTranslatorRoster::Default()->Translate(&ascd, NULL, NULL, &file, outType);
}

// Avvia (o riarma con un intervallo nuovo) il timer del salvataggio
// automatico -- SOLO se abilitato nelle preferenze E il documento ha
// gia' un file noto (fDocumentName non vuoto): un documento nuovo/mai
// salvato non ha ancora nessun posto dove scrivere un backup, vedi
// AutoSaveBackup sopra. Chiamata da OpenFile/SaveToFile (ogni volta
// che fDocumentName diventa valido) e da HandlePreferencesRequest
// (per applicare subito una modifica alle preferenze).
void MainWindow::StartOrUpdateAutoSaveRunner()
{
	if (!fAutoSaveEnabled || fDocumentName.Length() == 0)
	{
		StopAutoSaveRunner();
		return;
	}

	bigtime_t interval = (bigtime_t)fAutoSaveIntervalMinutes * 60 * 1000000;
	if (fAutoSaveRunner)
	{
		fAutoSaveRunner->SetInterval(interval);
		return;
	}

	fAutoSaveRunner = new BMessageRunner(BMessenger(this), new BMessage(kMsgAutoSaveTick), interval);
}

void MainWindow::StopAutoSaveRunner()
{
	delete fAutoSaveRunner;
	fAutoSaveRunner = NULL;
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
					bool divByZero = false;
					switch (operation)
					{
						case 1: result = destValue + srcValue; break;
						case 2: result = destValue - srcValue; break;
						case 3: result = destValue * srcValue; break;
						case 4:
							if (srcValue != 0.0)
								result = destValue / srcValue;
							else
								divByZero = true;
							break;
					}

					// Divisione per zero: come una formula =A1/0, la cella
					// mostra l'errore #DIV/0! (gDivNan) invece di scrivere
					// silenziosamente 0 -- bug reale trovato nell'audit dei
					// comandi (Fase 15).
					fDoc->NewCell(dest, divByZero ? Value(gDivNan) : Value(result), NULL);
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

	// Se il foglio ha gia' una selezione di piu' di una cella al
	// momento dell'apertura, precompila l'intervallo del grafico con
	// quella selezione e disegna subito l'anteprima (Fase 18) invece
	// di lasciare sempre il valore fisso di default -- l'utente puo'
	// selezionare i dati PRIMA di aprire Grafico. Una singola cella
	// selezionata (il caso comune quando non si e' scelto apposta un
	// intervallo) non sovrascrive il campo: non sarebbe un intervallo
	// utile per un grafico.
	range sel = fSheetView->SelectionRange();
	if (sel.left != sel.right || sel.top != sel.bottom)
	{
		char rangeText[32];
		FormatRangeRef(sel, rangeText, sizeof(rangeText));
		fChartWindow->LoadRange(rangeText);
	}

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
	// fNameWindow e' una BWindow a se', con un thread/BLooper proprio
	// (non lo stesso di MainWindow): toccare una sua BView (SetNames
	// aggiorna la BListView interna) da qui senza il suo lock viola le
	// regole di threading di Haiku -- bug reale scoperto da un crash
	// realmente riprodotto dall'utente sullo stesso schema in
	// ShowColorWindow() sotto (vedi il commento li'), poi trovato
	// replicato anche qui.
	if (fNameWindow->Lock())
	{
		fNameWindow->SetNames(names, ranges);
		fNameWindow->Unlock();
	}
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

void MainWindow::ShowColorWindow(ColorTarget target)
{
	if (!fColorWindow)
		fColorWindow = new ColorWindow(BMessenger(this));

	CellStyle cs;
	if (fDoc)
		fDoc->GetCellStyle(fSheetView->Selection(), cs);

	rgb_color initial;
	switch (target)
	{
		case eBackgroundColor: initial = cs.fLowColor; break;
		case eBorderColor: initial = cs.fBorderColor; break;
		default: initial = cs.fHighColor; break;
	}

	// fColorWindow e' una BWindow a se' (thread/BLooper proprio, non
	// quello di MainWindow): SetMode() tocca fColorControl, una sua
	// BView (BColorControl::SetValue chiama Invalidate()), quindi va
	// fatto col suo lock preso -- bug reale, crash riprodotto
	// dall'utente ("Looper must be locked", vedi il report di crash):
	// mentre la ColorWindow gia' aperta ha un thread proprio attivo,
	// chiamare un suo metodo da qui senza Lock() e' esattamente la
	// corsa che Haiku intercetta e blocca in debug.
	if (fColorWindow->Lock())
	{
		fColorWindow->SetMode(target, initial);
		fColorWindow->Unlock();
	}

	if (fColorWindow->IsHidden())
		fColorWindow->Show();
	fColorWindow->Activate();
}

void MainWindow::ShowBorderWindow()
{
	if (!fBorderWindow)
		fBorderWindow = new BorderWindow(BMessenger(this));

	CellStyle cs;
	if (fDoc)
		fDoc->GetCellStyle(fSheetView->Selection(), cs);

	// Spessore da mostrare in anteprima: il primo lato gia' presente
	// (se nessuno ce l'ha, 1/Sottile come punto di partenza normale).
	int thickness = 1;
	if (cs.fTBorderColor) thickness = cs.fTBorderColor;
	else if (cs.fLBorderColor) thickness = cs.fLBorderColor;
	else if (cs.fBBorderColor) thickness = cs.fBBorderColor;
	else if (cs.fRBorderColor) thickness = cs.fRBorderColor;

	// Stesso motivo di fNameWindow/fColorWindow sopra: SetInitial tocca
	// le BView interne di BorderWindow, che vive sul proprio thread.
	if (fBorderWindow->Lock())
	{
		fBorderWindow->SetInitial(cs.fTBorderColor != 0, cs.fLBorderColor != 0,
			cs.fBBorderColor != 0, cs.fRBorderColor != 0, thickness, cs.fBorderColor);
		fBorderWindow->Unlock();
	}

	if (fBorderWindow->IsHidden())
		fBorderWindow->Show();
	fBorderWindow->Activate();
}

void MainWindow::ShowPreferencesWindow()
{
	if (!fPreferencesWindow)
		fPreferencesWindow = new PreferencesWindow(BMessenger(this));

	// Stesso motivo di fNameWindow/fColorWindow sopra: SetValues tocca
	// le BView interne di PreferencesWindow, che vive sul proprio
	// thread.
	if (fPreferencesWindow->Lock())
	{
		bool showSplash = gPrefs ? (gPrefs->GetPrefInt("showSplash", 1) != 0) : true;
		fPreferencesWindow->SetValues(fSheetView->ShowGrid(), gDecimalPoint, gListSeparator,
			fMaxRecentFiles, showSplash, gThousandSeparator, gCurrencySymbol,
			fAutoSaveEnabled, fAutoSaveIntervalMinutes);
		fPreferencesWindow->Unlock();
	}

	if (fPreferencesWindow->IsHidden())
		fPreferencesWindow->Show();
	fPreferencesWindow->Activate();
}

void MainWindow::HandlePreferencesRequest(bool showGrid, char decimalSep, char listSep,
	int maxRecentFiles, bool showSplash, char thousandSep, const char* currencySymbol,
	bool autoSaveEnabled, int autoSaveIntervalMinutes)
{
	fSheetView->SetShowGrid(showGrid);
	// showGrid e' ora un attributo per-foglio (vedi AscdSheet::showGrid
	// in AscdIO.h): va scritto anche sul foglio attivo, non solo sulla
	// SheetView in vista, altrimenti la scelta si perderebbe al primo
	// cambio scheda (SwitchToSheet risincronizza da fSheets, non da
	// SheetView). gPrefs resta comunque aggiornato sotto: serve come
	// preferenza di default per i PROSSIMI fogli nuovi (ResetWorkbook),
	// non piu' come unica fonte di verita' per quelli gia' aperti.
	if (fActiveSheetIndex >= 0 && fActiveSheetIndex < (int)fSheets.size())
		fSheets[fActiveSheetIndex].showGrid = showGrid;
	gDecimalPoint = decimalSep;
	gListSeparator = listSep;
	gThousandSeparator = thousandSep;
	if (currencySymbol && currencySymbol[0])
		strlcpy(gCurrencySymbol, currencySymbol, 32); // vedi il commento in App::App()

	if (maxRecentFiles < 1)
		maxRecentFiles = 1;
	if (maxRecentFiles > kMaxRecentFilesLimit)
		maxRecentFiles = kMaxRecentFilesLimit;
	fMaxRecentFiles = maxRecentFiles;
	// Rifa' subito l'elenco visibile: se l'utente ha appena abbassato il
	// numero, RebuildRecentMenu tronca la visualizzazione da solo (vedi
	// il commento li'), ma i file "in eccesso" restano su disco finche'
	// non si apre un altro file -- non serve troncarli qui, solo
	// rinfrescare il menu con la nuova preferenza appena impostata.
	RebuildRecentMenu();

	if (autoSaveIntervalMinutes < 1)
		autoSaveIntervalMinutes = 1;
	if (autoSaveIntervalMinutes > 120)
		autoSaveIntervalMinutes = 120;
	fAutoSaveEnabled = autoSaveEnabled;
	fAutoSaveIntervalMinutes = autoSaveIntervalMinutes;
	// Applica subito: arma/riarma il timer se appena abilitato o se
	// l'intervallo e' cambiato, oppure lo ferma se appena disabilitato
	// (StartOrUpdateAutoSaveRunner gestisce entrambi i casi da sola).
	StartOrUpdateAutoSaveRunner();

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
		gPrefs->SetPrefInt("maxRecentFiles", fMaxRecentFiles);
		char thousandStr[2] = { thousandSep, 0 };
		gPrefs->SetPrefString("thousandSeparator", thousandStr);
		gPrefs->SetPrefString("currencySymbol", gCurrencySymbol);
		// Letta solo da App::ReadyToRun al prossimo avvio (vedi il
		// commento li'): non c'e' nessuno stato "vivo" da aggiornare qui
		// nella finestra corrente, a differenza di showGrid/separatori/
		// file recenti sopra.
		gPrefs->SetPrefInt("showSplash", showSplash ? 1 : 0);
		gPrefs->SetPrefInt("autoSaveEnabled", fAutoSaveEnabled ? 1 : 0);
		gPrefs->SetPrefInt("autoSaveInterval", fAutoSaveIntervalMinutes);
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

	// Bug reale segnalato dall'utente: "Annulla" non toccava affatto la
	// formattazione, solo i valori delle celle -- SaveUndoState cattura
	// ora anche CellStyle (vedi SheetView::UndoCellSnapshot), qui come
	// nelle altre funzioni di formattazione sotto.
	fSheetView->SaveUndoState(fSheetView->SelectionRange());

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

	// Stesso motivo di ToggleBold sopra.
	fSheetView->SaveUndoState(fSheetView->SelectionRange());

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
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
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
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
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
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
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
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
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
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
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

void MainWindow::SetBorderThickness(uchar thickness)
{
	if (!fDoc)
		return;

	// Solo i lati GIA' presenti cambiano spessore -- un lato a 0
	// (nessun bordo) resta a 0: cambiare spessore non e' lo stesso di
	// attivare un lato che non c'era (quello resta compito di
	// ToggleBorder).
	range sel = fSheetView->SelectionRange();
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			bool changed = false;
			if (cs.fTBorderColor) { cs.fTBorderColor = thickness; changed = true; }
			if (cs.fLBorderColor) { cs.fLBorderColor = thickness; changed = true; }
			if (cs.fBBorderColor) { cs.fBBorderColor = thickness; changed = true; }
			if (cs.fRBorderColor) { cs.fRBorderColor = thickness; changed = true; }
			if (changed)
				fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::SetBorderColor(rgb_color color)
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fBorderColor = color;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::HandleBorderFormatRequest(bool top, bool left, bool bottom, bool right,
	int thickness, rgb_color color)
{
	if (!fDoc)
		return;

	if (thickness < 1)
		thickness = 1;
	if (thickness > 3)
		thickness = 3;

	// A differenza di ToggleBorder/SetBorderThickness/SetBorderColor
	// sopra (un lato/aspetto alla volta, alternato rispetto allo stato
	// attuale), qui i quattro lati vengono SOSTITUITI dallo stato
	// scelto in BorderWindow -- un solo passo di Annulla per l'intera
	// scelta, non quattro.
	range sel = fSheetView->SelectionRange();
	fSheetView->SaveUndoState(sel);
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fTBorderColor = top ? thickness : 0;
			cs.fLBorderColor = left ? thickness : 0;
			cs.fBBorderColor = bottom ? thickness : 0;
			cs.fRBorderColor = right ? thickness : 0;
			cs.fBorderColor = color;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

// Sottolineato (Fase 12): un booleano semplice come i quattro lati dei
// bordi sopra (BFont non ha un attributo sottolineato nativo, vedi il
// commento su CellStyle::fUnderline), stesso principio "stato letto
// dalla sola cella attiva, applicato all'intera selezione" di
// Grassetto/Corsivo/Bordo.
void MainWindow::ToggleUnderline()
{
	if (!fDoc)
		return;

	CellStyle activeStyle;
	fDoc->GetCellStyle(fSheetView->Selection(), activeStyle);
	bool newValue = !activeStyle.fUnderline;

	range sel = fSheetView->SelectionRange();
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fUnderline = newValue;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->Invalidate();
	MarkModified();
}

// A capo automatico (Fase 12): stesso principio di ToggleUnderline
// sopra, ma dopo aver applicato il campo alla selezione serve anche
// far crescere l'altezza delle righe coinvolte se il testo non
// entrerebbe piu' su una riga sola alla larghezza di colonna
// corrente -- RecalculateWrappedRowHeights (SheetView) non riduce mai
// una riga, quindi disattivare l'a capo automatico non restringe le
// righe gia' allargate (nessun modo semplice di sapere se erano state
// allargate per quel motivo o a mano dall'utente).
void MainWindow::ToggleWrapText()
{
	if (!fDoc)
		return;

	CellStyle activeStyle;
	fDoc->GetCellStyle(fSheetView->Selection(), activeStyle);
	bool newValue = !activeStyle.fWrapText;

	range sel = fSheetView->SelectionRange();
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			fDoc->GetCellStyle(c, cs);
			cs.fWrapText = newValue;
			fDoc->SetCellStyle(c, cs);
		}
	fSheetView->RecalculateWrappedRowHeights();
	fSheetView->Invalidate();
	MarkModified();
}

// Rimuove dall'elenco gli intervalli uniti che si sovrappongono a
// "sel", restituendo quelli che restano -- usata sia da MergeCells
// (per non lasciare due intervalli sovrapposti, ambigui sia per il
// disegno che per GetMergedRange) sia da UnmergeCells.
static std::vector<range> RemoveOverlappingMerges(const std::vector<range>& existing, range sel)
{
	std::vector<range> result;
	for (size_t i = 0; i < existing.size(); i++)
	{
		bool overlaps = existing[i].left <= sel.right && existing[i].right >= sel.left
			&& existing[i].top <= sel.bottom && existing[i].bottom >= sel.top;
		if (!overlaps)
			result.push_back(existing[i]);
	}
	return result;
}

// Celle unite (Fase 12): un rettangolo per foglio (CContainer::
// AddMergedRange), non un campo per cella -- una selezione di una
// sola cella non fa nulla (niente da unire). A differenza di Excel
// vero, il contenuto delle celle diverse da quella in alto a sinistra
// NON viene cancellato (solo nascosto dal disegno, vedi SheetView::
// DrawCellBand): scelta deliberata per restare non distruttiva senza
// una finestra di conferma dedicata -- Dividi celle lo farebbe
// ricomparire.
//
// Non ancora annullabile con Annulla/Ripeti (a differenza delle
// funzioni di formattazione sopra, corrette per lo stesso motivo):
// GetMergedRanges/AddMergedRange sono per foglio, non per cella, e non
// rientrano nel formato di UndoSnapshot (SheetView.h) -- includerli
// li' richiederebbe salvare l'intero elenco a ogni istantanea, con il
// rischio concreto di annullare anche unioni successive scorrelate
// quando si annulla un'altra modifica nel frattempo. Limite noto,
// lasciato esplicito qui invece di un mezzo fix silenzioso.
void MainWindow::MergeCells()
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	if (sel.left == sel.right && sel.top == sel.bottom)
		return;

	std::vector<range> kept = RemoveOverlappingMerges(fDoc->GetMergedRanges(), sel);
	fDoc->ClearMergedRanges();
	for (size_t i = 0; i < kept.size(); i++)
		fDoc->AddMergedRange(kept[i]);
	fDoc->AddMergedRange(sel);

	fSheetView->RecalculateWrappedRowHeights();
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::UnmergeCells()
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	std::vector<range> kept = RemoveOverlappingMerges(fDoc->GetMergedRanges(), sel);
	fDoc->ClearMergedRanges();
	for (size_t i = 0; i < kept.size(); i++)
		fDoc->AddMergedRange(kept[i]);

	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::SetCellComment(int row, int col, const char* text)
{
	if (!fDoc)
		return;
	fDoc->SetComment(cell(col, row), text ? text : "");
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::RemoveCellComment(int row, int col)
{
	if (!fDoc)
		return;
	fDoc->SetComment(cell(col, row), ""); // stringa vuota = nessun commento, vedi CContainer::SetComment
	fSheetView->Invalidate();
	MarkModified();
}

BString MainWindow::CellComment(int row, int col) const
{
	if (!fDoc)
		return BString();
	return BString(fDoc->GetComment(cell(col, row)).c_str());
}

void MainWindow::SetCellHyperlink(int row, int col, const char* url)
{
	if (!fDoc)
		return;
	fDoc->SetHyperlink(cell(col, row), url ? url : "");
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::RemoveCellHyperlink(int row, int col)
{
	if (!fDoc)
		return;
	fDoc->SetHyperlink(cell(col, row), ""); // stringa vuota = nessun collegamento, vedi CContainer::SetHyperlink
	fSheetView->Invalidate();
	MarkModified();
}

BString MainWindow::CellHyperlink(int row, int col) const
{
	if (!fDoc)
		return BString();
	return BString(fDoc->GetHyperlink(cell(col, row)).c_str());
}

void MainWindow::OpenCellHyperlink(int row, int col)
{
	if (!fDoc)
		return;
	BString url = CellHyperlink(row, col);
	if (url.Length() == 0)
		return;
	// Non testabile in automatico: lancia davvero l'applicazione
	// preferita per l'URL (browser, client di posta per "mailto:",
	// ecc.), stesso limite gia' noto di DeleteSheet/RenameSheet con un
	// vero BAlert -- i test verificano solo Set/Remove/CellHyperlink.
	BUrl bUrl(url.String(), false);
	bUrl.OpenWithPreferredApplication(false);
}

void MainWindow::SetCellValidation(int row, int col, int type, const char* list,
	double min, double max)
{
	if (!fDoc)
		return;
	ValidationRule rule;
	rule.type = (type >= 0 && type <= 2) ? (ValidationType)type : eNoValidation;
	rule.list = list ? list : "";
	rule.min = min;
	rule.max = max;
	fDoc->SetValidation(cell(col, row), rule);
	MarkModified();
}

void MainWindow::RemoveCellValidation(int row, int col)
{
	if (!fDoc)
		return;
	fDoc->SetValidation(cell(col, row), ValidationRule()); // eNoValidation = nessuna regola, vedi CContainer::SetValidation
	MarkModified();
}

ValidationRule MainWindow::CellValidation(int row, int col) const
{
	if (!fDoc)
		return ValidationRule();
	return fDoc->GetValidation(cell(col, row));
}

// Confronto testuale esatto (case-sensitive) con uno dei valori
// dell'elenco separati da virgola -- stessa semplicita' di scope gia'
// scelta per l'intervallo numerico sotto (niente jolly/espressioni
// regolari, niente elenco preso da un intervallo di celle come
// permette anche il vero Excel).
bool MainWindow::ValidateCellValue(int row, int col, const char* text, BString* errorMessage) const
{
	if (!fDoc)
		return true;

	ValidationRule rule = fDoc->GetValidation(cell(col, row));

	if (rule.type == eListValidation)
	{
		BString candidates(rule.list.c_str());
		BString value(text);
		int32 start = 0;
		while (start <= candidates.Length())
		{
			int32 comma = candidates.FindFirst(',', start);
			BString piece;
			if (comma < 0)
			{
				candidates.CopyInto(piece, start, candidates.Length() - start);
				start = candidates.Length() + 1;
			}
			else
			{
				candidates.CopyInto(piece, start, comma - start);
				start = comma + 1;
			}
			piece.Trim();
			if (piece == value)
				return true;
		}
		if (errorMessage)
		{
			errorMessage->SetToFormat(B_TRANSLATE("\"%s\" non è uno dei valori ammessi:\n%s"),
				text, rule.list.c_str());
		}
		return false;
	}

	if (rule.type == eNumberRangeValidation)
	{
		char* end = NULL;
		double v = strtod(text, &end);
		bool isNumber = end != text && *end == 0;
		if (!isNumber || v < rule.min || v > rule.max)
		{
			if (errorMessage)
			{
				char buf[128];
				snprintf(buf, sizeof(buf),
					B_TRANSLATE("\"%s\" non è un numero compreso fra %g e %g."), text, rule.min, rule.max);
				errorMessage->SetTo(buf);
			}
			return false;
		}
		return true;
	}

	return true; // eNoValidation
}

void MainWindow::ApplyValidationToSelection(int type, const char* list, double min, double max)
{
	if (!fDoc)
		return;

	ValidationRule rule;
	rule.type = (type >= 0 && type <= 2) ? (ValidationType)type : eNoValidation;
	rule.list = list ? list : "";
	rule.min = min;
	rule.max = max;

	range sel = fSheetView->SelectionRange();
	fSheetView->SaveValidationUndoState(sel);
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
			fDoc->SetValidation(cell(col, row), rule);

	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::RemoveValidationFromSelection()
{
	if (!fDoc)
		return;

	range sel = fSheetView->SelectionRange();
	fSheetView->SaveValidationUndoState(sel);
	for (int row = sel.top; row <= sel.bottom; row++)
		for (int col = sel.left; col <= sel.right; col++)
			fDoc->SetValidation(cell(col, row), ValidationRule());

	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::ApplyConditionalFormatToSelection(int type, const char* value, rgb_color color)
{
	if (!fDoc)
		return;

	fSheetView->SaveCondFormatUndoState();
	ConditionalFormatRule rule;
	rule.type = (type == 1) ? eCondDuplicateValues : eCondCellIsEqual;
	rule.compareValue = value ? value : "";
	rule.bgColor = color;
	rule.ranges.push_back(fSheetView->SelectionRange());
	fDoc->AddConditionalFormatRule(rule);

	// A differenza di convalida dati/bordi/colori sopra, non c'e'
	// nessun CellStyle da scrivere: la regola stessa e' il dato, il
	// colore che ne risulta si ricalcola da solo a ogni ridisegno
	// (vedi SheetView::Draw/CContainer::EvaluateConditionalFormatting)
	// -- Invalidate basta, non serve toccare nessuna cella.
	fSheetView->Invalidate();
	MarkModified();
}

void MainWindow::RemoveAllConditionalFormatRules()
{
	if (!fDoc)
		return;

	fSheetView->SaveCondFormatUndoState();
	fDoc->ClearConditionalFormatRules();
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
	if (!ParseRangeRef(rangeText, r))
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Grafico"),
			B_TRANSLATE("Intervallo non valido: serve esattamente due colonne "
				"(etichette, valori) con almeno una riga numerica, es. A1:B5."),
			B_TRANSLATE("OK"));
		alert->Go();
		return;
	}

	// Un intervallo con piu' di due colonne e' una richiesta di serie
	// multiple (Fase 17, vedi MultiChartData in Chart.h) -- due colonne
	// restano il percorso a singola serie di sempre, invariato.
	if (r.right - r.left > 1)
	{
		MultiChartData multi;
		if (!BuildMultiChartSeries(fDoc, r, multi))
		{
			BAlert* alert = new BAlert(B_TRANSLATE("Grafico"),
				B_TRANSLATE("Intervallo non valido: serve almeno una riga numerica "
					"in ogni colonna serie."), B_TRANSLATE("OK"));
			alert->Go();
			return;
		}

		// Codifica piatta: vedi il commento gemello in
		// ChartWindow::MessageReceived (kMsgChartDataMulti) per l'ordine
		// esatto ricostruito li'.
		BMessage data(kMsgChartDataMulti);
		for (size_t c = 0; c < multi.categories.size(); c++)
			data.AddString("category", multi.categories[c]);
		for (size_t s = 0; s < multi.seriesNames.size(); s++)
			data.AddString("seriesName", multi.seriesNames[s]);
		for (size_t s = 0; s < multi.values.size(); s++)
			for (size_t c = 0; c < multi.values[s].size(); c++)
				data.AddDouble("value", multi.values[s][c]);
		BMessenger(fChartWindow).SendMessage(&data);
		return;
	}

	std::vector<ChartSeries> series;
	if (!BuildChartSeries(fDoc, r, series))
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Grafico"),
			B_TRANSLATE("Intervallo non valido: serve esattamente due colonne "
				"(etichette, valori) con almeno una riga numerica, es. A1:B5."),
			B_TRANSLATE("OK"));
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
void MainWindow::HandleChartInsert(const char* rangeText, const char* destText,
	ChartType type, const char* title)
{
	if (!fDoc)
		return;

	range dataRange;
	cell dest;
	if (!ParseRangeRef(rangeText, dataRange) || !cell::GetCell(destText, dest))
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Grafico"),
			B_TRANSLATE("Intervallo dati o cella di destinazione non validi: l'intervallo "
				"deve avere due colonne (etichette, valori) con almeno una riga "
				"numerica, es. A1:B5."), B_TRANSLATE("OK"));
		alert->Go();
		return;
	}

	// Validazione: il ChartObject memorizza solo l'intervallo (letto
	// dal vivo a ogni ridisegno da SheetView, vedi il commento sopra),
	// quindi qui basta verificare che i dati abbiano una forma valida,
	// non serve tenerli. Due colonne (o meno) usano la validazione a
	// singola serie di sempre; piu' colonne quella a serie multiple
	// (Fase 17, vedi MultiChartData in Chart.h).
	bool validData;
	if (dataRange.right - dataRange.left > 1)
	{
		MultiChartData multi;
		validData = BuildMultiChartSeries(fDoc, dataRange, multi);
	}
	else
	{
		std::vector<ChartSeries> series;
		validData = BuildChartSeries(fDoc, dataRange, series);
	}
	if (!validData)
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Grafico"),
			B_TRANSLATE("Intervallo dati o cella di destinazione non validi: l'intervallo "
				"deve avere due colonne (etichette, valori) con almeno una riga "
				"numerica, es. A1:B5."), B_TRANSLATE("OK"));
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
	obj.type = type;
	obj.title = title;
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
		BAlert* alert = new BAlert(B_TRANSLATE("Tabella pivot"),
			B_TRANSLATE("Intervallo dati o cella di destinazione non validi."), B_TRANSLATE("OK"));
		alert->Go();
		return;
	}

	std::vector<PivotRow> rows;
	if (!BuildPivotTable(fDoc, source, rows))
	{
		BAlert* alert = new BAlert(B_TRANSLATE("Tabella pivot"),
			B_TRANSLATE("Nessun dato valido nell'intervallo (servono due colonne: "
				"categoria testuale, valore numerico)."), B_TRANSLATE("OK"));
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
		BAlert* alert = new BAlert(B_TRANSLATE("Tabella pivot"),
			B_TRANSLATE("La cella di destinazione si sovrappone all'intervallo dati: "
				"scegline una fuori dai dati sorgente."), B_TRANSLATE("OK"));
		alert->Go();
		return;
	}

	// Trovato durante l'audit: mancava SaveUndoState, a differenza di
	// ogni altra scrittura di celle vere (incolla, riempi, ordina,
	// formattazione...) -- a differenza di grafici/celle unite (fuori
	// dallo scope di UndoSnapshot per motivi architetturali, vedi
	// MainWindow::MergeCells), qui i dati scritti sono celle normali,
	// perfettamente nello scope di CaptureSnapshot/ApplySnapshot.
	fSheetView->SaveUndoState(destRange);
	WritePivotTable(fDoc, dest, rows, (PivotAggFunc)agg);
	fSheetView->Invalidate();
	MarkModified();

	BString msg;
	msg.SetToFormat(B_TRANSLATE("%d categoria/e trovate."), (int)rows.size());
	BAlert* alert = new BAlert(B_TRANSLATE("Tabella pivot"), msg.String(), B_TRANSLATE("OK"));
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
	msg.SetToFormat(B_TRANSLATE("%d cella/e sostituita/e."), (int)matches.size());
	BAlert* alert = new BAlert(B_TRANSLATE("Sostituisci tutto"), msg.String(), B_TRANSLATE("OK"));
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
	fSheetView->SaveUndoState(sel); // stesso motivo di ToggleBold
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
	SetCellMode(false);
}

void MainWindow::SetCellMode(bool editing)
{
	fEditingCell = editing;
	RefreshCellModeText();
}

void MainWindow::RefreshCellModeText()
{
	if (!fCellMode)
		return;

	BString text;
	// Stesso prefisso "* " gia' usato dal titolo della finestra (vedi
	// UpdateTitle) per le modifiche non salvate: qui nel footer resta
	// visibile anche quando il titolo e' tagliato o non guardato, senza
	// introdurre un secondo linguaggio visivo per lo stesso concetto.
	if (fModified)
		text << "* ";
	text << (fEditingCell
		? B_TRANSLATE_COMMENT("Modifica", "Indicatore di modalita' nel footer: modifica di una cella in corso, non la voce di menu \"Modifica\"")
		: B_TRANSLATE("Pronto"));
	fCellMode->SetText(text.String());
}

void MainWindow::ToggleFooterStat(int statBit)
{
	fFooterStatsMask ^= statBit;
	if (gPrefs)
	{
		gPrefs->SetPrefInt("footerStats", fFooterStatsMask);
		try { gPrefs->WritePrefFile(); } catch (...) { /* preferenze non salvabili: non bloccante */ }
	}
	SelectionChanged(fSheetView->Selection());
}

void MainWindow::SelectionChanged(cell c)
{
	// Spostare la selezione esce sempre da "Modifica" (come Excel: un
	// clic su un'altra cella non lascia mai il footer bloccato su
	// "Modifica" -- SheetView::CommitEditing/CommitFormulaBar chiamano
	// gia' SetCellMode(false) sul percorso normale, questo e' solo la
	// rete di sicurezza per ogni altro modo di cambiare selezione).
	SetCellMode(false);

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

	// Statistiche della selezione (footer, come Excel): Somma/Media/
	// Minimo/Massimo/Conteggio numerico contano solo le celle
	// NUMERICHE -- testo/vuoto/errore vengono ignorati, mai contati
	// come zero (altrimenti la media sarebbe sbagliata). Conteggio
	// invece conta ogni cella non vuota, testo compreso, esattamente
	// come il vero status bar di Excel. Quali di queste compaiono e'
	// scelto dall'utente col tasto destro (fFooterStatsMask, vedi
	// FooterStatsView sopra), non piu' fisso.
	if (fDoc)
	{
		double sum = 0.0, min = 0.0, max = 0.0;
		int numCount = 0, count = 0;
		for (int v = sel.top; v <= sel.bottom; v++)
		{
			for (int h = sel.left; h <= sel.right; h++)
			{
				Value val;
				fDoc->GetValue(cell(h, v), val);
				if (val.fType != eNoData)
					count++;
				if (val.fType == eNumData && !val.IsNan())
				{
					double d = (double)val;
					if (numCount == 0 || d < min)
						min = d;
					if (numCount == 0 || d > max)
						max = d;
					sum += d;
					numCount++;
				}
			}
		}

		BString stats;
		if ((fFooterStatsMask & kStatAverage) && numCount > 0)
			AppendFooterStat(stats, B_TRANSLATE("Media"), sum / numCount);
		if ((fFooterStatsMask & kStatCount) && count > 0)
			AppendFooterStatInt(stats, B_TRANSLATE("Conteggio"), count);
		if ((fFooterStatsMask & kStatNumCount) && numCount > 0)
			AppendFooterStatInt(stats, B_TRANSLATE("Conteggio numerico"), numCount);
		if ((fFooterStatsMask & kStatMin) && numCount > 0)
			AppendFooterStat(stats, B_TRANSLATE("Minimo"), min);
		if ((fFooterStatsMask & kStatMax) && numCount > 0)
			AppendFooterStat(stats, B_TRANSLATE("Massimo"), max);
		if ((fFooterStatsMask & kStatSum) && numCount > 0)
			AppendFooterStat(stats, B_TRANSLATE("Somma"), sum);
		fSelectionStats->SetText(stats.String());
	}

	char formula[4096];
	if (fDoc)
		fDoc->GetCellFormula(c, formula, sizeof(formula), false);
	else
		formula[0] = 0;

	// GetCellFormula/CFormula::UnMangle non antepongono mai "=" (il
	// vecchio interruttore storico gWithEqualSign resta sempre a false,
	// mai davvero collegato a nulla nel porting): senza questo, una
	// formula appariva identica a un numero/testo normale nella barra
	// della formula, a differenza di ogni altro foglio di calcolo
	// (Excel/LibreOffice Calc mostrano sempre "=A1+B1"). Aggiunto qui
	// (solo per la visualizzazione) invece di riattivare il flag
	// globale, che influenzerebbe anche l'export XLSX/ODS -- l'elemento
	// <f> di quei formati non deve mai avere "=", per specifica
	// ECMA-376/OpenDocument -- e il testo scritto da SaveASCD, dove non
	// serve (TryToParseString accetta comunque entrambe le forme in
	// lettura). Segnalato dall'utente su un file reale.
	if (fDoc && fDoc->GetCellFormula(c) != NULL)
	{
		char withEquals[4097];
		snprintf(withEquals, sizeof(withEquals), "=%s", formula);
		strlcpy(formula, withEquals, sizeof(formula));
	}

	fFormulaBar->SetText(formula);
}

const char* MainWindow::FormulaBarText() const
{
	return fFormulaBar->Text();
}

const char* MainWindow::SelectionStatsText() const
{
	return fSelectionStats->Text();
}

const char* MainWindow::CellModeText() const
{
	return fCellMode->Text();
}

void MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgNew:
			NewDocument();
			break;

		case kMsgFixupScrollBars:
			if (fSheetView)
			{
				fSheetView->FixupScrollBars();
				fSheetView->Invalidate();
			}
			break;

		case kMsgSwitchSheet:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				SwitchToSheet(index);
			break;
		}

		case kMsgNewSheet:
			NewSheet();
			break;

		case kMsgDeleteSheetRequest:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				DeleteSheet(index);
			break;
		}

		case kMsgRenameSheetRequest:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK && index >= 0 && index < (int)fSheets.size())
			{
				if (!fRenameSheetWindow)
					fRenameSheetWindow = new RenameSheetWindow(BMessenger(this));
				// Stesso motivo del lock su fCommentWindow in
				// kMsgShowCommentWindow piu' sotto in questo stesso
				// MessageReceived.
				if (fRenameSheetWindow->Lock())
				{
					fRenameSheetWindow->SetSheet(index, fSheets[index].name.String());
					fRenameSheetWindow->Unlock();
				}
				if (fRenameSheetWindow->IsHidden())
					fRenameSheetWindow->Show();
				fRenameSheetWindow->Activate();
			}
			break;
		}

		case kMsgRenameSheetCommit:
		{
			int32 index;
			BString name;
			if (message->FindInt32("index", &index) == B_OK
				&& message->FindString("name", &name) == B_OK)
				RenameSheet(index, name.String());
			break;
		}

		case kMsgOpen:
			fOpenPanel->Show();
			break;

		case kMsgOpenRecent:
		{
			BString path;
			if (message->FindString("path", &path) != B_OK)
				break;

			BEntry entry(path.String());
			entry_ref ref;
			if (entry.InitCheck() != B_OK || !entry.Exists() || entry.GetRef(&ref) != B_OK)
			{
				// Voce non piu' valida (file spostato/cancellato): tolta
				// dall'elenco invece di restare li' a fallire ogni volta.
				std::vector<BString> recent = LoadRecentFiles();
				for (size_t i = 0; i < recent.size(); i++)
				{
					if (recent[i] == path)
					{
						recent.erase(recent.begin() + i);
						break;
					}
				}
				SaveRecentFiles(recent);
				RebuildRecentMenu();

				BAlert* alert = new BAlert(B_TRANSLATE("Errore"),
					B_TRANSLATE("Il file non e' piu' disponibile nella posizione registrata."), B_TRANSLATE("OK"));
				alert->Go();
				break;
			}
			OpenFile(ref);
			break;
		}

		case kMsgSave:
			Save();
			break;

		case kMsgSaveAs:
			fSavePanel->Show();
			break;

		// Voci del sottomenu "Salva con nome" (Fase 21): precompilano il
		// nome nel pannello con l'estensione del formato scelto prima di
		// mostrarlo -- SaveToFile sceglie comunque il formato vero
		// dall'estensione digitata quando l'utente conferma (vedi il
		// commento li'), qui basta impostare un punto di partenza
		// coerente con la scelta appena fatta.
		case kMsgSaveAsAscd:
		{
			BString saveName = BaseNameWithoutExtension(fDocumentName);
			saveName << ".ascd";
			fSavePanel->SetSaveText(saveName.String());
			fSavePanel->Show();
			break;
		}

		case kMsgSaveAsCsv:
		{
			BString saveName = BaseNameWithoutExtension(fDocumentName);
			saveName << ".csv";
			fSavePanel->SetSaveText(saveName.String());
			fSavePanel->Show();
			break;
		}

		case kMsgSaveAsXlsx:
		{
			BString saveName = BaseNameWithoutExtension(fDocumentName);
			saveName << ".xlsx";
			fSavePanel->SetSaveText(saveName.String());
			fSavePanel->Show();
			break;
		}

		case kMsgSaveAsOds:
		{
			BString saveName = BaseNameWithoutExtension(fDocumentName);
			saveName << ".ods";
			fSavePanel->SetSaveText(saveName.String());
			fSavePanel->Show();
			break;
		}

		case B_REFS_RECEIVED:
		{
			// Trascinare uno o piu' file direttamente su questa finestra
			// (non sull'icona dell'applicazione, gia' gestita da
			// App::RefsReceived con la stessa politica) non deve MAI
			// sostituire un documento gia' aperto qui -- stessa regola:
			// il primo file riusa la finestra SOLO se e' ancora vergine
			// (IsUntouched), ogni altro file apre una finestra nuova.
			// Bug reale segnalato dall'utente: trascinare un file dentro
			// una finestra gia' occupata ne cancellava il contenuto
			// invece di aprirne una seconda.
			entry_ref ref;
			for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++)
			{
				if (IsUntouched())
				{
					OpenFile(ref);
					continue;
				}
				MainWindow* target = new MainWindow();
				target->Show();
				BMessage oneRef(B_REFS_RECEIVED);
				oneRef.AddRef("refs", &ref);
				target->PostMessage(&oneRef);
			}
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

		case kMsgFormulaModified:
			SetCellMode(true);
			break;

		case kMsgToggleFooterStat:
		{
			int32 bit;
			if (message->FindInt32("bit", &bit) == B_OK)
				ToggleFooterStat(bit);
			break;
		}

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

		case kMsgToggleUnderline:
			ToggleUnderline();
			break;

		case kMsgToggleWrapText:
			ToggleWrapText();
			break;

		case kMsgMergeCells:
			MergeCells();
			break;

		case kMsgUnmergeCells:
			UnmergeCells();
			break;

		case kMsgShowCommentWindow:
		{
			if (!fDoc)
				break;
			cell sel = fSheetView->Selection();
			if (!fCommentWindow)
				fCommentWindow = new CommentWindow(BMessenger(this));
			BString current = CellComment(sel.v, sel.h);
			// Show()/Activate() PRIMA di SetCell(), non dopo: il fuoco
			// tastiera si stabilisce davvero solo quando la finestra e'
			// gia' attiva sul serio (bug reale segnalato dall'utente --
			// il cursore lampeggiava ma i tasti non scrivevano nulla,
			// perche' MakeFocus() nel costruttore avveniva prima che la
			// finestra fosse mai mostrata). SetCell() sotto chiama di
			// nuovo MakeFocus() alla fine, ora che l'ordine e' quello
			// giusto.
			if (fCommentWindow->IsHidden())
				fCommentWindow->Show();
			fCommentWindow->Activate();
			// SetCell tocca fTextView, una BView che vive sul thread di
			// fCommentWindow (una BWindow a se'): senza il lock, un
			// secondo commento aperto su questo stesso thread va in
			// crash con "Looper must be locked" -- stesso bug reale gia'
			// corretto per ColorWindow/PreferencesWindow/NameWindow (vedi
			// ShowColorWindow/ShowPreferencesWindow/ShowNameWindow).
			if (fCommentWindow->Lock())
			{
				fCommentWindow->SetCell(sel.v, sel.h, current.String());
				fCommentWindow->Unlock();
			}
			break;
		}

		case kMsgCommentCommit:
		{
			int32 row, col;
			BString text;
			if (message->FindInt32("row", &row) == B_OK
				&& message->FindInt32("col", &col) == B_OK
				&& message->FindString("text", &text) == B_OK)
				SetCellComment(row, col, text.String());
			break;
		}

		case kMsgCommentRemove:
		{
			int32 row, col;
			if (message->FindInt32("row", &row) == B_OK
				&& message->FindInt32("col", &col) == B_OK)
				RemoveCellComment(row, col);
			break;
		}

		case kMsgShowHyperlinkWindow:
		{
			if (!fDoc)
				break;
			cell sel = fSheetView->Selection();
			if (!fHyperlinkWindow)
				fHyperlinkWindow = new HyperlinkWindow(BMessenger(this));
			BString current = CellHyperlink(sel.v, sel.h);
			// Stesso motivo del lock su fCommentWindow poco sopra.
			if (fHyperlinkWindow->Lock())
			{
				fHyperlinkWindow->SetCell(sel.v, sel.h, current.String());
				fHyperlinkWindow->Unlock();
			}
			if (fHyperlinkWindow->IsHidden())
				fHyperlinkWindow->Show();
			fHyperlinkWindow->Activate();
			break;
		}

		case kMsgHyperlinkCommit:
		{
			int32 row, col;
			BString url;
			if (message->FindInt32("row", &row) == B_OK
				&& message->FindInt32("col", &col) == B_OK
				&& message->FindString("url", &url) == B_OK)
				SetCellHyperlink(row, col, url.String());
			break;
		}

		case kMsgHyperlinkRemove:
		{
			int32 row, col;
			if (message->FindInt32("row", &row) == B_OK
				&& message->FindInt32("col", &col) == B_OK)
				RemoveCellHyperlink(row, col);
			break;
		}

		case kMsgShowValidationWindow:
		{
			if (!fDoc)
				break;
			cell sel = fSheetView->Selection();
			if (!fValidationWindow)
				fValidationWindow = new ValidationWindow(BMessenger(this));
			ValidationRule rule = CellValidation(sel.v, sel.h);
			// Stesso motivo del lock su fCommentWindow piu' sopra.
			if (fValidationWindow->Lock())
			{
				fValidationWindow->SetCell(sel.v, sel.h, (int)rule.type,
					rule.list.c_str(), rule.min, rule.max);
				fValidationWindow->Unlock();
			}
			if (fValidationWindow->IsHidden())
				fValidationWindow->Show();
			fValidationWindow->Activate();
			break;
		}

		case kMsgValidationCommit:
		{
			int32 type = 0;
			BString list;
			double min = 0, max = 0;
			message->FindInt32("type", &type);
			message->FindString("list", &list);
			message->FindDouble("min", &min);
			message->FindDouble("max", &max);
			ApplyValidationToSelection(type, list.String(), min, max);
			break;
		}

		case kMsgValidationRemove:
			RemoveValidationFromSelection();
			break;

		case kMsgShowConditionalFormatWindow:
		{
			if (!fDoc)
				break;
			if (!fConditionalFormatWindow)
				fConditionalFormatWindow = new ConditionalFormatWindow(BMessenger(this));
			if (fConditionalFormatWindow->IsHidden())
				fConditionalFormatWindow->Show();
			fConditionalFormatWindow->Activate();
			break;
		}

		case kMsgCondFormatCommit:
		{
			int32 type = 0;
			BString value;
			rgb_color* color = NULL;
			ssize_t size = 0;
			message->FindInt32("type", &type);
			message->FindString("value", &value);
			if (message->FindData("color", B_RGB_COLOR_TYPE,
					(const void**)&color, &size) == B_OK && color)
				ApplyConditionalFormatToSelection(type, value.String(), *color);
			break;
		}

		case kMsgCondFormatRemoveAll:
			RemoveAllConditionalFormatRules();
			break;

		case kMsgSetAlignment:
		{
			int32 alignment;
			if (message->FindInt32("alignment", &alignment) == B_OK)
				SetAlignment((char)alignment);
			break;
		}

		case kMsgShowTextColor:
			ShowColorWindow(eTextColor);
			break;

		case kMsgShowBgColor:
			ShowColorWindow(eBackgroundColor);
			break;

		case kMsgShowBorderColor:
			ShowColorWindow(eBorderColor);
			break;

		case kMsgShowBorderWindow:
			ShowBorderWindow();
			break;

		case kMsgBorderFormatRequest:
		{
			bool top = false, left = false, bottom = false, right = false;
			int32 thickness = 1;
			rgb_color* color = NULL;
			ssize_t size = 0;
			message->FindBool("top", &top);
			message->FindBool("left", &left);
			message->FindBool("bottom", &bottom);
			message->FindBool("right", &right);
			message->FindInt32("thickness", &thickness);
			rgb_color chosen = { 0, 0, 0, 255 };
			if (message->FindData("color", B_RGB_COLOR_TYPE,
					(const void**)&color, &size) == B_OK && color)
				chosen = *color;
			HandleBorderFormatRequest(top, left, bottom, right, (int)thickness, chosen);
			break;
		}

		case kMsgColorRequest:
		{
			rgb_color* color = NULL;
			ssize_t size = 0;
			int32 target = eTextColor;
			message->FindInt32("target", &target);
			if (message->FindData("color", B_RGB_COLOR_TYPE,
					(const void**)&color, &size) == B_OK && color)
			{
				switch ((ColorTarget)target)
				{
					case eBackgroundColor: SetBackgroundColor(*color); break;
					case eBorderColor: SetBorderColor(*color); break;
					default: SetTextColor(*color); break;
				}
			}
			break;
		}

		case kMsgShowPreferences:
			ShowPreferencesWindow();
			break;

		case kMsgAutoSaveTick:
			AutoSaveBackup();
			break;

		case B_ABOUT_REQUESTED:
			(new AboutWindow())->Show();
			break;

		case kMsgPreferencesRequest:
		{
			bool showGrid = true;
			int8 decimalSep = '.', listSep = ';', thousandSep = ',';
			int32 maxRecentFiles = fMaxRecentFiles;
			bool showSplash = true;
			const char* currencySymbol = gCurrencySymbol;
			bool autoSaveEnabled = fAutoSaveEnabled;
			int32 autoSaveInterval = fAutoSaveIntervalMinutes;
			message->FindBool("showGrid", &showGrid);
			message->FindInt8("decimalSeparator", &decimalSep);
			message->FindInt8("listSeparator", &listSep);
			message->FindInt32("maxRecentFiles", &maxRecentFiles);
			message->FindBool("showSplash", &showSplash);
			message->FindInt8("thousandSeparator", &thousandSep);
			message->FindString("currencySymbol", &currencySymbol);
			message->FindBool("autoSaveEnabled", &autoSaveEnabled);
			message->FindInt32("autoSaveInterval", &autoSaveInterval);
			HandlePreferencesRequest(showGrid, (char)decimalSep, (char)listSep,
				(int)maxRecentFiles, showSplash, (char)thousandSep, currencySymbol,
				autoSaveEnabled, (int)autoSaveInterval);
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

		case kMsgSetBorderThickness:
		{
			int32 thickness = 1;
			if (message->FindInt32("thickness", &thickness) == B_OK)
				SetBorderThickness((uchar)thickness);
			break;
		}

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
			BString rangeText, destText, titleText;
			int32 type = eBarChart;
			message->FindInt32("type", &type);
			message->FindString("title", &titleText);
			if (message->FindString("range", &rangeText) == B_OK
				&& message->FindString("dest", &destText) == B_OK)
				HandleChartInsert(rangeText.String(), destText.String(), (ChartType)type,
					titleText.String());
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

void MainWindow::FrameResized(float width, float height)
{
	BWindow::FrameResized(width, height);
	if (fSheetView)
		fSheetView->FixupScrollBars();
}

void MainWindow::Show()
{
	BWindow::Show();

	// BMessageRunner "spara e dimentica" (count=1, un solo colpo): non
	// serve tenerne il puntatore, vedi il commento sulla dichiarazione
	// in MainWindow.h per il motivo del ritardo.
	new BMessageRunner(BMessenger(this), new BMessage(kMsgFixupScrollBars),
		200000, 1);
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
