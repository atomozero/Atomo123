# Applicazione nativa Atomo123 (Fase 4) — Interface/Layout Kit.
#
# Usa l'engine di calcolo isolato (../engine/libengine.a) e apre i
# file tramite il Translation Kit (BTranslatorRoster), che sceglie
# automaticamente fra i translator installati (CSV/XLS/XLSX/ODS/ASCD
# nativo — vedi translators/*/README o docs/TRANSLATORS.md). I
# translator vanno installati separatamente ("make install" nella
# rispettiva cartella) perche' sono add-on caricati a runtime, non
# linkati staticamente in questo binario.

CXX := g++
# -MMD -MP: genera un file .d di dipendenze (header inclusi, non solo
# il .cpp) accanto a ogni .o -- senza questo, "make" non sa che un
# .cpp va ricompilato quando cambia solo un header che include (regola
# "%.o: %.cpp" sotto non lo dice), lasciando .o vecchi linkati insieme
# a .o nuovi con un layout di struct diverso per lo stesso header.
# Causa reale di un crash (vector<bool> corrotto in
# ChartWindow::RebuildSeriesCheckboxes, 21/08/2026): MultiChartData in
# Chart.h aveva guadagnato un campo, ma MainWindow.o/SheetView.o non
# erano stati ricompilati insieme a ChartWindow.o/ChartView.o/Chart.o
# perche' i loro .cpp non erano stati toccati direttamente in
# quell'ultima modifica -- stesso sizeof(MultiChartData) atteso da
# funzioni diverse, in realta' diverso fra le unita' di compilazione.
CXXFLAGS := -std=gnu++17 -Wall -g -MMD -MP
ENGINE_DIR := ../engine
ENGINE_SRCDIRS := $(ENGINE_DIR)/src/Cell $(ENGINE_DIR)/src/Formula \
	$(ENGINE_DIR)/src/Functions $(ENGINE_DIR)/src/Excel \
	$(ENGINE_DIR)/src/FileSys $(ENGINE_DIR)/src/Collections \
	$(ENGINE_DIR)/src/Misc-Classes $(ENGINE_DIR)/src/Utils \
	$(ENGINE_DIR)/src/Config $(ENGINE_DIR)/src/bsl \
	$(ENGINE_DIR)/src/Metrowerks $(ENGINE_DIR)/src/Stubs
INCLUDES := $(addprefix -I, $(ENGINE_SRCDIRS)) -Isrc

LIBS := -L$(ENGINE_DIR) -lengine -lbe -llocalestub -ltranslation -ltracker -lGL -lGLU

APP := Atomo123
SOURCES := src/App.cpp src/MainWindow.cpp src/SheetView.cpp src/SheetTabView.cpp \
	src/ToolbarView.cpp \
	src/AscdIO.cpp src/FindWindow.cpp src/ChartWindow.cpp src/ChartView.cpp src/Chart.cpp \
	src/PivotWindow.cpp src/Pivot.cpp src/RangeRef.cpp src/PrintLayout.cpp src/IconCatalog.cpp src/IconData.cpp \
	src/AutoFill.cpp \
	src/FileTypeIcons.cpp \
	src/ProgressWindow.cpp \
	src/NameWindow.cpp src/PasteSpecialWindow.cpp src/GoToWindow.cpp src/RenameSheetWindow.cpp \
	src/CommentWindow.cpp \
	src/HyperlinkWindow.cpp \
	src/ValidationWindow.cpp \
	src/ConditionalFormatWindow.cpp \
	src/ColorWindow.cpp \
	src/PreferencesWindow.cpp src/AboutWindow.cpp src/ClickableStringView.cpp \
	src/BorderWindow.cpp src/PageSetupWindow.cpp src/PrintPreviewView.cpp \
	src/SplashWindow.cpp src/AtomGLView.cpp src/AtomTheme.cpp src/TextTexture.cpp
OBJECTS := $(SOURCES:.cpp=.o)
RDEF := Atomo123.rdef
RSRC := Atomo123.rsrc

# Strumenti storici di compilazione risorse (rez/bsl, dalla stessa
# Sum-It da cui deriva l'engine), ricompilati per questo sistema in
# Fase 1 (vedi legacy/opensumit/PORTING_STATUS.md) ma finora mai
# davvero usati: servono per generare la risorsa 'Func' (tabella
# delle funzioni con nome nelle formule) da
# engine/resources/funcs_by_nr.r, vedi $(RSRC) sotto e
# docs/ENGINE_API.md.
LEGACY_DIR := ../legacy/opensumit
REZ := $(LEGACY_DIR)/rez/rez
BSL := $(LEGACY_DIR)/bsl/bsl
FUNC_RESOURCES := ../engine/resources

# Localizzazione (Locale Kit): le stringhe sorgente nel codice sono in
# italiano (non inglese come da convenzione Haiku), quindi qui non c'e'
# nessuna lingua "en" da estrarre -- locales/en.catkeys e' scritto a
# mano con le traduzioni inglesi vere, mai generato automaticamente.
#
# ATTENZIONE quando si aggiungono/spostano stringhe B_TRANSLATE: il
# 4o campo dell'intestazione di locales/en.catkeys ("1 English <sig>
# <numero>") NON e' un segnaposto arbitrario -- e' un fingerprint del
# contenuto (chiave/contesto/commento), verificato da linkcatkeys al
# link (bug reale scoperto qui: un fingerprint non aggiornato dopo
# un'aggiunta fa fallire tutto il link con "Bad data", anche se ogni
# singola riga e' sintatticamente valida). Dopo ogni modifica al set di
# stringhe, rigenerare il fingerprint corretto con "make catkeys" e
# copiare il 4o campo dell'intestazione di locales/it.catkeys (mai il
# contenuto delle traduzioni, solo quel numero) in locales/en.catkeys.
APP_MIME_SIG := application/x-vnd.Atomo-Atomo123
CATKEYS_DIR := locales
EN_CATKEYS := $(CATKEYS_DIR)/en.catkeys

all: $(APP)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(REZ):
	$(MAKE) -C $(LEGACY_DIR)/rez

$(BSL):
	$(MAKE) -C $(LEGACY_DIR)/bsl

