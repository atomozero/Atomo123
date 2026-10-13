/*
	vba_dir_stream_test.cpp

	Verifica ParseVbaDirStream (VbaDirStream.h -- Path to full Excel
	parity, VBA/macro execution, Fase 1c) contro stream "dir" sintetici
	costruiti a mano da AppendRecord()/AppendProjectVersion() sotto,
	con gli stessi ID di record descritti in [MS-OVBA] 2.3.4.2.

	I casi sintetici sotto verificano solo la coerenza interna (un
	record scritto con un dato ID viene riconosciuto con quell'ID). I
	DUE casi "MS-OVBA ufficiale" in fondo invece sono i byte ESATTI di
	due esempi pubblicati dalla specifica [MS-OVBA] stessa (sezione
	3.1.1/3.1.2, "ThisWorkbook Document Module Record Example" e
	"Sheet1 Document Module Record Example" --
	https://learn.microsoft.com/en-us/openspecs/office_file_formats/ms-ovba/12fae4e5-a9ca-4f02-a564-848fd4ec0413
	e
	https://learn.microsoft.com/en-us/openspecs/office_file_formats/ms-ovba/9a7f6068-6bb4-4000-9fab-08a9612736f1):
	confermano che OGNI Id di record usato da questo parser (incluso il
	dettaglio meno ovvio che MODULESTREAMNAMEUNICODE/0x0032 e
	MODULEDOCSTRINGUNICODE/0x0048 sono, sul disco reale, record
	generici Id+Size+Dati separati e consecutivi, non campi annidati
	dentro un unico record piu' grande nonostante la tabella della
	specifica li raggruppi sotto un solo nome per la documentazione)
	corrisponde esattamente a quello vero -- non solo coerenza interna
	come il resto di questo file.

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

	// ---- Vettori di test ufficiali [MS-OVBA] (sezione 3.1.1/3.1.2), non auto-coerenza ----
	// Vedi il commento in cima al file. Byte ESATTI dagli esempi
	// pubblicati da Microsoft stesso -- ricostruiti a mano dalla
	// tabella Offset/Size/Struttura/Valore di ciascuna pagina, con la
	// dimensione totale di ogni record verificata di nuovo contro
	// quella dichiarata nella prima riga della tabella (0x94=148 byte
	// per "ThisWorkbook", 0xBE=190 byte per "Sheet1") prima di fidarsi
	// della trascrizione.
	{
		// "ThisWorkbook Document Module Record Example" (sezione 3.1.1):
		// nome modulo e nome stream entrambi "ThisWorkbook", offset
		// del testo 0, modulo documento (0x0022), docstring/help
		// context vuoti.
		unsigned bytes[] = {
			0x19,0x00, 0x0C,0x00,0x00,0x00, 'T','h','i','s','W','o','r','k','b','o','o','k',
			0x47,0x00, 0x18,0x00,0x00,0x00,
				'T',0,'h',0,'i',0,'s',0,'W',0,'o',0,'r',0,'k',0,'b',0,'o',0,'o',0,'k',0,
			0x1A,0x00, 0x0C,0x00,0x00,0x00, 'T','h','i','s','W','o','r','k','b','o','o','k',
			0x32,0x00, 0x18,0x00,0x00,0x00,
				'T',0,'h',0,'i',0,'s',0,'W',0,'o',0,'r',0,'k',0,'b',0,'o',0,'o',0,'k',0,
			0x1C,0x00, 0x00,0x00,0x00,0x00,
			0x48,0x00, 0x00,0x00,0x00,0x00,
			0x31,0x00, 0x04,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
			0x1E,0x00, 0x04,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
			0x2C,0x00, 0x02,0x00,0x00,0x00, 0xFF,0xFF,
			0x22,0x00, 0x00,0x00,0x00,0x00,
			0x2B,0x00, 0x00,0x00,0x00,0x00,
		};
		std::vector<uint8> dir;
		for (unsigned b : bytes)
			dir.push_back((uint8)b);
		Check(dir.size() == 148, "[MS-OVBA] vettore \"ThisWorkbook\": la trascrizione totalizza "
			"148 byte, come dichiarato dalla tabella della specifica (0x94)");

		std::vector<VbaModuleInfo> modules;
		bool ok = ParseVbaDirStream(&dir[0], dir.size(), &modules);
		Check(ok, "[MS-OVBA] vettore ufficiale \"ThisWorkbook\": l'analisi riesce senza errori");
		Check(ok && modules.size() == 1, "[MS-OVBA] vettore ufficiale \"ThisWorkbook\": trova esattamente un modulo");
		if (ok && modules.size() == 1)
		{
			Check(modules[0].name == "ThisWorkbook",
				"[MS-OVBA] vettore ufficiale \"ThisWorkbook\": il nome del modulo combacia");
			Check(modules[0].streamName == "ThisWorkbook",
				"[MS-OVBA] vettore ufficiale \"ThisWorkbook\": il nome dello stream combacia");
			Check(modules[0].textOffset == 0,
				"[MS-OVBA] vettore ufficiale \"ThisWorkbook\": l'offset del testo combacia (0)");
			Check(modules[0].isDocumentModule,
				"[MS-OVBA] vettore ufficiale \"ThisWorkbook\": riconosciuto come modulo documento");
		}
	}
	{
		// "Sheet1 Document Module Record Example" (sezione 3.1.2):
		// nomi piu' corti ("Sheet1"), e stavolta con un vero
		// MODULEDOCSTRING non vuoto ("Example Module Description") e
		// un vero MODULEHELPCONTEXT (2) -- prova che il salto generico
		// via il campo Size funziona anche per record con contenuto
		// vero, non solo per quelli vuoti gia' coperti dall'esempio
		// precedente.
		unsigned bytes[] = {
			0x19,0x00, 0x06,0x00,0x00,0x00, 'S','h','e','e','t','1',
			0x47,0x00, 0x0C,0x00,0x00,0x00, 'S',0,'h',0,'e',0,'e',0,'t',0,'1',0,
			0x1A,0x00, 0x06,0x00,0x00,0x00, 'S','h','e','e','t','1',
			0x32,0x00, 0x0C,0x00,0x00,0x00, 'S',0,'h',0,'e',0,'e',0,'t',0,'1',0,
			0x1C,0x00, 0x1A,0x00,0x00,0x00,
				'E','x','a','m','p','l','e',' ','M','o','d','u','l','e',' ',
				'D','e','s','c','r','i','p','t','i','o','n',
			0x48,0x00, 0x34,0x00,0x00,0x00,
				'E',0,'x',0,'a',0,'m',0,'p',0,'l',0,'e',0,' ',0,
				'M',0,'o',0,'d',0,'u',0,'l',0,'e',0,' ',0,
				'D',0,'e',0,'s',0,'c',0,'r',0,'i',0,'p',0,'t',0,'i',0,'o',0,'n',0,
			0x31,0x00, 0x04,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
			0x1E,0x00, 0x04,0x00,0x00,0x00, 0x02,0x00,0x00,0x00,
			0x2C,0x00, 0x02,0x00,0x00,0x00, 0xFF,0xFF,
			0x22,0x00, 0x00,0x00,0x00,0x00,
			0x2B,0x00, 0x00,0x00,0x00,0x00,
		};
		std::vector<uint8> dir;
		for (unsigned b : bytes)
			dir.push_back((uint8)b);
		Check(dir.size() == 190, "[MS-OVBA] vettore \"Sheet1\": la trascrizione totalizza "
			"190 byte, come dichiarato dalla tabella della specifica (0xBE)");

		std::vector<VbaModuleInfo> modules;
		bool ok = ParseVbaDirStream(&dir[0], dir.size(), &modules);
		Check(ok, "[MS-OVBA] vettore ufficiale \"Sheet1\": l'analisi riesce senza errori, "
			"incluso il salto corretto di un vero MODULEDOCSTRING non vuoto");
		Check(ok && modules.size() == 1, "[MS-OVBA] vettore ufficiale \"Sheet1\": trova esattamente un modulo");
		if (ok && modules.size() == 1)
		{
			Check(modules[0].name == "Sheet1",
				"[MS-OVBA] vettore ufficiale \"Sheet1\": il nome del modulo combacia");
			Check(modules[0].streamName == "Sheet1",
				"[MS-OVBA] vettore ufficiale \"Sheet1\": il nome dello stream combacia");
			Check(modules[0].isDocumentModule,
				"[MS-OVBA] vettore ufficiale \"Sheet1\": riconosciuto come modulo documento");
		}
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
