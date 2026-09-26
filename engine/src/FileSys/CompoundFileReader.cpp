/*
	CompoundFileReader.cpp

	Vedi CompoundFileReader.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "CompoundFileReader.h"

#include <cstring>

static const uint8 kCfbSignature[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };

// Valori speciali di un elenco FAT/MiniFAT (MS-CFB 2.1): un indice di
// settore normale e' sempre < 0xFFFFFFFA, questi quattro sono riservati.
static const uint32 kEndOfChain = 0xFFFFFFFE;
static const uint32 kFreeSect = 0xFFFFFFFF;
static const uint32 kFatSect = 0xFFFFFFFD;
static const uint32 kDifSect = 0xFFFFFFFC;
static const int32 kNoStream = -1;

bool IsCompoundFile(const uint8* data, size_t len)
{
	return len >= 8 && memcmp(data, kCfbSignature, 8) == 0;
}

static uint16 ReadU16(const uint8* p) { return (uint16)(p[0] | (p[1] << 8)); }
static uint32 ReadU32(const uint8* p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}
static uint64 ReadU64(const uint8* p)
{
	return (uint64)ReadU32(p) | ((uint64)ReadU32(p + 4) << 32);
}

bool CompoundFileReader::ReadSector(uint32 sectorIndex, std::vector<uint8>* out) const
{
	// I settori sono numerati a partire da 0 SUBITO dopo l'header (che
	// occupa sempre esattamente 512 byte, anche quando fSectorSize e'
	// 4096 per un file v4 -- MS-CFB 2.2: l'header resta sempre di 512
	// byte, il resto del "settore 0" logico da 4096 in un file v4 e'
	// gia' area dati vera, non padding).
	uint64 offset = 512 + (uint64)sectorIndex * fSectorSize;
	if (offset + fSectorSize > fLen)
		return false;
	out->assign(fData + offset, fData + offset + fSectorSize);
	return true;
}

bool CompoundFileReader::FollowChain(uint32 startSector, uint64 streamSize,
	const std::vector<uint32>& fat, uint32 sectorSize,
	const uint8* streamData, size_t streamDataLen,
	std::vector<uint8>* out) const
{
	out->clear();
	out->reserve((size_t)streamSize);
	uint32 sector = startSector;
	// Limite di sicurezza sul numero di settori seguiti: una catena reale
	// non e' mai piu' lunga del numero totale di voci FAT -- senza
	// questo, un file corrotto con un ciclo (settore N punta a un
	// settore gia' visitato) farebbe girare questo ciclo per sempre.
	size_t maxSteps = fat.size() + 1;
	size_t steps = 0;
	while (sector != kEndOfChain && sector != kFreeSect && out->size() < streamSize)
	{
		if (steps++ > maxSteps)
			return false;
		uint64 sectorOffset = (uint64)sector * sectorSize;
		if (sectorOffset + sectorSize > streamDataLen)
			return false;
		size_t remaining = (size_t)(streamSize - out->size());
		size_t take = remaining < sectorSize ? remaining : sectorSize;
		out->insert(out->end(), streamData + sectorOffset, streamData + sectorOffset + take);
		if (sector >= fat.size())
			return false;
		sector = fat[sector];
	}
	return out->size() == streamSize;
}

bool CompoundFileReader::Open(const uint8* data, size_t len)
{
	fData = data;
	fLen = len;
	fFat.clear();
	fMiniFat.clear();
	fMiniStream.clear();
	fEntries.clear();

	if (!IsCompoundFile(data, len) || len < 512)
		return false;

	uint16 sectorShift = ReadU16(data + 30);
	uint16 miniSectorShift = ReadU16(data + 32);
	fSectorSize = 1u << sectorShift;
	fMiniSectorSize = 1u << miniSectorShift;
	// Solo 512 o 4096 sono valori reali (MS-CFB 2.2, versione major 3 o
	// 4) -- qualunque altro valore e' un file non valido o corrotto, si
	// scarta subito invece di rischiare un settore di dimensione assurda.
	if ((fSectorSize != 512 && fSectorSize != 4096) || fMiniSectorSize != 64)
		return false;

	uint32 numFatSectors = ReadU32(data + 44);
	uint32 firstDirSector = ReadU32(data + 48);
	fMiniCutoff = ReadU32(data + 56);
	uint32 firstMiniFatSector = ReadU32(data + 60);
	uint32 numMiniFatSectors = ReadU32(data + 64);
	uint32 firstDifatSector = ReadU32(data + 68);
	uint32 numDifatSectors = ReadU32(data + 72);

	// DIFAT: le prime 109 voci vivono gia' nell'header (byte 76..511),
	// poi eventuali settori DIFAT aggiuntivi in catena (rari: servono
	// solo oltre ~109 settori FAT, cioe' file da svariati MB solo di
	// FAT) -- supportati comunque, non solo il caso comune.
	std::vector<uint32> fatSectorList;
	for (int i = 0; i < 109 && fatSectorList.size() < numFatSectors; i++)
	{
		uint32 v = ReadU32(data + 76 + i * 4);
		if (v == kFreeSect)
			break;
		fatSectorList.push_back(v);
	}
	uint32 difatSector = firstDifatSector;
	for (uint32 d = 0; d < numDifatSectors && fatSectorList.size() < numFatSectors; d++)
	{
		std::vector<uint8> sec;
		if (!ReadSector(difatSector, &sec))
			return false;
		uint32 entriesPerSector = fSectorSize / 4;
		for (uint32 i = 0; i + 1 < entriesPerSector && fatSectorList.size() < numFatSectors; i++)
		{
			uint32 v = ReadU32(&sec[i * 4]);
			if (v == kFreeSect)
				break;
			fatSectorList.push_back(v);
		}
		difatSector = ReadU32(&sec[(entriesPerSector - 1) * 4]);
		if (difatSector == kEndOfChain || difatSector == kFreeSect)
			break;
	}

	// FAT vera: concatenazione di tutti i settori FAT elencati sopra,
	// ognuno un elenco di indici uint32 (il "prossimo" settore di
	// qualunque catena, dati o FAT stessa).
	for (size_t i = 0; i < fatSectorList.size(); i++)
	{
		std::vector<uint8> sec;
		if (!ReadSector(fatSectorList[i], &sec))
			return false;
		uint32 entriesPerSector = fSectorSize / 4;
		for (uint32 j = 0; j < entriesPerSector; j++)
			fFat.push_back(ReadU32(&sec[j * 4]));
	}

	// Directory: una catena di settori normali (segue la FAT vera come
	// ogni altro stream), 128 byte per voce, 4 (o 32, per settori da
	// 4096) voci per settore.
	std::vector<uint8> dirBytes;
	{
		uint32 sector = firstDirSector;
		size_t maxSteps = fFat.size() + 1, steps = 0;
		while (sector != kEndOfChain && sector != kFreeSect)
		{
			if (steps++ > maxSteps)
				return false;
			std::vector<uint8> sec;
			if (!ReadSector(sector, &sec))
				return false;
			dirBytes.insert(dirBytes.end(), sec.begin(), sec.end());
			if (sector >= fFat.size())
				return false;
			sector = fFat[sector];
		}
	}

	int32 rootStartSector = kNoStream;
	uint64 rootStreamSize = 0;
	for (size_t off = 0; off + 128 <= dirBytes.size(); off += 128)
	{
		const uint8* e = &dirBytes[off];
		uint16 nameLenBytes = ReadU16(e + 64); // include il terminatore nullo UTF-16LE
		uint8 objectType = e[66];
		if (objectType == 0) // "unknown", voce inutilizzata
			continue;

		std::string name;
		int nameChars = nameLenBytes >= 2 ? (nameLenBytes / 2 - 1) : 0;
		for (int c = 0; c < nameChars; c++)
		{
			// Nomi di stream OLE2 reali (EncryptionInfo/EncryptedPackage,
			// "Book"/"Workbook" per il vecchio XLS) sono sempre ASCII:
			// un troncamento a un byte per carattere basta, niente
			// bisogno di un vero decoder UTF-16 qui.
			uint16 ch = ReadU16(e + c * 2);
			name += (char)(ch & 0xFF);
		}

		DirEntry entry;
		entry.name = name;
		entry.objectType = objectType;
		entry.startSector = (int32)ReadU32(e + 116);
		entry.streamSize = ReadU64(e + 120);
		fEntries.push_back(entry);

		if (objectType == 5) // root storage
		{
			rootStartSector = entry.startSector;
			rootStreamSize = entry.streamSize;
		}
	}

	// Mini stream: il contenuto VERO del Root Entry (non un elenco
	// separato di settori a parte) -- ogni stream piu' piccolo di
	// fMiniCutoff vive dentro QUESTO buffer, indicizzato dalla MiniFAT
	// sotto invece che dalla FAT normale.
	if (rootStartSector >= 0 && rootStreamSize > 0)
	{
		if (!FollowChain((uint32)rootStartSector, rootStreamSize, fFat, fSectorSize,
				fData + 512, fLen - 512, &fMiniStream))
			return false;
	}

	// MiniFAT: stessa forma della FAT vera, ma per mini-settori da 64
	// byte dentro fMiniStream -- una catena di settori NORMALI (letta
	// con la FAT vera), non un secondo formato a parte.
	if (numMiniFatSectors > 0)
	{
		std::vector<uint8> miniFatBytes;
		uint32 sector = firstMiniFatSector;
		size_t maxSteps = fFat.size() + 1, steps = 0;
		while (sector != kEndOfChain && sector != kFreeSect)
		{
			if (steps++ > maxSteps)
				return false;
			std::vector<uint8> sec;
			if (!ReadSector(sector, &sec))
				return false;
			miniFatBytes.insert(miniFatBytes.end(), sec.begin(), sec.end());
			if (sector >= fFat.size())
				return false;
			sector = fFat[sector];
		}
		uint32 entryCount = (uint32)(miniFatBytes.size() / 4);
		for (uint32 i = 0; i < entryCount; i++)
			fMiniFat.push_back(ReadU32(&miniFatBytes[i * 4]));
	}

	return true;
}

bool CompoundFileReader::ReadStream(const std::string& name, std::vector<uint8>* outBytes) const
{
	for (size_t i = 0; i < fEntries.size(); i++)
	{
		const DirEntry& e = fEntries[i];
		if (e.objectType != 2 || e.name != name) // 2 = stream
			continue;
		if (e.streamSize == 0)
		{
			outBytes->clear();
			return true;
		}
		if (e.streamSize < fMiniCutoff)
		{
			// Nel mini stream, indicizzato dalla MiniFAT.
			return FollowChain((uint32)e.startSector, e.streamSize, fMiniFat, fMiniSectorSize,
				fMiniStream.empty() ? NULL : &fMiniStream[0], fMiniStream.size(), outBytes);
		}
		// Settori normali, indicizzati dalla FAT vera.
		return FollowChain((uint32)e.startSector, e.streamSize, fFat, fSectorSize,
			fData + 512, fLen - 512, outBytes);
	}
	return false;
}
