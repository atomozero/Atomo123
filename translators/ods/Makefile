# Translator ODS — add-on Translation Kit di Haiku.
#
# Compila OdsTranslator.cpp + MiniZip.cpp in un add-on installabile
# (shared object) che usa il motore di calcolo isolato
# (../../engine/libengine.a), lo stesso lettore ZIP minimale del
# translator XLSX (MiniZip.*, sopra zlib) ed expat per il parsing XML
# (entrambi gia' presenti su Haiku, nessuna nuova dipendenza di
# sistema da installare).

CXX := g++
# -O2 (Fase 34): vedi il commento in ../../engine/Makefile.
CXXFLAGS := -std=gnu++17 -Wall -g -O2 -fPIC
ENGINE_DIR := ../../engine
ENGINE_SRCDIRS := $(ENGINE_DIR)/src/Cell $(ENGINE_DIR)/src/Formula \
	$(ENGINE_DIR)/src/Functions $(ENGINE_DIR)/src/Excel \
	$(ENGINE_DIR)/src/FileSys $(ENGINE_DIR)/src/Collections \
	$(ENGINE_DIR)/src/Misc-Classes $(ENGINE_DIR)/src/Utils \
	$(ENGINE_DIR)/src/Config $(ENGINE_DIR)/src/bsl \
	$(ENGINE_DIR)/src/Metrowerks $(ENGINE_DIR)/src/Stubs
INCLUDES := $(addprefix -I, $(ENGINE_SRCDIRS)) -I.

LIBS := -L$(ENGINE_DIR) -lengine -lbe -ltranslation -lexpat -lz

ADDON := OdsTranslator
SOURCES := OdsTranslator.cpp MiniZip.cpp

# Risorsa 'Func' per InitFunctions() nel test: senza questa,
# GetFunctionNr tratta OGNI nome di funzione (SUM incluso) come
# identificatore sconosciuto (gFuncCount resta 0) e il parser lancia
# CParseErr per "funzione sconosciuta" appena una formula esportata
# con separatori canonici usa una funzione con nome -- stesso principio
# di translators/xlsx/Makefile (FUNC_TEST_RSRC), che serve qui da
# quando il test dell'export con formule vive usa anche lui SUM().
LEGACY_DIR := ../../legacy/opensumit
REZ := $(LEGACY_DIR)/rez/rez
FUNC_TEST_RSRC := tests/named_functions.rsrc

# Localizzazione (Locale Kit) della vista Informazioni: vedi il
# commento analogo in translators/csv/Makefile.
CATALOG_SIG := application/x-vnd.Atomo-OdsTranslator
CATKEYS_DIR := locales
EN_CATKEYS := $(CATKEYS_DIR)/en.catkeys

all: $(ADDON)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(REZ):
	$(MAKE) -C $(LEGACY_DIR)/rez

$(FUNC_TEST_RSRC): $(REZ) $(ENGINE_DIR)/resources/funcs_by_nr.r
	rm -f $(FUNC_TEST_RSRC)
	$(REZ) -o $(FUNC_TEST_RSRC) $(ENGINE_DIR)/resources/funcs_by_nr.r

$(ADDON): $(SOURCES) $(ENGINE_DIR)/libengine.a $(EN_CATKEYS)
	$(CXX) -shared -o $(ADDON) $(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l en $(EN_CATKEYS)

install: $(ADDON)
	mkdir -p ~/config/non-packaged/add-ons/Translators
	cp $(ADDON) ~/config/non-packaged/add-ons/Translators/

# Passaggio manuale, non nel giro normale di "make": vedi il commento
# analogo in translators/csv/Makefile.
catkeys: OdsTranslator.cpp
	-cat OdsTranslator.cpp | $(CXX) -E -x c++ $(INCLUDES) $(CXXFLAGS) -DB_COLLECTING_CATKEYS - | /bin/grep -av '^#' > $(CATKEYS_DIR)/ods.pre
	collectcatkeys -s $(CATALOG_SIG) $(CATKEYS_DIR)/ods.pre -o $(CATKEYS_DIR)/it.catkeys
	rm -f $(CATKEYS_DIR)/ods.pre

test: $(ADDON) $(FUNC_TEST_RSRC)
	$(CXX) -c tests/test_ods_translator.cpp -o tests/test_ods_translator.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_ods_translator tests/test_ods_translator.o \
		$(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_ods_translator

clean:
	rm -f $(ADDON) tests/test_ods_translator tests/test_ods_translator.o $(FUNC_TEST_RSRC) \
		$(CATKEYS_DIR)/it.catkeys $(CATKEYS_DIR)/ods.pre

.PHONY: all install test catkeys clean
