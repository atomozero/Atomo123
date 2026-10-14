/*
	VbaEnvironment.cpp

	Vedi VbaEnvironment.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaEnvironment.h"

#include <cctype>
#include <utility>

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
	std::string key = NormalizeName(name);
	std::map<std::string, Value>::const_iterator it = fVars.find(key);
	if (it != fVars.end())
		return &it->second;
	// Stato di modulo persistente: ricade sul modulo SOLO se "name"
	// non esiste affatto qui a livello locale -- un "Dim" locale (o
	// una prima assegnazione locale) nasconde sempre una variabile di
	// modulo omonima per tutta la chiamata, come nel vero VBA.
	return fModuleEnv ? fModuleEnv->Find(name) : NULL;
}

void VbaEnvironment::Set(const std::string& name, const Value& value)
{
	std::string key = NormalizeName(name);
	if (fVars.find(key) == fVars.end() && fModuleEnv
		&& fModuleEnv->fVars.find(key) != fModuleEnv->fVars.end())
	{
		// Esiste SOLO a livello di modulo (nessun "Dim" locale
		// l'ha mai oscurata in questa chiamata): scrive LI', non in
		// una nuova variabile locale -- "x = 5" su una variabile di
		// modulo la modifica davvero, visibile anche dalle altre
		// Sub, esattamente come nel vero VBA.
		fModuleEnv->fVars[key] = value;
		return;
	}
	fVars[key] = value; // esiste gia' localmente, o non esiste in nessuno dei due: locale (nuova se serve)
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
	if (fArrays.find(NormalizeName(name)) != fArrays.end())
		return true;
	// Un array si dichiara SEMPRE con "Dim" (mai implicitamente, a
	// differenza di una variabile scalare): se non e' qui, non c'e'
	// alcuna ambiguita' locale/modulo da risolvere, si delega di
	// peso al modulo.
	return fModuleEnv && fModuleEnv->IsArray(name);
}

const Value* VbaEnvironment::GetArrayElement(const std::string& name,
	const std::vector<int>& indices) const
{
	std::map<std::string, VbaArrayStorage>::const_iterator it = fArrays.find(NormalizeName(name));
	if (it == fArrays.end())
		return fModuleEnv ? fModuleEnv->GetArrayElement(name, indices) : NULL;
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
		return fModuleEnv ? fModuleEnv->SetArrayElement(name, indices, value) : false;
	size_t flat;
	if (!ComputeFlatArrayIndex(it->second.dims, indices, &flat))
		return false;
	it->second.elements[flat] = value;
	return true;
}

VbaStructField* VbaStructInstance::FindField(const std::string& name)
{
	std::string key = name;
	for (size_t i = 0; i < key.size(); i++)
		key[i] = (char)tolower((unsigned char)key[i]);
	std::map<std::string, VbaStructField>::iterator it = fields.find(key);
	return it == fields.end() ? NULL : &it->second;
}

void VbaEnvironment::DeclareStruct(const std::string& name, VbaStructInstance&& instance)
{
	fStructs[NormalizeName(name)] = std::move(instance);
}

bool VbaEnvironment::IsStruct(const std::string& name) const
{
	if (fStructs.find(NormalizeName(name)) != fStructs.end())
		return true;
	// Stessa ragione di IsArray sopra: una struct si dichiara SEMPRE
	// con "Dim", mai implicitamente -- nessuna ambiguita' locale/
	// modulo da risolvere, si delega di peso al modulo.
	return fModuleEnv && fModuleEnv->IsStruct(name);
}

VbaStructInstance* VbaEnvironment::GetStruct(const std::string& name)
{
	std::map<std::string, VbaStructInstance>::iterator it = fStructs.find(NormalizeName(name));
	if (it != fStructs.end())
		return &it->second;
	return fModuleEnv ? fModuleEnv->GetStruct(name) : NULL;
}

const VbaStructInstance* VbaEnvironment::GetStruct(const std::string& name) const
{
	std::map<std::string, VbaStructInstance>::const_iterator it = fStructs.find(NormalizeName(name));
	if (it != fStructs.end())
		return &it->second;
	return fModuleEnv ? fModuleEnv->GetStruct(name) : NULL;
}
