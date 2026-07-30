# Motore di calcolo isolato — libreria statica senza dipendenze
# dall'Application/Interface Kit (nessuna BWindow/BView richiesta).
#
# Sorgente derivato ed estratto dal codice storico Sum-It/OpenSumIt
# (vedi ../legacy/opensumit/PORTING_STATUS.md per il porting a 64 bit
# e ../docs/ENGINE_API.md per la mappa dei file inclusi/esclusi).

CXX := g++
CXXFLAGS := -std=gnu++17 -Wall -g -fPIC
SRCDIRS := src/Cell src/Formula src/Functions src/Excel src/FileSys src/Collections src/Misc-Classes src/Utils src/Config src/bsl src/Metrowerks src/Stubs
INCLUDES := $(addprefix -I, $(SRCDIRS))
OBJDIR := obj

SOURCES := $(wildcard $(addsuffix /*.cpp, $(SRCDIRS)))
OBJECTS := $(patsubst %.cpp, $(OBJDIR)/%.o, $(notdir $(SOURCES)))

vpath %.cpp $(SRCDIRS)

LIB := libengine.a

all: $(LIB)

$(LIB): $(OBJDIR) $(OBJECTS)
	ar -cru $(LIB) $(OBJECTS)

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

clean:
	rm -rf $(OBJDIR) $(LIB) $(TESTBIN) $(FUNC_TEST_RSRC) tests/named_functions_test

.PHONY: all clean test test-functions
