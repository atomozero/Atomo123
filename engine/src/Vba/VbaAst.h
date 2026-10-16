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
	vbaExprWithImplicitTarget, // Fase 6e: un "." SENZA niente prima (es. ".Value" dentro un blocco
	                            // "With") -- un segnaposto senza nome/figli, risolto a RUNTIME
	                            // (VbaEvaluator.cpp) in base al "With" piu' vicino ancora attivo
	                            // (VbaEnvironment::GetCurrentWithTarget), non clonando l'albero del
	                            // target del "With" al momento del parsing: compare SEMPRE come
	                            // "target" di un vbaExprCall/vbaExprPropertyAccess che lo avvolge
	                            // (ParsePostfix in VbaExprParser.cpp lo tratta come una "base"
	                            // qualunque), mai da solo. Un "." fuori da qualunque "With" e' un
	                            // errore di RUNTIME pulito (nessun "With" attivo), non un errore di
	                            // sintassi -- vedi ROADMAP.md Fase 6e sul perche' questa scelta
	                            // evita di dover far attraversare un contesto "With" corrente a
	                            // ogni punto di chiamata del parser delle espressioni.
};

struct VbaExpr {
	VbaExprKind kind;
	double numberValue;
	std::string stringValue;
	bool boolValue;
	std::string name;   // nome variabile, nome funzione/proprieta', o testo dell'operatore (op)
	std::string op;      // solo per vbaExprBinary/vbaExprUnary: "+","-","*","/","^","&","=","<>",
	                      // "<",">","<=",">=","And","Or","Xor","Not","Mod","\" (quest'ultimo, Fase
	                      // 6g: la divisione INTERA di VBA, diversa da "/")
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
	                   // questo sottoinsieme tratta ogni variabile come Variant. Fase 6c: Dim
	                   // varName(limiteSuperiore[, limiteSuperiore...]) [As ...] dichiara invece
	                   // un array a dimensione FISSA -- vedi "arrayDims" sotto e VbaEnvironment.h
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
	                     // -- vedi "cases"/"elseBody" sotto. Ogni valore di un "Case" (separati
	                     // da virgola) puo' essere un'uguaglianza semplice, un intervallo
	                     // ("1 To 5") o un confronto ("Is > 5") -- vedi VbaCaseMatch sotto.
	vbaStmtDoLoop,     // Do [While|Until expr] body Loop  -- oppure --  Do body Loop [While|Until expr]
	                     // -- vedi doIsPreTest/doIsUntil sotto. "expr" nullptr = nessuna
	                     // condizione affatto ("Do ... Loop" nudo, esce solo con Exit Do/Exit
	                     // Sub) -- in quel caso doIsPreTest/doIsUntil non contano. Avere
	                     // ENTRAMBE le posizioni ("Do While..." E "...Loop While") sulla
	                     // stessa istruzione e' un errore di sintassi, non un'ambiguita'
	                     // risolta a caso.
	vbaStmtExitDo,
	vbaStmtWith,       // With expr body End With -- "expr" in "target", il corpo in "body" (stessi
	                     // campi di If/For, nessun campo nuovo). ".Nome" dentro "body" e' un
	                     // vbaExprPropertyAccess/vbaExprCall con "target" = vbaExprWithImplicitTarget
	                     // (vedi sopra); l'esecutore (VbaExecutor.cpp) valuta "target" UNA VOLTA
	                     // sola all'ingresso (Range/scalare) O lo risolve a un'istanza di struct
	                     // (Fase 6d, es. "With ThisVM.Screen(X,Y)"), lo mette in cima alla pila dei
	                     // With attivi di VbaEnvironment per tutta la durata di "body", poi lo
	                     // rimuove in uscita -- annidare piu' "With" funziona per costruzione
	                     // (una pila), un "." fa sempre riferimento al PIU' VICINO, mai a uno
	                     // esterno, come nel vero VBA.
	// Fase 6g: I/O binario in lettura -- "expr"/"fileNumber"/"varName"
	// sotto sono condivisi dalle quattro varianti, vedi i commenti sui
	// singoli campi in VbaStmt.
	vbaStmtOpen,       // Open expr(percorso) For Binary Access Read As #fileNumber -- SOLO questa
	                     // forma fissa ("For Binary Access Read"), non analizzata come token
	                     // dedicati (letta per testo in VbaStmtParser.cpp, stesso trattamento di
	                     // "Private"/"ByVal"): "Output"/"Append"/altri modi d'accesso sono un
	                     // errore di sintassi chiaro, non silenziosamente trattati come Read.
	vbaStmtClose,      // Close [#fileNumber] -- "fileNumber" nullptr = "Close" nudo, chiude TUTTI
	                     // i file aperti in questo VbaEnvironment (stesso comportamento del vero
	                     // VBA).
	vbaStmtGet,        // Get #fileNumber, [posizione], varName -- "expr" sotto e' la posizione
	                     // (nullptr = omessa, l'UNICA forma eseguita davvero in questa fase, vedi
	                     // ExecuteGet in VbaExecutor.cpp: una posizione esplicita si analizza ma
	                     // produce un errore di runtime onesto, non un dato sbagliato), "varName"
	                     // e' l'array di BYTE di destinazione (l'INTERO array, mai un elemento
	                     // singolo -- "Get" senza parentesi dopo il nome, a differenza di
	                     // "nomeArray(indice)").
	vbaStmtReDim,      // ReDim varName(limiti) -- SOLO "0 To espressione" (o "espressione" da
	                     // sola, stesso limite inferiore 0 implicito) per asse, mai "Preserve"
	                     // (un secondo "ReDim" azzera l'array, stesso comportamento di un secondo
	                     // "Dim" sullo stesso nome, Fase 6c) -- vedi "reDimDims" sotto.
};

