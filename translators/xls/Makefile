# Translator XLS legacy — add-on Translation Kit di Haiku.
#
# Compila XlsTranslator.cpp in un add-on installabile (shared object)
# che usa il motore di calcolo isolato (../../engine/libengine.a).

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
INCLUDES := $(addprefix -I, $(ENGINE_SRCDIRS))

ADDON := XlsTranslator

# Localizzazione (Locale Kit) della vista Informazioni: vedi il
# commento analogo in translators/csv/Makefile.
CATALOG_SIG := application/x-vnd.Atomo-XlsTranslator
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

$(ADDON): XlsTranslator.cpp $(ENGINE_DIR)/libengine.a $(EN_CATKEYS) $(KO_CATKEYS) $(JA_CATKEYS) $(ES_CATKEYS) $(FR_CATKEYS) $(DE_CATKEYS) $(PT_CATKEYS) $(ZH_CATKEYS) $(RU_CATKEYS) $(AR_CATKEYS)
	$(CXX) -shared -o $(ADDON) XlsTranslator.cpp $(INCLUDES) $(CXXFLAGS) \
		-L$(ENGINE_DIR) -lengine -lbe -ltranslation
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
