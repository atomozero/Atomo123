/*
	MiniZip.h

	Lettore ZIP minimale, sola lettura, per estrarre le voci di un
	file XLSX (che e' semplicemente un archivio ZIP con dentro XML).
	Scritto da zero sopra zlib (gia' presente su Haiku) invece di
	aggiungere una dipendenza a una libreria ZIP esterna (libzip/
	minizip non hanno gli header di sviluppo installati su questo
	sistema) — coerente con l'approccio di dipendenze minime gia'
	seguito nel resto del progetto.

	Supporta solo cio' che serve per leggere un XLSX generato da
	strumenti standard (Excel, LibreOffice, openpyxl, ecc.): voci non
	cifrate, metodo di compressione "stored" (0) o "deflate" (8),
	nessun supporto ZIP64 (file oltre 4 GiB — non rilevante per fogli
	di calcolo).
*/

#ifndef MINIZIP_H
#define MINIZIP_H

#include <string>
#include <vector>

#include <DataIO.h>
#include <SupportDefs.h>

class CZipReader {
public:
	CZipReader();
	~CZipReader();

	// Legge la central directory dell'archivio. Restituisce false se
	// non sembra un file ZIP valido.
	bool Open(BPositionIO* source);

	// Estrae la voce con questo nome esatto (case-sensitive, come
	// negli archivi ZIP) in outData, decomprimendola se necessario.
	// Restituisce false se la voce non esiste o e' corrotta.
	bool ReadEntry(const char* name, std::vector<unsigned char>& outData);

	bool HasEntry(const char* name) const;

private:
	struct Entry {
		std::string name;
		uint16 compressionMethod;
		uint32 compressedSize;
		uint32 uncompressedSize;
		uint32 localHeaderOffset;
	};

	BPositionIO* fSource;
	std::vector<Entry> fEntries;

	bool FindEndOfCentralDirectory(off_t& outEOCDOffset, uint32& outCDOffset,
		uint16& outEntryCount);
	bool ReadCentralDirectory(uint32 cdOffset, uint16 entryCount);
};

#endif
