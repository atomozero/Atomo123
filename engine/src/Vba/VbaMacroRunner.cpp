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

		if (!(tokens[pos].type == vbaTokKeyword
			&& (tokens[pos].text == "Sub" || tokens[pos].text == "Function")))
		{
			// Contenuto a livello di modulo che questo sottoinsieme non
			// riconosce come l'inizio di una Sub/Function (es.
			// "Attribute VB_Name = ...", "Option Explicit") -- si salta
			// UN token e si riprova, cosi' una riga che questo
			// sottoinsieme non capisce non blocca la ricerca delle
			// Sub/Function successive.
			pos++;
			continue;
		}

		std::string startKeyword = tokens[pos].text; // "Sub" o "Function", prima di avanzare
		size_t blockStart = pos;
		VbaParseError localError;
		std::unique_ptr<VbaSub> sub = ParseVbaSub(tokens, pos, &localError);
		if (!sub)
		{
			// Non si abortisce piu' l'INTERA scansione del modulo per
			// UNA Sub/Function con una sintassi fuori da questo
			// sottoinsieme (es. "Open ... For Binary", "ByRef" su un
			// parametro): si salta SOLO questo blocco, cercando "End
			// Sub"/"End Function" -- la stessa coppia di parole chiave
			// consecutive MAI prodotta da un If/For/Do annidato (che
			// userebbe "End If"/"Next"/"Loop"), stesso trucco di
			// ListVbaMacroNames -- e si riprende la ricerca con la
			// Sub/Function SUCCESSIVA. Si riparte da "blockStart + 1",
			// non da dove si e' fermato "ParseVbaSub" (che potrebbe
			// essere ovunque dentro il blocco rotto): una posizione
			// nota e sicura da cui scandire in avanti, non quella di
			// un parser che ha appena fallito.
			pos = blockStart + 1;
			while (pos + 1 < tokens.size() && !(tokens[pos].type == vbaTokKeyword
				&& tokens[pos].text == "End" && tokens[pos + 1].type == vbaTokKeyword
				&& tokens[pos + 1].text == startKeyword))
				pos++;
			pos = (pos + 1 < tokens.size()) ? pos + 2 : tokens.size(); // oltre "End Sub"/"End Function"
			continue;
		}

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

bool FindAllVbaTypeDecls(const std::vector<VbaToken>& tokens,
	std::map<std::string, VbaTypeDecl>* outTypes, VbaParseError* outError)
{
	outTypes->clear();
	size_t pos = 0;
	while (pos < tokens.size())
	{
		while (pos < tokens.size() && tokens[pos].type == vbaTokNewline)
			pos++;
		if (pos >= tokens.size() || tokens[pos].type == vbaTokEnd)
			break;

		if (tokens[pos].type == vbaTokKeyword
			&& (tokens[pos].text == "Sub" || tokens[pos].text == "Function"))
		{
			// Un Type non puo' comparire dentro una Sub/Function: si
			// salta l'intero corpo SENZA analizzarlo, cercando la coppia
			// di parole chiave consecutive "End Sub"/"End Function" --
			// MAI prodotta da un If/For annidato (che userebbe "End
			// If"/"Next"), stesso trucco di ListVbaMacroNames sopra.
			std::string endKeyword = tokens[pos].text;
			pos++;
			while (pos + 1 < tokens.size() && !(tokens[pos].type == vbaTokKeyword
				&& tokens[pos].text == "End" && tokens[pos + 1].type == vbaTokKeyword
				&& tokens[pos + 1].text == endKeyword))
				pos++;
			pos = (pos + 1 < tokens.size()) ? pos + 2 : tokens.size(); // oltre "End Sub"/"End Function"
			continue;
		}
		if (tokens[pos].type == vbaTokKeyword && tokens[pos].text == "Type")
		{
			std::unique_ptr<VbaTypeDecl> decl = ParseVbaTypeDecl(tokens, pos, outError);
			if (!decl)
				return false;
			(*outTypes)[NormalizeSubName(decl->name)] = *decl; // chiave normalizzata, vedi VbaAst.h
			continue;
		}
		pos++; // "Attribute VB_Name = ...", "Private"/"Public" davanti a "Type", ecc.
	}
	return true;
}

