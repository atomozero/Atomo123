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

	Gli ID dei record sono stati ricostruiti dalla documentazione della
	specifica, poi confermati byte per byte contro due esempi REALI
	pubblicati dalla specifica [MS-OVBA] stessa (sezione 3.1.1/3.1.2,
	"ThisWorkbook"/"Sheet1" Document Module Record Example -- vedi
	vba_dir_stream_test.cpp): confermano anche il dettaglio meno ovvio
	che MODULESTREAMNAMEUNICODE (0x0032) e MODULEDOCSTRINGUNICODE
	(0x0048) sono, sul disco reale, record Id+Size+Dati generici
	separati e consecutivi -- non campi annidati dentro un unico record
	piu' grande, nonostante la tabella della specifica li raggruppi
	sotto un solo nome per la documentazione. Quello che resta non
	confermato contro un file vero e' solo il contesto piu' ampio
	attorno a questi record isolati (un intero vbaProject.bin reale,
	completo di PROJECTINFORMATION/PROJECTREFERENCES prima di
	PROJECTMODULES) -- limite minore, segnalato comunque in ROADMAP.md.

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
