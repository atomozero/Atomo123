/*
	VbaAst.h

	Nodo dell'albero sintattico per un'espressione VBA (il sottoinsieme
	deciso in ROADMAP.md) -- prodotto da VbaExprParser.h (Fase 2b),
	consumato dall'interprete (Fase 3, non ancora scritta) e, in
	seguito, anche dal parser delle istruzioni (Fase 2c).

	std::unique_ptr per i figli, non puntatori grezzi: un albero
	ricorsivo si distrugge da solo in modo corretto senza un
	distruttore scritto a mano, niente rischio di perdita di memoria
	per un ramo dimenticato.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_AST_H
#define VBA_AST_H

#include <memory>
#include <string>
#include <vector>

enum VbaExprKind {
	vbaExprNumber,         // literale numerico, vedi numberValue
	vbaExprString,         // literale stringa, vedi stringValue
	vbaExprBool,           // True/False, vedi boolValue
	vbaExprVariable,       // riferimento a una variabile per nome, vedi name
	vbaExprBinary,         // op con due figli (children[0] op children[1]), vedi op
	vbaExprUnary,          // op con un figlio (op children[0]), vedi op -- solo "-" e "Not" in questo sottoinsieme
	vbaExprCall,           // name(children...) -- una chiamata a funzione o un accesso indicizzato
	                        // tipo Range("A1")/Cells(i,1): "name" e' il nome, "children" gli argomenti
	vbaExprPropertyAccess, // target.name -- un accesso a proprieta'/metodo senza argomenti su "target"
	                        // (es. Range("A1").Value): per una chiamata CON argomenti dopo il punto
	                        // (rara in questo sottoinsieme) il chiamante compone un vbaExprCall con
	                        // target come proprio "target" invece
};

struct VbaExpr {
	VbaExprKind kind;
	double numberValue;
	std::string stringValue;
	bool boolValue;
	std::string name;   // nome variabile, nome funzione/proprieta', o testo dell'operatore (op)
	std::string op;      // solo per vbaExprBinary/vbaExprUnary: "+","-","*","/","^","&","=","<>",
	                      // "<",">","<=",">=","And","Or","Not","Mod"
	std::unique_ptr<VbaExpr> target; // solo per vbaExprPropertyAccess, e opzionalmente vbaExprCall
	                                   // (una chiamata di metodo su un target, es. Range("A1").Select)
	std::vector<std::unique_ptr<VbaExpr>> children; // operandi (binario: 2, unario: 1) o argomenti di chiamata
	int line;

	VbaExpr(VbaExprKind k) : kind(k), numberValue(0.0), boolValue(false), line(0) {}
};

// ---------------------------------------------------------------------
// Istruzioni (Fase 2c, sopra le espressioni di VbaExpr): ancora il
// sottoinsieme esplicito deciso in ROADMAP.md -- Sub/End Sub, Dim,
// assegnazione/Set, If/ElseIf/Else/End If (anche la forma breve su una
// sola riga), For/Next, Exit For/Sub, e un'espressione nuda usata come
// istruzione (una chiamata di metodo per il suo effetto, es.
// "Range(\"A1\").Select").
// ---------------------------------------------------------------------

enum VbaStmtKind {
	vbaStmtAssign,   // target = expr -- vedi "target"/"expr"
	vbaStmtSet,       // Set target = expr -- VBA distingue l'assegnazione di un riferimento
	                   // a oggetto da un valore, ma qui si tratta allo stesso identico modo
	                   // a livello di albero sintattico, solo "kind" li distingue
	vbaStmtExprStmt, // un'espressione nuda usata come istruzione (una chiamata per il suo
	                   // effetto, es. "Range(\"A1\").Select" o "Foo()") -- vedi "expr"
	vbaStmtDim,       // Dim varName [As ...] -- il tipo (se presente) viene letto e scartato,
	                   // questo sottoinsieme tratta ogni variabile come Variant
	vbaStmtIf,        // If expr Then body [Else elseBody] End If -- un "ElseIf" si rappresenta
	                   // come un singolo vbaStmtIf annidato dentro elseBody, vedi il parser
	vbaStmtFor,       // For varName = expr To forEnd [Step forStep] body Next [varName]
	vbaStmtForEach,   // For Each varName In expr body Next [varName] -- "expr" deve valutare
	                   // a un intervallo (Range/Cells), vedi VbaExecutor.cpp; SOLO Range in
	                   // questo sottoinsieme, niente array/Collection generici (coerente col
	                   // resto del modello a oggetti, che ha solo Range/Cells)
	vbaStmtExitFor,
	vbaStmtExitSub,
	vbaStmtSelectCase, // Select Case expr [Case v1, v2, ...  body]... [Case Else elseBody] End Select
	                     // -- vedi "cases"/"elseBody" sotto. SOLO confronto per uguaglianza
	                     // (come "="), NIENTE "Case 1 To 5" o "Case Is > 5" in questo
	                     // sottoinsieme (limite onesto, non un dato sbagliato silenzioso: il
	                     // parser rifiuta "To"/"Is" dopo "Case" con un errore chiaro).
	vbaStmtDoLoop,     // Do [While|Until expr] body Loop  -- oppure --  Do body Loop [While|Until expr]
	                     // -- vedi doIsPreTest/doIsUntil sotto. "expr" nullptr = nessuna
	                     // condizione affatto ("Do ... Loop" nudo, esce solo con Exit Do/Exit
	                     // Sub) -- in quel caso doIsPreTest/doIsUntil non contano. Avere
	                     // ENTRAMBE le posizioni ("Do While..." E "...Loop While") sulla
	                     // stessa istruzione e' un errore di sintassi, non un'ambiguita'
	                     // risolta a caso.
	vbaStmtExitDo,
};

struct VbaStmt; // completa piu' sotto -- serve solo per unique_ptr<VbaStmt> qui

// Un singolo "Case v1, v2, ..." di un Select Case (vedi vbaStmtSelectCase
// sopra): zero o piu' valori di confronto (separati da virgola, come
// VBA vero) piu' il corpo da eseguire se uno qualunque combacia con
// l'espressione del Select Case (stesso confronto di "=", vedi
// VbaExecutor.cpp). "Case Else" si rappresenta a parte, in
// VbaStmt::elseBody, non come un elemento di questa lista (deve
// sempre essere l'ultimo, come nel vero VBA, e un corpo senza nessun
// valore di confronto qui dentro non avrebbe senso).
struct VbaCaseClause {
	std::vector<std::unique_ptr<VbaExpr>> matchValues;
	std::vector<std::unique_ptr<VbaStmt>> body;
	int line;

	VbaCaseClause() : line(0) {}
};

struct VbaStmt {
	VbaStmtKind kind;
	std::unique_ptr<VbaExpr> target;  // Assign/Set: il lato sinistro (dove scrivere)
	std::unique_ptr<VbaExpr> expr;     // Assign/Set: il lato destro; For: il valore iniziale;
	                                     // ExprStmt: l'espressione stessa; If/SelectCase: la
	                                     // condizione/l'espressione da confrontare
	std::unique_ptr<VbaExpr> forEnd;   // solo For: il valore finale ("To ...")
	std::unique_ptr<VbaExpr> forStep;  // solo For: l'incremento ("Step ..."), nullptr = 1 implicito
	std::string varName;                // Dim/For: il nome della variabile
	std::vector<std::unique_ptr<VbaStmt>> body;     // If (ramo Then) / For: il corpo
	std::vector<std::unique_ptr<VbaStmt>> elseBody; // If: il ramo Else (eventualmente un solo
	                                                   // vbaStmtIf annidato per un "ElseIf");
	                                                   // SelectCase: il ramo "Case Else"
	std::vector<VbaCaseClause> cases;                 // solo SelectCase, nell'ordine scritto
	bool doIsPreTest;  // solo DoLoop, e solo se "expr" non e' nullptr: vero = "Do While/Until
	                     // ... Loop" (condizione controllata PRIMA di ogni giro, puo' non
	                     // eseguire il corpo nemmeno una volta); falso = "Do ... Loop While/
	                     // Until" (controllata DOPO, il corpo gira SEMPRE almeno una volta)
	bool doIsUntil;    // solo DoLoop, e solo se "expr" non e' nullptr: vero = "Until" (continua
	                     // finche' "expr" e' falsa); falso = "While" (continua finche' e' vera)
	int line;

	VbaStmt(VbaStmtKind k) : kind(k), doIsPreTest(true), doIsUntil(false), line(0) {}
};

// Un'intera procedura: "Sub Nome(...)" ... "End Sub", oppure (Fase 6b)
// "Function Nome(...) [As Tipo]" ... "End Function" -- "isFunction"
// distingue le due SOLO per sapere quale parola chiave chiudente
// aspettarsi e se leggere un valore di ritorno dopo l'esecuzione
// (VbaExecutor.cpp/VbaEvaluator.cpp): l'esecuzione del corpo e'
// identica. Il valore di ritorno di una Function usa la stessa
// convenzione del vero VBA -- una variabile con lo stesso nome della
// Function, assegnata da una qualunque istruzione dentro il corpo
// ("NomeFunzione = risultato"), Empty (qui: 0.0) se non mai assegnata
// -- quindi non serve nessun nodo dedicato nell'albero: e' una normale
// vbaStmtAssign su una variabile normale, dichiarata in anticipo da
// ExecuteVbaSub prima di eseguire il corpo.
//
// Fase 6a: i parametri sono SOLO per valore ("ByVal", anche quando
// scritto senza -- e' il default di questo sottoinsieme, non quello
// del vero VBA, che userebbe "ByRef" -- vedi ROADMAP.md), quindi basta
// un elenco di nomi: ogni chiamata lega un argomento valutato a una
// NUOVA variabile locale con quel nome nell'ambiente della procedura
// chiamata, niente aliasing col chiamante da rappresentare qui. Un
// eventuale "As Tipo" dopo un parametro (o dopo l'intero elenco, per
// il tipo di ritorno di una Function) si legge e si scarta, stesso
// trattamento del tipo su "Dim".
struct VbaSub {
	std::string name;
	std::vector<std::string> params;
	std::vector<std::unique_ptr<VbaStmt>> body;
	bool isFunction;
	int line;

	VbaSub() : isFunction(false), line(0) {}
};

#endif
