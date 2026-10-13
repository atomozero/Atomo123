/*
	VbaModuleSource.h

	Estrazione del sorgente VBA reale e leggibile di ogni modulo di un
	progetto -- l'ultimo passo della Fase 1 (Path to full Excel parity,
	VBA/macro execution, vedi la decisione in ROADMAP.md), quello che
	chiude il cerchio: combina i tre livelli gia' costruiti e provati
	singolarmente --
	  - VbaProject.h: apre vbaProject.bin come contenitore OLE2/CFB,
	    legge uno stream per nome;
	  - VbaDecompress.h: decodifica un CompressedContainer [MS-OVBA]
	    2.4.1;
	  - VbaDirStream.h: analizza lo stream "dir" (gia' decompresso) in
	    un elenco di moduli, col nome del loro stream e l'offset dove
	    inizia il sorgente vero.

	Lo stream "dir" e' SEMPRE un CompressedContainer per intero (nessun
	prefisso non compresso). Lo stream di OGNI modulo invece ha una
	forma diversa ([MS-OVBA] 2.3.4.3): i primi "TextOffset" byte (da
	MODULEOFFSET, vedi VbaDirStream.h) sono una "PerformanceCache"
	binaria specifica dell'implementazione (p-code compilato) -- MAI
	MS-OVBA-compressa, da scartare senza nemmeno provare a
	decomprimerla -- seguita dal vero sorgente, quello si'
	MS-OVBA-compresso, dallo stesso offset fino alla fine dello stream.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_MODULE_SOURCE_H
#define VBA_MODULE_SOURCE_H

#include <string>
#include <vector>

#include <SupportDefs.h>

struct VbaModuleSource {
	std::string name;
	bool isDocumentModule;
	std::string sourceText; // il vero codice sorgente VBA, leggibile

	VbaModuleSource() : isDocumentModule(false) {}
};

// Estrazione punta a punta: dato il blob grezzo di un vbaProject.bin
// (xl/vbaProject.bin, preservato come blob opaco fin dalla v0.2.7, mai
// analizzato prima d'ora -- vedi translators/xlsx/XlsxTranslator.cpp),
// restituisce il sorgente VBA reale di ogni modulo. Falso se "data" non
// e' affatto un progetto VBA valido (vedi IsVbaProject), o se una
// qualunque delle fasi intermedie fallisce su dati troncati/malformati
// (mai una lettura oltre i limiti di un buffer, in nessuno dei tre
// livelli sottostanti).
bool ExtractVbaModuleSources(const uint8* data, size_t len,
	std::vector<VbaModuleSource>* outModules);

#endif
