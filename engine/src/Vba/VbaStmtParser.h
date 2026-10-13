/*
	VbaStmtParser.h

	Analisi sintattica delle ISTRUZIONI del sottoinsieme VBA deciso in
	ROADMAP.md (Fase 2c, sopra VbaExprParser.h -- Fase 2b -- per ogni
	espressione che compare dentro un'istruzione): Sub/End Sub, Dim,
	assegnazione/Set, If/ElseIf/Else/End If (anche la forma breve su
	una sola riga "If cond Then stmt"), For/Next, Exit For/Sub, e
	un'espressione nuda usata come istruzione (una chiamata di metodo
	per il suo effetto, es. "Range(\"A1\").Select").

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_STMT_PARSER_H
#define VBA_STMT_PARSER_H

#include <memory>
#include <vector>

#include "VbaAst.h"
#include "VbaExprParser.h"
#include "VbaToken.h"

// Analizza un'intera procedura "Sub Nome()" ... "End Sub" a partire da
// tokens[pos] (che deve gia' essere la parola chiave "Sub"), aggiornato
// oltre l'ultimo token consumato al ritorno. nullptr e "outError"
// popolato in caso di errore sintattico in qualunque punto -- mai un
// crash, mai una lettura oltre la fine del flusso di token.
std::unique_ptr<VbaSub> ParseVbaSub(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError);

#endif
