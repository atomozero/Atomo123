/*
	named_ranges_test.cpp

	Test headless degli intervalli con nome (Fase 7): definizione
	tramite CContainer::GetOrCreateNameTable() (usata dalla UI, vedi
	ui/src/NameWindow.cpp), riconoscimento in una formula ("=Totale*2"),
	calcolo e round-trip testuale (UnMangle). Prima di questo lavoro il
	riconoscimento in fase di parsing passava da
	CCellView::IsNamedRange() (EngineViewStub.h), sempre NULL/false
	nella UI reale -- un nome definito non veniva quindi mai
	riconosciuto in una formula, solo tramite ResolveName() se per
	assurdo il bytecode l'avesse gia' contenuto (irraggiungibile senza
	passare dal parser).

	Un nome per un SOLO cella (non un intervallo multi-cella) basta a
	verificare tutto il percorso end-to-end senza bisogno delle
	funzioni con nome (SUM, ecc. -- richiedono InitFunctions() e la
	risorsa 'Func' compilata, vedi named_functions_test.cpp): un
	intervallo a una cella si riduce a un valore scalare da solo (vedi
	CFormula::Calculate, case valName), utilizzabile direttamente in
	un'espressione aritmetica. La risoluzione di un VERO intervallo
	multi-cella (dove quella riduzione non avviene) si verifica invece
	direttamente con CContainer::ResolveName(), senza passare da una
	formula.
*/

#include <cstdio>
#include <cstring>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "NameTable.h"

static int gFailures = 0;

static void Check(bool condition, const char *what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

int main()
{
	CContainer &doc = *new CContainer(NULL, NULL);

	cell a1(1, 1), a2(1, 2);
	TryToParseString("10", a1, &doc, true);
	TryToParseString("20", a2, &doc, true);

	// Definizione di un nome per la cella A1 -- stesso passo che
	// NameWindow compie quando l'utente preme "Aggiungi".
	CNameTable *names = doc.GetOrCreateNameTable();
	Check(names != NULL, "GetOrCreateNameTable() restituisce una tabella non nulla");
	(*names)["Totale"] = range(1, 1, 1, 1); // A1:A1, cioè A1

	// Un nome NON ancora definito e' ora un valName vivo (mai piu'
	// rifiutato in fase di parsing, stesso principio di
	// ParseSheetReference in Fase 9): ResolveName() lancia, Calculate()
	// la cattura e restituisce gNameNan (eNumData, NaN), non piu' un
	// identificatore testuale morto.
	cell b0(3, 1);
	TryToParseString("=NonDefinito*2", b0, &doc, true);
	doc.CalcCell(b0);
	Value undefinedVal;
	doc.GetValue(b0, undefinedVal);
	Check(undefinedVal.fType == eNumData && undefinedVal.IsNan(),
		"un nome non ancora definito calcola gNameNan (NaN), formula viva");

	// Una formula che usa il nome appena definito lo riconosce e lo
	// calcola.
	cell b1(2, 1);
	TryToParseString("=Totale*2", b1, &doc, true);
	doc.CalcCell(b1);
	Value v;
	doc.GetValue(b1, v);
	Check((double)v == 20.0, "=Totale*2 con Totale=A1 (10) calcola 20");

	// UnMangle: la formula si ricostruisce con "Totale" nel testo, non
	// con la cella grezza A1. Cella a parte con SOLO il nome, senza
	// "*2": UnMangle di un valNum (il letterale 2) passa da ftoa(),
	// che legge la larghezza di una stringa dal Font Kit
	// (gFontSizeTable[0].Font().StringWidth) -- richiede una
	// connessione app_server che questo test headless non ha (stessa
	// categoria di limite gia' incontrata in
	// engine/tests/xsheet_test.cpp). Il solo nome, senza numeri, non
	// tocca affatto quel percorso.
	cell b2(4, 1);
	TryToParseString("=Totale", b2, &doc, true);
	char formula[256];
	doc.GetCellFormula(b2, formula, sizeof(formula), false);
	Check(strstr(formula, "Totale") != NULL,
		"la formula ricostruita mostra \"Totale\", non A1");

	// Ridefinire il nome (stesso ruolo del pulsante "Aggiorna" in
	// NameWindow) sposta la cella referenziata: un nuovo CalcCell
	// (nessun grafo di dipendenze, come per i riferimenti fra fogli)
	// riflette il cambiamento.
	(*names)["Totale"] = range(1, 2, 1, 2); // A2
	doc.CalcCell(b1);
	doc.GetValue(b1, v);
	Check((double)v == 40.0, "dopo aver ridefinito Totale su A2 (20), un nuovo CalcCell dà 40");

	// Un vero intervallo multi-cella si risolve correttamente tramite
	// ResolveName() (usato da NameWindow per elencare/verificare le
	// definizioni esistenti), senza bisogno di SUM().
	(*names)["Intervallo"] = range(1, 1, 1, 2); // A1:A2
	range resolved = doc.ResolveName("Intervallo");
	Check(resolved.TopLeft() == cell(1, 1) && resolved.BotRight() == cell(1, 2),
		"ResolveName(\"Intervallo\") restituisce A1:A2 intatto");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
