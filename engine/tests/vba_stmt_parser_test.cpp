/*
	vba_stmt_parser_test.cpp

	Verifica ParseVbaSub (VbaStmtParser.h -- Path to full Excel parity,
	VBA/macro execution, Fase 2c, l'ultima di questa parte della Fase
	2) convertendo l'albero risultante in una rappresentazione
	testuale compatta (DumpSub/DumpStmt sotto), stesso principio gia'
	usato in vba_expr_parser_test.cpp.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <string>
#include <vector>

#include "VbaStmtParser.h"
#include "VbaToken.h"

static int gFailures = 0;

static void Check(bool condition, const char* what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

static std::string FormatNumber(double v)
{
	char buf[64];
	if (v == (long long)v)
		snprintf(buf, sizeof(buf), "%lld", (long long)v);
	else
		snprintf(buf, sizeof(buf), "%g", v);
	return buf;
}

static std::string DumpExpr(const VbaExpr* e)
{
	if (e == NULL)
		return "<null>";
	switch (e->kind)
	{
		case vbaExprNumber: return FormatNumber(e->numberValue);
		case vbaExprString: return "\"" + e->stringValue + "\"";
		case vbaExprBool: return e->boolValue ? "True" : "False";
		case vbaExprVariable: return e->name;
		case vbaExprUnary: return "(" + e->op + " " + DumpExpr(e->children[0].get()) + ")";
		case vbaExprBinary:
			return "(" + DumpExpr(e->children[0].get()) + " " + e->op + " "
				+ DumpExpr(e->children[1].get()) + ")";
		case vbaExprCall:
		{
			std::string out;
			if (e->target)
				out += DumpExpr(e->target.get()) + ".";
			out += e->name + "(";
			for (size_t i = 0; i < e->children.size(); i++)
			{
				if (i > 0)
					out += ", ";
				out += DumpExpr(e->children[i].get());
			}
			out += ")";
			return out;
		}
		case vbaExprPropertyAccess:
			return DumpExpr(e->target.get()) + "." + e->name;
	}
	return "?";
}

static std::string DumpStmtList(const std::vector<std::unique_ptr<VbaStmt>>& stmts);

static std::string DumpStmt(const VbaStmt* s)
{
	switch (s->kind)
	{
		case vbaStmtAssign:
			return DumpExpr(s->target.get()) + " := " + DumpExpr(s->expr.get());
		case vbaStmtSet:
			return "Set " + DumpExpr(s->target.get()) + " := " + DumpExpr(s->expr.get());
		case vbaStmtExprStmt:
			return DumpExpr(s->expr.get());
		case vbaStmtDim:
		{
			std::string dims;
			if (!s->arrayDims.empty())
			{
				dims += "(";
				for (size_t i = 0; i < s->arrayDims.size(); i++)
				{
					if (i > 0)
						dims += ", ";
					dims += std::to_string(s->arrayDims[i]); // conteggio elementi, non il limite scritto
				}
				dims += ")";
			}
			std::string asType = s->asTypeName.empty() ? "" : (" As " + s->asTypeName);
			return "Dim " + s->varName + dims + asType;
		}
		case vbaStmtIf:
		{
			std::string out = "If " + DumpExpr(s->expr.get()) + " Then [" + DumpStmtList(s->body) + "]";
			if (!s->elseBody.empty())
				out += " Else [" + DumpStmtList(s->elseBody) + "]";
			return out;
		}
		case vbaStmtFor:
		{
			std::string out = "For " + s->varName + " = " + DumpExpr(s->expr.get())
				+ " To " + DumpExpr(s->forEnd.get());
			if (s->forStep)
				out += " Step " + DumpExpr(s->forStep.get());
			out += " [" + DumpStmtList(s->body) + "]";
			return out;
		}
		case vbaStmtExitFor: return "Exit For";
		case vbaStmtExitSub: return "Exit Sub";
		case vbaStmtOpen:
			return "Open " + DumpExpr(s->expr.get()) + " As #" + DumpExpr(s->fileNumber.get());
		case vbaStmtClose:
			return s->fileNumber ? ("Close #" + DumpExpr(s->fileNumber.get())) : "Close";
		case vbaStmtGet:
			return "Get #" + DumpExpr(s->fileNumber.get()) + ", "
				+ (s->expr ? DumpExpr(s->expr.get()) : "") + ", " + s->varName;
		case vbaStmtReDim:
		{
			std::string dims;
			for (size_t i = 0; i < s->reDimDims.size(); i++)
			{
				if (i > 0)
					dims += ", ";
				dims += DumpExpr(s->reDimDims[i].get());
			}
			return "ReDim " + s->varName + "(" + dims + ")";
		}
	}
	return "?";
}

static std::string DumpStmtList(const std::vector<std::unique_ptr<VbaStmt>>& stmts)
{
	std::string out;
	for (size_t i = 0; i < stmts.size(); i++)
	{
		if (i > 0)
			out += "; ";
		out += DumpStmt(stmts[i].get());
	}
	return out;
}

static std::string ParseAndDump(const char* source)
{
	std::vector<VbaToken> tokens;
	TokenizeVba(source, &tokens);
	size_t pos = 0;
	VbaParseError error;
	std::unique_ptr<VbaSub> sub = ParseVbaSub(tokens, pos, &error);
	if (!sub)
		return "<errore riga " + std::to_string(error.line) + ": " + error.message + ">";
	std::string params;
	for (size_t i = 0; i < sub->params.size(); i++)
	{
		if (i > 0)
			params += ", ";
		params += sub->params[i];
	}
	return std::string(sub->isFunction ? "Function " : "Sub ") + sub->name + "(" + params + ") ["
		+ DumpStmtList(sub->body) + "]";
}

static void CheckSub(const char* source, const char* expected)
{
	std::string actual = ParseAndDump(source);
	bool ok = actual == expected;
	std::string what = std::string(source) + " -> " + actual;
	if (!ok)
		what += " (atteso: " + std::string(expected) + ")";
	Check(ok, what.c_str());
}

// ---- Fase 6d: "Type ... End Type" ----

static std::string DumpTypeField(const VbaTypeField& f)
{
	std::string out = f.name;
	if (!f.arrayDims.empty())
	{
		out += "(";
		for (size_t i = 0; i < f.arrayDims.size(); i++)
		{
			if (i > 0)
				out += ", ";
			out += std::to_string(f.arrayDims[i]);
		}
		out += ")";
	}
	return out + " As " + f.typeName;
}

static std::string ParseAndDumpType(const char* source)
{
	std::vector<VbaToken> tokens;
	TokenizeVba(source, &tokens);
	size_t pos = 0;
	VbaParseError error;
	std::unique_ptr<VbaTypeDecl> decl = ParseVbaTypeDecl(tokens, pos, &error);
	if (!decl)
		return "<errore riga " + std::to_string(error.line) + ": " + error.message + ">";
	std::string out = "Type " + decl->name + " [";
	for (size_t i = 0; i < decl->fields.size(); i++)
	{
		if (i > 0)
			out += "; ";
		out += DumpTypeField(decl->fields[i]);
	}
	return out + "]";
}

static void CheckType(const char* source, const char* expected)
{
	std::string actual = ParseAndDumpType(source);
	bool ok = actual == expected;
	std::string what = std::string(source) + " -> " + actual;
	if (!ok)
		what += " (atteso: " + std::string(expected) + ")";
	Check(ok, what.c_str());
}

int main()
{
	// ---- Sub vuota ----
	CheckSub("Sub Foo()\r\nEnd Sub\r\n", "Sub Foo() []");

	// ---- Dim e assegnazione ----
	CheckSub("Sub Foo()\r\nDim x\r\nx = 5\r\nEnd Sub\r\n",
		"Sub Foo() [Dim x; x := 5]");
	CheckSub("Sub Foo()\r\nDim x As Integer\r\nEnd Sub\r\n",
		"Sub Foo() [Dim x As Integer]"); // Fase 6d: il nome del tipo si CATTURA (non si scarta piu'
		                                   // al parsing) -- se e' uno scalare incorporato o un Type
		                                   // lo decide VbaExecutor.cpp a runtime, vedi VbaAst.h

	// ---- Assegnazione a una proprieta' (il caso piu' comune in una macro reale) ----
	CheckSub("Sub Foo()\r\nRange(\"A1\").Value = 42\r\nEnd Sub\r\n",
		"Sub Foo() [Range(\"A1\").Value := 42]");

	// ---- Set (assegnazione di riferimento a oggetto) ----
	CheckSub("Sub Foo()\r\nSet x = Range(\"A1\")\r\nEnd Sub\r\n",
		"Sub Foo() [Set x := Range(\"A1\")]");

	// ---- Una chiamata nuda usata come istruzione (per il suo effetto) ----
	CheckSub("Sub Foo()\r\nRange(\"A1\").Select\r\nEnd Sub\r\n",
		"Sub Foo() [Range(\"A1\").Select]");

	// ---- If/Then/End If, forma a blocco ----
	CheckSub(
		"Sub Foo()\r\n"
		"If x > 0 Then\r\n"
		"    y = 1\r\n"
		"End If\r\n"
		"End Sub\r\n",
		"Sub Foo() [If (x > 0) Then [y := 1]]");

	// ---- If/Then su una sola riga, niente End If ----
	CheckSub("Sub Foo()\r\nIf x > 0 Then y = 1\r\nEnd Sub\r\n",
		"Sub Foo() [If (x > 0) Then [y := 1]]");

	// ---- If/Else/End If ----
	CheckSub(
		"Sub Foo()\r\n"
		"If x > 0 Then\r\n"
		"    y = 1\r\n"
		"Else\r\n"
		"    y = 2\r\n"
		"End If\r\n"
		"End Sub\r\n",
		"Sub Foo() [If (x > 0) Then [y := 1] Else [y := 2]]");

	// ---- If/ElseIf/Else/End If: l'ElseIf diventa un If annidato in Else ----
	CheckSub(
		"Sub Foo()\r\n"
		"If x = 1 Then\r\n"
		"    y = 1\r\n"
		"ElseIf x = 2 Then\r\n"
		"    y = 2\r\n"
		"Else\r\n"
		"    y = 3\r\n"
		"End If\r\n"
		"End Sub\r\n",
		"Sub Foo() [If (x = 1) Then [y := 1] Else [If (x = 2) Then [y := 2] Else [y := 3]]]");

	// ---- For/Next, con e senza Step ----
	CheckSub(
		"Sub Foo()\r\n"
		"For i = 1 To 10\r\n"
		"    x = i\r\n"
		"Next i\r\n"
		"End Sub\r\n",
		"Sub Foo() [For i = 1 To 10 [x := i]]");
	CheckSub(
		"Sub Foo()\r\n"
		"For i = 10 To 1 Step -1\r\n"
		"    x = i\r\n"
		"Next\r\n" // "Next" senza nome dopo e' valido
		"End Sub\r\n",
		"Sub Foo() [For i = 10 To 1 Step (- 1) [x := i]]");

	// ---- Exit For / Exit Sub ----
	CheckSub(
		"Sub Foo()\r\n"
		"For i = 1 To 10\r\n"
		"    If i = 5 Then Exit For\r\n"
		"Next i\r\n"
		"End Sub\r\n",
		"Sub Foo() [For i = 1 To 10 [If (i = 5) Then [Exit For]]]");
	CheckSub(
		"Sub Foo()\r\n"
		"If x = 0 Then Exit Sub\r\n"
		"y = 1\r\n"
		"End Sub\r\n",
		"Sub Foo() [If (x = 0) Then [Exit Sub]; y := 1]");
	// Fase 6g (seguito): "Exit Function" era rimasta scoperta da quando
	// le Function sono arrivate (Fase 6b) -- trovata controllando il
	// vero sorgente chip-8-vba ("If bytes = 0 Then / Exit Function /
	// End If" in DrawSprite). Stesso nodo "Exit Sub" (DumpStmt non
	// distingue le due forme, l'esecuzione e' identica, vedi VbaAst.h).
	CheckSub(
		"Function Foo()\r\n"
		"If x = 0 Then Exit Function\r\n"
		"y = 1\r\n"
		"End Function\r\n",
		"Function Foo() [If (x = 0) Then [Exit Sub]; y := 1]");

	// ---- Uno spezzone realistico completo, lo stesso visto nel test del lexer ----
	CheckSub(
		"Sub FillRange()\r\n"
		"    Dim i As Integer\r\n"
		"    For i = 1 To 10\r\n"
		"        If i Mod 2 = 0 Then\r\n"
		"            Cells(i, 1).Value = \"even\"\r\n"
		"        Else\r\n"
		"            Cells(i, 1).Value = \"odd\"\r\n"
		"        End If\r\n"
		"    Next i\r\n"
		"End Sub\r\n",
		"Sub FillRange() [Dim i As Integer; For i = 1 To 10 "
		"[If ((i Mod 2) = 0) Then [Cells(i, 1).Value := \"even\"] "
		"Else [Cells(i, 1).Value := \"odd\"]]]");

	// ---- Un If annidato dentro un For, verificando che "Next" fermi "If" al posto giusto ----
	CheckSub(
		"Sub Foo()\r\n"
		"For i = 1 To 3\r\n"
		"    If i = 2 Then\r\n"
		"        y = 1\r\n"
		"    End If\r\n"
		"Next i\r\n"
		"End Sub\r\n",
		"Sub Foo() [For i = 1 To 3 [If (i = 2) Then [y := 1]]]");

	// ---- Fase 6a: parametri su Sub (solo per valore in questo
	// sottoinsieme -- vedi VbaAst.h) ----
	CheckSub("Sub Foo(x)\r\nEnd Sub\r\n", "Sub Foo(x) []");
	CheckSub("Sub Foo(x, y)\r\nEnd Sub\r\n", "Sub Foo(x, y) []");
	CheckSub("Sub Foo(ByVal x As Integer, y As Long)\r\nEnd Sub\r\n",
		"Sub Foo(x, y) []"); // "ByVal" e il tipo dopo "As" si leggono e si scartano
	CheckSub("Sub Foo(x)\r\nResult = x * 2\r\nEnd Sub\r\n",
		"Sub Foo(x) [Result := (x * 2)]");

	// ---- Fase 6a: "Call" e' zucchero sintattico, stessa istruzione nuda ----
	CheckSub("Sub Foo()\r\nCall Bar(1, 2)\r\nEnd Sub\r\n",
		"Sub Foo() [Bar(1, 2)]");

	// ---- "Dim a, b, c" -- un SOLO "Dim" del vero VBA, ma PIU' di un
	// vbaStmtDim qui (nessun nodo "lista" dedicato, vedi
	// ParseDimStatementList) -- trovato indagando il sorgente reale
	// del CHIP-8 ("Dim i, lastN As Long" in InitConstants) ----
	CheckSub("Sub Foo()\r\nDim a, b, c\r\nEnd Sub\r\n", "Sub Foo() [Dim a; Dim b; Dim c]");
	CheckSub("Sub Foo()\r\nDim i, lastN As Long\r\nEnd Sub\r\n",
		"Sub Foo() [Dim i; Dim lastN As Long]"); // SOLO l'ultimo dichiaratore prende "As Long"
	CheckSub("Sub Foo()\r\nDim a As Integer, b As String\r\nEnd Sub\r\n",
		"Sub Foo() [Dim a As Integer; Dim b As String]"); // ogni dichiaratore puo' avere il proprio tipo
	CheckSub("Sub Foo()\r\nDim V(3), W(1, 1)\r\nx = 1\r\nEnd Sub\r\n",
		"Sub Foo() [Dim V(4); Dim W(2, 2); x := 1]"); // anche array, mischiati con variabili dopo

	// ---- Fase 6c: array a dimensione fissa su "Dim" -- il numero
	// stampato da DumpStmt e' gia' il CONTEGGIO di elementi (limite
	// superiore scritto + 1, Option Base 0 implicito) ----
	CheckSub("Sub Foo()\r\nDim V(15) As Byte\r\nEnd Sub\r\n",
		"Sub Foo() [Dim V(16) As Byte]"); // 16 elementi, indici 0..15 -- i registri V0..VF del CHIP-8
	CheckSub("Sub Foo()\r\nDim Screen(63, 31)\r\nEnd Sub\r\n",
		"Sub Foo() [Dim Screen(64, 32)]"); // 2D: 64x32, lo schermo del CHIP-8
	CheckSub("Sub Foo()\r\nDim x\r\nEnd Sub\r\n",
		"Sub Foo() [Dim x]"); // nessuna parentesi: ancora una variabile scalare normale
	CheckSub("Sub Foo(x)\r\nV(x) = 5\r\nEnd Sub\r\n",
		"Sub Foo(x) [V(x) := 5]"); // una scrittura sembra sintatticamente una chiamata qualunque
	{
		std::string r = ParseAndDump("Sub Foo()\r\nDim V(-1)\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "un limite superiore negativo su un array produce un errore");
	}
	CheckSub("Sub Foo()\r\nDim rom() As Byte\r\nEnd Sub\r\n",
		"Sub Foo() [Dim rom As Byte]"); // Fase 6g: parentesi vuote, array DINAMICO non ancora
		                                  // dimensionato -- indistinguibile da uno scalare (stesso
		                                  // "arrayDims" vuoto) finche' un "ReDim" vero non arriva

	// ---- Fase 6g: Open/Close/Get/ReDim (I/O binario in lettura, vedi
	// ROADMAP.md sul vero macro trigger chip-8-vba che li usa tutti e
	// quattro in sequenza dentro CommandButton1_Click) ----
	CheckSub("Sub Foo()\r\nOpen filePath For Binary Access Read As #file\r\nEnd Sub\r\n",
		"Sub Foo() [Open filePath As #file]");
	CheckSub("Sub Foo()\r\nClose #file\r\nEnd Sub\r\n", "Sub Foo() [Close #file]");
	CheckSub("Sub Foo()\r\nClose\r\nEnd Sub\r\n", "Sub Foo() [Close]"); // nudo: chiude TUTTI i file
	CheckSub("Sub Foo()\r\nGet #file, , rom\r\nEnd Sub\r\n", "Sub Foo() [Get #file, , rom]");
	CheckSub("Sub Foo()\r\nReDim rom(0 To n)\r\nEnd Sub\r\n", "Sub Foo() [ReDim rom(n)]");
	CheckSub("Sub Foo()\r\nReDim V(15)\r\nEnd Sub\r\n",
		"Sub Foo() [ReDim V(15)]"); // "espressione" da sola: limite inferiore 0 implicito, come Dim
	{
		std::string r = ParseAndDump("Sub Foo()\r\nOpen x For Output As #1\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "\"Open ... For Output\" (un modo diverso da "
			"Binary/Read) produce un errore chiaro, non un dato sbagliato silenzioso");
	}
	{
		std::string r = ParseAndDump("Sub Foo()\r\nReDim Preserve rom(10)\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "\"ReDim Preserve\" non e' supportato, errore chiaro");
	}
	{
		std::string r = ParseAndDump("Sub Foo()\r\nReDim rom(1 To n)\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "\"ReDim\" con un limite inferiore diverso da zero "
			"produce un errore chiaro");
	}

	// ---- Fase 6g (seguito): "Step" NON e' una parola chiave riservata
	// -- una vera "Sub Step()" (trovata controllando il vero sorgente
	// chip-8-vba) analizza senza errori, e "For ... Step ..." continua
	// a funzionare esattamente come prima ----
	CheckSub("Sub Step()\r\nx = 1\r\nEnd Sub\r\n", "Sub Step() [x := 1]");
	CheckSub("Sub Foo()\r\nFor i = 10 To 1 Step -1\r\nx = i\r\nNext i\r\nEnd Sub\r\n",
		"Sub Foo() [For i = 10 To 1 Step (- 1) [x := i]]");

	// ---- Fase 6g: "MsgBox expr" -- un caso unico hardcoded, riscritto
	// come una chiamata-istruzione nuda (vedi ParseMsgBox) ----
	CheckSub("Sub Foo()\r\nMsgBox \"ciao\"\r\nEnd Sub\r\n", "Sub Foo() [MsgBox(\"ciao\")]");

	// ---- Fase 6g (seguito): "NomeSub arg1, arg2" -- la "forma a
	// comando" del vero VBA, senza parentesi (trovata controllando il
	// vero sorgente chip-8-vba: "SetScreenPixel i, j, 0", "VMSetRunning
	// False", "TODO stringa") -- riscritta nell'ALBERO esattamente come
	// la forma con parentesi equivalente, nessuna differenza a valle ----
	CheckSub("Sub Foo()\r\nSetScreenPixel i, j, 0\r\nEnd Sub\r\n",
		"Sub Foo() [SetScreenPixel(i, j, 0)]");
	CheckSub("Sub Foo()\r\nVMSetRunning False\r\nEnd Sub\r\n", "Sub Foo() [VMSetRunning(False)]");
	CheckSub("Sub Foo()\r\nTODO \"errore\"\r\nEnd Sub\r\n", "Sub Foo() [TODO(\"errore\")]");
	CheckSub("Sub Foo()\r\nFoo -1\r\nEnd Sub\r\n", "Sub Foo() [Foo((- 1))]"); // un argomento negativo
	// Zero argomenti (nessun token dopo il nome sulla stessa riga):
	// resta la forma di SEMPRE, una variabile nuda, non una chiamata --
	// l'eventuale risoluzione come Sub sorella avviene a RUNTIME
	// (VbaEvaluator.cpp, EvalVariable), non qui.
	CheckSub("Sub Foo()\r\nRedraw\r\nEnd Sub\r\n", "Sub Foo() [Redraw]");
	// Non deve rompere nessuna forma esistente: nome solo con parentesi,
	// assegnazione, accesso a proprieta', "Call".
	CheckSub("Sub Foo()\r\nRedraw()\r\nEnd Sub\r\n", "Sub Foo() [Redraw()]");
	CheckSub("Sub Foo()\r\nx = 5\r\nEnd Sub\r\n", "Sub Foo() [x := 5]");
	CheckSub("Sub Foo()\r\nRange(\"A1\").Select\r\nEnd Sub\r\n", "Sub Foo() [Range(\"A1\").Select]");
	CheckSub("Sub Foo()\r\nCall Redraw\r\nEnd Sub\r\n", "Sub Foo() [Redraw]"); // "Call" su zero argomenti

	// ---- Fase 6g: "Dim file As Integer: file = FreeFile" -- ":" separa
	// due istruzioni sulla stessa riga esattamente come un newline (gia'
	// cosi' dalla tokenizzazione, Fase 2a: nessuna modifica al parser
	// e' servita per questo) ----
	CheckSub("Sub Foo()\r\nDim file As Integer: file = FreeFile\r\nEnd Sub\r\n",
		"Sub Foo() [Dim file As Integer; file := FreeFile]");

	// ---- Fase 6d: "Type Nome ... End Type" -- un elenco di campi,
	// scalari o array (stessa convenzione "conteggio elementi" di
	// VbaStmt::arrayDims sopra), senza corpo eseguibile ----
	CheckType("Type ScreenPixel\r\nvalue As Byte\r\nDirty As Byte\r\nEnd Type\r\n",
		"Type ScreenPixel [value As Byte; Dirty As Byte]");
	CheckType(
		"Type VM\r\n"
		"Running As Boolean\r\n"
		"PC As Long\r\n"
		"V(15) As Byte\r\n"
		"Screen(63, 31) As ScreenPixel\r\n"
		"End Type\r\n",
		"Type VM [Running As Boolean; PC As Long; V(16) As Byte; Screen(64, 32) As ScreenPixel]");
	CheckType("Type Empty\r\nEnd Type\r\n", "Type Empty []");
	{
		std::string r = ParseAndDumpType("Type VM\r\nPC As Long\r\n");
		Check(r.substr(0, 8) == "<errore ", "\"End Type\" mancante produce un errore, non una lettura "
			"oltre la fine del flusso di token");
	}
	{
		std::string r = ParseAndDumpType("Type VM\r\nPC\r\nEnd Type\r\n"); // campo senza "As Tipo"
		Check(r.substr(0, 8) == "<errore ", "un campo senza \"As Tipo\" produce un errore");
	}

	// ---- Fase 6d: "Dim nome As NomeTipo" -- il parser CATTURA sempre
	// il nome (vedi il test su "Dim x As Integer" sopra): se sia uno
	// scalare incorporato o un Type lo decide VbaExecutor.cpp a
	// runtime, non ParseVbaSub, che non ha alcun registro da
	// consultare qui ----
	CheckSub("Sub Foo()\r\nDim ThisVM As VM\r\nEnd Sub\r\n", "Sub Foo() [Dim ThisVM As VM]");

	// ---- Fase 6b: "Function ... End Function", col valore di ritorno
	// rappresentato come una normale assegnazione alla variabile
	// omonima della Function (vedi VbaAst.h) -- nessun nodo dedicato ----
	CheckSub("Function Foo(x)\r\nFoo = x * 2\r\nEnd Function\r\n",
		"Function Foo(x) [Foo := (x * 2)]");
	CheckSub("Function Foo(ByVal x As Integer) As Long\r\nFoo = x\r\nEnd Function\r\n",
		"Function Foo(x) [Foo := x]"); // il tipo di ritorno dopo "As" si legge e si scarta
	CheckSub("Function Foo()\r\nEnd Function\r\n", "Function Foo() []");
	{
		// "End Sub" per chiudere una "Function" (o viceversa) e' un
		// errore chiaro, non una chiusura tollerata a caso.
		std::string r = ParseAndDump("Function Foo()\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "\"Function\" chiusa con \"End Sub\" produce un errore");
	}

	// ---- Errori di sintassi: un messaggio chiaro, mai un crash ----
	{
		std::string r = ParseAndDump("Sub Foo()\r\nIf x > 0 Then\r\ny = 1\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "un \"If\" senza \"End If\" (trova \"End Sub\" al suo posto) "
			"produce un errore, non un \"If\" silenziosamente incompleto");
	}
	{
		std::string r = ParseAndDump("Sub Foo()\r\nFor i = 1 To 10\r\nx = i\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "un \"For\" senza \"Next\" produce un errore");
	}
	{
		std::string r = ParseAndDump("Sub Foo(\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "parentesi di \"Sub\" non chiusa produce un errore");
	}
	{
		std::string r = ParseAndDump("NotASub\r\n");
		Check(r.substr(0, 8) == "<errore ", "un sorgente che non inizia con \"Sub\" produce un errore");
	}
	{
		std::string r = ParseAndDump("Sub Foo(ByRef x)\r\nEnd Sub\r\n");
		Check(r.substr(0, 8) == "<errore ", "\"ByRef\" su un parametro produce un errore chiaro "
			"(solo ByVal e' supportato in questo sottoinsieme)");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
