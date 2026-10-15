/*
	VbaExprParser.cpp

	Vedi VbaExprParser.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaExprParser.h"

namespace {

class Parser {
public:
	Parser(const std::vector<VbaToken>& tokens, size_t& pos, VbaParseError* outError)
		: fTokens(tokens), fPos(pos), fError(outError), fFailed(false) {}

	std::unique_ptr<VbaExpr> ParseTop()
	{
		return ParseXor(); // "fPos" (riferimento al "pos" del chiamante) e' gia' aggiornato in place a ogni Advance()
	}

	// Punto di ingresso pubblico per ParseVbaPrimaryExpression, vedi il
	// commento in VbaExprParser.h sul perche' serve un livello separato
	// da ParseTop() (nessun consumo di "=" come operatore di confronto).
	std::unique_ptr<VbaExpr> ParsePrimaryTop()
	{
		return ParsePrimary();
	}

private:
	const std::vector<VbaToken>& fTokens;
	size_t& fPos;
	VbaParseError* fError;
	bool fFailed;

	const VbaToken& Current() const { return fTokens[fPos]; }

	void Advance()
	{
		if (fPos + 1 < fTokens.size())
			fPos++;
	}

	bool IsOp(const char* text) const
	{
		return Current().type == vbaTokOperator && Current().text == text;
	}

	bool IsKeyword(const char* text) const
	{
		return Current().type == vbaTokKeyword && Current().text == text;
	}

	void SetError(const std::string& message)
	{
		if (fFailed)
			return; // mantiene il PRIMO errore incontrato, piu' utile di uno a cascata
		fFailed = true;
		fError->message = message;
		fError->line = Current().line;
	}

	bool Expect(const char* opText)
	{
		if (!IsOp(opText))
		{
			SetError(std::string("atteso \"") + opText + "\"");
			return false;
		}
		Advance();
		return true;
	}

	std::unique_ptr<VbaExpr> MakeNode(VbaExprKind kind)
	{
		auto node = std::unique_ptr<VbaExpr>(new VbaExpr(kind));
		node->line = Current().line;
		return node;
	}

	// ---- livelli di precedenza, dal piu' debole al piu' forte ----

	// Fase 6g (seguito): "Xor" e' bit a bit come "And"/"Or" (vedi
	// VbaEvaluator.cpp), ma si lega PIU' DEBOLMENTE di entrambi nel vero
	// VBA (ordine: Not, And, Or, Xor, dal piu' forte al piu' debole) --
	// un livello a parte SOPRA ParseOr, non dentro, cosi' "a And b Xor c"
	// si legge come "(a And b) Xor c", non "a And (b Xor c)".
	std::unique_ptr<VbaExpr> ParseXor()
	{
		auto left = ParseOr();
		if (!left)
			return nullptr;
		while (IsKeyword("Xor"))
		{
			Advance();
			auto right = ParseOr();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = "Xor";
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	std::unique_ptr<VbaExpr> ParseOr()
	{
		auto left = ParseAnd();
		if (!left)
			return nullptr;
		while (IsKeyword("Or"))
		{
			Advance();
			auto right = ParseAnd();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = "Or";
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	std::unique_ptr<VbaExpr> ParseAnd()
	{
		auto left = ParseNot();
		if (!left)
			return nullptr;
		while (IsKeyword("And"))
		{
			Advance();
			auto right = ParseNot();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = "And";
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	std::unique_ptr<VbaExpr> ParseNot()
	{
		if (IsKeyword("Not"))
		{
			int line = Current().line;
			Advance();
			auto operand = ParseComparison();
			if (!operand)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprUnary));
			node->op = "Not";
			node->line = line;
			node->children.push_back(std::move(operand));
			return node;
		}
		return ParseComparison();
	}

	static bool IsComparisonOpText(const std::string& s)
	{
		return s == "=" || s == "<>" || s == "<" || s == ">" || s == "<=" || s == ">=";
	}

	std::unique_ptr<VbaExpr> ParseComparison()
	{
		auto left = ParseConcat();
		if (!left)
			return nullptr;
		while (Current().type == vbaTokOperator && IsComparisonOpText(Current().text))
		{
			std::string op = Current().text;
			Advance();
			auto right = ParseConcat();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = op;
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	std::unique_ptr<VbaExpr> ParseConcat()
	{
		auto left = ParseAdditive();
		if (!left)
			return nullptr;
		while (IsOp("&"))
		{
			Advance();
			auto right = ParseAdditive();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = "&";
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	std::unique_ptr<VbaExpr> ParseAdditive()
	{
		auto left = ParseMod();
		if (!left)
			return nullptr;
		while (IsOp("+") || IsOp("-"))
		{
			std::string op = Current().text;
			Advance();
			auto right = ParseMod();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = op;
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	std::unique_ptr<VbaExpr> ParseMod()
	{
		auto left = ParseIntDiv();
		if (!left)
			return nullptr;
		while (IsKeyword("Mod"))
		{
			Advance();
			auto right = ParseIntDiv();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = "Mod";
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	// Fase 6g (seguito): "\" e' la divisione INTERA di VBA (diversa da
	// "/", vera divisione) -- si lega PIU' DEBOLMENTE di "*"/"/" ma PIU'
	// FORTEMENTE di "Mod" nel vero VBA (ordine: * e /, poi \, poi Mod),
	// trovata controllando il vero sorgente chip-8-vba (estrae bit da
	// un'istruzione a 16 bit, es. "(instruction And &HF00&) \ 256").
	std::unique_ptr<VbaExpr> ParseIntDiv()
	{
		auto left = ParseMultiplicative();
		if (!left)
			return nullptr;
		while (IsOp("\\"))
		{
			Advance();
			auto right = ParseMultiplicative();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = "\\";
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	std::unique_ptr<VbaExpr> ParseMultiplicative()
	{
		auto left = ParseUnary();
		if (!left)
			return nullptr;
		while (IsOp("*") || IsOp("/"))
		{
			std::string op = Current().text;
			Advance();
			auto right = ParseUnary();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = op;
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			left = std::move(node);
		}
		return left;
	}

	// Il meno unario si lega PIU' DEBOLMENTE dell'elevamento a potenza
	// in VBA (una particolarita' nota, diversa dalla maggior parte dei
	// linguaggi): "-2^2" vale -4, non 4 -- ParseUnary avvolge
	// ParseExponent, non il contrario, cosi' "-2^2" si legge come
	// "-(2^2)".
	std::unique_ptr<VbaExpr> ParseUnary()
	{
		if (IsOp("-"))
		{
			int line = Current().line;
			Advance();
			auto operand = ParseExponent();
			if (!operand)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprUnary));
			node->op = "-";
			node->line = line;
			node->children.push_back(std::move(operand));
			return node;
		}
		return ParseExponent();
	}

	// Associativo a destra, e il lato destro PUO' avere un proprio meno
	// unario ("2^-2" e' valido: 2 elevato a -2).
	std::unique_ptr<VbaExpr> ParseExponent()
	{
		auto left = ParsePrimary();
		if (!left)
			return nullptr;
		if (IsOp("^"))
		{
			Advance();
			auto right = ParseUnary();
			if (!right)
				return nullptr;
			auto node = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprBinary));
			node->op = "^";
			node->line = left->line;
			node->children.push_back(std::move(left));
			node->children.push_back(std::move(right));
			return node;
		}
		return left;
	}

	// Legge gli argomenti di una chiamata APERTA (il "(" e' gia' stato
	// consumato dal chiamante) fino al ")" corrispondente, che QUESTA
	// funzione consuma.
	bool ParseArgumentList(std::vector<std::unique_ptr<VbaExpr>>* outArgs)
	{
		if (IsOp(")"))
		{
			Advance();
			return true;
		}
		while (true)
		{
			auto arg = ParseXor();
			if (!arg)
				return false;
			outArgs->push_back(std::move(arg));
			if (IsOp(","))
			{
				Advance();
				continue;
			}
			break;
		}
		return Expect(")");
	}

	// "target.Nome" o "target.Nome(argomenti)", incatenabile
	// (Range("A1").Offset(1,0).Value) -- "base" e' gia' stato
	// analizzato dal chiamante, qui si leggono zero o piu' ".Nome[...]"
	// che seguono.
	std::unique_ptr<VbaExpr> ParsePostfix(std::unique_ptr<VbaExpr> base)
	{
		while (IsOp("."))
		{
			Advance();
			if (Current().type != vbaTokIdentifier && Current().type != vbaTokKeyword)
			{
				SetError("atteso un nome di proprieta'/metodo dopo \".\"");
				return nullptr;
			}
			std::string propName = Current().text;
			int line = Current().line;
			Advance();

			if (IsOp("("))
			{
				Advance();
				auto call = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprCall));
				call->name = propName;
				call->line = line;
				call->target = std::move(base);
				if (!ParseArgumentList(&call->children))
					return nullptr;
				base = std::move(call);
			}
			else
			{
				auto access = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprPropertyAccess));
				access->name = propName;
				access->line = line;
				access->target = std::move(base);
				base = std::move(access);
			}
		}
		return base;
	}

	std::unique_ptr<VbaExpr> ParsePrimary()
	{
		const VbaToken& tok = Current();

		if (tok.type == vbaTokOperator && tok.text == ".")
		{
			// Fase 6e: ".Nome" dentro un "With" -- un segnaposto senza
			// nome (vbaExprWithImplicitTarget, risolto a RUNTIME in
			// base al "With" piu' vicino, vedi VbaAst.h) usato come
			// "base" di ParsePostfix esattamente come un qualunque
			// altro primario: il "." corrente non si consuma qui,
			// resta per il ciclo "while (IsOp(\".\"))" di ParsePostfix,
			// che lo tratta come la prima (e in questo caso unica)
			// propieta'/chiamata della catena.
			auto marker = MakeNode(vbaExprWithImplicitTarget);
			return ParsePostfix(std::move(marker));
		}

		if (tok.type == vbaTokNumber)
		{
			auto node = MakeNode(vbaExprNumber);
			node->numberValue = tok.numberValue;
			Advance();
			return ParsePostfix(std::move(node));
		}
		if (tok.type == vbaTokString)
		{
			auto node = MakeNode(vbaExprString);
			node->stringValue = tok.stringValue;
			Advance();
			return ParsePostfix(std::move(node));
		}
		if (tok.type == vbaTokKeyword && (tok.text == "True" || tok.text == "False"))
		{
			auto node = MakeNode(vbaExprBool);
			node->boolValue = (tok.text == "True");
			Advance();
			return node;
		}
		if (tok.type == vbaTokOperator && tok.text == "(")
		{
			Advance();
			auto inner = ParseXor();
			if (!inner)
				return nullptr;
			if (!Expect(")"))
				return nullptr;
			return ParsePostfix(std::move(inner));
		}
		if (tok.type == vbaTokIdentifier)
		{
			std::string name = tok.text;
			int line = tok.line;
			Advance();

			std::unique_ptr<VbaExpr> node;
			if (IsOp("("))
			{
				Advance();
				auto call = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprCall));
				call->name = name;
				call->line = line;
				if (!ParseArgumentList(&call->children))
					return nullptr;
				node = std::move(call);
			}
			else
			{
				auto var = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprVariable));
				var->name = name;
				var->line = line;
				node = std::move(var);
			}
			return ParsePostfix(std::move(node));
		}

		SetError("atteso un numero, una stringa, un identificatore o \"(\"");
		return nullptr;
	}
};

} // namespace

std::unique_ptr<VbaExpr> ParseVbaExpression(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError)
{
	Parser parser(tokens, pos, outError);
	std::unique_ptr<VbaExpr> result = parser.ParseTop();
	if (!result && outError->message.empty())
		outError->message = "errore di sintassi";
	return result;
}

std::unique_ptr<VbaExpr> ParseVbaPrimaryExpression(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError)
{
	Parser parser(tokens, pos, outError);
	std::unique_ptr<VbaExpr> result = parser.ParsePrimaryTop();
	if (!result && outError->message.empty())
		outError->message = "errore di sintassi";
	return result;
}
