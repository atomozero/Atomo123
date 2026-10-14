/*
	VbaEvaluator.h

	Valuta un albero VbaExpr (Fase 2b) in un Value a runtime (Fase 3a
	dell'interprete -- vedi la decisione del sottoinsieme in
	ROADMAP.md): literali, variabili, operatori aritmetici/di
	confronto/di concatenazione/logici con le regole di conversione di
	VBA, e le funzioni predefinite di questo sottoinsieme (Len/Left/
	Right/Mid/UCase/LCase/Trim/Abs/Int/Round).

	ANCORA NESSUN modello a oggetti Excel qui (Range/Cells/.Value/
	.Select) -- un nodo vbaExprCall/vbaExprPropertyAccess con un nome
	che questo file non riconosce come funzione predefinita produce un
	errore di esecuzione "funzione sconosciuta", non un crash: la Fase
	3c (binding di Range/Cells sul vero CContainer) estendera' questo
	stesso valutatore, non lo sostituira'.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_EVALUATOR_H
#define VBA_EVALUATOR_H

#include <string>

#include "Value.h"
#include "VbaAst.h"
#include "VbaEnvironment.h"

struct VbaRuntimeError {
	std::string message;
	int line;

	VbaRuntimeError() : line(0) {}
};

// Valuta "expr" nel contesto di "env" (lettura/scrittura delle
// variabili). Vero e "outResult" popolato in caso di successo; falso e
// "outError" popolato altrimenti (variabile non dichiarata -- in
// realta' mai, vedi VbaEnvironment::Set sull'assegnazione implicita,
// ma UNA LETTURA di una variabile mai assegnata SI' e' un errore;
// operazione su tipi incompatibili; funzione sconosciuta; argomenti
// sbagliati per una funzione nota). Mai un crash, qualunque sia
// l'albero passato.
bool EvaluateVbaExpression(const VbaExpr* expr, VbaEnvironment* env, Value* outResult,
	VbaRuntimeError* outError);

// Le stesse conversioni Variant-style usate internamente per gli
// operatori (mai un errore, vedi il commento in VbaEvaluator.cpp) --
// esposte cosi' l'esecutore di istruzioni (Fase 3b, If/For) valuta
// una condizione con la STESSA regola di coercizione a booleano
// invece di duplicarla.
double VbaToNumber(const Value& v);
std::string VbaToText(const Value& v);
bool VbaToBool(const Value& v);

#endif
