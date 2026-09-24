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

	// --- CContainer::AddPivotTable/GetPivotTables/ClearPivotTables
	// (persistenza): verifica che il container conservi l'oggetto
	// cosi' com'e', senza nessuna logica di ricalcolo qui -- quella e'
	// gia' provata sopra da BuildPivotTable/WritePivotTable. Vedi
	// PivotTableObject in Container.h. ---
	{
		Check(doc.GetPivotTables().empty(),
			"un documento appena creato non ha nessuna tabella pivot persistita");

		PivotTableObject pivot;
		pivot.sourceRange = range(1, 1, 2, 3);
		pivot.destAnchor = cell(4, 1);
		pivot.aggFunc = ePivotSum;
		pivot.cachedRows = rows; // dal blocco Mela/Pera in cima
		doc.AddPivotTable(pivot);

		Check(doc.GetPivotTables().size() == 1, "AddPivotTable aggiunge la tabella");
		if (doc.GetPivotTables().size() == 1)
		{
			const PivotTableObject& stored = doc.GetPivotTables()[0];
			Check(stored.sourceRange.left == 1 && stored.sourceRange.right == 2,
				"sourceRange conservato");
			Check(stored.destAnchor.h == 4 && stored.destAnchor.v == 1,
				"destAnchor conservato");
			Check(stored.aggFunc == ePivotSum, "aggFunc conservato");
			Check(stored.cachedRows.size() == rows.size(), "cachedRows conservato");
		}

		doc.ClearPivotTables();
		Check(doc.GetPivotTables().empty(), "ClearPivotTables svuota davvero");
	}

	// --- Pivot 2D (campo Colonne + misure multiple): BuildPivotTable2D/
	// WritePivotTable2D/PivotTable2DDestRange -- BuildPivotTable/
	// WritePivotTable sopra restano invariate, questo blocco copia solo
	// nuove colonne/righe per non interferire con nessun controllo gia'
	// fatto. ---

	// Caso base: 1 colonna chiave di riga + campo Colonne, 1 misura.
	// "Nord/Q1" ripetuto due volte per esercitare davvero l'aggregazione.
	{
		doc.NewCell(cell(20, 20), Value("Nord"), NULL);
		doc.NewCell(cell(21, 20), Value("Q1"), NULL);
		doc.NewCell(cell(22, 20), Value(100.0), NULL);
		doc.NewCell(cell(20, 21), Value("Nord"), NULL);
		doc.NewCell(cell(21, 21), Value("Q2"), NULL);
		doc.NewCell(cell(22, 21), Value(200.0), NULL);
		doc.NewCell(cell(20, 22), Value("Sud"), NULL);
		doc.NewCell(cell(21, 22), Value("Q1"), NULL);
		doc.NewCell(cell(22, 22), Value(50.0), NULL);
		doc.NewCell(cell(20, 23), Value("Sud"), NULL);
		doc.NewCell(cell(21, 23), Value("Q2"), NULL);
		doc.NewCell(cell(22, 23), Value(80.0), NULL);
		doc.NewCell(cell(20, 24), Value("Nord"), NULL);
		doc.NewCell(cell(21, 24), Value("Q1"), NULL);
		doc.NewCell(cell(22, 24), Value(10.0), NULL);

		std::vector<PivotMeasure> measuresA(1);
		measuresA[0].sourceCol = 22;
		measuresA[0].aggFunc = ePivotSum;

		std::vector<BString> colValsA;
		std::vector<PivotRow2D> rowsA;
		range sourceA(20, 20, 22, 24);
		bool okA = BuildPivotTable2D(&doc, sourceA, 21, measuresA, &colValsA, &rowsA);

		Check(okA, "BuildPivotTable2D riesce (1 colonna chiave di riga + campo Colonne, 1 misura)");
		Check(colValsA.size() == 2 && colValsA[0] == "Q1" && colValsA[1] == "Q2",
			"colValsA sono Q1/Q2, ordine lessicografico");
		Check(rowsA.size() == 2, "due gruppi di riga (Nord, Sud)");
		if (rowsA.size() == 2 && colValsA.size() == 2)
		{
			Check(rowsA[0].categories[0] == "Nord", "il primo gruppo e' Nord");
			Check(rowsA[0].cells[0][0].aggregate == 110.0 && rowsA[0].cells[0][0].count == 2
					&& rowsA[0].cells[0][0].minVal == 10.0 && rowsA[0].cells[0][0].maxVal == 100.0,
				"Nord/Q1: somma 110 (100+10), conteggio 2 -- aggregazione vera, non solo passaggio");
			Check(rowsA[0].cells[1][0].aggregate == 200.0 && rowsA[0].cells[1][0].count == 1,
				"Nord/Q2: somma 200, conteggio 1");
			Check(rowsA[1].categories[0] == "Sud", "il secondo gruppo e' Sud");
			Check(rowsA[1].cells[0][0].aggregate == 50.0 && rowsA[1].cells[1][0].aggregate == 80.0,
				"Sud/Q1=50, Sud/Q2=80");
		}

		cell destA(25, 20);
		WritePivotTable2D(&doc, destA, colValsA, measuresA, rowsA);

		Value h1q1, h1q2;
		doc.GetValue(cell(26, 20), h1q1);
		doc.GetValue(cell(27, 20), h1q2);
		Check(BString((const char*)h1q1) == "Q1" && BString((const char*)h1q2) == "Q2",
			"riga di intestazione 1: valore del campo Colonne (Q1/Q2)");

		Value h2cat, h2m0, h2m1;
		doc.GetValue(cell(25, 21), h2cat);
		doc.GetValue(cell(26, 21), h2m0);
		doc.GetValue(cell(27, 21), h2m1);
		Check(BString((const char*)h2cat) == "Categoria" && BString((const char*)h2m0) == "Somma"
				&& BString((const char*)h2m1) == "Somma",
			"riga di intestazione 2: Categoria + etichetta di misura (Somma, nessuna label esplicita)");

		Value nordCat, nordQ1, nordQ2, sudCat, sudQ1, sudQ2;
		doc.GetValue(cell(25, 22), nordCat);
		doc.GetValue(cell(26, 22), nordQ1);
		doc.GetValue(cell(27, 22), nordQ2);
		doc.GetValue(cell(25, 23), sudCat);
		doc.GetValue(cell(26, 23), sudQ1);
		doc.GetValue(cell(27, 23), sudQ2);
		Check(BString((const char*)nordCat) == "Nord" && (double)nordQ1 == 110.0 && (double)nordQ2 == 200.0,
			"riga Nord scritta correttamente (110/200)");
		Check(BString((const char*)sudCat) == "Sud" && (double)sudQ1 == 50.0 && (double)sudQ2 == 80.0,
			"riga Sud scritta correttamente (50/80)");

		range destRangeA = PivotTable2DDestRange(destA, 1, colValsA, measuresA, rowsA.size());
		Check(destRangeA.left == 25 && destRangeA.top == 20 && destRangeA.right == 27
				&& destRangeA.bottom == 23,
			"PivotTable2DDestRange combacia con l'estensione vera scritta (F20:H23 -> col 25-27, righe 20-23)");
	}

	// 2+ misure, NESSUN campo Colonne: la riga di intestazione 1 resta
	// vuota (nessun valore di campo Colonne da mostrare), la riga 2
	// elenca le due etichette di misura esplicite.
	{
		doc.NewCell(cell(30, 30), Value("Mela"), NULL);
		doc.NewCell(cell(31, 30), Value(10.0), NULL);
		doc.NewCell(cell(32, 30), Value(1.0), NULL);
		doc.NewCell(cell(30, 31), Value("Pera"), NULL);
		doc.NewCell(cell(31, 31), Value(5.0), NULL);
		doc.NewCell(cell(32, 31), Value(2.0), NULL);
		doc.NewCell(cell(30, 32), Value("Mela"), NULL);
		doc.NewCell(cell(31, 32), Value(20.0), NULL);
		doc.NewCell(cell(32, 32), Value(3.0), NULL);

		std::vector<PivotMeasure> measuresB(2);
		measuresB[0].sourceCol = 31; measuresB[0].aggFunc = ePivotSum; measuresB[0].label = "A";
		measuresB[1].sourceCol = 32; measuresB[1].aggFunc = ePivotSum; measuresB[1].label = "B";

		std::vector<BString> colValsB;
		std::vector<PivotRow2D> rowsB;
		range sourceB(30, 30, 32, 32);
		bool okB = BuildPivotTable2D(&doc, sourceB, -1, measuresB, &colValsB, &rowsB);

		Check(okB, "BuildPivotTable2D riesce (nessun campo Colonne, 2 misure)");
		Check(colValsB.empty(), "colValsB e' vuoto -- nessun campo Colonne");
		Check(rowsB.size() == 2, "due gruppi di riga (Mela, Pera)");
		if (rowsB.size() == 2)
		{
			Check(rowsB[0].categories[0] == "Mela" && rowsB[0].cells.size() == 1,
				"Mela: un solo blocco di colonna (nessun campo Colonne)");
			Check(rowsB[0].cells[0][0].aggregate == 30.0 && rowsB[0].cells[0][0].count == 2
					&& rowsB[0].cells[0][1].aggregate == 4.0 && rowsB[0].cells[0][1].count == 2,
				"Mela: misura A=30 (10+20), misura B=4 (1+3)");
			Check(rowsB[1].categories[0] == "Pera"
					&& rowsB[1].cells[0][0].aggregate == 5.0 && rowsB[1].cells[0][1].aggregate == 2.0,
				"Pera: misura A=5, misura B=2");
		}

		cell destB(34, 30);
		WritePivotTable2D(&doc, destB, colValsB, measuresB, rowsB);

		Value h1blankA, h1blankB;
		doc.GetValue(cell(35, 30), h1blankA);
		doc.GetValue(cell(36, 30), h1blankB);
		Check(h1blankA.fType == eNoData && h1blankB.fType == eNoData,
			"riga di intestazione 1 resta vuota quando non c'e' campo Colonne");

		Value h2catB, h2labelA, h2labelB;
		doc.GetValue(cell(34, 31), h2catB);
		doc.GetValue(cell(35, 31), h2labelA);
		doc.GetValue(cell(36, 31), h2labelB);
		Check(BString((const char*)h2catB) == "Categoria" && BString((const char*)h2labelA) == "A"
				&& BString((const char*)h2labelB) == "B",
			"riga di intestazione 2: Categoria + le due etichette esplicite (A, B)");

		Value melaCatB, melaA, melaB;
		doc.GetValue(cell(34, 32), melaCatB);
		doc.GetValue(cell(35, 32), melaA);
		doc.GetValue(cell(36, 32), melaB);
		Check(BString((const char*)melaCatB) == "Mela" && (double)melaA == 30.0 && (double)melaB == 4.0,
			"riga Mela scritta correttamente sulle due misure affiancate");
	}

	// Campo Colonne E 2+ misure insieme: intestazione a due righe
	// completa, PivotTable2DDestRange combacia con l'estensione vera.
	{
		doc.NewCell(cell(40, 40), Value("Nord"), NULL);
		doc.NewCell(cell(41, 40), Value("Q1"), NULL);
		doc.NewCell(cell(42, 40), Value(100.0), NULL);
		doc.NewCell(cell(43, 40), Value(10.0), NULL);
		doc.NewCell(cell(40, 41), Value("Nord"), NULL);
		doc.NewCell(cell(41, 41), Value("Q2"), NULL);
		doc.NewCell(cell(42, 41), Value(200.0), NULL);
		doc.NewCell(cell(43, 41), Value(20.0), NULL);
		doc.NewCell(cell(40, 42), Value("Sud"), NULL);
		doc.NewCell(cell(41, 42), Value("Q1"), NULL);
		doc.NewCell(cell(42, 42), Value(50.0), NULL);
		doc.NewCell(cell(43, 42), Value(5.0), NULL);
		doc.NewCell(cell(40, 43), Value("Sud"), NULL);
		doc.NewCell(cell(41, 43), Value("Q2"), NULL);
		doc.NewCell(cell(42, 43), Value(80.0), NULL);
		doc.NewCell(cell(43, 43), Value(8.0), NULL);

		std::vector<PivotMeasure> measuresC(2);
		measuresC[0].sourceCol = 42; measuresC[0].aggFunc = ePivotSum; measuresC[0].label = "Sales";
		measuresC[1].sourceCol = 43; measuresC[1].aggFunc = ePivotSum; measuresC[1].label = "Cost";

		std::vector<BString> colValsC;
		std::vector<PivotRow2D> rowsC;
		range sourceC(40, 40, 43, 43);
		bool okC = BuildPivotTable2D(&doc, sourceC, 41, measuresC, &colValsC, &rowsC);
		Check(okC, "BuildPivotTable2D riesce (campo Colonne + 2 misure insieme)");
		Check(colValsC.size() == 2 && rowsC.size() == 2,
			"due valori di campo Colonne (Q1/Q2) e due gruppi di riga (Nord/Sud)");

		cell destC(45, 40);
		WritePivotTable2D(&doc, destC, colValsC, measuresC, rowsC);

		Value h1q1a, h1q1b, h1q2a, h1q2b;
		doc.GetValue(cell(46, 40), h1q1a); doc.GetValue(cell(47, 40), h1q1b);
		doc.GetValue(cell(48, 40), h1q2a); doc.GetValue(cell(49, 40), h1q2b);
		Check(BString((const char*)h1q1a) == "Q1" && BString((const char*)h1q1b) == "Q1"
				&& BString((const char*)h1q2a) == "Q2" && BString((const char*)h1q2b) == "Q2",
			"riga di intestazione 1: Q1 ripetuto sulle sue 2 colonne misura, poi Q2");

		Value h2salesQ1, h2costQ1, h2salesQ2, h2costQ2;
		doc.GetValue(cell(46, 41), h2salesQ1); doc.GetValue(cell(47, 41), h2costQ1);
		doc.GetValue(cell(48, 41), h2salesQ2); doc.GetValue(cell(49, 41), h2costQ2);
		Check(BString((const char*)h2salesQ1) == "Sales" && BString((const char*)h2costQ1) == "Cost"
				&& BString((const char*)h2salesQ2) == "Sales" && BString((const char*)h2costQ2) == "Cost",
			"riga di intestazione 2: Sales/Cost ripetute per ogni valore di campo Colonne");

		Value nordSalesQ1, nordCostQ1, nordSalesQ2, nordCostQ2;
		doc.GetValue(cell(46, 42), nordSalesQ1); doc.GetValue(cell(47, 42), nordCostQ1);
		doc.GetValue(cell(48, 42), nordSalesQ2); doc.GetValue(cell(49, 42), nordCostQ2);
		Check((double)nordSalesQ1 == 100.0 && (double)nordCostQ1 == 10.0
				&& (double)nordSalesQ2 == 200.0 && (double)nordCostQ2 == 20.0,
			"riga Nord scritta correttamente su tutte e 4 le colonne dati");

		range destRangeC = PivotTable2DDestRange(destC, 1, colValsC, measuresC, rowsC.size());
		Check(destRangeC.left == 45 && destRangeC.top == 40 && destRangeC.right == 49
				&& destRangeC.bottom == 43,
			"PivotTable2DDestRange combacia con l'estensione vera scritta (5 colonne x 4 righe)");
	}

	// Validita' per-misura: un valore non numerico in UNA misura esclude
	// SOLO quella misura per quella riga sorgente, non l'intera riga (a
	// differenza di BuildPivotTable, che scarterebbe l'intera riga).
	{
		doc.NewCell(cell(50, 50), Value("X"), NULL);
		doc.NewCell(cell(51, 50), Value(10.0), NULL);
		doc.NewCell(cell(52, 50), Value("non numerico"), NULL); // misura B non valida SOLO qui
		doc.NewCell(cell(50, 51), Value("X"), NULL);
		doc.NewCell(cell(51, 51), Value(20.0), NULL);
		doc.NewCell(cell(52, 51), Value(5.0), NULL);

		std::vector<PivotMeasure> measuresD(2);
		measuresD[0].sourceCol = 51; measuresD[0].aggFunc = ePivotSum;
		measuresD[1].sourceCol = 52; measuresD[1].aggFunc = ePivotSum;

		std::vector<BString> colValsD;
		std::vector<PivotRow2D> rowsD;
		range sourceD(50, 50, 52, 51);
		bool okD = BuildPivotTable2D(&doc, sourceD, -1, measuresD, &colValsD, &rowsD);
		Check(okD, "BuildPivotTable2D riesce anche con una misura parzialmente non valida");
		Check(rowsD.size() == 1, "un solo gruppo (X), la riga con misura B non numerica non e' scartata del tutto");
		if (rowsD.size() == 1)
		{
			Check(rowsD[0].cells[0][0].aggregate == 30.0 && rowsD[0].cells[0][0].count == 2,
				"misura A: entrambe le righe valide, somma 30 (10+20)");
			Check(rowsD[0].cells[0][1].aggregate == 5.0 && rowsD[0].cells[0][1].count == 1,
				"misura B: solo la seconda riga valida, somma 5 (la prima, testo, e' esclusa SOLO per B)");
		}
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
