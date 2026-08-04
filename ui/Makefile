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
CXXFLAGS := -std=gnu++17 -Wall -g
ENGINE_DIR := ../engine
ENGINE_SRCDIRS := $(ENGINE_DIR)/src/Cell $(ENGINE_DIR)/src/Formula \
	$(ENGINE_DIR)/src/Functions $(ENGINE_DIR)/src/Excel \
	$(ENGINE_DIR)/src/FileSys $(ENGINE_DIR)/src/Collections \
	$(ENGINE_DIR)/src/Misc-Classes $(ENGINE_DIR)/src/Utils \
	$(ENGINE_DIR)/src/Config $(ENGINE_DIR)/src/bsl \
	$(ENGINE_DIR)/src/Metrowerks $(ENGINE_DIR)/src/Stubs
INCLUDES := $(addprefix -I, $(ENGINE_SRCDIRS)) -Isrc

LIBS := -L$(ENGINE_DIR) -lengine -lbe -ltranslation -ltracker

APP := Atomo123
SOURCES := src/App.cpp src/MainWindow.cpp src/SheetView.cpp src/SheetTabView.cpp \
	src/ToolbarView.cpp \
	src/AscdIO.cpp src/FindWindow.cpp src/ChartWindow.cpp src/ChartView.cpp src/Chart.cpp \
	src/PivotWindow.cpp src/Pivot.cpp src/RangeRef.cpp src/IconCatalog.cpp src/IconData.cpp \
	src/NameWindow.cpp src/PasteSpecialWindow.cpp src/GoToWindow.cpp src/ColorWindow.cpp \
	src/PreferencesWindow.cpp src/AboutWindow.cpp src/ClickableStringView.cpp
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

all: $(APP)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(REZ):
	$(MAKE) -C $(LEGACY_DIR)/rez

$(BSL):
	$(MAKE) -C $(LEGACY_DIR)/bsl

%.o: %.cpp
	$(CXX) -c $< -o $@ $(INCLUDES) $(CXXFLAGS)

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
$(APP): $(OBJECTS) $(ENGINE_DIR)/libengine.a $(RSRC)
	$(CXX) -o $(APP) $(OBJECTS) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	xres -o $(APP) $(RSRC)

run: $(APP)
	./$(APP)

# Test di round-trip di AscdIO (SaveASCD/LoadASCD): non richiede una
# sessione grafica, solo l'engine e libbe per BFile/BPositionIO.
test: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_ascd_io.cpp -o tests/test_ascd_io.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/AscdIO.cpp -o tests/AscdIO.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_ascd_io tests/test_ascd_io.o tests/AscdIO.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe
	./tests/test_ascd_io

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

# Test dei bordi di cella (Fase 11): un lato alla volta, applicati a
# tutta la selezione, indipendenti fra loro, ClearBorders. Stessa
# ragione di test-paste-range (serve una vera MainWindow).
test-borders: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_borders.cpp -o tests/test_borders.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_borders tests/test_borders.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_overflow

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
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe
	./tests/test_chart

# Test della logica della tabella pivot (Pivot.cpp): stessa ragione di
# test-chart, headless.
test-pivot: $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_pivot.cpp -o tests/test_pivot.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -c src/Pivot.cpp -o tests/Pivot.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_pivot tests/test_pivot.o tests/Pivot.o \
		$(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_undo

# Test di Taglia/Copia/Incolla e Formato estesi all'intervallo
# selezionato (Fase 7): richiede una vera MainWindow (non la sola
# SheetView), vedi il commento in cima al test.
test-paste-range: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_paste_range.cpp -o tests/test_paste_range.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_paste_range tests/test_paste_range.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_resize

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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_multisheet

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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_xsheet_formulas

# Test della striscia di schede del foglio attivo (Fase 9): nessuna
# vera MainWindow, solo SheetTabView dentro una finestra di prova --
# stessa ragione di test-clipboard (richiede una vera sessione
# grafica, non tocca l'engine).
test-sheet-tabs: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_sheet_tabs.cpp -o tests/test_sheet_tabs.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_sheet_tabs tests/test_sheet_tabs.o \
		src/SheetTabView.o \
		$(INCLUDES) $(CXXFLAGS) -lbe
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_preferences

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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_format_toolbar

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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_freeze

# Test di "Vai a" (Fase 7): sposta la selezione su una cella singola
# o un intervallo digitato, un testo non valido non fa nulla. Stessa
# ragione di test-paste-range (serve una vera MainWindow).
test-goto: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_goto.cpp -o tests/test_goto.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_goto tests/test_goto.o \
		src/MainWindow.o src/SheetView.o src/SheetTabView.o src/ToolbarView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
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
		src/PivotWindow.o src/Pivot.o src/RangeRef.o src/IconCatalog.o src/IconData.o \
		src/NameWindow.o src/PasteSpecialWindow.o src/GoToWindow.o src/ColorWindow.o \
		src/PreferencesWindow.o src/AboutWindow.o src/ClickableStringView.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_names

clean:
	rm -f $(APP) $(OBJECTS) $(RSRC) tests/test_ascd_io tests/test_ascd_io.o tests/AscdIO.o \
		tests/roundtrip.ascd tests/roundtrip_charts.ascd tests/roundtrip_widths.ascd \
		tests/roundtrip_colors.ascd tests/edge.ascd \
		tests/test_clipboard tests/test_clipboard.o \
		tests/test_scroll tests/test_scroll.o \
		tests/test_chart tests/test_chart.o tests/Chart.o \
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
		tests/test_freeze tests/test_freeze.o \
		tests/test_format tests/test_format.o \
		tests/test_preferences tests/test_preferences.o \
		tests/test_persistence tests/test_persistence.o tests/AscdIO_persistence.o \
		/tmp/test_persistence.ascd /tmp/test_persistence_old.ascd \
		tests/test_borders tests/test_borders.o

.PHONY: all run test test-ascd-book test-chart test-pivot test-clipboard test-scroll test-editing test-real-input-edit test-navigation test-selection test-image-alpha test-fill test-sort test-undo test-paste-range test-insert-delete test-unsaved-changes test-resize test-multisheet test-xsheet-formulas test-sheet-tabs test-toolbar-view test-multiwindow test-names test-paste-special test-goto test-freeze test-format test-format-toolbar test-preferences test-persistence test-borders test-overflow test-currency-format clean
