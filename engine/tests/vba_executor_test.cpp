/*
	vba_executor_test.cpp

	Verifica ExecuteVbaSub (VbaExecutor.h -- Path to full Excel parity,
	VBA/macro execution, Fase 3b dell'interprete): Dim, assegnazione/
	Set, If/ElseIf/Else, For/Next con Step (anche negativo), Exit For/
	Exit Sub. Ancora nessun modello a oggetti Excel (Range/Cells),
	vedi Fase 3c -- un target di assegnazione che lo richiede produce
	un errore chiaro, verificato qui.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <string>
#include <vector>

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

// Analizza ed esegue un'intera Sub in "env". Vero in caso di successo
// (fino alla fine o fino a un "Exit Sub" esplicito); falso con
// "outError" popolato altrimenti -- sia per un errore di sintassi sia
// per un errore di esecuzione, cosi' i test sugli errori non devono
// distinguere i due casi.
static bool RunSub(const char* source, VbaEnvironment* env, std::string* outError)
{
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

// Esegue "source" e verifica che la variabile "varName" valga
// "expected" alla fine -- fallisce chiaramente (non in modo confuso)
// se l'esecuzione stessa va in errore.
static void CheckVar(const char* source, const char* varName, const char* expected)
{
	VbaEnvironment env;
	std::string error;
	std::string what = std::string(source) + " -> " + varName;
	if (!RunSub(source, &env, &error))
	{
		Check(false, (what + " (esecuzione fallita: " + error + ")").c_str());
		return;
	}
	const Value* v = env.Find(varName);
	if (!v)
	{
		Check(false, (what + " (variabile non trovata)").c_str());
		return;
	}
	std::string actual = FormatValue(*v);
	bool ok = actual == expected;
	what += " = " + actual;
	if (!ok)
		what += " (atteso: " + std::string(expected) + ")";
	Check(ok, what.c_str());
}

static void CheckError(const char* source, const char* what)
{
	VbaEnvironment env;
	std::string error;
	bool ok = RunSub(source, &env, &error);
	Check(!ok, what);
}

int main()
{
	// ---- Dim, assegnazione, Set ----
	CheckVar("Sub T()\nx = 5\nEnd Sub", "x", "5");
	CheckVar("Sub T()\nDim x\nx = 7\nEnd Sub", "x", "7");
	CheckVar("Sub T()\nSet x = 10\nEnd Sub", "x", "10");
	CheckVar("Sub T()\nx = 2\ny = x * 3\nEnd Sub", "y", "6");

	// ---- Dim non reimposta una variabile gia' assegnata ----
	CheckVar("Sub T()\nx = 5\nDim x\nEnd Sub", "x", "5");

	// ---- Variabili senza distinzione fra maiuscole/minuscole ----
	CheckVar("Sub T()\nTotal = 5\ntotal = total + 1\nEnd Sub", "TOTAL", "6");

	// ---- If a riga unica ----
	CheckVar("Sub T()\nIf 5 > 3 Then x = 1\nEnd Sub", "x", "1");

	// ---- If/Else a blocco, ramo vero e ramo falso ----
	CheckVar("Sub T()\nIf 5 > 3 Then\nx = 1\nElse\nx = 2\nEnd If\nEnd Sub", "x", "1");
	CheckVar("Sub T()\nIf 1 > 3 Then\nx = 1\nElse\nx = 2\nEnd If\nEnd Sub", "x", "2");

	// ---- Catena ElseIf: solo il ramo giusto si esegue ----
	CheckVar("Sub T()\nx = 0\nIf 1 > 3 Then\nx = 1\nElseIf 2 > 3 Then\nx = 2\n"
		"ElseIf 4 > 3 Then\nx = 3\nElse\nx = 4\nEnd If\nEnd Sub", "x", "3");

	// ---- For/Next: passo predefinito, passo esplicito, passo negativo ----
	CheckVar("Sub T()\ntotal = 0\nFor i = 1 To 5\ntotal = total + i\nNext i\nEnd Sub", "total", "15");
	CheckVar("Sub T()\ntotal = 0\nFor i = 0 To 10 Step 2\ntotal = total + i\nNext\nEnd Sub", "total", "30");
	CheckVar("Sub T()\ncount = 0\nFor i = 5 To 1 Step -1\ncount = count + 1\nNext\nEnd Sub", "count", "5");

	// ---- For con passo positivo e inizio > fine: il corpo non si esegue mai ----
	CheckVar("Sub T()\nx = 0\nFor i = 5 To 1\nx = x + 1\nNext\nEnd Sub", "x", "0");

	// ---- Exit For interrompe SOLO il ciclo che lo contiene ----
	CheckVar("Sub T()\ntotal = 0\nFor i = 1 To 10\nIf i = 4 Then Exit For\n"
		"total = total + i\nNext\nEnd Sub", "total", "6"); // 1+2+3, i=4 esce prima di somma
	CheckVar("Sub T()\nouter = 0\nFor i = 1 To 3\nFor j = 1 To 3\n"
		"If j = 2 Then Exit For\nouter = outer + 1\nNext j\nNext i\nEnd Sub", "outer", "3");

	// ---- Exit Sub interrompe l'INTERA procedura, non solo il ciclo ----
	CheckVar("Sub T()\nx = 1\nExit Sub\nx = 2\nEnd Sub", "x", "1");
	CheckVar("Sub T()\ntotal = 0\nFor i = 1 To 10\nIf i = 3 Then Exit Sub\n"
		"total = total + i\nNext\nEnd Sub", "total", "3"); // 1+2, esce all'intera Sub con i=3

	// ---- Errori di esecuzione: un messaggio chiaro, mai un crash ----
	CheckError("Sub T()\ny = x + 1\nEnd Sub",
		"leggere una variabile mai assegnata produce un errore, non 0 silenzioso");
	CheckError("Sub T()\nRange(\"A1\").Value = 5\nEnd Sub",
		"assegnare a una proprieta' del modello a oggetti Excel (non ancora collegato "
		"in questa fase) produce un errore chiaro, non un crash");
	CheckError("Sub T()\nFor i = 1 To 10 Step 0\nx = i\nNext\nEnd Sub",
		"un For con Step 0 (ciclo infinito) produce un errore invece di non terminare mai");
	CheckError("Sub T()\nIf z > 1 Then\nx = 1\nEnd If\nEnd Sub",
		"un errore di valutazione nella condizione di un If si propaga come errore della Sub");

	// ---- Un errore di sintassi si propaga con lo stesso meccanismo ----
	CheckError("Sub T(\nEnd Sub", "un errore di sintassi nella Sub produce un errore, non un crash");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
