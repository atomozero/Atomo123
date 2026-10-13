/*
	VbaDirStream.cpp

	Vedi VbaDirStream.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaDirStream.h"

// ID dei record [MS-OVBA] 2.3.4.2 che servono per l'elenco dei moduli
// -- ogni altro ID viene saltato genericamente via il proprio campo
// Size, mai elencato qui per nome (vedi il commento in cima all'header
// sul perche' questo non serve capire l'intera sezione
// PROJECTINFORMATION/PROJECTREFERENCES per arrivare a PROJECTMODULES).
static const uint16 kIdProjectVersion = 0x0009; // formato irregolare, vedi sotto
static const uint16 kIdModuleName = 0x0019;
static const uint16 kIdModuleNameUnicode = 0x0047;
static const uint16 kIdModuleStreamName = 0x001A;
static const uint16 kIdModuleStreamNameUnicode = 0x0032;
static const uint16 kIdModuleOffset = 0x0031;
static const uint16 kIdModuleTypeProcedural = 0x0021;
static const uint16 kIdModuleTypeDocument = 0x0022;
static const uint16 kIdModuleTerminator = 0x002B;

static uint16 ReadU16(const uint8* p)
{
	return (uint16)p[0] | ((uint16)p[1] << 8);
}

static uint32 ReadU32(const uint8* p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}

// Stessa semplificazione gia' usata altrove in questo codice
// (CompoundFileReader::Open per i nomi degli stream OLE2): un
// troncamento a un byte per carattere, niente vero decoder UTF-16.
// Nomi di modulo reali sono quasi sempre ASCII; un nome con caratteri
// davvero non ASCII finirebbe troncato in modo scorretto, limite
// accettato per ora, stesso spirito del resto di questo file.
static std::string Utf16LeToAscii(const uint8* data, size_t byteLen)
{
	std::string out;
	out.reserve(byteLen / 2);
	for (size_t i = 0; i + 1 < byteLen; i += 2)
		out += (char)(data[i] & 0xFF);
	return out;
}

bool ParseVbaDirStream(const uint8* data, size_t len, std::vector<VbaModuleInfo>* outModules)
{
	outModules->clear();
	size_t pos = 0;
	VbaModuleInfo current;
	bool inModule = false;

	while (pos < len)
	{
		if (pos + 2 > len)
			return false; // Id troncato
		uint16 id = ReadU16(data + pos);
		pos += 2;

		if (id == kIdProjectVersion)
		{
			// [MS-OVBA] 2.3.4.2.4: NON ha un campo Size -- corpo fisso
			// Reserved(4) + VersionMajor(4) + VersionMinor(2), sempre
			// 10 byte, trattarlo come Id+Size+Dati generico
			// disallineerebbe tutto cio' che segue.
			if (pos + 10 > len)
				return false;
			pos += 10;
			continue;
		}

		if (pos + 4 > len)
			return false; // Size troncato
		uint32 size = ReadU32(data + pos);
		pos += 4;
		if (pos + size > len)
			return false; // il record dichiara piu' byte di quanti ne restino

		const uint8* recordData = data + pos;

		switch (id)
		{
			case kIdModuleName:
				if (!inModule)
				{
					current = VbaModuleInfo();
					inModule = true;
				}
				// Riempie il nome solo se la variante unicode (letta
				// dopo, vedi kIdModuleNameUnicode) non l'ha gia' fatto
				// -- MODULENAME viene sempre prima nell'ordine reale
				// del formato, quindi in pratica questo e' sempre il
				// primo a scrivere current.name.
				current.name = std::string((const char*)recordData, size);
				break;

			case kIdModuleNameUnicode:
				if (!inModule)
				{
					current = VbaModuleInfo();
					inModule = true;
				}
				// Preferita: sovrascrive il nome ANSI gia' letto sopra.
				current.name = Utf16LeToAscii(recordData, size);
				break;

			case kIdModuleStreamName:
				if (inModule)
					current.streamName = std::string((const char*)recordData, size);
				break;

			case kIdModuleStreamNameUnicode:
				if (inModule)
					current.streamName = Utf16LeToAscii(recordData, size);
				break;

			case kIdModuleOffset:
				if (inModule && size == 4)
					current.textOffset = ReadU32(recordData);
				break;

			case kIdModuleTypeProcedural:
				if (inModule)
					current.isDocumentModule = false;
				break;

			case kIdModuleTypeDocument:
				if (inModule)
					current.isDocumentModule = true;
				break;

			case kIdModuleTerminator:
				if (inModule)
				{
					outModules->push_back(current);
					inModule = false;
				}
				break;

			default:
				// Record non necessario per l'elenco dei moduli
				// (riferimenti di progetto, docstring, codepage, help
				// context, ...): gia' saltato correttamente sopra via
				// il proprio campo Size, niente da fare qui.
				break;
		}

		pos += size;
	}

	// Un modulo iniziato (MODULENAME visto) ma mai chiuso da un
	// MODULETERMINATOR e' uno stream "dir" troncato/malformato, non un
	// caso valido da restituire silenziosamente a meta'.
	return !inModule;
}
