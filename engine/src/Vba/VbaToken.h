/*
	VbaToken.h

	Analisi lessicale di un sorgente VBA (gia' estratto e decompresso,
	vedi VbaModuleSource.h) nel sottoinsieme esplicito deciso in
	ROADMAP.md (opzione C): le istruzioni che coprono la maggior parte
	di un vero output "registra macro" (Sub/End Sub, Dim, assegnazioni,
	If/Then/Else/End If, For/Next, chiamate a Range()/Cells()/funzioni
	di base), rifiutando qualunque cosa fuori da questo ambito con un
	errore chiaro piuttosto che fingere compatibilita' totale -- vedi
	il commento esteso in ROADMAP.md sul perche' la fedelta' al 100% e'
	comunque irraggiungibile su Haiku (Declare/COM/ActiveX puntano ad
	API Windows che qui non esistono).

	A differenza delle Fasi 1a-1d (decodifica di un formato binario
	documentato ma mai confermato contro un vero file Microsoft), le
	regole lessicali di VBA sono standard e ben note (un dialetto BASIC
	con poche sorprese): questo file porta un rischio di correttezza
	molto piu' basso, nessuna "convalida indipendente mancante" da
	segnalare qui.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_TOKEN_H
#define VBA_TOKEN_H

#include <string>
#include <vector>

enum VbaTokenType {
	vbaTokEnd = 0,      // fine dell'input
	vbaTokNewline,      // fine di un'istruzione (newline reale o ":")
	vbaTokIdentifier,   // nome di variabile/funzione/proprieta'
	vbaTokKeyword,      // vedi l'elenco in VbaToken.cpp -- il testo esatto e' normalizzato
	                     // alla grafia canonica (VBA non distingue maiuscole/minuscole)
	vbaTokNumber,
	vbaTokString,
	vbaTokOperator,     // + - * / ^ & = <> < > <= >= ( ) , .
	vbaTokError,        // carattere non riconosciuto nel sottoinsieme supportato
};

struct VbaToken {
	VbaTokenType type;
	std::string text;        // testo grezzo (identificatore/parola chiave/operatore)
	double numberValue;      // valido solo se type == vbaTokNumber
	std::string stringValue; // valido solo se type == vbaTokString (gia' senza virgolette,
	                          // "" risolto nella singola virgoletta letterale)
	int line;                 // numero di riga nel sorgente originale (1-based), per i messaggi d'errore

	VbaToken() : type(vbaTokEnd), numberValue(0.0), line(0) {}
};

// Analizza l'intero sorgente in una sequenza di token, terminata da un
// vbaTokEnd finale. Non fallisce mai per un carattere non riconosciuto
// -- emette un vbaTokError con quel carattere come testo e continua,
// cosi' il chiamante (il parser, Fase 2b) puo' decidere se e come
// segnalarlo nel contesto della singola istruzione che lo conteneva,
// invece di perdere il resto del modulo per un singolo carattere
// fuori posto.
void TokenizeVba(const std::string& source, std::vector<VbaToken>* outTokens);

#endif
