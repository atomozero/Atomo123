/*
	VbaProject.cpp

	Vedi VbaProject.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaProject.h"

#include "CompoundFileReader.h"

bool IsVbaProject(const uint8* data, size_t len)
{
	if (!IsCompoundFile(data, len))
		return false;

	CompoundFileReader reader;
	if (!reader.Open(data, len))
		return false;

	std::vector<uint8> unused;
	return reader.ReadStream("PROJECT", &unused);
}

bool ReadVbaStream(const uint8* data, size_t len, const char* streamName,
	std::vector<uint8>* outBytes)
{
	if (!IsVbaProject(data, len))
		return false;

	CompoundFileReader reader;
	if (!reader.Open(data, len))
		return false;

	return reader.ReadStream(streamName, outBytes);
}
