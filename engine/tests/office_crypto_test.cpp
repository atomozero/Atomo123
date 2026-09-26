/*
	office_crypto_test.cpp

	Verifica DecryptAgileEncryptedXlsx (MS-OFFCRYPTO Agile Encryption,
	Path to full Excel parity -- gestione completa delle password, la
	parte separata dall'hash di protezione foglio) contro un vero file
	di prova cifrato: translators/xlsx/tests/sample_encrypted.xlsx,
	generato da un'implementazione Python indipendente (pycryptodome,
	non questo codice) con password nota "test123" -- vedi il commento
	in cima a OfficeCrypto.h per il resto del contesto.

	L'hash SHA-512 atteso qui sotto e' stato calcolato UNA VOLTA sul vero
	pacchetto ZIP recuperato con la password corretta (confermato allora,
	fuori da questo test, contenere davvero A1="Segreto"/B1=42 tramite
	python3.10 -c "import zipfile; ..."), poi congelato qui come controllo
	di regressione -- non serve un vero decompressore ZIP/DEFLATE dentro
	questo test solo per verificare che l'algoritmo di decifratura
	continui a produrre esattamente gli stessi byte.
*/

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include "CompoundFileReader.h"
#include "ExcelPasswordHash.h"
#include "OfficeCrypto.h"

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
	std::ifstream f("../translators/xlsx/tests/sample_encrypted.xlsx", std::ios::binary);
	Check((bool)f, "il file di prova cifrato si apre");
	if (!f)
	{
		printf("\nALCUNI TEST SONO FALLITI\n");
		return 1;
	}
	std::vector<uint8> fileData((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	Check(fileData.size() == 5120, "il file di prova ha la dimensione attesa (5120 byte)");

	Check(IsCompoundFile(&fileData[0], fileData.size()),
		"il file di prova ha davvero la firma OLE2/CFB (non e' uno ZIP semplice)");

	std::vector<uint8> plain;
	bool ok = DecryptAgileEncryptedXlsx(&fileData[0], fileData.size(), "test123", &plain);
	Check(ok, "DecryptAgileEncryptedXlsx accetta la password corretta");
	Check(plain.size() == 1581, "il pacchetto ZIP recuperato ha la dimensione reale attesa (1581 byte)");
	Check(plain.size() >= 4 && memcmp(&plain[0], "PK\x03\x04", 4) == 0,
		"il pacchetto recuperato inizia con una vera firma ZIP (PK\\x03\\x04)");

	const std::string kExpectedPlainHash =
		"7pTIjEpPlFJXQtoTsodwaIRmkl/o598O/K108yPAqqNgXezWyjsE1XunJ0vLtq8NUCA61RS19eNxKySxtk4MJQ==";
	std::string plainHash = Base64Encode(
		(const uint8*)Sha512Raw(std::string((const char*)&plain[0], plain.size())).data(), 64);
	Check(plainHash == kExpectedPlainHash,
		"il pacchetto ZIP recuperato combacia byte per byte col riscontro congelato "
		"(contiene davvero A1=\"Segreto\"/B1=42, verificato una volta con python3.10+zipfile)");

	std::vector<uint8> plainWrong;
	bool okWrong = DecryptAgileEncryptedXlsx(&fileData[0], fileData.size(), "password sbagliata", &plainWrong);
	Check(!okWrong, "DecryptAgileEncryptedXlsx rifiuta una password sbagliata senza crashare");
	Check(plainWrong.empty(), "nessun byte parziale/spazzatura restituito per una password sbagliata");

	std::vector<uint8> notAFile;
	notAFile.push_back('P'); notAFile.push_back('K');
	notAFile.push_back(3); notAFile.push_back(4);
	Check(!IsCompoundFile(&notAFile[0], notAFile.size()),
		"un vero file ZIP (non cifrato) non viene scambiato per un contenitore OLE2/CFB");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
