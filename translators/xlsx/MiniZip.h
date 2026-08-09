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

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef MINIZIP_H
#define MINIZIP_H

#include <string>
#include <vector>

#include <DataIO.h>
#include <SupportDefs.h>

// Scrittore ZIP minimale, simmetrico a CZipReader: solo voci
// "stored" (senza compressione, metodo 0). Piu' semplice e robusto
// di "deflate" per le dimensioni tipiche di un foglio di calcolo
// esportato, a costo di una dimensione file leggermente maggiore --
// stesso approccio di dipendenze minime del lettore.
class CZipWriter {
public:
	CZipWriter();

	// "dest" deve restare valido fino a Close().
	void Begin(BPositionIO* dest);

	// Aggiunge una voce (nome + contenuto), scrivendola subito nello
	// stream. Le voci vanno aggiunte nell'ordine in cui devono
	// comparire nell'archivio. Restituisce false in caso di errore
	// di scrittura.
	bool AddEntry(const char* name, const void* data, size_t size);

	// Scrive la central directory e l'End Of Central Directory,
	// completando l'archivio. Va chiamato una sola volta, dopo
	// l'ultimo AddEntry().
	bool Close();

private:
	struct WrittenEntry {
		std::string name;
		uint32 crc;
		uint32 size;
		uint32 localHeaderOffset;
	};

	BPositionIO* fDest;
	std::vector<WrittenEntry> fEntries;
};

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
