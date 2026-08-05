# Translator XLSX — add-on Translation Kit di Haiku.
#
# Compila XlsxTranslator.cpp + MiniZip.cpp in un add-on installabile
# (shared object) che usa il motore di calcolo isolato
# (../../engine/libengine.a), il lettore ZIP minimale scritto per
# questo progetto (MiniZip.*, sopra zlib) ed expat per il parsing XML
# (entrambi gia' presenti su Haiku, nessuna nuova dipendenza di
# sistema da installare).

CXX := g++
CXXFLAGS := -std=gnu++17 -Wall -g -fPIC
ENGINE_DIR := ../../engine
ENGINE_SRCDIRS := $(ENGINE_DIR)/src/Cell $(ENGINE_DIR)/src/Formula \
	$(ENGINE_DIR)/src/Functions $(ENGINE_DIR)/src/Excel \
	$(ENGINE_DIR)/src/FileSys $(ENGINE_DIR)/src/Collections \
	$(ENGINE_DIR)/src/Misc-Classes $(ENGINE_DIR)/src/Utils \
	$(ENGINE_DIR)/src/Config $(ENGINE_DIR)/src/bsl \
	$(ENGINE_DIR)/src/Metrowerks $(ENGINE_DIR)/src/Stubs
INCLUDES := $(addprefix -I, $(ENGINE_SRCDIRS)) -I.

LIBS := -L$(ENGINE_DIR) -lengine -lbe -ltranslation -lexpat -lz

ADDON := XlsxTranslator
SOURCES := XlsxTranslator.cpp MiniZip.cpp

# Risorsa 'Func' per InitFunctions() nel test (Fase 13): senza questa,
# GetFunctionNr tratta OGNI nome di funzione come identificatore
# sconosciuto (gFuncCount resta 0), quindi IF/VLOOKUP/IFERROR nelle
# formule importate da un file XLSX reale non venivano mai calcolati
# nel test -- mai notato prima perche' nessun controllo qui aveva mai
# verificato il VALORE calcolato di una formula con funzione con nome,
# solo stile/formattazione. Stesso principio di engine/Makefile
# (FUNC_TEST_RSRC).
LEGACY_DIR := ../../legacy/opensumit
REZ := $(LEGACY_DIR)/rez/rez
BSL := $(LEGACY_DIR)/bsl/bsl
FUNC_TEST_RSRC := tests/named_functions.rsrc

all: $(ADDON)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(REZ):
	$(MAKE) -C $(LEGACY_DIR)/rez

$(BSL):
	$(MAKE) -C $(LEGACY_DIR)/bsl

$(FUNC_TEST_RSRC): $(REZ) $(BSL) $(ENGINE_DIR)/resources/funcs_by_nr.r
	rm -f $(FUNC_TEST_RSRC)
	$(REZ) -o $(FUNC_TEST_RSRC) $(ENGINE_DIR)/resources/funcs_by_nr.r

$(ADDON): $(SOURCES) $(ENGINE_DIR)/libengine.a
	$(CXX) -shared -o $(ADDON) $(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)

install: $(ADDON)
	mkdir -p ~/config/non-packaged/add-ons/Translators
	cp $(ADDON) ~/config/non-packaged/add-ons/Translators/

test: $(ADDON) $(FUNC_TEST_RSRC)
	$(CXX) -c tests/test_xlsx_translator.cpp -o tests/test_xlsx_translator.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_xlsx_translator tests/test_xlsx_translator.o \
		$(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_xlsx_translator

clean:
	rm -f $(ADDON) tests/test_xlsx_translator tests/test_xlsx_translator.o $(FUNC_TEST_RSRC)

.PHONY: all install test clean
