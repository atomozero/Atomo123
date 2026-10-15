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

// Fase 6d: stesso principio, ma per un intero "Type Nome" ... "End
// Type" a partire da tokens[pos] (che deve gia' essere la parola
// chiave "Type" -- un eventuale "Private"/"Public" davanti va saltato
// dal CHIAMANTE prima, vedi VbaMacroRunner.h).
std::unique_ptr<VbaTypeDecl> ParseVbaTypeDecl(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError);

// Stesso principio, per "Dim nome1[(limiti)] [As Tipo1], nome2...,
// ..." a partire da tokens[pos] (che deve gia' essere la parola
// chiave "Dim"). Un solo "Dim" del vero VBA, ma PIU' di un VbaStmt
// qui (uno per dichiaratore, nessun nodo "lista" dedicato -- vedi
// VbaStmtParser.cpp): "outStmts" riceve ogni dichiaratore come un
// vbaStmtDim indipendente, nell'ordine scritto. Esposta separatamente
// dall'uso normale dentro ParseStatementList perche' serve anche a
// chi scandisce le variabili dichiarate a LIVELLO DI MODULO
// (VbaMacroRunner.h, stato persistente), fuori da qualunque Sub.
bool ParseVbaDimStatementList(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError, std::vector<std::unique_ptr<VbaStmt>>* outStmts);

// Fase 6h: stesso principio, per "Const nome1 [As Tipo1] = espr1,
// nome2..., ..." a partire da tokens[pos] (che deve gia' essere la
// parola chiave "Const"). Ogni dichiaratore produce DUE VbaStmt (un
// vbaStmtDim seguito dal vbaStmtAssign che lo inizializza, vedi
// VbaStmtParser.cpp) -- esposta separatamente per lo stesso motivo di
// ParseVbaDimStatementList: chi scandisce le costanti a LIVELLO DI
// MODULO (VbaMacroRunner.h, stato persistente) la usa direttamente.
bool ParseVbaConstStatementList(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError, std::vector<std::unique_ptr<VbaStmt>>* outStmts);

#endif
