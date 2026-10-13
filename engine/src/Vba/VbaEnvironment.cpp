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