void FindAllVbaModuleLevelDims(const std::vector<VbaToken>& tokens,
	std::vector<std::unique_ptr<VbaStmt>>* outStmts)
{
	outStmts->clear();
	size_t pos = 0;
	while (pos < tokens.size())
	{
		while (pos < tokens.size() && tokens[pos].type == vbaTokNewline)
			pos++;
		if (pos >= tokens.size() || tokens[pos].type == vbaTokEnd)
			break;

		if (tokens[pos].type == vbaTokKeyword
			&& (tokens[pos].text == "Sub" || tokens[pos].text == "Function"
				|| tokens[pos].text == "Type"))
		{
			// Una variabile di modulo non puo' comparire dentro una
			// Sub/Function/Type: si salta l'intero blocco SENZA
			// analizzarlo, stesso trucco "End" + parola chiave
			// corrispondente di FindAllVbaTypeDecls sopra (i Type
			// stessi sono gia' raccolti a parte da quella funzione,
			// qui si salta solo per non confondere un loro campo con
			// una variabile di modulo).
			std::string endKeyword = tokens[pos].text;
			pos++;
			while (pos + 1 < tokens.size() && !(tokens[pos].type == vbaTokKeyword
				&& tokens[pos].text == "End" && tokens[pos + 1].type == vbaTokKeyword
				&& tokens[pos + 1].text == endKeyword))
				pos++;
			pos = (pos + 1 < tokens.size()) ? pos + 2 : tokens.size();
			continue;
		}
		if (tokens[pos].type == vbaTokKeyword && tokens[pos].text == "Dim")
		{
			size_t before = pos;
			VbaParseError localError;
			std::vector<std::unique_ptr<VbaStmt>> group;
			if (ParseVbaDimStatementList(tokens, pos, &localError, &group))
			{
				for (size_t i = 0; i < group.size(); i++)
					outStmts->push_back(std::move(group[i]));
			}
			else
			{
				// Sintassi fuori da questo sottoinsieme su UN gruppo
				// "Dim" di modulo -- si salta solo in avanti di un
				// token dal punto di partenza e si riprova (questa
				// funzione non fallisce MAI, vedi VbaMacroRunner.h),
				// stesso spirito di "Attribute VB_Name = ..." sotto.
				pos = before + 1;
			}
			continue;
		}
		pos++; // "Attribute VB_Name = ...", "Private"/"Public" davanti, "Option Explicit", ecc.
	}
}

