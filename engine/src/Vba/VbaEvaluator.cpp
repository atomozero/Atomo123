/*
	VbaEvaluator.cpp

	Vedi VbaEvaluator.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaEvaluator.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------
// Conversioni di tipo stile VBA (Variant): piu' permissive del C++,
// MAI un errore qui -- una conversione impossibile (es. testo non
// numerico verso numero) produce semplicemente 0.0/testo vuoto/falso,
// stesso principio di una funzione di libreria del motore di calcolo
// gia' esistente (es. Value::operator double() su un Value non
// numerico restituisce NaN invece di fallire) piuttosto che inventare
// qui un secondo stile di segnalazione errori per le sole conversioni.
// ---------------------------------------------------------------------

static double ToNumber(const Value& v)
{
	switch (v.fType)
	{
		case eNumData: return (double)v;
		case eBoolData: return (bool)v ? 1.0 : 0.0;
		case eTextData: return atof((const char*)v);
		default: return 0.0;
	}
}

static std::string ToText(const Value& v)
{
	switch (v.fType)
	{
		case eTextData: return std::string((const char*)v);
		case eBoolData: return (bool)v ? "True" : "False";
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
		default: return "";
	}
}

static bool ToBool(const Value& v)
{
	switch (v.fType)
	{
		case eBoolData: return (bool)v;
		case eNumData: return (double)v != 0.0;
		case eTextData:
		{
			std::string s = ToText(v);
			return s == "True" || s == "true" || s == "TRUE";
		}
		default: return false;
	}
}

namespace {

class Evaluator {
public:
	Evaluator(VbaEnvironment* env, VbaRuntimeError* outError)
		: fEnv(env), fError(outError), fFailed(false) {}

	bool Evaluate(const VbaExpr* expr, Value* outResult)
	{
		Value v = EvalNode(expr);
		if (fFailed)
			return false;
		*outResult = v;
		return true;
	}

private:
	VbaEnvironment* fEnv;
	VbaRuntimeError* fError;
	bool fFailed;

	void SetError(const std::string& message, int line)
	{
		if (fFailed)
			return; // mantiene il PRIMO errore incontrato
		fFailed = true;
		fError->message = message;
		fError->line = line;
	}

	Value EvalNode(const VbaExpr* expr)
	{
		if (fFailed)
			return Value(0.0);

		switch (expr->kind)
		{
			case vbaExprNumber:
				return Value(expr->numberValue);
			case vbaExprString:
				return Value(expr->stringValue.c_str());
			case vbaExprBool:
				return Value(expr->boolValue);
			case vbaExprVariable:
				return EvalVariable(expr);
			case vbaExprUnary:
				return EvalUnary(expr);
			case vbaExprBinary:
				return EvalBinary(expr);
			case vbaExprCall:
				return EvalCall(expr);
			case vbaExprPropertyAccess:
				SetError("il modello a oggetti Excel (Range/Cells/proprieta') "
					"non e' ancora supportato in questa fase dell'interprete", expr->line);
				return Value(0.0);
		}
		SetError("tipo di espressione sconosciuto", expr->line);
		return Value(0.0);
	}

	Value EvalVariable(const VbaExpr* expr)
	{
		const Value* v = fEnv->Find(expr->name);
		if (v == NULL)
		{
			SetError("variabile \"" + expr->name + "\" usata prima di essere assegnata", expr->line);
			return Value(0.0);
		}
		return *v;
	}

	Value EvalUnary(const VbaExpr* expr)
	{
		Value operand = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		if (expr->op == "-")
			return Value(-ToNumber(operand));
		if (expr->op == "Not")
			return Value(!ToBool(operand));
		SetError("operatore unario sconosciuto \"" + expr->op + "\"", expr->line);
		return Value(0.0);
	}

	Value EvalBinary(const VbaExpr* expr)
	{
		Value left = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		Value right = EvalNode(expr->children[1].get());
		if (fFailed)
			return Value(0.0);

		const std::string& op = expr->op;

		if (op == "&")
			return Value((ToText(left) + ToText(right)).c_str());

		if (op == "+" || op == "-" || op == "*" || op == "/" || op == "^")
		{
			double a = ToNumber(left), b = ToNumber(right);
			if (op == "+") return Value(a + b);
			if (op == "-") return Value(a - b);
			if (op == "*") return Value(a * b);
			if (op == "/")
			{
				if (b == 0.0)
				{
					SetError("divisione per zero", expr->line);
					return Value(0.0);
				}
				return Value(a / b);
			}
			return Value(pow(a, b)); // "^"
		}

		if (op == "Mod")
		{
			long long a = (long long)ToNumber(left), b = (long long)ToNumber(right);
			if (b == 0)
			{
				SetError("divisione per zero (Mod)", expr->line);
				return Value(0.0);
			}
			return Value((double)(a % b));
		}

		if (op == "And")
			return Value(ToBool(left) && ToBool(right));
		if (op == "Or")
			return Value(ToBool(left) || ToBool(right));

		if (op == "=" || op == "<>" || op == "<" || op == ">" || op == "<=" || op == ">=")
		{
			// Se ENTRAMBI gli operandi sono numerici (o booleani, che
			// si confrontano come numeri 0/1), confronto numerico;
			// altrimenti confronto come testo -- stessa regola
			// semplificata gia' documentata nell'header.
			bool numericCompare = (left.fType == eNumData || left.fType == eBoolData)
				&& (right.fType == eNumData || right.fType == eBoolData);
			int cmp;
			if (numericCompare)
			{
				double a = ToNumber(left), b = ToNumber(right);
				cmp = (a < b) ? -1 : (a > b) ? 1 : 0;
			}
			else
			{
				std::string a = ToText(left), b = ToText(right);
				cmp = a.compare(b);
				cmp = (cmp < 0) ? -1 : (cmp > 0) ? 1 : 0;
			}
			if (op == "=") return Value(cmp == 0);
			if (op == "<>") return Value(cmp != 0);
			if (op == "<") return Value(cmp < 0);
			if (op == ">") return Value(cmp > 0);
			if (op == "<=") return Value(cmp <= 0);
			return Value(cmp >= 0); // ">="
		}

		SetError("operatore binario sconosciuto \"" + op + "\"", expr->line);
		return Value(0.0);
	}

	Value EvalCall(const VbaExpr* expr)
	{
		if (expr->target)
		{
			SetError("il modello a oggetti Excel (una chiamata di metodo su un target, "
				"es. Range(...).Offset(...)) non e' ancora supportato in questa fase "
				"dell'interprete", expr->line);
			return Value(0.0);
		}

		std::vector<Value> args;
		for (size_t i = 0; i < expr->children.size(); i++)
		{
			args.push_back(EvalNode(expr->children[i].get()));
			if (fFailed)
				return Value(0.0);
		}

		std::string name = expr->name;
		for (size_t i = 0; i < name.size(); i++)
			name[i] = (char)tolower((unsigned char)name[i]);

		if (name == "len")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value((double)ToText(args[0]).size());
		}
		if (name == "left")
		{
			if (!CheckArgCount(expr, args, 2)) return Value(0.0);
			std::string s = ToText(args[0]);
			long long n = (long long)ToNumber(args[1]);
			if (n < 0) n = 0;
			if ((size_t)n > s.size()) n = (long long)s.size();
			return Value(s.substr(0, (size_t)n).c_str());
		}
		if (name == "right")
		{
			if (!CheckArgCount(expr, args, 2)) return Value(0.0);
			std::string s = ToText(args[0]);
			long long n = (long long)ToNumber(args[1]);
			if (n < 0) n = 0;
			if ((size_t)n > s.size()) n = (long long)s.size();
			return Value(s.substr(s.size() - (size_t)n).c_str());
		}
		if (name == "mid")
		{
			if (args.size() != 2 && args.size() != 3)
			{
				SetError("Mid richiede 2 o 3 argomenti", expr->line);
				return Value(0.0);
			}
			std::string s = ToText(args[0]);
			long long start = (long long)ToNumber(args[1]); // Mid e' 1-based in VBA
			if (start < 1) start = 1;
			if ((size_t)start > s.size())
				return Value("");
			size_t startIdx = (size_t)(start - 1);
			size_t len = args.size() == 3 ? (size_t)ToNumber(args[2]) : (s.size() - startIdx);
			if (startIdx + len > s.size())
				len = s.size() - startIdx;
			return Value(s.substr(startIdx, len).c_str());
		}
		if (name == "ucase")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = ToText(args[0]);
			for (size_t i = 0; i < s.size(); i++)
				s[i] = (char)toupper((unsigned char)s[i]);
			return Value(s.c_str());
		}
		if (name == "lcase")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = ToText(args[0]);
			for (size_t i = 0; i < s.size(); i++)
				s[i] = (char)tolower((unsigned char)s[i]);
			return Value(s.c_str());
		}
		if (name == "trim")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = ToText(args[0]);
			size_t start = s.find_first_not_of(" \t");
			size_t end = s.find_last_not_of(" \t");
			if (start == std::string::npos)
				return Value("");
			return Value(s.substr(start, end - start + 1).c_str());
		}
		if (name == "abs")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(fabs(ToNumber(args[0])));
		}
		if (name == "int")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(floor(ToNumber(args[0]))); // Int() di VBA arrotonda verso il basso, non tronca
		}
		if (name == "round")
		{
			if (args.size() != 1 && args.size() != 2)
			{
				SetError("Round richiede 1 o 2 argomenti", expr->line);
				return Value(0.0);
			}
			double d = ToNumber(args[0]);
			int digits = args.size() == 2 ? (int)ToNumber(args[1]) : 0;
			double factor = pow(10.0, digits);
			return Value(floor(d * factor + 0.5) / factor);
		}

		SetError("funzione sconosciuta \"" + expr->name + "\" "
			"(fuori dal sottoinsieme supportato, o un nome dell'oggetto Excel "
			"non ancora collegato -- vedi Fase 3c)", expr->line);
		return Value(0.0);
	}

	bool CheckArgCount(const VbaExpr* expr, const std::vector<Value>& args, size_t expected)
	{
		if (args.size() == expected)
			return true;
		char buf[128];
		snprintf(buf, sizeof(buf), "%s richiede esattamente %zu argomento/i, ricevuti %zu",
			expr->name.c_str(), expected, args.size());
		SetError(buf, expr->line);
		return false;
	}
};

} // namespace

bool EvaluateVbaExpression(const VbaExpr* expr, VbaEnvironment* env, Value* outResult,
	VbaRuntimeError* outError)
{
	Evaluator eval(env, outError);
	return eval.Evaluate(expr, outResult);
}
