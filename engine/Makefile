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
# -O2 (Fase 34, richiesta esplicita dell'utente dopo aver misurato
# l'apertura di un file XLSX reale a 13 fogli): l'intero progetto non
# aveva mai avuto un flag di ottimizzazione, solo -g -- confermato con
# un confronto diretto (stesso identico binario/libreria, solo -O2 in
# piu'): Translate() 53,9s -> 17,1s, LoadASCDBook 30,0s -> 9,2s,
# RecalculateWorkbook 2,0s -> 0,26s. -g resta insieme a -O2 (entrambi
# supportati da gcc): i crash report restano leggibili, vedi il
# commento su feedback_debugger_save_report nella memoria di progetto.
CXXFLAGS := -std=gnu++17 -Wall -g -O2 -fPIC -MMD -MP
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

# Test di CContainer::NewCell dopo la riscrittura per una sola discesa
# nell'albero invece di due (Fase 34): vedi il commento in cima a
# newcell_test.cpp.
test-newcell: $(LIB)
	$(CXX) -c tests/newcell_test.cpp -o $(OBJDIR)/newcell_test.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/newcell_test $(OBJDIR)/newcell_test.o -L. -lengine -lbe
	./tests/newcell_test

# Test dei riferimenti a tabella strutturata di Excel (Fase 14,
# "Tabella12[Codice]"): CContainer::AddTable/ResolveName -- vedi il
# commento in cima a table_refs_test.cpp.
test-tables: $(LIB)
	$(CXX) -c tests/table_refs_test.cpp -o $(OBJDIR)/table_refs_test.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/table_refs_test $(OBJDIR)/table_refs_test.o -L. -lengine -lbe
	./tests/table_refs_test

# Test degli intervalli dinamici (Fase 36, "OFFSET(...):OFFSET(...)"):
# valRefRange/opRangeOp, il primo argomento di OFFSET come vero
# riferimento, il collasso all'intersezione implicita in CalcCell,
# GetPrecedents attraverso l'intervallo, l'aggiornamento dei riferimenti
# su inserimento riga -- vedi il commento in cima a
# dynamic_range_test.cpp. Riusa la stessa risorsa 'Func' di
# test-functions (nessuna funzione nuova registrata, solo un nuovo
# comportamento del parser/motore su OFFSET gia' esistente).
test-dynamic-range: $(LIB) $(FUNC_TEST_RSRC)
	$(CXX) -c tests/dynamic_range_test.cpp -o $(OBJDIR)/dynamic_range_test.o $(INCLUDES) $(CXXFLAGS)
	$(CXX) -o tests/dynamic_range_test $(OBJDIR)/dynamic_range_test.o -L. -lengine -lbe
	./tests/dynamic_range_test

clean:
	rm -rf $(OBJDIR) $(LIB) $(TESTBIN) $(FUNC_TEST_RSRC) tests/named_functions_test tests/xsheet_test tests/named_ranges_test tests/table_refs_test tests/dynamic_range_test

.PHONY: all clean test test-functions test-xsheet test-names test-tables
