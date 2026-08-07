/*
	test_unary_plus.cpp

	Verifica il "+" unario davanti a un fattore di formula ("+A1",
	"+'Foglio'!A1", "+D16*E16"...) -- bug reale segnalato dall'utente:
	un file XLSX reale aperto in Atomo123 mostrava moltissime celle con
	il testo letterale della formula invece del valore calcolato.
	Causa: CParser::Factor() (engine/src/Formula/parser.cpp) gestiva il
	"-" unario ma non il "+" unario, quindi qualunque formula con
	questo prefisso falliva il parsing e TryToParseString la degradava
	silenziosamente a testo puro. Nel file reale che ha fatto scoprire
	il bug, il 59% di tutte le formule (1046 su 1764) iniziava con "+".

	Non passa da una vera MainWindow: e' un comportamento del parser
	dell'engine, verificato con TryToParseString/CContainer direttamente
	come test_xsheet_formulas.cpp.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>

#include "Cell.h"
#include "Value.h"
#include "Range.h"
#include "Container.h"
#include "CellParser.h"

static int gFailures = 0;

static void Check(bool condition, const char* what)
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
	BApplication app("application/x-vnd.Atomo-TestUnaryPlus");

	CContainer* doc = new CContainer(NULL, NULL);

	// "+5": il caso piu' semplice, un numero letterale con un "+"
	// davanti -- prima di questo fix falliva il parsing anche qui. Un
	// numero letterale (nessun riferimento a cella) e' un'espressione
	// COSTANTE: CParser::Formula().ReduceToValue() la riduce subito al
	// solo valore (5.0), senza tenere in giro la formula "+5" com'e'
	// (stesso comportamento gia' scelto per qualunque altra formula
	// costante, es. "=2+3" -- non specifico di questo fix). Se invece
	// il parsing fosse fallito (il bug prima di questo fix), il tipo
	// sarebbe stato testo con "+5" letterale, non un numero.
	TryToParseString("+5", cell(1, 1), doc, true); // A1
	Value v;
	doc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 5.0,
		"\"+5\" si calcola a 5, non resta testo letterale");

	// "+A1+B1": lo stesso prefisso, ma davanti a una vera espressione
	// con riferimenti a celle, come nel file reale che ha fatto
	// scoprire il bug (es. "+D16*E16").
	TryToParseString("10", cell(1, 2), doc, true); // A2
	TryToParseString("20", cell(2, 2), doc, true); // B2
	TryToParseString("+A2+B2", cell(3, 2), doc, true); // C2
	// Una formula con riferimenti a celle (non costante) non si
	// calcola gia' dentro TryToParseString -- solo le formule
	// costanti (come "+5" sopra) lo fanno. CalcCell() e' il modo
	// pubblico per farlo esplicitamente in un test headless (stesso
	// principio di test_ascd_io.cpp), la UI vera lo fa gia' da sola
	// tramite RecalculateOwningWorkbook() dopo ogni modifica.
	doc->CalcCell(cell(3, 2));
	doc->GetValue(cell(3, 2), v);
	Check(v.fType == eNumData && (double)v == 30.0,
		"\"+A2+B2\" (10+20) si calcola a 30");

	// "+A3+B3+C3": il "+" unario davanti a un'espressione con piu' di
	// due termini, come "+D16*E16" o "+D27*E27" nel file reale che ha
	// fatto scoprire il bug. Non un "+SUM(...)" apposta: le funzioni
	// con nome (SUM compresa) richiedono che InitFunctions() abbia
	// gia' caricato la tabella da una risorsa 'Func' legata al
	// binario (vedi App::ReadyToRun) -- indisponibile in un
	// eseguibile di test come questo, che non passa da xres come
	// $(APP) nel Makefile. Fuori scopo per QUESTO test (il "+"
	// unario, non le funzioni), vedi invece test_xsheet_formulas.cpp
	// per la stessa limitazione documentata sulle funzioni nei test.
	TryToParseString("1", cell(1, 3), doc, true); // A3
	TryToParseString("2", cell(2, 3), doc, true); // B3
	TryToParseString("3", cell(3, 3), doc, true); // C3
	TryToParseString("+A3+B3+C3", cell(4, 3), doc, true); // D3
	doc->CalcCell(cell(4, 3)); // stesso motivo di C2 sopra
	doc->GetValue(cell(4, 3), v);
	Check(v.fType == eNumData && (double)v == 6.0,
		"\"+A3+B3+C3\" (1+2+3) si calcola a 6");

	// "+'Foglio con spazi'!A1": il caso che ha fatto scoprire il bug
	// nel file reale -- un riferimento incrociato fra fogli, col nome
	// del foglio fra apici perche' contiene spazi/trattini, preceduto
	// da "+". La risoluzione VERA del valore incrociato (con un
	// ISheetResolver collegato) e' gia' verificata a fondo da
	// test_xsheet_formulas.cpp; qui interessa solo che il "+" davanti
	// non faccia fallire il parsing (senza resolver collegato il
	// riferimento resta semplicemente non risolto, non e' quello sotto
	// test) -- prima di questo fix, l'intera stringa sarebbe degradata
	// a testo puro non appena il parser incontrava il "+" iniziale.
	TryToParseString("+'Foglio 2 - dati'!I9", cell(5, 3), doc, true); // E3
	char xsheetFormula[128];
	doc->GetCellFormula(cell(5, 3), xsheetFormula, sizeof(xsheetFormula), false);
	Check(strstr(xsheetFormula, "'Foglio 2 - dati'!I9") != NULL,
		"\"+'Foglio 2 - dati'!I9\" (riferimento incrociato con nome foglio fra apici, preceduto da \"+\") "
		"e' davvero riconosciuta come formula, non degradata a testo");

	// Controprova: un testo che comincia per "+" ma non e' affatto
	// un'espressione valida (es. un numero di telefono digitato
	// "+39 345 678") deve restare testo puro, non un errore rumoroso.
	// inWarnIfError=false qui (non true come sopra): CParser::Match
	// segna mIsFormula=true al primo token qualunque riconosciuto con
	// successo (anche solo un NUMBER, vedi parser.cpp), non solo per
	// un vero prefisso "=" -- un ingresso con PIU' di un "pezzo"
	// valido ("39" seguito da "345" senza un operatore in mezzo) fa
	// fallire il resto del parsing con mIsFormula gia' vero, quindi
	// CParser::Parse rilancia invece di degradare da solo a testo (lo
	// fa comunque TryToParseString stessa, ma solo se non le si chiede
	// esplicitamente di rilanciare) -- lo stesso path gia' percorso
	// dalla UI vera (SheetView::CommitEditing) con un try/catch
	// esplicito attorno alla chiamata, per la stessa ragione.
	TryToParseString("+39 345 678", cell(6, 3), doc, false); // F3
	doc->GetValue(cell(6, 3), v);
	Check(v.fType == eTextData,
		"\"+39 345 678\" (non un'espressione valida) resta testo puro, non diventa una formula per sbaglio");

	doc->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
