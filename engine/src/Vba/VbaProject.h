/*
	VbaProject.h

	Entry point into a VBA project's raw container: xl/vbaProject.bin
	(preserved as an opaque blob since v0.2.7, see
	translators/xlsx/XlsxTranslator.cpp, never parsed until now) is
	itself an OLE2/Compound File Binary container per [MS-OVBA] 2.2 --
	the exact same format already read by engine/src/FileSys/
	CompoundFileReader for Agile Encryption's EncryptionInfo/
	EncryptedPackage streams. This file does not duplicate that reader;
	it just identifies a VBA project (signature plus structure check,
	same spirit as IsASCDFile/IsCompoundFile elsewhere in this
	codebase) and exposes named-stream access through it.

	Still just container access: no MS-OVBA stream decompression, no
	module enumeration, no VBA source text yet -- see VbaDecompress.h
	and VbaDirStream.h for those, built on top of this.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef VBA_PROJECT_H
#define VBA_PROJECT_H

#include <vector>

#include <SupportDefs.h>

// True if "data" parses as a valid OLE2/CFB container AND contains the
// "PROJECT" stream every real VBA project has ([MS-OVBA] 2.3.4.1) --
// a plain CompoundFileReader::Open() success alone isn't enough, since
// an XLSX's password-encryption CFB container (EncryptionInfo/
// EncryptedPackage) would also pass that check but is a completely
// different kind of file.
bool IsVbaProject(const uint8* data, size_t len);

// Reads one named stream's raw bytes out of a vbaProject.bin blob --
// still MS-OVBA-compressed where applicable (e.g. "dir", and any
// module stream past its PerformanceCache, see VbaDecompress.h). Thin
// pass-through to CompoundFileReader; nothing VBA-specific happens at
// this layer. False if "data" isn't a valid VBA project, or no stream
// with this exact name exists in it.
bool ReadVbaStream(const uint8* data, size_t len, const char* streamName,
	std::vector<uint8>* outBytes);

#endif