%.o: %.cpp
	$(CXX) -c $< -o $@ $(INCLUDES) $(CXXFLAGS)

# Hash del commit corrente (mostrato nello splash, sotto la versione,
# vedi src/AtomGLView.cpp) -- rigenerato a OGNI "make" (dipendenza
# fittizia sempre-fuori-data "FORCE" sotto), ma la ricompilazione a
# valle scatta solo se il contenuto e' davvero cambiato (confronto con
# "cmp" prima di sovrascrivere: aggiornare il timestamp a ogni "make"
# anche a commit invariato ricompilerebbe AtomGLView.o inutilmente a
# ogni build). "-dirty" quando l'albero di lavoro ha modifiche non
# committate rispetto a HEAD, utile in sviluppo per non scambiare per
# sbaglio una build locale modificata per quella di un commit pulito.
BUILD_INFO_H := src/BuildInfo.h

.PHONY: FORCE
FORCE:

$(BUILD_INFO_H): FORCE
	@commit=$$(git rev-parse --short=8 HEAD 2>/dev/null || echo unknown); \
	if [ "$$commit" != "unknown" ] && ! git diff --quiet HEAD -- 2>/dev/null; then \
		commit="$$commit-dirty"; \
	fi; \
	printf '#define ATOMO123_GIT_COMMIT "%s"\n' "$$commit" > $(BUILD_INFO_H).tmp; \
	cmp -s $(BUILD_INFO_H).tmp $(BUILD_INFO_H) 2>/dev/null && rm -f $(BUILD_INFO_H).tmp \
		|| mv $(BUILD_INFO_H).tmp $(BUILD_INFO_H)

src/AtomGLView.o: $(BUILD_INFO_H)

# rc compila l'icona/firma/versione da Atomo123.rdef; rez e bsl
# aggiungono (per fusione nello stesso file, non sovrascrivendolo --
# comportamento verificato a mano invocandoli in sequenza su un file
# di prova) la risorsa 'Func' (tabella delle funzioni) e due risorse
# 'StrL' (nomi/descrizioni delle funzioni, usate da InitFunctions()
# in App.cpp) allo stesso Atomo123.rsrc, cosi' xres le allega tutte
# insieme in un solo passaggio.
$(RSRC): $(RDEF) $(REZ) $(BSL) $(FUNC_RESOURCES)/funcs_by_nr.r \
		$(FUNC_RESOURCES)/FuncNames.txt $(FUNC_RESOURCES)/FuncDescs.txt
	rc -o $(RSRC) $(RDEF)
	$(REZ) -o $(RSRC) $(FUNC_RESOURCES)/funcs_by_nr.r
	$(BSL) -o $(RSRC) -id 7 -name "FunctionNames" $(FUNC_RESOURCES)/FuncNames.txt
	$(BSL) -o $(RSRC) -id 8 -name "FuncDescs" $(FUNC_RESOURCES)/FuncDescs.txt

# L'icona (risorsa VICN/BEOS:ICON, disegnata in icons/atomo123.svg e
# convertita in HVIF con Icon-O-Matic, incorporata in Atomo123.rdef)
# va allegata al binario con xres DOPO il link, non prima: xres opera
# sul file eseguibile finito.
$(APP): $(OBJECTS) $(ENGINE_DIR)/libengine.a $(RSRC) $(EN_CATKEYS)
	$(CXX) -o $(APP) $(OBJECTS) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	xres -o $(APP) $(RSRC)
	linkcatkeys -o $(APP) -s $(APP_MIME_SIG) -tr -l en $(EN_CATKEYS)

run: $(APP)
	./$(APP)

# Passaggio manuale, non nel giro normale di "make": rigenera
# locales/it.catkeys, l'elenco delle chiavi italiane davvero presenti
# nel sorgente (via B_TRANSLATE), estratte con lo stesso strumento e
# preprocessore che Haiku userebbe per una lingua "en" -- qui pero'
# l'italiano E' la lingua sorgente, quindi questo file serve solo da
# lista di controllo per non dimenticare una stringa nuova in
# locales/en.catkeys quando si aggiunge testo, non viene mai letto a
# runtime ne' committato.
catkeys: $(SOURCES)
	-cat $(SOURCES) | $(CXX) -E -x c++ $(INCLUDES) $(CXXFLAGS) -DB_COLLECTING_CATKEYS - | grep -av '^#' > $(CATKEYS_DIR)/atomo123.pre
	collectcatkeys -s $(APP_MIME_SIG) $(CATKEYS_DIR)/atomo123.pre -o $(CATKEYS_DIR)/it.catkeys
	rm -f $(CATKEYS_DIR)/atomo123.pre

# Test di round-trip di AscdIO (SaveASCD/LoadASCD): non richiede una
# sessione grafica, solo l'engine e libbe per BFile/BPositionIO.
test: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_ascd_io.cpp -o tests/test_ascd_io.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/AscdIO.cpp -o tests/AscdIO.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_ascd_io tests/test_ascd_io.o tests/AscdIO.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe
	./tests/test_ascd_io

# Test del "+" unario davanti a un fattore di formula (bug reale
# scoperto in un file XLSX reale, vedi tests/test_unary_plus.cpp):
# comportamento del parser dell'engine, stessa ragione headless di
# "test" sopra (serve solo l'engine e libbe per BApplication/CFormatter).
test-unary-plus: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_unary_plus.cpp -o tests/test_unary_plus.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_unary_plus tests/test_unary_plus.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe
	./tests/test_unary_plus

# Test del formato "cartella di lavoro" multi-foglio (Fase 9): stessa
# ragione headless di "test" sopra (serve solo BApplication, non una
# finestra).
test-ascd-book: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_ascd_book.cpp -o tests/test_ascd_book.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/AscdIO.cpp -o tests/AscdIO_book.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_ascd_book tests/test_ascd_book.o tests/AscdIO_book.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe
	./tests/test_ascd_book

