/*
	VbaDecompress.h

	Decodifica il formato di compressione [MS-OVBA] 2.4.1 ("RLE
	Compression", in realta' un LZ77 con una finestra/larghezza di bit
	che dipende dalla posizione gia' decompressa dentro al chunk
	corrente -- niente a che vedere col vero RLE nonostante il nome
	usato dallo stesso spec Microsoft). Usato da OGNI stream dentro
	vbaProject.bin che contiene davvero codice/dati del progetto: lo
	stream "dir" per intero, e ogni modulo oltre al suo breve prefisso
	non compresso ("PerformanceCache", vedi VbaDirStream.h -- fuori
	scopo qui, questo file decodifica solo un singolo
	CompressedContainer gia' isolato dal chiamante).

	Algoritmo validato per auto-coerenza (compressore + decompressore
	scritti insieme, provati in giro completo su oltre una dozzina di
	casi sintetici inclusi i confini critici della larghezza di bit a
	posizione 16/17/32/33/64/65/256/257 e il reset del contesto a ogni
	cambio di chunk) PRIMA di questa porting in C++ -- vedi il
	prototipo Python usato per questo sviluppo. Nessun campione
	compresso VERO prodotto da Microsoft Excel e' stato disponibile in
	questo ambiente per una convalida indipendente: limite noto,
	segnalato anche in ROADMAP.md, da chiudere non appena un file reale
	con macro sara' reperibile.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_DECOMPRESS_H
#define VBA_DECOMPRESS_H

#include <vector>

#include <SupportDefs.h>

// Decodifica un intero CompressedContainer [MS-OVBA] 2.4.1.1 (un byte
// di firma 0x01 seguito da uno o piu' chunk, ciascuno compresso con
// token letterali/copia o scritto crudo per 4096 byte) in "out". Falso
// se il byte di firma non e' 0x01, o se "data" e' troncato/malformato
// (un header di chunk che dichiara piu' byte di quanti ne restino, un
// token di copia che punta prima dell'inizio del chunk corrente, ecc.)
// -- mai una lettura oltre la fine di "data", anche su input corrotto.
bool DecompressVba(const uint8* data, size_t len, std::vector<uint8>* out);

#endif
