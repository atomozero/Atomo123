# Translator XLS legacy — add-on Translation Kit di Haiku.
#
# Compila XlsTranslator.cpp in un add-on installabile (shared object)
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

ADDON := XlsTranslator

all: $(ADDON)

$(ENGINE_DIR)/libengine.a:
	$(MAKE) -C $(ENGINE_DIR)

$(ADDON): XlsTranslator.cpp $(ENGINE_DIR)/libengine.a
	$(CXX) -shared -o $(ADDON) XlsTranslator.cpp $(INCLUDES) $(CXXFLAGS) \
		-L$(ENGINE_DIR) -lengine -lbe -ltranslation

install: $(ADDON)
	mkdir -p ~/config/non-packaged/add-ons/Translators
	cp $(ADDON) ~/config/non-packaged/add-ons/Translators/

test: $(ADDON)
	$(CXX) -c tests/test_xls_translator.cpp -o tests/test_xls_translator.o -I. \
		$(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/test_xls_translator tests/test_xls_translator.o \
		XlsTranslator.cpp $(INCLUDES) $(CXXFLAGS) -L$(ENGINE_DIR) -lengine -lbe -ltranslation
	./tests/test_xls_translator

clean:
	rm -f $(ADDON) tests/test_xls_translator tests/test_xls_translator.o

.PHONY: all install test clean