# Regressione del disallineamento fra il formato ASCD duplicato dal
# translator XLSX e quello vero letto da AscdIO.cpp (vedi il commento
# in cima a test_translator_multisheet_import.cpp) -- a differenza di
# test-ascd-book sopra, passa DAVVERO per il translator installato
# (BTranslatorRoster, -ltranslation), l'unico modo per rilevare un
# disallineamento fra i due formati duplicati.
test-translator-multisheet: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_translator_multisheet_import.cpp -o tests/test_translator_multisheet_import.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/AscdIO.cpp -o tests/AscdIO_translator_multisheet.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_translator_multisheet_import \
		tests/test_translator_multisheet_import.o tests/AscdIO_translator_multisheet.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -ltranslation
	./tests/test_translator_multisheet_import

# Test dei bordi di cella (Fase 11): un lato alla volta, applicati a
# tutta la selezione, indipendenti fra loro, ClearBorders. Stessa
# ragione di test-paste-range (serve una vera MainWindow).
test-borders: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_borders.cpp -o tests/test_borders.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_borders tests/test_borders.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_borders

# Test della riformattazione locale-aware valuta/percentuale (Fase 5,
# SheetView::FormattedCellText): stessa ragione di test-borders sopra
# (serve una vera MainWindow, il metodo e' pubblico apposta per essere
# testabile).
test-currency-format: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_currency_format.cpp -o tests/test_currency_format.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_currency_format tests/test_currency_format.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_currency_format

# Test del testo che trabocca sulle colonne vuote vicine (Fase 5,
# SheetView::ExpandOverflowRect): stessa ragione di test-borders sopra
# (serve una vera MainWindow, il metodo e' pubblico apposta per essere
# testabile).
test-overflow: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_overflow.cpp -o tests/test_overflow.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_overflow tests/test_overflow.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_overflow

# Genera examples/atomo123_demo.ascd, una cartella di lavoro a due
# fogli che mostra un sottoinsieme delle funzionalita' del formato
# nativo (formule fra fogli, valuta, colori, bordi, celle unite,
# Blocca riquadri, AutoFilter, commento, collegamento, grafico
# incorporato): stessa ragione headless di test-ascd-book sopra (solo
# AscdIO, nessuna vera MainWindow).
demo: $(ENGINE_DIR)/libengine.a
	$(CXX) -c examples/generate_demo.cpp -o examples/generate_demo.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/AscdIO.cpp -o examples/AscdIO_demo.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o examples/generate_demo examples/generate_demo.o examples/AscdIO_demo.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe
	cd examples && ./generate_demo

# Test della persistenza di Blocca riquadri/altezza di riga nel
# formato nativo (Fase 10): stessa ragione headless di test-ascd-book
# sopra (solo AscdIO, nessuna vera MainWindow).
test-persistence: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_persistence.cpp -o tests/test_persistence.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/AscdIO.cpp -o tests/AscdIO_persistence.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_persistence tests/test_persistence.o tests/AscdIO_persistence.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe
	./tests/test_persistence

# Test della logica del grafico a barre (Chart.cpp): headless come
# "test" sopra, non tocca mai BView/BWindow (quello e' ChartView, non
# testato qui).
test-chart: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_chart.cpp -o tests/test_chart.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/Chart.cpp -o tests/Chart.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_chart tests/test_chart.o tests/Chart.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -llocalestub
	./tests/test_chart

# Verifica che le etichette lunghe (categorie/legenda) non sconfinino
# piu' fuori dal grafico (Fase 29, bug segnalato dall'utente): a
# differenza di test-chart sopra, qui serve una vera BView (misura del
# testo con StringWidth), agganciata pero' a una BBitmap offscreen, mai
# a una vera finestra -- vedi il commento in cima al file.
test-chart-label-wrap: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_chart_label_wrap.cpp -o tests/test_chart_label_wrap.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/Chart.cpp -o tests/Chart_label_wrap.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_chart_label_wrap tests/test_chart_label_wrap.o tests/Chart_label_wrap.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -llocalestub
	./tests/test_chart_label_wrap

# Test di FormatRangeRef/ParseRangeRef (RangeRef.cpp): stessa ragione
# di test-chart, headless.
test-range-ref: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_range_ref.cpp -o tests/test_range_ref.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/RangeRef.cpp -o tests/RangeRef.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_range_ref tests/test_range_ref.o tests/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -llocalestub
	./tests/test_range_ref

# Test del calcolo delle pagine di stampa (PrintLayout.cpp): stessa
# ragione di test-chart, headless -- niente BPrintJob vero (serve una
# stampante configurata), solo la matematica di quante pagine servono
# e da dove inizia ognuna.
test-print-layout: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_print_layout.cpp -o tests/test_print_layout.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/PrintLayout.cpp -o tests/PrintLayout.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_print_layout tests/test_print_layout.o tests/PrintLayout.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -llocalestub
	./tests/test_print_layout

# Test del riconoscimento della serie per il riempimento automatico
# (AutoFill.cpp, Fase 29): stessa ragione di test-chart/test-pivot,
# headless -- nessuna SheetView/CContainer necessaria, solo vettori di
# Value.
test-autofill: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_autofill.cpp -o tests/test_autofill.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/AutoFill.cpp -o tests/AutoFill.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_autofill tests/test_autofill.o tests/AutoFill.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -llocalestub
	./tests/test_autofill

# Test della logica della tabella pivot (Pivot.cpp): stessa ragione di
# test-chart, headless.
test-pivot: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_pivot.cpp -o tests/test_pivot.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/Pivot.cpp -o tests/Pivot.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_pivot tests/test_pivot.o tests/Pivot.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -llocalestub
	./tests/test_pivot

# Test degli appunti (be_clipboard): a differenza di "test" richiede
# una vera sessione grafica (il Clipboard Kit passa dall'app_server),
# quindi e' un target separato, non incluso nel normale "make test".
test-clipboard: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_clipboard.cpp -o tests/test_clipboard.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_clipboard tests/test_clipboard.o \
		$(INCLUDES) $(CXXFLAGS) -lbe
	./tests/test_clipboard

# Test dello scroll automatico verso la cella selezionata: richiede
# una vera sessione grafica (Layout Kit/app_server) e le stesse
# classi dell'app (SheetView usa dynamic_cast<MainWindow*>, quindi
# serve linkare anche MainWindow/AscdIO/FindWindow, non solo
# SheetView) — target separato come test-clipboard.
test-scroll: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_scroll.cpp -o tests/test_scroll.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_scroll tests/test_scroll.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_scroll

