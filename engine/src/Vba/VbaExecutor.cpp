/*
	VbaExecutor.cpp

	Vedi VbaExecutor.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaExecutor.h"

namespace {

// Come si e' appena fermata l'esecuzione di una lista di istruzioni:
// in fondo normalmente, per un "Exit For" (il ciclo For piu' vicino lo
// intercetta e lo trasforma in execNormal), per un "Exit Sub"
// (propaga fino a ExecuteVbaSub, dove e' un normale completamento
// anticipato), o per un errore di runtime (outError e' gia' popolato).
enum ExecResult {
	execNormal,
	execExitFor,
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

	ExecResult ExecuteStmt(const VbaStmt* stmt)
	{
		switch (stmt->kind)
		{
			case vbaStmtDim:
				fEnv->Declare(stmt->varName);
				return execNormal;

			case vbaStmtAssign:
			case vbaStmtSet:
			{
				Value v;
				if (!Eval(stmt->expr.get(), &v))
					return execError;
				if (stmt->target->kind != vbaExprVariable)
				{
					SetError("l'assegnazione a \"" + stmt->target->name + "\" richiede il modello a "
						"oggetti Excel (una proprieta' come Range(\"A1\").Value), non ancora "
						"supportato in questa fase dell'interprete", stmt->line);
					return execError;
				}
				fEnv->Set(stmt->target->name, v);
				return execNormal;
			}

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

			case vbaStmtExitFor:
				return execExitFor;

			case vbaStmtExitSub:
				return execExitSub;
		}
		SetError("tipo di istruzione sconosciuto", stmt->line);
		return execError;
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
			if (r == execExitSub || r == execError)
				return r; // propaga fino alla Sub chiamante

			current += step;
			fEnv->Set(stmt->varName, Value(current));
		}
		return execNormal;
	}
};

} // namespace

bool ExecuteVbaSub(const VbaSub* sub, VbaEnvironment* env, VbaRuntimeError* outError)
{
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
		case execError:
			return false; // outError e' gia' popolato da Eval()/SetError()
	}
	return false;
}
