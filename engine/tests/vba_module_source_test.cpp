/*
	vba_module_source_test.cpp

	Verifica ExtractVbaModuleSources (VbaModuleSource.h -- Path to full
	Excel parity, VBA/macro execution, Fase 1d, l'ultima della Fase 1)
	punta a punta: costruisce un vero vbaProject.bin sintetico (un
	contenitore CFB con uno stream "dir" MS-OVBA-compresso che descrive
	due moduli, e uno stream per modulo con un prefisso
	"PerformanceCache" fittizio seguito dal vero sorgente VBA
	compresso), poi verifica che il sorgente ricostruito combaci
	esattamente con l'originale -- l'unico test di questa fase che
	esercita i tre livelli sottostanti (VbaProject/VbaDecompress/
	VbaDirStream) INSIEME, lo stesso percorso che un vero file .xlsm
	seguirebbe, non solo ciascuno isolatamente come nei rispettivi test
	dedicati.

	Stesso limite gia' segnalato per gli altri test di questa fase (e
	in ROADMAP.md): nessun vero file .xlsm con macro prodotto da
	Microsoft Excel era disponibile in questo ambiente per una
	convalida indipendente.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "VbaModuleSource.h"

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

// ---------------------------------------------------------------------
// Compressore [MS-OVBA] 2.4.1 solo-letterale: stessa logica gia'
// validata (round-trip, confini di larghezza di bit, fallback al
// chunk non compresso) in vba_decompress_test.cpp -- qui serve solo
// per costruire dati di prova validi, non per ritestare l'algoritmo di
// compressione in se', gia' coperto a parte.
// ---------------------------------------------------------------------
static std::vector<uint8> CompressLiteralOnly(const std::vector<uint8>& data)
{
	std::vector<uint8> out;
	out.push_back(0x01);
	if (data.empty())
		return out;

	for (size_t chunkOff = 0; chunkOff < data.size(); chunkOff += 4096)
	{
		size_t chunkLen = data.size() - chunkOff;
		if (chunkLen > 4096)
			chunkLen = 4096;

		std::vector<uint8> body;
		for (size_t i = 0; i < chunkLen; i += 8)
		{
			size_t groupLen = chunkLen - i < 8 ? chunkLen - i : 8;
			body.push_back(0x00);
			for (size_t j = 0; j < groupLen; j++)
				body.push_back(data[chunkOff + i + j]);
		}

		if ((int32)body.size() + 2 - 3 > 0x0FFF)
		{
			out.push_back((uint8)(0x0FFF & 0xFF));
			out.push_back((uint8)(((0x0FFF >> 8) & 0x0F) | (0b011 << 4) | 0x00));
			for (size_t i = 0; i < 4096; i++)
				out.push_back(i < chunkLen ? data[chunkOff + i] : 0);
			continue;
		}

		uint32 chunkSize = (uint32)body.size() + 2;
		uint16 header = (uint16)(((chunkSize - 3) & 0x0FFF) | (0b011 << 12) | (1 << 15));
		out.push_back((uint8)(header & 0xFF));
		out.push_back((uint8)((header >> 8) & 0xFF));
		out.insert(out.end(), body.begin(), body.end());
	}
	return out;
}

static std::vector<uint8> CompressStr(const char* s)
{
	return CompressLiteralOnly(std::vector<uint8>((const uint8*)s, (const uint8*)s + strlen(s)));
}

// ---------------------------------------------------------------------
// Costruzione dello stream "dir" (non ancora compresso -- lo si
// comprime a parte prima di metterlo nel CFB, esattamente come un vero
// vbaProject.bin): stessa logica di record gia' validata in
// vba_dir_stream_test.cpp.
// ---------------------------------------------------------------------
static const uint16 kIdModuleName = 0x0019;
static const uint16 kIdModuleStreamName = 0x001A;
static const uint16 kIdModuleOffset = 0x0031;
static const uint16 kIdModuleTypeProcedural = 0x0021;
static const uint16 kIdModuleTypeDocument = 0x0022;
static const uint16 kIdModuleTerminator = 0x002B;

static void AppendU16(std::vector<uint8>& buf, uint16 v)
{
	buf.push_back((uint8)(v & 0xFF));
	buf.push_back((uint8)((v >> 8) & 0xFF));
}

static void AppendU32(std::vector<uint8>& buf, uint32 v)
{
	for (int i = 0; i < 4; i++)
		buf.push_back((uint8)((v >> (8 * i)) & 0xFF));
}

static void AppendRecordStr(std::vector<uint8>& buf, uint16 id, const char* s)
{
	AppendU16(buf, id);
	AppendU32(buf, (uint32)strlen(s));
	buf.insert(buf.end(), s, s + strlen(s));
}

static void AppendRecordU32(std::vector<uint8>& buf, uint16 id, uint32 value)
{
	AppendU16(buf, id);
	AppendU32(buf, 4);
	AppendU32(buf, value);
}

static void AppendRecordEmpty(std::vector<uint8>& buf, uint16 id)
{
	AppendU16(buf, id);
	AppendU32(buf, 0);
}

static void AppendModuleRecord(std::vector<uint8>& dir, const char* name, const char* streamName,
	uint32 textOffset, bool isDocument)
{
	AppendRecordStr(dir, kIdModuleName, name);
	AppendRecordStr(dir, kIdModuleStreamName, streamName);
	AppendRecordU32(dir, kIdModuleOffset, textOffset);
	AppendRecordEmpty(dir, isDocument ? kIdModuleTypeDocument : kIdModuleTypeProcedural);
	AppendRecordEmpty(dir, kIdModuleTerminator);
}

// ---------------------------------------------------------------------
// Costruttore CFB minimale (stessa logica gia' validata contro
// `olefile`, un lettore Python indipendente, in vba_project_test.cpp)
// -- ripetuta qui perche' ogni file di test in questo progetto e'
// autosufficiente, nessun helper condiviso fra file di test diversi.
// ---------------------------------------------------------------------
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

// Stessa funzione di vba_project_test.cpp: ogni stream deve restare
// sotto kMiniCutoff (tutto passa per il mini-stream), al massimo 3
// stream (una sola settore di directory). Qui bastano: "PROJECT",
// "dir", e due stream modulo -- 4 stream, uno in piu' del limite di 3
// -- quindi si usano DUE settori di directory consecutivi.
static std::vector<uint8> BuildTestCfb(const std::vector<TestStream>& streams)
{
	for (size_t i = 0; i < streams.size(); i++)
	{
		if (streams[i].data.size() >= kMiniCutoff)
			return std::vector<uint8>();
	}

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

	std::vector<uint32> fat;
	fat.push_back(kFatSect);

	uint32 miniFatEntriesPerSector = kSectorSize / 4;
	uint32 numMiniFatSectors = miniFat.empty() ? 0
		: (uint32)((miniFat.size() + miniFatEntriesPerSector - 1) / miniFatEntriesPerSector);
	uint32 firstMiniFatSector = numMiniFatSectors ? (uint32)fat.size() : kEndOfChain;
	for (uint32 s = 0; s < numMiniFatSectors; s++)
		fat.push_back(s + 1 == numMiniFatSectors ? kEndOfChain : (uint32)(fat.size() + 1));

	// Settori di directory: 4 voci per settore (Root Entry conta come
	// una voce nel primo), quindi servono ceil((streams+1)/4) settori.
	uint32 dirEntriesTotal = (uint32)streams.size() + 1;
	uint32 numDirSectors = (dirEntriesTotal + 3) / 4;
	uint32 firstDirSector = (uint32)fat.size();
	for (uint32 s = 0; s < numDirSectors; s++)
		fat.push_back(s + 1 == numDirSectors ? kEndOfChain : (uint32)(fat.size() + 1));

	int32 rootStreamStart = miniStream.empty() ? -1 : (int32)fat.size();
	uint32 numRootSectors = miniStream.empty() ? 0
		: (uint32)((miniStream.size() + kSectorSize - 1) / kSectorSize);
	for (uint32 s = 0; s < numRootSectors; s++)
		fat.push_back(s + 1 == numRootSectors ? kEndOfChain : (uint32)(fat.size() + 1));

	uint32 fatEntriesPerSector = kSectorSize / 4;
	while (fat.size() % fatEntriesPerSector != 0)
		fat.push_back(kFreeSect);
	if (fat.size() > fatEntriesPerSector)
		return std::vector<uint8>(); // il test non copre piu' di un settore FAT

	std::vector<uint8> header(512, 0);
	static const uint8 sig[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
	memcpy(&header[0], sig, 8);
	PutU16(header, 26, 3);
	PutU16(header, 28, 0xFFFE);
	PutU16(header, 30, 9);
	PutU16(header, 32, 6);
	PutU32(header, 44, 1);
	PutU32(header, 48, firstDirSector);
	PutU32(header, 56, kMiniCutoff);
	PutU32(header, 60, firstMiniFatSector);
	PutU32(header, 64, numMiniFatSectors);
	PutU32(header, 68, kEndOfChain);
	PutU32(header, 72, 0);
	PutU32(header, 76, 0);
	PutU32(header, 80, kFreeSect);

	std::vector<uint8> dirSectors(numDirSectors * kSectorSize, 0);
	auto writeDirEntry = [&](size_t index, const std::string& name, uint8 objectType,
		int32 startSector, uint64 streamSize, uint32 left, uint32 right, uint32 child)
	{
		size_t off = index * 128;
		size_t n = name.size() < 31 ? name.size() : 31;
		for (size_t c = 0; c < n; c++)
		{
			dirSectors[off + c * 2] = (uint8)name[c];
			dirSectors[off + c * 2 + 1] = 0;
		}
		PutU16(dirSectors, off + 64, (uint16)((n + 1) * 2));
		dirSectors[off + 66] = objectType;
		PutU32(dirSectors, off + 68, left);
		PutU32(dirSectors, off + 72, right);
		PutU32(dirSectors, off + 76, child);
		PutU32(dirSectors, off + 116, (uint32)startSector);
		PutU64(dirSectors, off + 120, streamSize);
	};

	writeDirEntry(0, "Root Entry", 5, rootStreamStart, miniStream.size(),
		kNoStream, kNoStream, streams.empty() ? kNoStream : 1);
	for (size_t i = 0; i < streams.size(); i++)
	{
		uint32 right = (i + 1 < streams.size()) ? (uint32)(i + 2) : kNoStream;
		writeDirEntry(i + 1, streams[i].name, 2, streamMiniStart[i], streams[i].data.size(),
			kNoStream, right, kNoStream);
	}

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
	out.insert(out.end(), dirSectors.begin(), dirSectors.end());
	std::vector<uint8> paddedMiniStream = miniStream;
	paddedMiniStream.resize((size_t)numRootSectors * kSectorSize, 0);
	out.insert(out.end(), paddedMiniStream.begin(), paddedMiniStream.end());
	return out;
}

int main()
{
	const char* module1Source =
		"Attribute VB_Name = \"Module1\"\r\n"
		"Sub HelloWorld()\r\n"
		"    MsgBox \"Hello from Module1\"\r\n"
		"End Sub\r\n";
	const char* thisWorkbookSource =
		"Private Sub Workbook_Open()\r\n"
		"    Cells(1, 1).Value = 42\r\n"
		"End Sub\r\n";

	// Prefisso PerformanceCache fittizio (p-code binario di fantasia,
	// MAI decompresso -- solo scartato): lunghezza diversa per ogni
	// modulo, cosi' il test prova davvero che MODULEOFFSET viene letto
	// e applicato per ciascuno, non solo un valore fisso che capita di
	// funzionare per coincidenza.
	std::vector<uint8> fakePCache1(37, 0xCC);
	std::vector<uint8> fakePCache2(12, 0xDD);

	std::vector<uint8> module1Compressed = CompressStr(module1Source);
	std::vector<uint8> module2Compressed = CompressStr(thisWorkbookSource);

	std::vector<uint8> module1Stream = fakePCache1;
	module1Stream.insert(module1Stream.end(), module1Compressed.begin(), module1Compressed.end());

	std::vector<uint8> module2Stream = fakePCache2;
	module2Stream.insert(module2Stream.end(), module2Compressed.begin(), module2Compressed.end());

	std::vector<uint8> dirRaw;
	AppendModuleRecord(dirRaw, "Module1", "Module1", (uint32)fakePCache1.size(), false);
	AppendModuleRecord(dirRaw, "ThisWorkbook", "ThisWorkbook", (uint32)fakePCache2.size(), true);
	std::vector<uint8> dirCompressed = CompressLiteralOnly(dirRaw);

	std::vector<TestStream> streams;
	streams.push_back(TestStream{"PROJECT",
		std::vector<uint8>((const uint8*)"ID=\"test\"\r\n", (const uint8*)"ID=\"test\"\r\n" + 11)});
	streams.push_back(TestStream{"dir", dirCompressed});
	streams.push_back(TestStream{"Module1", module1Stream});
	streams.push_back(TestStream{"ThisWorkbook", module2Stream});

	std::vector<uint8> blob = BuildTestCfb(streams);
	Check(!blob.empty(), "il vbaProject.bin sintetico (CFB + \"dir\" compresso + due moduli) si costruisce");

	std::vector<VbaModuleSource> modules;
	bool ok = ExtractVbaModuleSources(blob.empty() ? NULL : &blob[0], blob.size(), &modules);
	Check(ok, "ExtractVbaModuleSources riesce punta a punta "
		"(CFB -> decompressione \"dir\" -> elenco moduli -> decompressione del sorgente di ciascuno)");
	Check(modules.size() == 2, "trova esattamente i due moduli descritti");

	if (modules.size() == 2)
	{
		Check(modules[0].name == "Module1", "il primo modulo si chiama \"Module1\"");
		Check(!modules[0].isDocumentModule, "il primo modulo e' procedurale");
		Check(modules[0].sourceText == module1Source,
			"il sorgente ricostruito di Module1 combacia byte per byte con l'originale, "
			"PerformanceCache scartata correttamente (37 byte fittizi)");

		Check(modules[1].name == "ThisWorkbook", "il secondo modulo si chiama \"ThisWorkbook\"");
		Check(modules[1].isDocumentModule, "il secondo modulo e' un modulo documento");
		Check(modules[1].sourceText == thisWorkbookSource,
			"il sorgente ricostruito di ThisWorkbook combacia byte per byte con l'originale, "
			"PerformanceCache scartata correttamente (12 byte fittizi, lunghezza diversa dal primo)");
	}

	// ---- Un file che non e' affatto un progetto VBA ----
	{
		std::vector<uint8> notVba(64, 0x41);
		std::vector<VbaModuleSource> out;
		Check(!ExtractVbaModuleSources(&notVba[0], notVba.size(), &out),
			"dati che non sono un progetto VBA restituiscono false, non un crash");
	}

	// ---- Un MODULEOFFSET oltre la fine del vero stream del modulo ----
	{
		std::vector<uint8> badDirRaw;
		AppendModuleRecord(badDirRaw, "BadModule", "BadModule", 99999, false);
		std::vector<uint8> badDirCompressed = CompressLiteralOnly(badDirRaw);

		std::vector<TestStream> badStreams;
		badStreams.push_back(TestStream{"PROJECT",
			std::vector<uint8>((const uint8*)"X", (const uint8*)"X" + 1)});
		badStreams.push_back(TestStream{"dir", badDirCompressed});
		badStreams.push_back(TestStream{"BadModule",
			std::vector<uint8>((const uint8*)"short", (const uint8*)"short" + 5)});

		std::vector<uint8> badBlob = BuildTestCfb(badStreams);
		std::vector<VbaModuleSource> out;
		Check(!ExtractVbaModuleSources(badBlob.empty() ? NULL : &badBlob[0], badBlob.size(), &out),
			"un MODULEOFFSET oltre la fine del vero stream del modulo restituisce false, "
			"non un indice fuori dai limiti");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