# Test dell'editing in-cella (digitare su una cella, confermare con
# Invio/annullare con Escape): stessa ragione di test-scroll (richiede
# una vera sessione grafica per BTextControl), target separato.
test-editing: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_editing.cpp -o tests/test_editing.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_editing tests/test_editing.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_editing

# Come test-editing, ma con un vero B_MOUSE_DOWN/B_KEY_DOWN
# sintetizzato e smistato dal vero ciclo di dispatch della finestra
# (BMessenger::SendMessage sincrono), invece di chiamare
# MouseDown()/KeyDown() direttamente: chiude il punto rimasto aperto
# in Fase 4 sul test end-to-end del doppio click/digitazione diretta.
test-real-input-edit: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_real_input_edit.cpp -o tests/test_real_input_edit.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_real_input_edit tests/test_real_input_edit.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_real_input_edit

# Test delle scorciatoie da tastiera in stile Excel (Inizio/Ctrl+
# Inizio/Ctrl+Fine/PagSu/PagGiu/Maiusc+Tab/Maiusc+Invio): stessa
# ragione di test-scroll (BScrollView reale per PagSu/PagGiu, che
# leggono Parent()->Bounds()), target separato.
test-navigation: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_navigation.cpp -o tests/test_navigation.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_navigation tests/test_navigation.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_navigation

# Test della selezione multi-cella (Fase 7): Maiusc+frecce,
# trascinamento del mouse, Ctrl+A, Backspace/Canc su un intervallo
# intero -- stessa ragione di test-navigation (BScrollView reale).
test-selection: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_selection.cpp -o tests/test_selection.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_selection tests/test_selection.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_selection

# Test della trasparenza nelle immagini incorporate (bug reale
# segnalato dall'utente): stessa ragione di legare all'engine di
# traduzione (-ltranslation, gia' in $(LIBS)) di test-chart/test-pivot,
# ma qui serve anche codificare un vero PNG con canale alpha al volo.
test-image-alpha: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_image_alpha.cpp -o tests/test_image_alpha.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_image_alpha tests/test_image_alpha.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_image_alpha

# Test di Riempi in basso/a destra (Fase 7): stessa ragione di
# test-selection (BScrollView reale).
test-fill: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_fill.cpp -o tests/test_fill.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_fill tests/test_fill.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_fill

# Test di Ordina crescente/decrescente (Fase 7): stessa ragione di
# test-selection (BScrollView reale).
test-sort: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_sort.cpp -o tests/test_sort.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_sort tests/test_sort.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_sort

# Test di Annulla/Ripeti (Fase 7): stessa ragione di test-selection
# (BScrollView reale).
test-undo: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_undo.cpp -o tests/test_undo.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_undo tests/test_undo.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_undo

# Il tasto Canc/Backspace deve svuotare solo valore/formula, non la
# formattazione della cella (colore, bordo, ecc.) -- vedi il commento
# in cima al test e su CContainer::ClearCellContent in Container.h.
test-clear-keeps-style: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_clear_keeps_style.cpp -o tests/test_clear_keeps_style.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_clear_keeps_style tests/test_clear_keeps_style.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_clear_keeps_style

# Test di Taglia/Copia/Incolla e Formato estesi all'intervallo
# selezionato (Fase 7): richiede una vera MainWindow (non la sola
# SheetView), vedi il commento in cima al test.
test-paste-range: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_paste_range.cpp -o tests/test_paste_range.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_paste_range tests/test_paste_range.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_paste_range

# Test di Inserisci/Elimina riga e colonna (Fase 7): stessa ragione di
# test-selection (BScrollView reale).
test-insert-delete: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_insert_delete.cpp -o tests/test_insert_delete.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_insert_delete tests/test_insert_delete.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_insert_delete

# Test della protezione dalle modifiche non salvate (dirty flag +
# titolo + conferma prima di scartare): richiede una vera MainWindow,
# stessa ragione di test-paste-range.
test-unsaved-changes: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_unsaved_changes.cpp -o tests/test_unsaved_changes.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_unsaved_changes tests/test_unsaved_changes.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_unsaved_changes

# Test del ridimensionamento riga/colonna (Fase 8): stessa ragione di
# test-selection (BScrollView reale).
test-resize: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_resize.cpp -o tests/test_resize.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_resize tests/test_resize.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_resize

# Test dell'AutoFilter (migliorie di fedelta' XLSX): stessa ragione di
# test-resize (BScrollView reale).
test-autofilter: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_autofilter.cpp -o tests/test_autofilter.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_autofilter tests/test_autofilter.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_autofilter

# Test dello spostamento con il mouse di un grafico incorporato:
# stessa ragione di test-image-drag (BScrollView reale), su
# ChartObject::frame invece che sull'offset di un'immagine.
test-chart-drag: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_chart_drag.cpp -o tests/test_chart_drag.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_chart_drag tests/test_chart_drag.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_chart_drag

# Test del ridimensionamento con il mouse di un grafico incorporato
# (Fase 20): stessa ragione di test-chart-drag.
test-chart-resize: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_chart_resize.cpp -o tests/test_chart_resize.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_chart_resize tests/test_chart_resize.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_chart_resize

# Test dello spostamento con il mouse di un'immagine incorporata:
# stessa ragione di test-resize (BScrollView reale).
test-image-drag: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_image_drag.cpp -o tests/test_image_drag.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_image_drag tests/test_image_drag.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_image_drag

# Test del ridimensionamento con il mouse di un'immagine incorporata:
# stessa ragione di test-image-drag, sulla maniglia invece che sul corpo.
test-image-resize: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_image_resize.cpp -o tests/test_image_resize.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_image_resize tests/test_image_resize.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_image_resize

# Test della cancellazione (Canc/Backspace) di un'immagine incorporata
# selezionata: stessa ragione di test-image-drag (BScrollView reale).
test-image-delete: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_image_delete.cpp -o tests/test_image_delete.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_image_delete tests/test_image_delete.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_image_delete

