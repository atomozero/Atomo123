# Translator CSV/ASCD — add-on Translation Kit di Haiku.
#
# Compila CsvTranslator.cpp in un add-on installabile (shared object)
# che usa il motore di calcolo isolato (../../engine/libengine.a).

CXX := g++
CXXFLAGS := -std=gnu++17 -Wall -g -fPIC
ENGINE_DIR := ../../engine
ENGINE_SRCDIRS := $(ENGINE_DIR)/src/Cell $(ENGINE_DIR)/src/Formula \
	$(ENGINE_DIR)/src/Functions $(ENGINE_DIR)/src/Excel \
	$(ENGINE_DIR)/src/FileSys $(ENGINE_DIR)/src/Collections \
	$(ENGINE_DIR)/src/Misc-Classes $(ENGINE_DIR)/src/Utils \
	$(ENGINE_DIR)/src/Config $(ENGINE_DIR)/src/bsl \
	$(ENGINE_DIR)/src/Metrowerks $(ENGINE_DIR)/src/Stubs
INCLUDES := $(addprefix -I, $(ENGINE_SRCDIRS))

ADDON := CsvTranslator

# Localizzazione (Locale Kit) della vista Informazioni
# (MakeConfigurationView, vedi CsvTranslator.cpp): questo add-on non e'
# una BApplication, quindi non usa il catalogo dell'host (che
# comunque non conterrebbe le sue chiavi) ma un catalogo incorporato
# in QUESTO STESSO file .so, risolto a runtime tramite l'image_id
# ricevuto da make_nth_translator -- stesso principio di
# ui/Makefile ($(APP)), stesso avvertimento sul fingerprint
# nell'intestazione di locales/en.catkeys (vedi il commento li' in
# ui/Makefile: NON e' un segnaposto, va rigenerato con "make catkeys"
# dopo ogni modifica alle stringhe B_TRANSLATE).
CATALOG_SIG := application/x-vnd.Atomo-CsvTranslator
CATKEYS_DIR := locales
EN_CATKEYS := $(CATKEYS_DIR)/en.catkeys

all: $(ADDON)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(ADDON): CsvTranslator.cpp $(ENGINE_DIR)/libengine.a $(EN_CATKEYS)
	$(CXX) -shared -o $(ADDON) CsvTranslator.cpp $(INCLUDES) $(CXXFLAGS) \
		-L$(ENGINE_DIR) -lengine -lbe -ltranslation
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l en $(EN_CATKEYS)

install: $(ADDON)
	mkdir -p ~/config/non-packaged/add-ons/Translators
	cp $(ADDON) ~/config/non-packaged/add-ons/Translators/

# Passaggio manuale, non nel giro normale di "make" (stesso principio
# di "catkeys" in ui/Makefile): rigenera locales/it.catkeys, l'elenco
# delle chiavi italiane davvero presenti nel sorgente via B_TRANSLATE,
# solo per controllare di non aver dimenticato una stringa nuova in
# locales/en.catkeys e per copiarne il fingerprint aggiornato (4o
# campo dell'intestazione) -- mai letto a runtime ne' committato.
catkeys: CsvTranslator.cpp
	-cat CsvTranslator.cpp | $(CXX) -E -x c++ $(INCLUDES) $(CXXFLAGS) -DB_COLLECTING_CATKEYS - | /bin/grep -av '^#' > $(CATKEYS_DIR)/csv.pre
	collectcatkeys -s $(CATALOG_SIG) $(CATKEYS_DIR)/csv.pre -o $(CATKEYS_DIR)/it.catkeys
	rm -f $(CATKEYS_DIR)/csv.pre

test: $(ADDON)
	$(CXX) -c tests/test_csv_translator.cpp -o tests/test_csv_translator.o -I. \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_csv_translator tests/test_csv_translator.o \
		CsvTranslator.cpp $(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -ltranslation
	./tests/test_csv_translator

clean:
	rm -f $(ADDON) tests/test_csv_translator tests/test_csv_translator.o \
		$(CATKEYS_DIR)/it.catkeys $(CATKEYS_DIR)/csv.pre

.PHONY: all install test catkeys clean
