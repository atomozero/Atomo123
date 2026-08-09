/*
	MiniZip.cpp

	Vedi MiniZip.h per la descrizione. Implementazione minimale del
	formato ZIP: End Of Central Directory -> Central Directory ->
	Local File Header -> dati (eventualmente compressi in deflate).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "MiniZip.h"

#include <cstring>

#include <zlib.h>

static const uint32 kEOCDSignature = 0x06054b50;
static const uint32 kCentralDirSignature = 0x02014b50;
static const uint32 kLocalHeaderSignature = 0x04034b50;

static const int kEOCDMinSize = 22;
static const int kEOCDSearchWindow = 65536 + kEOCDMinSize; // + commento max

CZipWriter::CZipWriter()
	: fDest(NULL)
{
}

void CZipWriter::Begin(BPositionIO* dest)
{
	fDest = dest;
	fEntries.clear();
}

bool CZipWriter::AddEntry(const char* name, const void* data, size_t size)
{
	off_t offset = fDest->Position();
	uint32 crc = (uint32)crc32(0, (const Bytef*)data, (uInt)size);
	uint16 nameLen = (uint16)strlen(name);
	uint32 sizeField = (uint32)size;

	unsigned char header[30];
	memset(header, 0, sizeof(header));
	memcpy(&header[0], &kLocalHeaderSignature, 4);
	uint16 versionNeeded = 20;
	memcpy(&header[4], &versionNeeded, 2);
	// bit generico(2)=0, metodo compressione(2)=0 ("stored"), lasciati
	// a zero dal memset iniziale.
	uint16 modTime = 0, modDate = 0x21; // 1 gennaio 1980, mezzanotte:
	// data/ora fissa e valida per qualunque lettore ZIP, i timestamp
	// delle singole voci non hanno alcun significato per un foglio di
	// calcolo esportato.
	memcpy(&header[10], &modTime, 2);
	memcpy(&header[12], &modDate, 2);
	memcpy(&header[14], &crc, 4);
	memcpy(&header[18], &sizeField, 4); // dimensione compressa == non compressa ("stored")
	memcpy(&header[22], &sizeField, 4); // dimensione non compressa
	memcpy(&header[26], &nameLen, 2);

	if (fDest->Write(header, sizeof(header)) != (ssize_t)sizeof(header))
		return false;
	if (fDest->Write(name, nameLen) != nameLen)
		return false;
	if (size > 0 && fDest->Write(data, size) != (ssize_t)size)
		return false;

	WrittenEntry entry;
	entry.name = name;
	entry.crc = crc;
	entry.size = sizeField;
	entry.localHeaderOffset = (uint32)offset;
	fEntries.push_back(entry);

	return true;
}

bool CZipWriter::Close()
{
	off_t cdStart = fDest->Position();

	for (size_t i = 0; i < fEntries.size(); i++)
	{
		const WrittenEntry& e = fEntries[i];
		uint16 nameLen = (uint16)e.name.size();

		unsigned char header[46];
		memset(header, 0, sizeof(header));
		memcpy(&header[0], &kCentralDirSignature, 4);
		uint16 versionMadeBy = 20, versionNeeded = 20;
		memcpy(&header[4], &versionMadeBy, 2);
		memcpy(&header[6], &versionNeeded, 2);
		uint16 modTime = 0, modDate = 0x21;
		memcpy(&header[12], &modTime, 2);
		memcpy(&header[14], &modDate, 2);
		memcpy(&header[16], &e.crc, 4);
		memcpy(&header[20], &e.size, 4);
		memcpy(&header[24], &e.size, 4);
		memcpy(&header[28], &nameLen, 2);
		memcpy(&header[42], &e.localHeaderOffset, 4);

		if (fDest->Write(header, sizeof(header)) != (ssize_t)sizeof(header))
			return false;
		if (fDest->Write(e.name.data(), nameLen) != nameLen)
			return false;
	}

	off_t cdEnd = fDest->Position();

	unsigned char eocd[22];
	memset(eocd, 0, sizeof(eocd));
	memcpy(&eocd[0], &kEOCDSignature, 4);
	uint16 entryCount = (uint16)fEntries.size();
	memcpy(&eocd[8], &entryCount, 2);
	memcpy(&eocd[10], &entryCount, 2);
	uint32 cdSize = (uint32)(cdEnd - cdStart);
	memcpy(&eocd[12], &cdSize, 4);
	uint32 cdOffset32 = (uint32)cdStart;
	memcpy(&eocd[16], &cdOffset32, 4);

	return fDest->Write(eocd, sizeof(eocd)) == (ssize_t)sizeof(eocd);
}

CZipReader::CZipReader()
	: fSource(NULL)
{
}

CZipReader::~CZipReader()
{
}

bool CZipReader::Open(BPositionIO* source)
{
	fSource = source;
	fEntries.clear();

	off_t eocdOffset;
	uint32 cdOffset;
	uint16 entryCount;

	if (!FindEndOfCentralDirectory(eocdOffset, cdOffset, entryCount))
		return false;

	return ReadCentralDirectory(cdOffset, entryCount);
}

bool CZipReader::FindEndOfCentralDirectory(off_t& outEOCDOffset,
	uint32& outCDOffset, uint16& outEntryCount)
{
	off_t fileSize;
	fSource->GetSize(&fileSize);

	if (fileSize < kEOCDMinSize)
		return false;

	int window = kEOCDSearchWindow;
	if (window > fileSize)
		window = (int)fileSize;

	std::vector<unsigned char> buf(window);
	off_t readPos = fileSize - window;
	fSource->ReadAt(readPos, buf.data(), window);

	// Cerca la firma partendo dalla fine (il commento dell'archivio,
	// se presente, e' l'ultima cosa nel file, quindi la firma EOCD e'
	// nelle ultime kEOCDSearchWindow byte ma non necessariamente in
	// fondo).
	for (int i = window - kEOCDMinSize; i >= 0; i--)
	{
		uint32 sig;
		memcpy(&sig, &buf[i], 4);
		if (sig == kEOCDSignature)
		{
			outEOCDOffset = readPos + i;
			memcpy(&outEntryCount, &buf[i + 10], 2);
			memcpy(&outCDOffset, &buf[i + 16], 4);
			return true;
		}
	}

	return false;
}

bool CZipReader::ReadCentralDirectory(uint32 cdOffset, uint16 entryCount)
{
	off_t pos = cdOffset;

	for (uint16 i = 0; i < entryCount; i++)
	{
		unsigned char header[46];
		if (fSource->ReadAt(pos, header, 46) != 46)
			return false;

		uint32 sig;
		memcpy(&sig, &header[0], 4);
		if (sig != kCentralDirSignature)
			return false;

		Entry entry;
		memcpy(&entry.compressionMethod, &header[10], 2);
		memcpy(&entry.compressedSize, &header[20], 4);
		memcpy(&entry.uncompressedSize, &header[24], 4);

		uint16 nameLen, extraLen, commentLen;
		memcpy(&nameLen, &header[28], 2);
		memcpy(&extraLen, &header[30], 2);
		memcpy(&commentLen, &header[32], 2);
		memcpy(&entry.localHeaderOffset, &header[42], 4);

		std::vector<char> nameBuf(nameLen + 1);
		fSource->ReadAt(pos + 46, nameBuf.data(), nameLen);
		nameBuf[nameLen] = 0;
		entry.name = nameBuf.data();

		fEntries.push_back(entry);

		pos += 46 + nameLen + extraLen + commentLen;
	}

	return true;
}

bool CZipReader::HasEntry(const char* name) const
{
	for (size_t i = 0; i < fEntries.size(); i++)
	{
		if (fEntries[i].name == name)
			return true;
	}
	return false;
}

bool CZipReader::ReadEntry(const char* name, std::vector<unsigned char>& outData)
{
	const Entry* found = NULL;
	for (size_t i = 0; i < fEntries.size(); i++)
	{
		if (fEntries[i].name == name)
		{
			found = &fEntries[i];
			break;
		}
	}
	if (!found)
		return false;

	unsigned char localHeader[30];
	if (fSource->ReadAt(found->localHeaderOffset, localHeader, 30) != 30)
		return false;

	uint32 sig;
	memcpy(&sig, &localHeader[0], 4);
	if (sig != kLocalHeaderSignature)
		return false;

	uint16 nameLen, extraLen;
	memcpy(&nameLen, &localHeader[26], 2);
	memcpy(&extraLen, &localHeader[28], 2);

	off_t dataOffset = found->localHeaderOffset + 30 + nameLen + extraLen;

	std::vector<unsigned char> compressed(found->compressedSize);
	if (found->compressedSize > 0)
	{
		if (fSource->ReadAt(dataOffset, compressed.data(), found->compressedSize)
			!= (ssize_t)found->compressedSize)
			return false;
	}

	if (found->compressionMethod == 0)
	{
		// "Stored": nessuna compressione.
		outData = compressed;
		return true;
	}

	if (found->compressionMethod != 8)
		return false; // metodo di compressione non supportato

	// "Deflate" grezzo (senza header/trailer zlib): serve
	// inflateInit2 con finestra negativa.
	outData.resize(found->uncompressedSize);

	z_stream strm;
	memset(&strm, 0, sizeof(strm));
	if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
		return false;

	strm.next_in = compressed.data();
	strm.avail_in = found->compressedSize;
	strm.next_out = outData.data();
	strm.avail_out = found->uncompressedSize;

	int result = inflate(&strm, Z_FINISH);
	inflateEnd(&strm);

	return result == Z_STREAM_END || result == Z_OK;
}
