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
		return ExecuteVbaSub(sub.get(), &env, outError);
	}

	outError->message = "macro \"" + macroName + "\" non trovata in nessun modulo di questo progetto VBA";
	outError->line = 0;
	return false;
}
