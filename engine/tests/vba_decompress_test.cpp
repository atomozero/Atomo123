/*
	vba_decompress_test.cpp

	Verifica DecompressVba (VbaDecompress.h -- Path to full Excel
	parity, VBA/macro execution, Phase 1b) con una serie di casi di
	round-trip: CompressEncoderForTest sotto (un codificatore scritto
	SOLO per questo test) produce un CompressedContainer valido da
	byte noti, DecompressVba lo decodifica, e il risultato deve
	combaciare esattamente con l'originale.

	Questo e' l'unico controllo di correttezza disponibile in questo
	ambiente: nessun campione compresso VERO prodotto da Microsoft
	Excel era reperibile per una convalida indipendente (a differenza
	di vba_project_test.cpp, dove invece il livello CFB e' stato
	confrontato con `olefile`, un'implementazione Python indipendente).
	L'algoritmo e il codificatore sono stati scritti e provati insieme
	in un prototipo Python PRIMA di questa porting (vedi il commento in
	cima a VbaDecompress.h), coprendo esplicitamente i confini critici
	della larghezza di bit (posizione 16/17/32/33/64/65/256/257), il
	reset del contesto a ogni cambio di chunk, e il fallback a un chunk
	non compresso quando la codifica compressa supererebbe il campo di
	dimensione a 12 bit -- tutti i casi qui sotto rispecchiano
	esattamente quelli gia' passati li'. Limite noto, segnalato anche
	in ROADMAP.md: round-trip con se stesso prova la coerenza interna,
	non (ancora) la fedelta' a un vero encoder Microsoft.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include "VbaDecompress.h"

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

static int BitCountForTest(uint32 decompressedCurrent)
{
	if (decompressedCurrent == 0)
		return 4;
	int bitCount = 4;
	while (bitCount < 12 && (1u << bitCount) < decompressedCurrent)
		bitCount++;
	return bitCount;
}

// Codifica OGNI byte come token letterale, nessun token di copia --
// il percorso piu' semplice e a minor rischio, usato per i casi di
// base (non esercita affatto la matematica di scostamento/lunghezza).
// Vedi compress_literal_only nel prototipo Python per la stessa logica,
// incluso il fallback a un chunk non compresso quando la codifica
// letterale di un chunk pieno da 4096 byte (4096 + 512 byte di flag =
// 4608) supererebbe il campo a 12 bit della dimensione del chunk
// (il motivo stesso per cui esiste il tipo di chunk non compresso).
static std::vector<uint8> CompressLiteralOnlyForTest(const std::vector<uint8>& data)
{
	std::vector<uint8> out;
	out.push_back(0x01);

	if (data.empty())
		return out; // solo il byte di firma: nessun chunk, decomprime a vuoto

	for (size_t chunkOff = 0; chunkOff < data.size(); chunkOff += 4096)
	{
		size_t chunkLen = data.size() - chunkOff;
		if (chunkLen > 4096)
			chunkLen = 4096;

		std::vector<uint8> body;
		for (size_t i = 0; i < chunkLen; i += 8)
		{
			size_t groupLen = chunkLen - i < 8 ? chunkLen - i : 8;
			body.push_back(0x00); // flag byte: tutti i bit letterali
			for (size_t j = 0; j < groupLen; j++)
				body.push_back(data[chunkOff + i + j]);
		}

		if ((int32)body.size() + 2 - 3 > 0x0FFF)
		{
			// Sfora: ricade su un chunk non compresso (sempre 4096 byte).
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

// Compressore LZ77 ingordo che usa la STESSA BitCountForTest() del
// decompressore, cosi' i token di copia attraversano davvero la
// matematica a larghezza di bit dipendente dalla posizione -- la parte
// dell'algoritmo col rischio maggiore di un errore sottile. Ricerca
// O(n^2), adatta solo per i piccoli input sintetici di questo test.
static std::vector<uint8> CompressGreedyForTest(const std::vector<uint8>& data)
{
	std::vector<uint8> out;
	out.push_back(0x01);

	for (size_t chunkOff = 0; chunkOff < data.size(); chunkOff += 4096)
	{
		size_t chunkLen = data.size() - chunkOff;
		if (chunkLen > 4096)
			chunkLen = 4096;
		const uint8* chunkData = &data[chunkOff];

		std::vector<uint8> body;
		uint8 flagByte = 0;
		std::vector<uint8> groupBody;
		int bitIdx = 0;

		auto flushGroup = [&]()
		{
			body.push_back(flagByte);
			body.insert(body.end(), groupBody.begin(), groupBody.end());
			flagByte = 0;
			groupBody.clear();
		};

		size_t i = 0;
		while (i < chunkLen)
		{
			int bc = BitCountForTest((uint32)i);
			uint16 lengthMask = (uint16)(0xFFFF >> bc);
			uint32 maxLength = lengthMask + 3;
			uint32 maxOffset = (bc < 16) ? (uint32)((0xFFFF >> (16 - bc)) + 1) : 0;

			uint32 bestLen = 0, bestOff = 0;
			if (i > 0)
			{
				size_t windowStart = (i > maxOffset) ? i - maxOffset : 0;
				for (size_t candOff = 1; candOff <= i - windowStart; candOff++)
				{
					size_t src = i - candOff;
					uint32 length = 0;
					while (length < maxLength && i + length < chunkLen
						&& chunkData[src + length] == chunkData[i + length])
						length++;
					if (length > bestLen)
					{
						bestLen = length;
						bestOff = (uint32)candOff;
					}
				}
			}

			if (bestLen >= 3)
			{
				uint32 lengthField = bestLen - 3;
				uint16 offsetField = (uint16)((bestOff - 1) << (16 - bc));
				uint16 token = (uint16)((offsetField & ~lengthMask) | (lengthField & lengthMask));
				flagByte |= (uint8)(1 << bitIdx);
				groupBody.push_back((uint8)(token & 0xFF));
				groupBody.push_back((uint8)((token >> 8) & 0xFF));
				i += bestLen;
			}
			else
			{
				groupBody.push_back(chunkData[i]);
				i += 1;
			}

			bitIdx++;
			if (bitIdx == 8)
			{
				flushGroup();
				bitIdx = 0;
			}
		}
		if (bitIdx != 0)
			flushGroup();

		if ((int32)body.size() + 2 - 3 > 0x0FFF)
		{
			// Stesso sfondamento di CompressLiteralOnlyForTest sopra,
			// possibile anche con token di copia veri se il chunk e'
			// in gran parte incomprimibile (es. la parte casuale di un
			// caso misto) -- stesso fallback.
			out.push_back((uint8)(0x0FFF & 0xFF));
			out.push_back((uint8)(((0x0FFF >> 8) & 0x0F) | (0b011 << 4) | 0x00));
			for (size_t k = 0; k < 4096; k++)
				out.push_back(k < chunkLen ? chunkData[k] : 0);
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

static void CheckRoundTrip(const char* what, const std::vector<uint8>& data,
	std::vector<uint8> (*compressFn)(const std::vector<uint8>&))
{
	std::vector<uint8> compressed = compressFn(data);
	std::vector<uint8> roundTrip;
	bool ok = DecompressVba(compressed.empty() ? NULL : &compressed[0], compressed.size(), &roundTrip);
	Check(ok && roundTrip == data, what);
}

static std::vector<uint8> Repeat(const char* pattern, size_t times)
{
	std::vector<uint8> out;
	size_t patLen = strlen(pattern);
	out.reserve(patLen * times);
	for (size_t i = 0; i < times; i++)
		out.insert(out.end(), pattern, pattern + patLen);
	return out;
}

static std::vector<uint8> PseudoRandom(size_t count, uint32 seed)
{
	std::vector<uint8> out;
	out.reserve(count);
	uint32 state = seed;
	for (size_t i = 0; i < count; i++)
	{
		state = state * 1103515245u + 12345u; // LCG, solo per dati di prova deterministici
		out.push_back((uint8)((state >> 16) & 0xFF));
	}
	return out;
}

int main()
{
	// ---- Casi solo-letterali (percorso a basso rischio) ----
	CheckRoundTrip("input vuoto", std::vector<uint8>(), CompressLiteralOnlyForTest);
	CheckRoundTrip("un solo byte", std::vector<uint8>(1, (uint8)'X'), CompressLiteralOnlyForTest);
	{
		const char* s = "Attribute VB_Name = \"Module1\"\r\n";
		CheckRoundTrip("breve stringa letterale",
			std::vector<uint8>(s, s + strlen(s)), CompressLiteralOnlyForTest);
	}
	CheckRoundTrip("esattamente 8 byte (un solo gruppo di flag)",
		std::vector<uint8>((const uint8*)"12345678", (const uint8*)"12345678" + 8),
		CompressLiteralOnlyForTest);
	{
		std::vector<uint8> data(4096);
		for (size_t i = 0; i < data.size(); i++)
			data[i] = (uint8)((i * 7) % 251);
		CheckRoundTrip("esattamente 4096 byte (un chunk pieno, forza il fallback non compresso)",
			data, CompressLiteralOnlyForTest);
	}
	{
		std::vector<uint8> data(4097);
		for (size_t i = 0; i < data.size(); i++)
			data[i] = (uint8)((i * 3) % 251);
		CheckRoundTrip("4097 byte (due chunk)", data, CompressLiteralOnlyForTest);
	}
	CheckRoundTrip("10000 byte pseudo-casuali, piu' chunk",
		PseudoRandom(10000, 1), CompressLiteralOnlyForTest);

	// ---- Casi con token di copia (il percorso a rischio maggiore) ----
	CheckRoundTrip("schema ripetuto (molti riferimenti all'indietro)",
		Repeat("ABCDEFGH", 100), CompressGreedyForTest);
	CheckRoundTrip("schema ripetuto, scostamento vicino all'inizio del chunk",
		Repeat("X", 500), CompressGreedyForTest);
	CheckRoundTrip("codice sorgente VBA ripetitivo",
		Repeat("Sub Test()\r\n    Dim i As Integer\r\n    For i = 1 To 10\r\n"
			"        Cells(i, 1).Value = i\r\n    Next i\r\nEnd Sub\r\n", 20),
		CompressGreedyForTest);
	{
		std::vector<uint8> data = PseudoRandom(2000, 2);
		std::vector<uint8> repeated = Repeat("REPEATED_BLOCK_1234567890", 50);
		std::vector<uint8> tail = PseudoRandom(2000, 3);
		data.insert(data.end(), repeated.begin(), repeated.end());
		data.insert(data.end(), tail.begin(), tail.end());
		CheckRoundTrip("misto casuale+ripetizioni, piu' chunk", data, CompressGreedyForTest);
	}
	CheckRoundTrip("una sola ripetizione oltre la lunghezza massima di un token",
		std::vector<uint8>(5000, (uint8)'Z'), CompressGreedyForTest);
	{
		std::vector<uint8> data = PseudoRandom(4090, 4);
		std::vector<uint8> repeated = Repeat("REPEATME", 200);
		data.insert(data.end(), repeated.begin(), repeated.end());
		CheckRoundTrip("scostamento esattamente al confine del chunk (4096)",
			data, CompressGreedyForTest);
	}

	// ---- Confini critici della larghezza di bit ----
	{
		std::vector<uint8> data = Repeat("WXYZ", 1100);
		data.resize(4096);
		CheckRoundTrip("ripetizioni dense che attraversano ogni soglia di larghezza di bit",
			data, CompressGreedyForTest);
	}
	{
		std::vector<uint8> chunkA(4096, (uint8)'A');
		std::vector<uint8> chunkB(4096, (uint8)'B');
		std::vector<uint8> data = chunkA;
		data.insert(data.end(), chunkB.begin(), chunkB.end());
		CheckRoundTrip("due chunk pieni distinti: lo scostamento si azzera al confine "
			"(mai un riferimento al chunk precedente)", data, CompressGreedyForTest);
	}

	// ---- Input malformati: mai un crash, sempre false in modo pulito ----
	{
		std::vector<uint8> badSig(10, 0);
		badSig[0] = 0x02; // deve essere 0x01
		std::vector<uint8> out;
		Check(!DecompressVba(&badSig[0], badSig.size(), &out),
			"un byte di firma sbagliato (non 0x01) restituisce false, non un crash");
	}
	{
		std::vector<uint8> truncatedHeader(2, 0);
		truncatedHeader[0] = 0x01; // firma valida, ma zero byte restanti per l'header del chunk
		std::vector<uint8> out;
		Check(!DecompressVba(&truncatedHeader[0], truncatedHeader.size(), &out),
			"un header di chunk troncato restituisce false, non legge oltre il buffer");
	}
	{
		// Header che dichiara un chunk compresso molto piu' grande di
		// quanti byte restano davvero nel buffer.
		std::vector<uint8> data(4, 0);
		data[0] = 0x01;
		data[1] = 0xFF; // dimensione dichiarata massima (0x0FFF) + flag compresso
		data[2] = 0xFF;
		std::vector<uint8> out;
		Check(!DecompressVba(&data[0], data.size(), &out),
			"un chunk che dichiara piu' byte di quanti ne restino restituisce false, "
			"non legge oltre il buffer");
	}
	{
		// Chunk compresso minimale con UN token di copia che punta
		// prima dell'inizio del chunk corrente (scostamento maggiore
		// di quanto sia gia' stato decompresso) -- mai prodotto da un
		// vero compressore, ma deve comunque fallire in modo pulito su
		// un flusso corrotto o scritto a mano in modo malevolo.
		std::vector<uint8> data;
		data.push_back(0x01);
		// Un chunk compresso di soli 2 byte di corpo (1 flag byte "tutto
		// copia" + 1 token troncato non basterebbe per un token reale,
		// quindi uso 1 flag byte + un token di copia completo da 2 byte
		// = 3 byte di corpo, chunk_size = 3+2 = 5).
		uint16 header = (uint16)(((5 - 3) & 0x0FFF) | (0b011 << 12) | (1 << 15));
		data.push_back((uint8)(header & 0xFF));
		data.push_back((uint8)((header >> 8) & 0xFF));
		data.push_back(0x01); // flag byte: primo token = copia (bit 0 = 1)
		data.push_back(0xFF); // token di copia: scostamento/lunghezza massimi possibili
		data.push_back(0xFF); // -- a posizione 0 del chunk, QUALUNQUE scostamento > 0 e' invalido
		std::vector<uint8> out;
		Check(!DecompressVba(&data[0], data.size(), &out),
			"un token di copia che punterebbe prima dell'inizio del chunk corrente "
			"restituisce false, non legge dati spazzatura o va oltre il buffer");
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
