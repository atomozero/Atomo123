/*
	vba_expr_parser_test.cpp

	Verifica ParseVbaExpression (VbaExprParser.h -- Path to full Excel
	parity, VBA/macro execution, Fase 2b) convertendo l'albero
	risultante in una rappresentazione testuale compatta (DumpExpr
	sotto, una notazione stile S-expression) e confrontandola con la
	forma attesa -- molto piu' leggibile e meno fragile che ispezionare
	ogni nodo a mano in ogni singolo test.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

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

static std::string FormatNumber(double v)
{
	// I numeri di prova in questo file sono tutti interi piccoli o
	// decimali semplici: una formattazione minimale basta, non serve
	// la piena fedelta' di stampa in virgola mobile.
	char buf[64];
	if (v == (long long)v)
		snprintf(buf, sizeof(buf), "%lld", (long long)v);
	else
		snprintf(buf, sizeof(buf), "%g", v);
	return buf;
}

static std::string DumpExpr(const VbaExpr* e)
{
	if (e == NULL)
		return "<null>";
	switch (e->kind)
	{
		case vbaExprNumber:
			return FormatNumber(e->numberValue);
		case vbaExprString:
			return "\"" + e->stringValue + "\"";
		case vbaExprBool:
			return e->boolValue ? "True" : "False";
		case vbaExprVariable:
			return e->name;
		case vbaExprUnary:
			return "(" + e->op + " " + DumpExpr(e->children[0].get()) + ")";
		case vbaExprBinary:
			return "(" + DumpExpr(e->children[0].get()) + " " + e->op + " "
				+ DumpExpr(e->children[1].get()) + ")";
		case vbaExprCall:
		{
			std::string out;
			if (e->target)
				out += DumpExpr(e->target.get()) + ".";
			out += e->name + "(";
			for (size_t i = 0; i < e->children.size(); i++)
			{
				if (i > 0)
					out += ", ";
				out += DumpExpr(e->children[i].get());
			}
			out += ")";
			return out;
		}
		case vbaExprPropertyAccess:
			return DumpExpr(e->target.get()) + "." + e->name;
	}
	return "?";
}

// Analizza UNA espressione da "source" e restituisce la sua forma
// testuale, o "<errore: messaggio>" se l'analisi fallisce -- cosi'
// anche i test sugli errori possono usare lo stesso confronto diretto
// di stringa degli altri.
static std::string ParseAndDump(const char* source)
{
	std::vector<VbaToken> tokens;
	TokenizeVba(source, &tokens);
	size_t pos = 0;
	VbaParseError error;
	std::unique_ptr<VbaExpr> expr = ParseVbaExpression(tokens, pos, &error);
	if (!expr)
		return "<errore: " + error.message + ">";
	return DumpExpr(expr.get());
}

static void CheckExpr(const char* source, const char* expected)
{
	std::string actual = ParseAndDump(source);
	bool ok = actual == expected;
	std::string what = std::string("\"") + source + "\" -> " + actual;
	if (!ok)
		what += " (atteso: " + std::string(expected) + ")";
	Check(ok, what.c_str());
}

int main()
{
	// ---- Literali e variabili ----
	CheckExpr("42", "42");
	CheckExpr("3.14", "3.14");
	CheckExpr("\"hello\"", "\"hello\"");
	CheckExpr("True", "True");
	CheckExpr("False", "False");
	CheckExpr("x", "x");

	// ---- Precedenza aritmetica standard ----
	CheckExpr("1 + 2 * 3", "(1 + (2 * 3))");
	CheckExpr("(1 + 2) * 3", "((1 + 2) * 3)");
	CheckExpr("10 - 2 - 3", "((10 - 2) - 3)"); // sottrazione associativa a sinistra
	CheckExpr("2 * 3 / 4", "((2 * 3) / 4)");

	// ---- La particolarita' di VBA: il meno unario si lega PIU' DEBOLMENTE di "^" ----
	CheckExpr("-2 ^ 2", "(- (2 ^ 2))"); // -4, non 4: il meno si applica DOPO l'elevamento a potenza
	CheckExpr("2 ^ -2", "(2 ^ (- 2))"); // il lato destro di "^" puo' avere un proprio meno unario
	CheckExpr("2 ^ 3 ^ 2", "(2 ^ (3 ^ 2))"); // "^" associativo a destra: 2^(3^2)=512, non (2^3)^2=64

	// ---- Mod, concatenazione, confronto ----
	CheckExpr("7 Mod 3", "(7 Mod 3)");
	CheckExpr("1 + 2 Mod 3", "(1 + (2 Mod 3))"); // Mod si lega piu' stretto di +/-
	CheckExpr("\"a\" & \"b\" & \"c\"", "((\"a\" & \"b\") & \"c\")"); // "&" associativo a sinistra
	CheckExpr("x + 1 & \"!\"", "((x + 1) & \"!\")"); // "&" si lega piu' debole di "+"
	CheckExpr("x = 5", "(x = 5)");
	CheckExpr("x <> 5", "(x <> 5)");
	CheckExpr("x <= 5", "(x <= 5)");
	CheckExpr("1 + 2 = 3", "((1 + 2) = 3)"); // il confronto si lega piu' debole dell'addizione

	// ---- Not/And/Or, con la precedenza reale di VBA (Or il piu' debole) ----
	CheckExpr("Not x", "(Not x)");
	CheckExpr("Not x = 5", "(Not (x = 5))"); // "Not" si lega piu' debole del confronto
	CheckExpr("x And y Or z", "((x And y) Or z)"); // "And" si lega piu' stretto di "Or"
	CheckExpr("x Or y And z", "(x Or (y And z))"); // stessa cosa, ordine invertito nel sorgente

	// ---- Chiamate a funzione, con zero, uno, e piu' argomenti ----
	CheckExpr("Foo()", "Foo()");
	CheckExpr("Len(x)", "Len(x)");
	CheckExpr("Cells(1, 2)", "Cells(1, 2)");
	CheckExpr("Mid(x, 1, 3)", "Mid(x, 1, 3)");
	CheckExpr("Range(\"A1\")", "Range(\"A1\")");

	// ---- Accesso a proprieta' e chiamate di metodo incatenate ----
	CheckExpr("Range(\"A1\").Value", "Range(\"A1\").Value");
	CheckExpr("Range(\"A1\").Value = 5", "(Range(\"A1\").Value = 5)");
	CheckExpr("Cells(i, 1).Value", "Cells(i, 1).Value");
	CheckExpr("Range(\"A1\").Offset(1, 0).Value", "Range(\"A1\").Offset(1, 0).Value");
	CheckExpr("Range(\"A1\").Select", "Range(\"A1\").Select");

	// ---- Argomenti complessi dentro una chiamata ----
	CheckExpr("Cells(i + 1, j * 2)", "Cells((i + 1), (j * 2))");

	// ---- Espressioni miste realistiche ----
	CheckExpr("i Mod 2 = 0", "((i Mod 2) = 0)");
	CheckExpr("(B4 - B5) * B7 - B6", "(((B4 - B5) * B7) - B6)");

	// ---- Errori di sintassi: un messaggio chiaro, mai un crash ----
	{
		std::string result = ParseAndDump("1 +");
		Check(result.substr(0, 9) == "<errore: ",
			"\"1 +\" (operando mancante dopo l'operatore) produce un errore, non un crash");
	}
	{
		std::string result = ParseAndDump("Cells(1, 2");
		Check(result.substr(0, 9) == "<errore: ",
			"una chiamata senza la parentesi di chiusura produce un errore, non legge oltre la fine");
	}
	{
		std::string result = ParseAndDump("Range(\"A1\").");
		Check(result.substr(0, 9) == "<errore: ",
			"un \".\" senza un nome di proprieta' dopo produce un errore, non un crash");
	}
	{
		std::string result = ParseAndDump("");
		Check(result.substr(0, 9) == "<errore: ",
			"un'espressione vuota produce un errore, non un crash");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
