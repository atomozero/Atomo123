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

#include "Container.h"

// ---------------------------------------------------------------------
// Conversioni di tipo stile VBA (Variant): piu' permissive del C++,
// MAI un errore qui -- una conversione impossibile (es. testo non
// numerico verso numero) produce semplicemente 0.0/testo vuoto/falso,
// stesso principio di una funzione di libreria del motore di calcolo
// gia' esistente (es. Value::operator double() su un Value non
// numerico restituisce NaN invece di fallire) piuttosto che inventare
// qui un secondo stile di segnalazione errori per le sole conversioni.
// ---------------------------------------------------------------------

double VbaToNumber(const Value& v)
{
	switch (v.fType)
	{
		case eNumData: return (double)v;
		case eBoolData: return (bool)v ? 1.0 : 0.0;
		case eTextData: return atof((const char*)v);
		default: return 0.0;
	}
}

std::string VbaToText(const Value& v)
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

bool VbaToBool(const Value& v)
{
	switch (v.fType)
	{
		case eBoolData: return (bool)v;
		case eNumData: return (double)v != 0.0;
		case eTextData:
		{
			std::string s = VbaToText(v);
			return s == "True" || s == "true" || s == "TRUE";
		}
		default: return false;
	}
}

// ---------------------------------------------------------------------
// Fase 3c, modello a oggetti Excel minimo: Range("A1")/Range("A1:B2")/
// Cells(riga,colonna) producono un Value eRangeData legato al
// CContainer attivo (vedi Value::fRangeContainer); ".Value"/la
// coercizione implicita leggono/scrivono la cella vera attraverso
// quel puntatore. Solo celle SINGOLE in questa fase -- un intervallo
// di piu' celle e' un errore chiaro, non un dato sbagliato silenzioso.
// ---------------------------------------------------------------------

// "A1" o "A1:B2" (MAI un nome definito/una tabella in questa fase,
// solo un riferimento letterale) -- entrambe le meta' devono
// consumare l'INTERA sottostringa, altrimenti un testo come "A1X" o
// "Foo" sarebbe accettato per coincidenza (cell::GetCell si ferma al
// primo carattere che non riconosce, non richiede di arrivare alla
// fine).
static bool ParseSimpleRangeRef(const std::string& text, range* outRange)
{
	size_t colon = text.find(':');
	std::string leftPart = (colon == std::string::npos) ? text : text.substr(0, colon);
	std::string rightPart = (colon == std::string::npos) ? text : text.substr(colon + 1);

	cell topLeft, botRight;
	int leftConsumed = cell::GetCell(leftPart.c_str(), topLeft);
	int rightConsumed = cell::GetCell(rightPart.c_str(), botRight);
	if (leftConsumed != (int)leftPart.size() || rightConsumed != (int)rightPart.size())
		return false;
	if (!topLeft.IsValid() || !botRight.IsValid())
		return false;

	int left = topLeft.h < botRight.h ? topLeft.h : botRight.h;
	int right = topLeft.h > botRight.h ? topLeft.h : botRight.h;
	int top = topLeft.v < botRight.v ? topLeft.v : botRight.v;
	int bottom = topLeft.v > botRight.v ? topLeft.v : botRight.v;
	*outRange = range(left, top, right, bottom);
	return true;
}

static Value MakeRangeValue(const range& r, CContainer* doc)
{
	Value v;
	v = r; // Value::operator=(const range&): fType = eRangeData, fRange = r
	v.fRangeContainer = doc; // vedi il commento su Value::fRangeContainer in Value.h
	return v;
}

bool VbaReadRangeValue(const Value& rangeValue, Value* outResult, VbaRuntimeError* outError, int line)
{
	if (rangeValue.fType != eRangeData)
	{
		outError->message = "qui serve un intervallo (Range/Cells)";
		outError->line = line;
		return false;
	}
	range r = (range)rangeValue;
	if (r.TopLeft() != r.BotRight())
	{
		outError->message = "un intervallo di piu' celle non e' ancora supportato in questa fase "
			"dell'interprete (solo celle singole)";
		outError->line = line;
		return false;
	}
	CContainer* doc = rangeValue.fRangeContainer;
	if (!doc)
	{
		outError->message = "nessun foglio di calcolo collegato a questa esecuzione VBA";
		outError->line = line;
		return false;
	}
	Value cellValue;
	if (!doc->GetValue(r.TopLeft(), cellValue) || cellValue.fType == eNoData)
	{
		*outResult = Value(0.0); // cella vuota: 0, come una Variant vuota di VBA
		return true;
	}
	if (cellValue.fType == eNumData || cellValue.fType == eTextData || cellValue.fType == eBoolData)
	{
		*outResult = cellValue;
		return true;
	}
	*outResult = Value(0.0); // altri tipi di cella (es. un errore di formula) non ancora gestiti qui
	return true;
}

