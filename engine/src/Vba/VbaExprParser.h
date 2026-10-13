/*
	VbaExprParser.h

	Analisi sintattica di UNA espressione VBA (il sottoinsieme deciso
	in ROADMAP.md) a partire da un flusso di token gia' prodotto da
	TokenizeVba (Fase 2a), in un albero VbaExpr (Fase 2b). Precedenza
	degli operatori, da quella piu' debole a quella piu' forte -- lo
	stesso ordine reale di VBA, Xor/Eqv/Imp/divisione intera "\" fuori
	da questo sottoinsieme (rari in una macro registrata):

		Or
		And
		Not (unario)
		confronto:  =  <>  <  >  <=  >=
		concatenazione: &
		addizione/sottrazione: + -
		Mod
		moltiplicazione/divisione: * /
		meno unario: -
		elevamento a potenza: ^ (associativo a destra)
		primario: literali, variabili, Nome(argomenti...), Nome.Proprieta', ( espressione )

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_EXPR_PARSER_H
#define VBA_EXPR_PARSER_H

#include <memory>
#include <string>
#include <vector>

#include "VbaAst.h"
#include "VbaToken.h"

struct VbaParseError {
	std::string message;
	int line;

	VbaParseError() : line(0) {}
};

// Analizza UNA espressione a partire da tokens[pos] (aggiornato oltre
// l'ultimo token consumato al ritorno, sia in caso di successo che di
// errore -- utile al chiamante per riprendere/segnalare dal punto
// giusto). nullptr e "outError" popolato in caso di errore sintattico
// -- mai un crash su un token inatteso, mancante, o sulla fine
// prematura del flusso di token.
std::unique_ptr<VbaExpr> ParseVbaExpression(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError);

// Analizza un singolo "termine primario" (literale, variabile,
// Nome(argomenti), o un accesso a proprieta'/metodo incatenato:
// Nome.Prop, Nome(args).Prop, ecc.) SENZA consumare nessun operatore
// binario che segue -- in particolare mai un "=" che segue, lasciato
// al chiamante. Usato dal parser delle istruzioni (VbaStmtParser.h)
// per analizzare il lato sinistro di una possibile assegnazione: "=" e'
// ambiguo in VBA fra assegnazione e confronto, e il vero VBA risolve
// l'ambiguita' in base al CONTESTO (inizio di un'istruzione vs. dentro
// un'espressione), non dentro la grammatica delle espressioni stessa
// -- se ParseVbaExpression normale venisse usato qui, "x = 1" verrebbe
// gia' interamente consumato come l'espressione di confronto "(x = 1)"
// prima che il chiamante abbia la possibilita' di riconoscere
// l'assegnazione.
std::unique_ptr<VbaExpr> ParseVbaPrimaryExpression(const std::vector<VbaToken>& tokens, size_t& pos,
	VbaParseError* outError);

#endif