bool ExecuteVbaMacro(const uint8* vbaProjectData, size_t vbaProjectLen,
	const std::string& macroName, CContainer* doc, VbaRuntimeError* outError,
	VbaEnvironment* persistentModuleEnv)
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

		// Fase 6d: il registro dei Type di QUESTO STESSO modulo --
		// serve PRIMA di eseguire, non durante (un Type puo'
		// referenziarne un altro dichiarato piu' in basso, vedi
		// VbaTypeField in VbaAst.h). Un errore di sintassi in un Type
		// e' trattato come in FindVbaSubByName sopra: si rinuncia a
		// QUESTO modulo (la Sub cercata potrebbe benissimo essere
		// valida, ma non si puo' separare "i suoi Type" dal resto senza
		// un secondo scanner ad hoc) e si prova il successivo.
		std::map<std::string, VbaTypeDecl> types;
		VbaParseError typeError;
		if (!FindAllVbaTypeDecls(tokens, &types, &typeError))
			continue;

		// Stato di modulo persistente (trovato controllando il
		// sorgente reale del CHIP-8 -- vedi ROADMAP.md): "env" e'
		// SEMPRE nuovo a ogni chiamata (le variabili LOCALI di una Sub
		// -- comprese quelle mai dichiarate con "Dim", solo assegnate
		// -- non devono MAI persistere da una chiamata all'altra,
		// esattamente come nel vero VBA, dove sono "procedure-level").
		// Solo se il chiamante passa un ambiente ESTERNO, "env" lo
		// collega come proprio FALLBACK (VbaEnvironment::
		// SetModuleEnvironment): Find/Set/IsArray/IsStruct/GetStruct/
		// *ArrayElement cercano prima qui, localmente, e SOLO se
		// assente ricadono su quell'ambiente -- una variabile di
		// modulo come "ThisVM" resta quindi visibile e modificabile
		// da ogni Sub, ma una variabile locale con lo stesso nome di
		// una di modulo la oscura comunque per tutta la chiamata,
		// come dovrebbe.
		VbaEnvironment env;
		env.SetActiveDocument(doc);
		// Fase 6a: collega i token di QUESTO modulo all'ambiente, cosi'
		// EvalCall (VbaEvaluator.cpp) puo' risolvere una chiamata a una
		// Sub sorella con FindVbaSubByName invece di fermarsi al solo
		// elenco di funzioni predefinite -- "tokens" resta valido per
		// tutta la durata di questa chiamata sincrona, anche attraverso
		// chiamate a Sub annidate.
		env.SetModuleTokens(&tokens);
		env.SetTypeRegistry(&types); // Fase 6d, stesso principio di SetModuleTokens
		env.SetCallDepth(0);

		if (persistentModuleEnv)
		{
			env.SetModuleEnvironment(persistentModuleEnv);
			// ExecuteDimDeclaration (sotto) legge GetTypeRegistry() per
			// risolvere un eventuale "As NomeTipo" -- "persistentModuleEnv"
			// non ha il SUO (vive piu' a lungo di "types", che e' locale
			// a QUESTA chiamata): glielo si presta per la durata di questa
			// sola chiamata, stesso "types" gia' collegato a "env" sopra.
			persistentModuleEnv->SetTypeRegistry(&types);

			// "Dim ThisVM As VM" a livello di modulo si inizializza
			// una SOLA volta per sessione (non a ogni "Esegui macro"):
			// DIRETTAMENTE dentro "persistentModuleEnv" (mai "env",
			// che e' locale e sparisce con questa chiamata), e SOLO se
			// non e' gia' presente LI' -- un secondo "Esegui macro"
			// sullo stesso modulo deve trovare la variabile come l'ha
			// lasciata l'esecuzione precedente, mai azzerata di nuovo
			// (a differenza di un "Dim" dentro una Sub, che azzera
			// sempre, vedi ExecuteDimDeclaration/VbaExecutor.cpp --
			// qui si evita di richiamarlo affatto quando gia' presente
			// a livello di modulo).
			std::vector<std::unique_ptr<VbaStmt>> moduleDims;
			FindAllVbaModuleLevelDims(tokens, &moduleDims);
			for (size_t d = 0; d < moduleDims.size(); d++)
			{
				const VbaStmt* dimStmt = moduleDims[d].get();
				bool alreadyDeclared = persistentModuleEnv->Find(dimStmt->varName) != NULL
					|| persistentModuleEnv->IsArray(dimStmt->varName)
					|| persistentModuleEnv->IsStruct(dimStmt->varName);
				if (!alreadyDeclared)
				{
					VbaRuntimeError dimErr; // un errore qui non e' fatale per QUESTA macro: si
					ExecuteDimDeclaration(dimStmt, persistentModuleEnv, &dimErr); // ignora, vedi .h
				}
			}
		}

		return ExecuteVbaSub(sub.get(), &env, outError);
	}

	outError->message = "macro \"" + macroName + "\" non trovata in nessun modulo di questo progetto VBA";
	outError->line = 0;
	return false;
}
