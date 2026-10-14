/*
	VbaExecutor.h

	Esegue un albero VbaStmt (Fase 2c) usando il valutatore di
	espressioni della Fase 3a (Fase 3b dell'interprete -- vedi la
	decisione del sottoinsieme in ROADMAP.md): Dim, assegnazione/Set,
	If/ElseIf/Else, For/Next con Step, Exit For/Exit Sub.

	Fase 3c, modello a oggetti Excel minimo: un'assegnazione verso
	Range("A1")/Cells(riga,colonna)/".Value" scrive davvero nella
	cella (CContainer::NewCell tramite VbaEvaluator::VbaWriteRangeValue
	-- vedi il commento in VbaEvaluator.h), mentre "Set x = Range(...)"
	memorizza il riferimento stesso nella variabile invece del suo
	valore scalare (lo stesso Set-vs-Let del vero VBA). Qualunque altro
	target non riconosciuto produce ancora un errore chiaro, non un
	crash.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_EXECUTOR_H
#define VBA_EXECUTOR_H

#include "VbaAst.h"
#include "VbaEnvironment.h"
#include "VbaEvaluator.h"

// Esegue l'intera Sub dall'inizio alla fine (o fino a un "Exit Sub"
// esplicito, che qui e' un normale completamento, non un errore).
// Vero se l'esecuzione e' arrivata alla fine senza un errore di
// runtime; falso con "outError" popolato altrimenti (stesse categorie
// di errore di EvaluateVbaExpression, piu' un target di assegnazione
// non supportato in questa fase). Mai un crash, qualunque sia
// l'albero passato -- un "Exit For" fuori da un ciclo For e' un
// errore di esecuzione pulito, non un comportamento indefinito (non
// puo' capitare da un sorgente analizzato da ParseVbaSub, che accetta
// "Exit For" in qualsiasi punto senza verificare il contesto, quindi
// questo e' l'unico posto dove la verifica puo' avvenire).
bool ExecuteVbaSub(const VbaSub* sub, VbaEnvironment* env, VbaRuntimeError* outError);

#endif
