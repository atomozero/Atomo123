/*
	VbaStmtParser.cpp

	Vedi VbaStmtParser.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaStmtParser.h"

namespace {

class Parser {
public:
	Parser(const std::vector<VbaToken>& tokens, size_t& pos, VbaParseError* outError)
		: fTokens(tokens), fPos(pos), fError(outError), fFailed(false) {}

	std::unique_ptr<VbaSub> ParseSub()
	{
		if (!IsKeyword("Sub"))
		{
			SetError("atteso \"Sub\"");
			return nullptr;
		}
		int line = Current().line;
		Advance();
		if (Current().type != vbaTokIdentifier)
		{
			SetError("atteso un nome di procedura dopo \"Sub\"");
			return nullptr;
		}
		std::string name = Current().text;
		Advance();
		if (!ExpectOp("(") || !ExpectOp(")")) // nessun parametro in questo sottoinsieme
			return nullptr;
		if (!ExpectStatementEnd())
			return nullptr;

		auto sub = std::unique_ptr<VbaSub>(new VbaSub());
		sub->name = name;
		sub->line = line;

		if (!ParseStatementList(&sub->body))
			return nullptr;

		if (!ExpectKeyword("End") || !ExpectKeyword("Sub"))
			return nullptr;
		ExpectStatementEnd(); // facoltativo: puo' essere l'ultimo token del modulo

		return fFailed ? nullptr : std::move(sub);
	}

private:
	const std::vector<VbaToken>& fTokens;
	size_t& fPos;
	VbaParseError* fError;
	bool fFailed;

	const VbaToken& Current() const { return fTokens[fPos]; }
	void Advance() { if (fPos + 1 < fTokens.size()) fPos++; }
	bool IsKeyword(const char* text) const { return Current().type == vbaTokKeyword && Current().text == text; }
	bool IsOp(const char* text) const { return Current().type == vbaTokOperator && Current().text == text; }
	bool IsNewlineTok() const { return Current().type == vbaTokNewline; }
	bool IsEndTok() const { return Current().type == vbaTokEnd; }

	void SetError(const std::string& message)
	{
		if (fFailed)
			return; // mantiene il PRIMO errore incontrato
		fFailed = true;
		fError->message = message;
		fError->line = Current().line;
	}

	bool ExpectKeyword(const char* text)
	{
		if (!IsKeyword(text))
		{
			SetError(std::string("atteso \"") + text + "\"");
			return false;
		}
		Advance();
		return true;
	}

	bool ExpectOp(const char* text)
	{
		if (!IsOp(text))
		{
			SetError(std::string("atteso \"") + text + "\"");
			return false;
		}
		Advance();
		return true;
	}

	void SkipNewlines()
	{
		while (IsNewlineTok())
			Advance();
	}

	// Vero se il token corrente segnala la fine plausibile di un blocco
	// di istruzioni -- il chiamante specifico (ParseSub/ParseIf/
	// ParseFor) verifica poi se e' DAVVERO quello giusto per il
	// proprio contesto, producendo un errore chiaro se non lo e' (es.
	// un "If" senza "End If" prima di un "Next" del For che lo
	// contiene).
	bool IsBlockTerminator() const
	{
		if (Current().type != vbaTokKeyword)
			return false;
		return Current().text == "End" || Current().text == "Else"
			|| Current().text == "ElseIf" || Current().text == "Next";
	}

	// Una nuova riga O la fine del flusso chiudono un'istruzione in
	// modo pulito; trovarsi gia' su un terminatore di blocco (es.
	// "x = 1" seguito subito da "End Sub" sulla stessa riga logica, un
	// caso raro ma non invalido) va bene allo stesso modo.
	bool ExpectStatementEnd()
	{
		if (IsNewlineTok())
		{
			Advance();
			return true;
		}
		if (IsEndTok() || IsBlockTerminator())
			return true;
		SetError("fine istruzione attesa");
		return false;
	}

	bool ParseStatementList(std::vector<std::unique_ptr<VbaStmt>>* outStmts)
	{
		SkipNewlines();
		while (!fFailed && !IsBlockTerminator() && !IsEndTok())
		{
			auto stmt = ParseOneStatement();
			if (!stmt)
				return false;
			outStmts->push_back(std::move(stmt));
			SkipNewlines();
		}
		return !fFailed;
	}

	std::unique_ptr<VbaStmt> ParseOneStatement()
	{
		if (IsKeyword("Dim"))
			return ParseDim();
		if (IsKeyword("If"))
			return ParseIf();
		if (IsKeyword("For"))
			return ParseFor();
		if (IsKeyword("Set"))
			return ParseSetStmt();
		if (IsKeyword("Exit"))
			return ParseExit();
		return ParseAssignOrExprStmt();
	}

	std::unique_ptr<VbaStmt> ParseDim()
	{
		int line = Current().line;
		Advance(); // "Dim"
		if (Current().type != vbaTokIdentifier)
		{
			SetError("atteso un nome di variabile dopo \"Dim\"");
			return nullptr;
		}
		std::string varName = Current().text;
		Advance();
		if (IsKeyword("As"))
		{
			Advance();
			if (Current().type != vbaTokIdentifier)
			{
				SetError("atteso un nome di tipo dopo \"As\"");
				return nullptr;
			}
			Advance(); // il tipo si legge e si scarta -- vedi VbaStmt::kind == vbaStmtDim nell'header
		}
		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtDim));
		stmt->varName = varName;
		stmt->line = line;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	std::unique_ptr<VbaStmt> ParseSetStmt()
	{
		int line = Current().line;
		Advance(); // "Set"
		// Livello primario, non l'espressione completa: "=" e' ambiguo
		// in VBA fra assegnazione e confronto, vedi il commento in
		// VbaExprParser.h su ParseVbaPrimaryExpression.
		auto target = ParseVbaPrimaryExpression(fTokens, fPos, fError);
		if (!target)
		{
			fFailed = true;
			return nullptr;
		}
		if (!ExpectOp("="))
			return nullptr;
		auto value = ParseVbaExpression(fTokens, fPos, fError);
		if (!value)
		{
			fFailed = true;
			return nullptr;
		}
		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtSet));
		stmt->target = std::move(target);
		stmt->expr = std::move(value);
		stmt->line = line;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// Analizza UNA espressione; se seguita da "=", e' un'assegnazione
	// (il lato sinistro gia' analizzato diventa il target); altrimenti
	// e' una chiamata-istruzione nuda (es. "Range(\"A1\").Select").
	// Livello primario, non l'espressione completa, per lo stesso
	// motivo di ParseSetStmt sopra: "x = 1" non deve gia' consumare
	// "=" come operatore di confronto prima che questa funzione abbia
	// la possibilita' di riconoscere l'assegnazione.
	std::unique_ptr<VbaStmt> ParseAssignOrExprStmt()
	{
		int line = Current().line;
		auto expr = ParseVbaPrimaryExpression(fTokens, fPos, fError);
		if (!expr)
		{
			fFailed = true;
			return nullptr;
		}
		if (IsOp("="))
		{
			Advance();
			auto value = ParseVbaExpression(fTokens, fPos, fError);
			if (!value)
			{
				fFailed = true;
				return nullptr;
			}
			auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtAssign));
			stmt->target = std::move(expr);
			stmt->expr = std::move(value);
			stmt->line = line;
			if (!ExpectStatementEnd())
				return nullptr;
			return stmt;
		}
		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtExprStmt));
		stmt->expr = std::move(expr);
		stmt->line = line;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// Il token corrente deve essere "If" (chiamato dal livello piu'
	// esterno o da ParseStatementList) OPPURE "ElseIf" (chiamato in
	// ricorsione da se stesso per una catena di ElseIf, vedi sotto) --
	// la sintassi del resto (condizione, "Then", corpo) e' identica in
	// entrambi i casi. Solo il livello aperto da "If" consuma "End If"
	// per davvero alla fine: ogni "ElseIf" annidato lascia che sia
	// quello piu' esterno a farlo, cosi' "End If" chiude l'intera
	// catena una volta sola, qualunque sia la sua lunghezza.
	std::unique_ptr<VbaStmt> ParseIf()
	{
		bool isElseIf = IsKeyword("ElseIf");
		int line = Current().line;
		Advance(); // "If" oppure "ElseIf"

		auto cond = ParseVbaExpression(fTokens, fPos, fError);
		if (!cond)
		{
			fFailed = true;
			return nullptr;
		}
		if (!ExpectKeyword("Then"))
			return nullptr;

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtIf));
		stmt->expr = std::move(cond);
		stmt->line = line;

		if (!IsNewlineTok() && !IsEndTok() && !IsBlockTerminator())
		{
			// Forma breve su una sola riga: "If cond Then stmt", mai
			// "End If" per questa forma.
			auto single = ParseOneStatement();
			if (!single)
				return nullptr;
			stmt->body.push_back(std::move(single));
			return stmt;
		}

		if (!ParseStatementList(&stmt->body))
			return nullptr;

		if (IsKeyword("ElseIf"))
		{
			// Ricorsione: gestisce il resto della catena (un altro
			// ElseIf, o il vero Else finale), ma NON "End If" -- ogni
			// livello annidato ha isElseIf=true, quindi salta il blocco
			// "if (!isElseIf)" sotto. Solo il livello PIU' ESTERNO
			// (aperto da "If", non da "ElseIf") arriva fin li' e
			// consuma "End If" per davvero, qualunque sia la lunghezza
			// della catena -- per questo niente "return" qui dentro,
			// si cade attraverso fino al blocco condiviso in fondo.
			auto nestedIf = ParseIf();
			if (!nestedIf)
				return nullptr;
			stmt->elseBody.push_back(std::move(nestedIf));
		}
		else if (IsKeyword("Else"))
		{
			Advance();
			if (!ParseStatementList(&stmt->elseBody))
				return nullptr;
		}

		if (!isElseIf)
		{
			if (!ExpectKeyword("End") || !ExpectKeyword("If"))
				return nullptr;
			if (!ExpectStatementEnd())
				return nullptr;
		}
		return stmt;
	}

	std::unique_ptr<VbaStmt> ParseFor()
	{
		int line = Current().line;
		Advance(); // "For"
		if (Current().type != vbaTokIdentifier)
		{
			SetError("atteso un nome di variabile dopo \"For\"");
			return nullptr;
		}
		std::string varName = Current().text;
		Advance();
		if (!ExpectOp("="))
			return nullptr;
		auto startExpr = ParseVbaExpression(fTokens, fPos, fError);
		if (!startExpr)
		{
			fFailed = true;
			return nullptr;
		}
		if (!ExpectKeyword("To"))
			return nullptr;
		auto endExpr = ParseVbaExpression(fTokens, fPos, fError);
		if (!endExpr)
		{
			fFailed = true;
			return nullptr;
		}
		std::unique_ptr<VbaExpr> stepExpr;
		if (IsKeyword("Step"))
		{
			Advance();
			stepExpr = ParseVbaExpression(fTokens, fPos, fError);
			if (!stepExpr)
			{
				fFailed = true;
				return nullptr;
			}
		}

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtFor));
		stmt->varName = varName;
		stmt->expr = std::move(startExpr);
		stmt->forEnd = std::move(endExpr);
		stmt->forStep = std::move(stepExpr);
		stmt->line = line;

		if (!ParseStatementList(&stmt->body))
			return nullptr;

		if (!ExpectKeyword("Next"))
			return nullptr;
		if (Current().type == vbaTokIdentifier)
			Advance(); // "Next i" -- il nome dopo Next e' facoltativo, letto e scartato
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	std::unique_ptr<VbaStmt> ParseExit()
	{
		int line = Current().line;
		Advance(); // "Exit"
		VbaStmtKind kind;
		if (IsKeyword("For"))
		{
			kind = vbaStmtExitFor;
			Advance();
		}
		else if (IsKeyword("Sub"))
		{
			kind = vbaStmtExitSub;
			Advance();
		}
		else
		{
			SetError("atteso \"For\" o \"Sub\" dopo \"Exit\"");
			return nullptr;
		}
		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(kind));
		stmt->line = line;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}
};

} // namespace

std::unique_ptr<VbaSub> ParseVbaSub(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError)
{
	Parser parser(tokens, pos, outError);
	std::unique_ptr<VbaSub> result = parser.ParseSub();
	if (!result && outError->message.empty())
		outError->message = "errore di sintassi";
	return result;
}
