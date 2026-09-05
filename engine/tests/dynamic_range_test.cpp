/*
	dynamic_range_test.cpp

	Verifica il supporto a "OFFSET(...):OFFSET(...)" come intervallo
	dinamico (nuovi token valRefRange/opRangeOp in Formula.h), scoperto
	mancante analizzando un vero file XLSX reale (agile-kanban-board.xlsx,
	Vertex42): "=SUBTOTAL(9,OFFSET(H11,1,0):OFFSET(H15,-1,0))" (il
	conteggio delle card di ogni colonna del Kanban) falliva
	completamente il parsing.

	Tre bug distinti, non uno solo -- tutti coperti qui:
	1. La grammatica accettava ":" solo fra due CELL letterali, mai fra
	   il risultato di una chiamata di funzione.
	2. OFFSET() era gia' rotta anche DA SOLA, senza ":": il suo primo
	   argomento (una CELL letterale) veniva analizzato come valCell
	   (il VALORE della cella, non un riferimento), che GetRangeArgument
	   in OFFSETFunction non accetta (richiede fType==eRangeData).
	3. OFFSET(range,righe,colonne) aveva il secondo e terzo argomento
	   invertiti rispetto alla vera Excel (colonne poi righe, non righe
	   poi colonne) -- scoperto verificando un vero file XLSX reale
	   (agile-kanban-board.xlsx): l'ordine sbagliato calcolava
	   silenziosamente la cella sbagliata per QUALUNQUE file scritto da
	   Excel, non solo per l'intervallo dinamico. Corretto in
	   Functions.spreadsheet.cpp; i test qui sotto usano gia' l'ordine
	   giusto (righe poi colonne).
*/

#include <cstdio>
#include <cstring>

#include <OS.h>
#include <Path.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "Functions.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "ResourceManager.h"
#include "Utils.h"

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

// Parse locale-indipendente ('.'/',' come in un vero file XLSX, vedi
// CompileSharedFormulaAt in XlsxTranslator.cpp) invece dei separatori
// di default (che in questo ambiente sono ';'/','): le formule qui
// sotto usano la virgola come separatore di argomento, come nel vero
// file XLSX che ha scoperto il problema.
static void ParseXlsxStyle(const char *text, cell c, CContainer *doc)
{
	try
	{
		TryToParseString(text, c, doc, true, '.', ',');
	}
	catch (CErr &e)
	{
		printf("FAIL parse fallito per \"%s\": %s\n", text, (char *)e);
		gFailures++;
	}
}

