/*
	VbaEnvironment.cpp

	Vedi VbaEnvironment.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaEnvironment.h"

#include <cctype>

std::string VbaEnvironment::NormalizeName(const std::string& name)
{
	std::string out = name;
	for (size_t i = 0; i < out.size(); i++)
		out[i] = (char)tolower((unsigned char)out[i]);
	return out;
}

void VbaEnvironment::Declare(const std::string& name)
{
	std::string key = NormalizeName(name);
	if (fVars.find(key) == fVars.end())
		fVars[key] = Value(0.0);
}

const Value* VbaEnvironment::Find(const std::string& name) const
{
	std::map<std::string, Value>::const_iterator it = fVars.find(NormalizeName(name));
	return it == fVars.end() ? NULL : &it->second;
}

void VbaEnvironment::Set(const std::string& name, const Value& value)
{
	fVars[NormalizeName(name)] = value;
}

// Appiattisce "indices" (uno per asse) in un unico indice nel vettore
// "elements" di VbaArrayStorage, riga per riga (l'ultimo asse varia
// piu' velocemente) -- stessa idea di un array C multidimensionale.
// Falso se il numero di indici non combacia con "dims" o uno qualunque
// e' fuori dai limiti del proprio asse.
static bool ComputeFlatArrayIndex(const std::vector<int>& dims, const std::vector<int>& indices,
	size_t* outIndex)
{
	if (indices.size() != dims.size())
		return false;
	size_t flat = 0;
	for (size_t i = 0; i < dims.size(); i++)
	{
		if (indices[i] < 0 || indices[i] >= dims[i])
			return false;
		flat = flat * (size_t)dims[i] + (size_t)indices[i];
	}
	*outIndex = flat;
	return true;
}

void VbaEnvironment::DeclareArray(const std::string& name, const std::vector<int>& dims)
{
	size_t total = 1;
	for (size_t i = 0; i < dims.size(); i++)
		total *= (size_t)dims[i];
	VbaArrayStorage storage;
	storage.dims = dims;
	storage.elements.assign(total, Value(0.0));
	fArrays[NormalizeName(name)] = storage;
}

bool VbaEnvironment::IsArray(const std::string& name) const
{
	return fArrays.find(NormalizeName(name)) != fArrays.end();
}

const Value* VbaEnvironment::GetArrayElement(const std::string& name,
	const std::vector<int>& indices) const
{
	std::map<std::string, VbaArrayStorage>::const_iterator it = fArrays.find(NormalizeName(name));
	if (it == fArrays.end())
		return NULL;
	size_t flat;
	if (!ComputeFlatArrayIndex(it->second.dims, indices, &flat))
		return NULL;
	return &it->second.elements[flat];
}

bool VbaEnvironment::SetArrayElement(const std::string& name, const std::vector<int>& indices,
	const Value& value)
{
	std::map<std::string, VbaArrayStorage>::iterator it = fArrays.find(NormalizeName(name));
	if (it == fArrays.end())
		return false;
	size_t flat;
	if (!ComputeFlatArrayIndex(it->second.dims, indices, &flat))
		return false;
	it->second.elements[flat] = value;
	return true;
}
