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

	// Fase 6b: "Sub Nome(...)" ... "End Sub" oppure "Function
	// Nome(...) [As Tipo]" ... "End Function" -- stessa struttura,
	// solo la parola chiave (e se c'e' un tipo di ritorno da leggere e
	// scartare) cambia in base a "isFunction" -- vedi VbaSub in
	// VbaAst.h sul perche' non serve altro per il valore di ritorno.
	std::unique_ptr<VbaSub> ParseSub()
	{
		bool isFunction = IsKeyword("Function");
		if (!isFunction && !IsKeyword("Sub"))
		{
			SetError("atteso \"Sub\" o \"Function\"");
			return nullptr;
		}
		int line = Current().line;
		Advance();
		if (Current().type != vbaTokIdentifier)
		{
			SetError(std::string("atteso un nome di procedura dopo \"")
				+ (isFunction ? "Function" : "Sub") + "\"");
			return nullptr;
		}
		std::string name = Current().text;
		Advance();

		std::vector<std::string> params;
		if (!ParseParamList(&params))
			return nullptr;
		if (isFunction && IsKeyword("As"))
		{
			Advance();
			if (Current().type != vbaTokIdentifier)
			{
				SetError("atteso un nome di tipo dopo \"As\"");
				return nullptr;
			}
			Advance();
		}
		if (!ExpectStatementEnd())
			return nullptr;

		auto sub = std::unique_ptr<VbaSub>(new VbaSub());
		sub->name = name;
		sub->params = params;
		sub->isFunction = isFunction;
		sub->line = line;

		if (!ParseStatementList(&sub->body))
			return nullptr;

		if (!ExpectKeyword("End") || !ExpectKeyword(isFunction ? "Function" : "Sub"))
			return nullptr;
		ExpectStatementEnd(); // facoltativo: puo' essere l'ultimo token del modulo

		return fFailed ? nullptr : std::move(sub);
	}

	// Fase 6d: "Type Nome" ... "End Type" -- un elenco di campi, MAI
	// codice eseguibile (niente corpo di istruzioni, a differenza di
	// ParseSub sopra). Il chiamante (VbaMacroRunner.h) si aspetta di
	// essere gia' posizionato sulla parola chiave "Type" -- un
	// eventuale "Private"/"Public" davanti e' un semplice
	// identificatore per questo lessico, letto e scartato DAL
	// CHIAMANTE durante la scansione del modulo, stesso trattamento di
	// "Private Sub".
	std::unique_ptr<VbaTypeDecl> ParseTypeDecl()
	{
		if (!ExpectKeyword("Type"))
			return nullptr;
		if (Current().type != vbaTokIdentifier)
		{
			SetError("atteso un nome di tipo dopo \"Type\"");
			return nullptr;
		}
		auto decl = std::unique_ptr<VbaTypeDecl>(new VbaTypeDecl());
		decl->name = Current().text;
		Advance();
		if (!ExpectStatementEnd())
			return nullptr;

		while (!IsKeyword("End"))
		{
			if (IsEndTok())
			{
				SetError("\"End Type\" mancante");
				return nullptr;
			}
			if (IsNewlineTok())
			{
				Advance(); // riga vuota tra due campi
				continue;
			}
			if (Current().type != vbaTokIdentifier)
			{
				SetError("atteso un nome di campo o \"End Type\"");
				return nullptr;
			}
			VbaTypeField field;
			field.name = Current().text;
			Advance();
			if (!ParseArrayDimList(&field.arrayDims))
				return nullptr;
			if (!ExpectKeyword("As"))
				return nullptr;
			if (Current().type != vbaTokIdentifier)
			{
				SetError("atteso un nome di tipo dopo \"As\"");
				return nullptr;
			}
			field.typeName = Current().text;
			Advance();
			if (!ExpectStatementEnd())
				return nullptr;
			decl->fields.push_back(field);
		}
		if (!ExpectKeyword("End") || !ExpectKeyword("Type"))
			return nullptr;
		ExpectStatementEnd(); // facoltativo: puo' essere l'ultimo token del modulo

		return fFailed ? nullptr : std::move(decl);
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
			|| Current().text == "Case" || Current().text == "Loop"
			|| Current().text == "Wend";
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
			// "Dim" e' l'unica istruzione che puo' espandersi in PIU' di
			// un VbaStmt (un "Dim a, b, c" con piu' dichiaratori) --
			// gestita qui direttamente invece che tramite
			// ParseOneStatement, che ha il contratto di restituire
			// esattamente un nodo.
			if (IsKeyword("Dim"))
			{
				if (!ParseDimStatementList(outStmts))
					return false;
				SkipNewlines();
				continue;
			}
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
		if (IsKeyword("While"))
			return ParseWhileWend();
		if (IsKeyword("With"))
			return ParseWith();
		if (IsKeyword("Open"))
			return ParseOpen();
		if (IsKeyword("Close"))
			return ParseClose();
		if (IsKeyword("Get"))
			return ParseGet();
		if (IsKeyword("ReDim"))
			return ParseReDim();
		// Fase 6g: "MsgBox expr" -- un caso UNICO, hardcoded (non una
		// generalizzazione della chiamata-istruzione senza parentesi,
		// che resta fuori scopo, vedi il commento su "Call" piu' sopra
		// nel file): serve solo a far analizzare per intero il ramo di
		// annullamento del vero trigger macro del CHIP-8
		// ("Application.GetOpenFilename" + "MsgBox" + "Exit Sub"),
		// vedi ROADMAP.md.
		if (Current().type == vbaTokIdentifier && EqualsNoCase(Current().text, "msgbox"))
			return ParseMsgBox();
		return ParseAssignOrExprStmt();
	}

	// Fase 6e: "With expr" ... "End With" -- "expr" e' il target, mai
	// ri-valutato (vedi VbaStmtWith in VbaAst.h): il parser NON deve
	// sapere nulla sul contesto "With" attivo, la risoluzione di ogni
	// ".Nome" dentro "body" avviene interamente a runtime
	// (VbaEvaluator.cpp/VbaExecutor.cpp).
	std::unique_ptr<VbaStmt> ParseWith()
	{
		int line = Current().line;
		Advance(); // "With"
		auto target = ParseVbaExpression(fTokens, fPos, fError);
		if (!target)
		{
			fFailed = true;
			return nullptr;
		}
		if (!ExpectStatementEnd())
			return nullptr;

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtWith));
		stmt->target = std::move(target);
		stmt->line = line;

		if (!ParseStatementList(&stmt->body))
			return nullptr;

		if (!ExpectKeyword("End") || !ExpectKeyword("With"))
			return nullptr;
		ExpectStatementEnd(); // facoltativo: puo' essere l'ultimo token del modulo

		return fFailed ? nullptr : std::move(stmt);
	}

	// Un SOLO dichiaratore "nome[(limiti)] [As Tipo]" -- il "Dim" stesso
	// e' gia' stato consumato dal chiamante, "line" e' la riga di
	// QUELLA parola chiave (cosi' ogni dichiaratore di un "Dim a, b, c"
	// riporta la stessa riga, come il vero VBA tratterebbe un errore
	// su una qualunque di esse). Riusato sia da ParseDim (un solo
	// dichiaratore, per i contesti a istruzione singola come un "If"
	// su una riga) sia da ParseDimStatementList sotto (piu' di uno).
	std::unique_ptr<VbaStmt> ParseOneDimDeclarator(int line)
	{
		if (Current().type != vbaTokIdentifier)
		{
			SetError("atteso un nome di variabile dopo \"Dim\"");
			return nullptr;
		}
		std::string varName = Current().text;
		Advance();

		std::vector<int> arrayDims;
		if (!ParseArrayDimList(&arrayDims))
			return nullptr;

		std::string asTypeName;
		if (IsKeyword("As"))
		{
			Advance();
			if (Current().type != vbaTokIdentifier)
			{
				SetError("atteso un nome di tipo dopo \"As\"");
				return nullptr;
			}
			asTypeName = Current().text; // Fase 6d: NON si scarta piu' -- vedi VbaStmt::asTypeName
			Advance();
		}
		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtDim));
		stmt->varName = varName;
		stmt->arrayDims = arrayDims;
		stmt->asTypeName = asTypeName;
		stmt->line = line;
		return stmt;
	}

	// Un "Dim" con un SOLO dichiaratore -- usato dove il resto della
	// grammatica si aspetta una singola istruzione (es. il corpo di un
	// "If" su una riga, "If x Then Dim a"): il vero VBA non permette
	// comunque una lista multipla li', quindi non e' un limite in
	// piu' di questo sottoinsieme.
	std::unique_ptr<VbaStmt> ParseDim()
	{
		int line = Current().line;
		Advance(); // "Dim"
		auto stmt = ParseOneDimDeclarator(line);
		if (!stmt)
			return nullptr;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// "Dim nome1[(limiti)] [As Tipo1], nome2[(limiti)] [As Tipo2], ..."
	// -- un'istruzione sola nel vero VBA, ma PIU' di un VbaStmt qui
	// (nessun nodo "lista" dedicato: ogni dichiaratore diventa un
	// vbaStmtDim indipendente, appeso direttamente a "outStmts"), dato
	// che il resto dell'interprete (VbaExecutor.cpp) tratta ogni "Dim"
	// come un'istruzione singola. SOLO l'ultimo dichiaratore senza
	// "As" esplicito e' davvero "senza tipo" nel vero VBA -- ogni
	// dichiaratore PRECEDENTE senza "As" e' implicitamente Variant,
	// MAI il tipo del dichiaratore che segue (un errore comune di chi
	// legge "Dim a, b As Long" come "entrambi Long"): questo
	// sottoinsieme tratta comunque ogni variabile come Variant quando
	// "As" e' assente, quindi il comportamento osservabile e' identico
	// senza bisogno di spiegare la distinzione al runtime.
public:
	// Pubblico (a differenza del resto dei parser di singole istruzioni
	// qui sotto): serve anche al wrapper ParseVbaDimStatementList, che
	// chi scandisce le variabili a LIVELLO DI MODULO usa direttamente
	// (VbaMacroRunner.h), non solo ParseStatementList qui sotto.
	bool ParseDimStatementList(std::vector<std::unique_ptr<VbaStmt>>* outStmts)
	{
		int line = Current().line;
		Advance(); // "Dim"
		for (;;)
		{
			auto stmt = ParseOneDimDeclarator(line);
			if (!stmt)
				return false;
			outStmts->push_back(std::move(stmt));
			if (IsOp(","))
			{
				Advance();
				continue;
			}
			break;
		}
		return ExpectStatementEnd();
	}
private:

	// Fase 6c: "(" limiteSuperiore ("," limiteSuperiore)* ")" dopo il
	// nome di variabile su un "Dim", oppure niente affatto (variabile
	// scalare normale, "outDims" resta vuoto) -- SOLO un numero
	// letterale per ogni limite (niente espressioni generiche: "limiti
	// statici dichiarati al momento del Dim", vedi ROADMAP.md Fase 6c).
	// Ogni limite e' quello SUPERIORE incluso, Option Base 0 implicito
	// del vero VBA ("Dim x(10)" sono gli indici 0..10, 11 elementi) --
	// gia' convertito in conteggio di elementi qui, cosi' il resto del
	// programma (VbaEnvironment::DeclareArray) non deve rifare +1.
	bool ParseArrayDimList(std::vector<int>* outDims)
	{
		if (!IsOp("("))
			return true; // variabile scalare, nessun limite da leggere
		Advance();
		if (IsOp(")"))
		{
			// Fase 6g: "Dim nome() As Tipo" -- parentesi vuote, un array
			// DINAMICO non ancora dimensionato (indistinguibile da una
			// variabile scalare finche' un "ReDim" vero non lo
			// dimensiona, vedi VbaStmt::arrayDims): "outDims" resta
			// vuoto, esattamente come il caso scalare sopra.
			Advance();
			return true;
		}
		for (;;)
		{
			if (Current().type != vbaTokNumber || Current().numberValue < 0)
			{
				SetError("atteso un numero non negativo come limite superiore "
					"della dimensione dell'array");
				return false;
			}
			outDims->push_back((int)Current().numberValue + 1);
			Advance();
			if (IsOp(","))
			{
				Advance();
				continue;
			}
			break;
		}
		return ExpectOp(")");
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

		// Fase 6g (seguito): "NomeSub arg1, arg2" -- la "forma a comando"
		// del vero VBA, una chiamata SENZA parentesi con almeno un
		// argomento (trovata controllando il vero sorgente chip-8-vba:
		// "SetScreenPixel i, j, 0", "VMSetRunning False", "TODO
		// stringa"), SOLO in posizione di istruzione (il vero VBA non la
		// permette affatto dentro un'espressione, quindi non serve
		// toccare VbaExprParser.cpp). Riconosciuta guardando avanti UN
		// token dopo un identificatore: se cio' che segue NON e' "(" /
		// "=" / "." / una fine istruzione o un terminatore di blocco,
		// e' l'inizio di un elenco di argomenti, non un errore di
		// sintassi ("Foo" seguito da un token spurio). Si torna indietro
		// (fPos ripristinato) se non e' davvero questo caso, cosi' ogni
		// altra forma (nome solo, "nome(args)", "nome.Proprieta'",
		// un'assegnazione...) segue la strada di sempre, immutata.
		if (Current().type == vbaTokIdentifier)
		{
			size_t savedPos = fPos;
			std::string name = Current().text;
			int nameLine = Current().line;
			Advance();
			if (!IsOp("(") && !IsOp("=") && !IsOp(".") && !IsNewlineTok() && !IsEndTok()
				&& !IsBlockTerminator())
			{
				auto call = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprCall));
				call->name = name;
				call->line = nameLine;
				for (;;)
				{
					auto arg = ParseVbaExpression(fTokens, fPos, fError);
					if (!arg)
					{
						fFailed = true;
						return nullptr;
					}
					call->children.push_back(std::move(arg));
					if (IsOp(","))
					{
						Advance();
						continue;
					}
					break;
				}
				auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtExprStmt));
				stmt->expr = std::move(call);
				stmt->line = line;
				if (!ExpectStatementEnd())
					return nullptr;
				return stmt;
			}
			fPos = savedPos; // non e' questo caso: si riparte da capo con la strada di sempre
		}

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

	// Fase 6g (seguito): "While expr" ... "Wend" -- il vero VBA lo
	// tratta come un costrutto di ciclo A PARTE da "Do...Loop" (niente
	// "Exit While", solo la condizione), ma e' esattamente equivalente
	// a "Do While expr ... Loop" (controllo PRIMA, puo' non girare
	// nemmeno una volta): nessun nuovo nodo nell'albero, si costruisce
	// direttamente un vbaStmtDoLoop con doIsPreTest=true/doIsUntil=false,
	// stesso esecutore di VbaExecutor.cpp riusato senza modifiche.
	// Trovato controllando il vero sorgente chip-8-vba ("While
	// ThisVM.CurrentKey = -1 / DoEvents / Wend" nell'opcode FX0A).
	std::unique_ptr<VbaStmt> ParseWhileWend()
	{
		int line = Current().line;
		Advance(); // "While"
		auto cond = ParseVbaExpression(fTokens, fPos, fError);
		if (!cond)
		{
			fFailed = true;
			return nullptr;
		}
		if (!ExpectStatementEnd())
			return nullptr;

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtDoLoop));
		stmt->doIsPreTest = true;
		stmt->doIsUntil = false;
		stmt->expr = std::move(cond);
		stmt->line = line;

		if (!ParseStatementList(&stmt->body))
			return nullptr;

		if (!ExpectKeyword("Wend"))
			return nullptr;
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
		else if (IsKeyword("Sub") || IsKeyword("Function"))
		{
			// Fase 6g (seguito): "Exit Function" era rimasta scoperta da
			// quando le Function sono arrivate (Fase 6b) -- trovata
			// controllando il vero sorgente chip-8-vba ("If bytes = 0
			// Then / Exit Function / End If" in DrawSprite). Nessun nodo
			// nuovo nell'albero: l'esecuzione di "Exit Function" e'
			// IDENTICA a "Exit Sub" (interrompe la procedura corrente,
			// il valore di ritorno e' comunque gia' quello scritto
			// nella variabile omonima della Function finora -- vedi
			// VbaAst.h), stesso vbaStmtExitSub riusato cosi' com'e'.
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
			SetError("atteso \"For\", \"Do\", \"Sub\" o \"Function\" dopo \"Exit\"");
			return nullptr;
		}
		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(kind));
		stmt->line = line;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// Un identificatore (NON una parola chiave di questo lessico) che
	// deve avere esattamente questo testo, senza distinguere
	// maiuscole/minuscole -- stesso trattamento di "ByVal"/"Private" in
	// altri punti del parser, usato per "Binary"/"Access"/"Read" dentro
	// "Open ... For Binary Access Read" (Fase 6g): nessuno dei tre
	// merita una vera parola chiave riservata, dato che non compaiono
	// altrove nella grammatica.
	bool ExpectIdentifierText(const char* text)
	{
		if (Current().type != vbaTokIdentifier || !EqualsNoCase(Current().text, text))
		{
			SetError(std::string("atteso \"") + text + "\"");
			return false;
		}
		Advance();
		return true;
	}

	// Fase 6g: "Open percorso For Binary Access Read As #numeroFile" --
	// SOLO questa forma fissa (vedi vbaStmtOpen in VbaAst.h): un modo
	// d'accesso diverso da "Binary"/"Read" e' un errore di sintassi
	// chiaro invece di un dato silenziosamente sbagliato, dato che
	// questo interprete non gestirebbe comunque un file apribile in
	// scrittura.
	std::unique_ptr<VbaStmt> ParseOpen()
	{
		int line = Current().line;
		Advance(); // "Open"
		auto pathExpr = ParseVbaExpression(fTokens, fPos, fError);
		if (!pathExpr)
		{
			fFailed = true;
			return nullptr;
		}
		if (!ExpectKeyword("For"))
			return nullptr;
		if (!ExpectIdentifierText("Binary"))
			return nullptr;
		if (!ExpectIdentifierText("Access"))
			return nullptr;
		if (!ExpectIdentifierText("Read"))
			return nullptr;
		if (!ExpectKeyword("As"))
			return nullptr;
		if (!ExpectOp("#"))
			return nullptr;
		auto fileNumExpr = ParseVbaExpression(fTokens, fPos, fError);
		if (!fileNumExpr)
		{
			fFailed = true;
			return nullptr;
		}

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtOpen));
		stmt->expr = std::move(pathExpr);
		stmt->fileNumber = std::move(fileNumExpr);
		stmt->line = line;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// Fase 6g: "Close [#numeroFile]" -- "#numeroFile" facoltativo, vedi
	// vbaStmtClose in VbaAst.h.
	std::unique_ptr<VbaStmt> ParseClose()
	{
		int line = Current().line;
		Advance(); // "Close"
		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtClose));
		stmt->line = line;
		if (IsOp("#"))
		{
			Advance();
			auto fileNumExpr = ParseVbaExpression(fTokens, fPos, fError);
			if (!fileNumExpr)
			{
				fFailed = true;
				return nullptr;
			}
			stmt->fileNumber = std::move(fileNumExpr);
		}
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// Fase 6g: "Get #numeroFile, [posizione], nomeArray" -- la
	// posizione si ANALIZZA comunque quando presente (per non
	// rifiutare a torto una sintassi valida del vero VBA), ma solo la
	// forma con la posizione OMESSA si esegue davvero (ExecuteGet in
	// VbaExecutor.cpp produce un errore di runtime onesto altrimenti,
	// vedi vbaStmtGet in VbaAst.h).
	std::unique_ptr<VbaStmt> ParseGet()
	{
		int line = Current().line;
		Advance(); // "Get"
		if (!ExpectOp("#"))
			return nullptr;
		auto fileNumExpr = ParseVbaExpression(fTokens, fPos, fError);
		if (!fileNumExpr)
		{
			fFailed = true;
			return nullptr;
		}
		if (!ExpectOp(","))
			return nullptr;

		std::unique_ptr<VbaExpr> posExpr; // nullptr = posizione omessa ("Get #f, , var")
		if (!IsOp(","))
		{
			posExpr = ParseVbaExpression(fTokens, fPos, fError);
			if (!posExpr)
			{
				fFailed = true;
				return nullptr;
			}
		}
		if (!ExpectOp(","))
			return nullptr;

		if (Current().type != vbaTokIdentifier)
		{
			SetError("atteso il nome dell'array di destinazione dopo \"Get #..., ...,\"");
			return nullptr;
		}
		std::string varName = Current().text;
		Advance();

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtGet));
		stmt->fileNumber = std::move(fileNumExpr);
		stmt->expr = std::move(posExpr);
		stmt->varName = varName;
		stmt->line = line;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// Fase 6g: "ReDim nomeArray(limite[, limite...])" -- ogni "limite"
	// e' "espressione" (limite inferiore 0 implicito) oppure "0 To
	// espressione" (limite inferiore 0 ESPLICITO, mai un altro numero
	// -- vedi vbaStmtReDim in VbaAst.h). "Preserve" e' un errore di
	// sintassi chiaro, non un limite silenziosamente ignorato: questo
	// sottoinsieme ridimensiona SEMPRE azzerando, stesso comportamento
	// di un secondo "Dim" sullo stesso nome (Fase 6c).
	std::unique_ptr<VbaStmt> ParseReDim()
	{
		int line = Current().line;
		Advance(); // "ReDim"
		if (Current().type == vbaTokIdentifier && EqualsNoCase(Current().text, "preserve"))
		{
			SetError("\"ReDim Preserve\" non e' supportato in questo sottoinsieme (un "
				"ridimensionamento azzera sempre l'array, stesso limite di un secondo "
				"\"Dim\" sullo stesso nome -- vedi ROADMAP.md)");
			return nullptr;
		}
		if (Current().type != vbaTokIdentifier)
		{
			SetError("atteso un nome di variabile dopo \"ReDim\"");
			return nullptr;
		}
		std::string varName = Current().text;
		Advance();
		if (!ExpectOp("("))
			return nullptr;

		std::vector<std::unique_ptr<VbaExpr>> dims;
		for (;;)
		{
			auto boundExpr = ParseVbaExpression(fTokens, fPos, fError);
			if (!boundExpr)
			{
				fFailed = true;
				return nullptr;
			}
			if (IsKeyword("To"))
			{
				if (boundExpr->kind != vbaExprNumber || boundExpr->numberValue != 0.0)
				{
					SetError("\"ReDim\" supporta solo un limite inferiore letterale 0 "
						"(es. \"ReDim x(0 To n)\"), mai un limite inferiore diverso da zero");
					return nullptr;
				}
				Advance(); // "To"
				boundExpr = ParseVbaExpression(fTokens, fPos, fError);
				if (!boundExpr)
				{
					fFailed = true;
					return nullptr;
				}
			}
			dims.push_back(std::move(boundExpr));
			if (IsOp(","))
			{
				Advance();
				continue;
			}
			break;
		}
		if (!ExpectOp(")"))
			return nullptr;

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtReDim));
		stmt->varName = varName;
		stmt->reDimDims = std::move(dims);
		stmt->line = line;
		if (!ExpectStatementEnd())
			return nullptr;
		return stmt;
	}

	// Fase 6g: "MsgBox espressione" -- un UNICO argomento posizionale
	// (il vero VBA accetta anche pulsanti/titolo separati da virgola,
	// fuori scopo qui: nessuna macro reale controllata finora ne ha
	// bisogno), riscritto come la stessa identica forma ad albero di
	// una chiamata-istruzione nuda ("vbaStmtExprStmt" su un
	// "vbaExprCall" sintetico) cosi' l'esecutore non ha bisogno di un
	// caso dedicato -- vedi EvalCall in VbaEvaluator.cpp sul perche'
	// resta poi un no-op deliberato a runtime.
	std::unique_ptr<VbaStmt> ParseMsgBox()
	{
		int line = Current().line;
		Advance(); // "MsgBox"
		auto arg = ParseVbaExpression(fTokens, fPos, fError);
		if (!arg)
		{
			fFailed = true;
			return nullptr;
		}
		auto call = std::unique_ptr<VbaExpr>(new VbaExpr(vbaExprCall));
		call->name = "MsgBox";
		call->line = line;
		call->children.push_back(std::move(arg));

		auto stmt = std::unique_ptr<VbaStmt>(new VbaStmt(vbaStmtExprStmt));
		stmt->expr = std::move(call);
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

std::unique_ptr<VbaTypeDecl> ParseVbaTypeDecl(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError)
{
	Parser parser(tokens, pos, outError);
	std::unique_ptr<VbaTypeDecl> result = parser.ParseTypeDecl();
	if (!result && outError->message.empty())
		outError->message = "errore di sintassi";
	return result;
}

bool ParseVbaDimStatementList(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError, std::vector<std::unique_ptr<VbaStmt>>* outStmts)
{
	Parser parser(tokens, pos, outError);
	bool ok = parser.ParseDimStatementList(outStmts);
	if (!ok && outError->message.empty())
		outError->message = "errore di sintassi";
	return ok;
}
