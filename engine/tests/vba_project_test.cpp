/*
	vba_project_test.cpp

	Verifica IsVbaProject/ReadVbaStream (VbaProject.h -- Path to full
	Excel parity, VBA/macro execution, Phase 1a: just container access,
	no MS-OVBA decompression yet, see VbaProject.h's own header
	comment) against synthetic OLE2/CFB fixtures built by
	BuildTestCfb() below.

	BuildTestCfb() builds a REAL mini-stream/miniFAT structure (every
	test stream here is well under the standard 4096-byte cutoff,
	matching how a real vbaProject.bin's small streams -- "PROJECT",
	"dir", a short module -- are actually stored), not a shortcut: this
	exercises CompoundFileReader's mini-stream code path for real,
	the same path a real file would take. A proper (if degenerate,
	right-sibling-chained rather than rebalanced) red-black directory
	tree is built too, even though CompoundFileReader.cpp itself never
	walks it (flat positional scan instead) -- a realistic fixture
	should have one anyway, same as a real Microsoft-produced file
	would, and it was needed to independently cross-check this exact
	byte layout against `olefile` (a separate, independent Python CFB
	implementation) during development before porting it here: that
	cross-check is what gives this fixture real confidence beyond "my
	own encoder's bugs happen to match my own reader's bugs" -- see the
	commit message for the one-time olefile validation this layout was
	checked against.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "VbaProject.h"

static int gFailures = 0;

static void Check(bool condition, const char* what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

static const uint32 kEndOfChain = 0xFFFFFFFE;
static const uint32 kFreeSect = 0xFFFFFFFF;
static const uint32 kFatSect = 0xFFFFFFFD;
static const uint32 kNoStream = 0xFFFFFFFF;
static const uint32 kSectorSize = 512;
static const uint32 kMiniSectorSize = 64;
static const uint32 kMiniCutoff = 4096;

struct TestStream { std::string name; std::vector<uint8> data; };

static void PutU16(std::vector<uint8>& buf, size_t off, uint16 v)
{
	buf[off] = (uint8)(v & 0xFF);
	buf[off + 1] = (uint8)((v >> 8) & 0xFF);
}

static void PutU32(std::vector<uint8>& buf, size_t off, uint32 v)
{
	for (int i = 0; i < 4; i++)
		buf[off + i] = (uint8)((v >> (8 * i)) & 0xFF);
}

static void PutU64(std::vector<uint8>& buf, size_t off, uint64 v)
{
	for (int i = 0; i < 8; i++)
		buf[off + i] = (uint8)((v >> (8 * i)) & 0xFF);
}

// Builds a minimal but spec-correct OLE2/CFB blob holding the given
// streams, all through the mini-stream (every stream here must stay
// under kMiniCutoff) -- see the file-level comment above. At most 3
// streams (fits Root Entry + 3 in a single 512-byte directory sector).
static std::vector<uint8> BuildTestCfb(const std::vector<TestStream>& streams)
{
	for (size_t i = 0; i < streams.size(); i++)
	{
		if (streams[i].data.size() >= kMiniCutoff)
			return std::vector<uint8>(); // caller bug: use a real file fixture instead
	}

	// Mini-stream content (concatenated, each entry padded to a whole
	// number of 64-byte mini sectors) + its MiniFAT chain entries.
	std::vector<uint8> miniStream;
	std::vector<uint32> miniFat;
	std::vector<int32> streamMiniStart(streams.size());
	for (size_t i = 0; i < streams.size(); i++)
	{
		const std::vector<uint8>& content = streams[i].data;
		if (content.empty())
		{
			streamMiniStart[i] = -1;
			continue;
		}
		streamMiniStart[i] = (int32)miniFat.size();
		uint32 numMiniSectors = (uint32)((content.size() + kMiniSectorSize - 1) / kMiniSectorSize);
		size_t base = miniStream.size();
		miniStream.resize(base + numMiniSectors * kMiniSectorSize, 0);
		memcpy(&miniStream[base], &content[0], content.size());
		for (uint32 s = 0; s < numMiniSectors; s++)
			miniFat.push_back(s + 1 == numMiniSectors ? kEndOfChain : (uint32)(miniFat.size() + 1));
	}

	// Lay out regular 512-byte sectors: 0=FAT, 1..=miniFAT, next=directory, rest=mini-stream backing store.
	std::vector<uint32> fat;
	fat.push_back(kFatSect); // sector 0 reserved for the FAT sector itself

	uint32 miniFatEntriesPerSector = kSectorSize / 4;
	uint32 numMiniFatSectors = miniFat.empty() ? 0
		: (uint32)((miniFat.size() + miniFatEntriesPerSector - 1) / miniFatEntriesPerSector);
	uint32 firstMiniFatSector = numMiniFatSectors ? (uint32)fat.size() : kEndOfChain;
	for (uint32 s = 0; s < numMiniFatSectors; s++)
		fat.push_back(s + 1 == numMiniFatSectors ? kEndOfChain : (uint32)(fat.size() + 1));

	uint32 dirSectorIndex = (uint32)fat.size();
	fat.push_back(kEndOfChain); // directory: exactly 1 sector

	int32 rootStreamStart = miniStream.empty() ? -1 : (int32)fat.size();
	uint32 numRootSectors = miniStream.empty() ? 0
		: (uint32)((miniStream.size() + kSectorSize - 1) / kSectorSize);
	for (uint32 s = 0; s < numRootSectors; s++)
		fat.push_back(s + 1 == numRootSectors ? kEndOfChain : (uint32)(fat.size() + 1));

	uint32 fatEntriesPerSector = kSectorSize / 4;
	while (fat.size() % fatEntriesPerSector != 0)
		fat.push_back(kFreeSect);

	// Header.
	std::vector<uint8> header(512, 0);
	static const uint8 sig[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
	memcpy(&header[0], sig, 8);
	PutU16(header, 26, 3);
	PutU16(header, 28, 0xFFFE);
	PutU16(header, 30, 9); // sector shift -> 512
	PutU16(header, 32, 6); // mini sector shift -> 64
	PutU32(header, 44, 1); // number of FAT sectors
	PutU32(header, 48, dirSectorIndex);
	PutU32(header, 56, kMiniCutoff);
	PutU32(header, 60, firstMiniFatSector);
	PutU32(header, 64, numMiniFatSectors);
	PutU32(header, 68, kEndOfChain);
	PutU32(header, 72, 0);
	PutU32(header, 76, 0); // DIFAT[0] = sector 0 (the FAT sector)
	PutU32(header, 80, kFreeSect);

	// Directory sector: Root Entry + up to 3 stream entries, linked by
	// right-sibling pointers starting at Root's child (see file comment).
	std::vector<uint8> dirSector(512, 0);
	auto writeDirEntry = [&](size_t index, const std::string& name, uint8 objectType,
		int32 startSector, uint64 streamSize, uint32 left, uint32 right, uint32 child)
	{
		size_t off = index * 128;
		size_t n = name.size() < 31 ? name.size() : 31;
		for (size_t c = 0; c < n; c++)
		{
			dirSector[off + c * 2] = (uint8)name[c];
			dirSector[off + c * 2 + 1] = 0;
		}
		PutU16(dirSector, off + 64, (uint16)((n + 1) * 2));
		dirSector[off + 66] = objectType;
		PutU32(dirSector, off + 68, left);
		PutU32(dirSector, off + 72, right);
		PutU32(dirSector, off + 76, child);
		PutU32(dirSector, off + 116, (uint32)startSector);
		PutU64(dirSector, off + 120, streamSize);
	};

	writeDirEntry(0, "Root Entry", 5, rootStreamStart, miniStream.size(),
		kNoStream, kNoStream, streams.empty() ? kNoStream : 1);
	for (size_t i = 0; i < streams.size(); i++)
	{
		uint32 right = (i + 1 < streams.size()) ? (uint32)(i + 2) : kNoStream;
		writeDirEntry(i + 1, streams[i].name, 2, streamMiniStart[i], streams[i].data.size(),
			kNoStream, right, kNoStream);
	}

	// Assemble: header, FAT sector, miniFAT sector(s), directory sector, mini-stream backing store.
	std::vector<uint8> fatSector(512, 0);
	for (size_t i = 0; i < fat.size() && i < fatEntriesPerSector; i++)
		PutU32(fatSector, i * 4, fat[i]);

	std::vector<uint8> miniFatBytes(numMiniFatSectors * kSectorSize, 0);
	for (size_t i = 0; i < miniFat.size(); i++)
		PutU32(miniFatBytes, i * 4, miniFat[i]);
	for (size_t i = miniFat.size(); i < (size_t)numMiniFatSectors * miniFatEntriesPerSector; i++)
		PutU32(miniFatBytes, i * 4, kFreeSect);

	std::vector<uint8> out;
	out.insert(out.end(), header.begin(), header.end());
	out.insert(out.end(), fatSector.begin(), fatSector.end());
	out.insert(out.end(), miniFatBytes.begin(), miniFatBytes.end());
	out.insert(out.end(), dirSector.begin(), dirSector.end());
	std::vector<uint8> paddedMiniStream = miniStream;
	paddedMiniStream.resize((size_t)numRootSectors * kSectorSize, 0);
	out.insert(out.end(), paddedMiniStream.begin(), paddedMiniStream.end());
	return out;
}

int main()
{
	// A realistic (if tiny) VBA project: PROJECT + dir + one module
	// stream, same three streams every real vbaProject.bin always has
	// at minimum. Content is NOT yet MS-OVBA-decompressed VBA source
	// (that's Phase 1b/1c/1d) -- arbitrary bytes are fine here, this
	// phase only proves the container layer round-trips correctly.
	std::vector<TestStream> streams;
	streams.push_back(TestStream{"PROJECT",
		std::vector<uint8>((const uint8*)"ID=\"{00000000-0000-0000-0000-000000000000}\"\r\n",
			(const uint8*)"ID=\"{00000000-0000-0000-0000-000000000000}\"\r\n" + 46)});
	std::vector<uint8> dirContent(800);
	for (size_t i = 0; i < dirContent.size(); i++)
		dirContent[i] = (uint8)(i % 251); // non-trivial pattern, not all-zero/all-same
	streams.push_back(TestStream{"dir", dirContent});
	streams.push_back(TestStream{"Module1",
		std::vector<uint8>((const uint8*)"hello module content",
			(const uint8*)"hello module content" + 21)});

	std::vector<uint8> blob = BuildTestCfb(streams);
	Check(!blob.empty(), "il blob CFB di prova si costruisce");

	Check(IsVbaProject(&blob[0], blob.size()),
		"IsVbaProject riconosce un vero contenitore CFB con lo stream \"PROJECT\"");

	std::vector<uint8> out;
	bool ok = ReadVbaStream(&blob[0], blob.size(), "PROJECT", &out);
	Check(ok, "ReadVbaStream legge lo stream \"PROJECT\"");
	Check(out.size() == streams[0].data.size() && out == streams[0].data,
		"il contenuto di \"PROJECT\" e' identico byte per byte all'originale");

	ok = ReadVbaStream(&blob[0], blob.size(), "dir", &out);
	Check(ok, "ReadVbaStream legge lo stream \"dir\" (800 byte, piu' settori mini)");
	Check(out.size() == dirContent.size() && out == dirContent,
		"il contenuto di \"dir\" e' identico byte per byte all'originale, "
		"inclusa la catena di piu' mini-settori");

	ok = ReadVbaStream(&blob[0], blob.size(), "Module1", &out);
	Check(ok, "ReadVbaStream legge lo stream \"Module1\"");
	Check(out == streams[2].data, "il contenuto di \"Module1\" e' identico byte per byte all'originale");

	ok = ReadVbaStream(&blob[0], blob.size(), "DoesNotExist", &out);
	Check(!ok, "ReadVbaStream su un nome di stream inesistente restituisce false, non un crash");

	// Un file che e' un vero CFB ma non ha lo stream "PROJECT" non deve
	// essere scambiato per un progetto VBA -- esattamente il caso reale
	// di un XLSX cifrato con password (EncryptionInfo/EncryptedPackage,
	// vedi CompoundFileReader.h), che e' un contenitore completamente
	// diverso nonostante usi lo stesso formato CFB.
	std::vector<TestStream> nonVbaStreams;
	nonVbaStreams.push_back(TestStream{"EncryptionInfo",
		std::vector<uint8>((const uint8*)"not a vba project", (const uint8*)"not a vba project" + 18)});
	std::vector<uint8> nonVbaBlob = BuildTestCfb(nonVbaStreams);
	Check(!nonVbaBlob.empty(), "il blob CFB non-VBA di prova si costruisce");
	Check(!IsVbaProject(&nonVbaBlob[0], nonVbaBlob.size()),
		"IsVbaProject rifiuta un vero CFB senza lo stream \"PROJECT\" "
		"(es. un XLSX cifrato con EncryptionInfo/EncryptedPackage)");

	// Dati che non sono affatto un CFB (firma sbagliata): deve fallire
	// in modo pulito, non leggere oltre il buffer.
	std::vector<uint8> garbage(64, 0x41);
	Check(!IsVbaProject(&garbage[0], garbage.size()),
		"IsVbaProject rifiuta dati che non hanno la firma OLE2/CFB");
	Check(!ReadVbaStream(&garbage[0], garbage.size(), "PROJECT", &out),
		"ReadVbaStream su dati non-CFB restituisce false, non un crash");

	// Buffer troppo corto per avere anche solo l'header da 512 byte.
	std::vector<uint8> tooShort(16, 0);
	memcpy(&tooShort[0], "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8);
	Check(!IsVbaProject(&tooShort[0], tooShort.size()),
		"IsVbaProject rifiuta un buffer troppo corto per un header CFB completo, non va in crash");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