int main()
{
	setbuf(stdout, NULL);

	BPath path("tests/named_functions.rsrc");
	gAppName = path;
	if (gResourceManager.SetTo(&path) != B_OK)
	{
		printf("FAIL impossibile aprire tests/named_functions.rsrc\n");
		return 1;
	}

	try { InitFunctions(); }
	catch (CErr &e)
	{
		printf("FAIL InitFunctions ha lanciato un'eccezione: %s\n", (char *)e);
		return 1;
	}

	CContainer &doc = *new CContainer(NULL, NULL);

	ParseXlsxStyle("10", cell(8, 11), &doc);  // H11
	ParseXlsxStyle("100", cell(8, 12), &doc); // H12
	ParseXlsxStyle("200", cell(8, 13), &doc); // H13
	ParseXlsxStyle("300", cell(8, 14), &doc); // H14
	ParseXlsxStyle("10", cell(8, 15), &doc);  // H15

	Value v;

	// Bug 2: OFFSET() da sola, wrappata in SUM cosi' il suo risultato
	// (un riferimento, non un numero) non deve nemmeno passare per lo
	// storage di una cella -- isola il problema dell'argomento di
	// OFFSET da quello del collasso a livello di cella verificato sotto.
	ParseXlsxStyle("=SUM(OFFSET(H11,1,0))", cell(10, 1), &doc); // J1
	doc.CalcCell(cell(10, 1));
	doc.GetValue(cell(10, 1), v);
	Check((double)v == 100.0,
		"SUM(OFFSET(H11,1,0)) = 100 (H12): OFFSET risolve il suo argomento base come riferimento, non come valore");

	// La stessa OFFSET() scritta DA SOLA in una cella (nessuna funzione
	// che ne consumi il risultato come argomento): il risultato finale
	// della formula e' un eRangeData non degenere per il motore
	// (CFormula::Calculate), che CellData/Value::operator=(const
	// CellData&) non sanno rappresentare come valore permanente di una
	// cella -- senza il collasso all'intersezione implicita in
	// CContainer::CalcCell (Container.graph.cpp) la cella
	// resterebbe silenziosamente vuota (eNoData), un dato calcolato
	// perso senza nessun avviso.
	ParseXlsxStyle("=OFFSET(H11,1,0)", cell(11, 1), &doc); // K1
	doc.CalcCell(cell(11, 1));
	doc.GetValue(cell(11, 1), v);
	Check((double)v == 100.0,
		"OFFSET(H11,1,0) scritta da sola (non wrappata) collassa al valore della cella in alto a sinistra, 100");

	// Le quattro combinazioni reali di ":" con un operando dinamico.
	ParseXlsxStyle("=SUM(OFFSET(H11,1,0):OFFSET(H15,-1,0))", cell(10, 2), &doc); // J2
	doc.CalcCell(cell(10, 2));
	doc.GetValue(cell(10, 2), v);
	Check((double)v == 600.0, "SUM(OFFSET(...):OFFSET(...)) = 600 (H12+H13+H14): funzione:funzione");

	ParseXlsxStyle("=SUM(H12:OFFSET(H15,-1,0))", cell(10, 3), &doc); // J3
	doc.CalcCell(cell(10, 3));
	doc.GetValue(cell(10, 3), v);
	Check((double)v == 600.0, "SUM(H12:OFFSET(...)) = 600: letterale:funzione");

	ParseXlsxStyle("=SUM(OFFSET(H11,1,0):H14)", cell(10, 4), &doc); // J4
	doc.CalcCell(cell(10, 4));
	doc.GetValue(cell(10, 4), v);
	Check((double)v == 600.0, "SUM(OFFSET(...):H14) = 600: funzione:letterale");

	// La formula esatta del file reale che ha scoperto il problema
	// (SUBTOTAL invece di SUM, come nel vero conteggio delle card del
	// Kanban).
	ParseXlsxStyle("=SUBTOTAL(9,OFFSET(H11,1,0):OFFSET(H15,-1,0))", cell(10, 5), &doc); // J5
	doc.CalcCell(cell(10, 5));
	doc.GetValue(cell(10, 5), v);
	Check((double)v == 600.0,
		"SUBTOTAL(9,OFFSET(...):OFFSET(...)) = 600: la formula esatta di agile-kanban-board.xlsx");

	// Regressione: un intervallo letterale semplice non deve essere
	// toccato da questa modifica (stesso identico bytecode di sempre,
	// valRange -- vedi il commento nel ramo "case CELL" di
	// CParser::Factor).
	ParseXlsxStyle("=SUM(H12:H14)", cell(10, 6), &doc); // J6
	doc.CalcCell(cell(10, 6));
	doc.GetValue(cell(10, 6), v);
	Check((double)v == 600.0, "SUM(H12:H14) = 600: un intervallo letterale semplice resta invariato");

	// Nota: il round-trip nella barra della formula (UnMangle) e
	// l'inserimento riga con verifica testuale sono coperti da
	// ui/tests/test_formula_auditing.cpp, non qui -- CFormatter::
	// FormatValue/ftoa (chiamato da UnMangle per ogni valNum) dipende da
	// gFontSizeTable/BFont::StringWidth, che in QUESTO binario headless
	// (solo -lbe, senza -llocalestub/-ltranslation come nell'app vera o
	// nei test in ui/) si blocca in un loop infinito -- riprodotto anche
	// su una formula banale "=SUM(A1,0)" senza nessun OFFSET/intervallo
	// dinamico, quindi un limite preesistente e separato di questo
	// specifico ambiente di test, non un bug introdotto qui (vedi la
	// memoria di progetto sui test headless che vanno in crash/loop per
	// setup mancante, stessa causa).

	// Un operando che non e' affatto un riferimento (es. un file con un
	// errore utente reale) non deve crashare ne' leggere memoria a
	// caso -- degrada a un errore di parsing pulito (il motore
	// storicamente rifiuta ":" applicato a due numeri letterali anche
	// per un altro motivo, la sintassi "riga intera" non e' modellata
	// qui, ma il punto e' che non crasha).
	bool threw = false;
	try { TryToParseString("=SUM(1:2)", cell(10, 7), &doc, true, '.', ','); }
	catch (CErr &) { threw = true; }
	Check(threw, "SUM(1:2) (un operando non-riferimento) fallisce il parsing in modo pulito, non crasha");

	// GetPrecedents (Formula auditing views, Fase 35) deve vedere
	// attraverso l'intervallo dinamico: H11 e H15 sono gli argomenti
	// base di OFFSET, non "nascosti" dentro la sua chiamata di funzione
	// (il bytecode e' postfisso/piatto, vedi Formula.iter.cpp).
	std::vector<cell> precedents;
	doc.GetPrecedents(cell(10, 5), precedents);
	bool hasH11 = false, hasH15 = false;
	for (size_t i = 0; i < precedents.size(); i++)
	{
		if (precedents[i] == cell(8, 11)) hasH11 = true;
		if (precedents[i] == cell(8, 15)) hasH15 = true;
	}
	Check(hasH11 && hasH15, "GetPrecedents(J5) trova H11 e H15, gli argomenti base di OFFSET dentro l'intervallo dinamico");

	// Nota: l'inserimento riga (CFormula::UpdateReferences che sposta i
	// riferimenti dentro un intervallo dinamico) e' verificato in
	// ui/tests/test_formula_auditing.cpp tramite la vera SheetView::
	// InsertRows, non qui -- stesso motivo del round-trip sopra
	// (richiede GetCellFormula/UnMangle per leggere il risultato).

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
