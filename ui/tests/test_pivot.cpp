/*
	test_pivot.cpp

	Verifica la logica della tabella pivot di base (Pivot.h/.cpp)
	senza sessione grafica, stesso principio di test_chart.cpp:
	costruisce un documento headless, inserisce dati a due colonne
	(categoria, valore) con categorie ripetute, e controlla che
	BuildPivotTable raggruppi/aggreghi correttamente e che
	WritePivotTable scriva il risultato atteso nel foglio. Copre anche
	il raggruppamento multi-livello (Fase 29: piu' di una colonna di
	categoria) e le aggregazioni Minimo/Massimo.
*/

#include <cstdio>

#include "Cell.h"
#include "Container.h"
#include "Range.h"
#include "Value.h"
#include "Pivot.h"

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
	CContainer& doc = *new CContainer(NULL, NULL);

	// Frutta/quantita', con "Mela" ripetuta due volte: il caso base
	// che una tabella pivot deve raggruppare.
	doc.NewCell(cell(1, 1), Value("Mela"), NULL);
	doc.NewCell(cell(2, 1), Value(10.0), NULL);
	doc.NewCell(cell(1, 2), Value("Pera"), NULL);
	doc.NewCell(cell(2, 2), Value(5.0), NULL);
	doc.NewCell(cell(1, 3), Value("Mela"), NULL);
	doc.NewCell(cell(2, 3), Value(20.0), NULL);

	std::vector<PivotRow> rows;
	range source(1, 1, 2, 3);
	bool ok = BuildPivotTable(&doc, source, rows);

	Check(ok, "BuildPivotTable riesce sui dati di prova");
	Check(rows.size() == 2, "due categorie distinte (Mela, Pera)");
	if (rows.size() == 2)
	{
		// Ordinato per categoria: Mela prima di Pera. Un solo livello
		// di raggruppamento: categories ha un solo elemento.
		Check(rows[0].categories.size() == 1 && rows[0].categories[0] == "Mela"
				&& rows[0].aggregate == 30.0 && rows[0].count == 2,
			"Mela: somma 30 (10+20), conteggio 2");
		Check(rows[1].categories.size() == 1 && rows[1].categories[0] == "Pera"
				&& rows[1].aggregate == 5.0 && rows[1].count == 1,
			"Pera: somma 5, conteggio 1");
		Check(rows[0].minVal == 10.0 && rows[0].maxVal == 20.0,
			"Mela: minimo 10, massimo 20");
	}

	// Scrive il risultato nel foglio (Somma) e verifica le celle.
	cell dest(4, 1);
	WritePivotTable(&doc, dest, rows, ePivotSum);

	Value header;
	doc.GetValue(cell(4, 1), header);
	Check(BString((const char*)header) == "Categoria", "intestazione categoria scritta in D1");

	Value melaCat, melaVal;
	doc.GetValue(cell(4, 2), melaCat);
	doc.GetValue(cell(5, 2), melaVal);
	Check(BString((const char*)melaCat) == "Mela" && (double)melaVal == 30.0,
		"riga Mela scritta correttamente (D2/E2)");

	Value peraCat, peraVal;
	doc.GetValue(cell(4, 3), peraCat);
	doc.GetValue(cell(5, 3), peraVal);
	Check(BString((const char*)peraCat) == "Pera" && (double)peraVal == 5.0,
		"riga Pera scritta correttamente (D3/E3)");

	// Stessa aggregazione ricalcolata come Media invece di Somma,
	// scritta altrove per non sovrascrivere il controllo precedente.
	WritePivotTable(&doc, cell(7, 1), rows, ePivotAverage);
	Value melaAvg;
	doc.GetValue(cell(8, 2), melaAvg);
	Check((double)melaAvg == 15.0, "Media di Mela (10 e 20) e' 15");

	// Minimo/Massimo (Fase 29): stessi dati di Mela (10, 20).
	WritePivotTable(&doc, cell(10, 1), rows, ePivotMin);
	Value melaMin;
	doc.GetValue(cell(11, 2), melaMin);
	Check((double)melaMin == 10.0, "Minimo di Mela (10 e 20) e' 10");

	WritePivotTable(&doc, cell(13, 1), rows, ePivotMax);
	Value melaMax;
	doc.GetValue(cell(14, 2), melaMax);
	Check((double)melaMax == 20.0, "Massimo di Mela (10 e 20) e' 20");

	// Intervallo con una sola colonna: non e' il formato atteso
	// (serve almeno categoria + valore), deve fallire esplicitamente.
	std::vector<PivotRow> badShape;
	range oneColumn(1, 1, 1, 3);
	Check(!BuildPivotTable(&doc, oneColumn, badShape),
		"un intervallo con una sola colonna viene rifiutato");

	// Raggruppamento multi-livello (Fase 29): Regione, Prodotto,
	// Vendite -- due colonne di categoria invece di una sola. Stessa
	// combinazione (Nord, Mele) ripetuta due volte, per verificare che
	// il raggruppamento avvenga sulla COPPIA di categorie, non solo
	// sulla prima.
	doc.NewCell(cell(1, 10), Value("Nord"), NULL);
	doc.NewCell(cell(2, 10), Value("Mele"), NULL);
	doc.NewCell(cell(3, 10), Value(100.0), NULL);
	doc.NewCell(cell(1, 11), Value("Nord"), NULL);
	doc.NewCell(cell(2, 11), Value("Pere"), NULL);
	doc.NewCell(cell(3, 11), Value(50.0), NULL);
	doc.NewCell(cell(1, 12), Value("Sud"), NULL);
	doc.NewCell(cell(2, 12), Value("Mele"), NULL);
	doc.NewCell(cell(3, 12), Value(30.0), NULL);
	doc.NewCell(cell(1, 13), Value("Nord"), NULL);
	doc.NewCell(cell(2, 13), Value("Mele"), NULL);
	doc.NewCell(cell(3, 13), Value(20.0), NULL);

	std::vector<PivotRow> multiRows;
	range multiSource(1, 10, 3, 13);
	bool multiOk = BuildPivotTable(&doc, multiSource, multiRows);
	Check(multiOk, "BuildPivotTable riesce su un intervallo a tre colonne (due livelli)");
	Check(multiRows.size() == 3,
		"tre combinazioni distinte (Nord/Mele, Nord/Pere, Sud/Mele)");
	if (multiRows.size() == 3)
	{
		// Ordine lessicografico sul vettore: Nord/Mele, Nord/Pere,
		// Sud/Mele (Nord < Sud, poi Mele < Pere a parita' del primo
		// livello).
		Check(multiRows[0].categories.size() == 2
				&& multiRows[0].categories[0] == "Nord"
				&& multiRows[0].categories[1] == "Mele"
				&& multiRows[0].aggregate == 120.0 && multiRows[0].count == 2,
			"Nord/Mele: somma 120 (100+20), conteggio 2 -- raggruppato sulla COPPIA");
		Check(multiRows[1].categories[0] == "Nord" && multiRows[1].categories[1] == "Pere"
				&& multiRows[1].aggregate == 50.0,
			"Nord/Pere: somma 50");
		Check(multiRows[2].categories[0] == "Sud" && multiRows[2].categories[1] == "Mele"
				&& multiRows[2].aggregate == 30.0,
			"Sud/Mele: somma 30 (non confuso con Nord/Mele)");
	}

	// Scrittura su due livelli: due colonne di intestazione ("Categoria
	// 1"/"Categoria 2", non solo "Categoria" -- distingue il caso a un
	// livello, che deve restare invariato) piu' una di aggregazione.
	WritePivotTable(&doc, cell(6, 10), multiRows, ePivotSum);
	Value multiHeader1, multiHeader2, multiHeaderAgg;
	doc.GetValue(cell(6, 10), multiHeader1);
	doc.GetValue(cell(7, 10), multiHeader2);
	doc.GetValue(cell(8, 10), multiHeaderAgg);
	Check(BString((const char*)multiHeader1) == "Categoria 1"
			&& BString((const char*)multiHeader2) == "Categoria 2",
		"intestazioni a due livelli numerate (Categoria 1/Categoria 2), non solo \"Categoria\"");
	Check(BString((const char*)multiHeaderAgg) == "Somma", "intestazione aggregazione invariata");

	Value multiCat1, multiCat2, multiVal;
	doc.GetValue(cell(6, 11), multiCat1);
	doc.GetValue(cell(7, 11), multiCat2);
	doc.GetValue(cell(8, 11), multiVal);
	Check(BString((const char*)multiCat1) == "Nord" && BString((const char*)multiCat2) == "Mele"
			&& (double)multiVal == 120.0,
		"prima riga scritta su tre colonne (Nord, Mele, 120)");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
