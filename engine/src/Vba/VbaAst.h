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

#endif