struct VbaStmt; // completa piu' sotto -- serve solo per unique_ptr<VbaStmt> qui

// Le tre forme di un singolo valore dentro un "Case" (separati da
// virgola, possono mescolarsi liberamente nello stesso elenco, come
// nel vero VBA: "Case 1, 3, 5 To 10, Is > 100"):
//  - vbaCaseEquals: un valore singolo, confrontato con "=" (VbaCompareValues)
//  - vbaCaseRange: "lo To hi" (estremi INCLUSI, come il vero VBA)
//  - vbaCaseRelational: "Is <op> value" -- "<op>" e' uno dei sei
//    operatori di confronto gia' tokenizzati altrove in questo lessico
//    ("=","<>","<",">","<=",">="), riusati qui alla lettera.
enum VbaCaseMatchKind {
	vbaCaseEquals,
	vbaCaseRange,
	vbaCaseRelational,
};

struct VbaCaseMatch {
	VbaCaseMatchKind kind;
	// vbaCaseEquals: il valore. vbaCaseRange: l'estremo basso ("lo").
	// vbaCaseRelational: il valore a destra dell'operatore.
	std::unique_ptr<VbaExpr> value;
	std::unique_ptr<VbaExpr> rangeHigh; // SOLO vbaCaseRange: l'estremo alto ("hi")
	std::string relOp;                  // SOLO vbaCaseRelational: "=","<>","<",">","<=",">="
};

