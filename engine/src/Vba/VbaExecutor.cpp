/*
	VbaExecutor.cpp

	Vedi VbaExecutor.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaExecutor.h"

#include <cctype>
#include <map>
#include <utility>

namespace {

static std::string LowercaseTypeName(const std::string& s)
{
	std::string out = s;
	for (size_t i = 0; i < out.size(); i++)
		out[i] = (char)tolower((unsigned char)out[i]);
	return out;
}

// Fase 6d: costruisce una NUOVA istanza di struct azzerata a partire
// dal suo VbaTypeDecl (VbaAst.h), risolvendo RECURSIVAMENTE i campi
// che sono a loro volta un tipo definito dall'utente (es.
// "Screen(63,31) As ScreenPixel" dentro "VM") tramite lo stesso
// registro -- "registry" e' l'INTERO modulo (VbaMacroRunner.h,
// FindAllVbaTypeDecls), non solo i tipi gia' visti finora, perche' un
// Type puo' referenziarne un altro dichiarato PIU' IN BASSO nello
// stesso modulo. "depth" e' una guardia anti-ricorsione (Type A che
// contiene un campo di Type A, direttamente o tramite una catena --
// strutturalmente impossibile nel vero VBA per una dimensione finita,
// ma questo interprete non deve mai andare in stack overflow nemmeno
// su un sorgente scritto apposta per romperlo). Falso con un
// messaggio chiaro se un campo referenzia un tipo inesistente o la
// ricorsione supera il limite -- MAI un crash.
static bool BuildVbaStructInstance(const std::string& typeName,
	const std::map<std::string, VbaTypeDecl>& registry, VbaStructInstance* out,
	int depth, std::string* outErr)
{
	if (depth > 20)
	{
		*outErr = "troppi livelli di annidamento fra tipi definiti dall'utente "
			"(possibile riferimento circolare fra Type)";
		return false;
	}
	std::map<std::string, VbaTypeDecl>::const_iterator typeIt = registry.find(LowercaseTypeName(typeName));
	if (typeIt == registry.end())
	{
		*outErr = "tipo \"" + typeName + "\" non dichiarato (nessun \"Type ... End Type\" con questo nome)";
		return false;
	}
	const VbaTypeDecl& decl = typeIt->second;
	for (size_t i = 0; i < decl.fields.size(); i++)
	{
		const VbaTypeField& f = decl.fields[i];
		VbaStructField sf;
		sf.isArray = !f.arrayDims.empty();
		sf.isStruct = registry.find(LowercaseTypeName(f.typeName)) != registry.end();
		sf.dims = f.arrayDims;

		size_t total = 1;
		for (size_t d = 0; d < f.arrayDims.size(); d++)
			total *= (size_t)f.arrayDims[d];

		if (!sf.isArray && !sf.isStruct)
		{
			sf.scalarValue = Value(0.0);
		}
		else if (sf.isArray && !sf.isStruct)
		{
			sf.scalarArray.assign(total, Value(0.0));
		}
		else if (!sf.isArray && sf.isStruct)
		{
			sf.structValue.reset(new VbaStructInstance());
			if (!BuildVbaStructInstance(f.typeName, registry, sf.structValue.get(), depth + 1, outErr))
				return false;
		}
		else // isArray && isStruct
		{
			sf.structArray.resize(total);
			for (size_t e = 0; e < total; e++)
			{
				sf.structArray[e].reset(new VbaStructInstance());
				if (!BuildVbaStructInstance(f.typeName, registry, sf.structArray[e].get(),
					depth + 1, outErr))
					return false;
			}
		}
		out->fields[LowercaseTypeName(f.name)] = std::move(sf);
	}
	return true;
}

} // namespace

// Esegue UN "Dim" (un singolo vbaStmtDim, gia' espanso da "Dim a, b,
// c" se serve -- vedi ParseDimStatementList) dentro "env": scalare,
// array (Fase 6c) o istanza di Type (Fase 6d), a seconda di
// "asTypeName"/"arrayDims". Esposta (non piu' solo il corpo del
// "case vbaStmtDim" dentro Executor::ExecuteStmt) perche' serve anche
// a chi inizializza le variabili dichiarate a LIVELLO DI MODULO prima
// di eseguire una Sub (ExecuteVbaMacro, stato persistente -- vedi
// ROADMAP.md), non solo all'esecutore delle istruzioni dentro una Sub.
bool ExecuteDimDeclaration(const VbaStmt* stmt, VbaEnvironment* env, VbaRuntimeError* outError)
{
	const std::map<std::string, VbaTypeDecl>* registry = env->GetTypeRegistry();
	bool isUserType = registry && !stmt->asTypeName.empty()
		&& registry->find(LowercaseTypeName(stmt->asTypeName)) != registry->end();
	if (isUserType)
	{
		if (!stmt->arrayDims.empty())
		{
			// Limite onesto (vedi ROADMAP.md Fase 6d): un array di UDT
			// e' supportato SOLO come campo di un altro Type (es.
			// "Screen(63,31) As ScreenPixel"), non come "Dim" diretto.
			outError->message = "un array di un tipo definito dall'utente via \"Dim\" non e' "
				"supportato in questo sottoinsieme (solo come campo di un altro Type)";
			outError->line = stmt->line;
			return false;
		}
		VbaStructInstance instance;
		std::string buildErr;
		if (!BuildVbaStructInstance(stmt->asTypeName, *registry, &instance, 0, &buildErr))
		{
			outError->message = buildErr;
			outError->line = stmt->line;
			return false;
		}
		env->DeclareStruct(stmt->varName, std::move(instance));
	}
	else if (stmt->arrayDims.empty())
		env->Declare(stmt->varName);
	else
		env->DeclareArray(stmt->varName, stmt->arrayDims); // Fase 6c
	return true;
}

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
			{
				VbaRuntimeError dimErr;
				if (!ExecuteDimDeclaration(stmt, fEnv, &dimErr))
				{
					SetError(dimErr.message, dimErr.line);
					return execError;
				}
				return execNormal;
			}

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

			case vbaStmtWith:
				return ExecuteWith(stmt);

			case vbaStmtOpen:
				return ExecuteOpen(stmt);

			case vbaStmtClose:
				return ExecuteClose(stmt);

			case vbaStmtGet:
				return ExecuteGet(stmt);

			case vbaStmtReDim:
				return ExecuteReDim(stmt);

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

		// Fase 6d: "ThisVM.PC = valore", "ThisVM.V(2) = valore", o
		// "ThisVM.Screen(x,y).value = valore" -- STESSA risoluzione
		// della catena usata in lettura (VbaEvaluator.cpp,
		// EvalPropertyAccess/EvalCall), cosi' le due direzioni non
		// rischiano di divergere. Deve venire PRIMA del ramo generico
		// sotto, che altrimenti proverebbe a trattare il target come
		// un intervallo Range/Cells.
		{
			std::string structErr;
			Value* fieldSlot = ResolveStructScalarField(stmt->target.get(), fEnv, &structErr);
			if (fieldSlot)
			{
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
				*fieldSlot = rhs;
				return execNormal;
			}
			if (!structErr.empty())
			{
				SetError(structErr, stmt->line);
				return execError;
			}
		}

		// Fase 6f: "Range(...).Interior.Color = valore" o
		// "...Interior.ColorIndex = valore" -- DUE livelli di
		// PropertyAccess (diverso da ".Value", un livello solo), deve
		// venire PRIMA del ramo generico sotto, che altrimenti
		// proverebbe a trattare ".Interior" come il vero target di
		// scrittura (sbagliato: "Interior" non e' affatto una cella).
		if (stmt->target->kind == vbaExprPropertyAccess)
		{
			std::string outerName = Lowercase(stmt->target->name);
			const VbaExpr* innerTarget = stmt->target->target.get();
			if ((outerName == "color" || outerName == "colorindex")
				&& innerTarget->kind == vbaExprPropertyAccess
				&& Lowercase(innerTarget->name) == "interior")
			{
				Value rangeValue;
				if (!Eval(innerTarget->target.get(), &rangeValue))
					return execError;
				VbaRuntimeError colorErr;
				if (!VbaSetInteriorColor(rangeValue, outerName == "colorindex", rhs, &colorErr, stmt->line))
				{
					SetError(colorErr.message, colorErr.line);
					return execError;
				}
				return execNormal;
			}

			// "Range(...).Font.Bold = valore" (e Italic/Size/Name) --
			// stesso principio esatto del blocco Interior sopra, su
			// "Font" invece di "Interior".
			if ((outerName == "bold" || outerName == "italic" || outerName == "size"
					|| outerName == "name")
				&& innerTarget->kind == vbaExprPropertyAccess
				&& Lowercase(innerTarget->name) == "font")
			{
				Value rangeValue;
				if (!Eval(innerTarget->target.get(), &rangeValue))
					return execError;
				VbaRuntimeError fontErr;
				if (!VbaSetFontProperty(rangeValue, outerName, rhs, &fontErr, stmt->line))
				{
					SetError(fontErr.message, fontErr.line);
					return execError;
				}
				return execNormal;
			}
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

	// Fase 6e: "With expr" ... "End With" -- "expr" si valuta/risolve
	// UNA SOLA VOLTA qui, mai di nuovo per ogni ".Nome" dentro "body"
	// (vedi VbaWithTarget/EvalWithImplicitTarget). Prova PRIMA a
	// risolvere "expr" come un'ISTANZA di struct (riusando
	// ResolveStructInstanceStep -- stessa funzione che risolve un
	// passo INTERMEDIO di una catena di campi, Fase 6d, qui applicata
	// all'INTERA espressione del "With": "With ThisVM.Screen(X,Y)" si
	// risolve esattamente come se "Screen(X,Y)" fosse un passo
	// intermedio, perche' STRUTTURALMENTE lo e'); se non e' affatto un
	// accesso a struct (es. "With Range(\"A1\")"), valuta "expr" come
	// un Value normale. "PushWithTarget"/"PopWithTarget" sono SEMPRE
	// in coppia, anche quando "body" termina con un errore o un Exit
	// For/Do/Sub -- un "With" rimasto a meta' impilato sporcherebbe
	// l'esecuzione di cio' che segue.
	ExecResult ExecuteWith(const VbaStmt* stmt)
	{
		VbaWithTarget withTarget;
		std::string structErr;
		VbaStructInstance* structTarget = ResolveStructInstanceStep(stmt->target.get(), fEnv, &structErr);
		if (structTarget)
		{
			withTarget.isStruct = true;
			withTarget.structInstance = structTarget;
		}
		else if (!structErr.empty())
		{
			SetError(structErr, stmt->line);
			return execError;
		}
		else
		{
			Value v;
			if (!Eval(stmt->target.get(), &v))
				return execError;
			withTarget.isStruct = false;
			withTarget.value = v;
		}

		fEnv->PushWithTarget(withTarget);
		ExecResult r = ExecuteStmtList(stmt->body);
		fEnv->PopWithTarget();
		return r;
	}

	// Fase 6g: "Open percorso For Binary Access Read As #fileNumber" --
	// "percorso" deve GIA' essere stato approvato dall'utente in questa
	// esecuzione (vedi VbaEnvironment::ApproveOpenFilePath, scritto da
	// "Application.GetOpenFilename" in VbaEvaluator.cpp): e' questa la
	// decisione di sicurezza di Fase 6g, non un prefisso di cartella.
	ExecResult ExecuteOpen(const VbaStmt* stmt)
	{
		Value pathValue;
		if (!Eval(stmt->expr.get(), &pathValue))
			return execError;
		std::string path = VbaToText(pathValue);

		Value fileNumValue;
		if (!Eval(stmt->fileNumber.get(), &fileNumValue))
			return execError;
		int fileNumber = (int)VbaToNumber(fileNumValue);

		if (!fEnv->IsOpenFilePathApproved(path))
		{
			SetError("\"Open\" puo' leggere solo un file che l'utente ha scelto "
				"esplicitamente in questa esecuzione tramite un selettore di file "
				"nativo (Application.GetOpenFilename) -- decisione di sicurezza, vedi "
				"ROADMAP.md Fase 6g", stmt->line);
			return execError;
		}

		std::string openErr;
		if (!fEnv->OpenBinaryFileForReading(fileNumber, path, &openErr))
		{
			SetError(openErr, stmt->line);
			return execError;
		}
		return execNormal;
	}

	// Fase 6g: "Close [#fileNumber]" -- "fileNumber" assente chiude
	// TUTTI i file aperti in questo VbaEnvironment, stesso
	// comportamento del vero VBA.
	ExecResult ExecuteClose(const VbaStmt* stmt)
	{
		if (!stmt->fileNumber)
		{
			fEnv->CloseAllFiles();
			return execNormal;
		}
		Value fileNumValue;
		if (!Eval(stmt->fileNumber.get(), &fileNumValue))
			return execError;
		fEnv->CloseFile((int)VbaToNumber(fileNumValue));
		return execNormal;
	}

	// Fase 6g: "Get #fileNumber, , nomeArray" -- SOLO la posizione
	// omessa (vedi ParseGet in VbaStmtParser.cpp e vbaStmtGet in
	// VbaAst.h): legge l'INTERO array di destinazione (gia'
	// dimensionato da un "ReDim" precedente) a partire dalla posizione
	// corrente del file, un byte per elemento -- oltre la fine del
	// file legge 0, mai un accesso fuori dai limiti del buffer letto
	// dall'Open.
	ExecResult ExecuteGet(const VbaStmt* stmt)
	{
		if (stmt->expr)
		{
			SetError("\"Get\" con un numero di record esplicito non e' supportato in "
				"questo sottoinsieme (solo \"Get #file, , variabile\", una lettura "
				"sequenziale dell'intero array)", stmt->line);
			return execError;
		}
		Value fileNumValue;
		if (!Eval(stmt->fileNumber.get(), &fileNumValue))
			return execError;
		int fileNumber = (int)VbaToNumber(fileNumValue);

		VbaOpenFile* file = fEnv->FindOpenFile(fileNumber);
		if (!file)
		{
			SetError("\"Get\" su un numero di file non aperto", stmt->line);
			return execError;
		}
		if (!fEnv->IsArray(stmt->varName))
		{
			SetError("\"Get\" richiede un array di destinazione (es. \"Dim rom() As Byte\" "
				"ridimensionato con \"ReDim\" prima di questo \"Get\")", stmt->line);
			return execError;
		}
		std::vector<int> dims;
		if (!fEnv->GetArrayDims(stmt->varName, &dims) || dims.size() != 1)
		{
			SetError("\"Get\" supporta solo un array a UNA dimensione in questo "
				"sottoinsieme", stmt->line);
			return execError;
		}

		int count = dims[0];
		for (int i = 0; i < count; i++)
		{
			unsigned char byteValue = (file->position < file->data.size())
				? file->data[file->position] : 0;
			file->position++;
			std::vector<int> index(1, i);
			fEnv->SetArrayElement(stmt->varName, index, Value((double)byteValue));
		}
		return execNormal;
	}

	// Fase 6g: "ReDim nomeArray(limiti)" -- ogni limite si valuta a
	// RUNTIME (a differenza di "Dim", sempre un letterale), poi
	// ridimensiona e azzera esattamente come DeclareArray farebbe per
	// un "Dim" con quegli stessi limiti (niente "Preserve" in questo
	// sottoinsieme, vedi ParseReDim).
	ExecResult ExecuteReDim(const VbaStmt* stmt)
	{
		std::vector<int> dims;
		for (size_t i = 0; i < stmt->reDimDims.size(); i++)
		{
			Value v;
			if (!Eval(stmt->reDimDims[i].get(), &v))
				return execError;
			int upper = (int)VbaToNumber(v);
			if (upper < 0)
			{
				SetError("\"ReDim\" con un limite superiore negativo non e' valido", stmt->line);
				return execError;
			}
			dims.push_back(upper + 1);
		}
		fEnv->DeclareArray(stmt->varName, dims);
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
