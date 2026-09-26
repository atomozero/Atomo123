/*
	CompoundFileReader.h

	Lettore GENERALE del formato OLE2/CFB ("Compound File Binary", MS-CFB)
	-- header + DIFAT + FAT + MiniFAT + albero delle directory, con
	ricerca di uno stream per NOME qualunque. Serve per un file XLSX
	protetto da password all'apertura (Path to full Excel parity): un
	file cosi' non e' affatto uno ZIP, e' un intero contenitore OLE2 con
	due stream dentro ("EncryptionInfo" e "EncryptedPackage", vedi
	OfficeCrypto.h) -- lo stesso contenitore usato anche dal vecchio
	formato .xls (BIFF8), ma engine/src/Excel/Excel.OLE2.cpp legge SOLO
	lo stream "Book"/"Workbook" con un percorso a parte, non e' un
	lettore generale e non viene toccato da questo file (nessun rischio
	condiviso con l'importazione XLS gia' funzionante).

	Tutto in memoria (un file XLSX cifrato non e' mai enorme): il
	chiamante legge l'intero file in un buffer prima di passarlo qui,
	stesso principio gia' seguito ovunque in questo progetto per un file
	XLSX/ASCD (BMallocIO), niente lettura a pezzi da un vero BPositionIO.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef COMPOUND_FILE_READER_H
#define COMPOUND_FILE_READER_H

#include <string>
#include <vector>

#include <SupportDefs.h>

// Vero solo se i primi 8 byte di "data" sono la firma OLE2/CFB reale
// (D0 CF 11 E0 A1 B1 1A E1) -- stesso principio di IsASCDFile in
// ui/src/AscdIO.cpp, un controllo di firma prima di provare ad aprire
// per davvero. Un file XLSX protetto da password ha SEMPRE questa
// firma al posto della firma ZIP "PK\x03\x04".
bool IsCompoundFile(const uint8* data, size_t len);

class CompoundFileReader {
public:
	// Vero se "data" e' un vero contenitore OLE2/CFB v3 o v4 (settori da
	// 512 byte, gli unici due valori reali che esistono) e la sua
	// struttura interna (FAT/MiniFAT/directory) e' internamente
	// coerente -- non copia "data" (deve restare valido per tutta la
	// vita di questo oggetto).
	bool Open(const uint8* data, size_t len);

	// Vero solo se uno stream con questo nome esiste (confronto esatto,
	// case-sensitive: "EncryptionInfo"/"EncryptedPackage" sono sempre
	// scritti cosi' da un vero Excel) -- il suo contenuto completo
	// (gia' seguendo la catena FAT o MiniFAT, qualunque delle due usi)
	// viene copiato in "outBytes".
	bool ReadStream(const std::string& name, std::vector<uint8>* outBytes) const;

private:
	struct DirEntry {
		std::string name;
		uint8 objectType; // 2 = stream, 5 = root storage, altri ignorati
		int32 startSector;
		uint64 streamSize;
	};

	const uint8* fData;
	size_t fLen;
	uint32 fSectorSize;
	uint32 fMiniSectorSize;
	uint32 fMiniCutoff;
	std::vector<uint32> fFat;
	std::vector<uint32> fMiniFat;
	std::vector<uint8> fMiniStream; // "Root Entry"'s intero stream, gia' seguito con la FAT normale
	std::vector<DirEntry> fEntries;

	bool ReadSector(uint32 sectorIndex, std::vector<uint8>* out) const;
	bool FollowChain(uint32 startSector, uint64 streamSize,
		const std::vector<uint32>& fat, uint32 sectorSize,
		const uint8* streamData, size_t streamDataLen,
		std::vector<uint8>* out) const;
};

#endif