# Test della cancellazione di un grafico incorporato (Fase 22): stessa
# ragione di test-image-delete.
test-chart-delete: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_chart_delete.cpp -o tests/test_chart_delete.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_chart_delete tests/test_chart_delete.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_chart_delete

# Test di non-regressione per il trascinamento col tasto destro
# (esportazione di un'immagine fuori dal programma, DragMessage/
# B_COPY_TARGET): il vero drag-and-drop di sistema non e' testabile
# senza un handshake fra processi reale, vedi il commento in cima al
# file sorgente -- copre solo che il tasto sinistro (spostamento,
# gia' testato in test-image-drag) resta invariato dopo aver aggiunto
# la soglia/il ramo per il tasto destro.
test-image-export-drag: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_image_export_drag.cpp -o tests/test_image_export_drag.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_image_export_drag tests/test_image_export_drag.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_image_export_drag

# Test dello scorrimento automatico del foglio quando si trascina/
# ridimensiona un'immagine incorporata oltre il bordo dell'area
# visibile: stessa ragione di test-image-drag (BScrollView reale).
test-image-autoscroll: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_image_autoscroll.cpp -o tests/test_image_autoscroll.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_image_autoscroll tests/test_image_autoscroll.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_image_autoscroll

# Test del comportamento delle celle unite al clic del mouse (Fase 13):
# stessa ragione di test-image-drag (BScrollView reale) piu' una
# verifica sui pixel veri del riquadro di selezione, stesso principio
# di test-autofilter per la freccia a discesa.
test-merge-click: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_merge_click.cpp -o tests/test_merge_click.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_merge_click tests/test_merge_click.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_merge_click

# Test del supporto multi-foglio in MainWindow (Fase 9): apertura di
# una cartella di lavoro nativa a piu' fogli, cambio del foglio
# attivo. Stessa ragione di test-paste-range (serve una vera
# MainWindow).
test-multisheet: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_multisheet.cpp -o tests/test_multisheet.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_multisheet tests/test_multisheet.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_multisheet

# Apertura file su un thread separato con finestra di avanzamento
# (Fase 31): a differenza di test-multisheet sopra (OpenFile sincrona),
# verifica MainWindow::OpenFileAsync -- IsOpeningFile() vero appena
# dopo la chiamata (prova diretta che non blocca), poi lo stesso
# risultato finale di un'apertura sincrona una volta che il thread di
# lavoro ha finito.
test-open-async: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_open_async.cpp -o tests/test_open_async.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_open_async tests/test_open_async.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_open_async

# Ottimizzazione "celle sporche" di RecalculateWorkbook (Fase 32):
# verifica che l'elenco delle celle con formula, raccolto una sola
# volta prima di tutte le passate, resti corretto anche con una
# formula ad array (SEQUENCE, spilla VALORI nei vicini) su una cartella
# di lavoro multi-foglio con centinaia di celle letterali e un
# riferimento incrociato.
test-recalc-dirty-cells: $(OBJECTS) $(ENGINE_DIR)/libengine.a $(RSRC)
	$(CXX) -c tests/test_recalc_dirty_cells.cpp -o tests/test_recalc_dirty_cells.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_recalc_dirty_cells tests/test_recalc_dirty_cells.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	xres -o tests/test_recalc_dirty_cells $(RSRC)
	./tests/test_recalc_dirty_cells

# Menu File > "Apri recenti": elenco degli ultimi 5 file aperti,
# persistito in gPrefs (vedi il commento in test_recent_files.cpp sul
# perche' questo test, a differenza degli altri test UI, imposta un
# vero gPrefs invece di lasciarlo NULL).
test-recent-files: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_recent_files.cpp -o tests/test_recent_files.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_recent_files tests/test_recent_files.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_recent_files

# Test dei riferimenti fra fogli (Fase 9, "NomeFoglio!Cella") su una
# vera MainWindow aperta con MainWindow::OpenFile: verifica che
# AttachSheetResolver/RecalculateWorkbook (chiamati da OpenFile) diano
# subito il valore corretto, non solo la logica isolata del motore
# (vedi engine/tests/xsheet_test.cpp). Stessa ragione di
# test-multisheet (serve una vera MainWindow).
test-xsheet-formulas: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_xsheet_formulas.cpp -o tests/test_xsheet_formulas.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_xsheet_formulas tests/test_xsheet_formulas.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_xsheet_formulas

# Test di un bug reale grave (Fase 16, segnalato dall'utente con uno
# screenshot): apre tests/sample_formulas.xlsx (riusato da translators/
# xlsx/tests/) con una VERA MainWindow::OpenFile, che usa il vero
# BTranslatorRoster e quindi il vero XlsxTranslator.so installato --
# l'UNICO modo per verificare automaticamente che un translator .so
# caricato come add-on separato (non compilato direttamente
# nell'eseguibile di prova, come fa invece translators/xlsx/tests/
# test_xlsx_translator.cpp) inizializzi davvero la propria tabella
# delle funzioni con nome. Richiede che i quattro translator siano
# gia' installati (make install in translators/*), altrimenti
# BTranslatorRoster non trova nessun formato XLSX.
# Test del catalogo inglese incorporato (localizzazione, vedi
# locales/en.catkeys e la regola di $(APP) sopra che lo incorpora con
# "linkcatkeys -tr"): richiede $(APP) gia' compilato in questa stessa
# directory, perche' apre proprio quel file per leggerne il catalogo
# incorporato con la lingua forzata a "en" -- non serve una vera
# sessione grafica ne' cambiare la lingua di sistema.
test-locale-catalog: $(APP)
	$(CXX) -c tests/test_locale_catalog.cpp -o tests/test_locale_catalog.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_locale_catalog tests/test_locale_catalog.o \
		$(INCLUDES) $(CXXFLAGS) -lbe -llocalestub
	./tests/test_locale_catalog

test-xlsx-named-functions-dynamic: $(OBJECTS) $(ENGINE_DIR)/libengine.a $(RSRC)
	$(CXX) -c tests/test_xlsx_named_functions_dynamic.cpp -o tests/test_xlsx_named_functions_dynamic.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_xlsx_named_functions_dynamic tests/test_xlsx_named_functions_dynamic.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	xres -o tests/test_xlsx_named_functions_dynamic $(RSRC)
	./tests/test_xlsx_named_functions_dynamic

