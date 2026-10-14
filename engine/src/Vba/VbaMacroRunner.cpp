/*
	VbaMacroRunner.cpp

	Vedi VbaMacroRunner.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaMacroRunner.h"

#include <cctype>

#include "VbaExecutor.h"
#include "VbaModuleSource.h"

namespace {

std::string NormalizeSubName(const std::string& s)
{
	std::string out = s;
	for (size_t i = 0; i < out.size(); i++)
		out[i] = (char)tolower((unsigned char)out[i]);
	return out;
}

} // namespace

bool FindVbaSubByName(const std::vector<VbaToken>& tokens, const std::string& subName,
	std::unique_ptr<VbaSub>* outSub, VbaParseError* outError)
{
	std::string normalizedTarget = NormalizeSubName(subName);
	size_t pos = 0;
	while (pos < tokens.size())
	{
		while (pos < tokens.size() && tokens[pos].type == vbaTokNewline)
			pos++;
		if (pos >= tokens.size() || tokens[pos].type == vbaTokEnd)
			break;

		if (!(tokens[pos].type == vbaTokKeyword && tokens[pos].text == "Sub"))
		{
			// Contenuto a livello di modulo che questo sottoinsieme non
			// riconosce come l'inizio di una Sub (es. "Attribute
			// VB_Name = ...", "Option Explicit", una Function) -- si
			// salta UN token e si riprova, cosi' una riga che questo
			// sottoinsieme non capisce non blocca la ricerca delle Sub
			// successive.
			pos++;
			continue;
		}

		std::unique_ptr<VbaSub> sub = ParseVbaSub(tokens, pos, outError);
		if (!sub)
			return false; // vedi il LIMITE NOTO in VbaMacroRunner.h

		if (NormalizeSubName(sub->name) == normalizedTarget)
		{
			*outSub = std::move(sub);
			return true;
		}
		// Non e' quella giusta: "pos" e' gia' avanzato oltre "End Sub"
		// da ParseVbaSub, si continua con la prossima.
	}

	outError->message = "Sub \"" + subName + "\" non trovata in questo modulo";
	outError->line = 0;
	return false;
}

bool ListVbaMacroNames(const uint8* vbaProjectData, size_t vbaProjectLen,
	std::vector<std::string>* outNames)
{
	outNames->clear();

	std::vector<VbaModuleSource> modules;
	if (!ExtractVbaModuleSources(vbaProjectData, vbaProjectLen, &modules))
		return false;

	for (size_t m = 0; m < modules.size(); m++)
	{
		std::vector<VbaToken> tokens;
		TokenizeVba(modules[m].sourceText.c_str(), &tokens);

		for (size_t i = 0; i + 1 < tokens.size(); i++)
		{
			if (tokens[i].type != vbaTokKeyword || tokens[i].text != "Sub")
				continue;
			if (tokens[i + 1].type != vbaTokIdentifier)
				continue; // "End Sub": "End" e "Sub" sono due parole chiave, mai una Sub vera qui

			bool isPrivate = i > 0 && tokens[i - 1].type == vbaTokIdentifier
				&& NormalizeSubName(tokens[i - 1].text) == "private";
			if (!isPrivate)
				outNames->push_back(tokens[i + 1].text);
		}
	}
	return true;
}

bool ExecuteVbaMacro(const uint8* vbaProjectData, size_t vbaProjectLen,
	const std::string& macroName, CContainer* doc, VbaRuntimeError* outError)
{
	std::vector<VbaModuleSource> modules;
	if (!ExtractVbaModuleSources(vbaProjectData, vbaProjectLen, &modules))
	{
		outError->message = "vbaProject.bin non valido o illeggibile";
		outError->line = 0;
		return false;
	}

	for (size_t i = 0; i < modules.size(); i++)
	{
		std::vector<VbaToken> tokens;
		TokenizeVba(modules[i].sourceText.c_str(), &tokens);

		std::unique_ptr<VbaSub> sub;
		VbaParseError parseError;
		if (!FindVbaSubByName(tokens, macroName, &sub, &parseError))
			continue; // non in QUESTO modulo (o un errore di sintassi al suo interno):
			          // si prova il modulo successivo, vedi il LIMITE NOTO in VbaMacroRunner.h

		VbaEnvironment env;
		env.SetActiveDocument(doc);
		// Fase 6a: collega i token di QUESTO modulo all'ambiente, cosi'
		// EvalCall (VbaEvaluator.cpp) puo' risolvere una chiamata a una
		// Sub sorella con FindVbaSubByName invece di fermarsi al solo
		// elenco di funzioni predefinite -- "tokens" resta valido per
		// tutta la durata di questa chiamata sincrona, anche attraverso
		// chiamate a Sub annidate.
		env.SetModuleTokens(&tokens);
		return ExecuteVbaSub(sub.get(), &env, outError);
	}

	outError->message = "macro \"" + macroName + "\" non trovata in nessun modulo di questo progetto VBA";
	outError->line = 0;
	return false;
}
