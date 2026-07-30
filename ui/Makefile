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
SOURCES := src/App.cpp src/MainWindow.cpp src/SheetView.cpp src/AscdIO.cpp src/FindWindow.cpp
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
		$(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_scroll

clean:
	rm -f $(APP) $(OBJECTS) $(RSRC) tests/test_ascd_io tests/test_ascd_io.o tests/AscdIO.o \
		tests/roundtrip.ascd tests/test_clipboard tests/test_clipboard.o \
		tests/test_scroll tests/test_scroll.o

.PHONY: all run test test-clipboard test-scroll clean
