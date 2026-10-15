/*
	vba_object_model_test.cpp

	Verifica il modello a oggetti Excel minimo (Fase 3c dell'interprete
	VBA -- vedi la decisione del sottoinsieme in ROADMAP.md):
	Range("A1")/Range("A1:B2")/Cells(riga,colonna), ".Value" (lettura
	e scrittura, anche implicita senza ".Value" -- il "default member"
	di VBA), ".Offset(righe,colonne)", ".Select" (no-op deliberato a
	questo livello, nessuna UI collegata), e "For Each ... In" su un
	intervallo (Fase 5b, vedi ROADMAP.md -- ordine riga per riga).
	Scrive/legge celle VERE su un CContainer costruito direttamente,
	stesso schema di newcell_test.cpp.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Container.h"
#include "VbaExecutor.h"
#include "VbaStmtParser.h"
#include "VbaToken.h"

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

// Come vba_executor_test.cpp, ma con un CContainer collegato
// all'ambiente PRIMA di eseguire -- "doc" puo' essere NULL per i test
// che verificano l'errore "nessun foglio di calcolo collegato".
static bool RunSub(const char* source, CContainer* doc, VbaEnvironment* env, std::string* outError)
{
	env->SetActiveDocument(doc);

	std::vector<VbaToken> tokens;
	TokenizeVba(source, &tokens);
	size_t pos = 0;
	VbaParseError parseError;
	std::unique_ptr<VbaSub> sub = ParseVbaSub(tokens, pos, &parseError);
	if (!sub)
	{
		*outError = "<errore sintassi: " + parseError.message + ">";
		return false;
	}

	VbaRuntimeError runtimeError;
	if (!ExecuteVbaSub(sub.get(), env, &runtimeError))
	{
		*outError = "<errore: " + runtimeError.message + ">";
		return false;
	}
	return true;
}

static std::string FormatValue(const Value& v)
{
	switch (v.fType)
	{
		case eNumData:
		{
			double d = (double)v;
			char buf[64];
			if (d == (long long)d)
				snprintf(buf, sizeof(buf), "%lld", (long long)d);
			else
				snprintf(buf, sizeof(buf), "%g", d);
			return buf;
		}
		case eTextData:
			return std::string("\"") + (const char*)v + "\"";
		case eBoolData:
			return (bool)v ? "True" : "False";
		default:
			return "<tipo inatteso>";
	}
}

// Esegue "source" su un "doc" nuovo (vuoto, salvo "presetCell" se non
// NULL -- scritta PRIMA di eseguire, per i test di lettura) e
// verifica il valore finale della variabile "varName".
static void CheckVar(const char* source, const cell* presetCell, const char* presetValue,
	const char* varName, const char* expected)
{
	CContainer* doc = new CContainer(NULL, NULL);
	if (presetCell)
		doc->NewCell(*presetCell, Value(atof(presetValue)), NULL); // presetValue e' un numero scritto come testo

	VbaEnvironment env;
	std::string error;
	std::string what = std::string(source) + " -> " + varName;
	if (!RunSub(source, doc, &env, &error))
	{
		Check(false, (what + " (esecuzione fallita: " + error + ")").c_str());
		doc->Release();
		return;
	}
	const Value* v = env.Find(varName);
	if (!v)
	{
		Check(false, (what + " (variabile non trovata)").c_str());
		doc->Release();
		return;
	}
	std::string actual = FormatValue(*v);
	bool ok = actual == expected;
	what += " = " + actual;
	if (!ok)
		what += " (atteso: " + std::string(expected) + ")";
	Check(ok, what.c_str());
	doc->Release();
}

// Esegue "source" su un "doc" nuovo e verifica il valore scritto
// davvero in "checkCell" -- per i test di SCRITTURA (".Value = x" o
// la forma implicita "Range(...) = x"), dove il risultato interessante
// e' lo stato del documento, non una variabile VBA.
static void CheckCell(const char* source, const cell& checkCell, const char* expected)
{
	CContainer* doc = new CContainer(NULL, NULL);

	VbaEnvironment env;
	std::string error;
	std::string what = std::string(source) + " -> cella scritta";
	if (!RunSub(source, doc, &env, &error))
	{
		Check(false, (what + " (esecuzione fallita: " + error + ")").c_str());
		doc->Release();
		return;
	}
	Value v;
	doc->GetValue(checkCell, v);
	std::string actual = FormatValue(v);
	bool ok = actual == expected;
	what += " = " + actual;
	if (!ok)
		what += " (atteso: " + std::string(expected) + ")";
	Check(ok, what.c_str());
	doc->Release();
}

static void CheckError(const char* source, CContainer* doc, const char* what)
{
	VbaEnvironment env;
	std::string error;
	bool ok = RunSub(source, doc, &env, &error);
	Check(!ok, what);
}

int main()
{
	cell a1(1, 1); // colonna 1, riga 1

	// ---- Lettura: Cells/Range con ".Value" esplicito ----
	CheckVar("Sub T()\nx = Cells(1, 1).Value\nEnd Sub", &a1, "42", "x", "42");
	CheckVar("Sub T()\nx = Range(\"A1\").Value\nEnd Sub", &a1, "42", "x", "42");
	CheckVar("Sub T()\nx = Range(\"A1\").Value\nEnd Sub", NULL, NULL, "x", "0"); // cella vuota: 0

	// ---- Fase 6j: riferimento ASSOLUTO in stile formula ("$A$1") --
	// identico a quello nudo per Range(), mai relativo a nulla (a
	// differenza di una formula di foglio) -- trovato controllando il
	// vero sorgente chip-8-vba, "Range(\"$BO$3\").value = ..." ----
	CheckVar("Sub T()\nx = Range(\"$A$1\").Value\nEnd Sub", &a1, "42", "x", "42");
	CheckCell("Sub T()\nRange(\"$A$1\").Value = 7\nEnd Sub", a1, "7");

	// ---- Lettura implicita (default member), solo con "=" non "Set" ----
	CheckVar("Sub T()\nx = Range(\"A1\")\nEnd Sub", &a1, "42", "x", "42");
	CheckVar("Sub T()\nx = Cells(1, 1)\nEnd Sub", &a1, "42", "x", "42");

	// ---- Scrittura esplicita (".Value =") e implicita (default member) ----
	CheckCell("Sub T()\nRange(\"A1\").Value = 5\nEnd Sub", a1, "5");
	CheckCell("Sub T()\nCells(1, 1).Value = 7\nEnd Sub", a1, "7");
	CheckCell("Sub T()\nRange(\"A1\") = 9\nEnd Sub", a1, "9"); // implicita, senza ".Value"
	CheckCell("Sub T()\nCells(1, 1) = 11\nEnd Sub", a1, "11");

	// ---- Scrivere e rileggere nella stessa Sub ----
	CheckVar("Sub T()\nRange(\"A1\").Value = 55\nx = Range(\"A1\").Value\nEnd Sub",
		NULL, NULL, "x", "55");

	// ---- Set memorizza il RIFERIMENTO: .Value si usa in seguito tramite la variabile ----
	CheckCell("Sub T()\nSet rng = Range(\"A1\")\nrng.Value = 99\nEnd Sub", a1, "99");
	CheckVar("Sub T()\nRange(\"A1\").Value = 33\nSet rng = Range(\"A1\")\nx = rng.Value\nEnd Sub",
		NULL, NULL, "x", "33");

	// ---- Offset: sposta l'intervallo, conserva il riferimento al documento ----
	{
		cell a2(1, 2);
		CheckCell("Sub T()\nRange(\"A1\").Offset(1, 0).Value = 10\nEnd Sub", a2, "10");
	}
	{
		cell c2(3, 2);
		CheckCell("Sub T()\nSet rng = Range(\"B2\")\nSet rng2 = rng.Offset(0, 1)\n"
			"rng2.Value = 20\nEnd Sub", c2, "20");
	}

	// ---- Integrazione: For + Cells + .Value, il pattern reale di una macro ----
	{
		CContainer* doc = new CContainer(NULL, NULL);
		VbaEnvironment env;
		std::string error;
		bool ok = RunSub("Sub T()\nFor i = 1 To 3\nCells(i, 1).Value = i * 10\nNext\nEnd Sub",
			doc, &env, &error);
		Check(ok, "For + Cells + .Value (il pattern reale di una macro) esegue senza errori");
		Value v1, v2, v3;
		doc->GetValue(cell(1, 1), v1);
		doc->GetValue(cell(1, 2), v2);
		doc->GetValue(cell(1, 3), v3);
		Check(FormatValue(v1) == "10" && FormatValue(v2) == "20" && FormatValue(v3) == "30",
			"For + Cells + .Value scrive A1=10, A2=20, A3=30 come un vero ciclo di riempimento");
		doc->Release();
	}

	// ---- .Select e' un no-op deliberato in questa fase, ma non deve fermare l'esecuzione ----
	CheckVar("Sub T()\nRange(\"A1\").Select\nx = 1\nEnd Sub", NULL, NULL, "x", "1");

	// ---- For Each su un intervallo (Fase 5b): lettura, scrittura, ordine riga per riga ----
	{
		CContainer* doc = new CContainer(NULL, NULL);
		doc->NewCell(cell(1, 1), Value(1.0), NULL); // A1
		doc->NewCell(cell(2, 1), Value(2.0), NULL); // B1
		doc->NewCell(cell(1, 2), Value(3.0), NULL); // A2
		doc->NewCell(cell(2, 2), Value(4.0), NULL); // B2

		VbaEnvironment env;
		std::string error;
		bool ok = RunSub("Sub T()\norder = \"\"\nFor Each c In Range(\"A1:B2\")\n"
			"order = order & c.Value & \",\"\nNext\nEnd Sub", doc, &env, &error);
		Check(ok, "For Each su Range(\"A1:B2\") esegue senza errori");
		const Value* order = env.Find("order");
		Check(order && FormatValue(*order) == "\"1,2,3,4,\"",
			"For Each visita A1,B1,A2,B2 in quest'ordine -- riga per riga, come il vero VBA "
			"per un intervallo rettangolare");
		doc->Release();
	}
	{
		// Scrivere attraverso la variabile del ciclo: ogni "c" e' un
		// vero riferimento a UNA cella (c.Value = ... scrive davvero
		// nel documento, non in una copia).
		CContainer* doc = new CContainer(NULL, NULL);
		doc->NewCell(cell(1, 1), Value(1.0), NULL);
		doc->NewCell(cell(1, 2), Value(2.0), NULL);
		doc->NewCell(cell(1, 3), Value(3.0), NULL);

		VbaEnvironment env;
		std::string error;
		bool ok = RunSub("Sub T()\nFor Each c In Range(\"A1:A3\")\n"
			"c.Value = c.Value * 10\nNext\nEnd Sub", doc, &env, &error);
		Check(ok, "For Each con scrittura attraverso la variabile del ciclo esegue senza errori");
		Value v1, v2, v3;
		doc->GetValue(cell(1, 1), v1);
		doc->GetValue(cell(1, 2), v2);
		doc->GetValue(cell(1, 3), v3);
		Check(FormatValue(v1) == "10" && FormatValue(v2) == "20" && FormatValue(v3) == "30",
			"ogni \"c\" e' un riferimento VERO alla propria cella: A1=10, A2=20, A3=30");
		doc->Release();
	}
	{
		// Exit For dentro un For Each: esce dal ciclo, non e' un errore.
		CContainer* doc = new CContainer(NULL, NULL);
		doc->NewCell(cell(1, 1), Value(1.0), NULL);
		doc->NewCell(cell(1, 2), Value(2.0), NULL);
		doc->NewCell(cell(1, 3), Value(3.0), NULL);

		VbaEnvironment env;
		std::string error;
		bool ok = RunSub("Sub T()\ntotal = 0\nFor Each c In Range(\"A1:A3\")\n"
			"If c.Value = 2 Then Exit For\ntotal = total + c.Value\nNext\nEnd Sub",
			doc, &env, &error);
		Check(ok, "Exit For dentro un For Each esegue senza errori");
		const Value* total = env.Find("total");
		Check(total && FormatValue(*total) == "1",
			"Exit For interrompe il For Each non appena c.Value = 2, prima di sommarlo");
		doc->Release();
	}

	// ---- IsEmpty su un Range (Fase 5c): distingue una cella VERAMENTE vuota da un letterale 0 ----
	{
		cell a1(1, 1);
		CheckVar("Sub T()\nx = IsEmpty(Range(\"A1\"))\nEnd Sub", NULL, NULL, "x", "True");
		CheckVar("Sub T()\nx = IsEmpty(Range(\"A1\"))\nEnd Sub", &a1, "0", "x", "False");
		CheckVar("Sub T()\nx = IsEmpty(Cells(1, 1))\nEnd Sub", &a1, "42", "x", "False");
	}

	// ---- Errori: un messaggio chiaro, mai un crash o un dato sbagliato silenzioso ----
	CheckError("Sub T()\nFor Each c In Range(\"A1\")\nx = 1\nNext\nEnd Sub", NULL,
		"For Each senza un documento collegato produce un errore chiaro");
	{
		CContainer* doc = new CContainer(NULL, NULL);
		CheckError("Sub T()\nFor Each c In 5\nx = 1\nNext\nEnd Sub", doc,
			"For Each su un valore che non e' un intervallo (Range/Cells) produce un errore chiaro");
		doc->Release();
	}
	CheckError("Sub T()\nx = Range(\"A1\").Value\nEnd Sub", NULL,
		"Range/Cells senza un documento collegato (nessun CContainer attivo) produce un errore chiaro");
	{
		CContainer* doc = new CContainer(NULL, NULL);
		CheckError("Sub T()\nx = Range(\"A1:B2\").Value\nEnd Sub", doc,
			"\".Value\" su un intervallo di piu' celle non e' ancora supportato in questa fase");
		CheckError("Sub T()\nx = Range(\"NonEUnRiferimento\").Value\nEnd Sub", doc,
			"un testo che non e' un riferimento di cella valido produce un errore, non un indice a caso");
		CheckError("Sub T()\nx = Cells(0, 1).Value\nEnd Sub", doc,
			"Cells con riga/colonna fuori dai limiti del foglio produce un errore, non un crash");
		CheckError("Sub T()\nRange(\"A1\").Offset(-5, 0).Value = 1\nEnd Sub", doc,
			"Offset che esce dai limiti del foglio (riga negativa) produce un errore, non un crash");
		doc->Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
