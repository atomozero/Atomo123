/*
	VbaExecutor.cpp

	Vedi VbaExecutor.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaExecutor.h"

#include <cctype>

namespace {

// Come si e' appena fermata l'esecuzione di una lista di istruzioni:
// in fondo normalmente, per un "Exit For" (il ciclo For/For Each PIU'
// VICINO lo intercetta e lo trasforma in execNormal -- vedi il
// commento sotto su "propaga tutto cio' che non e' il MIO segnale di
// uscita"), per un "Exit Do" (stesso principio, ma e' il ciclo Do
// piu' vicino a intercettarlo), per un "Exit Sub" (propaga fino a
// ExecuteVbaSub, dove e' un normale completamento anticipato), o per
// un errore di runtime (outError e' gia' popolato).
enum ExecResult {
	execNormal,
	execExitFor,
	execExitDo,
	execExitSub,
	execError,
};

class Executor {
public:
	Executor(VbaEnvironment* env, VbaRuntimeError* outError)
		: fEnv(env), fError(outError) {}

	ExecResult ExecuteStmtList(const std::vector<std::unique_ptr<VbaStmt>>& stmts)
	{
		for (size_t i = 0; i < stmts.size(); i++)
		{
			ExecResult r = ExecuteStmt(stmts[i].get());
			if (r != execNormal)
				return r; // Exit For/Sub o errore: le istruzioni restanti non si eseguono
		}
		return execNormal;
	}

private:
	VbaEnvironment* fEnv;
	VbaRuntimeError* fError;

	void SetError(const std::string& message, int line)
	{
		fError->message = message;
		fError->line = line;
	}

	// Valuta "expr" nell'ambiente corrente; in caso di errore lo
	// riporta in fError e restituisce false, cosi' ogni chiamante puo'
	// propagare con un semplice "if (!Eval(...)) return execError;".
	bool Eval(const VbaExpr* expr, Value* outValue)
	{
		VbaRuntimeError err;
		if (EvaluateVbaExpression(expr, fEnv, outValue, &err))
			return true;
		SetError(err.message, err.line);
		return false;
	}

	// Fase 6c: valuta ogni argomento di "nomeArray(indici)" (gia'
	// sintatticamente un vbaExprCall, vedi ExecuteAssignOrSet/EvalCall)
	// come un indice intero -- stessa conversione numerica di
	// qualunque altra espressione, troncata verso zero come il vero
	// VBA accederebbe a V(2.9) come V(2).
	bool EvalArrayIndices(const VbaExpr* callExpr, std::vector<int>* outIndices)
	{
		for (size_t i = 0; i < callExpr->children.size(); i++)
		{
			Value v;
			if (!Eval(callExpr->children[i].get(), &v))
				return false;
			outIndices->push_back((int)VbaToNumber(v));
		}
		return true;
	}

	ExecResult ExecuteStmt(const VbaStmt* stmt)
	{
		switch (stmt->kind)
		{
			case vbaStmtDim:
				if (stmt->arrayDims.empty())
					fEnv->Declare(stmt->varName);
				else
					fEnv->DeclareArray(stmt->varName, stmt->arrayDims); // Fase 6c
				return execNormal;

			case vbaStmtAssign:
			case vbaStmtSet:
				return ExecuteAssignOrSet(stmt);

			case vbaStmtExprStmt:
			{
				Value discarded;
				if (!Eval(stmt->expr.get(), &discarded))
					return execError;
				return execNormal;
			}

			case vbaStmtIf:
			{
				Value cond;
				if (!Eval(stmt->expr.get(), &cond))
					return execError;
				return ExecuteStmtList(VbaToBool(cond) ? stmt->body : stmt->elseBody);
			}

			case vbaStmtFor:
				return ExecuteFor(stmt);

			case vbaStmtForEach:
				return ExecuteForEach(stmt);

			case vbaStmtSelectCase:
				return ExecuteSelectCase(stmt);

			case vbaStmtDoLoop:
				return ExecuteDoLoop(stmt);

			case vbaStmtExitFor:
				return execExitFor;

			case vbaStmtExitDo:
				return execExitDo;

			case vbaStmtExitSub:
				return execExitSub;
		}
		SetError("tipo di istruzione sconosciuto", stmt->line);
		return execError;
	}

	static std::string Lowercase(const std::string& s)
	{
		std::string out = s;
		for (size_t i = 0; i < out.size(); i++)
			out[i] = (char)tolower((unsigned char)out[i]);
		return out;
	}

	// Assegnazione ("=") o Set: condividono tutta la logica del
	// target -- solo la lettura del RHS differisce quando il target e'
	// una variabile semplice, vedi sotto.
	ExecResult ExecuteAssignOrSet(const VbaStmt* stmt)
	{
		Value rhs;
		if (!Eval(stmt->expr.get(), &rhs))
			return execError;

		if (stmt->target->kind == vbaExprVariable)
		{
			// Solo "=" (non "Set") de-referenzia implicitamente un
			// intervallo al suo valore scalare -- lo stesso "default
			// member" che il vero VBA applica a un'assegnazione Let,
			// MAI a un'assegnazione Set (che memorizza il riferimento
			// stesso, cosi' "rng.Offset(...)"/"rng.Value" restano
			// utilizzabili in seguito tramite la variabile "rng").
			if (stmt->kind == vbaStmtAssign && rhs.fType == eRangeData)
			{
				Value scalar;
				VbaRuntimeError err;
				if (!VbaReadRangeValue(rhs, &scalar, &err, stmt->line))
				{
					SetError(err.message, err.line);
					return execError;
				}
				rhs = scalar;
			}
			fEnv->Set(stmt->target->name, rhs);
			return execNormal;
		}

		// Fase 6c: "nomeArray(indici) = valore" -- sintatticamente
		// IDENTICO a una scrittura su Range/Cells o a una chiamata
		// (vbaExprCall), distinto solo controllando se "name" e' stato
		// dichiarato come array (VbaEnvironment::IsArray). Deve venire
		// PRIMA del ramo generico sotto, che altrimenti proverebbe a
		// valutare "nomeArray(indici)" come un intervallo.
		if (stmt->target->kind == vbaExprCall && !stmt->target->target
			&& fEnv->IsArray(stmt->target->name))
		{
			std::vector<int> indices;
			if (!EvalArrayIndices(stmt->target.get(), &indices))
				return execError;
			if (stmt->kind == vbaStmtAssign && rhs.fType == eRangeData)
			{
				Value scalar;
				VbaRuntimeError err;
				if (!VbaReadRangeValue(rhs, &scalar, &err, stmt->line))
				{
					SetError(err.message, err.line);
					return execError;
				}
				rhs = scalar;
			}
			if (!fEnv->SetArrayElement(stmt->target->name, indices, rhs))
			{
				SetError("indice fuori dai limiti dell'array \"" + stmt->target->name + "\"",
					stmt->line);
				return execError;
			}
			return execNormal;
		}

		// Il target e' un riferimento al modello a oggetti Excel: una
		// scrittura diretta ("Range(\"A1\") = 5"/"Cells(1,1) = 5",
		// dove il default member si applica anche in SCRITTURA, non
		// solo in lettura) oppure tramite la proprieta' esplicita
		// (".Value = 5", dove si scrive nel VERO target sotto
		// ".Value", non nella property-access stessa). In entrambi i
		// casi si scrive uno scalare in una cella vera, quindi qui il
		// RHS si de-referenzia SEMPRE, Assign o Set che sia -- non ha
		// senso "Impostare" una cella a "essere" un intervallo.
		if (rhs.fType == eRangeData)
		{
			Value scalar;
			VbaRuntimeError err;
			if (!VbaReadRangeValue(rhs, &scalar, &err, stmt->line))
			{
				SetError(err.message, err.line);
				return execError;
			}
			rhs = scalar;
		}

		const VbaExpr* destExpr = stmt->target.get();
		if (destExpr->kind == vbaExprPropertyAccess)
		{
			if (Lowercase(destExpr->name) != "value")
			{
				SetError("l'assegnazione alla proprieta' \"" + destExpr->name + "\" non e' "
					"supportata in questa fase dell'interprete", stmt->line);
				return execError;
			}
			destExpr = destExpr->target.get(); // scrivi nel VERO target sotto ".Value"
		}

		Value destRange;
		if (!Eval(destExpr, &destRange))
			return execError;

		VbaRuntimeError err;
		if (!VbaWriteRangeValue(destRange, rhs, &err, stmt->line))
		{
			SetError(err.message, err.line);
			return execError;
		}
		return execNormal;
	}

	ExecResult ExecuteFor(const VbaStmt* stmt)
	{
		Value startValue, endValue;
		if (!Eval(stmt->expr.get(), &startValue))
			return execError;
		if (!Eval(stmt->forEnd.get(), &endValue))
			return execError;

		double step = 1.0;
		if (stmt->forStep)
		{
			Value stepValue;
			if (!Eval(stmt->forStep.get(), &stepValue))
				return execError;
			step = VbaToNumber(stepValue);
		}
		if (step == 0.0)
		{
			SetError("For con Step 0 andrebbe in ciclo infinito", stmt->line);
			return execError;
		}

		double current = VbaToNumber(startValue);
		double end = VbaToNumber(endValue);
		fEnv->Set(stmt->varName, Value(current));

		while (step > 0.0 ? current <= end : current >= end)
		{
			ExecResult r = ExecuteStmtList(stmt->body);
			if (r == execExitFor)
				break; // consumato da QUESTO ciclo: un normale completamento anticipato
			if (r != execNormal)
				return r; // qualunque altra cosa (Exit Do/Sub, un errore) propaga fino al chiamante

			current += step;
			fEnv->Set(stmt->varName, Value(current));
		}
		return execNormal;
	}

	// "For Each varName In expr": "expr" deve valutare a un intervallo
	// (Range/Cells) -- questo sottoinsieme non ha array/Collection
	// generici da iterare, vedi il commento su vbaStmtForEach in
	// VbaAst.h. Ordine riga per riga (tutte le colonne della prima
	// riga, poi tutte quelle della seconda, ecc. -- lo stesso ordine
	// "a libro" del vero VBA per un Range rettangolare), un ciclo
	// annidato appiattito in un solo "return" invece di un "goto" per
	// uscire da entrambi i livelli insieme su "Exit For".
	ExecResult ExecuteForEach(const VbaStmt* stmt)
	{
		Value collectionValue;
		if (!Eval(stmt->expr.get(), &collectionValue))
			return execError;
		if (collectionValue.fType != eRangeData)
		{
			SetError("For Each richiede un intervallo (Range/Cells), non un valore semplice",
				stmt->line);
			return execError;
		}
		CContainer* doc = collectionValue.fRangeContainer;
		if (!doc)
		{
			SetError("nessun foglio di calcolo collegato a questa esecuzione VBA", stmt->line);
			return execError;
		}
		range r = (range)collectionValue;

		for (int row = r.top; row <= r.bottom; row++)
		{
			for (int col = r.left; col <= r.right; col++)
			{
				Value cellValue;
				cellValue = range(col, row, col, row); // Value::operator=(const range&)
				cellValue.fRangeContainer = doc;
				fEnv->Set(stmt->varName, cellValue);

				ExecResult res = ExecuteStmtList(stmt->body);
				if (res == execExitFor)
					return execNormal; // consumato: esce da ENTRAMBI i livelli del ciclo annidato
				if (res != execNormal)
					return res; // qualunque altra cosa (Exit Do/Sub, un errore) propaga fino al chiamante
			}
		}
		return execNormal;
	}

	// "Do [While|Until expr] ... Loop" (controllo PRIMA, puo' non
	// girare nemmeno una volta) oppure "Do ... Loop [While|Until
	// expr]" (controllo DOPO, gira sempre almeno una volta) -- vedi
	// doIsPreTest/doIsUntil in VbaAst.h. "expr" nullptr (nessuna
	// condizione affatto) continua all'infinito, esce solo con
	// "Exit Do"/"Exit Sub"/un errore.
	ExecResult ExecuteDoLoop(const VbaStmt* stmt)
	{
		for (;;)
		{
			if (stmt->expr && stmt->doIsPreTest)
			{
				Value cond;
				if (!Eval(stmt->expr.get(), &cond))
					return execError;
				bool condTrue = VbaToBool(cond);
				if (stmt->doIsUntil ? condTrue : !condTrue)
					break; // "Until" si ferma quando DIVENTA vera; "While" quando NON e' vera
			}

			ExecResult r = ExecuteStmtList(stmt->body);
			if (r == execExitDo)
				break; // consumato da QUESTO ciclo: un normale completamento anticipato
			if (r != execNormal)
				return r; // qualunque altra cosa (Exit For/Sub, un errore) propaga fino al chiamante

			if (stmt->expr && !stmt->doIsPreTest)
			{
				Value cond;
				if (!Eval(stmt->expr.get(), &cond))
					return execError;
				bool condTrue = VbaToBool(cond);
				if (stmt->doIsUntil ? condTrue : !condTrue)
					break;
			}
		}
		return execNormal;
	}

	// Valuta "stmt->expr" UNA sola volta, poi confronta con ogni
	// valore di ogni "Case" nell'ordine scritto (VbaCompareValues,
	// stessa regola di "=") -- il primo che combacia esegue il suo
	// corpo e basta, come il vero Select Case (nessun fallthrough).
	// Nessun Case combacia e "Case Else" c'e': esegue quello
	// (stmt->elseBody). Nessuno dei due: nessun corpo eseguito, non e'
	// un errore (come il vero VBA).
	ExecResult ExecuteSelectCase(const VbaStmt* stmt)
	{
		Value testValue;
		if (!Eval(stmt->expr.get(), &testValue))
			return execError;

		for (size_t i = 0; i < stmt->cases.size(); i++)
		{
			const VbaCaseClause& clause = stmt->cases[i];
			for (size_t j = 0; j < clause.matchValues.size(); j++)
			{
				Value matchValue;
				if (!Eval(clause.matchValues[j].get(), &matchValue))
					return execError;
				if (VbaCompareValues(testValue, matchValue) == 0)
					return ExecuteStmtList(clause.body);
			}
		}
		return ExecuteStmtList(stmt->elseBody); // vuoto se non c'e' "Case Else": execNormal
	}
};

} // namespace

bool ExecuteVbaSub(const VbaSub* sub, VbaEnvironment* env, VbaRuntimeError* outError)
{
	// Fase 6b: il valore di ritorno di una Function e' una variabile
	// normale con lo stesso nome della Function -- dichiararla ORA
	// (Empty/0.0, come il vero VBA se il corpo non la assegna mai)
	// fa si' che il chiamante (VbaEvaluator.cpp, EvalCall) la trovi
	// sempre con "env->Find(sub->name)" dopo l'esecuzione, senza
	// bisogno di un valore di ritorno dedicato in questa firma.
	if (sub->isFunction)
		env->Declare(sub->name);

	Executor executor(env, outError);
	ExecResult r = executor.ExecuteStmtList(sub->body);
	switch (r)
	{
		case execNormal:
		case execExitSub:
			return true; // "Exit Sub" e' un completamento anticipato, non un errore
		case execExitFor:
			outError->message = "\"Exit For\" fuori da un ciclo For";
			outError->line = 0;
			return false;
		case execExitDo:
			outError->message = "\"Exit Do\" fuori da un ciclo Do";
			outError->line = 0;
			return false;
		case execError:
			return false; // outError e' gia' popolato da Eval()/SetError()
	}
	return false;
}
