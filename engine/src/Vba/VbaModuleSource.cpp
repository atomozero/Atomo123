/*
	VbaModuleSource.cpp

	Vedi VbaModuleSource.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaModuleSource.h"

#include "VbaProject.h"
#include "VbaDecompress.h"
#include "VbaDirStream.h"

bool ExtractVbaModuleSources(const uint8* data, size_t len,
	std::vector<VbaModuleSource>* outModules)
{
	outModules->clear();

	if (!IsVbaProject(data, len))
		return false;

	std::vector<uint8> dirCompressed;
	if (!ReadVbaStream(data, len, "dir", &dirCompressed))
		return false;

	std::vector<uint8> dirDecompressed;
	if (!DecompressVba(dirCompressed.empty() ? NULL : &dirCompressed[0], dirCompressed.size(),
			&dirDecompressed))
		return false;

	std::vector<VbaModuleInfo> moduleInfos;
	if (!ParseVbaDirStream(dirDecompressed.empty() ? NULL : &dirDecompressed[0],
			dirDecompressed.size(), &moduleInfos))
		return false;

	for (size_t i = 0; i < moduleInfos.size(); i++)
	{
		const VbaModuleInfo& info = moduleInfos[i];

		std::vector<uint8> streamBytes;
		if (!ReadVbaStream(data, len, info.streamName.c_str(), &streamBytes))
			return false;

		// I primi "textOffset" byte sono la PerformanceCache binaria
		// (p-code, mai MS-OVBA-compressa) -- si scarta senza
		// analizzarla, vedi il commento in cima all'header. Un offset
		// oltre la fine dello stream e' un file corrotto/malformato,
		// non un modulo senza sorgente (che avrebbe comunque un
		// CompressedContainer valido, anche se vuoto una volta
		// decompresso).
		if (info.textOffset > streamBytes.size())
			return false;

		const uint8* compressedSource = streamBytes.empty() ? NULL
			: &streamBytes[info.textOffset];
		size_t compressedSourceLen = streamBytes.size() - info.textOffset;

		std::vector<uint8> sourceBytes;
		if (!DecompressVba(compressedSource, compressedSourceLen, &sourceBytes))
			return false;

		VbaModuleSource module;
		module.name = info.name;
		module.isDocumentModule = info.isDocumentModule;
		module.sourceText.assign(sourceBytes.begin(), sourceBytes.end());
		outModules->push_back(module);
	}

	return true;
}
