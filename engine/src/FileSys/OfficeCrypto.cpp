/*
	OfficeCrypto.cpp

	Vedi OfficeCrypto.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "OfficeCrypto.h"

#include <cstring>

#include <expat.h>
#include <openssl/evp.h>

#include "CompoundFileReader.h"
#include "ExcelPasswordHash.h"

// Le tre costanti "block key" di Agile Encryption (MS-OFFCRYPTO
// 2.3.4.11-13): byte fissi mescolati DOPO l'hash iterato per ottenere
// tre chiavi diverse dallo stesso "Hfinal" -- una per il verificatore
// (encryptedVerifierHashInput), una per il suo hash atteso
// (encryptedVerifierHashValue) e una per la chiave segreta vera
// (encryptedKeyValue). Valori dalla specifica pubblica, non inventati.
static const uint8 kBlockKeyVerifierHashInput[8] =
	{ 0xfe, 0xa7, 0xd2, 0x76, 0x3b, 0x4b, 0x9e, 0x79 };
static const uint8 kBlockKeyVerifierHashValue[8] =
	{ 0xd7, 0xaa, 0x0f, 0x6d, 0x30, 0x61, 0x34, 0x4e };
static const uint8 kBlockKeyEncryptedKeyValue[8] =
	{ 0x14, 0x6e, 0x0b, 0xe7, 0xab, 0xac, 0xd0, 0xd6 };

// Attributi reali di <keyData>/<keyEncryptor><p:encryptedKey>, catturati
// dal SAX minimale sotto -- solo quelli che questa implementazione usa
// per davvero, non un DOM completo (stesso principio "cattura solo cio'
// che serve" gia' seguito da ParseSheet in XlsxTranslator.cpp).
struct AgileEncryptionInfo {
	bool hasKeyData;
	bool hasKeyEncryptor;
	std::string keyDataSalt;      // base64
	int32 keyBits;
	int32 blockSize;
	std::string pwSalt;           // base64, del keyEncryptor (DIVERSO dal sale di keyData sopra)
	int32 spinCount;
	std::string encryptedVerifierHashInput; // base64
	std::string encryptedVerifierHashValue; // base64
	std::string encryptedKeyValue;          // base64

	AgileEncryptionInfo() : hasKeyData(false), hasKeyEncryptor(false),
		keyBits(0), blockSize(16), spinCount(0) {}
};

static const char* FindAttr(const char** atts, const char* name)
{
	for (int i = 0; atts[i]; i += 2)
		if (strcmp(atts[i], name) == 0)
			return atts[i + 1];
	return NULL;
}

// Il nome locale di un elemento/attributo XML puo' arrivare con un
// prefisso di namespace ("p:encryptedKey") a seconda di come expat e'
// configurato -- qui SENZA gestione dei namespace (XML_ParserCreate(NULL),
// stesso approccio gia' usato da ParseSheet in XlsxTranslator.cpp),
// quindi confrontare solo la parte dopo l'eventuale ":" evita di dover
// sapere in anticipo quale prefisso useranno gli elementi con namespace.
static const char* LocalName(const char* qualifiedName)
{
	const char* colon = strchr(qualifiedName, ':');
	return colon ? colon + 1 : qualifiedName;
}

static void XMLCALL EncryptionInfoStart(void* userData, const char* name, const char** atts)
{
	AgileEncryptionInfo* info = (AgileEncryptionInfo*)userData;
	const char* local = LocalName(name);

	if (strcmp(local, "keyData") == 0)
	{
		const char* salt = FindAttr(atts, "saltValue");
		const char* keyBits = FindAttr(atts, "keyBits");
		const char* blockSize = FindAttr(atts, "blockSize");
		if (salt) info->keyDataSalt = salt;
		if (keyBits) info->keyBits = atoi(keyBits);
		if (blockSize) info->blockSize = atoi(blockSize);
		info->hasKeyData = true;
	}
	else if (strcmp(local, "encryptedKey") == 0)
	{
		const char* salt = FindAttr(atts, "saltValue");
		const char* spinCount = FindAttr(atts, "spinCount");
		const char* verifierInput = FindAttr(atts, "encryptedVerifierHashInput");
		const char* verifierValue = FindAttr(atts, "encryptedVerifierHashValue");
		const char* keyValue = FindAttr(atts, "encryptedKeyValue");
		if (salt) info->pwSalt = salt;
		if (spinCount) info->spinCount = atoi(spinCount);
		if (verifierInput) info->encryptedVerifierHashInput = verifierInput;
		if (verifierValue) info->encryptedVerifierHashValue = verifierValue;
		if (keyValue) info->encryptedKeyValue = keyValue;
		info->hasKeyEncryptor = true;
	}
}

static bool ParseEncryptionInfoXml(const uint8* xml, size_t len, AgileEncryptionInfo* info)
{
	XML_Parser parser = XML_ParserCreate(NULL);
	XML_SetUserData(parser, info);
	XML_SetStartElementHandler(parser, EncryptionInfoStart);
	XML_Status status = XML_Parse(parser, (const char*)xml, (int)len, 1);
	XML_ParserFree(parser);
	return status == XML_STATUS_OK && info->hasKeyData && info->hasKeyEncryptor;
}

// AES-CBC senza padding (Agile cifra sempre blocchi gia' allineati a
// blockSize, riempiti a zero dal lato scrittura -- vedi il commento
// nella funzione principale sotto): EVP_DecryptUpdate/EncryptUpdate con
// EVP_CIPHER_CTX_set_padding(ctx, 0), un solo blocco alla volta o un
// buffer intero, "outLen" e' sempre uguale a "inLen" per questo motivo.
static bool AesCbcDecrypt(const uint8* key, size_t keyLen, const uint8* iv,
	const uint8* in, size_t inLen, std::vector<uint8>* out)
{
	if (inLen == 0 || (inLen % 16) != 0)
		return false;
	const EVP_CIPHER* cipher = (keyLen == 32) ? EVP_aes_256_cbc()
		: (keyLen == 16) ? EVP_aes_128_cbc() : NULL;
	if (!cipher)
		return false;

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv))
	{
		EVP_CIPHER_CTX_free(ctx);
		return false;
	}
	EVP_CIPHER_CTX_set_padding(ctx, 0);

	out->assign(inLen, 0);
	int outLen1 = 0, outLen2 = 0;
	bool ok = EVP_DecryptUpdate(ctx, &(*out)[0], &outLen1, in, (int)inLen)
		&& EVP_DecryptFinal_ex(ctx, &(*out)[0] + outLen1, &outLen2);
	EVP_CIPHER_CTX_free(ctx);
	if (!ok)
		return false;
	out->resize(outLen1 + outLen2);
	return true;
}

// Hash(Hfinal || blockKey), troncato a keyBits/8 byte -- la stessa
// funzione di derivazione usata per tutte e tre le chiavi di Agile
// Encryption (verificatore/hash atteso/chiave vera), solo con
// "blockKey" diverso. SHA-512 da' sempre 64 byte: per una chiave a 128
// o 256 bit si tronca, mai bisogno di espanderla (Agile non usa chiavi
// piu' lunghe di 512 bit in pratica).
static std::string DeriveAgileKey(const std::string& hFinal, const uint8 blockKey[8], int32 keyBits)
{
	std::string combined = hFinal + std::string((const char*)blockKey, 8);
	std::string fullHash = Sha512Raw(combined);
	return fullHash.substr(0, keyBits / 8);
}

bool DecryptAgileEncryptedXlsx(const uint8* fileData, size_t fileLen,
	const std::string& password, std::vector<uint8>* outPlainZip)
{
	if (!IsCompoundFile(fileData, fileLen))
		return false;

	CompoundFileReader reader;
	if (!reader.Open(fileData, fileLen))
		return false;

	std::vector<uint8> encryptionInfoBytes, encryptedPackageBytes;
	if (!reader.ReadStream("EncryptionInfo", &encryptionInfoBytes))
		return false;
	if (!reader.ReadStream("EncryptedPackage", &encryptedPackageBytes))
		return false;

	// Header fisso di 8 byte (MS-OFFCRYPTO 2.3.4.1): versione (4 byte,
	// 0x00040004 per Agile) + riservato (4 byte, 0x00000040) -- non
	// verificato byte per byte qui (un valore diverso vorrebbe dire
	// "Standard Encryption" o un formato ancora piu' vecchio, fuori
	// scope: si scarta comunque piu' sotto se l'XML non fa il suo
	// dovere, nessun bisogno di un controllo esplicito in piu').
	if (encryptionInfoBytes.size() < 8)
		return false;
	const uint8* xmlStart = &encryptionInfoBytes[8];
	size_t xmlLen = encryptionInfoBytes.size() - 8;

	AgileEncryptionInfo info;
	if (!ParseEncryptionInfoXml(xmlStart, xmlLen, &info))
		return false; // "Standard Encryption" (XML assente) o XML malformato

	std::string keyDataSalt, pwSalt;
	if (!Base64Decode(info.keyDataSalt, &keyDataSalt) || !Base64Decode(info.pwSalt, &pwSalt))
		return false;
	std::string encVerifierInput, encVerifierValue, encKeyValue;
	if (!Base64Decode(info.encryptedVerifierHashInput, &encVerifierInput)
		|| !Base64Decode(info.encryptedVerifierHashValue, &encVerifierValue)
		|| !Base64Decode(info.encryptedKeyValue, &encKeyValue))
		return false;
	if (info.keyBits != 128 && info.keyBits != 256)
		return false; // nessun altro valore reale in pratica

	// Passo 1: verifica la password PRIMA di fidarsi di qualunque cosa
	// derivata da lei (stesso principio del vero Excel: un errore di
	// password si vede subito, non a meta' di una decifratura del
	// pacchetto intero che poi risulta essere spazzatura).
	std::string hFinal = IteratedSha512(pwSalt, password, info.spinCount);

	std::string key1 = DeriveAgileKey(hFinal, kBlockKeyVerifierHashInput, info.keyBits);
	std::vector<uint8> verifierHashInput;
	if (!AesCbcDecrypt((const uint8*)key1.data(), key1.size(), (const uint8*)pwSalt.data(),
			(const uint8*)encVerifierInput.data(), encVerifierInput.size(), &verifierHashInput))
		return false;

	std::string key2 = DeriveAgileKey(hFinal, kBlockKeyVerifierHashValue, info.keyBits);
	std::vector<uint8> decryptedVerifierHashValue;
	if (!AesCbcDecrypt((const uint8*)key2.data(), key2.size(), (const uint8*)pwSalt.data(),
			(const uint8*)encVerifierValue.data(), encVerifierValue.size(), &decryptedVerifierHashValue))
		return false;

	std::string computedVerifierHash = Sha512Raw(
		std::string((const char*)&verifierHashInput[0], verifierHashInput.size()));
	if (decryptedVerifierHashValue.size() != computedVerifierHash.size()
		|| memcmp(&decryptedVerifierHashValue[0], computedVerifierHash.data(),
			computedVerifierHash.size()) != 0)
		return false; // password sbagliata

	// Passo 2: password confermata corretta -- recupera la vera chiave
	// segreta (quella che protegge EncryptedPackage, indipendente dalla
	// password: e' quello che permette a Excel di cambiare la password
	// di un file senza dover ricifrare l'intero pacchetto da capo).
	std::string key3 = DeriveAgileKey(hFinal, kBlockKeyEncryptedKeyValue, info.keyBits);
	std::vector<uint8> secretKey;
	if (!AesCbcDecrypt((const uint8*)key3.data(), key3.size(), (const uint8*)pwSalt.data(),
			(const uint8*)encKeyValue.data(), encKeyValue.size(), &secretKey))
		return false;

	// Passo 3: EncryptedPackage = 8 byte di dimensione VERA (il pacchetto
	// ZIP prima della cifratura, che aggiunge byte di riempimento fino al
	// prossimo multiplo di blockSize) seguiti dai segmenti cifrati veri e
	// propri, ognuno con il proprio IV derivato da (sale di keyData +
	// indice di segmento) -- MS-OFFCRYPTO 2.3.4.14/15, 4096 byte a
	// segmento sempre, indipendente da blockSize.
	if (encryptedPackageBytes.size() < 8)
		return false;
	uint64 realSize = 0;
	for (int i = 0; i < 8; i++)
		realSize |= (uint64)encryptedPackageBytes[i] << (8 * i);

	const size_t kSegmentSize = 4096;
	const uint8* cipherData = &encryptedPackageBytes[8];
	size_t cipherLen = encryptedPackageBytes.size() - 8;
	if (realSize > cipherLen)
		return false; // dimensione dichiarata assurda, file corrotto

	outPlainZip->clear();
	outPlainZip->reserve((size_t)realSize);
	uint32 segmentIndex = 0;
	for (size_t offset = 0; offset < cipherLen; offset += kSegmentSize, segmentIndex++)
	{
		size_t segLen = cipherLen - offset < kSegmentSize ? cipherLen - offset : kSegmentSize;
		if (segLen % 16 != 0)
			return false; // ogni segmento (tranne l'ultimo) e' sempre un multiplo di blockSize

		uint8 counter[4] = {
			(uint8)(segmentIndex & 0xFF), (uint8)((segmentIndex >> 8) & 0xFF),
			(uint8)((segmentIndex >> 16) & 0xFF), (uint8)((segmentIndex >> 24) & 0xFF)
		};
		std::string ivFull = Sha512Raw(keyDataSalt + std::string((const char*)counter, 4));
		std::vector<uint8> segment;
		if (!AesCbcDecrypt(&secretKey[0], secretKey.size(),
				(const uint8*)ivFull.data(), cipherData + offset, segLen, &segment))
			return false;
		outPlainZip->insert(outPlainZip->end(), segment.begin(), segment.end());
	}

	if (outPlainZip->size() < realSize)
		return false;
	outPlainZip->resize((size_t)realSize);
	return true;
}
