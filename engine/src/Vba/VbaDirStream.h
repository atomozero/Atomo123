/*
	VbaDirStream.h

	Analizza il contenuto GIA' DECOMPRESSO (vedi VbaDecompress.h) dello
	stream "dir" di un progetto VBA ([MS-OVBA] 2.3.4.2): un elenco di
	record Id(2 byte)+Size(4 byte)+Dati(Size byte) che descrivono il
	progetto nel suo insieme (nome, riferimenti, codepage -- ignorati
	qui, saltati genericamente dal proprio campo Size) e, soprattutto,
	l'elenco dei moduli: nome, in quale stream di vbaProject.bin vive
	il suo codice vero, e a quale offset dentro quello stream inizia il
	testo sorgente compresso (tutto cio' che precede e' una
	"PerformanceCache" binaria di p-code, da saltare senza analizzarla,
	vedi [MS-OVBA] 2.3.4.3).

	UN solo record ha un formato irregolare, senza campo Size affatto:
	PROJECTVERSION (Id 0x0009, [MS-OVBA] 2.3.4.2.4) -- Reserved(4)
	fisso + VersionMajor(4) + VersionMinor(2), 10 byte di corpo sempre
	della stessa lunghezza. Va riconosciuto e saltato con una lunghezza
	fissa, non come Id+Size+Dati generico: trattarlo come qualunque
	altro record disallineerebbe la lettura di TUTTO cio' che segue
	nello stream, moduli inclusi.

	Stesso limite gia' segnalato per VbaDecompress.h e in ROADMAP.md:
	gli ID dei record e il loro formato sono stati ricostruiti dalla
	documentazione della specifica, non confermati contro un vero
	progetto VBA prodotto da Microsoft Excel (nessun campione reale
	disponibile in questo ambiente) -- i test qui sotto verificano la
	coerenza interna (un vero stream "dir" sintetico, costruito a mano
	con gli stessi ID, viene letto correttamente), non la fedelta' a un
	file reale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_DIR_STREAM_H
#define VBA_DIR_STREAM_H

#include <string>
#include <vector>

#include <SupportDefs.h>

struct VbaModuleInfo {
	std::string name;       // da MODULENAMEUNICODE (preferito) o MODULENAME
	std::string streamName; // da MODULESTREAMNAMEUNICODE (preferito) o MODULESTREAMNAME --
	                         // il nome dello stream CFB dentro vbaProject.bin con questo modulo
	uint32 textOffset;       // da MODULEOFFSET: offset in byte, dentro quello stream, dove
	                         // inizia il vero sorgente compresso (prima = PerformanceCache binaria)
	bool isDocumentModule;   // vero per MODULETYPEDOCUMENT (ThisWorkbook/Sheet1/...),
	                         // falso per MODULETYPEPROCEDURAL (un Module/Class normale)

	VbaModuleInfo() : textOffset(0), isDocumentModule(false) {}
};

// Analizza uno stream "dir" gia' decompresso, popolando "outModules"
// con un'voce per ogni modulo descritto (nello stesso ordine in cui
// compaiono nello stream). Falso solo se lo stream e' troncato in un
// punto che impedirebbe di continuare in modo affidabile (un record
// che dichiara piu' byte di quanti ne restino) -- un record
// sconosciuto/non necessario non fa mai fallire l'analisi, viene solo
// saltato via il proprio campo Size.
bool ParseVbaDirStream(const uint8* data, size_t len, std::vector<VbaModuleInfo>* outModules);

#endif
