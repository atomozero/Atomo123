# Translator XLS legacy — add-on Translation Kit di Haiku.
#
# Compila XlsTranslator.cpp in un add-on installabile (shared object)
# che usa il motore di calcolo isolato (../../engine/libengine.a).

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
INCLUDES := $(addprefix -I, $(ENGINE_SRCDIRS))

ADDON := XlsTranslator

# Localizzazione (Locale Kit) della vista Informazioni: vedi il
# commento analogo in translators/csv/Makefile.
CATALOG_SIG := application/x-vnd.Atomo-XlsTranslator
CATKEYS_DIR := locales
EN_CATKEYS := $(CATKEYS_DIR)/en.catkeys

all: $(ADDON)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(ADDON): XlsTranslator.cpp $(ENGINE_DIR)/libengine.a $(EN_CATKEYS)
	$(CXX) -shared -o $(ADDON) XlsTranslator.cpp $(INCLUDES) $(CXXFLAGS) \
		-L$(ENGINE_DIR) -lengine -lbe -ltranslation
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l en $(EN_CATKEYS)

# "cp" seguito da "mv" nella stessa cartella, non "cp" diretto sul
# file gia' installato: un vero crash reale di Tracker (il suo
# thumbnail worker chiama BTranslatorRoster::Translate() su ogni file
# del Desktop in background, in qualunque momento) ha eseguito codice
# a meta' sovrascritto durante un "cp" diretto -- "mv" sullo stesso
# filesystem e' una rinomina atomica (rename(2)), un lettore concorrente
# vede sempre o il file vecchio intero o quello nuovo intero, mai uno
# stato intermedio.
install: $(ADDON)
	mkdir -p ~/config/non-packaged/add-ons/Translators
	cp $(ADDON) ~/config/non-packaged/add-ons/Translators/$(ADDON).new
	mv ~/config/non-packaged/add-ons/Translators/$(ADDON).new ~/config/non-packaged/add-ons/Translators/$(ADDON)

# Passaggio manuale, non nel giro normale di "make": vedi il commento
# analogo in translators/csv/Makefile.
catkeys: XlsTranslator.cpp
	-cat XlsTranslator.cpp | $(CXX) -E -x c++ $(INCLUDES) $(CXXFLAGS) -DB_COLLECTING_CATKEYS - | /bin/grep -av '^#' > $(CATKEYS_DIR)/xls.pre
	collectcatkeys -s $(CATALOG_SIG) $(CATKEYS_DIR)/xls.pre -o $(CATKEYS_DIR)/it.catkeys
	rm -f $(CATKEYS_DIR)/xls.pre

test: $(ADDON)
	$(CXX) -c tests/test_xls_translator.cpp -o tests/test_xls_translator.o -I. \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_xls_translator tests/test_xls_translator.o \
		XlsTranslator.cpp $(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -ltranslation
	./tests/test_xls_translator

clean:
	rm -f $(ADDON) tests/test_xls_translator tests/test_xls_translator.o \
		$(CATKEYS_DIR)/it.catkeys $(CATKEYS_DIR)/xls.pre

.PHONY: all install test catkeys clean
