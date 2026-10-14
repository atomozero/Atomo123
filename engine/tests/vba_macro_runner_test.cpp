/*
	vba_macro_runner_test.cpp

	Verifica FindVbaSubByName e ExecuteVbaMacro (VbaMacroRunner.h --
	Path to full Excel parity, VBA/macro execution, Fase 3d, l'ultima
	della Fase 3): la scansione multi-Sub di un modulo con solo
	sorgente di prova (senza CFB, la parte interessante qui non ha
	nulla a che fare con la decompressione), poi un vero vbaProject.bin
	sintetico a due moduli punta a punta (stesso costruttore CFB gia'
	validato in vba_module_source_test.cpp) per provare che Fase 1 +
	Fase 2 + Fase 3a/b/c sono davvero collegate, non solo ciascuna
	testata isolatamente.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Container.h"
#include "VbaMacroRunner.h"

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

// =======================================================================
// Parte 1: FindVbaSubByName su solo sorgente, nessun CFB coinvolto.
// =======================================================================

static bool FindSub(const char* source, const char* subName, std::string* outName)
{
	std::vector<VbaToken> tokens;
	TokenizeVba(source, &tokens);
	std::unique_ptr<VbaSub> sub;
	VbaParseError err;
	if (!FindVbaSubByName(tokens, subName, &sub, &err))
		return false;
	*outName = sub->name;
	return true;
}

static void TestFindVbaSubByName()
{
	const char* src =
		"Attribute VB_Name = \"Module1\"\r\n" // riga di modulo che questo sottoinsieme non riconosce
		"Sub First()\r\n"
		"    x = 1\r\n"
		"End Sub\r\n"
		"Sub Second()\r\n"
		"    If x > 0 Then\r\n" // un If annidato: deve saltarsi correttamente quando NON e' quella cercata
		"        y = 2\r\n"
		"    End If\r\n"
		"End Sub\r\n"
		"Sub Third()\r\n"
		"    z = 3\r\n"
		"End Sub\r\n";

	std::string name;
	Check(FindSub(src, "First", &name) && name == "First",
		"trova la prima Sub del modulo, saltando la riga iniziale non riconosciuta");
	Check(FindSub(src, "Second", &name) && name == "Second",
		"trova la seconda Sub, nonostante un If annidato dentro la prima");
	Check(FindSub(src, "Third", &name) && name == "Third",
		"trova la terza Sub, dopo aver saltato correttamente le prime due per intero");
	Check(FindSub(src, "second", &name) && name == "Second",
		"la ricerca per nome non distingue maiuscole/minuscole, come VBA stesso");

	{
		std::vector<VbaToken> tokens;
		TokenizeVba(src, &tokens);
		std::unique_ptr<VbaSub> sub;
		VbaParseError err;
		Check(!FindVbaSubByName(tokens, "DoesNotExist", &sub, &err),
			"una Sub che non esiste nel modulo produce un errore, non un crash");
	}
}

// =======================================================================
// Parte 2: ExecuteVbaMacro punta a punta -- vero vbaProject.bin
// sintetico a due moduli (stesso costruttore CFB di
// vba_module_source_test.cpp, non ripetuto qui parola per parola nei
// commenti: vedi quel file per i dettagli del formato).
// =======================================================================

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

static const uint16 kIdModuleName = 0x0019;
static const uint16 kIdModuleStreamName = 0x001A;
static const uint16 kIdModuleOffset = 0x0031;
static const uint16 kIdModuleTypeProcedural = 0x0021;
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
	uint32 textOffset)
{
	AppendRecordStr(dir, kIdModuleName, name);
	AppendRecordStr(dir, kIdModuleStreamName, streamName);
	AppendRecordU32(dir, kIdModuleOffset, textOffset);
	AppendRecordEmpty(dir, kIdModuleTypeProcedural);
	AppendRecordEmpty(dir, kIdModuleTerminator);
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
		return std::vector<uint8>();

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

static std::vector<uint8> BuildTwoModuleProject()
{
	// Module1 non contiene affatto la macro cercata dal test -- prova
	// che ExecuteVbaMacro continua col modulo SUCCESSIVO invece di
	// fermarsi al primo che non la contiene.
	const char* module1Source =
		"Sub Unrelated()\r\n"
		"    x = 1\r\n"
		"End Sub\r\n";
	const char* module2Source =
		"Sub FillColumn()\r\n"
		"    For i = 1 To 3\r\n"
		"        Cells(i, 1).Value = i * 100\r\n"
		"    Next\r\n"
		"End Sub\r\n";

	std::vector<uint8> fakePCache1(5, 0xCC);
	std::vector<uint8> fakePCache2(9, 0xDD);
	std::vector<uint8> module1Compressed = CompressStr(module1Source);
	std::vector<uint8> module2Compressed = CompressStr(module2Source);

	std::vector<uint8> module1Stream = fakePCache1;
	module1Stream.insert(module1Stream.end(), module1Compressed.begin(), module1Compressed.end());
	std::vector<uint8> module2Stream = fakePCache2;
	module2Stream.insert(module2Stream.end(), module2Compressed.begin(), module2Compressed.end());

	std::vector<uint8> dirRaw;
	AppendModuleRecord(dirRaw, "Module1", "Module1", (uint32)fakePCache1.size());
	AppendModuleRecord(dirRaw, "Module2", "Module2", (uint32)fakePCache2.size());
	std::vector<uint8> dirCompressed = CompressLiteralOnly(dirRaw);

	std::vector<TestStream> streams;
	streams.push_back(TestStream{"PROJECT",
		std::vector<uint8>((const uint8*)"ID=\"test\"\r\n", (const uint8*)"ID=\"test\"\r\n" + 11)});
	streams.push_back(TestStream{"dir", dirCompressed});
	streams.push_back(TestStream{"Module1", module1Stream});
	streams.push_back(TestStream{"Module2", module2Stream});
	return BuildTestCfb(streams);
}

static void TestExecuteVbaMacro()
{
	std::vector<uint8> project = BuildTwoModuleProject();
	Check(!project.empty(), "il vbaProject.bin sintetico a due moduli si costruisce");

	CContainer* doc = new CContainer(NULL, NULL);
	VbaRuntimeError err;
	bool ok = ExecuteVbaMacro(&project[0], project.size(), "FillColumn", doc, &err);
	Check(ok, "ExecuteVbaMacro trova \"FillColumn\" nel SECONDO modulo (il primo non la contiene) "
		"ed esegue l'intera pipeline Fase 1 + Fase 2 + Fase 3a/b/c punta a punta");

	Value v1, v2, v3;
	doc->GetValue(cell(1, 1), v1);
	doc->GetValue(cell(1, 2), v2);
	doc->GetValue(cell(1, 3), v3);
	Check((double)v1 == 100.0 && (double)v2 == 200.0 && (double)v3 == 300.0,
		"la macro scrive davvero A1=100, A2=200, A3=300 sul documento vero collegato");
	doc->Release();

	// ---- Macro che non esiste in nessun modulo del progetto ----
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		VbaRuntimeError err2;
		Check(!ExecuteVbaMacro(&project[0], project.size(), "DoesNotExist", doc2, &err2),
			"una macro che non esiste in NESSUN modulo produce un errore chiaro, non un crash");
		doc2->Release();
	}

	// ---- vbaProject.bin non valido ----
	{
		std::vector<uint8> notVba(32, 0x41);
		VbaRuntimeError err3;
		Check(!ExecuteVbaMacro(&notVba[0], notVba.size(), "FillColumn", NULL, &err3),
			"dati che non sono affatto un progetto VBA producono un errore, non un crash");
	}
}

// =======================================================================
// Parte 2b (Fase 6a): una Sub che richiama una Sub SORELLA dello stesso
// modulo passandole argomenti (SOLO ByVal in questo sottoinsieme -- vedi
// VbaAst.h), risolta a runtime da EvalCall tramite i token di modulo
// collegati da ExecuteVbaMacro (VbaEnvironment::SetModuleTokens).
// =======================================================================

static std::vector<uint8> BuildSiblingCallProject()
{
	// Solo la forma con parentesi e' legata a una chiamata in questo
	// sottoinsieme (vbaExprCall, via ParsePostfix): la forma SENZA
	// parentesi del vero VBA ("DrawCell 1, 10") resta fuori scopo, la
	// stessa grammatica di qualunque altra chiamata gia' supportata
	// (Range(...), Cells(...), le funzioni predefinite...).
	const char* moduleSource =
		"Sub Main()\r\n"
		"    DrawCell(1, 10)\r\n"
		"    DrawCell(2, 20)\r\n"
		"    Call DrawCell(3, 30)\r\n"
		"End Sub\r\n"
		"Sub DrawCell(ByVal r As Integer, ByVal v As Integer)\r\n"
		"    Cells(r, 1).Value = v\r\n"
		"End Sub\r\n"
		"Sub WrongArgCount()\r\n"
		"    DrawCell(1)\r\n"
		"End Sub\r\n"
		"Sub Recurse()\r\n"
		"    Recurse()\r\n"
		"End Sub\r\n";

	std::vector<uint8> fakePCache(4, 0xCC);
	std::vector<uint8> moduleCompressed = CompressStr(moduleSource);
	std::vector<uint8> moduleStream = fakePCache;
	moduleStream.insert(moduleStream.end(), moduleCompressed.begin(), moduleCompressed.end());

	std::vector<uint8> dirRaw;
	AppendModuleRecord(dirRaw, "Module1", "Module1", (uint32)fakePCache.size());
	std::vector<uint8> dirCompressed = CompressLiteralOnly(dirRaw);

	std::vector<TestStream> streams;
	streams.push_back(TestStream{"PROJECT",
		std::vector<uint8>((const uint8*)"ID=\"test\"\r\n", (const uint8*)"ID=\"test\"\r\n" + 11)});
	streams.push_back(TestStream{"dir", dirCompressed});
	streams.push_back(TestStream{"Module1", moduleStream});
	return BuildTestCfb(streams);
}

static void TestSiblingSubCall()
{
	std::vector<uint8> project = BuildSiblingCallProject();
	Check(!project.empty(), "il vbaProject.bin sintetico per la chiamata a Sub sorella si costruisce");

	CContainer* doc = new CContainer(NULL, NULL);
	VbaRuntimeError err;
	bool ok = ExecuteVbaMacro(&project[0], project.size(), "Main", doc, &err);
	Check(ok, "\"Main\" richiama \"DrawCell\" (una Sub sorella con parametri) senza errori");

	Value v1, v2, v3;
	doc->GetValue(cell(1, 1), v1);
	doc->GetValue(cell(1, 2), v2);
	doc->GetValue(cell(1, 3), v3);
	Check((double)v1 == 10.0 && (double)v2 == 20.0 && (double)v3 == 30.0,
		"ogni chiamata (con o senza \"Call\") lega r/v alla SUA nuova copia, "
		"scrivendo davvero A1=10, A2=20, A3=30 sul documento vero collegato");
	doc->Release();

	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		VbaRuntimeError err2;
		Check(!ExecuteVbaMacro(&project[0], project.size(), "WrongArgCount", doc2, &err2),
			"richiamare \"DrawCell\" con un numero di argomenti sbagliato produce un errore chiaro");
		doc2->Release();
	}

	{
		CContainer* doc3 = new CContainer(NULL, NULL);
		VbaRuntimeError err3;
		Check(!ExecuteVbaMacro(&project[0], project.size(), "Recurse", doc3, &err3),
			"una Sub che richiama se stessa senza condizione di uscita produce un errore di runtime "
			"pulito (guardia sulla profondita') invece di esaurire lo stack nativo");
		doc3->Release();
	}
}

// =======================================================================
// Parte 2c (Fase 6b): una Function sorella, richiamata dentro
// un'espressione (non come istruzione nuda) -- il valore di ritorno
// (la variabile omonima della Function, vedi VbaAst.h/VbaExecutor.cpp)
// deve arrivare davvero al chiamante, non solo eseguire senza errori.
// =======================================================================

static std::vector<uint8> BuildFunctionCallProject()
{
	const char* moduleSource =
		"Sub Main()\r\n"
		"    Cells(1, 1).Value = Double(21)\r\n"
		"    Cells(2, 1).Value = Double(Double(5))\r\n" // annidata: 5*2*2 = 20
		"End Sub\r\n"
		"Function Double(ByVal n As Integer) As Integer\r\n"
		"    Double = n * 2\r\n"
		"End Function\r\n"
		"Function NeverAssigns()\r\n" // non assegna mai il proprio nome: ritorna Empty (0.0)
		"End Function\r\n"
		"Sub UseNeverAssigns()\r\n"
		"    Cells(1, 1).Value = NeverAssigns()\r\n"
		"End Sub\r\n";

	std::vector<uint8> fakePCache(6, 0xCC);
	std::vector<uint8> moduleCompressed = CompressStr(moduleSource);
	std::vector<uint8> moduleStream = fakePCache;
	moduleStream.insert(moduleStream.end(), moduleCompressed.begin(), moduleCompressed.end());

	std::vector<uint8> dirRaw;
	AppendModuleRecord(dirRaw, "Module1", "Module1", (uint32)fakePCache.size());
	std::vector<uint8> dirCompressed = CompressLiteralOnly(dirRaw);

	std::vector<TestStream> streams;
	streams.push_back(TestStream{"PROJECT",
		std::vector<uint8>((const uint8*)"ID=\"test\"\r\n", (const uint8*)"ID=\"test\"\r\n" + 11)});
	streams.push_back(TestStream{"dir", dirCompressed});
	streams.push_back(TestStream{"Module1", moduleStream});
	return BuildTestCfb(streams);
}

static void TestSiblingFunctionCall()
{
	std::vector<uint8> project = BuildFunctionCallProject();
	Check(!project.empty(), "il vbaProject.bin sintetico per la chiamata a Function sorella si costruisce");

	CContainer* doc = new CContainer(NULL, NULL);
	VbaRuntimeError err;
	bool ok = ExecuteVbaMacro(&project[0], project.size(), "Main", doc, &err);
	Check(ok, "\"Main\" usa \"Double(...)\" dentro un'espressione senza errori");

	Value v1, v2;
	doc->GetValue(cell(1, 1), v1);
	doc->GetValue(cell(1, 2), v2);
	Check((double)v1 == 42.0, "Double(21) torna davvero 42 al chiamante, non solo esegue");
	Check((double)v2 == 20.0, "una Function sorella puo' richiamarne un'altra (Double(Double(5)) = 20)");
	doc->Release();

	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		VbaRuntimeError err2;
		bool ok2 = ExecuteVbaMacro(&project[0], project.size(), "UseNeverAssigns", doc2, &err2);
		Check(ok2, "una Function che non assegna mai il proprio nome esegue comunque senza errori");
		Value v;
		doc2->GetValue(cell(1, 1), v);
		Check((double)v == 0.0, "e il suo valore di ritorno e' Empty (0.0), come il vero VBA, "
			"non un errore \"variabile non dichiarata\"");
		doc2->Release();
	}
}

// =======================================================================
// Parte 2d (Fase 6c): array a dimensione fissa dichiarati con "Dim",
// in lettura/scrittura sia 1D che 2D, punta a punta tramite
// ExecuteVbaMacro -- e un indice fuori dai limiti (lettura e
// scrittura) che produce un errore di runtime pulito, non un accesso
// fuori dai limiti del vettore sottostante.
// =======================================================================

static std::vector<uint8> BuildArrayProject()
{
	const char* moduleSource =
		"Sub Main()\r\n"
		"    Dim V(3) As Byte\r\n"          // 4 elementi, indici 0..3
		"    Dim Screen(1, 1) As Byte\r\n"  // 2x2, indici 0..1 per asse
		"    V(0) = 10\r\n"
		"    V(1) = 20\r\n"
		"    V(2) = V(0) + V(1)\r\n"
		"    Screen(0, 0) = 100\r\n"
		"    Screen(1, 1) = 200\r\n"
		"    Cells(1, 1).Value = V(2)\r\n"
		"    Cells(2, 1).Value = Screen(0, 0)\r\n"
		"    Cells(3, 1).Value = Screen(1, 1)\r\n"
		"End Sub\r\n"
		"Sub WriteOutOfBounds()\r\n"
		"    Dim V(3) As Byte\r\n"
		"    V(10) = 1\r\n"
		"End Sub\r\n"
		"Sub ReadOutOfBounds()\r\n"
		"    Dim V(3) As Byte\r\n"
		"    Cells(1, 1).Value = V(10)\r\n"
		"End Sub\r\n";

	std::vector<uint8> fakePCache(8, 0xCC);
	std::vector<uint8> moduleCompressed = CompressStr(moduleSource);
	std::vector<uint8> moduleStream = fakePCache;
	moduleStream.insert(moduleStream.end(), moduleCompressed.begin(), moduleCompressed.end());

	std::vector<uint8> dirRaw;
	AppendModuleRecord(dirRaw, "Module1", "Module1", (uint32)fakePCache.size());
	std::vector<uint8> dirCompressed = CompressLiteralOnly(dirRaw);

	std::vector<TestStream> streams;
	streams.push_back(TestStream{"PROJECT",
		std::vector<uint8>((const uint8*)"ID=\"test\"\r\n", (const uint8*)"ID=\"test\"\r\n" + 11)});
	streams.push_back(TestStream{"dir", dirCompressed});
	streams.push_back(TestStream{"Module1", moduleStream});
	return BuildTestCfb(streams);
}

static void TestArrays()
{
	std::vector<uint8> project = BuildArrayProject();
	Check(!project.empty(), "il vbaProject.bin sintetico per gli array si costruisce");

	CContainer* doc = new CContainer(NULL, NULL);
	VbaRuntimeError err;
	bool ok = ExecuteVbaMacro(&project[0], project.size(), "Main", doc, &err);
	Check(ok, "\"Main\" legge e scrive array 1D e 2D senza errori");

	Value v1, v2, v3;
	doc->GetValue(cell(1, 1), v1);
	doc->GetValue(cell(1, 2), v2);
	doc->GetValue(cell(1, 3), v3);
	Check((double)v1 == 30.0, "V(2) = V(0) + V(1) = 10 + 20 = 30, letto davvero dall'array");
	Check((double)v2 == 100.0 && (double)v3 == 200.0,
		"Screen(0,0)=100 e Screen(1,1)=200, due celle DISTINTE dell'array 2D");
	doc->Release();

	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		VbaRuntimeError err2;
		Check(!ExecuteVbaMacro(&project[0], project.size(), "WriteOutOfBounds", doc2, &err2),
			"scrivere V(10) su un array di 4 elementi produce un errore chiaro, "
			"non un accesso fuori dai limiti del vettore sottostante");
		doc2->Release();
	}
	{
		CContainer* doc3 = new CContainer(NULL, NULL);
		VbaRuntimeError err3;
		Check(!ExecuteVbaMacro(&project[0], project.size(), "ReadOutOfBounds", doc3, &err3),
			"leggere V(10) su un array di 4 elementi produce un errore chiaro, "
			"non un accesso fuori dai limiti del vettore sottostante");
		doc3->Release();
	}
}

// =======================================================================
// Parte 3: ListVbaMacroNames -- stesso costruttore CFB, ma un progetto
// pensato apposta per provare che questa funzione NON chiama
// ParseVbaSub (un "ByRef" su un parametro, fuori da questo
// sottoinsieme, non deve far sparire NESSUN nome, nemmeno il proprio),
// e che una "Private Sub" viene esclusa come il vero elenco Macro di
// Excel.
// =======================================================================

static std::vector<uint8> BuildMacroListProject()
{
	const char* module1Source =
		"Sub Pub1()\r\n"
		"    x = 1\r\n"
		"End Sub\r\n"
		"Private Sub Hidden()\r\n"
		"    y = 2\r\n"
		"End Sub\r\n"
		"Sub Pub2(ByRef n As Integer)\r\n" // "ByRef": fuori dal sottoinsieme, ParseVbaSub la rifiuterebbe
		"    z = n\r\n"
		"End Sub\r\n";
	const char* module2Source =
		"Sub FromModule2()\r\n"
		"    w = 3\r\n"
		"End Sub\r\n";

	std::vector<uint8> fakePCache1(3, 0xCC);
	std::vector<uint8> fakePCache2(7, 0xDD);
	std::vector<uint8> module1Compressed = CompressStr(module1Source);
	std::vector<uint8> module2Compressed = CompressStr(module2Source);

	std::vector<uint8> module1Stream = fakePCache1;
	module1Stream.insert(module1Stream.end(), module1Compressed.begin(), module1Compressed.end());
	std::vector<uint8> module2Stream = fakePCache2;
	module2Stream.insert(module2Stream.end(), module2Compressed.begin(), module2Compressed.end());

	std::vector<uint8> dirRaw;
	AppendModuleRecord(dirRaw, "Module1", "Module1", (uint32)fakePCache1.size());
	AppendModuleRecord(dirRaw, "Module2", "Module2", (uint32)fakePCache2.size());
	std::vector<uint8> dirCompressed = CompressLiteralOnly(dirRaw);

	std::vector<TestStream> streams;
	streams.push_back(TestStream{"PROJECT",
		std::vector<uint8>((const uint8*)"ID=\"test\"\r\n", (const uint8*)"ID=\"test\"\r\n" + 11)});
	streams.push_back(TestStream{"dir", dirCompressed});
	streams.push_back(TestStream{"Module1", module1Stream});
	streams.push_back(TestStream{"Module2", module2Stream});
	return BuildTestCfb(streams);
}

static void TestListVbaMacroNames()
{
	std::vector<uint8> project = BuildMacroListProject();
	Check(!project.empty(), "il vbaProject.bin sintetico per l'elenco macro si costruisce");

	std::vector<std::string> names;
	bool ok = ListVbaMacroNames(&project[0], project.size(), &names);
	Check(ok, "ListVbaMacroNames riesce su un progetto valido");
	Check(names.size() == 3, "trova esattamente 3 Sub pubbliche (Hidden e' privata, esclusa)");
	if (names.size() == 3)
	{
		Check(names[0] == "Pub1", "la prima e' Pub1 (ordine di dichiarazione nel modulo)");
		Check(names[1] == "Pub2",
			"Pub2 compare NONOSTANTE il suo parametro \"ByRef\" sia fuori da questo sottoinsieme -- "
			"ListVbaMacroNames non chiama ParseVbaSub, solo una scansione lessicale");
		Check(names[2] == "FromModule2", "la terza viene dal SECONDO modulo, nell'ordine giusto");
	}

	// ---- vbaProject.bin non valido ----
	{
		std::vector<uint8> notVba(32, 0x41);
		std::vector<std::string> badNames;
		Check(!ListVbaMacroNames(&notVba[0], notVba.size(), &badNames),
			"dati che non sono affatto un progetto VBA producono false, non un crash");
		Check(badNames.empty(), "l'elenco resta vuoto quando il progetto non e' valido");
	}
}

int main()
{
	TestFindVbaSubByName();
	TestExecuteVbaMacro();
	TestSiblingSubCall();
	TestSiblingFunctionCall();
	TestArrays();
	TestListVbaMacroNames();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
