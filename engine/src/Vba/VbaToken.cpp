/*
	VbaToken.cpp

	Vedi VbaToken.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaToken.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>

// Parole chiave del sottoinsieme supportato (vedi il commento in cima
// a VbaToken.h). Confrontate senza distinguere maiuscole/minuscole
// (VBA e' case-insensitive) -- il testo del token viene comunque
// normalizzato alla grafia qui sotto, cosi' il parser (Fase 2b) puo'
// confrontare stringhe esatte invece di rifare ogni volta un confronto
// senza distinzione di maiuscole.
static const char* const kKeywords[] = {
	"Sub", "Function", "Type", "End", "Dim", "As", "If", "Then", "Else", "ElseIf",
	"For", "To", "Step", "Next", "True", "False", "And", "Or", "Not",
	"Mod", "Set", "Exit", "Call", "Select", "Case", "Each", "In",
	"Do", "Loop", "While", "Until",
};
static const int kKeywordCount = (int)(sizeof(kKeywords) / sizeof(kKeywords[0]));

static std::string ToLowerAscii(const std::string& s)
{
	std::string out = s;
	for (size_t i = 0; i < out.size(); i++)
		out[i] = (char)tolower((unsigned char)out[i]);
	return out;
}

// Mappa "testo in minuscolo" -> grafia canonica, costruita una sola
// volta al primo uso (nessuna variabile globale con inizializzazione
// statica complessa, stesso stile gia' seguito altrove in questo
// progetto per le tabelle piccole e immutabili).
static const char* MatchKeyword(const std::string& identifier)
{
	static std::map<std::string, const char*>* table = NULL;
	if (table == NULL)
	{
		table = new std::map<std::string, const char*>();
		for (int i = 0; i < kKeywordCount; i++)
			(*table)[ToLowerAscii(kKeywords[i])] = kKeywords[i];
	}
	std::map<std::string, const char*>::const_iterator it = table->find(ToLowerAscii(identifier));
	return it == table->end() ? NULL : it->second;
}

void TokenizeVba(const std::string& source, std::vector<VbaToken>* outTokens)
{
	outTokens->clear();
	size_t pos = 0;
	size_t len = source.size();
	int line = 1;
	bool atLineStart = true; // per evitare un vbaTokNewline spurio prima della primissima istruzione

	while (pos < len)
	{
		char c = source[pos];

		// Spazi/tabulazioni: separano i token, nessun significato proprio.
		if (c == ' ' || c == '\t')
		{
			pos++;
			continue;
		}

		// Continuazione di riga: uno spazio seguito da "_" a fine riga
		// (eventuale \r prima del \n) unisce la riga successiva a
		// questa, nessun token di newline emesso.
		if (c == '_' && pos + 1 < len)
		{
			size_t lookahead = pos + 1;
			while (lookahead < len && (source[lookahead] == ' ' || source[lookahead] == '\t'))
				lookahead++;
			if (lookahead < len && source[lookahead] == '\r')
				lookahead++;
			if (lookahead < len && source[lookahead] == '\n')
			{
				pos = lookahead + 1;
				line++;
				continue;
			}
			// altrimenti "_" fa parte di un identificatore normale,
			// gestito piu' sotto insieme a lettere/cifre
		}

		// Fine riga (statement separator): \r\n, \r da solo, o \n da solo.
		if (c == '\r' || c == '\n')
		{
			if (c == '\r' && pos + 1 < len && source[pos + 1] == '\n')
				pos++;
			pos++;
			line++;
			if (!atLineStart)
			{
				VbaToken tok;
				tok.type = vbaTokNewline;
				tok.line = line - 1;
				outTokens->push_back(tok);
				atLineStart = true;
			}
			continue;
		}

		// ":" sulla stessa riga separa due istruzioni esattamente come
		// andare a capo (es. "x = 1: y = 2").
		if (c == ':')
		{
			pos++;
			if (!atLineStart)
			{
				VbaToken tok;
				tok.type = vbaTokNewline;
				tok.line = line;
				outTokens->push_back(tok);
				atLineStart = true;
			}
			continue;
		}

		// Commento: da "'" alla fine della riga, mai un token proprio.
		if (c == '\'')
		{
			while (pos < len && source[pos] != '\r' && source[pos] != '\n')
				pos++;
			continue;
		}

		// Stringa letterale: fra virgolette doppie, "" dentro la
		// stringa e' una virgoletta letterale sfuggita (convenzione
		// VBA, niente backslash).
		if (c == '"')
		{
			pos++;
			std::string value;
			bool closed = false;
			while (pos < len)
			{
				if (source[pos] == '"')
				{
					if (pos + 1 < len && source[pos + 1] == '"')
					{
						value += '"';
						pos += 2;
						continue;
					}
					pos++;
					closed = true;
					break;
				}
				if (source[pos] == '\r' || source[pos] == '\n')
					break; // stringa non chiusa prima della fine della riga
				value += source[pos];
				pos++;
			}
			VbaToken tok;
			tok.type = closed ? vbaTokString : vbaTokError;
			tok.stringValue = value;
			tok.text = closed ? value : "stringa non chiusa";
			tok.line = line;
			outTokens->push_back(tok);
			atLineStart = false;
			continue;
		}

		// Numero: cifre, un punto decimale opzionale. &H/&O (esadecimale/
		// ottale VBA) non supportati in questo sottoinsieme -- raro in
		// una vera macro registrata, limite dichiarato.
		if (isdigit((unsigned char)c) || (c == '.' && pos + 1 < len && isdigit((unsigned char)source[pos + 1])))
		{
			size_t start = pos;
			bool sawDot = false;
			while (pos < len && (isdigit((unsigned char)source[pos])
				|| (source[pos] == '.' && !sawDot)))
			{
				if (source[pos] == '.')
					sawDot = true;
				pos++;
			}
			std::string numText = source.substr(start, pos - start);
			VbaToken tok;
			tok.type = vbaTokNumber;
			tok.text = numText;
			tok.numberValue = atof(numText.c_str());
			tok.line = line;
			outTokens->push_back(tok);
			atLineStart = false;
			continue;
		}

		// Identificatore o parola chiave: lettera o "_" iniziale,
		// seguita da lettere/cifre/"_".
		if (isalpha((unsigned char)c) || c == '_')
		{
			size_t start = pos;
			while (pos < len && (isalnum((unsigned char)source[pos]) || source[pos] == '_'))
				pos++;
			std::string word = source.substr(start, pos - start);
			const char* keyword = MatchKeyword(word);
			VbaToken tok;
			tok.type = keyword != NULL ? vbaTokKeyword : vbaTokIdentifier;
			tok.text = keyword != NULL ? keyword : word;
			tok.line = line;
			outTokens->push_back(tok);
			atLineStart = false;
			continue;
		}

		// Operatori, alcuni a due caratteri (<=, >=, <>) da riconoscere
		// prima della forma a un solo carattere.
		{
			static const char* const kTwoCharOps[] = { "<=", ">=", "<>" };
			bool matchedTwo = false;
			for (int i = 0; i < 3 && pos + 1 < len; i++)
			{
				if (c == kTwoCharOps[i][0] && source[pos + 1] == kTwoCharOps[i][1])
				{
					VbaToken tok;
					tok.type = vbaTokOperator;
					tok.text = kTwoCharOps[i];
					tok.line = line;
					outTokens->push_back(tok);
					pos += 2;
					matchedTwo = true;
					break;
				}
			}
			if (matchedTwo)
			{
				atLineStart = false;
				continue;
			}
		}

		static const char kOneCharOps[] = "+-*/^&=<>(),.";
		if (strchr(kOneCharOps, c) != NULL)
		{
			VbaToken tok;
			tok.type = vbaTokOperator;
			tok.text = std::string(1, c);
			tok.line = line;
			outTokens->push_back(tok);
			pos++;
			atLineStart = false;
			continue;
		}

		// Carattere non riconosciuto nel sottoinsieme supportato: un
		// token di errore, MAI un arresto dell'intera analisi (vedi il
		// commento in cima all'header sul perche').
		VbaToken tok;
		tok.type = vbaTokError;
		tok.text = std::string(1, c);
		tok.line = line;
		outTokens->push_back(tok);
		pos++;
		atLineStart = false;
	}

	if (!atLineStart)
	{
		VbaToken tok;
		tok.type = vbaTokNewline;
		tok.line = line;
		outTokens->push_back(tok);
	}

	VbaToken endTok;
	endTok.type = vbaTokEnd;
	endTok.line = line;
	outTokens->push_back(endTok);
}
