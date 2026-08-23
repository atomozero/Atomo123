# Motore di calcolo isolato — libreria statica senza dipendenze
# dall'Application/Interface Kit (nessuna BWindow/BView richiesta).
#
# Sorgente derivato ed estratto dal codice storico Sum-It/OpenSumIt
# (vedi ../legacy/opensumit/PORTING_STATUS.md per il porting a 64 bit
# e ../docs/ENGINE_API.md per la mappa dei file inclusi/esclusi).

CXX := g++
# -MMD -MP (Fase 29): senza tracciamento delle dipendenze sugli header,
# modificare un .h condiviso (es. Container.h) non forzava la
# ricompilazione di OGNI .cpp che lo include -- risultato: un binario
# linkato da object file costruiti contro DUE layout diversi della
# stessa struct (CContainer con/senza i nuovi campi fSpillOwnerOf/
# fSpillRangeOf), che scriveva fuori dai limiti del vero oggetto
# allocato invece di crashare in modo pulito. Bug reale, root-caused
# solo dopo un blocco silenzioso apparentemente casuale in ApplySpill
# (Container.spill.cpp) che spariva con una "make clean" completa --
# stessa identica classe di bug gia' corretta nel Makefile della UI
# (commit 8c895e5), mai applicata qui al motore fino ad ora.
CXXFLAGS := -std=gnu++17 -Wall -g -fPIC -MMD -MP
SRCDIRS := src/Cell src/Formula src/Functions src/Excel src/FileSys src/Collections src/Misc-Classes src/Utils src/Config src/bsl src/Metrowerks src/Stubs
INCLUDES := $(addprefix -I, $(SRCDIRS))
OBJDIR := obj

SOURCES := $(wildcard $(addsuffix /*.cpp, $(SRCDIRS)))
OBJECTS := $(patsubst %.cpp, $(OBJDIR)/%.o, $(notdir $(SOURCES)))
DEPS := $(patsubst %.o, %.d, $(OBJECTS))

vpath %.cpp $(SRCDIRS)

LIB := libengine.a

all: $(LIB)

$(LIB): $(OBJDIR) $(OBJECTS)
	ar -cru $(LIB) $(OBJECTS)

-include $(DEPS)

$(OBJDIR)/%.o: %.cpp
	@echo "-> $(<F)"
	@$(CXX) -c $< -o $@ $(INCLUDES) $(CXXFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

TESTBIN := tests/smoke_test

test: $(LIB)
	$(CXX) -c tests/smoke_test.cpp -o $(OBJDIR)/smoke_test.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o $(TESTBIN) $(OBJDIR)/smoke_test.o -L. -lengine -lbe
	./$(TESTBIN)

# Test delle funzioni con nome nelle formule (SUM, IF, MAX, ...): a
# differenza di "test" (smoke_test.cpp, solo formule aritmetiche),
# questo verifica InitFunctions() con una vera risorsa 'Func'
# compilata al volo dagli strumenti storici rez/bsl (ricompilati in
# Fase 1, mai usati finora -- vedi resources/ e docs/ENGINE_API.md).
LEGACY_DIR := ../legacy/opensumit
REZ := $(LEGACY_DIR)/rez/rez
BSL := $(LEGACY_DIR)/bsl/bsl
FUNC_TEST_RSRC := tests/named_functions.rsrc

$(REZ):
	$(MAKE) -C $(LEGACY_DIR)/rez

$(BSL):
	$(MAKE) -C $(LEGACY_DIR)/bsl

$(FUNC_TEST_RSRC): $(REZ) $(BSL) resources/funcs_by_nr.r resources/FuncNames.txt resources/FuncDescs.txt
	rm -f $(FUNC_TEST_RSRC)
	$(REZ) -o $(FUNC_TEST_RSRC) resources/funcs_by_nr.r
	$(BSL) -o $(FUNC_TEST_RSRC) -id 7 -name "FunctionNames" resources/FuncNames.txt
	$(BSL) -o $(FUNC_TEST_RSRC) -id 8 -name "FuncDescs" resources/FuncDescs.txt

test-functions: $(LIB) $(FUNC_TEST_RSRC)
	$(CXX) -c tests/named_functions_test.cpp -o $(OBJDIR)/named_functions_test.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/named_functions_test $(OBJDIR)/named_functions_test.o -L. -lengine -lbe
	./tests/named_functions_test

# Test dei riferimenti fra fogli (Fase 9, "NomeFoglio!Cella"):
# ISheetResolver, valXRef/valXRange, parsing/calcolo/UnMangle -- vedi
# il commento in cima a xsheet_test.cpp.
test-xsheet: $(LIB)
	$(CXX) -c tests/xsheet_test.cpp -o $(OBJDIR)/xsheet_test.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/xsheet_test $(OBJDIR)/xsheet_test.o -L. -lengine -lbe
	./tests/xsheet_test

# Test degli intervalli con nome (Fase 7): GetOrCreateNameTable,
# riconoscimento in una formula, calcolo, UnMangle -- vedi il commento
# in cima a named_ranges_test.cpp.
test-names: $(LIB)
	$(CXX) -c tests/named_ranges_test.cpp -o $(OBJDIR)/named_ranges_test.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/named_ranges_test $(OBJDIR)/named_ranges_test.o -L. -lengine -lbe
	./tests/named_ranges_test

# Test dei riferimenti a tabella strutturata di Excel (Fase 14,
# "Tabella12[Codice]"): CContainer::AddTable/ResolveName -- vedi il
# commento in cima a table_refs_test.cpp.
test-tables: $(LIB)
	$(CXX) -c tests/table_refs_test.cpp -o $(OBJDIR)/table_refs_test.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/table_refs_test $(OBJDIR)/table_refs_test.o -L. -lengine -lbe
	./tests/table_refs_test

clean:
	rm -rf $(OBJDIR) $(LIB) $(TESTBIN) $(FUNC_TEST_RSRC) tests/named_functions_test tests/xsheet_test tests/named_ranges_test tests/table_refs_test

.PHONY: all clean test test-functions test-xsheet test-names test-tables