// Un singolo "Case v1, v2, ..." di un Select Case (vedi vbaStmtSelectCase
// sopra): zero o piu' valori di confronto (vedi VbaCaseMatch sopra)
// piu' il corpo da eseguire se uno qualunque combacia con l'espressione
// del Select Case. "Case Else" si rappresenta a parte, in
// VbaStmt::elseBody, non come un elemento di questa lista (deve
// sempre essere l'ultimo, come nel vero VBA, e un corpo senza nessun
// valore di confronto qui dentro non avrebbe senso).
struct VbaCaseClause {
	std::vector<VbaCaseMatch> matches;
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
	std::string varName;                // Dim/For/Get/ReDim: il nome della variabile (per Get/ReDim,
	                                       // sempre il nome di un array, Fase 6g)
	std::vector<int> arrayDims;         // solo Dim: vuoto = variabile scalare normale OPPURE (Fase
	                                       // 6g) un array DINAMICO non ancora dimensionato ("Dim
	                                       // rom() As Byte", parentesi vuote -- indistinguibile da
	                                       // una variabile scalare finche' un "ReDim" non lo
	                                       // dimensiona davvero, vedi ParseArrayDimList); non vuoto =
	                                       // array a dimensione FISSA, un elemento per asse, GIA'
	                                       // "limite superiore + 1" (Option Base 0 implicito: "Dim
	                                       // x(10)" sono gli indici 0..10, 11 elementi) -- vedi
	                                       // ParseDim in VbaStmtParser.cpp
	std::string asTypeName;             // solo Dim: il nome dopo "As" (vuoto se assente). Fase 6d:
	                                       // a differenza delle fasi precedenti, qui NON si scarta --
	                                       // VbaExecutor.cpp decide a runtime se e' un Type definito
	                                       // dall'utente (VbaEnvironment::GetTypeRegistry) o un tipo
	                                       // scalare incorporato (Byte/Long/Integer/..., trattato
	                                       // come prima: una variabile/array Variant normale)
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
	// Fase 6g: Open/Close/Get -- il numero di file dopo "#". Open/Get lo richiedono sempre
	// (mai nullptr se il parsing e' andato a buon fine); Close lo ha SOLO quando scritto come
	// "Close #n" (nullptr per un "Close" nudo, vedi vbaStmtClose sopra). Per Open "expr" sopra
	// e' il percorso, per Get e' la posizione facoltativa.
	std::unique_ptr<VbaExpr> fileNumber;
	// Fase 6g: solo ReDim -- un'espressione di limite SUPERIORE per asse (gia' risolto "0 To"
	// se scritto, vedi ParseReDim in VbaStmtParser.cpp), valutata a RUNTIME ogni volta (a
	// differenza di "arrayDims" sopra, che e' sempre un letterale fissato al momento del
	// parsing) -- e' esattamente questa la differenza fra Dim e ReDim in questo sottoinsieme.
	std::vector<std::unique_ptr<VbaExpr>> reDimDims;
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

// Fase 6d: un campo dichiarato dentro un blocco "Type ... End Type"
// -- "campo [(limiteSuperiore[, ...])] As NomeTipo". "arrayDims" vuoto
// = campo scalare; non vuoto = campo array (stessa convenzione "limite
// superiore + 1" di VbaStmt::arrayDims sopra). "typeName" e' il nome
// scritto dopo "As": uno scalare incorporato (Byte/Long/Integer/...,
// trattati tutti come Variant/double in questo sottoinsieme, vedi
// VbaStmtParser.cpp su "Dim") OPPURE il nome di un ALTRO Type, risolto
// a runtime guardando se compare nel registro dei tipi del modulo
// (VbaEnvironment::GetTypeRegistry) -- non c'e' modo di saperlo gia'
// al momento del parsing di QUESTO campo, un Type puo' referenziarne
// un altro dichiarato PIU' IN BASSO nello stesso modulo (come "VM" che
// usa "ScreenPixel" nell'esempio del CHIP-8).
struct VbaTypeField {
	std::string name;
	std::string typeName;
	std::vector<int> arrayDims;
};

// Un intero blocco "[Private|Public] Type Nome" ... "End Type" --
// "Private"/"Public" sono semplici identificatori per questo lessico
// (non parole chiave), letti e scartati dal chiamante che scandisce i
// Type di un modulo (VbaMacroRunner.h), stesso trattamento di "Private
// Sub". Un Type non eseguibile da solo: serve solo a dare forma a
// "Dim nome As NomeTipo" (VbaEnvironment.h, VbaStructInstance).
struct VbaTypeDecl {
	std::string name;
	std::vector<VbaTypeField> fields;
};

#endif
