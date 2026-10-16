/*
	VbaEnvironment.cpp

	Vedi VbaEnvironment.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaEnvironment.h"

#include <cctype>
#include <fstream>
#include <iterator>
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

bool VbaEnvironment::GetArrayDims(const std::string& name, std::vector<int>* outDims) const
{
	std::map<std::string, VbaArrayStorage>::const_iterator it = fArrays.find(NormalizeName(name));
	if (it == fArrays.end())
		return fModuleEnv ? fModuleEnv->GetArrayDims(name, outDims) : false;
	*outDims = it->second.dims;
	return true;
}

bool VbaEnvironment::PreserveResizeArray(const std::string& name, const std::vector<int>& newDims,
	std::string* outError)
{
	std::map<std::string, VbaArrayStorage>::iterator it = fArrays.find(NormalizeName(name));
	if (it == fArrays.end())
	{
		// Stesso principio di GetArrayDims/SetArrayElement sopra: se
		// non e' qui, prova il modulo -- il CHIAMANTE ha gia' verificato
		// IsArray() prima di arrivare qui, quindi deve esistere da
		// qualche parte in questa catena.
		return fModuleEnv && fModuleEnv->PreserveResizeArray(name, newDims, outError);
	}

	const std::vector<int>& oldDims = it->second.dims;
	if (oldDims.size() != newDims.size())
	{
		*outError = "\"ReDim Preserve\" non puo' cambiare il numero di dimensioni ("
			+ std::to_string(oldDims.size()) + " prima, " + std::to_string(newDims.size()) + " ora)";
		return false;
	}
	for (size_t i = 0; i + 1 < oldDims.size(); i++)
	{
		if (oldDims[i] != newDims[i])
		{
			*outError = "\"ReDim Preserve\" puo' modificare solo l'ULTIMA dimensione "
				"(la " + std::to_string(i + 1) + "a e' cambiata)";
			return false;
		}
	}

	// Tutte le dimensioni tranne l'ultima sono IDENTICHE (appena
	// verificato sopra): il "blocco" di elementi per ogni combinazione
	// delle dimensioni precedenti resta nella stessa posizione relativa
	// sia nel vecchio sia nel nuovo storage, solo la sua LUNGHEZZA
	// (l'ultima dimensione) cambia -- copiare blocco per blocco, fino al
	// minimo fra vecchia/nuova lunghezza, riproduce esattamente il vero
	// comportamento di "ReDim Preserve" (troncato se si restringe,
	// nuove celle a 0.0/Empty se si allarga, mai un accesso fuori dai
	// limiti di nessuno dei due vettori).
	size_t prefixCount = 1;
	for (size_t i = 0; i + 1 < oldDims.size(); i++)
		prefixCount *= (size_t)oldDims[i];
	size_t oldLast = oldDims.empty() ? 0 : (size_t)oldDims.back();
	size_t newLast = newDims.empty() ? 0 : (size_t)newDims.back();
	size_t copyLast = oldLast < newLast ? oldLast : newLast;

	std::vector<Value> newElements(prefixCount * newLast, Value(0.0));
	for (size_t p = 0; p < prefixCount; p++)
	{
		for (size_t j = 0; j < copyLast; j++)
			newElements[p * newLast + j] = it->second.elements[p * oldLast + j];
	}
	it->second.dims = newDims;
	it->second.elements = newElements;
	return true;
}

bool VbaEnvironment::OpenBinaryFileForReading(int fileNumber, const std::string& path,
	std::string* outError)
{
	if (fOpenFiles.find(fileNumber) != fOpenFiles.end())
	{
		*outError = "il numero di file #" + std::to_string(fileNumber) + " e' gia' aperto";
		return false;
	}
	std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
	if (!in)
	{
		*outError = "impossibile aprire il file \"" + path + "\"";
		return false;
	}
	VbaOpenFile file;
	file.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	fOpenFiles[fileNumber] = file;
	return true;
}

const VbaOpenFile* VbaEnvironment::FindOpenFile(int fileNumber) const
{
	std::map<int, VbaOpenFile>::const_iterator it = fOpenFiles.find(fileNumber);
	return it == fOpenFiles.end() ? NULL : &it->second;
}

VbaOpenFile* VbaEnvironment::FindOpenFile(int fileNumber)
{
	std::map<int, VbaOpenFile>::iterator it = fOpenFiles.find(fileNumber);
	return it == fOpenFiles.end() ? NULL : &it->second;
}

int VbaEnvironment::AllocateFreeFileNumber() const
{
	int n = 1;
	while (fOpenFiles.find(n) != fOpenFiles.end())
		n++;
	return n;
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