# Test della striscia di schede del foglio attivo (Fase 9): nessuna
# vera MainWindow, solo SheetTabView dentro una finestra di prova --
# stessa ragione di test-clipboard (richiede una vera sessione
# grafica, non tocca l'engine).
test-sheet-tabs: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_sheet_tabs.cpp -o tests/test_sheet_tabs.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_sheet_tabs tests/test_sheet_tabs.o \
		src/SheetTabView.o \
		$(INCLUDES) $(CXXFLAGS) -lbe -llocalestub
	./tests/test_sheet_tabs

# Test del troppopieno della toolbar (vedi ToolbarView.h): stessa
# ragione di test-sheet-tabs sopra, nessuna vera MainWindow serve.
test-toolbar-view: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_toolbar_view.cpp -o tests/test_toolbar_view.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_toolbar_view tests/test_toolbar_view.o \
		src/ToolbarView.o \
		$(INCLUDES) $(CXXFLAGS) -lbe
	./tests/test_toolbar_view

# Test dell'apertura di un secondo file mentre Atomo123 e' gia' in
# esecuzione con un documento (richiesto dall'utente dopo la Fase 9):
# deve aprire una finestra nuova, non rimpiazzare quella esistente.
# Ricompila App.cpp a parte con -DATOMO123_TEST_BUILD per escluderne il
# main() (vedi il commento in App.cpp), cosi' il test esercita la vera
# App::RefsReceived/FindReusableWindow invece di una reimplementazione.
test-multiwindow: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_multiwindow.cpp -o tests/test_multiwindow.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/App.cpp -o tests/App_test.o -DATOMO123_TEST_BUILD \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_multiwindow tests/test_multiwindow.o tests/App_test.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		src/SplashWindow.o src/AtomGLView.o src/AtomTheme.o src/TextTexture.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_multiwindow

# Test di Incolla speciale (Fase 7): Solo valori, le quattro
# operazioni aritmetiche con la cella di destinazione, Trasponi.
# Stessa ragione di test-paste-range (serve una vera MainWindow).
test-paste-special: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_paste_special.cpp -o tests/test_paste_special.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_paste_special tests/test_paste_special.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_paste_special

# Test della finestra Preferenze (Fase 7): mostra/nascondi griglia,
# separatore decimale/di elenco applicati davvero al parser di
# formule. Stessa ragione di test-paste-range (serve una vera
# MainWindow).
test-preferences: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_preferences.cpp -o tests/test_preferences.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_preferences tests/test_preferences.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_preferences

# Area di stampa/margini-scala (Fase 27): SetPrintArea/ClearPrintArea/
# HasPrintArea/HandlePageSetupRequest, vedi tests/test_print_settings.cpp.
# PrintLayout.cpp (il calcolo puro di pagine/scala) e' gia' coperto da
# test-print-layout sopra, non ripetuto qui.
test-print-settings: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_print_settings.cpp -o tests/test_print_settings.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_print_settings tests/test_print_settings.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_print_settings

# Test del collegamento reale fra MainWindow::SaveToFile e i
# translator XLSX/ODS (bug segnalato dall'utente: il salvataggio non
# passava mai davvero da loro, scriveva byte ASCD sotto qualunque
# estensione diversa da ".csv"): manda il vero B_SAVE_REQUESTED che
# userebbe un BFilePanel, poi riapre il file scritto con una SECONDA
# MainWindow tramite OpenFile (vero giro BTranslatorRoster). Richiede
# i translator XLSX/ODS gia' installati (make install nelle rispettive
# cartelle), come test-xlsx-named-functions-dynamic.
test-export-formats: $(OBJECTS) $(ENGINE_DIR)/libengine.a $(RSRC)
	$(CXX) -c tests/test_export_formats.cpp -o tests/test_export_formats.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_export_formats tests/test_export_formats.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	xres -o tests/test_export_formats $(RSRC)
	./tests/test_export_formats

# "Salva" (Fase 22), a differenza di "Salva con nome": riscrive lo
# stesso file senza pannello. Serve $(RSRC) come test-export-formats
# sopra (uno degli scenari passa da un vero export XLSX).
test-plain-save: $(OBJECTS) $(ENGINE_DIR)/libengine.a $(RSRC)
	$(CXX) -c tests/test_plain_save.cpp -o tests/test_plain_save.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_plain_save tests/test_plain_save.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	xres -o tests/test_plain_save $(RSRC)
	./tests/test_plain_save

# Salvataggio automatico (Fase 23): scrive un backup, mai il file
# originale -- vedi il commento in cima al test. Serve $(RSRC) come
# test-plain-save sopra (uno scenario passa da un vero export XLSX).
test-autosave: $(OBJECTS) $(ENGINE_DIR)/libengine.a $(RSRC)
	$(CXX) -c tests/test_autosave.cpp -o tests/test_autosave.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_autosave tests/test_autosave.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	xres -o tests/test_autosave $(RSRC)
	./tests/test_autosave

# Esportazione dei grafici incorporati verso XLSX (Fase 24): ciclo
# end-to-end reale (HandleChartInsert + Salva con nome), vedi il
# commento in cima al test. La verifica approfondita dell'XML del
# grafico vive in translators/xlsx/tests/test_xlsx_translator.cpp.
test-chart-export-xlsx: $(OBJECTS) $(ENGINE_DIR)/libengine.a $(RSRC)
	$(CXX) -c tests/test_chart_export_xlsx.cpp -o tests/test_chart_export_xlsx.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_chart_export_xlsx tests/test_chart_export_xlsx.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	xres -o tests/test_chart_export_xlsx $(RSRC)
	./tests/test_chart_export_xlsx

# Test della formattazione grassetto/corsivo/allineamento/colore
# (Fase 7): tutti si applicano a tutto SelectionRange(), non solo
# alla cella attiva. Stessa ragione di test-paste-range (serve una
# vera MainWindow).
test-format: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_format.cpp -o tests/test_format.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_format tests/test_format.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_format

