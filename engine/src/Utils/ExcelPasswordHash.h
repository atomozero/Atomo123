/*
	ExcelPasswordHash.h

	Hash di protezione VERO di Excel (ECMA-376 18.3.1.85, forma moderna:
	algorithmName="SHA-512" hashValue/saltValue/spinCount) -- fino ad ora
	questo motore sapeva solo PRESERVARE questi campi quando importati da
	un vero file XLSX (vedi ConditionalFormatRule... no, vedi
	AscdSheetProtection in ui/src/AscdIO.h), mai calcolarli o verificarli
	per davvero. Condiviso fra ui/ e translators/xlsx/ (entrambi gia'
	linkano libengine.a, a differenza del formato ASCD che i translator
	duplicano apposta per non dipendere da ui/src/) -- prima vera
	dipendenza da una libreria di crittografia in questo progetto
	(OpenSSL, confermata installata e collegabile su questo sistema).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef EXCEL_PASSWORD_HASH_H
#define EXCEL_PASSWORD_HASH_H

#include <string>

#include <SupportDefs.h>

// Genera 16 byte di sale VERAMENTE casuali (OpenSSL RAND_bytes, un
// generatore crittografico, non rand()/mrand48() da soli) -- una nuova
// password impostata da questo programma ha sempre un sale nuovo, mai
// riusato fra regole/fogli diversi.
void GenerateExcelPasswordSalt(uint8 outSalt[16]);

// Algoritmo ECMA-376 18.3.1.85 (identico anche alla derivazione chiave
// di Agile Encryption, vedi OfficeCrypto.h -- stessa forma "hash
// iterato", riusata li' con costanti diverse):
//   H0 = SHA512(salt || password in UTF-16LE)
//   Hi = SHA512(H(i-1) || i come 4 byte little-endian), per i = 0..spinCount-1
// Restituisce l'hash finale (64 byte) codificato in base64, esattamente
// come compare nell'attributo hashValue="..." di un vero <sheetProtection>.
// "salt" e' gia' in base64 (stessa forma di saltValue="..."), non i byte
// grezzi -- decodificato internamente.
std::string ComputeModernPasswordHash(const std::string& password,
	const std::string& saltBase64, int32 spinCount);

// Ricalcola e confronta -- vero se "password" e' quella che ha generato
// "expectedHashBase64" con lo stesso sale/spinCount.
bool VerifyModernPasswordHash(const std::string& password,
	const std::string& saltBase64, int32 spinCount,
	const std::string& expectedHashBase64);

// Il nucleo dell'algoritmo (H0 poi spinCount iterazioni), esposto a parte
// perche' Agile Encryption (OfficeCrypto.h, Fase 2) usa la STESSA identica
// forma di iterazione per derivare le proprie chiavi, solo con costanti
// "block key" diverse mescolate dopo -- "salt" qui e' gia' decodificato
// (byte grezzi, non base64), a differenza di ComputeModernPasswordHash
// sopra. Restituisce l'hash finale GREZZO (64 byte), non in base64.
std::string IteratedSha512(const std::string& salt, const std::string& password,
	int32 spinCount);

// SHA-512 puro su byte grezzi, SENZA la conversione UTF-8->UTF-16LE che
// IteratedSha512 applica al suo secondo argomento (li' corretto, visto
// che quell'argomento e' sempre una password testuale vera) -- Agile
// Encryption (OfficeCrypto.h) lo usa per derivare le tre chiavi di
// blocco e gli IV per segmento da dati che sono GIA' byte grezzi
// (Hfinal concatenato a una costante, o un sale concatenato a un
// contatore), non testo: passarli per sbaglio attraverso la conversione
// UTF-8 li corromperebbe silenziosamente (byte non validi come UTF-8
// interpretati comunque da mcharlen/municode).
std::string Sha512Raw(const std::string& data);

// spinCount di default per una password generata da QUESTO programma
// (non quando se ne importa una gia' scritta da un vero Excel, che porta
// gia' il proprio spinCount) -- lo stesso valore che Excel stesso usa di
// default.
const int32 kDefaultPasswordSpinCount = 100000;

// Base64 (RFC 4648, senza a capo): serve qui per hashValue/saltValue, e
// per gli stessi attributi dentro EncryptionInfo in OfficeCrypto.h --
// nessuna libreria di base64 gia' presente in questo progetto (verificato
// in translators/xlsx/MiniZip.cpp: solo compressione/CRC32, niente
// codifica testuale), quindi una coppia piccola e autonoma invece di
// un'altra dipendenza esterna.
std::string Base64Encode(const uint8* data, size_t len);
// Vero solo se "in" e' base64 valido; "out" resta invariato altrimenti.
bool Base64Decode(const std::string& in, std::string* out);

#endif
