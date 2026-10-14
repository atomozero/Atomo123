/*
	vba_evaluator_test.cpp

	Verifica EvaluateVbaExpression (VbaEvaluator.h -- Path to full
	Excel parity, VBA/macro execution, Fase 3a dell'interprete, con
	l'aggiunta successiva di un gruppo di funzioni predefinite comuni
	nelle macro reali alla Fase 5c -- vedi ROADMAP.md): literali,
	variabili, operatori con le regole di conversione semplificate di
	questo sottoinsieme, e le funzioni predefinite (Len/Left/Right/
	Mid/UCase/LCase/Trim/Abs/Int/Round dalla Fase 3a; IsEmpty/
	IsNumeric/CStr/CInt/CLng/CDbl/CBool/Val/Space/InStr/Replace/
	StrComp dalla Fase 5c). IsEmpty su un Range senza un documento
	collegato e' verificato in vba_object_model_test.cpp (richiede un
	vero CContainer), non qui.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <string>
#include <vector>

#include "VbaEvaluator.h"
#include "VbaExprParser.h"
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

// Analizza ed esegue una singola espressione, con "env" facoltativo
// (condiviso fra piu' chiamate per i test che leggono variabili gia'
// assegnate in un passo precedente). Restituisce "<errore: msg>" in
// caso di fallimento sintattico O di esecuzione, cosi' i test sugli
// errori possono usare lo stesso confronto diretto degli altri.
static std::string EvalToString(const char* source, VbaEnvironment* env)
{
	std::vector<VbaToken> tokens;
	TokenizeVba(source, &tokens);
	size_t pos = 0;
	VbaParseError parseError;
	std::unique_ptr<VbaExpr> expr = ParseVbaExpression(tokens, pos, &parseError);
	if (!expr)
		return "<errore sintassi: " + parseError.message + ">";

	Value result;
	VbaRuntimeError runtimeError;
	if (!EvaluateVbaExpression(expr.get(), env, &result, &runtimeError))
		return "<errore: " + runtimeError.message + ">";

	switch (result.fType)
	{
		case eNumData:
		{
			double d = (double)result;
			char buf[64];
			if (d == (long long)d)
				snprintf(buf, sizeof(buf), "%lld", (long long)d);
			else
				snprintf(buf, sizeof(buf), "%g", d);
			return buf;
		}
		case eTextData:
			return std::string("\"") + (const char*)result + "\"";
		case eBoolData:
			return (bool)result ? "True" : "False";
		default:
			return "<tipo inatteso>";
	}
}

static void CheckEval(const char* source, const char* expected, VbaEnvironment* env = NULL)
{
	VbaEnvironment localEnv;
	std::string actual = EvalToString(source, env ? env : &localEnv);
	bool ok = actual == expected;
	std::string what = std::string(source) + " -> " + actual;
	if (!ok)
		what += " (atteso: " + std::string(expected) + ")";
	Check(ok, what.c_str());
}

int main()
{
	// ---- Literali ----
	CheckEval("42", "42");
	CheckEval("3.14", "3.14");
	CheckEval("\"hello\"", "\"hello\"");
	CheckEval("True", "True");
	CheckEval("False", "False");

	// ---- Aritmetica, con la stessa precedenza gia' provata nel parser ----
	CheckEval("1 + 2 * 3", "7");
	CheckEval("(1 + 2) * 3", "9");
	CheckEval("10 / 4", "2.5");
	CheckEval("2 ^ 10", "1024");
	CheckEval("-2 ^ 2", "-4"); // la particolarita' VBA: il meno si applica DOPO l'elevamento a potenza
	CheckEval("7 Mod 3", "1");
	CheckEval("-7 Mod 3", "-1"); // Mod di VBA segue il segno del primo operando (tronca verso zero)

	// ---- Concatenazione, con coercizione da numero/booleano a testo ----
	CheckEval("\"a\" & \"b\"", "\"ab\"");
	CheckEval("\"x = \" & 5", "\"x = 5\"");
	CheckEval("\"ok: \" & True", "\"ok: True\"");

	// ---- Confronto: numerico quando entrambi gli operandi sono numeri/booleani ----
	CheckEval("5 = 5", "True");
	CheckEval("5 <> 5", "False");
	CheckEval("5 > 3", "True");
	CheckEval("5 < 3", "False");
	CheckEval("5 >= 5", "True");
	CheckEval("5 <= 4", "False");
	// ---- Confronto come testo quando non sono entrambi numerici ----
	CheckEval("\"abc\" = \"abc\"", "True");
	CheckEval("\"abc\" < \"abd\"", "True");

	// ---- Not/And/Or ----
	CheckEval("Not True", "False");
	CheckEval("Not False", "True");
	CheckEval("True And False", "False");
	CheckEval("True Or False", "True");
	CheckEval("(5 > 3) And (2 > 1)", "True");

	// ---- Variabili: lettura dopo assegnazione, condivise fra due chiamate ----
	{
		VbaEnvironment env;
		env.Set("x", Value(10.0));
		CheckEval("x + 5", "15", &env);
		CheckEval("x * 2", "20", &env);
	}
	{
		// Senza distinzione fra maiuscole/minuscole, come il vero VBA:
		// la STESSA variabile letta/scritta con grafie diverse.
		VbaEnvironment env;
		env.Set("Total", Value(7.0));
		CheckEval("total + 1", "8", &env);
		CheckEval("TOTAL + 2", "9", &env);
	}

	// ---- Funzioni predefinite: stringhe ----
	CheckEval("Len(\"hello\")", "5");
	CheckEval("Left(\"hello\", 2)", "\"he\"");
	CheckEval("Right(\"hello\", 2)", "\"lo\"");
	CheckEval("Mid(\"hello\", 2, 3)", "\"ell\"");
	CheckEval("Mid(\"hello\", 2)", "\"ello\""); // senza lunghezza: fino alla fine
	CheckEval("UCase(\"hello\")", "\"HELLO\"");
	CheckEval("LCase(\"HELLO\")", "\"hello\"");
	CheckEval("Trim(\"  hi  \")", "\"hi\"");

	// ---- Funzioni predefinite: matematiche ----
	CheckEval("Abs(-5)", "5");
	CheckEval("Abs(5)", "5");
	CheckEval("Int(3.7)", "3");
	CheckEval("Int(-3.2)", "-4"); // Int() di VBA arrotonda verso il basso, non tronca verso zero
	CheckEval("Round(3.14159, 2)", "3.14");
	CheckEval("Round(2.5)", "3");

	// ---- Funzioni predefinite: Fase 5c, comuni nelle macro reali ----
	CheckEval("IsNumeric(\"123\")", "True");
	CheckEval("IsNumeric(\"12.5\")", "True");
	CheckEval("IsNumeric(\"12abc\")", "False"); // un prefisso numerico non basta, a differenza di Val
	CheckEval("IsNumeric(\"abc\")", "False");
	CheckEval("IsNumeric(42)", "True"); // un numero vero e' sempre numerico, nessun parsing di testo
	CheckEval("IsEmpty(5)", "False"); // una variabile/un letterale non e' mai "vuoto" in questo sottoinsieme
	CheckEval("CStr(42)", "\"42\"");
	CheckEval("CStr(True)", "\"True\"");
	CheckEval("CInt(3.6)", "4");
	CheckEval("CInt(3.4)", "3");
	CheckEval("CLng(-2.5)", "-2"); // stessa regola "meta' in su" di Round, non "verso zero"
	CheckEval("CDbl(\"3.5\")", "3.5");
	CheckEval("CBool(0)", "False");
	CheckEval("CBool(1)", "True");
	CheckEval("Val(\"123abc\")", "123"); // si ferma al primo carattere non numerico, nessun errore
	CheckEval("Val(\"abc\")", "0");
	CheckEval("Len(Space(3))", "3");
	CheckEval("InStr(\"hello world\", \"world\")", "7"); // 1-based come VBA
	CheckEval("InStr(\"hello\", \"xyz\")", "0"); // non trovato
	CheckEval("InStr(5, \"ababab\", \"ab\")", "5"); // forma a 3 argomenti: cerca a partire dalla posizione 5
	CheckEval("Replace(\"a-b-c\", \"-\", \"_\")", "\"a_b_c\""); // sostituisce TUTTE le occorrenze
	CheckEval("StrComp(\"abc\", \"abc\")", "0");
	CheckEval("StrComp(\"abc\", \"abd\")", "-1");
	CheckEval("StrComp(\"abd\", \"abc\")", "1");

	// ---- Errori di esecuzione: un messaggio chiaro, mai un crash ----
	{
		VbaEnvironment localEnv;
		std::string r = EvalToString("y + 1", &localEnv);
		Check(r.substr(0, 9) == "<errore: ",
			"leggere una variabile mai assegnata produce un errore, non 0 silenzioso");
	}
	{
		VbaEnvironment localEnv;
		std::string r = EvalToString("5 / 0", &localEnv);
		Check(r.substr(0, 9) == "<errore: ", "la divisione per zero produce un errore, non Inf/NaN silenzioso");
	}
	{
		VbaEnvironment localEnv;
		std::string r = EvalToString("Len(1, 2, 3)", &localEnv);
		Check(r.substr(0, 9) == "<errore: ", "un numero sbagliato di argomenti a una funzione nota produce un errore");
	}
	{
		VbaEnvironment localEnv;
		std::string r = EvalToString("DoesNotExist(1)", &localEnv);
		Check(r.substr(0, 9) == "<errore: ", "una funzione sconosciuta produce un errore, non un crash");
	}
	{
		VbaEnvironment localEnv;
		std::string r = EvalToString("Range(\"A1\").Value", &localEnv);
		Check(r.substr(0, 9) == "<errore: ",
			"un accesso al modello a oggetti Excel (non ancora collegato in questa fase) "
			"produce un errore chiaro, non un crash");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
