/*
	vba_dir_stream_test.cpp

	Verifica ParseVbaDirStream (VbaDirStream.h -- Path to full Excel
	parity, VBA/macro execution, Fase 1c) contro stream "dir" sintetici
	costruiti a mano da AppendRecord()/AppendProjectVersion() sotto,
	con gli stessi ID di record descritti in [MS-OVBA] 2.3.4.2.

	Stesso limite gia' segnalato in VbaDecompress.h/ROADMAP.md: nessun
	vero progetto VBA prodotto da Microsoft Excel era disponibile in
	questo ambiente, quindi questo verifica la coerenza interna (un
	record scritto con un dato ID viene riconosciuto con quell'ID),
	non la fedelta' degli ID stessi alla specifica reale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include "VbaDirStream.h"

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

static const uint16 kIdProjectVersion = 0x0009;
static const uint16 kIdModuleName = 0x0019;
static const uint16 kIdModuleNameUnicode = 0x0047;
static const uint16 kIdModuleStreamName = 0x001A;
static const uint16 kIdModuleStreamNameUnicode = 0x0032;
static const uint16 kIdModuleOffset = 0x0031;
static const uint16 kIdModuleTypeProcedural = 0x0021;
static const uint16 kIdModuleTypeDocument = 0x0022;
static const uint16 kIdModuleTerminator = 0x002B;
static const uint16 kIdProjectCodePage = 0x0003;    // non necessario per l'elenco moduli
static const uint16 kIdProjectName = 0x0004;         // idem
static const uint16 kIdProjectTerminator = 0x0010;   // idem

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

static void AppendRecord(std::vector<uint8>& buf, uint16 id, const uint8* data, uint32 size)
{
	AppendU16(buf, id);
	AppendU32(buf, size);
	buf.insert(buf.end(), data, data + size);
}

static void AppendRecordStr(std::vector<uint8>& buf, uint16 id, const char* s)
{
	AppendRecord(buf, id, (const uint8*)s, (uint32)strlen(s));
}

static void AppendRecordU16(std::vector<uint8>& buf, uint16 id, uint16 value)
{
	uint8 data[2] = { (uint8)(value & 0xFF), (uint8)((value >> 8) & 0xFF) };
	AppendRecord(buf, id, data, 2);
}

static void AppendRecordU32(std::vector<uint8>& buf, uint16 id, uint32 value)
{
	uint8 data[4];
	for (int i = 0; i < 4; i++)
		data[i] = (uint8)((value >> (8 * i)) & 0xFF);
	AppendRecord(buf, id, data, 4);
}

static void AppendUnicodeStr(std::vector<uint8>& buf, uint16 id, const char* s)
{
	std::vector<uint8> utf16;
	for (const char* p = s; *p; p++)
	{
		utf16.push_back((uint8)*p);
		utf16.push_back(0);
	}
	AppendRecord(buf, id, utf16.empty() ? NULL : &utf16[0], (uint32)utf16.size());
}

// [MS-OVBA] 2.3.4.2.4: Id(2) + Reserved(4) + VersionMajor(4) + VersionMinor(2),
// NESSUN campo Size -- vedi il commento in cima a VbaDirStream.h.
static void AppendProjectVersion(std::vector<uint8>& buf)
{
	AppendU16(buf, kIdProjectVersion);
	AppendU32(buf, 4);  // Reserved, sempre 4 per specifica
	AppendU32(buf, 1);  // VersionMajor (valore arbitrario per il test)
	AppendU16(buf, 0);  // VersionMinor
}

static void AppendModule(std::vector<uint8>& buf, const char* ansiName, const char* unicodeName,
	const char* ansiStream, const char* unicodeStream, uint32 textOffset, bool isDocument)
{
	AppendRecordStr(buf, kIdModuleName, ansiName);
	if (unicodeName != NULL)
		AppendUnicodeStr(buf, kIdModuleNameUnicode, unicodeName);
	AppendRecordStr(buf, kIdModuleStreamName, ansiStream);
	if (unicodeStream != NULL)
		AppendUnicodeStr(buf, kIdModuleStreamNameUnicode, unicodeStream);
	AppendRecordU32(buf, kIdModuleOffset, textOffset);
	AppendRecord(buf, isDocument ? kIdModuleTypeDocument : kIdModuleTypeProcedural, NULL, 0);
	AppendRecord(buf, kIdModuleTerminator, NULL, 0);
}

int main()
{
	// ---- Caso base: intestazione di progetto irrilevante (saltata
	// genericamente via il proprio Size) + due moduli, uno procedurale
	// e uno documento. ----
	{
		std::vector<uint8> dir;
		AppendProjectVersion(dir);                 // record irregolare, deve non disallineare nulla
		AppendRecordU16(dir, kIdProjectCodePage, 1252);
		AppendRecordStr(dir, kIdProjectName, "VBAProject");
		AppendModule(dir, "Module1", "Module1", "Module1", "Module1", 451, false);
		AppendModule(dir, "ThisWorkbook", "ThisWorkbook", "ThisWorkbook", "ThisWorkbook", 892, true);
		AppendRecord(dir, kIdProjectTerminator, NULL, 0);

		std::vector<VbaModuleInfo> modules;
		bool ok = ParseVbaDirStream(dir.empty() ? NULL : &dir[0], dir.size(), &modules);
		Check(ok, "ParseVbaDirStream riesce su uno stream \"dir\" sintetico valido");
		Check(modules.size() == 2, "trova esattamente i due moduli descritti");
		if (modules.size() == 2)
		{
			Check(modules[0].name == "Module1", "il primo modulo si chiama \"Module1\"");
			Check(modules[0].streamName == "Module1", "il primo modulo vive nello stream \"Module1\"");
			Check(modules[0].textOffset == 451, "l'offset del testo del primo modulo e' 451");
			Check(!modules[0].isDocumentModule, "il primo modulo e' procedurale, non un documento");

			Check(modules[1].name == "ThisWorkbook", "il secondo modulo si chiama \"ThisWorkbook\"");
			Check(modules[1].textOffset == 892, "l'offset del testo del secondo modulo e' 892");
			Check(modules[1].isDocumentModule, "il secondo modulo e' un modulo documento");
		}
	}

	// ---- La variante Unicode del nome deve prevalere su quella ANSI
	// quando entrambe sono presenti e DIVERSE (non solo coincidenza). ----
	{
		std::vector<uint8> dir;
		AppendModule(dir, "ANSI_NAME", "UnicodePreferredName", "ANSI_STREAM", "UnicodePreferredStream",
			10, false);

		std::vector<VbaModuleInfo> modules;
		bool ok = ParseVbaDirStream(&dir[0], dir.size(), &modules);
		Check(ok && modules.size() == 1, "parsing riuscito con un modulo");
		if (ok && modules.size() == 1)
		{
			Check(modules[0].name == "UnicodePreferredName",
				"il nome Unicode prevale su quello ANSI quando entrambi sono presenti");
			Check(modules[0].streamName == "UnicodePreferredStream",
				"il nome di stream Unicode prevale su quello ANSI quando entrambi sono presenti");
		}
	}

	// ---- Un modulo con SOLO la variante ANSI (nessun record Unicode)
	// deve comunque funzionare, come fallback. ----
	{
		std::vector<uint8> dir;
		AppendModule(dir, "OnlyAnsiName", NULL, "OnlyAnsiStream", NULL, 5, false);

		std::vector<VbaModuleInfo> modules;
		bool ok = ParseVbaDirStream(&dir[0], dir.size(), &modules);
		Check(ok && modules.size() == 1 && modules[0].name == "OnlyAnsiName",
			"un modulo con solo il nome ANSI (nessuna variante Unicode) si legge comunque correttamente");
	}

	// ---- Stream "dir" vuoto: zero moduli, non un errore. ----
	{
		std::vector<uint8> empty;
		std::vector<VbaModuleInfo> modules;
		bool ok = ParseVbaDirStream(NULL, 0, &modules);
		Check(ok && modules.empty(), "uno stream \"dir\" vuoto restituisce zero moduli, non un errore");
	}

	// ---- Input malformati: mai un crash, sempre false in modo pulito. ----
	{
		// Un record che dichiara piu' byte di Size di quanti ne restino davvero.
		std::vector<uint8> dir;
		AppendU16(dir, kIdModuleName);
		AppendU32(dir, 1000); // dichiara 1000 byte di dati, ma non ce ne sono cosi' tanti
		dir.push_back('X'); // un solo byte vero

		std::vector<VbaModuleInfo> modules;
		Check(!ParseVbaDirStream(&dir[0], dir.size(), &modules),
			"un record che dichiara piu' byte di quanti ne restino restituisce false, "
			"non legge oltre il buffer");
	}
	{
		// Un modulo iniziato (MODULENAME) ma mai chiuso da MODULETERMINATOR.
		std::vector<uint8> dir;
		AppendRecordStr(dir, kIdModuleName, "UnterminatedModule");
		AppendRecordU32(dir, kIdModuleOffset, 100);
		// nessun MODULETERMINATOR

		std::vector<VbaModuleInfo> modules;
		Check(!ParseVbaDirStream(&dir[0], dir.size(), &modules),
			"un modulo iniziato ma mai terminato restituisce false, non un elenco a meta'");
	}
	{
		// Id troncato (un solo byte restante, serve 2 per leggerlo).
		std::vector<uint8> dir(1, 0x19);
		std::vector<VbaModuleInfo> modules;
		Check(!ParseVbaDirStream(&dir[0], dir.size(), &modules),
			"un Id di record troncato restituisce false, non legge oltre il buffer");
	}
	{
		// PROJECTVERSION troncato (meno di 10 byte di corpo disponibili).
		std::vector<uint8> dir;
		AppendU16(dir, kIdProjectVersion);
		dir.push_back(0x04); // solo un byte del campo Reserved da 4

		std::vector<VbaModuleInfo> modules;
		Check(!ParseVbaDirStream(&dir[0], dir.size(), &modules),
			"un record PROJECTVERSION troncato (il caso a formato irregolare) "
			"restituisce false, non legge oltre il buffer");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
