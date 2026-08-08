/*
	table_refs_test.cpp

	Test headless dei riferimenti a tabella strutturata di Excel (Fase
	14, "Tabella12[Codice]"): bug reale segnalato dall'utente, un file
	XLSX reale con XLOOKUP su colonne di una Tabella (ListObject) Excel
	mostrava il testo letterale della formula invece del valore
	calcolato -- il motore non aveva alcun concetto di "tabella", solo
	di intervalli con nome (CNameTable, gia' verificato da
	named_ranges_test.cpp).

	Stesso schema di named_ranges_test.cpp: CContainer::AddTable() gioca
	il ruolo che l'importazione XLSX (Excel.cpp) svolgera' da sola in
	un file reale, verificato qui direttamente senza passare da un vero
	file .xlsm.
*/

#include <cstdio>
#include <cstring>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"

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

	// Tabella12: A10:D121 nel file Excel originale (intestazione in
	// riga 10, dati da riga 11), colonne Codice/Descrizione/u.m./p.u.
	// -- qui ridotta a poche righe, stesso principio.
	TryToParseString("Codice", cell(1, 1), &doc, true);      // A1 (intestazione, mai parte di dataRange)
	TryToParseString("Descrizione", cell(2, 1), &doc, true); // B1
	TryToParseString("u.m.", cell(3, 1), &doc, true);        // C1 (nome colonna CON punto finale)
	TryToParseString("ABC", cell(1, 2), &doc, true);         // A2
	TryToParseString("Primo articolo", cell(2, 2), &doc, true); // B2
	TryToParseString("pz", cell(3, 2), &doc, true);          // C2
	TryToParseString("XYZ", cell(1, 3), &doc, true);         // A3
	TryToParseString("Secondo articolo", cell(2, 3), &doc, true); // B3
	TryToParseString("kg", cell(3, 3), &doc, true);          // C3

	CTableDef table;
	table.dataRange = range(1, 2, 3, 3); // A2:C3, intestazione ESCLUSA
	table.columnNames.push_back("Codice");
	table.columnNames.push_back("Descrizione");
	table.columnNames.push_back("u.m.");
	doc.AddTable("Tabella12", table);

	// ResolveName riconosce "Tabella12[Colonna]" e restituisce solo la
	// colonna richiesta (mai l'intera tabella, mai l'intestazione).
	range codiceCol = doc.ResolveName("Tabella12[Codice]");
	Check(codiceCol.TopLeft() == cell(1, 2) && codiceCol.BotRight() == cell(1, 3),
		"Tabella12[Codice] risolve alla sola colonna A (A2:A3), intestazione esclusa");

	range descCol = doc.ResolveName("Tabella12[Descrizione]");
	Check(descCol.TopLeft() == cell(2, 2) && descCol.BotRight() == cell(2, 3),
		"Tabella12[Descrizione] risolve alla colonna B (B2:B3)");

	// Nome di colonna CON un punto finale ("u.m."): il lessico non deve
	// ne' mangiarlo ne' fermarsi prima -- vedi lexer.cpp, il nome di
	// colonna e' un token TBLCOL a parte (stati 60-61), non passa mai
	// dalla logica di identificatore punteggiato (stato 42, per
	// "_xlfn.XLOOKUP"/"CEILING.MATH") che invece tratterebbe un punto
	// finale come ambiguo.
	range umCol = doc.ResolveName("Tabella12[u.m.]");
	Check(umCol.TopLeft() == cell(3, 2) && umCol.BotRight() == cell(3, 3),
		"Tabella12[u.m.] (nome colonna con punto finale) risolve alla colonna C");

	// Una formula VERA che usa il riferimento a tabella: "+" unario
	// davanti (comunissimo nei file XLSX reali, vedi ui/tests/
	// test_unary_plus.cpp) su un riferimento a colonna a cella singola
	// si riduce a un valore scalare, stesso principio di un nome
	// definito con TryToParseString in named_ranges_test.cpp. Serve una
	// tabella con UNA SOLA riga dati apposta: su piu' righe il
	// riferimento resta un intervallo vero (eRangeData, consumato da
	// una funzione come XLOOKUP/VLOOKUP, mai da una cella nuda -- stessa
	// cosa succederebbe scrivendo "=A2:A3" da solo in una cella, nulla
	// di specifico ai riferimenti a tabella).
	TryToParseString("Sole", cell(6, 1), &doc, true);   // F1 (intestazione tabella a una riga)
	TryToParseString("Riga singola", cell(6, 2), &doc, true); // F2 (unico dato)
	CTableDef oneRowTable;
	oneRowTable.dataRange = range(6, 2, 6, 2); // F2:F2
	oneRowTable.columnNames.push_back("Sole");
	doc.AddTable("TabellaUnaRiga", oneRowTable);

	TryToParseString("+TabellaUnaRiga[Sole]", cell(5, 2), &doc, true); // E2
	doc.CalcCell(cell(5, 2));
	Value v;
	doc.GetValue(cell(5, 2), v);
	Check(v.fType == eTextData && strcmp((const char *)v, "Riga singola") == 0,
		"\"+TabellaUnaRiga[Sole]\" su una tabella a una sola riga calcola il valore della cella, non resta testo letterale");

	// Tabella/colonna inesistenti: stesso errKeyNotFound di un nome di
	// intervallo non trovato (vedi named_ranges_test.cpp), non un
	// crash ne' un nuovo tipo di errore.
	bool threwForMissingTable = false;
	try { doc.ResolveName("TabellaInesistente[Codice]"); }
	catch (...) { threwForMissingTable = true; }
	Check(threwForMissingTable, "una tabella inesistente lancia (come un nome non definito), non crasha");

	bool threwForMissingColumn = false;
	try { doc.ResolveName("Tabella12[ColonnaInesistente]"); }
	catch (...) { threwForMissingColumn = true; }
	Check(threwForMissingColumn, "una colonna inesistente in una tabella nota lancia allo stesso modo");

	// Una formula che referenzia una colonna inesistente resta
	// comunque viva (gNameNan), stesso principio gia' verificato in
	// named_ranges_test.cpp per "=NonDefinito*2" -- ma solo se il
	// riferimento fa parte di un'espressione piu' grande (qui "*1"):
	// un valName NUDO che non si risolve (nessun altro token attorno)
	// e' indistinguibile da un testo letterale battuto per sbaglio
	// (es. "Codice" sopra), quindi CFormula::ReduceToValue lo riduce
	// deliberatamente a testo invece di lasciarlo un riferimento vivo
	// -- stesso identico comportamento, per lo stesso motivo, di un
	// nome di intervallo indefinito scritto da solo.
	TryToParseString("=Tabella12[NonEsiste]*1", cell(7, 2), &doc, true); // G2
	doc.CalcCell(cell(7, 2));
	Value missing;
	doc.GetValue(cell(7, 2), missing);
	Check(missing.fType == eNumData && missing.IsNan(),
		"un riferimento a colonna inesistente dentro un'espressione piu' grande calcola gNameNan (NaN), formula viva, non testo morto");

	// Riferimenti composti (Fase 16): "Tabella12[[#This Row],[Col]]"
	// (riga della formula, una colonna specifica) e
	// "Tabella12[[Col1]:[Col2]]" (intervallo multi-colonna) -- 554
	// celle interessate in un file XLSX reale (vedi memoria
	// project_xlsx_formula_gaps_20260808). Scritti qui con la vera
	// sintassi di Excel (parentesi annidate), non con la codifica
	// interna semplificata ("Tabella12[#Col]"/"Tabella12[Col1:Col2]")
	// che CParser::ParseTableReference produce internamente -- questa
	// e' esattamente la sintassi che compare nei file reali.
	TryToParseString("=Tabella12[[#This Row],[Descrizione]]", cell(4, 2), &doc, true, '.', ',');
	doc.CalcCell(cell(4, 2));
	Value thisRow2;
	doc.GetValue(cell(4, 2), thisRow2);
	Check(thisRow2.fType == eTextData && strcmp((const char *)thisRow2, "Primo articolo") == 0,
		"Tabella12[[#This Row],[Descrizione]] scritto sulla riga 2 legge \"Primo articolo\" (quella riga)");

	// Stessa formula, riga 3: deve leggere QUELLA riga, non ripetere il
	// valore della riga 2 (verifica che la riga sia davvero presa dalla
	// posizione della formula, non da un indice fisso).
	TryToParseString("=Tabella12[[#This Row],[Descrizione]]", cell(4, 3), &doc, true, '.', ',');
	doc.CalcCell(cell(4, 3));
	Value thisRow3;
	doc.GetValue(cell(4, 3), thisRow3);
	Check(thisRow3.fType == eTextData && strcmp((const char *)thisRow3, "Secondo articolo") == 0,
		"la stessa formula sulla riga 3 legge \"Secondo articolo\", segue la riga della formula");

	// Fuori dalle righe dati della tabella (riga 1, l'intestazione):
	// resta non risolto (gNameNan), non legge una riga a caso.
	TryToParseString("=Tabella12[[#This Row],[Descrizione]]&\"\"", cell(4, 1), &doc, true, '.', ',');
	doc.CalcCell(cell(4, 1));
	Value thisRowOutside;
	doc.GetValue(cell(4, 1), thisRowOutside);
	Check(thisRowOutside.fType != eTextData || strlen((const char *)thisRowOutside) == 0,
		"Tabella12[[#This Row],[Descrizione]] scritto FUORI dalle righe dati della tabella non risolve a nulla");

	// Intervallo multi-colonna: [[Descrizione]:[u.m.]] -> B2:C3 (due
	// colonne, tutte le righe dati).
	range descToUm = doc.ResolveName("Tabella12[Descrizione:u.m.]", cell(5, 1));
	Check(descToUm.TopLeft() == cell(2, 2) && descToUm.BotRight() == cell(3, 3),
		"Tabella12[Descrizione:u.m.] (codifica interna di [[Descrizione]:[u.m.]]) risolve a B2:C3");

	// Stessa cosa, ma facendola davvero analizzare a CParser (sintassi
	// Excel con parentesi annidate) invece di richiamare direttamente
	// ResolveName con la codifica interna gia' pronta -- verifica che
	// il lessico/l'analizzatore producano davvero "Descrizione:u.m."
	// (non "Descrizione:u" o simili, dato che il nome contiene un
	// punto). Nessuna funzione con nome coinvolta (SUM/COUNTA
	// richiederebbero InitFunctions(), mai chiamata in questo file --
	// vedi named_functions_test.cpp per la verifica di calcolo vera
	// con funzioni con nome su questa stessa sintassi), quindi si
	// legge il bytecode risultante con GetCellFormula invece di
	// calcolare.
	TryToParseString("=Tabella12[[Descrizione]:[u.m.]]", cell(8, 1), &doc, true, '.', ',');
	char multiColFormula[256];
	doc.GetCellFormula(cell(8, 1), multiColFormula, sizeof(multiColFormula), false);
	Check(strcmp(multiColFormula, "Tabella12[Descrizione:u.m.]") == 0,
		"Tabella12[[Descrizione]:[u.m.]] analizzato da CParser produce Tabella12[Descrizione:u.m.]");

	// Non regressione: la colonna singola gia' esistente resta
	// invariata dopo tutte queste aggiunte.
	range codiceColAgain = doc.ResolveName("Tabella12[Codice]");
	Check(codiceColAgain.TopLeft() == cell(1, 2) && codiceColAgain.BotRight() == cell(1, 3),
		"Tabella12[Codice] (colonna singola) non e' regredito dopo aver aggiunto i riferimenti composti");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
