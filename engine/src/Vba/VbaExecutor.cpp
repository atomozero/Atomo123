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

			case vbaStmtSelectCase:
				return ExecuteSelectCase(stmt);

			case vbaStmtExitFor:
				return execExitFor;

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
			if (r == execExitSub || r == execError)
				return r; // propaga fino alla Sub chiamante

			current += step;
			fEnv->Set(stmt->varName, Value(current));
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
