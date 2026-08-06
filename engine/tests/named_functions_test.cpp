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

	Check(gFuncCount == 101, "InitFunctions carica tutte le 101 funzioni della risorsa 'Func'");

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

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
