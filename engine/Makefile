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

clean:
	rm -rf $(OBJDIR) $(LIB) $(TESTBIN)

.PHONY: all clean test
