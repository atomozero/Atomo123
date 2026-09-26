/*
	ExcelPasswordHash.cpp

	Vedi ExcelPasswordHash.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ExcelPasswordHash.h"

#include <cstring>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include "utf-support.h"

void GenerateExcelPasswordSalt(uint8 outSalt[16])
{
	RAND_bytes(outSalt, 16);
}

// UTF-8 (quello che arriva da un BTextControl) -> UTF-16LE (quello che
// l'algoritmo ECMA-376 vuole per la password): un codepoint per volta via
// mcharlen/municode (gia' in questo motore, vedi utf-support.h), con
// coppie di surrogati per i rarissimi codepoint oltre il BMP -- una
// password reale non ne ha quasi mai, ma l'algoritmo va implementato per
// intero, non solo per il caso comune.
static std::string Utf8ToUtf16LE(const std::string& utf8)
{
	std::string out;
	const char* p = utf8.c_str();
	while (*p)
	{
		int len = mcharlen(p);
		if (len <= 0)
			break;
		int codepoint = municode(p);
		p += len;

		if (codepoint <= 0xFFFF)
		{
			out += (char)(codepoint & 0xFF);
			out += (char)((codepoint >> 8) & 0xFF);
		}
		else
		{
			int v = codepoint - 0x10000;
			int hi = 0xD800 + (v >> 10);
			int lo = 0xDC00 + (v & 0x3FF);
			out += (char)(hi & 0xFF);
			out += (char)((hi >> 8) & 0xFF);
			out += (char)(lo & 0xFF);
			out += (char)((lo >> 8) & 0xFF);
		}
	}
	return out;
}

static std::string Sha512(const uint8* data, size_t len)
{
	uint8 digest[EVP_MAX_MD_SIZE];
	unsigned int digestLen = 0;
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	EVP_DigestInit_ex(ctx, EVP_sha512(), NULL);
	EVP_DigestUpdate(ctx, data, len);
	EVP_DigestFinal_ex(ctx, digest, &digestLen);
	EVP_MD_CTX_free(ctx);
	return std::string((const char*)digest, digestLen);
}

static std::string Sha512(const std::string& data)
{
	return Sha512((const uint8*)data.data(), data.size());
}

// Algoritmo ECMA-376 18.3.1.85 (vedi il commento in ExcelPasswordHash.h):
// H0 = SHA512(salt || password UTF-16LE), poi spinCount iterazioni di
// Hi = SHA512(H(i-1) || i a 4 byte little-endian). Restituisce l'hash
// finale GREZZO (64 byte), non ancora in base64 -- OfficeCrypto.h
// (Agile Encryption, stessa forma di iterazione con costanti diverse)
// riusa questa funzione a parte per non duplicare il ciclo.
std::string IteratedSha512(const std::string& salt, const std::string& password,
	int32 spinCount)
{
	std::string h = Sha512(salt + Utf8ToUtf16LE(password));
	for (int32 i = 0; i < spinCount; i++)
	{
		uint8 iterBytes[4] = {
			(uint8)(i & 0xFF), (uint8)((i >> 8) & 0xFF),
			(uint8)((i >> 16) & 0xFF), (uint8)((i >> 24) & 0xFF)
		};
		h = Sha512(h + std::string((const char*)iterBytes, 4));
	}
	return h;
}

std::string ComputeModernPasswordHash(const std::string& password,
	const std::string& saltBase64, int32 spinCount)
{
	std::string salt;
	if (!Base64Decode(saltBase64, &salt))
		return std::string();
	return Base64Encode((const uint8*)IteratedSha512(salt, password, spinCount).data(), 64);
}

bool VerifyModernPasswordHash(const std::string& password,
	const std::string& saltBase64, int32 spinCount,
	const std::string& expectedHashBase64)
{
	return ComputeModernPasswordHash(password, saltBase64, spinCount) == expectedHashBase64;
}

static const char kBase64Chars[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const uint8* data, size_t len)
{
	std::string out;
	out.reserve(((len + 2) / 3) * 4);
	size_t i = 0;
	while (i + 3 <= len)
	{
		uint32 n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
		out += kBase64Chars[(n >> 18) & 0x3F];
		out += kBase64Chars[(n >> 12) & 0x3F];
		out += kBase64Chars[(n >> 6) & 0x3F];
		out += kBase64Chars[n & 0x3F];
		i += 3;
	}
	size_t rem = len - i;
	if (rem == 1)
	{
		uint32 n = data[i] << 16;
		out += kBase64Chars[(n >> 18) & 0x3F];
		out += kBase64Chars[(n >> 12) & 0x3F];
		out += "==";
	}
	else if (rem == 2)
	{
		uint32 n = (data[i] << 16) | (data[i + 1] << 8);
		out += kBase64Chars[(n >> 18) & 0x3F];
		out += kBase64Chars[(n >> 12) & 0x3F];
		out += kBase64Chars[(n >> 6) & 0x3F];
		out += "=";
	}
	return out;
}

bool Base64Decode(const std::string& in, std::string* out)
{
	int8 table[256];
	memset(table, -1, sizeof(table));
	for (int i = 0; i < 64; i++)
		table[(unsigned char)kBase64Chars[i]] = (int8)i;

	std::string result;
	result.reserve((in.size() / 4) * 3);
	int32 buffer = 0, bits = 0;
	for (size_t i = 0; i < in.size(); i++)
	{
		char c = in[i];
		if (c == '=' || c == '\n' || c == '\r')
			continue;
		int8 v = table[(unsigned char)c];
		if (v < 0)
			return false;
		buffer = (buffer << 6) | v;
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			result += (char)((buffer >> bits) & 0xFF);
		}
	}
	*out = result;
	return true;
}
