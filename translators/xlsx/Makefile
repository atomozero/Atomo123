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

all: $(ADDON)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(ADDON): $(SOURCES) $(ENGINE_DIR)/libengine.a
	$(CXX) -shared -o $(ADDON) $(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)

install: $(ADDON)
	mkdir -p ~/config/non-packaged/add-ons/Translators
	cp $(ADDON) ~/config/non-packaged/add-ons/Translators/

test: $(ADDON)
	$(CXX) -c tests/test_xlsx_translator.cpp -o tests/test_xlsx_translator.o \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_xlsx_translator tests/test_xlsx_translator.o \
		$(SOURCES) $(INCLUDES) $(CXXFLAGS) $(LIBS)
	./tests/test_xlsx_translator

clean:
	rm -f $(ADDON) tests/test_xlsx_translator tests/test_xlsx_translator.o

.PHONY: all install test clean
