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
SOURCES := src/App.cpp src/MainWindow.cpp src/SheetView.cpp src/AscdIO.cpp
OBJECTS := $(SOURCES:.cpp=.o)

all: $(APP)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

%.o: %.cpp
	$(CXX) -c $< -o $@ $(INCLUDES) $(CXXFLAGS)

$(APP): $(OBJECTS) $(ENGINE_DIR)/libengine.a
	$(CXX) -o $(APP) $(OBJECTS) $(INCLUDES) $(CXXFLAGS) $(LIBS)

run: $(APP)
	./$(APP)

clean:
	rm -f $(APP) $(OBJECTS)

.PHONY: all run clean
