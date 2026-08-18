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
#
# Sul VERO add-on installato (non solo nel test) serve lo stesso fix,
# ma non un secondo file di risorsa: questo translator .so ha la
# propria copia INDIPENDENTE di gFuncCount rispetto all'eseguibile
# principale (engine/ e' una libreria statica), ma EnsureFunctionsInitialized
# (FunctionUtils.h/.cpp, chiamata da Translate() prima di analizzare
# qualunque formula) trova da sola l'eseguibile ospite tramite be_app
# e riusa la SUA risorsa 'Func' (Atomo123.rsrc) -- stesso principio
# gia' in uso per il vecchio import XLS in Excel.cpp, prima che questo
# stesso bug si ripresentasse identico per XLSX.
LEGACY_DIR := ../../legacy/opensumit
REZ := $(LEGACY_DIR)/rez/rez
BSL := $(LEGACY_DIR)/bsl/bsl
FUNC_TEST_RSRC := tests/named_functions.rsrc

# Localizzazione (Locale Kit) della vista Informazioni: vedi il
# commento analogo in translators/csv/Makefile.
CATALOG_SIG := application/x-vnd.Atomo-XlsxTranslator
CATKEYS_DIR := locales
EN_CATKEYS := $(CATKEYS_DIR)/en.catkeys

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

$(ADDON): $(SOURCES) $(ENGINE_DIR)/libengine.a $(EN_CATKEYS)
	$(CXX) -shared -o $(ADDON) $(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l en $(EN_CATKEYS)

install: $(ADDON)
	mkdir -p ~/config/non-packaged/add-ons/Translators
	cp $(ADDON) ~/config/non-packaged/add-ons/Translators/

# Passaggio manuale, non nel giro normale di "make": vedi il commento
# analogo in translators/csv/Makefile.
catkeys: XlsxTranslator.cpp
	-cat XlsxTranslator.cpp | $(CXX) -E -x c++ $(INCLUDES) $(CXXFLAGS) -DB_COLLECTING_CATKEYS - | /bin/grep -av '^#' > $(CATKEYS_DIR)/xlsx.pre
	collectcatkeys -s $(CATALOG_SIG) $(CATKEYS_DIR)/xlsx.pre -o $(CATKEYS_DIR)/it.catkeys
	rm -f $(CATKEYS_DIR)/xlsx.pre

test: $(ADDON) $(FUNC_TEST_RSRC)
	$(CXX) -c tests/test_xlsx_translator.cpp -o tests/test_xlsx_translator.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_xlsx_translator tests/test_xlsx_translator.o \
		$(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_xlsx_translator

clean:
	rm -f $(ADDON) tests/test_xlsx_translator tests/test_xlsx_translator.o $(FUNC_TEST_RSRC) \
		$(CATKEYS_DIR)/it.catkeys $(CATKEYS_DIR)/xlsx.pre

.PHONY: all install test catkeys clean
