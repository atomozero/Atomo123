/*
	vba_token_test.cpp

	Verifica TokenizeVba (VbaToken.h -- Path to full Excel parity,
	VBA/macro execution, Fase 2a, il sottoinsieme deciso in
	ROADMAP.md).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <string>
#include <vector>

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

static std::string TypeName(VbaTokenType t)
{
	switch (t)
	{
		case vbaTokEnd: return "End";
		case vbaTokNewline: return "Newline";
		case vbaTokIdentifier: return "Identifier";
		case vbaTokKeyword: return "Keyword";
		case vbaTokNumber: return "Number";
		case vbaTokString: return "String";
		case vbaTokOperator: return "Operator";
		case vbaTokError: return "Error";
	}
	return "?";
}

// Confronta la sequenza di token (tipo+testo, Newline/End ignorano il
// testo) con un elenco atteso "Tipo:testo" separato da spazi, per
// controlli compatti e leggibili invece di ispezionare ogni token uno
// per uno a mano in ogni test.
static bool MatchSequence(const std::vector<VbaToken>& tokens, const std::vector<std::string>& expected)
{
	size_t i = 0;
	for (size_t e = 0; e < expected.size(); e++)
	{
		if (i >= tokens.size())
			return false;
		size_t colon = expected[e].find(':');
		std::string expType = expected[e].substr(0, colon);
		std::string expText = colon == std::string::npos ? "" : expected[e].substr(colon + 1);

		if (TypeName(tokens[i].type) != expType)
			return false;
		if (colon != std::string::npos && tokens[i].text != expText)
			return false;
		i++;
	}
	return true;
}

int main()
{
	// ---- Token elementari ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("x = 5", &tokens);
		std::vector<std::string> expected;
		expected.push_back("Identifier:x");
		expected.push_back("Operator:=");
		expected.push_back("Number:5");
		expected.push_back("Newline");
		expected.push_back("End");
		Check(MatchSequence(tokens, expected), "assegnazione semplice: identificatore, =, numero");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("Dim total As Integer", &tokens);
		std::vector<std::string> expected;
		expected.push_back("Keyword:Dim");
		expected.push_back("Identifier:total");
		expected.push_back("Keyword:As");
		expected.push_back("Identifier:Integer"); // "Integer" non e' nel sottoinsieme di parole chiave
		expected.push_back("Newline");
		expected.push_back("End");
		Check(MatchSequence(tokens, expected), "\"Dim x As Integer\": Dim/As riconosciute, Integer no (non necessaria)");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("sub SUB Sub", &tokens);
		Check(tokens.size() >= 3 && tokens[0].type == vbaTokKeyword && tokens[0].text == "Sub"
			&& tokens[1].type == vbaTokKeyword && tokens[1].text == "Sub"
			&& tokens[2].type == vbaTokKeyword && tokens[2].text == "Sub",
			"le parole chiave si riconoscono senza distinguere maiuscole/minuscole, "
			"normalizzate alla stessa grafia");
	}

	// ---- Numeri ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("3.14", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokNumber && tokens[0].numberValue == 3.14,
			"numero decimale con virgola mobile");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("42", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokNumber && tokens[0].numberValue == 42.0,
			"numero intero");
	}

	// ---- Stringhe, incluso l'escape "" ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("\"hello\"", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokString && tokens[0].stringValue == "hello",
			"stringa letterale semplice");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("\"He said \"\"hi\"\"\"", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokString
			&& tokens[0].stringValue == "He said \"hi\"",
			"\"\" dentro una stringa diventa una virgoletta letterale");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("\"unterminated", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokError,
			"una stringa mai chiusa prima della fine della riga produce un token di errore, "
			"non un crash o una lettura oltre la fine del sorgente");
	}

	// ---- Operatori a due caratteri riconosciuti prima di quelli a un solo carattere ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("a <= b", &tokens);
		Check(tokens.size() >= 3 && tokens[1].type == vbaTokOperator && tokens[1].text == "<=",
			"\"<=\" si riconosce come un solo operatore, non \"<\" seguito da \"=\"");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("a <> b", &tokens);
		Check(tokens.size() >= 3 && tokens[1].type == vbaTokOperator && tokens[1].text == "<>",
			"\"<>\" (diverso da) si riconosce come un solo operatore");
	}

	// ---- Commenti: spariscono, ma la riga continua a contare come un'istruzione finita ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("x = 1 ' questo e' un commento\r\ny = 2", &tokens);
		std::vector<std::string> expected;
		expected.push_back("Identifier:x");
		expected.push_back("Operator:=");
		expected.push_back("Number:1");
		expected.push_back("Newline");
		expected.push_back("Identifier:y");
		expected.push_back("Operator:=");
		expected.push_back("Number:2");
		expected.push_back("Newline");
		expected.push_back("End");
		Check(MatchSequence(tokens, expected),
			"un commento \"'...\" sparisce del tutto, ma il newline dopo resta");
	}

	// ---- Continuazione di riga con " _" ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("x = 1 + _\r\n    2", &tokens);
		std::vector<std::string> expected;
		expected.push_back("Identifier:x");
		expected.push_back("Operator:=");
		expected.push_back("Number:1");
		expected.push_back("Operator:+");
		expected.push_back("Number:2");
		expected.push_back("Newline");
		expected.push_back("End");
		Check(MatchSequence(tokens, expected),
			"\" _\" a fine riga unisce la riga successiva, nessun Newline spurio in mezzo");
	}

	// ---- ":" separa due istruzioni sulla stessa riga, come andare a capo ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("x = 1: y = 2", &tokens);
		std::vector<std::string> expected;
		expected.push_back("Identifier:x");
		expected.push_back("Operator:=");
		expected.push_back("Number:1");
		expected.push_back("Newline");
		expected.push_back("Identifier:y");
		expected.push_back("Operator:=");
		expected.push_back("Number:2");
		expected.push_back("Newline");
		expected.push_back("End");
		Check(MatchSequence(tokens, expected), "\":\" produce un Newline, come una vera fine riga");
	}

	// ---- Carattere non riconosciuto: un token di errore, mai un arresto totale ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("x = 1 @ y = 2", &tokens);
		bool hasError = false;
		bool hasY = false;
		for (size_t i = 0; i < tokens.size(); i++)
		{
			if (tokens[i].type == vbaTokError && tokens[i].text == "@")
				hasError = true;
			if (tokens[i].type == vbaTokIdentifier && tokens[i].text == "y")
				hasY = true;
		}
		Check(hasError, "un carattere non riconosciuto (\"@\") produce un token di errore");
		Check(hasY, "...ma l'analisi continua dopo: il resto della riga (\"y\") si legge comunque");
	}

	// ---- Uno spezzone realistico, completo, come da sottoinsieme supportato ----
	{
		const char* source =
			"Sub FillRange()\r\n"
			"    Dim i As Integer\r\n"
			"    For i = 1 To 10\r\n"
			"        If i Mod 2 = 0 Then\r\n"
			"            Cells(i, 1).Value = \"even\"\r\n"
			"        Else\r\n"
			"            Cells(i, 1).Value = \"odd\"\r\n"
			"        End If\r\n"
			"    Next i\r\n"
			"End Sub\r\n";
		std::vector<VbaToken> tokens;
		TokenizeVba(source, &tokens);

		bool hasError = false;
		int subCount = 0, endCount = 0, forCount = 0, ifCount = 0, cellsCount = 0;
		for (size_t i = 0; i < tokens.size(); i++)
		{
			if (tokens[i].type == vbaTokError)
				hasError = true;
			if (tokens[i].type == vbaTokKeyword && tokens[i].text == "Sub")
				subCount++;
			if (tokens[i].type == vbaTokKeyword && tokens[i].text == "End")
				endCount++;
			if (tokens[i].type == vbaTokKeyword && tokens[i].text == "For")
				forCount++;
			if (tokens[i].type == vbaTokKeyword && tokens[i].text == "If")
				ifCount++;
			if (tokens[i].type == vbaTokIdentifier && tokens[i].text == "Cells")
				cellsCount++;
		}
		Check(!hasError, "uno spezzone VBA realistico completo (Sub/Dim/For/If/Cells.Value) "
			"non produce nessun token di errore");
		Check(subCount == 2, "\"Sub\" compare due volte (l'apertura \"Sub FillRange()\" "
			"e \"End Sub\", che tokenizza come due parole chiave separate: \"End\" + \"Sub\")");
		Check(endCount == 2, "\"End\" compare due volte (\"End If\" e \"End Sub\")");
		Check(forCount == 1 && ifCount == 2,
			"\"For\" compare una volta, \"If\" due (l'apertura e \"End If\", "
			"anch'esso due parole chiave separate: \"End\" + \"If\")");
		Check(cellsCount == 2, "\"Cells\" compare due volte (un ramo Then, un ramo Else)");
	}

	// ---- Fase 6g: Open/Close/Get/ReDim come parole chiave, "#" come operatore ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("Open path For Binary Access Read As #file", &tokens);
		std::vector<std::string> expected;
		expected.push_back("Keyword:Open");
		expected.push_back("Identifier:path");
		expected.push_back("Keyword:For");
		expected.push_back("Identifier:Binary"); // non una parola chiave, solo "Open"/"Close"/"Get"/"ReDim" lo sono
		expected.push_back("Identifier:Access");
		expected.push_back("Identifier:Read");
		expected.push_back("Keyword:As");
		expected.push_back("Operator:#");
		expected.push_back("Identifier:file");
		expected.push_back("Newline");
		expected.push_back("End");
		Check(MatchSequence(tokens, expected),
			"\"Open ... For Binary Access Read As #file\": Open/For/As parole chiave, "
			"Binary/Access/Read semplici identificatori, \"#\" un operatore a parte");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("Close #file", &tokens);
		Check(tokens.size() >= 3 && tokens[0].type == vbaTokKeyword && tokens[0].text == "Close"
			&& tokens[1].type == vbaTokOperator && tokens[1].text == "#",
			"\"Close\" e' una parola chiave, \"#\" il suo stesso operatore");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("Get #file, , rom", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokKeyword && tokens[0].text == "Get",
			"\"Get\" e' una parola chiave");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("ReDim rom(0 To n)", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokKeyword && tokens[0].text == "ReDim",
			"\"ReDim\" e' una parola chiave");
	}

	// ---- Fase 6g (seguito): letterali esadecimali "&H..."/"&H...&" ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("&HFF", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokNumber && tokens[0].numberValue == 255.0,
			"\"&HFF\" e' un numero, 255");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("&HF000&", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokNumber && tokens[0].numberValue == 61440.0,
			"\"&HF000&\" (col suffisso Long) e' un numero, 61440 -- il suffisso si legge e si scarta");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("x & y", &tokens);
		std::vector<std::string> expected;
		expected.push_back("Identifier:x");
		expected.push_back("Operator:&");
		expected.push_back("Identifier:y");
		expected.push_back("Newline");
		expected.push_back("End");
		Check(MatchSequence(tokens, expected),
			"un \"&\" NON seguito da \"H\" resta il normale operatore di concatenamento");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("&HFF& & x", &tokens);
		Check(tokens.size() >= 3 && tokens[0].type == vbaTokNumber && tokens[0].numberValue == 255.0
			&& tokens[1].type == vbaTokOperator && tokens[1].text == "&",
			"\"&HFF& & x\": il letterale esadecimale col suo suffisso Long, poi un vero "
			"\"&\" di concatenamento separato");
	}

	// ---- Fase 6g (seguito): "\" (divisione intera), "Xor", "Wend" ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("x \\ y", &tokens);
		Check(tokens.size() >= 3 && tokens[1].type == vbaTokOperator && tokens[1].text == "\\",
			"\"\\\" (divisione intera) e' un operatore a parte da \"/\"");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("x Xor y", &tokens);
		Check(tokens.size() >= 2 && tokens[1].type == vbaTokKeyword && tokens[1].text == "Xor",
			"\"Xor\" e' una parola chiave");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("While x\r\nWend", &tokens);
		Check(tokens.size() >= 1 && tokens[0].type == vbaTokKeyword && tokens[0].text == "While",
			"\"While\" e' una parola chiave (gia' lo era per \"Do While\")");
		bool hasWend = false;
		for (size_t i = 0; i < tokens.size(); i++)
			if (tokens[i].type == vbaTokKeyword && tokens[i].text == "Wend")
				hasWend = true;
		Check(hasWend, "\"Wend\" e' una parola chiave");
	}

	// ---- Fase 6g (seguito): "Step" NON e' una parola chiave riservata
	// (trovato controllando il vero sorgente chip-8-vba, che dichiara
	// una vera "Sub Step()") -- resta SEMPRE un identificatore, anche
	// dentro "For ... Step ...", dove ParseFor (VbaStmtParser.cpp) lo
	// riconosce per TESTO invece che per tipo di token ----
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("For i = 1 To 10 Step 2", &tokens);
		bool stepIsIdentifier = false;
		for (size_t i = 0; i < tokens.size(); i++)
			if (tokens[i].text == "Step")
				stepIsIdentifier = (tokens[i].type == vbaTokIdentifier);
		Check(stepIsIdentifier, "\"Step\" dentro \"For ... To ... Step ...\" tokenizza come un "
			"identificatore, non una parola chiave");
	}
	{
		std::vector<VbaToken> tokens;
		TokenizeVba("Sub Step()\r\nEnd Sub\r\n", &tokens);
		Check(tokens.size() >= 2 && tokens[0].type == vbaTokKeyword && tokens[0].text == "Sub"
			&& tokens[1].type == vbaTokIdentifier && tokens[1].text == "Step",
			"\"Sub Step()\" -- \"Step\" come nome di procedura tokenizza come un identificatore "
			"vero, non un errore");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
