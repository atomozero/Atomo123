/*
	password_hash_test.cpp

	Verifica ExcelPasswordHash.cpp (ECMA-376 18.3.1.85, hash VERO di
	protezione foglio -- Path to full Excel parity, gestione completa
	delle password): il vettore di prova qui sotto e' stato ricalcolato
	in modo indipendente in Python (hashlib.sha512 iterato a mano, stessa
	identica forma dell'algoritmo, nessuna libreria di terze parti) per
	avere un vero riscontro esterno, non solo "questa funzione e'
	coerente con se stessa".
*/

#include <cstdio>
#include <cstring>

#include "ExcelPasswordHash.h"

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

int main()
{
	// Sale fisso (non casuale) per riproducibilita': i byte 0..15, cosi'
	// come scritto a mano anche nel riscontro Python -- vedi il commento
	// in cima al file.
	uint8 salt[16];
	for (int i = 0; i < 16; i++)
		salt[i] = (uint8)i;
	std::string saltBase64 = Base64Encode(salt, 16);
	Check(saltBase64 == "AAECAwQFBgcICQoLDA0ODw==",
		"Base64Encode del sale fisso combacia con l'atteso");

	std::string decodedSalt;
	Check(Base64Decode(saltBase64, &decodedSalt) && decodedSalt.size() == 16,
		"Base64Decode e' l'inverso esatto di Base64Encode (16 byte)");
	Check(memcmp(decodedSalt.data(), salt, 16) == 0,
		"il sale decodificato combacia byte per byte con l'originale");

	// Vettore di prova reale: password="test123", spinCount=100000,
	// stesso sale sopra -- hash atteso ricalcolato in modo indipendente
	// in Python:
	//   h = sha512(salt + "test123".encode("utf-16-le")).digest()
	//   for i in range(100000): h = sha512(h + struct.pack("<I", i)).digest()
	//   base64.b64encode(h)
	const std::string kExpectedHash =
		"u7OZ74Y5K0WWHCY95nyeRRecUuxH1rDCEoOoRFk+0cKC3hqqgRaxd0AYJioVLKaMTkQB8GdHehzWtfFtlQHEAQ==";

	std::string hash = ComputeModernPasswordHash("test123", saltBase64, 100000);
	Check(hash == kExpectedHash,
		"ComputeModernPasswordHash(\"test123\", spinCount=100000) combacia col riscontro Python indipendente");

	Check(VerifyModernPasswordHash("test123", saltBase64, 100000, kExpectedHash),
		"VerifyModernPasswordHash accetta la password corretta");
	Check(!VerifyModernPasswordHash("test124", saltBase64, 100000, kExpectedHash),
		"VerifyModernPasswordHash rifiuta una password quasi identica ma sbagliata");
	Check(!VerifyModernPasswordHash("test123", saltBase64, 99999, kExpectedHash),
		"VerifyModernPasswordHash rifiuta lo stesso hash con uno spinCount diverso");

	// Un sale diverso deve produrre un hash diverso per la STESSA
	// password -- altrimenti il sale non starebbe facendo il suo lavoro
	// (impedire che due password uguali producano lo stesso hash).
	uint8 otherSalt[16];
	for (int i = 0; i < 16; i++)
		otherSalt[i] = (uint8)(15 - i);
	std::string otherSaltBase64 = Base64Encode(otherSalt, 16);
	std::string otherHash = ComputeModernPasswordHash("test123", otherSaltBase64, 100000);
	Check(otherHash != hash,
		"la stessa password con un sale diverso produce un hash diverso");

	// Una password UTF-8 con caratteri fuori ASCII (l'algoritmo vuole
	// UTF-16LE, non un troncamento a un byte per carattere): non deve
	// ne' crashare ne' produrre un hash vuoto/degenere.
	// "\xc3\xa8" da solo (UTF-8 di "e" accentata), poi "123" concatenato
	// come letterale a parte: "\xa8123" nella stessa stringa verrebbe
	// letto come un'unica sequenza esadecimale piu' lunga (ogni cifra
	// 0-9/a-f dopo \x viene inglobata), un valore fuori range per un
	// solo byte -- lo stesso avviso del compilatore appena visto.
	std::string utf8Hash = ComputeModernPasswordHash("caff\xc3\xa8" "123", saltBase64, 1000);
	Check(!utf8Hash.empty() && utf8Hash != hash,
		"una password con caratteri accentati (UTF-8 multi-byte) produce un hash valido e diverso");

	uint8 saltA[16], saltB[16];
	GenerateExcelPasswordSalt(saltA);
	GenerateExcelPasswordSalt(saltB);
	Check(memcmp(saltA, saltB, 16) != 0,
		"GenerateExcelPasswordSalt produce sali diversi a ogni chiamata (non un valore fisso)");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
