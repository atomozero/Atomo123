# Translator ODS — add-on Translation Kit di Haiku.
#
# Compila OdsTranslator.cpp + MiniZip.cpp in un add-on installabile
# (shared object) che usa il motore di calcolo isolato
# (../../engine/libengine.a), lo stesso lettore ZIP minimale del
# translator XLSX (MiniZip.*, sopra zlib) ed expat per il parsing XML
# (entrambi gia' presenti su Haiku, nessuna nuova dipendenza di
# sistema da installare).

# CXX/CXXFLAGS sovrascrivibili ("?="): vedi il commento in
# ../../engine/Makefile sul compilatore dell'architettura secondaria a
# 32 bit.
CXX ?= g++
# -O2 (Fase 34): vedi il commento in ../../engine/Makefile.
CXXFLAGS ?= -std=gnu++17 -Wall -g -O2 -fPIC
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
KO_CATKEYS := $(CATKEYS_DIR)/ko.catkeys
JA_CATKEYS := $(CATKEYS_DIR)/ja.catkeys
ES_CATKEYS := $(CATKEYS_DIR)/es.catkeys
FR_CATKEYS := $(CATKEYS_DIR)/fr.catkeys
DE_CATKEYS := $(CATKEYS_DIR)/de.catkeys
PT_CATKEYS := $(CATKEYS_DIR)/pt.catkeys
ZH_CATKEYS := $(CATKEYS_DIR)/zh.catkeys
RU_CATKEYS := $(CATKEYS_DIR)/ru.catkeys
AR_CATKEYS := $(CATKEYS_DIR)/ar.catkeys

all: $(ADDON)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(REZ):
	$(MAKE) -C $(LEGACY_DIR)/rez

$(FUNC_TEST_RSRC): $(REZ) $(ENGINE_DIR)/resources/funcs_by_nr.r
	rm -f $(FUNC_TEST_RSRC)
	$(REZ) -o $(FUNC_TEST_RSRC) $(ENGINE_DIR)/resources/funcs_by_nr.r

$(ADDON): $(SOURCES) $(ENGINE_DIR)/libengine.a $(EN_CATKEYS) $(KO_CATKEYS) $(JA_CATKEYS) $(ES_CATKEYS) $(FR_CATKEYS) $(DE_CATKEYS) $(PT_CATKEYS) $(ZH_CATKEYS) $(RU_CATKEYS) $(AR_CATKEYS)
	$(CXX) -shared -o $(ADDON) $(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l en $(EN_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l ko $(KO_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l ja $(JA_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l es $(ES_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l fr $(FR_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l de $(DE_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l pt $(PT_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l zh $(ZH_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l ru $(RU_CATKEYS)
	linkcatkeys -o $(ADDON) -s $(CATALOG_SIG) -tr -l ar $(AR_CATKEYS)

# Dove installare l'add-on: Haiku mette gli add-on di un'architettura
# SECONDARIA in una sottocartella che porta il nome di quell'architettura
# (~/config/non-packaged/add-ons/x86/Translators), mentre quelli
# dell'architettura primaria stanno nella cartella stessa
# (~/config/non-packaged/add-ons/Translators). Un'applicazione compilata
# per l'architettura secondaria cerca SOLO nella prima: installare la',
# su un sistema ibrido x86_gcc2/x86, e' la differenza fra "Formato file
# non riconosciuto da nessun translator installato" e un file che si apre.
#
# Come si riconosce quale delle due: lo si chiede al compilatore in uso,
# che e' cio' che determina l'architettura dell'add-on prodotto.
# "-print-file-name=libbe.so" risponde con .../develop/lib/x86/libbe.so
# per un toolchain secondario e .../develop/lib/libbe.so per il primario.
# "-dumpmachine" NON serve: su un ibrido x86_gcc2 entrambi i compilatori
# rispondono "i586-pc-haiku". L'ultimo componente di quel percorso e'
# quindi "lib" sull'architettura primaria (x86_64 compreso: nessuna
# sottocartella, percorso identico a prima) e il nome dell'architettura
# su una secondaria.
LIBBE_DIR := $(patsubst %/,%,$(dir $(shell $(CXX) -print-file-name=libbe.so)))
ARCH_NAME := $(notdir $(LIBBE_DIR))
ifeq ($(filter-out lib .,$(ARCH_NAME)),)
ADDONS_DIR := $(HOME)/config/non-packaged/add-ons
else
ADDONS_DIR := $(HOME)/config/non-packaged/add-ons/$(ARCH_NAME)
endif
TRANSLATORS_DIR := $(ADDONS_DIR)/Translators

# "cp" seguito da "mv" nella stessa cartella, non "cp" diretto sul
# file gia' installato: un vero crash reale di Tracker (il suo
# thumbnail worker chiama BTranslatorRoster::Translate() su ogni file
# del Desktop in background, in qualunque momento) ha eseguito codice
# a meta' sovrascritto durante un "cp" diretto -- "mv" sullo stesso
# filesystem e' una rinomina atomica (rename(2)), un lettore concorrente
# vede sempre o il file vecchio intero o quello nuovo intero, mai uno
# stato intermedio.
install: $(ADDON)
	mkdir -p $(TRANSLATORS_DIR)
	cp $(ADDON) $(TRANSLATORS_DIR)/$(ADDON).new
	mv $(TRANSLATORS_DIR)/$(ADDON).new $(TRANSLATORS_DIR)/$(ADDON)

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
