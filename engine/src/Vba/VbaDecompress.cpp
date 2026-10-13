/*
	VbaDecompress.cpp

	Vedi VbaDecompress.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaDecompress.h"

static const size_t kRawChunkDataSize = 4096;

// Larghezza di bit (4..12) usata per spezzare un CopyToken da 16 bit in
// campo lunghezza/campo scostamento -- dipende da quanti byte sono gia'
// stati decompressi DENTRO AL CHUNK CORRENTE (si azzera a ogni nuovo
// chunk, vedi il reset di "decompressedChunkStart" sotto). Equivalente
// intero di "il piu' piccolo bitCount tale che 2^bitCount >=
// decompressedCurrent", prima calcolato con ceil(log2(...)) nel
// prototipo Python -- qui una scansione di bit evita qualunque rischio
// di arrotondamento in virgola mobile sui valori di confine (16, 32,
// 64, 256, ... tutti verificati uno per uno contro il prototipo prima
// di questa porting).
static int BitCountFor(uint32 decompressedCurrent)
{
	if (decompressedCurrent == 0)
		return 4;
	int bitCount = 4;
	while (bitCount < 12 && (1u << bitCount) < decompressedCurrent)
		bitCount++;
	return bitCount;
}

bool DecompressVba(const uint8* data, size_t len, std::vector<uint8>* out)
{
	out->clear();
	if (data == NULL || len < 1 || data[0] != 0x01)
		return false;

	size_t pos = 1;
	while (pos < len)
	{
		if (pos + 2 > len)
			return false; // header di chunk troncato

		uint16 header = (uint16)data[pos] | ((uint16)data[pos + 1] << 8);
		size_t chunkStart = pos;
		pos += 2;

		size_t chunkSize = (size_t)(header & 0x0FFF) + 3;
		bool chunkCompressed = (header & 0x8000) != 0;
		size_t chunkEnd = chunkStart + chunkSize;
		if (chunkEnd > len || chunkEnd < chunkStart) // oltre il buffer, o overflow
			return false;

		if (!chunkCompressed)
		{
			// Chunk non compresso: sempre esattamente 4096 byte grezzi
			// (mai una lunghezza diversa, vedi il commento nell'header
			// -- e' l'unica forma rappresentabile per un chunk vicino
			// o pari a 4096 byte quando la codifica compressa
			// sforerebbe il campo a 12 bit della dimensione).
			if (pos + kRawChunkDataSize > len || pos + kRawChunkDataSize > chunkEnd)
				return false;
			out->insert(out->end(), data + pos, data + pos + kRawChunkDataSize);
			pos = chunkEnd;
			continue;
		}

		size_t decompressedChunkStart = out->size();
		while (pos < chunkEnd)
		{
			uint8 flagByte = data[pos++];
			for (int bit = 0; bit < 8 && pos < chunkEnd; bit++)
			{
				if (((flagByte >> bit) & 1) == 0)
				{
					// Token letterale: un byte, copiato cosi' com'e'.
					out->push_back(data[pos++]);
					continue;
				}

				// Token di copia: due byte, decodificati con una
				// larghezza di bit che dipende da quanto e' gia' stato
				// decompresso dentro QUESTO chunk finora.
				if (pos + 2 > chunkEnd)
					return false;
				uint16 token = (uint16)data[pos] | ((uint16)data[pos + 1] << 8);
				pos += 2;

				uint32 decompressedCurrent = (uint32)(out->size() - decompressedChunkStart);
				int bitCount = BitCountFor(decompressedCurrent);
				uint16 lengthMask = (uint16)(0xFFFF >> bitCount);
				uint16 offsetMask = (uint16)(~lengthMask);
				uint32 length = (uint32)(token & lengthMask) + 3;
				uint32 offset = (uint32)((token & offsetMask) >> (16 - bitCount)) + 1;

				// Non deve mai puntare prima dell'inizio di QUESTO
				// chunk (un chunk compresso non puo' mai riferirsi al
				// contenuto del chunk precedente, vedi il commento in
				// cima sul reset per-chunk) -- un token cosi' esiste
				// solo in un flusso corrotto o scritto a mano male,
				// mai in un vero output di un compressore reale.
				if (offset > decompressedCurrent)
					return false;

				size_t copySource = out->size() - offset;
				for (uint32 i = 0; i < length; i++)
				{
					// Copia prima in una variabile locale, MAI
					// out->push_back((*out)[copySource + i]) diretto:
					// push_back puo' riallocare il buffer di "out"
					// PRIMA di leggere l'argomento per riferimento,
					// lasciando un riferimento pendente -- un
					// comportamento indefinito reale del C++, non solo
					// teorico, per una copia auto-sovrapposta come
					// questa (offset=1 per una lunga sequenza dello
					// stesso byte e' il caso piu' comune che la
					// scatenerebbe).
					uint8 b = (*out)[copySource + i];
					out->push_back(b);
				}
			}
		}
		pos = chunkEnd;
	}
	return true;
}
