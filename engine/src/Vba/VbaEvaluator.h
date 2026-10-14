/*
	VbaEvaluator.h

	Valuta un albero VbaExpr (Fase 2b) in un Value a runtime (Fase 3a
	dell'interprete -- vedi la decisione del sottoinsieme in
	ROADMAP.md): literali, variabili, operatori aritmetici/di
	confronto/di concatenazione/logici con le regole di conversione di
	VBA, e le funzioni predefinite di questo sottoinsieme (Len/Left/
	Right/Mid/UCase/LCase/Trim/Abs/Int/Round).

	Fase 3c, modello a oggetti Excel minimo (Range/Cells/.Value/
	.Select/.Offset): Range("A1")/Range("A1:B2")/Cells(riga,colonna)
	producono un Value di tipo eRangeData legato al CContainer attivo
	di VbaEnvironment (Value::fRangeContainer, lo stesso campo gia'
	usato per i riferimenti a tabella tra fogli diversi -- vedi
	Value.h). ".Offset(righe,colonne)" sposta un intervallo esistente
	e resta un Value eRangeData, cosi' si incatena
	(Range("A1").Offset(1,0).Value). ".Select" e' un no-op deliberato
	in questa fase: nessun concetto di selezione a livello di motore
	puro, senza una UI collegata -- la Fase 3d/5 colleghera' la
	selezione vera del foglio. Qualunque altra proprieta'/metodo non
	riconosciuto produce ancora un errore di esecuzione chiaro, non un
	crash.

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

// Legge il valore scalare di un intervallo di UNA sola cella --
// il "default member" di Range/Cells in VBA, usato sia per ".Value"
// esplicito sia per la coercizione implicita di "x = Range(...)"
// senza ".Value" (VbaExecutor.cpp, vbaStmtAssign). Una cella vuota
// da' 0.0 (Variant vuota di VBA), non un errore. Falso con "outError"
// popolato se "rangeValue" non e' affatto un intervallo, ne copre
// piu' di una cella (fuori dal sottoinsieme di questa fase), o non
// ha un documento collegato.
bool VbaReadRangeValue(const Value& rangeValue, Value* outResult, VbaRuntimeError* outError, int line);

// Scrive un valore scalare in UNA sola cella di un intervallo --
// stesso principio inverso di VbaReadRangeValue sopra, usato sia per
// ".Value = x" esplicito sia per la scrittura implicita
// "Range(...) = x" (VbaExecutor.cpp). "newValue" deve essere gia' uno
// scalare: il chiamante de-referenzia un eventuale intervallo PRIMA
// di chiamare questa funzione, mai qui dentro.
bool VbaWriteRangeValue(const Value& rangeValue, const Value& newValue, VbaRuntimeError* outError, int line);

#endif
