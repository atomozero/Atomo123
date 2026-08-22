/*
	test_autofill.cpp

	Verifica GenerateAutoFillSequence (AutoFill.h/.cpp) senza sessione
	grafica, stesso principio di test_pivot.cpp/test_chart.cpp: nessuna
	SheetView/CContainer, solo vettori di Value costruiti a mano.
*/

#include <cstdio>
#include <cmath>

#include <String.h>

#include "Value.h"
#include "AutoFill.h"

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
	// Il caso letterale dell'utente: 1, 2, 3 selezionati, si prosegue
	// con 4, 5, 6.
	{
		std::vector<Value> source;
		source.push_back(Value(1.0));
		source.push_back(Value(2.0));
		source.push_back(Value(3.0));
		std::vector<Value> next = GenerateAutoFillSequence(source, 3);
		Check(next.size() == 3, "1,2,3 -> genera esattamente 3 nuovi valori");
		if (next.size() == 3)
		{
			Check(next[0].fType == eNumData && (double)next[0] == 4.0, "1,2,3 -> il primo nuovo valore e' 4");
			Check((double)next[1] == 5.0, "1,2,3 -> il secondo nuovo valore e' 5");
			Check((double)next[2] == 6.0, "1,2,3 -> il terzo nuovo valore e' 6");
		}
	}

	// Passo diverso da 1 (progressione aritmetica generica: 2,4,6 ->
	// passo 2, non solo "sempre +1").
	{
		std::vector<Value> source;
		source.push_back(Value(2.0));
		source.push_back(Value(4.0));
		source.push_back(Value(6.0));
		std::vector<Value> next = GenerateAutoFillSequence(source, 2);
		Check(next.size() == 2 && (double)next[0] == 8.0 && (double)next[1] == 10.0,
			"2,4,6 (passo 2) -> prosegue con 8, 10, non con +1");
	}

	// Passo negativo (progressione decrescente): 10, 8, 6 -> 4, 2.
	{
		std::vector<Value> source;
		source.push_back(Value(10.0));
		source.push_back(Value(8.0));
		source.push_back(Value(6.0));
		std::vector<Value> next = GenerateAutoFillSequence(source, 2);
		Check(next.size() == 2 && (double)next[0] == 4.0 && (double)next[1] == 2.0,
			"10,8,6 (passo -2) -> prosegue con 4, 2, la progressione puo' anche decrescere");
	}

	// Passo frazionario, dove l'arrotondamento in virgola mobile
	// potrebbe far sembrare il passo "non costante" con un confronto
	// esatto: 0.1, 0.2, 0.3 -> 0.4, 0.5.
	{
		std::vector<Value> source;
		source.push_back(Value(0.1));
		source.push_back(Value(0.2));
		source.push_back(Value(0.3));
		std::vector<Value> next = GenerateAutoFillSequence(source, 2);
		Check(next.size() == 2 && std::fabs((double)next[0] - 0.4) < 1e-9
				&& std::fabs((double)next[1] - 0.5) < 1e-9,
			"0.1,0.2,0.3 (passo frazionario) -> prosegue con 0.4, 0.5 nonostante l'arrotondamento");
	}

	// Una sola cella sorgente: nessuna progressione possibile da una
	// lunghezza 1, si ripete lo stesso valore -- comportamento
	// invariato rispetto a Riempi a destra/in basso.
	{
		std::vector<Value> source;
		source.push_back(Value(42.0));
		std::vector<Value> next = GenerateAutoFillSequence(source, 3);
		Check(next.size() == 3 && (double)next[0] == 42.0 && (double)next[1] == 42.0
				&& (double)next[2] == 42.0,
			"una sola cella (42) -> ripete 42 tre volte, non inventa una progressione");
	}

	// Progressione di date: passo di un giorno (86400 secondi).
	{
		std::vector<Value> source;
		time_t day0 = 1000000000; // un istante arbitrario, non serve una data reale
		source.push_back(Value((time_t)(day0)));
		source.push_back(Value((time_t)(day0 + 86400)));
		source.push_back(Value((time_t)(day0 + 2 * 86400)));
		std::vector<Value> next = GenerateAutoFillSequence(source, 2);
		Check(next.size() == 2 && next[0].fType == eTimeData
				&& (time_t)next[0] == day0 + 3 * 86400
				&& (time_t)next[1] == day0 + 4 * 86400,
			"tre date consecutive (passo di un giorno) -> prosegue con le due date successive");
	}

	// Progressione di date con passo di una settimana: il passo
	// costante viene riconosciuto qualunque esso sia, non solo un
	// giorno.
	{
		std::vector<Value> source;
		time_t day0 = 1000000000;
		source.push_back(Value((time_t)(day0)));
		source.push_back(Value((time_t)(day0 + 7 * 86400)));
		std::vector<Value> next = GenerateAutoFillSequence(source, 1);
		Check(next.size() == 1 && (time_t)next[0] == day0 + 14 * 86400,
			"due date a distanza di una settimana -> prosegue con lo stesso passo (14 giorni dopo la prima)");
	}

	// Testo non in progressione (due celle, nessun pattern numerico):
	// si ripetono ciclicamente i valori sorgente, come farebbe Excel.
	{
		std::vector<Value> source;
		source.push_back(Value("Lunedi"));
		source.push_back(Value("Martedi"));
		std::vector<Value> next = GenerateAutoFillSequence(source, 4);
		Check(next.size() == 4, "due celle di testo -> genera comunque 4 nuovi valori");
		if (next.size() == 4)
		{
			BString a((const char*)next[0]), b((const char*)next[1]),
				c((const char*)next[2]), d((const char*)next[3]);
			Check(a == "Lunedi" && b == "Martedi" && c == "Lunedi" && d == "Martedi",
				"testo senza progressione numerica -> ripete ciclicamente Lunedi/Martedi/Lunedi/Martedi");
		}
	}

	// Numeri senza un passo costante (1, 2, 4 -- non e' una
	// progressione aritmetica): nessuna progressione riconosciuta,
	// ripete ciclicamente come per il testo sopra.
	{
		std::vector<Value> source;
		source.push_back(Value(1.0));
		source.push_back(Value(2.0));
		source.push_back(Value(4.0));
		std::vector<Value> next = GenerateAutoFillSequence(source, 3);
		Check(next.size() == 3 && (double)next[0] == 1.0 && (double)next[1] == 2.0
				&& (double)next[2] == 4.0,
			"1,2,4 (nessun passo costante) -> ripete ciclicamente 1,2,4, non inventa una progressione");
	}

	// Tipi misti (numero + testo nella stessa selezione): nessuna
	// progressione possibile, ripete ciclicamente.
	{
		std::vector<Value> source;
		source.push_back(Value(1.0));
		source.push_back(Value("Ciao"));
		std::vector<Value> next = GenerateAutoFillSequence(source, 2);
		Check(next.size() == 2 && next[0].fType == eNumData && (double)next[0] == 1.0
				&& next[1].fType == eTextData,
			"tipi misti (numero + testo) -> ripete ciclicamente, nessuna progressione forzata");
	}

	// count <= 0: nessun valore generato, non un crash.
	{
		std::vector<Value> source;
		source.push_back(Value(1.0));
		std::vector<Value> next = GenerateAutoFillSequence(source, 0);
		Check(next.empty(), "count 0 -> nessun valore generato");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
