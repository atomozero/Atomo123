/*
	named_functions_test.cpp

	Verifica che le formule con funzioni con nome (SUM, IF, ecc.)
	funzionino davvero, non solo che il motore compili: fino a questa
	sessione la libreria engine non chiamava mai InitFunctions(),
	quindi GetFunctionNr trattava sempre "SUM(...)" come un
	identificatore sconosciuto invece che come una chiamata di
	funzione (vedi docs/ENGINE_API.md, nota "funzioni con nome non
	sono ancora realmente utilizzabili").

	A differenza di smoke_test.cpp (che testa solo formule aritmetiche
	senza funzioni con nome, e quindi non aveva mai bisogno di questo
	passaggio), questo test lega esplicitamente gResourceManager/
	gAppName a un file di risorse compilato al volo da questo stesso
	Makefile (con gli strumenti storici rez/bsl, vedi target
	test-functions) e chiama InitFunctions() prima di analizzare le
	formule -- lo stesso passaggio che ui/src/App.cpp fa all'avvio
	dell'app vera.
*/

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <Path.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "ResourceManager.h"

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
	BPath path("tests/named_functions.rsrc");
	gAppName = path;
	if (gResourceManager.SetTo(&path) != B_OK)
	{
		printf("FAIL impossibile aprire tests/named_functions.rsrc\n");
		return 1;
	}

	try
	{
		InitFunctions();
	}
	catch (CErr &e)
	{
		printf("FAIL InitFunctions ha lanciato un'eccezione: %s\n", (char *)e);
		return 1;
	}

	Check(gFuncCount == 121, "InitFunctions carica tutte le 121 funzioni della risorsa 'Func'");

	CContainer &doc = *new CContainer(NULL, NULL);

	cell a1(1, 1), a2(1, 2), a3(1, 3);
	cell b1(2, 1), b2(2, 2), b3(2, 3);

	TryToParseString("10", a1, &doc, true);
	TryToParseString("20", a2, &doc, true);
	TryToParseString("30", a3, &doc, true);

	Value v;
	try
	{
		TryToParseString("=SUM(A1:A3)", b1, &doc, true);
		doc.CalcCell(b1);
		doc.GetValue(b1, v);
		Check((double)v == 60.0, "=SUM(A1:A3) calcola 60");
	}
	catch (CErr &e)
	{
		printf("FAIL =SUM(A1:A3): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=IF(A1>5;100;200)", b2, &doc, true);
		doc.CalcCell(b2);
		doc.GetValue(b2, v);
		Check((double)v == 100.0, "=IF(A1>5;100;200) calcola 100 (ramo vero)");
	}
	catch (CErr &e)
	{
		printf("FAIL =IF(A1>5;100;200): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=MAX(A1:A3)", b3, &doc, true);
		doc.CalcCell(b3);
		doc.GetValue(b3, v);
		Check((double)v == 30.0, "=MAX(A1:A3) calcola 30");
	}
	catch (CErr &e)
	{
		printf("FAIL =MAX(A1:A3): %s\n", (char *)e);
		gFailures++;
	}

	// SUMIF/COUNTIF/AVERAGEIF: assenti dalle 86 funzioni originali di
	// Sum-It, aggiunte in questa sessione perche' mancava proprio
	// l'aggregazione condizionata. D1:D4 = categoria, E1:E4 = valore.
	TryToParseString("Mela", cell(4, 1), &doc, true);
	TryToParseString("10", cell(5, 1), &doc, true);
	TryToParseString("Pera", cell(4, 2), &doc, true);
	TryToParseString("5", cell(5, 2), &doc, true);
	TryToParseString("Mela", cell(4, 3), &doc, true);
	TryToParseString("20", cell(5, 3), &doc, true);
	TryToParseString("Banana", cell(4, 4), &doc, true);
	TryToParseString("7", cell(5, 4), &doc, true);

	try
	{
		TryToParseString("=SUMIF(D1:D4;\"Mela\";E1:E4)", cell(6, 1), &doc, true);
		doc.CalcCell(cell(6, 1));
		doc.GetValue(cell(6, 1), v);
		Check((double)v == 30.0, "=SUMIF(D1:D4;\"Mela\";E1:E4) calcola 30 (10+20)");
	}
	catch (CErr &e)
	{
		printf("FAIL =SUMIF: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=COUNTIF(D1:D4;\"Mela\")", cell(6, 2), &doc, true);
		doc.CalcCell(cell(6, 2));
		doc.GetValue(cell(6, 2), v);
		Check((double)v == 2.0, "=COUNTIF(D1:D4;\"Mela\") calcola 2");
	}
	catch (CErr &e)
	{
		printf("FAIL =COUNTIF: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=AVERAGEIF(D1:D4;\"Mela\";E1:E4)", cell(6, 3), &doc, true);
		doc.CalcCell(cell(6, 3));
		doc.GetValue(cell(6, 3), v);
		Check((double)v == 15.0, "=AVERAGEIF(D1:D4;\"Mela\";E1:E4) calcola 15 (media di 10 e 20)");
	}
	catch (CErr &e)
	{
		printf("FAIL =AVERAGEIF: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=SUMIF(E1:E4;\">8\")", cell(6, 4), &doc, true);
		doc.CalcCell(cell(6, 4));
		doc.GetValue(cell(6, 4), v);
		Check((double)v == 30.0,
			"=SUMIF(E1:E4;\">8\") con operatore di confronto calcola 30 (10+20, esclusi 5 e 7)");
	}
	catch (CErr &e)
	{
		printf("FAIL =SUMIF con operatore: %s\n", (char *)e);
		gFailures++;
	}

	// COUNTIFS (Fase 14): bug reale scoperto analizzando un file XLSX
	// reale (18 formule COUNTIFS mostrate come testo grezzo, funzione
	// del tutto assente dalla tabella). Colonna 22 (V) in poi, del
	// tutto separata dalle celle usate altrove in questo file (nessuna
	// dipendenza dall'ordine in cui le sezioni vengono eseguite).
	// V1:V4 = disponibilita' (Si/No), stesse righe di D1:D4/E1:E4
	// sopra -- solo la riga 1 (Mela, Si) soddisfa ENTRAMBI i criteri
	// (riga 3 e' Mela ma "No").
	TryToParseString("Si", cell(22, 1), &doc, true);
	TryToParseString("Si", cell(22, 2), &doc, true);
	TryToParseString("No", cell(22, 3), &doc, true);
	TryToParseString("Si", cell(22, 4), &doc, true);

	try
	{
		TryToParseString("=COUNTIFS(D1:D4;\"Mela\";V1:V4;\"Si\")", cell(23, 1), &doc, true);
		doc.CalcCell(cell(23, 1));
		doc.GetValue(cell(23, 1), v);
		Check((double)v == 1.0,
			"=COUNTIFS(D1:D4;\"Mela\";V1:V4;\"Si\") conta solo la riga 1 (Mela E Si, non la riga 3 che e' Mela ma No)");
	}
	catch (CErr &e)
	{
		printf("FAIL =COUNTIFS: %s\n", (char *)e);
		gFailures++;
	}

	// CEILING.MATH (Fase 14): altro bug reale scoperto sullo stesso
	// file (2 formule, sempre con un solo argomento -- vedi
	// GetFunctionNr in Utils.cpp sul perche' e' un alias diretto di
	// CEILING invece di una vera nuova funzione).
	try
	{
		TryToParseString("=_xlfn.CEILING.MATH(7.35+2+1.8)", cell(23, 2), &doc, true, '.', ',');
		doc.CalcCell(cell(23, 2));
		doc.GetValue(cell(23, 2), v);
		Check((double)v == 12.0,
			"=_xlfn.CEILING.MATH(7.35+2+1.8) (11.15) arrotonda per eccesso a 12, come nel file reale che ha fatto scoprire il bug");
	}
	catch (CErr &e)
	{
		printf("FAIL =_xlfn.CEILING.MATH: %s\n", (char *)e);
		gFailures++;
	}

	// CONCATENATE/ROUNDUP/ROUNDDOWN/TEXT (Fase 14): scoperti mancanti
	// analizzando altri file XLSX reali dell'utente (non lo stesso file
	// di XLOOKUP/COUNTIFS sopra). CONCATENATE e' un alias diretto di
	// CONCAT (stesso motivo di CEILING.MATH sopra, nome troppo lungo
	// per la risorsa 'Func' a lunghezza fissa).
	try
	{
		TryToParseString("=CONCATENATE(\"Totale: \";A1)", cell(23, 3), &doc, true);
		doc.CalcCell(cell(23, 3));
		doc.GetValue(cell(23, 3), v);
		Check(strcmp((const char *)v, "Totale: 10") == 0,
			"=CONCATENATE(\"Totale: \";A1) (nome storico Excel) calcola \"Totale: 10\", come CONCAT");
	}
	catch (CErr &e)
	{
		printf("FAIL =CONCATENATE: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=ROUNDUP(3.21,1)", cell(23, 4), &doc, true, '.', ',');
		doc.CalcCell(cell(23, 4));
		doc.GetValue(cell(23, 4), v);
		Check((double)v == 3.3, "=ROUNDUP(3.21,1) arrotonda SEMPRE per eccesso a 3.3, non al piu' vicino");
	}
	catch (CErr &e)
	{
		printf("FAIL =ROUNDUP: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=ROUNDDOWN(3.29,1)", cell(23, 5), &doc, true, '.', ',');
		doc.CalcCell(cell(23, 5));
		doc.GetValue(cell(23, 5), v);
		Check((double)v == 3.2, "=ROUNDDOWN(3.29,1) tronca SEMPRE verso lo zero a 3.2, non al piu' vicino");
	}
	catch (CErr &e)
	{
		printf("FAIL =ROUNDDOWN: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// Stesso formato ("000", zero-riempimento) visto nel file reale
		// che ha fatto scoprire il bug (analisi_funzioni_xls.md).
		TryToParseString("=TEXT(7;\"000\")", cell(23, 6), &doc, true);
		doc.CalcCell(cell(23, 6));
		doc.GetValue(cell(23, 6), v);
		Check(strcmp((const char *)v, "007") == 0,
			"=TEXT(7,\"000\") riempie di zeri a 3 cifre (\"007\"), come nel file reale che ha fatto scoprire il bug");
	}
	catch (CErr &e)
	{
		printf("FAIL =TEXT: %s\n", (char *)e);
		gFailures++;
	}

	// TRIM/UPPER/LOWER/PROPER/FIND/SEARCH/CONCAT/MEDIAN/MODE: assenti
	// dalle funzioni originali di Sum-It, aggiunte in questa sessione
	// (Fase 13) per colmare il divario con Excel sulle funzioni di
	// testo/statistiche piu' comuni.
	TryToParseString("\"  Atomo   123  \"", cell(7, 1), &doc, true);
	TryToParseString("\"Atomo123\"", cell(7, 2), &doc, true);
	TryToParseString("\"atomo123\"", cell(7, 3), &doc, true);
	TryToParseString("\"atomo 123 haiku\"", cell(7, 4), &doc, true);

	try
	{
		TryToParseString("=TRIM(G1)", cell(8, 1), &doc, true);
		doc.CalcCell(cell(8, 1));
		doc.GetValue(cell(8, 1), v);
		Check(strcmp((const char *)v, "Atomo 123") == 0,
			"=TRIM(\"  Atomo   123  \") elimina gli spazi esterni e riduce quelli interni a uno solo");
	}
	catch (CErr &e)
	{
		printf("FAIL =TRIM: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=UPPER(G2)", cell(8, 2), &doc, true);
		doc.CalcCell(cell(8, 2));
		doc.GetValue(cell(8, 2), v);
		Check(strcmp((const char *)v, "ATOMO123") == 0, "=UPPER(\"Atomo123\") calcola \"ATOMO123\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =UPPER: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=LOWER(G2)", cell(8, 3), &doc, true);
		doc.CalcCell(cell(8, 3));
		doc.GetValue(cell(8, 3), v);
		Check(strcmp((const char *)v, "atomo123") == 0, "=LOWER(\"Atomo123\") calcola \"atomo123\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =LOWER: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=PROPER(G4)", cell(8, 4), &doc, true);
		doc.CalcCell(cell(8, 4));
		doc.GetValue(cell(8, 4), v);
		Check(strcmp((const char *)v, "Atomo 123 Haiku") == 0,
			"=PROPER(\"atomo 123 haiku\") calcola \"Atomo 123 Haiku\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =PROPER: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=FIND(\"123\";G2)", cell(9, 1), &doc, true);
		doc.CalcCell(cell(9, 1));
		doc.GetValue(cell(9, 1), v);
		Check((double)v == 6.0, "=FIND(\"123\";\"Atomo123\") calcola 6 (posizione 1-based)");
	}
	catch (CErr &e)
	{
		printf("FAIL =FIND: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=FIND(\"XYZ\";G2)", cell(9, 2), &doc, true);
		doc.CalcCell(cell(9, 2));
		doc.GetValue(cell(9, 2), v);
		Check(v.fType == eNumData && std::isnan((double)v),
			"=FIND(\"XYZ\";\"Atomo123\") non trova nulla, restituisce un errore");
	}
	catch (CErr &e)
	{
		printf("FAIL =FIND senza corrispondenza: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=SEARCH(\"ATOMO\";G2)", cell(9, 3), &doc, true);
		doc.CalcCell(cell(9, 3));
		doc.GetValue(cell(9, 3), v);
		Check((double)v == 1.0,
			"=SEARCH(\"ATOMO\";\"Atomo123\") ignora maiuscole/minuscole, calcola 1");
	}
	catch (CErr &e)
	{
		printf("FAIL =SEARCH: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=CONCAT(\"Foglio \";A1;\" righe\")", cell(9, 4), &doc, true);
		doc.CalcCell(cell(9, 4));
		doc.GetValue(cell(9, 4), v);
		Check(strcmp((const char *)v, "Foglio 10 righe") == 0,
			"=CONCAT(\"Foglio \";A1;\" righe\") unisce testo e numero in \"Foglio 10 righe\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =CONCAT: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=MEDIAN(A1:A3)", cell(10, 1), &doc, true);
		doc.CalcCell(cell(10, 1));
		doc.GetValue(cell(10, 1), v);
		Check((double)v == 20.0, "=MEDIAN(A1:A3) su 10,20,30 calcola 20");
	}
	catch (CErr &e)
	{
		printf("FAIL =MEDIAN: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=MODE(5;7;5;9)", cell(10, 2), &doc, true);
		doc.CalcCell(cell(10, 2));
		doc.GetValue(cell(10, 2), v);
		Check((double)v == 5.0, "=MODE(5;7;5;9) calcola 5 (l'unico valore ripetuto)");
	}
	catch (CErr &e)
	{
		printf("FAIL =MODE: %s\n", (char *)e);
		gFailures++;
	}

	// IFERROR/VLOOKUP/HLOOKUP con separatore ',' esplicito (Fase 13):
	// tre bug reali distinti scoperti su un file XLSX reale (colonna di
	// codici sparita, formule mostrate come testo grezzo invece del
	// valore calcolato).
	//
	// 1. "IFERROR" (nome standard Excel) non esisteva affatto nella
	//    tabella funzioni -- solo "IFERR" (nome storico di Sum-It), mai
	//    usato da un file XLSX vero. Aggiunta come alias della stessa
	//    IFERRFunction.
	// 2. VLOOKUP/HLOOKUP avevano argCnt=3 ESATTO nella risorsa 'Func',
	//    ma il vero VLOOKUP di Excel ha un quarto argomento opzionale
	//    (corrispondenza esatta/approssimata) che un file reale usa
	//    quasi sempre esplicitamente -- un parser che rifiuta 4
	//    argomenti fa fallire l'intera formula.
	// 3. Il traduttore XLSX chiamava TryToParseString senza mai passare
	//    decSep='.'/listSep=',' espliciti: il testo di <f> in un file
	//    XLSX e' SEMPRE nel formato canonico ECMA-376 (virgola fra gli
	//    argomenti), indipendente dalla lingua con cui e' stato scritto
	//    in Excel -- con gListSeparator=';' (l'impostazione predefinita
	//    per l'Italia, vedi App.cpp) OGNI formula con piu' di un
	//    argomento falliva l'analisi grammaticale e ripiegava sul testo
	//    grezzo della formula invece di calcolarla. Qui simulato
	//    passando esplicitamente '.'/',' a TryToParseString, come fa
	//    ora XlsxTranslator::ParseSheet.
	//
	// L1:L3 = colonna chiave (1,2,3), M1:M3 = seconda colonna (valori
	// di testo), per verificare che VLOOKUP prenda davvero la colonna
	// giusta (bug indipendente scoperto verificando il punto 2 sopra:
	// l'aritmetica dello scarto di colonna/riga era sfasata di uno,
	// restituiva sempre la colonna/riga SUCCESSIVA a quella richiesta
	// -- mai notato prima perche' nessun test aveva mai controllato il
	// valore VERO restituito, solo l'assenza di crash).
	TryToParseString("1", cell(12, 1), &doc, true); // L1
	TryToParseString("2", cell(12, 2), &doc, true); // L2
	TryToParseString("3", cell(12, 3), &doc, true); // L3
	TryToParseString("uno", cell(13, 1), &doc, true); // M1
	TryToParseString("due", cell(13, 2), &doc, true); // M2
	TryToParseString("tre", cell(13, 3), &doc, true); // M3
	TryToParseString("1", cell(12, 4), &doc, true); // L4
	TryToParseString("2", cell(13, 4), &doc, true); // M4
	TryToParseString("3", cell(14, 4), &doc, true); // N4
	TryToParseString("dieci", cell(12, 5), &doc, true); // L5
	TryToParseString("venti", cell(13, 5), &doc, true); // M5
	TryToParseString("trenta", cell(14, 5), &doc, true); // N5

	try
	{
		TryToParseString("=IFERROR(1/0,99)", cell(14, 1), &doc, true, '.', ',');
		doc.CalcCell(cell(14, 1));
		doc.GetValue(cell(14, 1), v);
		Check((double)v == 99.0, "=IFERROR(1/0,99) riconosce il nome standard Excel, calcola 99");
	}
	catch (CErr &e)
	{
		printf("FAIL =IFERROR: %s\n", (char *)e);
		gFailures++;
	}

	// Bug reale scoperto su un file XLSX reale (analisi_funzioni_xls.md,
	// "=IFERROR(CONCAT(...);\"\")", comunissimo li'): quando "valore"
	// (primo argomento) NON e' un numero controllabile con isnan() --
	// cioe' non un errore rilevabile da questo motore, che non ha un
	// vero tipo di errore -- IFERROR con SOLO due argomenti finiva
	// sempre nel ramo "else stack[0].Clear();" di IFERRFunction,
	// perdendo il risultato buono invece di restituirlo cosi' com'e'
	// (come fa il vero IFERROR di Excel). Peggio ancora, Value::Clear()
	// non azzerava fType insieme a fText: una Value gia' eTextData
	// restava "eTextData" con fText=NULL dopo Clear(), e
	// CellData::operator=(Value&) (Container.cpp) chiamava STRDUP(NULL)
	// su quel puntatore nullo -- FailNil() lanciava un'eccezione
	// (errInsufficientMemory) che sembrava scollegata dalla causa reale,
	// mai catturata da CalcCell ne' da RecalculateAll, mandando in
	// crash l'intera importazione del file reale. Qui riprodotto con
	// CONCAT (non CONCATENATE, per verificare che il bug sia in
	// IFERROR/Value::Clear(), non nell'alias) su un argomento testo
	// vero, non un errore.
	try
	{
		TryToParseString("=IFERROR(CONCAT(\"a\";\"b\");\"fallback\")", cell(23, 7), &doc, true);
		doc.CalcCell(cell(23, 7));
		doc.GetValue(cell(23, 7), v);
		Check(v.fType == eTextData && strcmp((const char *)v, "ab") == 0,
			"=IFERROR(CONCAT(\"a\";\"b\");\"fallback\") con un risultato valido (non un errore) "
			"restituisce \"ab\", non lo perde ne' va in crash");
	}
	catch (CErr &e)
	{
		printf("FAIL =IFERROR(CONCAT(...)): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=VLOOKUP(2,L1:M3,2,0)", cell(14, 2), &doc, true, '.', ',');
		doc.CalcCell(cell(14, 2));
		doc.GetValue(cell(14, 2), v);
		Check(strcmp((const char *)v, "due") == 0,
			"=VLOOKUP(2,L1:M3,2,0) con quattro argomenti e corrispondenza esatta calcola \"due\" (colonna giusta)");
	}
	catch (CErr &e)
	{
		printf("FAIL =VLOOKUP esatto: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=VLOOKUP(99,L1:M3,2,0)", cell(14, 3), &doc, true, '.', ',');
		doc.CalcCell(cell(14, 3));
		doc.GetValue(cell(14, 3), v);
		Check(v.fType == eNumData && std::isnan((double)v),
			"=VLOOKUP(99,L1:M3,2,0) senza corrispondenza esatta restituisce un errore, non la riga sbagliata");
	}
	catch (CErr &e)
	{
		printf("FAIL =VLOOKUP senza corrispondenza: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=HLOOKUP(2,L4:N5,2,0)", cell(15, 1), &doc, true, '.', ',');
		doc.CalcCell(cell(15, 1));
		doc.GetValue(cell(15, 1), v);
		Check(strcmp((const char *)v, "venti") == 0,
			"=HLOOKUP(2,L4:N5,2,0) con quattro argomenti calcola \"venti\" (riga giusta, L4:N4=1,2,3)");
	}
	catch (CErr &e)
	{
		printf("FAIL =HLOOKUP: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=IF(1<>2,\"vero\",\"falso\")", cell(14, 5), &doc, true, '.', ',');
		doc.CalcCell(cell(14, 5));
		doc.GetValue(cell(14, 5), v);
		Check(strcmp((const char *)v, "vero") == 0,
			"=IF(1<>2,\"vero\",\"falso\") con virgole esplicite (formato XLSX) calcola \"vero\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =IF con virgole: %s\n", (char *)e);
		gFailures++;
	}

	// INDEX/MATCH (Fase 13): non HINDEX/VINDEX preesistenti (nonostante
	// il nome, quelli fanno una ricerca in stile MATCH approssimato,
	// non "valore alla posizione N" -- vedi il commento su
	// INDEXFunction in Functions.spreadsheet.cpp). Riusa L1:N5 gia'
	// popolata sopra per VLOOKUP/HLOOKUP.
	try
	{
		TryToParseString("=INDEX(M1:M3,2)", cell(15, 2), &doc, true, '.', ',');
		doc.CalcCell(cell(15, 2));
		doc.GetValue(cell(15, 2), v);
		Check(strcmp((const char *)v, "due") == 0,
			"=INDEX(M1:M3,2) su una colonna sola calcola \"due\" (seconda riga)");
	}
	catch (CErr &e)
	{
		printf("FAIL =INDEX su una colonna: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=INDEX(L1:M3,2,2)", cell(15, 3), &doc, true, '.', ',');
		doc.CalcCell(cell(15, 3));
		doc.GetValue(cell(15, 3), v);
		Check(strcmp((const char *)v, "due") == 0,
			"=INDEX(L1:M3,2,2) con riga e colonna esplicite calcola \"due\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =INDEX con riga e colonna: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// L4:N4 = 1,2,3: intervallo a una riga sola, un unico argomento
		// numerico seleziona la COLONNA, non la riga (che puo' essere
		// solo 1) -- stesso comportamento "opzionale" del vero INDEX.
		TryToParseString("=INDEX(L4:N4,2)", cell(15, 4), &doc, true, '.', ',');
		doc.CalcCell(cell(15, 4));
		doc.GetValue(cell(15, 4), v);
		Check((double)v == 2.0,
			"=INDEX(L4:N4,2) su una riga sola interpreta l'unico argomento come colonna, calcola 2");
	}
	catch (CErr &e)
	{
		printf("FAIL =INDEX su una riga: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// riga=0: l'intera colonna 1 di L1:M3 (L1:L3 = 1,2,3) come
		// INTERVALLO, consumato da SUM -- stesso principio di OFFSET.
		TryToParseString("=SUM(INDEX(L1:M3,0,1))", cell(15, 5), &doc, true, '.', ',');
		doc.CalcCell(cell(15, 5));
		doc.GetValue(cell(15, 5), v);
		Check((double)v == 6.0,
			"=SUM(INDEX(L1:M3,0,1)) con riga 0 restituisce l'intera colonna come intervallo, somma 6");
	}
	catch (CErr &e)
	{
		printf("FAIL =INDEX con riga 0: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=MATCH(2,L1:L3,0)", cell(16, 1), &doc, true, '.', ',');
		doc.CalcCell(cell(16, 1));
		doc.GetValue(cell(16, 1), v);
		Check((double)v == 2.0,
			"=MATCH(2,L1:L3,0) con corrispondenza esatta calcola 2 (posizione, non il valore)");
	}
	catch (CErr &e)
	{
		printf("FAIL =MATCH esatto numerico: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=MATCH(\"tre\",M1:M3,0)", cell(16, 2), &doc, true, '.', ',');
		doc.CalcCell(cell(16, 2));
		doc.GetValue(cell(16, 2), v);
		Check((double)v == 3.0,
			"=MATCH(\"tre\",M1:M3,0) con corrispondenza esatta su testo calcola 3");
	}
	catch (CErr &e)
	{
		printf("FAIL =MATCH esatto testo: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=MATCH(3,L4:N4,0)", cell(16, 3), &doc, true, '.', ',');
		doc.CalcCell(cell(16, 3));
		doc.GetValue(cell(16, 3), v);
		Check((double)v == 3.0,
			"=MATCH(3,L4:N4,0) su un intervallo a una riga sola calcola 3");
	}
	catch (CErr &e)
	{
		printf("FAIL =MATCH su una riga: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=MATCH(99,L1:L3,0)", cell(16, 4), &doc, true, '.', ',');
		doc.CalcCell(cell(16, 4));
		doc.GetValue(cell(16, 4), v);
		Check(v.fType == eNumData && std::isnan((double)v),
			"=MATCH(99,L1:L3,0) senza corrispondenza restituisce un errore");
	}
	catch (CErr &e)
	{
		printf("FAIL =MATCH senza corrispondenza: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// L1:L3 = 1,2,3, crescente: tipo=1 (predefinito) trova l'ultimo
		// valore <= chiave.
		TryToParseString("=MATCH(2.5,L1:L3,1)", cell(16, 5), &doc, true, '.', ',');
		doc.CalcCell(cell(16, 5));
		doc.GetValue(cell(16, 5), v);
		Check((double)v == 2.0,
			"=MATCH(2.5,L1:L3,1) con corrispondenza approssimata calcola 2 (ultimo valore <= 2.5)");
	}
	catch (CErr &e)
	{
		printf("FAIL =MATCH approssimato: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// INDEX+MATCH insieme, l'uso combinato piu' comune: cerca "tre"
		// nella colonna chiave (M1:M3) e restituisce il valore
		// corrispondente nella colonna L (L1:L3).
		TryToParseString("=INDEX(L1:L3,MATCH(\"tre\",M1:M3,0))", cell(16, 6), &doc, true, '.', ',');
		doc.CalcCell(cell(16, 6));
		doc.GetValue(cell(16, 6), v);
		Check((double)v == 3.0,
			"=INDEX(L1:L3,MATCH(\"tre\",M1:M3,0)) combinati restituiscono 3");
	}
	catch (CErr &e)
	{
		printf("FAIL =INDEX+MATCH combinati: %s\n", (char *)e);
		gFailures++;
	}

	// XLOOKUP/IFS (Fase 14): funzioni Excel piu' recenti del formato
	// dichiarato del file, scritte con "_xlfn." davanti nei file XLSX
	// veri -- bug reale segnalato dall'utente, un file XLSX reale con
	// XLOOKUP su colonne di una Tabella Excel mostrava il testo
	// letterale della formula invece del valore calcolato. Riusa
	// L1:M3 gia' popolata sopra per VLOOKUP/INDEX/MATCH.
	try
	{
		TryToParseString("=XLOOKUP(2,L1:L3,M1:M3)", cell(17, 1), &doc, true, '.', ',');
		doc.CalcCell(cell(17, 1));
		doc.GetValue(cell(17, 1), v);
		Check(strcmp((const char *)v, "due") == 0,
			"=XLOOKUP(2,L1:L3,M1:M3) trova 2 in L1:L3 e restituisce \"due\" da M1:M3 (stessa posizione)");
	}
	catch (CErr &e)
	{
		printf("FAIL =XLOOKUP: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=XLOOKUP(99,L1:L3,M1:M3,\"non trovato\")", cell(17, 2), &doc, true, '.', ',');
		doc.CalcCell(cell(17, 2));
		doc.GetValue(cell(17, 2), v);
		Check(strcmp((const char *)v, "non trovato") == 0,
			"=XLOOKUP(99,L1:L3,M1:M3,\"non trovato\") senza corrispondenza usa il quarto argomento (if_not_found)");
	}
	catch (CErr &e)
	{
		printf("FAIL =XLOOKUP senza corrispondenza: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=IFS(A1>100;\"alto\";A1>5;\"medio\";TRUE;\"basso\")", cell(17, 3), &doc, true);
		doc.CalcCell(cell(17, 3));
		doc.GetValue(cell(17, 3), v);
		Check(strcmp((const char *)v, "medio") == 0,
			"=IFS(...) con A1=10 sceglie la prima condizione vera (\"medio\"), non la prima in assoluto (\"alto\" e' falsa)");
	}
	catch (CErr &e)
	{
		printf("FAIL =IFS: %s\n", (char *)e);
		gFailures++;
	}

	// Scenario reale (screenshot dell'utente): "+_xlfn.XLOOKUP(B5,
	// Tabella12[Codice],Tabella12[Descrizione])" su una vera Tabella
	// Excel (ListObject) -- combina TUTTI i pezzi di questa fase: il
	// "+" unario (gia' testato altrove, vedi ui/tests/
	// test_unary_plus.cpp), il prefisso "_xlfn." (identificatore
	// punteggiato nel lessico), e il riferimento a tabella strutturata
	// (vedi engine/tests/table_refs_test.cpp per ResolveName da solo).
	// Qui verificato insieme, end-to-end, con una vera XLOOKUPFunction
	// a leggere il risultato -- la prova diretta che il bug segnalato
	// e' risolto.
	TryToParseString("Codice", cell(19, 1), &doc, true);      // S1 (intestazione)
	TryToParseString("Descrizione", cell(20, 1), &doc, true); // T1
	TryToParseString("ABC", cell(19, 2), &doc, true);         // S2
	TryToParseString("Primo articolo", cell(20, 2), &doc, true); // T2
	TryToParseString("XYZ", cell(19, 3), &doc, true);         // S3
	TryToParseString("Secondo articolo", cell(20, 3), &doc, true); // T3
	{
		CTableDef table;
		table.dataRange = range(19, 2, 20, 3); // S2:T3, intestazione esclusa
		table.columnNames.push_back("Codice");
		table.columnNames.push_back("Descrizione");
		doc.AddTable("Tabella12", table);
	}
	TryToParseString("XYZ", cell(21, 1), &doc, true); // U1 (valore cercato, come B5 nel file reale)

	try
	{
		TryToParseString("=+_xlfn.XLOOKUP(U1,Tabella12[Codice],Tabella12[Descrizione])",
			cell(21, 2), &doc, true, '.', ','); // U2
		doc.CalcCell(cell(21, 2));
		doc.GetValue(cell(21, 2), v);
		Check(strcmp((const char *)v, "Secondo articolo") == 0,
			"\"+_xlfn.XLOOKUP(U1,Tabella12[Codice],Tabella12[Descrizione])\" (scenario reale) "
			"calcola \"Secondo articolo\", non resta testo letterale");
	}
	catch (CErr &e)
	{
		printf("FAIL scenario reale +_xlfn.XLOOKUP su Tabella[...]: %s\n", (char *)e);
		gFailures++;
	}

	// NOT/XOR/SWITCH/IFNA/ISBLANK/ISERROR/ISNA/ISFORMULA (Fase 26, vedi
	// ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni
	// originali di Sum-It, mancanti confrontando la tabella con
	// l'elenco standard di Excel. A1 (10) e B1 (=SUM(A1:A3)) sono gia'
	// definite piu' sopra; risultati scritti in colonna 40, ben lontano
	// dalle colonne 25/26 (Y/Z) gia' riservate piu' sotto per il test
	// di IF su una cella vuota -- un vero incidente successo scrivendo
	// questi stessi test la prima volta (colonna 25 sovrascriveva Y1,
	// che quel test si aspettava restasse vuota).
	try
	{
		TryToParseString("=NOT(A1>100)", cell(40, 1), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 1));
		doc.GetValue(cell(40, 1), v);
		Check(v.fType == eBoolData && (bool)v == true,
			"=NOT(A1>100) calcola VERO (10>100 e' falso)");
	}
	catch (CErr &e)
	{
		printf("FAIL =NOT: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// A1=10, A2=20: entrambe >5, due argomenti VERI (pari) -> FALSO.
		TryToParseString("=XOR(A1>5,A2>5)", cell(40, 2), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 2));
		doc.GetValue(cell(40, 2), v);
		Check(v.fType == eBoolData && (bool)v == false,
			"=XOR(A1>5,A2>5) calcola FALSO (due argomenti VERI, numero pari)");
	}
	catch (CErr &e)
	{
		printf("FAIL =XOR (pari): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// A1=10 (>5, vero), A3=30 (<=25, falso): un solo VERO (dispari) -> VERO.
		TryToParseString("=XOR(A1>5,A3<=25)", cell(40, 3), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 3));
		doc.GetValue(cell(40, 3), v);
		Check(v.fType == eBoolData && (bool)v == true,
			"=XOR(A1>5,A3<=25) calcola VERO (un solo argomento VERO, numero dispari)");
	}
	catch (CErr &e)
	{
		printf("FAIL =XOR (dispari): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=SWITCH(A1,10,\"dieci\",20,\"venti\",\"altro\")", cell(40, 4), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 4));
		doc.GetValue(cell(40, 4), v);
		Check(strcmp((const char *)v, "dieci") == 0,
			"=SWITCH(A1,10,\"dieci\",20,\"venti\",\"altro\") con A1=10 calcola \"dieci\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =SWITCH (corrispondenza): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=SWITCH(A3,10,\"dieci\",20,\"venti\",\"altro\")", cell(40, 5), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 5));
		doc.GetValue(cell(40, 5), v);
		Check(strcmp((const char *)v, "altro") == 0,
			"=SWITCH(A3,10,\"dieci\",20,\"venti\",\"altro\") con A3=30 (nessuna corrispondenza) "
			"calcola il predefinito \"altro\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =SWITCH (predefinito): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=IFNA(NA(),\"mancante\")", cell(40, 6), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 6));
		doc.GetValue(cell(40, 6), v);
		Check(strcmp((const char *)v, "mancante") == 0,
			"=IFNA(NA(),\"mancante\") sostituisce #N/A con \"mancante\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =IFNA (con #N/A): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=IFNA(A1,\"mancante\")", cell(40, 7), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 7));
		doc.GetValue(cell(40, 7), v);
		Check(v.fType == eNumData && (double)v == 10.0,
			"=IFNA(A1,\"mancante\") con A1=10 (nessun errore) lascia passare 10, "
			"non lo sostituisce col valore di riserva");
	}
	catch (CErr &e)
	{
		printf("FAIL =IFNA (senza errore): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// AO1 (colonna 41), mai scritta altrove in questo file: stesso
		// principio di Y1 usata piu' sotto per IF(cellaVuota=0;...) --
		// una colonna diversa apposta, per non ripetere lo stesso
		// errore di disattenzione gia' documentato li' (vedi il
		// commento su "cell(26, 4)" invece di "cell(25, 4)").
		TryToParseString("=ISBLANK(AO1)", cell(40, 8), &doc, true);
		doc.CalcCell(cell(40, 8));
		doc.GetValue(cell(40, 8), v);
		Check(v.fType == eBoolData && (bool)v == true,
			"=ISBLANK(AO1) calcola VERO (AO1 non ha mai avuto un valore)");
	}
	catch (CErr &e)
	{
		printf("FAIL =ISBLANK (vuota): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=ISBLANK(A1)", cell(40, 9), &doc, true);
		doc.CalcCell(cell(40, 9));
		doc.GetValue(cell(40, 9), v);
		Check(v.fType == eBoolData && (bool)v == false,
			"=ISBLANK(A1) calcola FALSO (A1 vale 10)");
	}
	catch (CErr &e)
	{
		printf("FAIL =ISBLANK (piena): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=ISERROR(1/0)", cell(40, 10), &doc, true);
		doc.CalcCell(cell(40, 10));
		doc.GetValue(cell(40, 10), v);
		Check(v.fType == eBoolData && (bool)v == true,
			"=ISERROR(1/0) calcola VERO (divisione per zero, un errore qualunque)");
	}
	catch (CErr &e)
	{
		printf("FAIL =ISERROR (con errore): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=ISERROR(A1)", cell(40, 11), &doc, true);
		doc.CalcCell(cell(40, 11));
		doc.GetValue(cell(40, 11), v);
		Check(v.fType == eBoolData && (bool)v == false,
			"=ISERROR(A1) calcola FALSO (A1 vale 10, nessun errore)");
	}
	catch (CErr &e)
	{
		printf("FAIL =ISERROR (senza errore): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=ISNA(NA())", cell(40, 12), &doc, true);
		doc.CalcCell(cell(40, 12));
		doc.GetValue(cell(40, 12), v);
		Check(v.fType == eBoolData && (bool)v == true, "=ISNA(NA()) calcola VERO");
	}
	catch (CErr &e)
	{
		printf("FAIL =ISNA (con #N/A): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		// #DIV/0! non e' #N/A: ISNA deve distinguere i due errori (a
		// differenza di ISERROR sopra, che li tratta tutti allo stesso
		// modo) -- la prova diretta che GetNanNr funziona davvero.
		TryToParseString("=ISNA(1/0)", cell(40, 13), &doc, true);
		doc.CalcCell(cell(40, 13));
		doc.GetValue(cell(40, 13), v);
		Check(v.fType == eBoolData && (bool)v == false,
			"=ISNA(1/0) calcola FALSO (#DIV/0! non e' #N/A, anche se entrambi sono errori)");
	}
	catch (CErr &e)
	{
		printf("FAIL =ISNA (con un altro errore): %s\n", (char *)e);
		gFailures++;
	}

	// ISFORMULA ha bisogno del riferimento di cella VERO (eRangeData),
	// non del suo valore -- ma il parser (parser.cpp, "case CELL")
	// genera SEMPRE un valCell per un riferimento a una sola cella
	// senza ":", che il bytecode interpreter (Formula.cpp, "case
	// valCell") deferenzia SUBITO al valore della cella, qualunque sia
	// la funzione che lo usa: non esiste modo, in questo motore, di
	// far arrivare a una funzione il riferimento NUDO invece del
	// valore. "B1" da solo restituirebbe quindi #REF! (lo stesso
	// limite gia' presente, mai scoperto finora, di ROW()/COLUMN() con
	// un solo argomento) -- un intervallo VERO di almeno due celle
	// (qui B1:B2) e' l'unico modo di preservare il riferimento fino
	// alla funzione, che legge sempre e solo l'angolo in alto a
	// sinistra dell'intervallo.
	try
	{
		TryToParseString("=ISFORMULA(B1:B2)", cell(40, 14), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 14));
		doc.GetValue(cell(40, 14), v);
		Check(v.fType == eBoolData && (bool)v == true,
			"=ISFORMULA(B1:B2) calcola VERO (B1, l'angolo in alto a sinistra, e' =SUM(A1:A3))");
	}
	catch (CErr &e)
	{
		printf("FAIL =ISFORMULA (con formula): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=ISFORMULA(A1:A2)", cell(40, 15), &doc, true, '.', ',');
		doc.CalcCell(cell(40, 15));
		doc.GetValue(cell(40, 15), v);
		Check(v.fType == eBoolData && (bool)v == false,
			"=ISFORMULA(A1:A2) calcola FALSO (A1, l'angolo in alto a sinistra, e' un valore letterale)");
	}
	catch (CErr &e)
	{
		printf("FAIL =ISFORMULA (senza formula): %s\n", (char *)e);
		gFailures++;
	}

	// SUBSTITUTE/REPLACE/REPT/TEXTJOIN/VALUE/EXACT (Fase 26, vedi
	// ROADMAP.md "v3.0 Consolidation"): assenti dalle funzioni
	// originali di Sum-It, mancanti confrontando la tabella con
	// l'elenco standard di Excel. Risultati in colonna 45, ben lontano
	// da colonna 40 (batch precedente) e da 25/26 (Y/Z, riservate per
	// IF su cella vuota piu' sotto) -- stessa cautela gia' imparata li'.
	try
	{
		TryToParseString("=SUBSTITUTE(\"banana\",\"a\",\"o\")", cell(45, 1), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 1));
		doc.GetValue(cell(45, 1), v);
		Check(strcmp((const char *)v, "bonono") == 0,
			"=SUBSTITUTE(\"banana\",\"a\",\"o\") senza occorrenza sostituisce TUTTE le \"a\", calcola \"bonono\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =SUBSTITUTE (tutte): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=SUBSTITUTE(\"banana\",\"a\",\"o\",2)", cell(45, 2), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 2));
		doc.GetValue(cell(45, 2), v);
		Check(strcmp((const char *)v, "banona") == 0,
			"=SUBSTITUTE(\"banana\",\"a\",\"o\",2) con occorrenza sostituisce SOLO la seconda \"a\", calcola \"banona\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =SUBSTITUTE (occorrenza): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=REPLACE(\"Atomo123\",1,5,\"Hello\")", cell(45, 3), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 3));
		doc.GetValue(cell(45, 3), v);
		Check(strcmp((const char *)v, "Hello123") == 0,
			"=REPLACE(\"Atomo123\",1,5,\"Hello\") sostituisce i primi 5 caratteri, calcola \"Hello123\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =REPLACE: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=REPT(\"ab\",3)", cell(45, 4), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 4));
		doc.GetValue(cell(45, 4), v);
		Check(strcmp((const char *)v, "ababab") == 0, "=REPT(\"ab\",3) calcola \"ababab\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =REPT: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=TEXTJOIN(\"-\",TRUE,\"a\",\"\",\"b\")", cell(45, 5), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 5));
		doc.GetValue(cell(45, 5), v);
		Check(strcmp((const char *)v, "a-b") == 0,
			"=TEXTJOIN(\"-\",TRUE,\"a\",\"\",\"b\") con ignora_vuoti=VERO salta l'argomento vuoto, calcola \"a-b\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =TEXTJOIN (ignora vuoti): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=TEXTJOIN(\"-\",FALSE,\"a\",\"\",\"b\")", cell(45, 6), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 6));
		doc.GetValue(cell(45, 6), v);
		Check(strcmp((const char *)v, "a--b") == 0,
			"=TEXTJOIN(\"-\",FALSE,\"a\",\"\",\"b\") con ignora_vuoti=FALSO tiene l'argomento vuoto, calcola \"a--b\"");
	}
	catch (CErr &e)
	{
		printf("FAIL =TEXTJOIN (tiene vuoti): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=VALUE(\"42\")", cell(45, 7), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 7));
		doc.GetValue(cell(45, 7), v);
		Check(v.fType == eNumData && (double)v == 42.0, "=VALUE(\"42\") calcola 42");
	}
	catch (CErr &e)
	{
		printf("FAIL =VALUE: %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=EXACT(\"Atomo\",\"atomo\")", cell(45, 8), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 8));
		doc.GetValue(cell(45, 8), v);
		Check(v.fType == eBoolData && (bool)v == false,
			"=EXACT(\"Atomo\",\"atomo\") calcola FALSO (distingue maiuscole/minuscole, a differenza di =)");
	}
	catch (CErr &e)
	{
		printf("FAIL =EXACT (diverso): %s\n", (char *)e);
		gFailures++;
	}

	try
	{
		TryToParseString("=EXACT(\"Atomo\",\"Atomo\")", cell(45, 9), &doc, true, '.', ',');
		doc.CalcCell(cell(45, 9));
		doc.GetValue(cell(45, 9), v);
		Check(v.fType == eBoolData && (bool)v == true, "=EXACT(\"Atomo\",\"Atomo\") calcola VERO");
	}
	catch (CErr &e)
	{
		printf("FAIL =EXACT (uguale): %s\n", (char *)e);
		gFailures++;
	}

	// Fase 15 (bug reale scoperto verificando questo stesso scenario
	// contro un file reale): a differenza del test sopra (tabella e
	// formula sullo STESSO foglio), un file XLSX vero ha spesso un
	// foglio "Indice"/riepilogo con XLOOKUP verso una Tabella Excel
	// definita su un foglio DATI separato -- CContainer::ResolveName
	// cercava la tabella solo in fTables locale, mai negli altri fogli
	// della cartella di lavoro, quindi restituiva sempre NaN. Qui
	// riprodotto con due CContainer distinti collegati da un
	// ISheetResolver minimale, come farebbe MainWindow nell'app vera.
	{
		class TableResolver : public ISheetResolver {
		public:
			CContainer *dataSheet;
			CContainer *ResolveSheetByName(const char *) { return NULL; }
			CContainer *FindSheetWithTable(const std::string &tableName)
			{
				return (tableName == "opere_elettriche") ? dataSheet : NULL;
			}
		};

		CContainer &summarySheet = *new CContainer(NULL, NULL);
		CContainer &dataSheet = *new CContainer(NULL, NULL);

		TableResolver resolver;
		resolver.dataSheet = &dataSheet;
		summarySheet.SetSheetResolver(&resolver);
		dataSheet.SetSheetResolver(&resolver);

		// opere_elettriche vive SOLO su dataSheet, mai su summarySheet.
		// NewCell diretto per i codici "P-EL-a"/"P-EL-b" (MAI
		// TryToParseString): sono tre nomi non definiti concatenati da
		// un "meno", un'espressione sintatticamente valida quanto una
		// vera formula (bug reale, vedi ui/src/AscdIO.cpp e
		// XlsxTranslator.cpp per lo stesso identico motivo) -- qui
		// servono come semplice testo letterale di prova.
		TryToParseString("Codice", cell(1, 1), &dataSheet, true);
		TryToParseString("Descrizione", cell(2, 1), &dataSheet, true);
		dataSheet.NewCell(cell(1, 2), Value("P-EL-a"), NULL);
		TryToParseString("Quadro elettrico", cell(2, 2), &dataSheet, true);
		dataSheet.NewCell(cell(1, 3), Value("P-EL-b"), NULL);
		TryToParseString("Interruttori sezionatori", cell(2, 3), &dataSheet, true);
		{
			CTableDef table;
			table.dataRange = range(1, 2, 2, 3); // A2:B3, intestazione esclusa
			table.columnNames.push_back("Codice");
			table.columnNames.push_back("Descrizione");
			dataSheet.AddTable("opere_elettriche", table);
		}

		// La chiave di ricerca (B33 nel file reale) e la formula vivono
		// sul foglio riepilogativo, MAI su dataSheet. NewCell diretto,
		// stesso motivo del commento sopra.
		summarySheet.NewCell(cell(2, 33), Value("P-EL-b"), NULL);

		try
		{
			TryToParseString("=+_xlfn.XLOOKUP(B33,opere_elettriche[Codice],opere_elettriche[Descrizione])",
				cell(3, 33), &summarySheet, true, '.', ',');
			summarySheet.CalcCell(cell(3, 33));
			Value crossValue;
			summarySheet.GetValue(cell(3, 33), crossValue);
			Check(crossValue.fType == eTextData
					&& strcmp((const char *)crossValue, "Interruttori sezionatori") == 0,
				"XLOOKUP su una tabella definita su un ALTRO foglio calcola \"Interruttori sezionatori\", "
				"non NaN (bug reale, tabella e formula su fogli diversi)");
		}
		catch (CErr &e)
		{
			printf("FAIL XLOOKUP su tabella fra fogli diversi: %s\n", (char *)e);
			gFailures++;
		}

		summarySheet.Release();
		dataSheet.Release();
	}

	// Bug reale scoperto analizzando file XLSX veri: una cella VUOTA
	// (mai scritta) confrontata con "=0"/"=\"\""/"=FALSO" doveva
	// risultare vera, come in Excel -- il pattern comunissimo
	// "=IF(cella=0;\"\";calcolo)" per nascondere una riga finche' non
	// e' compilata prendeva sempre il ramo sbagliato. Colonna 25,
	// scratch, mai usata prima in questo file.
	{
		// Colonna 25 riga 1: MAI scritta, resta eNoData davvero vuota
		// (a differenza di un DisposeCell su una cella gia' esistente,
		// che potrebbe lasciare altro stato).
		TryToParseString("=IF(Y1=0;\"vuota\";\"non vuota\")", cell(26, 1), &doc, true, '.', ';');
		doc.CalcCell(cell(26, 1));
		doc.GetValue(cell(26, 1), v);
		Check(strcmp((const char *)v, "vuota") == 0,
			"IF(cellaVuota=0;...) prende il ramo vero, come in Excel (bug reale, file XLSX veri)");

		TryToParseString("=IF(Y1=\"\";\"vuota\";\"non vuota\")", cell(26, 2), &doc, true, '.', ';');
		doc.CalcCell(cell(26, 2));
		doc.GetValue(cell(26, 2), v);
		Check(strcmp((const char *)v, "vuota") == 0,
			"IF(cellaVuota=\"\";...) prende il ramo vero, come in Excel");

		TryToParseString("=IF(Y1=FALSE;\"vuota\";\"non vuota\")", cell(26, 3), &doc, true, '.', ';');
		doc.CalcCell(cell(26, 3));
		doc.GetValue(cell(26, 3), v);
		Check(strcmp((const char *)v, "vuota") == 0,
			"IF(cellaVuota=FALSO;...) prende il ramo vero, come in Excel");

		// Y4 = 5 (non zero): il confronto con 0 deve restare falso.
		// Risolto un "mistero" di una sessione precedente qui (vedi
		// memoria project_cellref_equals_literal_mystery): NON era un
		// bug del motore, era il test stesso a scrivere per sbaglio in
		// "cell(26, 4)" (colonna 26 = Z) invece di "cell(25, 4)"
		// (colonna 25 = Y, la Y di "Y4" nel testo della formula) --
		// "Y4" restava quindi davvero vuota, e IF(Y4=0;...) prendeva
		// correttamente il ramo "vuota" (per lo stesso motivo dei test
		// sopra), solo che sembrava sbagliato perche' ci si aspettava
		// Y4=5. Isolato con cell::GetCell("Y4", ...) per conferma.
		cell y4;
		cell::GetCell("Y4", y4); // colonna 25, riga 4 -- MAI cell(26, 4)
		TryToParseString("5", y4, &doc, true);

		TryToParseString("=IF(Y4=0;\"vuota\";\"non vuota\")", cell(26, 4), &doc, true, '.', ';');
		doc.CalcCell(cell(26, 4));
		doc.GetValue(cell(26, 4), v);
		Check(strcmp((const char *)v, "non vuota") == 0,
			"IF(Y4=0;...) con Y4 davvero impostata a 5 (non vuota) prende il ramo falso, come atteso");

		// IF(condizione;valore_se_vero) a DUE argomenti (sintassi
		// Excel valida, il terzo argomento -- valore_se_falso -- e'
		// opzionale e sottintende FALSO): bug reale scoperto
		// analizzando file XLSX veri, 1.529 celle in un solo file
		// mostravano il testo grezzo della formula invece del
		// risultato, perche' funcs_by_nr.r fissava l'argCnt di IF
		// esattamente a 3 (nessuna sintassi a 2 argomenti passava
		// nemmeno l'analisi grammaticale). IFFunction (Functions.
		// spreadsheet.cpp) gestiva gia' correttamente argCnt==2 --
		// bastava sbloccare il parser (argCnt -1/65535, come XLOOKUP).
		TryToParseString("=IF(1<>1;\"vero\")", cell(26, 8), &doc, true, '.', ';');
		doc.CalcCell(cell(26, 8));
		doc.GetValue(cell(26, 8), v);
		Check(v.fType == eBoolData && (bool)v == false,
			"IF(condizione falsa;valore) a due argomenti, senza terzo, calcola FALSO (come in Excel), non un errore di sintassi");

		TryToParseString("=IF(1=1;\"vero\")", cell(26, 9), &doc, true, '.', ';');
		doc.CalcCell(cell(26, 9));
		doc.GetValue(cell(26, 9), v);
		Check(v.fType == eTextData && strcmp((const char *)v, "vero") == 0,
			"IF(condizione vera;valore) a due argomenti calcola ancora il valore_se_vero normalmente");

		// La forma a TRE argomenti (la piu' comune) resta invariata.
		TryToParseString("=IF(1=1;\"vero\";\"falso\")", cell(26, 12), &doc, true, '.', ';');
		doc.CalcCell(cell(26, 12));
		doc.GetValue(cell(26, 12), v);
		Check(strcmp((const char *)v, "vero") == 0,
			"IF a tre argomenti (la forma piu' comune) resta invariato dopo aver sbloccato il parser");

		// Due celle vuote diverse restano uguali fra loro.
		TryToParseString("=IF(Y1=Y6;\"uguali\";\"diverse\")", cell(26, 7), &doc, true, '.', ';');
		doc.CalcCell(cell(26, 7));
		doc.GetValue(cell(26, 7), v);
		Check(strcmp((const char *)v, "uguali") == 0,
			"due celle vuote diverse (Y1 e Y6) restano uguali fra loro (Value::operator== eNoData/eNoData)");
	}

	// Riferimenti a colonna intera ("A:A", "$A:$C", Fase 15): sintassi
	// Excel valida, prima di questo fix il parser falliva del tutto
	// (nessun grammatica accettava un identificatore senza numero di
	// riga seguito da ":") -- 795 celle interessate in un file XLSX
	// reale (vedi memoria project_xlsx_formula_gaps_20260808).
	// Contenitore isolato (come dataSheet/summarySheet sopra): un vero
	// SUM(A:A) su "doc" sommerebbe anche tutti i dati scritti da ogni
	// altro test in questo file nella stessa colonna A.
	{
		CContainer &wc = *new CContainer(NULL, NULL);

		TryToParseString("10", cell(1, 1), &wc, true); // A1
		TryToParseString("20", cell(1, 2), &wc, true); // A2
		TryToParseString("30", cell(1, 8000), &wc, true); // A8000, lontanissima: verifica che l'iteratore resti sparso

		TryToParseString("=SUM(A:A)", cell(5, 1), &wc, true);
		wc.CalcCell(cell(5, 1));
		wc.GetValue(cell(5, 1), v);
		Check(v.fType == eNumData && (double)v == 60.0,
			"SUM(A:A) somma tutta la colonna A, comprese righe sparse lontane (10+20+30=60)");

		char formula[256];
		wc.GetCellFormula(cell(5, 1), formula, sizeof(formula), false);
		Check(strstr(formula, "A:A") != NULL,
			"SUM(A:A) si ridisegna ancora come A:A, non come A1..A16384");

		// $A:$C assoluto, multi-colonna: B e C restano vuote, la somma
		// e' identica a prima.
		TryToParseString("=SUM($A:$C)", cell(5, 2), &wc, true);
		wc.CalcCell(cell(5, 2));
		wc.GetValue(cell(5, 2), v);
		Check(v.fType == eNumData && (double)v == 60.0,
			"SUM($A:$C) somma A (B e C sono vuote), gestisce anche la sintassi assoluta multi-colonna");

		char formula2[256];
		wc.GetCellFormula(cell(5, 2), formula2, sizeof(formula2), false);
		Check(strstr(formula2, "$A:$C") != NULL,
			"SUM($A:$C) si ridisegna ancora come $A:$C");

		// Un intervallo normale (con righe esplicite) non deve
		// diventare accidentalmente un riferimento a colonna intera.
		TryToParseString("=SUM(A1:A2)", cell(5, 3), &wc, true);
		wc.CalcCell(cell(5, 3));
		wc.GetValue(cell(5, 3), v);
		Check(v.fType == eNumData && (double)v == 30.0,
			"SUM(A1:A2) resta un intervallo normale (10+20=30, non tutta la colonna)");

		char formula3[256];
		wc.GetCellFormula(cell(5, 3), formula3, sizeof(formula3), false);
		Check(strcmp(formula3, "SUM(A1..A2)") == 0,
			"SUM(A1:A2) si ridisegna ancora come A1..A2, non come colonna intera");

		wc.Release();
	}

	// Argomenti-intervallo fra fogli diversi (Fase 16, es.
	// "SUM(Foglio!A1:A3)", "MATCH(x,Foglio!A:A,0)"): prima di questo
	// fix, CFormula::Calculate (caso valXRange) rinunciava del tutto
	// per un vero intervallo multi-cella fra fogli (eNoData), e le
	// funzioni di aggregazione (SUM/AVG/COUNT/MAX/MATCH/HINDEX/VINDEX/
	// NPV/IRR/...) leggevano comunque dal CContainer della formula
	// stessa invece che da quello risolto -- entrambi i lati del bug
	// dovevano essere corretti insieme (vedi memoria
	// project_xlsx_formula_gaps_20260808). E' proprio il pattern del
	// file reale che ha motivato la scoperta: "INDEX(Foglio!$D:$F,
	// MATCH(x,Foglio!$A:$A,0),1)" con la tabella di appoggio su un
	// foglio DATI separato da quello con la formula.
	{
		class RangeResolver : public ISheetResolver {
		public:
			CContainer *dataSheet;
			CContainer *ResolveSheetByName(const char *name)
			{
				return (strcmp(name, "Dati") == 0) ? dataSheet : NULL;
			}
			CContainer *FindSheetWithTable(const std::string &) { return NULL; }
		};

		CContainer &formulaSheet = *new CContainer(NULL, NULL);
		CContainer &dataSheet2 = *new CContainer(NULL, NULL);

		RangeResolver resolver;
		resolver.dataSheet = &dataSheet2;
		formulaSheet.SetSheetResolver(&resolver);
		dataSheet2.SetSheetResolver(&resolver);

		TryToParseString("100", cell(1, 1), &dataSheet2, true); // Dati!A1
		TryToParseString("200", cell(1, 2), &dataSheet2, true); // Dati!A2
		TryToParseString("300", cell(1, 3), &dataSheet2, true); // Dati!A3
		TryToParseString("uno", cell(4, 1), &dataSheet2, true); // Dati!D1
		TryToParseString("due", cell(4, 2), &dataSheet2, true); // Dati!D2
		TryToParseString("tre", cell(4, 3), &dataSheet2, true); // Dati!D3

		TryToParseString("=SUM(Dati!A1:A3)", cell(10, 1), &formulaSheet, true, '.', ',');
		formulaSheet.CalcCell(cell(10, 1));
		formulaSheet.GetValue(cell(10, 1), v);
		Check(v.fType == eNumData && (double)v == 600.0,
			"SUM(Dati!A1:A3) somma le celle vere sull'altro foglio (100+200+300), non 0");

		TryToParseString("=AVG(Dati!A1:A3)", cell(10, 2), &formulaSheet, true, '.', ',');
		formulaSheet.CalcCell(cell(10, 2));
		formulaSheet.GetValue(cell(10, 2), v);
		Check(v.fType == eNumData && (double)v == 200.0,
			"AVG(Dati!A1:A3) calcola la media vera (200), non 0");

		TryToParseString("=COUNT(Dati!A1:A3)", cell(10, 3), &formulaSheet, true, '.', ',');
		formulaSheet.CalcCell(cell(10, 3));
		formulaSheet.GetValue(cell(10, 3), v);
		Check(v.fType == eNumData && (double)v == 3.0,
			"COUNT(Dati!A1:A3) conta le tre celle vere, non 0");

		TryToParseString("=MAX(Dati!A1:A3)", cell(10, 4), &formulaSheet, true, '.', ',');
		formulaSheet.CalcCell(cell(10, 4));
		formulaSheet.GetValue(cell(10, 4), v);
		Check(v.fType == eNumData && (double)v == 300.0,
			"MAX(Dati!A1:A3) trova il massimo vero (300), non 0");

		TryToParseString("=MATCH(200,Dati!A1:A3,0)", cell(10, 5), &formulaSheet, true, '.', ',');
		formulaSheet.CalcCell(cell(10, 5));
		formulaSheet.GetValue(cell(10, 5), v);
		Check(v.fType == eNumData && (double)v == 2.0,
			"MATCH(200,Dati!A1:A3,0) trova la posizione vera (2), non NaN");

		TryToParseString("=INDEX(Dati!A1:A3,2)", cell(10, 6), &formulaSheet, true, '.', ',');
		formulaSheet.CalcCell(cell(10, 6));
		formulaSheet.GetValue(cell(10, 6), v);
		Check(v.fType == eNumData && (double)v == 200.0,
			"INDEX(Dati!A1:A3,2) legge la cella vera (200), non NaN");

		// Colonna intera fra fogli (Fase 15+16 insieme): stesso
		// meccanismo, con l'intervallo che copre tutte le righe.
		TryToParseString("=MATCH(200,Dati!$A:$A,0)", cell(10, 7), &formulaSheet, true, '.', ',');
		formulaSheet.CalcCell(cell(10, 7));
		formulaSheet.GetValue(cell(10, 7), v);
		Check(v.fType == eNumData && (double)v == 2.0,
			"MATCH(200,Dati!$A:$A,0) funziona anche con una colonna intera fra fogli");

		// Il pattern reale che ha motivato la scoperta: INDEX/MATCH
		// con la tabella di appoggio su un foglio dati separato,
		// entrambi gli intervalli a colonna intera.
		try
		{
			TryToParseString(
				"=IFERROR(INDEX(Dati!$D:$F,MATCH(200,Dati!$A:$A,0),1),\"errore\")",
				cell(10, 8), &formulaSheet, true, '.', ',');
			formulaSheet.CalcCell(cell(10, 8));
			formulaSheet.GetValue(cell(10, 8), v);
			Check(v.fType == eTextData && strcmp((const char *)v, "due") == 0,
				"INDEX(Dati!$D:$F,MATCH(200,Dati!$A:$A,0),1) (scenario reale) trova \"due\", "
				"non l'errore di fallback");
		}
		catch (CErr &e)
		{
			printf("FAIL INDEX/MATCH fra fogli con colonna intera: %s\n", (char *)e);
			gFailures++;
		}

		// Non regressione: lo stesso identico intervallo SULLO STESSO
		// foglio (nessun "Foglio!" davanti) deve continuare a
		// funzionare esattamente come prima.
		TryToParseString("=SUM(A1:A1)", cell(10, 9), &dataSheet2, true, '.', ',');
		dataSheet2.CalcCell(cell(10, 9));
		dataSheet2.GetValue(cell(10, 9), v);
		Check(v.fType == eNumData && (double)v == 100.0,
			"SUM(A1:A1) sullo stesso foglio non e' regredito (100)");

		formulaSheet.Release();
		dataSheet2.Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
