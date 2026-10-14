/*
	VbaStmtParser.cpp

	Vedi VbaStmtParser.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaStmtParser.h"

#include <cctype>

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

		std::vector<std::string> params;
		if (!ParseParamList(&params))
			return nullptr;
		if (!ExpectStatementEnd())
			return nullptr;

		auto sub = std::unique_ptr<VbaSub>(new VbaSub());
		sub->name = name;
		sub->params = params;
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
	// Guarda il token SUCCESSIVO senza consumarlo -- serve solo per
	// distinguere "For Each" da un "For" normale appena dopo aver
	// visto "For" (ParseOneStatement), senza dover rianalizzare da
	// capo.
	bool NextIsKeyword(const char* text) const
	{
		return fPos + 1 < fTokens.size() && fTokens[fPos + 1].type == vbaTokKeyword
			&& fTokens[fPos + 1].text == text;
	}
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

	static bool EqualsNoCase(const std::string& a, const char* b)
	{
		size_t i = 0;
		for (; a[i] && b[i]; i++)
		{
			if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
				return false;
		}
		return a[i] == b[i]; // entrambi finiti alla stessa posizione
	}

	// Fase 6a: elenco parametri di una Sub, "(" [ [ByVal] nome [As
	// Tipo] ("," [ByVal] nome [As Tipo])* ] ")" -- SOLO per valore in
	// questo sottoinsieme (vedi VbaSub::params in VbaAst.h): "ByVal" e'
	// un semplice identificatore per questo lessico (non una parola
	// chiave), si legge e si scarta se presente; "ByRef" e' invece un
	// errore di parsing esplicito e chiaro, non trattato silenziosamente
	// come ByVal -- l'aliasing col chiamante che richiederebbe non e'
	// supportato da questo interprete. Il tipo dopo "As" (se presente)
	// si legge e si scarta, stesso trattamento di "Dim".
	bool ParseParamList(std::vector<std::string>* outParams)
	{
		if (!ExpectOp("("))
			return false;
		if (IsOp(")"))
		{
			Advance();
			return true;
		}
		for (;;)
		{
			if (Current().type == vbaTokIdentifier && EqualsNoCase(Current().text, "byref"))
			{
				SetError("\"ByRef\" non e' supportato in questo sottoinsieme "
					"(solo il passaggio per valore, il default implicito -- vedi ROADMAP.md)");
				return false;
			}
			if (Current().type == vbaTokIdentifier && EqualsNoCase(Current().text, "byval"))
				Advance();
			if (Current().type != vbaTokIdentifier)
			{
				SetError("atteso un nome di parametro");
				return false;
			}
			outParams->push_back(Current().text);
			Advance();
			if (IsKeyword("As"))
			{
				Advance();
				if (Current().type != vbaTokIdentifier)
				{
					SetError("atteso un nome di tipo dopo \"As\"");
					return false;
				}
				Advance();
			}
			if (IsOp(","))
			{
				Advance();
				continue;
			}
			break;
		}
		return ExpectOp(")");
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
			|| Current().text == "ElseIf" || Current().text == "Next"
			|| Current().text == "Case" || Current().text == "Loop";
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
			return NextIsKeyword("Each") ? ParseForEach() : ParseFor();
		if (IsKeyword("Set"))
			return ParseSetStmt();
		if (IsKeyword("Exit"))
			return ParseExit();
		if (IsKeyword("Select"))
			return ParseSelectCase();
		if (IsKeyword("Do"))
			return ParseDo();
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
		// Fase 6a: "Call NomeProcedura(argomenti)" e' solo zucchero
		// sintattico del vero VBA sopra la stessa identica forma di
		// "NomeProcedura(argomenti)" usata come istruzione nuda (vedi
		// vbaStmtExprStmt) -- "Call" si legge e si scarta, nessun nodo
		// dedicato nell'albero.
		if (IsKeyword("Call"))
			Advance();
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

	// "For Each varName In expr" ... "Next [varName]" -- il token
	// corrente e' gia' "For" (NON ancora consumato, a differenza di
	// ParseFor: il chiamante ha solo GUARDATO avanti con
	// NextIsKeyword, mai avanzato). "expr" deve valutare a un
	// intervallo (Range/Cells) a runtime, verificato dall'esecutore,
	// non qui -- questo sottoinsieme non ha array/Collection generici
	// da iterare.
	std::unique_ptr<VbaStmt> ParseForEach()
	{
		int line = Current().line;
		Advance(); // "For"
		Advance(); // "Each"
		if (Current().type != vbaTokIdentifier)
		{
			SetError("atteso un nome di variabile dopo \"For Each\"");
			return nullptr;
		}
		std::string varName = Current().text;
		Advance();
		if (!ExpectKeyword("In"))
			return nullptr;

		auto collectionExpr = ParseVbaExpression(fTokens, fPos, fError);
		if (!collectionExpr)
		{
			fFailed = true;
			return nullptr;
		}

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtForEach));
		stmt->varName = varName;
		stmt->expr = std::move(collectionExpr);
		stmt->line = line;

		if (!ParseStatementList(&stmt->body))
			return nullptr;

		if (!ExpectKeyword("Next"))
			return nullptr;
		if (Current().type == vbaTokIdentifier)
			Advance(); // "Next cell" -- facoltativo, come nel For normale
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// "Select Case expr" seguito da zero o piu' "Case v1, v2, ..."
	// (confronto per sola uguaglianza con "expr", vedi il commento su
	// vbaStmtSelectCase in VbaAst.h) e un "Case Else" facoltativo,
	// SEMPRE l'ultimo -- un "Case" dopo un "Case Else" e' un errore di
	// sintassi, come nel vero VBA.
	std::unique_ptr<VbaStmt> ParseSelectCase()
	{
		int line = Current().line;
		Advance(); // "Select"
		if (!ExpectKeyword("Case"))
			return nullptr;

		auto testExpr = ParseVbaExpression(fTokens, fPos, fError);
		if (!testExpr)
		{
			fFailed = true;
			return nullptr;
		}
		if (!ExpectStatementEnd())
			return nullptr;

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtSelectCase));
		stmt->expr = std::move(testExpr);
		stmt->line = line;

		bool sawCaseElse = false;
		while (IsKeyword("Case"))
		{
			int caseLine = Current().line;
			Advance(); // "Case"

			if (sawCaseElse)
			{
				SetError("\"Case\" dopo \"Case Else\" non e' consentito "
					"(\"Case Else\" deve essere l'ultimo)");
				return nullptr;
			}

			if (IsKeyword("Else"))
			{
				Advance();
				sawCaseElse = true;
				if (!ExpectStatementEnd())
					return nullptr;
				if (!ParseStatementList(&stmt->elseBody))
					return nullptr;
				continue;
			}

			if (IsKeyword("To") || IsKeyword("Is"))
			{
				// "Is" non e' nemmeno una parola chiave di questo
				// lessico (tokenizza come un identificatore), quindi
				// questo ramo intercetta solo "Case To ..." (un errore
				// comunque) -- il vero "Case Is > 5"/"Case 1 To 5" e'
				// gestito dal messaggio sotto, dopo aver letto almeno
				// un valore, per un errore piu' chiaro.
				SetError("\"Case To\"/\"Case Is\" (un intervallo o un confronto) non e' "
					"supportato in questa fase: solo un elenco di valori separati da virgola");
				return nullptr;
			}

			VbaCaseClause clause;
			clause.line = caseLine;
			for (;;)
			{
				auto value = ParseVbaExpression(fTokens, fPos, fError);
				if (!value)
				{
					fFailed = true;
					return nullptr;
				}
				clause.matchValues.push_back(std::move(value));

				if (IsKeyword("To") || (Current().type == vbaTokIdentifier && Current().text == "Is"))
				{
					SetError("\"Case 1 To 5\"/\"Case Is > 5\" (un intervallo o un confronto) "
						"non e' supportato in questa fase: solo un elenco di valori separati "
						"da virgola");
					return nullptr;
				}
				if (IsOp(","))
				{
					Advance();
					continue;
				}
				break;
			}
			if (!ExpectStatementEnd())
				return nullptr;
			if (!ParseStatementList(&clause.body))
				return nullptr;
			stmt->cases.push_back(std::move(clause));
		}

		if (!ExpectKeyword("End") || !ExpectKeyword("Select"))
			return nullptr;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// "Do [While|Until expr]" ... "Loop [While|Until expr]" -- al
	// massimo UNA delle due posizioni ha davvero una condizione
	// (entrambe insieme sulla stessa istruzione e' un errore di
	// sintassi, non un'ambiguita' risolta a caso scegliendone una).
	// Nessuna delle due: un "Do...Loop" nudo, esce solo con
	// "Exit Do"/"Exit Sub".
	std::unique_ptr<VbaStmt> ParseDo()
	{
		int line = Current().line;
		Advance(); // "Do"

		bool hasPreCond = false, preIsUntil = false;
		std::unique_ptr<VbaExpr> preCond;
		if (IsKeyword("While") || IsKeyword("Until"))
		{
			hasPreCond = true;
			preIsUntil = IsKeyword("Until");
			Advance();
			preCond = ParseVbaExpression(fTokens, fPos, fError);
			if (!preCond)
			{
				fFailed = true;
				return nullptr;
			}
		}
		if (!ExpectStatementEnd())
			return nullptr;

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtDoLoop));
		stmt->line = line;

		if (!ParseStatementList(&stmt->body))
			return nullptr;

		if (!ExpectKeyword("Loop"))
			return nullptr;

		bool hasPostCond = false, postIsUntil = false;
		std::unique_ptr<VbaExpr> postCond;
		if (IsKeyword("While") || IsKeyword("Until"))
		{
			hasPostCond = true;
			postIsUntil = IsKeyword("Until");
			Advance();
			postCond = ParseVbaExpression(fTokens, fPos, fError);
			if (!postCond)
			{
				fFailed = true;
				return nullptr;
			}
		}
		if (!ExpectStatementEnd())
			return nullptr;

		if (hasPreCond && hasPostCond)
		{
			SetError("una condizione sia su \"Do While/Until\" che su \"Loop While/Until\" "
				"nella stessa istruzione non e' supportato in questa fase: scegline una sola "
				"posizione");
			return nullptr;
		}

		if (hasPreCond)
		{
			stmt->doIsPreTest = true;
			stmt->doIsUntil = preIsUntil;
			stmt->expr = std::move(preCond);
		}
		else if (hasPostCond)
		{
			stmt->doIsPreTest = false;
			stmt->doIsUntil = postIsUntil;
			stmt->expr = std::move(postCond);
		}
		// altrimenti: "Do...Loop" nudo, stmt->expr resta nullptr (vedi
		// il commento su vbaStmtDoLoop in VbaAst.h).
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
		else if (IsKeyword("Do"))
		{
			kind = vbaStmtExitDo;
			Advance();
		}
		else
		{
			SetError("atteso \"For\", \"Do\" o \"Sub\" dopo \"Exit\"");
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
