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
SOURCES := src/App.cpp src/MainWindow.cpp src/SheetView.cpp src/AscdIO.cpp \
	src/FindWindow.cpp src/ChartWindow.cpp src/ChartView.cpp src/Chart.cpp \
	src/PivotWindow.cpp src/Pivot.cpp src/RangeRef.cpp
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
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_scroll

# Test dell'editing in-cella (digitare su una cella, confermare con
# Invio/annullare con Escape): stessa ragione di test-scroll (richiede
# una vera sessione grafica per BTextControl), target separato.
test-editing: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_editing.cpp -o tests/test_editing.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_editing tests/test_editing.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_editing

# Test delle scorciatoie da tastiera in stile Excel (Inizio/Ctrl+
# Inizio/Ctrl+Fine/PagSu/PagGiu/Maiusc+Tab/Maiusc+Invio): stessa
# ragione di test-scroll (BScrollView reale per PagSu/PagGiu, che
# leggono Parent()->Bounds()), target separato.
test-navigation: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_navigation.cpp -o tests/test_navigation.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_navigation tests/test_navigation.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_navigation

# Test della selezione multi-cella (Fase 7): Maiusc+frecce,
# trascinamento del mouse, Ctrl+A, Backspace/Canc su un intervallo
# intero -- stessa ragione di test-navigation (BScrollView reale).
test-selection: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_selection.cpp -o tests/test_selection.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_selection tests/test_selection.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_selection

# Test di Riempi in basso/a destra (Fase 7): stessa ragione di
# test-selection (BScrollView reale).
test-fill: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_fill.cpp -o tests/test_fill.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_fill tests/test_fill.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_fill

# Test di Ordina crescente/decrescente (Fase 7): stessa ragione di
# test-selection (BScrollView reale).
test-sort: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_sort.cpp -o tests/test_sort.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_sort tests/test_sort.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_sort

# Test di Annulla/Ripeti (Fase 7): stessa ragione di test-selection
# (BScrollView reale).
test-undo: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_undo.cpp -o tests/test_undo.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_undo tests/test_undo.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_undo

# Test di Taglia/Copia/Incolla e Formato estesi all'intervallo
# selezionato (Fase 7): richiede una vera MainWindow (non la sola
# SheetView), vedi il commento in cima al test.
test-paste-range: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_paste_range.cpp -o tests/test_paste_range.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_paste_range tests/test_paste_range.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_paste_range

# Test di Inserisci/Elimina riga e colonna (Fase 7): stessa ragione di
# test-selection (BScrollView reale).
test-insert-delete: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_insert_delete.cpp -o tests/test_insert_delete.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_insert_delete tests/test_insert_delete.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_insert_delete

# Test della protezione dalle modifiche non salvate (dirty flag +
# titolo + conferma prima di scartare): richiede una vera MainWindow,
# stessa ragione di test-paste-range.
test-unsaved-changes: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_unsaved_changes.cpp -o tests/test_unsaved_changes.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_unsaved_changes tests/test_unsaved_changes.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_unsaved_changes

# Test del ridimensionamento riga/colonna (Fase 8): stessa ragione di
# test-selection (BScrollView reale).
test-resize: $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -c tests/test_resize.cpp -o tests/test_resize.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_resize tests/test_resize.o \
		src/MainWindow.o src/SheetView.o src/AscdIO.o src/FindWindow.o \
		src/ChartWindow.o src/ChartView.o src/Chart.o \
		src/PivotWindow.o src/Pivot.o src/RangeRef.o \
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_resize

clean:
	rm -f $(APP) $(OBJECTS) $(RSRC) tests/test_ascd_io tests/test_ascd_io.o tests/AscdIO.o \
		tests/roundtrip.ascd tests/roundtrip_charts.ascd tests/edge.ascd \
		tests/test_clipboard tests/test_clipboard.o \
		tests/test_scroll tests/test_scroll.o \
		tests/test_chart tests/test_chart.o tests/Chart.o \
		tests/test_pivot tests/test_pivot.o tests/Pivot.o \
		tests/test_editing tests/test_editing.o \
		tests/test_navigation tests/test_navigation.o \
		tests/test_selection tests/test_selection.o \
		tests/test_fill tests/test_fill.o \
		tests/test_sort tests/test_sort.o \
		tests/test_undo tests/test_undo.o \
		tests/test_paste_range tests/test_paste_range.o \
		tests/test_insert_delete tests/test_insert_delete.o \
		tests/test_unsaved_changes tests/test_unsaved_changes.o \
		tests/test_resize tests/test_resize.o

.PHONY: all run test test-chart test-pivot test-clipboard test-scroll test-editing test-navigation test-selection test-fill test-sort test-undo test-paste-range test-insert-delete test-unsaved-changes test-resize clean