# Pulsanti del gruppo "Formato" nella toolbar (vedi kFormatToolbarButtons
# in MainWindow.cpp): stessa ragione di test-format sopra (serve una
# vera MainWindow), ma passando dal vero BButton invece che da una
# chiamata diretta al metodo.
test-format-toolbar: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_format_toolbar.cpp -o tests/test_format_toolbar.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_format_toolbar tests/test_format_toolbar.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_format_toolbar

# Test dei cinque pulsanti Fase 13 (hyperlink/commento/vai a/intervalli
# con nome/colore bordo) e della nuova toolbar su piu' righe per
# categoria: stessa ragione headless-ma-con-vera-MainWindow di
# test-format-toolbar sopra.
test-toolbar-phase13: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_toolbar_phase13.cpp -o tests/test_toolbar_phase13.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_toolbar_phase13 tests/test_toolbar_phase13.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_toolbar_phase13

# Test di Blocca riquadri (Fase 7): righe/colonne bloccate restano
# ferme sullo schermo durante lo scroll, CellAt() risolve un clic
# sulla banda bloccata alla cella giusta anche a foglio scorso.
# Stessa ragione di test-scroll (richiede una vera sessione grafica,
# stesso schema di finestra di prova).
test-freeze: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_freeze.cpp -o tests/test_freeze.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_freeze tests/test_freeze.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_freeze

# Test della visualizzazione delle formule con "=" iniziale (Fase 13):
# stessa ragione di test-goto (serve una vera MainWindow, la barra
# della formula si aggiorna solo tramite il ciclo di notifica reale
# innescato da SheetView::SetSelection).
test-formula-display: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_formula_display.cpp -o tests/test_formula_display.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_formula_display tests/test_formula_display.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_formula_display

# Test del footer Somma/Media/Massimo/Conteggio della selezione: stessa
# ragione di test-formula-display sopra (serve una vera MainWindow).
test-selection-stats: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_selection_stats.cpp -o tests/test_selection_stats.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_selection_stats tests/test_selection_stats.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_selection_stats

# Test che le barre di scorrimento della griglia siano davvero dentro
# l'area visibile: stessa ragione di test-formula-display sopra (serve
# una vera MainWindow).
test-scrollbars-visible: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_scrollbars_visible.cpp -o tests/test_scrollbars_visible.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_scrollbars_visible tests/test_scrollbars_visible.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_scrollbars_visible

# Test dei commenti/note per cella (Fase 13): stessa ragione di
# test-sheet-management sotto (serve una vera MainWindow), piu' una
# verifica sui pixel veri del triangolo rosso di indicazione, stesso
# principio di test-merge-click.
test-comments: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_comments.cpp -o tests/test_comments.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_comments tests/test_comments.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_comments

# Test dei collegamenti ipertestuali (Fase 13): stesso schema di
# test-comments sopra, piu' una verifica sui pixel veri del testo blu
# sottolineato (stesso principio del triangolo rosso dei commenti).
test-hyperlinks: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_hyperlinks.cpp -o tests/test_hyperlinks.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_hyperlinks tests/test_hyperlinks.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_hyperlinks

# Test della convalida dati (Fase 13): stesso schema di test-comments/
# test-hyperlinks sopra, piu' la verifica che un valore respinto non
# venga davvero scritto nella cella (SheetView::CommitEditing).
test-validation: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_validation.cpp -o tests/test_validation.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_validation tests/test_validation.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_validation

# Test della formattazione condizionale VIVA (Fase 13): stesso schema
# di test-validation sopra (ApplyXToSelection/RemoveAllX), piu' la
# verifica sui pixel veri che il colore si ricalcola davvero a ogni
# ridisegno (non solo che la regola sia stata aggiunta).
test-condformat: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_condformat.cpp -o tests/test_condformat.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_condformat tests/test_condformat.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_condformat

# Test di Nuovo/Elimina/Rinomina foglio (Fase 13): stessa ragione di
# test-goto sotto (serve una vera MainWindow). DeleteSheetNoConfirm,
# non DeleteSheet: quest'ultima mostra un vero BAlert di conferma, che
# bloccherebbe il test (vedi il commento in cima al file di test).
test-sheet-management: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_sheet_management.cpp -o tests/test_sheet_management.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_sheet_management tests/test_sheet_management.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_sheet_management

# Test di Trova successivo/Sostituisci/Sostituisci tutto (Fase 16):
# nessuna copertura esisteva prima (vedi memoria
# project_command_audit_20260808), stessa ragione di test-sheet-management
# sopra (serve una vera MainWindow).
test-find-replace: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_find_replace.cpp -o tests/test_find_replace.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_find_replace tests/test_find_replace.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_find_replace

# Test di Inserisci Grafico come vero comando di MainWindow (Fase 16,
# distinto da test-chart sopra, che verifica solo Chart.h/.cpp senza
# mai passare da MainWindow::HandleChartInsert): nessuna copertura
# esisteva prima (vedi memoria project_command_audit_20260808).
test-insert-chart: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_insert_chart.cpp -o tests/test_insert_chart.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_insert_chart tests/test_insert_chart.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_insert_chart

# Test di "Vai a" (Fase 7): sposta la selezione su una cella singola
# o un intervallo digitato, un testo non valido non fa nulla. Stessa
# ragione di test-paste-range (serve una vera MainWindow).
test-goto: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_goto.cpp -o tests/test_goto.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_goto tests/test_goto.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_goto

# Test degli intervalli con nome (Fase 7) su una vera MainWindow:
# HandleDefineName/HandleDeleteName/HandleGoToName, chiamati
# direttamente come CopySelection/PasteSelection in
# test-paste-range. Stessa ragione di test-paste-range (serve una
# vera MainWindow, non il solo motore -- vedi
# engine/tests/named_ranges_test.cpp per quello).
test-names: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_names.cpp -o tests/test_names.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_names tests/test_names.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/PrintLayout.o src/IconCatalog.o src/IconData.o src/AutoFill.o src/FileTypeIcons.o src/ProgressWindow.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/RenameSheetWindow.o src/CommentWindow.o src/HyperlinkWindow.o src/ValidationWindow.o src/ConditionalFormatWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o src/BorderWindow.o src/PageSetupWindow.o src/PrintPreviewView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_names