bool VbaWriteRangeValue(const Value& rangeValue, const Value& newValue, VbaRuntimeError* outError, int line)
{
	if (rangeValue.fType != eRangeData)
	{
		outError->message = "il target dell'assegnazione richiede un intervallo (Range/Cells)";
		outError->line = line;
		return false;
	}
	range r = (range)rangeValue;
	if (r.TopLeft() != r.BotRight())
	{
		outError->message = "la scrittura su un intervallo di piu' celle non e' ancora supportata "
			"in questa fase dell'interprete (solo celle singole)";
		outError->line = line;
		return false;
	}
	CContainer* doc = rangeValue.fRangeContainer;
	if (!doc)
	{
		outError->message = "nessun foglio di calcolo collegato a questa esecuzione VBA";
		outError->line = line;
		return false;
	}
	// NewCell, non SetValue: una scrittura da VBA sostituisce
	// interamente il contenuto della cella (come l'utente che digita
	// sopra), cancellando un'eventuale formula precedente -- stessa
	// tecnica gia' usata da Ricerca Obiettivo/Tabelle di simulazione
	// altrove in questo codice.
	doc->NewCell(r.TopLeft(), newValue, NULL);
	return true;
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
				return EvalPropertyAccess(expr);
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
			return Value(-VbaToNumber(operand));
		if (expr->op == "Not")
			return Value(!VbaToBool(operand));
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
			return Value((VbaToText(left) + VbaToText(right)).c_str());

		if (op == "+" || op == "-" || op == "*" || op == "/" || op == "^")
		{
			double a = VbaToNumber(left), b = VbaToNumber(right);
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
			long long a = (long long)VbaToNumber(left), b = (long long)VbaToNumber(right);
			if (b == 0)
			{
				SetError("divisione per zero (Mod)", expr->line);
				return Value(0.0);
			}
			return Value((double)(a % b));
		}

		if (op == "And")
			return Value(VbaToBool(left) && VbaToBool(right));
		if (op == "Or")
			return Value(VbaToBool(left) || VbaToBool(right));

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
				double a = VbaToNumber(left), b = VbaToNumber(right);
				cmp = (a < b) ? -1 : (a > b) ? 1 : 0;
			}
			else
			{
				std::string a = VbaToText(left), b = VbaToText(right);
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

	static std::string Lowercase(const std::string& s)
	{
		std::string out = s;
		for (size_t i = 0; i < out.size(); i++)
			out[i] = (char)tolower((unsigned char)out[i]);
		return out;
	}

	Value EvalCall(const VbaExpr* expr)
	{
		if (expr->target)
		{
			if (Lowercase(expr->name) == "offset")
				return EvalOffsetCall(expr);
			SetError("il metodo \"" + expr->name + "\" del modello a oggetti Excel "
				"non e' ancora supportato in questa fase dell'interprete", expr->line);
			return Value(0.0);
		}

		std::string name = Lowercase(expr->name);
		if (name == "range")
			return EvalRangeFunction(expr);
		if (name == "cells")
			return EvalCellsFunction(expr);

		std::vector<Value> args;
		for (size_t i = 0; i < expr->children.size(); i++)
		{
			args.push_back(EvalNode(expr->children[i].get()));
			if (fFailed)
				return Value(0.0);
		}

		if (name == "len")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value((double)VbaToText(args[0]).size());
		}
		if (name == "left")
		{
			if (!CheckArgCount(expr, args, 2)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			long long n = (long long)VbaToNumber(args[1]);
			if (n < 0) n = 0;
			if ((size_t)n > s.size()) n = (long long)s.size();
			return Value(s.substr(0, (size_t)n).c_str());
		}
		if (name == "right")
		{
			if (!CheckArgCount(expr, args, 2)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			long long n = (long long)VbaToNumber(args[1]);
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
			std::string s = VbaToText(args[0]);
			long long start = (long long)VbaToNumber(args[1]); // Mid e' 1-based in VBA
			if (start < 1) start = 1;
			if ((size_t)start > s.size())
				return Value("");
			size_t startIdx = (size_t)(start - 1);
			size_t len = args.size() == 3 ? (size_t)VbaToNumber(args[2]) : (s.size() - startIdx);
			if (startIdx + len > s.size())
				len = s.size() - startIdx;
			return Value(s.substr(startIdx, len).c_str());
		}
		if (name == "ucase")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			for (size_t i = 0; i < s.size(); i++)
				s[i] = (char)toupper((unsigned char)s[i]);
			return Value(s.c_str());
		}
		if (name == "lcase")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			for (size_t i = 0; i < s.size(); i++)
				s[i] = (char)tolower((unsigned char)s[i]);
			return Value(s.c_str());
		}
		if (name == "trim")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			size_t start = s.find_first_not_of(" \t");
			size_t end = s.find_last_not_of(" \t");
			if (start == std::string::npos)
				return Value("");
			return Value(s.substr(start, end - start + 1).c_str());
		}
		if (name == "abs")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(fabs(VbaToNumber(args[0])));
		}
		if (name == "int")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(floor(VbaToNumber(args[0]))); // Int() di VBA arrotonda verso il basso, non tronca
		}
		if (name == "round")
		{
			if (args.size() != 1 && args.size() != 2)
			{
				SetError("Round richiede 1 o 2 argomenti", expr->line);
				return Value(0.0);
			}
			double d = VbaToNumber(args[0]);
			int digits = args.size() == 2 ? (int)VbaToNumber(args[1]) : 0;
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

	// Fase 3c, modello a oggetti Excel minimo -- vedi il commento in
	// cima al file.

	CContainer* RequireActiveDocument(int line)
	{
		CContainer* doc = fEnv->GetActiveDocument();
		if (!doc)
			SetError("nessun foglio di calcolo collegato a questa esecuzione VBA", line);
		return doc;
	}

	Value EvalRangeFunction(const VbaExpr* expr)
	{
		if (expr->children.size() != 1)
		{
			SetError("Range richiede esattamente 1 argomento (un testo come \"A1\" o \"A1:B2\")",
				expr->line);
			return Value(0.0);
		}
		Value arg = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		std::string text = VbaToText(arg);
		range r;
		if (!ParseSimpleRangeRef(text, &r))
		{
			SetError("riferimento di intervallo non valido: \"" + text + "\"", expr->line);
			return Value(0.0);
		}
		CContainer* doc = RequireActiveDocument(expr->line);
		if (fFailed)
			return Value(0.0);
		return MakeRangeValue(r, doc);
	}

	Value EvalCellsFunction(const VbaExpr* expr)
	{
		if (expr->children.size() != 2)
		{
			SetError("Cells richiede esattamente 2 argomenti (riga, colonna)", expr->line);
			return Value(0.0);
		}
		Value rowValue = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		Value colValue = EvalNode(expr->children[1].get());
		if (fFailed)
			return Value(0.0);
		int row = (int)VbaToNumber(rowValue);
		int col = (int)VbaToNumber(colValue);
		cell c(col, row);
		if (!c.IsValid())
		{
			SetError("Cells(riga, colonna) e' fuori dai limiti del foglio di calcolo", expr->line);
			return Value(0.0);
		}
		CContainer* doc = RequireActiveDocument(expr->line);
		if (fFailed)
			return Value(0.0);
		return MakeRangeValue(range(c.h, c.v, c.h, c.v), doc);
	}

	// "target.Offset(righe, colonne)": sposta un intervallo esistente
	// di un numero di righe/colonne, conservando larghezza e altezza
	// -- stesso comportamento di range::OffsetBy, gia' usato altrove
	// nel motore (es. Fase 15, riferimenti di formula).
	Value EvalOffsetCall(const VbaExpr* expr)
	{
		Value targetValue = EvalNode(expr->target.get());
		if (fFailed)
			return Value(0.0);
		if (targetValue.fType != eRangeData)
		{
			SetError("Offset richiede un intervallo (Range/Cells), non un valore semplice", expr->line);
			return Value(0.0);
		}
		if (expr->children.size() != 2)
		{
			SetError("Offset richiede esattamente 2 argomenti (righe, colonne)", expr->line);
			return Value(0.0);
		}
		Value rowOffsetValue = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		Value colOffsetValue = EvalNode(expr->children[1].get());
		if (fFailed)
			return Value(0.0);

		range r = (range)targetValue;
		r.OffsetBy((int)VbaToNumber(colOffsetValue), (int)VbaToNumber(rowOffsetValue));
		if (!r.IsValid())
		{
			SetError("Offset porta fuori dai limiti del foglio di calcolo", expr->line);
			return Value(0.0);
		}
		return MakeRangeValue(r, targetValue.fRangeContainer);
	}

	Value EvalPropertyAccess(const VbaExpr* expr)
	{
		std::string name = Lowercase(expr->name);
		if (name == "select")
		{
			// No-op deliberato: nessun concetto di selezione a questo
			// livello (motore puro, nessuna UI collegata) -- vedi il
			// commento in VbaEvaluator.h. Si valuta comunque "target"
			// per propagare un eventuale errore (es. un riferimento
			// fuori dai limiti), ma il risultato si scarta sempre:
			// "Select" si usa solo come istruzione nuda.
			EvalNode(expr->target.get());
			return Value(0.0);
		}
		if (name == "value")
		{
			Value target = EvalNode(expr->target.get());
			if (fFailed)
				return Value(0.0);
			Value result;
			VbaRuntimeError err;
			if (!VbaReadRangeValue(target, &result, &err, expr->line))
			{
				SetError(err.message, err.line);
				return Value(0.0);
			}
			return result;
		}
		SetError("la proprieta'/il metodo \"" + expr->name + "\" del modello a oggetti Excel "
			"non e' ancora supportato in questa fase dell'interprete", expr->line);
		return Value(0.0);
	}
};

} // namespace

bool EvaluateVbaExpression(const VbaExpr* expr, VbaEnvironment* env, Value* outResult,
	VbaRuntimeError* outError)
{
	Evaluator eval(env, outError);
	return eval.Evaluate(expr, outResult);
}