clean:
	rm -f $(OBJECTS:.o=.d) tests/*.d
	rm -f $(APP) $(OBJECTS) $(RSRC) $(BUILD_INFO_H) tests/test_ascd_io tests/test_ascd_io.o tests/AscdIO.o \
		tests/roundtrip.ascd tests/roundtrip_charts.ascd tests/roundtrip_widths.ascd \
		tests/roundtrip_colors.ascd tests/roundtrip_border_color.ascd \
		tests/roundtrip_condformat.ascd \
		tests/roundtrip_ambiguous_text.ascd tests/edge.ascd \
		tests/test_clipboard tests/test_clipboard.o \
		tests/test_scroll tests/test_scroll.o \
		tests/test_chart tests/test_chart.o tests/Chart.o \
		tests/test_range_ref tests/test_range_ref.o tests/RangeRef.o \
		tests/test_print_layout tests/test_print_layout.o tests/PrintLayout.o \
		tests/test_pivot tests/test_pivot.o tests/Pivot.o \
		tests/test_editing tests/test_editing.o \
		tests/test_real_input_edit tests/test_real_input_edit.o \
		tests/test_navigation tests/test_navigation.o \
		tests/test_selection tests/test_selection.o \
		tests/test_image_alpha tests/test_image_alpha.o \
		tests/test_fill tests/test_fill.o \
		tests/test_sort tests/test_sort.o \
		tests/test_undo tests/test_undo.o \
		tests/test_paste_range tests/test_paste_range.o \
		tests/test_insert_delete tests/test_insert_delete.o \
		tests/test_unsaved_changes tests/test_unsaved_changes.o \
		tests/test_resize tests/test_resize.o \
		tests/test_autofilter tests/test_autofilter.o \
		tests/test_image_drag tests/test_image_drag.o \
		tests/test_image_resize tests/test_image_resize.o \
		tests/test_merge_click tests/test_merge_click.o \
		tests/test_ascd_book tests/test_ascd_book.o tests/AscdIO_book.o \
		tests/test_multisheet tests/test_multisheet.o \
		tests/test_recent_files tests/test_recent_files.o \
		tests/test_xsheet_formulas tests/test_xsheet_formulas.o /tmp/test_xsheet_formulas.ascd \
		tests/test_sheet_tabs tests/test_sheet_tabs.o \
		tests/test_multiwindow tests/test_multiwindow.o tests/App_test.o \
		/tmp/test_multiwindow_1.ascd /tmp/test_multiwindow_2.ascd \
		tests/test_names tests/test_names.o \
		tests/test_paste_special tests/test_paste_special.o \
		tests/test_goto tests/test_goto.o \
		tests/test_sheet_management tests/test_sheet_management.o \
		tests/test_find_replace tests/test_find_replace.o \
		tests/test_insert_chart tests/test_insert_chart.o \
		tests/test_comments tests/test_comments.o tests/roundtrip_comments.ascd \
		tests/test_hyperlinks tests/test_hyperlinks.o tests/roundtrip_hyperlinks.ascd \
		tests/test_validation tests/test_validation.o tests/roundtrip_validation.ascd \
		tests/test_condformat tests/test_condformat.o \
		tests/test_formula_display tests/test_formula_display.o \
		tests/test_freeze tests/test_freeze.o \
		tests/test_format tests/test_format.o \
		tests/test_preferences tests/test_preferences.o \
		tests/test_persistence tests/test_persistence.o tests/AscdIO_persistence.o \
		/tmp/test_persistence.ascd /tmp/test_persistence_old.ascd \
		tests/test_borders tests/test_borders.o \
		tests/test_locale_catalog tests/test_locale_catalog.o \
		tests/test_export_formats tests/test_export_formats.o \
		/tmp/test_export_formats.xlsx /tmp/test_export_formats.ods \
		examples/generate_demo examples/generate_demo.o examples/AscdIO_demo.o \
		examples/atomo123_demo.ascd \
		$(CATKEYS_DIR)/atomo123.pre $(CATKEYS_DIR)/it.catkeys

.PHONY: all run catkeys demo test test-ascd-book test-chart test-range-ref test-print-layout test-chart-resize test-chart-delete test-pivot test-clipboard test-scroll test-editing test-real-input-edit test-navigation test-selection test-image-alpha test-fill test-sort test-undo test-paste-range test-insert-delete test-unsaved-changes test-resize test-autofilter test-image-drag test-image-resize test-image-delete test-image-export-drag test-image-autoscroll test-chart-drag test-merge-click test-multisheet test-xsheet-formulas test-xlsx-named-functions-dynamic test-locale-catalog test-export-formats test-sheet-tabs test-toolbar-view test-multiwindow test-names test-paste-special test-goto test-sheet-management test-find-replace test-insert-chart test-comments test-hyperlinks test-validation test-condformat test-formula-display test-selection-stats test-scrollbars-visible test-freeze test-format test-format-toolbar test-preferences test-persistence test-borders test-overflow test-currency-format test-clear-keeps-style test-plain-save test-autosave test-chart-export-xlsx clean

# Include i file di dipendenza generati da -MMD/-MP (vedi CXXFLAGS in
# cima) -- src/*.d per la build principale, tests/*.d per ogni target
# di test che compila il proprio .o. In FONDO al file apposta: "include"
# spezza il testo del file incluso esattamente in quel punto del parse,
# e la PRIMA regola vera che make incontra ovunque nel Makefile diventa
# il goal di default per un "make" senza argomenti -- messo in cima
# (come un primo tentativo qui) faceva diventare "src/App.o" (la prima
# regola dentro un .d incluso) il goal di default invece di "all" sopra,
# silenziosamente: un "make" senza argomenti ricompilava un solo file
# invece di tutti quelli che dipendevano dall'header appena cambiato
# (bug reale scoperto subito dopo aver aggiunto questo tracciamento
# delle dipendenze, vedi il commento su CXXFLAGS). Il "-" iniziale
# rende l'include silenzioso quando i .d non esistono ancora (prima
# build, o dopo "make clean").
-include $(OBJECTS:.o=.d)
-include $(wildcard tests/*.d)
