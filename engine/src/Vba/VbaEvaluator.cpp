/*
	VbaEvaluator.cpp

	Vedi VbaEvaluator.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "VbaEvaluator.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

#include "CellStyle.h"
#include "Container.h"
#include "Excel.colors.h"
#include "FontMetrics.h"
#include "VbaExecutor.h"
#include "VbaMacroRunner.h"
#include "VbaModuleSource.h"
#include "VbaToken.h"

// ---------------------------------------------------------------------
// Conversioni di tipo stile VBA (Variant): piu' permissive del C++,
// MAI un errore qui -- una conversione impossibile (es. testo non
// numerico verso numero) produce semplicemente 0.0/testo vuoto/falso,
// stesso principio di una funzione di libreria del motore di calcolo
// gia' esistente (es. Value::operator double() su un Value non
// numerico restituisce NaN invece di fallire) piuttosto che inventare
// qui un secondo stile di segnalazione errori per le sole conversioni.
// ---------------------------------------------------------------------

// Fase 6h: numero seriale VBA/Excel per l'istante corrente, in GIORNI
// (epoca 1899-12-30, costante 25569 condivisa con Excel.pass1.cpp/
// XlsxTranslator.cpp per il caso non-1904) -- condiviso fra "Now"
// (EvalVariable sotto) e "Application.OnTime" (EvalCall sotto), che
// deve confrontare il proprio primo argomento con QUESTO stesso
// istante per calcolare un ritardo. Vedi il commento su "Now" sotto
// sul perche' NON e' la variante taggata "time_t" di Value.
static double CurrentVbaSerialNow()
{
	return (double)time(NULL) / 86400.0 + 25569.0;
}

double VbaToNumber(const Value& v)
{
	switch (v.fType)
	{
		case eNumData: return (double)v;
		case eBoolData: return (bool)v ? 1.0 : 0.0;
		case eTextData: return atof((const char*)v);
		default: return 0.0;
	}
}

std::string VbaToText(const Value& v)
{
	switch (v.fType)
	{
		case eTextData: return std::string((const char*)v);
		case eBoolData: return (bool)v ? "True" : "False";
		case eNumData:
		{
			double d = (double)v;
			char buf[64];
			if (d == (long long)d)
				snprintf(buf, sizeof(buf), "%lld", (long long)d);
			else
				snprintf(buf, sizeof(buf), "%g", d);
			return buf;
		}
		default: return "";
	}
}

bool VbaToBool(const Value& v)
{
	switch (v.fType)
	{
		case eBoolData: return (bool)v;
		case eNumData: return (double)v != 0.0;
		case eTextData:
		{
			std::string s = VbaToText(v);
			return s == "True" || s == "true" || s == "TRUE";
		}
		default: return false;
	}
}

// Confronto condiviso da "=","<>","<",">","<=",">=" (EvalBinary sotto)
// E dal confronto per uguaglianza di Select Case (VbaExecutor.cpp) --
// stessa regola semplificata di VbaToBool/VbaToNumber: numerico se
// ENTRAMBI gli operandi sono numerici/booleani, altrimenti come testo.
// -1/0/1, mai un errore.
int VbaCompareValues(const Value& a, const Value& b)
{
	bool numericCompare = (a.fType == eNumData || a.fType == eBoolData)
		&& (b.fType == eNumData || b.fType == eBoolData);
	if (numericCompare)
	{
		double x = VbaToNumber(a), y = VbaToNumber(b);
		return (x < y) ? -1 : (x > y) ? 1 : 0;
	}
	std::string x = VbaToText(a), y = VbaToText(b);
	int cmp = x.compare(y);
	return (cmp < 0) ? -1 : (cmp > 0) ? 1 : 0;
}

// ---------------------------------------------------------------------
// Fase 3c, modello a oggetti Excel minimo: Range("A1")/Range("A1:B2")/
// Cells(riga,colonna) producono un Value eRangeData legato al
// CContainer attivo (vedi Value::fRangeContainer); ".Value"/la
// coercizione implicita leggono/scrivono la cella vera attraverso
// quel puntatore. Solo celle SINGOLE in questa fase -- un intervallo
// di piu' celle e' un errore chiaro, non un dato sbagliato silenzioso.
// ---------------------------------------------------------------------

// "A1" o "A1:B2" (MAI un nome definito/una tabella in questa fase,
// solo un riferimento letterale) -- entrambe le meta' devono
// consumare l'INTERA sottostringa, altrimenti un testo come "A1X" o
// "Foo" sarebbe accettato per coincidenza (cell::GetCell si ferma al
// primo carattere che non riconosce, non richiede di arrivare alla
// fine).
// Fase 5c, IsNumeric: vero SOLO se l'intero testo (spazi iniziali/
// finali ignorati, come il vero VBA) e' un numero valido -- a
// differenza di VbaToNumber/atof, che si fermano silenziosamente al
// primo carattere che non riconoscono e restituiscono comunque un
// numero parziale (va bene per Val(), MAI per IsNumeric: "12abc" deve
// dare False, non True col valore 12).
static bool IsFullyNumericText(const std::string& text)
{
	size_t start = text.find_first_not_of(" \t");
	if (start == std::string::npos)
		return false; // vuoto o di soli spazi
	size_t end = text.find_last_not_of(" \t");
	std::string trimmed = text.substr(start, end - start + 1);

	char* endPtr = NULL;
	strtod(trimmed.c_str(), &endPtr);
	return endPtr == trimmed.c_str() + trimmed.size() && endPtr != trimmed.c_str();
}

static bool ParseSimpleRangeRef(const std::string& rawText, range* outRange)
{
	// Un riferimento assoluto in stile formula ("$BO$3", trovato nel
	// vero sorgente chip-8-vba: "Range(\"$BO$3\").value = ...") e'
	// IDENTICO a quello nudo ("BO3") per Range()/Cells() -- a
	// differenza di una formula di foglio (cell::GetFormulaCell, che
	// gestisce "$" per davvero), Range() non ha mai un riferimento di
	// cella BASE a cui essere "relativo", quindi il simbolo non cambia
	// alcun significato: solo lo stile di scrittura del vero VBA.
	// cell::GetCell (sotto) non lo riconosce affatto, quindi va
	// scartato qui PRIMA, non passato cosi' com'e'.
	std::string text;
	text.reserve(rawText.size());
	for (size_t i = 0; i < rawText.size(); i++)
		if (rawText[i] != '$')
			text.push_back(rawText[i]);

	size_t colon = text.find(':');
	std::string leftPart = (colon == std::string::npos) ? text : text.substr(0, colon);
	std::string rightPart = (colon == std::string::npos) ? text : text.substr(colon + 1);

	cell topLeft, botRight;
	int leftConsumed = cell::GetCell(leftPart.c_str(), topLeft);
	int rightConsumed = cell::GetCell(rightPart.c_str(), botRight);
	if (leftConsumed != (int)leftPart.size() || rightConsumed != (int)rightPart.size())
		return false;
	if (!topLeft.IsValid() || !botRight.IsValid())
		return false;

	int left = topLeft.h < botRight.h ? topLeft.h : botRight.h;
	int right = topLeft.h > botRight.h ? topLeft.h : botRight.h;
	int top = topLeft.v < botRight.v ? topLeft.v : botRight.v;
	int bottom = topLeft.v > botRight.v ? topLeft.v : botRight.v;
	*outRange = range(left, top, right, bottom);
	return true;
}

static Value MakeRangeValue(const range& r, CContainer* doc)
{
	Value v;
	v = r; // Value::operator=(const range&): fType = eRangeData, fRange = r
	v.fRangeContainer = doc; // vedi il commento su Value::fRangeContainer in Value.h
	return v;
}

bool VbaReadRangeValue(const Value& rangeValue, Value* outResult, VbaRuntimeError* outError, int line)
{
	if (rangeValue.fType != eRangeData)
	{
		outError->message = "qui serve un intervallo (Range/Cells)";
		outError->line = line;
		return false;
	}
	range r = (range)rangeValue;
	if (r.TopLeft() != r.BotRight())
	{
		outError->message = "un intervallo di piu' celle non e' ancora supportato in questa fase "
			"dell'interprete (solo celle singole)";
		outError->line = line;
		return false;
	}
	CContainer* doc = rangeValue.fRangeContainer;
	if (!doc)
	{
		outError->message = "nessun foglio di calcolo collegato a questa esecuzione VBA";
		outError->line = line;
		return false;
	}
	Value cellValue;
	if (!doc->GetValue(r.TopLeft(), cellValue) || cellValue.fType == eNoData)
	{
		*outResult = Value(0.0); // cella vuota: 0, come una Variant vuota di VBA
		return true;
	}
	if (cellValue.fType == eNumData || cellValue.fType == eTextData || cellValue.fType == eBoolData)
	{
		*outResult = cellValue;
		return true;
	}
	*outResult = Value(0.0); // altri tipi di cella (es. un errore di formula) non ancora gestiti qui
	return true;
}

bool VbaWriteRangeValue(const Value& rangeValue, const Value& newValue, VbaRuntimeError* outError, int line)
{
	if (rangeValue.fType != eRangeData)
	{
		outError->message = "il target dell'assegnazione richiede un intervallo (Range/Cells)";
		outError->line = line;
		return false;
	}
	range r = (range)rangeValue;
	if (r.TopLeft() != r.BotRight())
	{
		outError->message = "la scrittura su un intervallo di piu' celle non e' ancora supportata "
			"in questa fase dell'interprete (solo celle singole)";
		outError->line = line;
		return false;
	}
	CContainer* doc = rangeValue.fRangeContainer;
	if (!doc)
	{
		outError->message = "nessun foglio di calcolo collegato a questa esecuzione VBA";
		outError->line = line;
		return false;
	}
	// NewCell, non SetValue: una scrittura da VBA sostituisce
	// interamente il contenuto della cella (come l'utente che digita
	// sopra), cancellando un'eventuale formula precedente -- stessa
	// tecnica gia' usata da Ricerca Obiettivo/Tabelle di simulazione
	// altrove in questo codice.
	doc->NewCell(r.TopLeft(), newValue, NULL);
	return true;
}

// Stesso controllo di VbaReadRangeValue/VbaWriteRangeValue sopra
// (un intervallo di UNA sola cella con un documento vero collegato),
// fattorizzato qui perche' sia la lettura sia la scrittura del colore
// (sotto) ne hanno bisogno prima di toccare CellStyle.
static bool GetSingleCellForStyle(const Value& rangeValue, cell* outCell, CContainer** outDoc,
	VbaRuntimeError* outError, int line)
{
	if (rangeValue.fType != eRangeData)
	{
		outError->message = "qui serve un intervallo (Range/Cells)";
		outError->line = line;
		return false;
	}
	range r = (range)rangeValue;
	if (r.TopLeft() != r.BotRight())
	{
		outError->message = "un intervallo di piu' celle non e' ancora supportato in questa fase "
			"dell'interprete (solo celle singole)";
		outError->line = line;
		return false;
	}
	if (!rangeValue.fRangeContainer)
	{
		outError->message = "nessun foglio di calcolo collegato a questa esecuzione VBA";
		outError->line = line;
		return false;
	}
	*outCell = r.TopLeft();
	*outDoc = rangeValue.fRangeContainer;
	return true;
}

bool VbaGetInteriorColor(const Value& rangeValue, bool asColorIndex, Value* outResult,
	VbaRuntimeError* outError, int line)
{
	cell c;
	CContainer* doc;
	if (!GetSingleCellForStyle(rangeValue, &c, &doc, outError, line))
		return false;

	CellStyle cs;
	doc->GetCellStyle(c, cs);

	if (asColorIndex)
	{
		// Ricerca per corrispondenza ESATTA nella tavolozza standard --
		// un colore che non vi appartiene (es. scritto da ".Color" con
		// un RGB qualunque) non ha un ColorIndex valido: 0 (come
		// xlColorIndexNone del vero VBA) e' l'esito onesto, non una
		// corrispondenza approssimata a caso.
		for (int i = 0; i < 56; i++)
		{
			const rgb_color& k = kExcelColorTable[i];
			if (k.red == cs.fLowColor.red && k.green == cs.fLowColor.green && k.blue == cs.fLowColor.blue)
			{
				*outResult = Value((double)(i + 1));
				return true;
			}
		}
		*outResult = Value(0.0);
		return true;
	}

	// ".Color": un Long in formato &H00BBGGRR, stesso ordine di byte
	// della vera funzione VBA RGB(r, g, b) = r + g*256 + b*65536.
	*outResult = Value((double)cs.fLowColor.red + (double)cs.fLowColor.green * 256.0
		+ (double)cs.fLowColor.blue * 65536.0);
	return true;
}

bool VbaSetInteriorColor(const Value& rangeValue, bool asColorIndex, const Value& newValue,
	VbaRuntimeError* outError, int line)
{
	cell c;
	CContainer* doc;
	if (!GetSingleCellForStyle(rangeValue, &c, &doc, outError, line))
		return false;

	rgb_color color;
	if (asColorIndex)
	{
		int idx = (int)VbaToNumber(newValue);
		if (idx < 1 || idx > 56)
		{
			outError->message = "ColorIndex deve essere compreso tra 1 e 56";
			outError->line = line;
			return false;
		}
		color = kExcelColorTable[idx - 1];
	}
	else
	{
		long long v = (long long)VbaToNumber(newValue);
		color.red = (uchar)(v & 0xFF);
		color.green = (uchar)((v >> 8) & 0xFF);
		color.blue = (uchar)((v >> 16) & 0xFF);
		color.alpha = 255;
	}

	CellStyle cs;
	doc->GetCellStyle(c, cs);
	cs.fLowColor = color;
	doc->SetCellStyle(c, cs);
	return true;
}

// Sottostringa case-insensitive minima, serve solo a questa sezione
// (stessa ricerca che MainWindow::ToggleBold/ToggleItalic gia' fanno
// con BString::IFindFirst sullo stile corrente) -- un piccolo
// duplicato locale invece di dipendere da BString qui, che l'engine
// non usa altrove in questo file.
static bool StyleNameContains(const char* style, const char* needleLower)
{
	std::string s(style);
	for (size_t i = 0; i < s.size(); i++)
		s[i] = (char)tolower((unsigned char)s[i]);
	return s.find(needleLower) != std::string::npos;
}

// ".Font.Bold"/".Font.Italic"/".Font.Size"/".Font.Name" -- a differenza
// di ".Interior.Color" sopra, "CellStyle::fFont" E' un indice (non un
// valore diretto come "fLowColor"), ma la tabella che lo risolve
// (gFontSizeTable, FontMetrics.h) e' GIA' interamente nell'engine,
// usata gia' da Container.cpp per lo stesso identico scopo in
// importazione -- NON serve affatto lo strato UI qui, a differenza di
// quanto una lettura superficiale di ROADMAP.md (nota "deliberately
// deferred" di Fase 5) lascerebbe credere: quella nota riguardava la
// UI toolbar (GetCellFontInfo/ApplyFontToRange, solo wrapper sottili
// attorno a QUESTA stessa tabella), non un vero limite architetturale.
// Stesso principio esatto di MainWindow::ToggleBold/ToggleItalic (lo
// stile si ricompone sempre per intero da zero, "Bold Italic" incluso,
// mai un flag separato) ma generalizzato a un valore ESPLICITO invece
// che un semplice inversione.
bool VbaGetFontProperty(const Value& rangeValue, const std::string& propName, Value* outResult,
	VbaRuntimeError* outError, int line)
{
	cell c;
	CContainer* doc;
	if (!GetSingleCellForStyle(rangeValue, &c, &doc, outError, line))
		return false;

	CellStyle cs;
	doc->GetCellStyle(c, cs);
	font_family family;
	font_style style;
	float size;
	gFontSizeTable.GetFontInfo(cs.fFont, &family, &style, &size);

	if (propName == "bold")
		*outResult = Value(StyleNameContains(style, "bold"));
	else if (propName == "italic")
		*outResult = Value(StyleNameContains(style, "italic"));
	else if (propName == "size")
		*outResult = Value((double)size);
	else if (propName == "name")
		*outResult = Value(family);
	else
	{
		outError->message = "proprieta' \"" + propName + "\" di Font non supportata in questa fase";
		outError->line = line;
		return false;
	}
	return true;
}

bool VbaSetFontProperty(const Value& rangeValue, const std::string& propName,
	const Value& newValue, VbaRuntimeError* outError, int line)
{
	cell c;
	CContainer* doc;
	if (!GetSingleCellForStyle(rangeValue, &c, &doc, outError, line))
		return false;

	CellStyle cs;
	doc->GetCellStyle(c, cs);
	font_family family;
	font_style style;
	float size;
	rgb_color color;
	gFontSizeTable.GetFontInfo(cs.fFont, &family, &style, &size, &color);

	bool isBold = StyleNameContains(style, "bold");
	bool isItalic = StyleNameContains(style, "italic");

	if (propName == "bold")
		isBold = VbaToBool(newValue);
	else if (propName == "italic")
		isItalic = VbaToBool(newValue);
	else if (propName == "size")
		size = (float)VbaToNumber(newValue);
	else if (propName == "name")
	{
		std::string n = VbaToText(newValue);
		strlcpy(family, n.c_str(), sizeof(font_family));
	}
	else
	{
		outError->message = "proprieta' \"" + propName + "\" di Font non supportata in questa fase";
		outError->line = line;
		return false;
	}

	font_style newStyle;
	if (isBold && isItalic)
		strlcpy(newStyle, "Bold Italic", sizeof(font_style));
	else if (isBold)
		strlcpy(newStyle, "Bold", sizeof(font_style));
	else if (isItalic)
		strlcpy(newStyle, "Italic", sizeof(font_style));
	else
		strlcpy(newStyle, "Regular", sizeof(font_style));

	cs.fFont = (int)gFontSizeTable.GetFontID(family, newStyle, size, color);
	doc->SetCellStyle(c, cs);
	return true;
}

// ---------------------------------------------------------------------
// Fase 6d: accesso a un campo di un tipo definito dall'utente (es.
// "ThisVM.PC", "ThisVM.V(2)", "ThisVM.Screen(x,y).value"). Vedi il
// commento su ResolveStructScalarField in VbaEvaluator.h per i tre
// esiti possibili e perche' e' condivisa fra lettura e scrittura.
// ---------------------------------------------------------------------

// Calcola l'indice appiattito di "expr->children" (gia' valutati come
// numeri) dentro "field->dims" -- stessa idea di ComputeFlatArrayIndex
// in VbaEnvironment.cpp, ma qui serve anche valutare le espressioni
// indice (quella funzione le riceve gia' pronte), quindi non si riusa
// direttamente: duplicazione minima, non vale la pena esporre una
// terza firma solo per questo.
static bool EvalStructFieldFlatIndex(const VbaExpr* expr, VbaEnvironment* env,
	const VbaStructField* field, size_t* outFlat, std::string* outError)
{
	std::vector<int> indices;
	for (size_t i = 0; i < expr->children.size(); i++)
	{
		Value v;
		VbaRuntimeError err;
		if (!EvaluateVbaExpression(expr->children[i].get(), env, &v, &err))
		{
			*outError = err.message;
			return false;
		}
		indices.push_back((int)VbaToNumber(v));
	}
	if (indices.size() != field->dims.size())
	{
		*outError = "numero di indici errato per l'array \"" + expr->name + "\"";
		return false;
	}
	size_t flat = 0;
	for (size_t i = 0; i < field->dims.size(); i++)
	{
		if (indices[i] < 0 || indices[i] >= field->dims[i])
		{
			*outError = "indice fuori dai limiti dell'array \"" + expr->name + "\"";
			return false;
		}
		flat = flat * (size_t)field->dims[i] + (size_t)indices[i];
	}
	*outFlat = flat;
	return true;
}

// Risolve "expr" a un'ISTANZA di struct -- un passo INTERMEDIO di una
// catena, mai il campo finale (quello lo risolve ResolveStructScalarField
// sotto). "expr" deve essere "NomeVariabile" (caso base: una struct
// dichiarata con "Dim ... As NomeTipo") oppure un altro passo di catena
// (Call-con-target o PropertyAccess) che porta a un campo che e' ESSO
// STESSO un tipo definito dall'utente. NULL se "expr" non e' risolvibile
// a una struct -- "outError" si popola SOLO se "expr" era chiaramente
// un tentativo di accesso a struct poi fallito, mai per un "non e'
// questo il caso" silenzioso (il chiamante lo distingue controllando se
// "outError" e' vuoto).
VbaStructInstance* ResolveStructInstanceStep(const VbaExpr* expr, VbaEnvironment* env,
	std::string* outError)
{
	if (expr->kind == vbaExprVariable)
		return env->GetStruct(expr->name); // caso base, NULL se non e' affatto una struct dichiarata

	if (expr->kind == vbaExprWithImplicitTarget)
	{
		// Fase 6e: ".Nome" dentro un "With" -- caso base alternativo,
		// stesso principio di vbaExprVariable sopra ma leggendo dalla
		// pila dei "With" attivi invece che da una variabile per nome.
		const VbaWithTarget* top = env->GetCurrentWithTarget();
		if (!top)
		{
			*outError = "\".\" fuori da un blocco \"With\"";
			return NULL;
		}
		return top->isStruct ? top->structInstance : NULL; // NULL (nessun errore) se il With e' su un Value
	}

	const VbaExpr* stepTarget = NULL;
	if (expr->kind == vbaExprCall && expr->target)
		stepTarget = expr->target.get();
	else if (expr->kind == vbaExprPropertyAccess)
		stepTarget = expr->target.get();
	else
		return NULL; // non e' affatto un nodo di catena (una chiamata nuda, un letterale, ...)

	VbaStructInstance* base = ResolveStructInstanceStep(stepTarget, env, outError);
	if (!base)
		return NULL; // il passo precedente non porta a una struct -- propaga (outError gia' impostato se serve)

	VbaStructField* field = base->FindField(expr->name);
	if (!field)
	{
		*outError = "il tipo non ha un campo \"" + expr->name + "\"";
		return NULL;
	}
	if (!field->isStruct)
	{
		*outError = "\"" + expr->name + "\" non e' un campo di tipo definito dall'utente";
		return NULL;
	}

	if (expr->kind == vbaExprPropertyAccess)
	{
		if (field->isArray)
		{
			*outError = "\"" + expr->name + "\" e' un array: serve un indice";
			return NULL;
		}
		return field->structValue.get();
	}

	// vbaExprCall con target: accesso indicizzato a un array-di-struct
	// (es. "ThisVM.Screen(x,y)" come passo intermedio di ".value")
	if (!field->isArray)
	{
		*outError = "\"" + expr->name + "\" non e' un array";
		return NULL;
	}
	size_t flat;
	if (!EvalStructFieldFlatIndex(expr, env, field, &flat, outError))
		return NULL;
	return field->structArray[flat].get();
}

Value* ResolveStructScalarField(const VbaExpr* expr, VbaEnvironment* env, std::string* outError)
{
	const VbaExpr* stepTarget = NULL;
	if (expr->kind == vbaExprCall && expr->target)
		stepTarget = expr->target.get();
	else if (expr->kind == vbaExprPropertyAccess)
		stepTarget = expr->target.get();
	else
		return NULL; // "expr" stesso non e' un nodo di catena -- non e' affatto un accesso a struct

	std::string chainErr;
	VbaStructInstance* base = ResolveStructInstanceStep(stepTarget, env, &chainErr);
	if (!base)
	{
		if (!chainErr.empty())
			*outError = chainErr;
		return NULL; // "stepTarget" non porta a una struct: non e' questo il caso (outError vuoto)
	}

	VbaStructField* field = base->FindField(expr->name);
	if (!field)
	{
		*outError = "il tipo non ha un campo \"" + expr->name + "\"";
		return NULL;
	}

	if (expr->kind == vbaExprPropertyAccess)
	{
		if (field->isArray || field->isStruct)
		{
			*outError = "\"" + expr->name + "\" non e' un campo scalare (l'assegnazione di "
				"un'intera struct/di un intero array non e' supportata in questo sottoinsieme)";
			return NULL;
		}
		return &field->scalarValue;
	}

	// vbaExprCall con target: "ThisVM.V(indici)" -- un array SCALARE
	if (field->isStruct)
	{
		*outError = "\"" + expr->name + "\" e' un array di tipo definito dall'utente, "
			"serve ancora un campo dopo l'indice (es. \".value\")";
		return NULL;
	}
	if (!field->isArray)
	{
		*outError = "\"" + expr->name + "\" non e' un array";
		return NULL;
	}
	size_t flat;
	if (!EvalStructFieldFlatIndex(expr, env, field, &flat, outError))
		return NULL;
	return &field->scalarArray[flat];
}

// Fase 6g: vedi il commento su VbaOpenFileDialogFn in VbaEvaluator.h --
// NULL finche' ui/ (MainWindow.cpp) non chiama VbaSetOpenFileDialogHook
// una sola volta all'avvio. Variabile di file, non di classe: la legge
// l'Evaluator dentro il namespace anonimo sotto, la scrive solo questo
// setter.
static VbaOpenFileDialogFn gOpenFileDialogHook = NULL;

void VbaSetOpenFileDialogHook(VbaOpenFileDialogFn fn)
{
	gOpenFileDialogHook = fn;
}

// Fase 6h: stesso principio di gOpenFileDialogHook sopra, per
// VbaOnTimeFn/VbaSetOnTimeHook (vedi VbaEvaluator.h).
static VbaOnTimeFn gOnTimeHook = NULL;

void VbaSetOnTimeHook(VbaOnTimeFn fn)
{
	gOnTimeHook = fn;
}

// Stesso principio di gOpenFileDialogHook/gOnTimeHook sopra, per
// VbaSetControlCaptionFn/VbaSetControlCaptionHook (vedi VbaEvaluator.h)
// -- usato da VbaExecutor.cpp (ExecuteAssignOrSet), non da questo file,
// ma vive qui insieme agli altri ganci per restare facile da trovare.
static VbaSetControlCaptionFn gSetControlCaptionHook = NULL;

void VbaSetControlCaptionHook(VbaSetControlCaptionFn fn)
{
	gSetControlCaptionHook = fn;
}

// Wrapper non-statico attorno a gSetControlCaptionHook (file-locale,
// invisibile da VbaExecutor.cpp come gli altri ganci): a differenza di
// VbaSetFontProperty/VbaWriteRangeValue sopra, qui serve distinguere
// "nessun gancio installato affatto" (nessuna interfaccia collegata,
// es. engine/tests) da "gancio installato ma nessun controllo con
// questo nome" -- due errori onesti diversi, non uno generico --
// quindi "outHookInstalled" invece di un semplice bool di ritorno.
bool VbaSetControlCaption(void* hostContext, const std::string& controlName,
	const std::string& caption, bool* outHookInstalled)
{
	*outHookInstalled = (gSetControlCaptionHook != NULL);
	if (!gSetControlCaptionHook)
		return false;
	return gSetControlCaptionHook(hostContext, controlName, caption);
}

// Cerca "name" come Sub/Function PRIMA nel modulo corrente
// (env->GetModuleTokens()), poi -- se non trovata li' -- in ogni ALTRO
// modulo dello stesso progetto (env->GetAllModules(), collegato da
// ExecuteVbaMacro). Chiude un gap noto esplicitamente della Fase 6a
// ("una chiamata sorella risolve solo nello STESSO modulo, una Sub di
// un modulo diverso e' un gap separato"). "scratchTokens" e' di
// proprieta' del CHIAMANTE (mai di questa funzione): quando la Sub
// trovata viene da un modulo DIVERSO da quello corrente, il suo
// sorgente si ri-tokenizza al volo dentro "scratchTokens" (nessuna
// estrazione CFB ripetuta, il sorgente e' gia' in GetAllModules()) --
// il puntatore restituito punta li', valido per tutta la durata della
// chiamata (lo stesso principio di fModuleTokens, mai un puntatore a
// un temporaneo che sparisce subito dopo). Riprovare lo stesso modulo
// corrente una seconda volta dentro GetAllModules() (se ce lo
// restituisce) e' innocuo, solo ridondante -- FindVbaSubByName e'
// gia' fallita la prima volta, fallisce di nuovo identica. NULL
// (outSub invariato) se nessun modulo (corrente o altro) la contiene.
static const std::vector<VbaToken>* FindVbaSubAcrossModules(VbaEnvironment* env,
	const std::string& name, std::unique_ptr<VbaSub>* outSub,
	std::vector<VbaToken>* scratchTokens)
{
	if (env->GetModuleTokens())
	{
		VbaParseError err;
		if (FindVbaSubByName(*env->GetModuleTokens(), name, outSub, &err))
			return env->GetModuleTokens();
	}
	if (env->GetAllModules())
	{
		for (size_t m = 0; m < env->GetAllModules()->size(); m++)
		{
			const VbaModuleSource& mod = (*env->GetAllModules())[m];
			scratchTokens->clear();
			TokenizeVba(mod.sourceText, scratchTokens);
			VbaParseError err;
			if (FindVbaSubByName(*scratchTokens, name, outSub, &err))
				return scratchTokens;
		}
	}
	return NULL;
}

namespace {

class Evaluator {
public:
	Evaluator(VbaEnvironment* env, VbaRuntimeError* outError)
		: fEnv(env), fError(outError), fFailed(false) {}

	bool Evaluate(const VbaExpr* expr, Value* outResult)
	{
		Value v = EvalNode(expr);
		if (fFailed)
			return false;
		*outResult = v;
		return true;
	}

private:
	VbaEnvironment* fEnv;
	VbaRuntimeError* fError;
	bool fFailed;

	void SetError(const std::string& message, int line)
	{
		if (fFailed)
			return; // mantiene il PRIMO errore incontrato
		fFailed = true;
		fError->message = message;
		fError->line = line;
	}

	Value EvalNode(const VbaExpr* expr)
	{
		if (fFailed)
			return Value(0.0);

		switch (expr->kind)
		{
			case vbaExprNumber:
				return Value(expr->numberValue);
			case vbaExprString:
				return Value(expr->stringValue.c_str());
			case vbaExprBool:
				return Value(expr->boolValue);
			case vbaExprVariable:
				return EvalVariable(expr);
			case vbaExprUnary:
				return EvalUnary(expr);
			case vbaExprBinary:
				return EvalBinary(expr);
			case vbaExprCall:
				return EvalCall(expr);
			case vbaExprPropertyAccess:
				return EvalPropertyAccess(expr);
			case vbaExprWithImplicitTarget:
				return EvalWithImplicitTarget(expr);
		}
		SetError("tipo di espressione sconosciuto", expr->line);
		return Value(0.0);
	}

	Value EvalVariable(const VbaExpr* expr)
	{
		// "DoEvents" si scrive SENZA parentesi nel vero VBA (una Sub a
		// zero argomenti chiamata senza "()"), quindi arriva qui come
		// una variabile mai dichiarata, non come un vbaExprCall --
		// no-op deliberato (restituisce 0.0 e basta), STESSO principio
		// di ".Select" in EvalPropertyAccess: nessun event loop/message
		// pump reale a questo livello (motore puro, nessuna UI
		// collegata). Diverso da un vero "Application.OnTime"
		// ri-entrante (ancora fuori scopo, vedi ROADMAP.md Fase 6h) --
		// qui serve solo perche' una macro reale lo scrive dentro un
		// ciclo stretto per restare reattiva, non per schedulare nulla.
		if (Lowercase(expr->name) == "doevents")
			return Value(0.0);

		// Fase 6g: "FreeFile" e' un'altra funzione del vero VBA scritta
		// SENZA parentesi quando chiamata a zero argomenti (stesso
		// motivo di "DoEvents" sopra), ma qui NON e' un no-op: deve
		// restituire un numero di file davvero libero (VbaEnvironment::
		// AllocateFreeFileNumber), serve a "Open ... As #file" subito
		// dopo per sapere quale canale usare.
		if (Lowercase(expr->name) == "freefile")
			return Value((double)fEnv->AllocateFreeFileNumber());

		// Fase 6g (seguito): "Randomize" e' un'altra Sub incorporata del
		// vero VBA scritta SENZA parentesi a zero argomenti (stesso
		// motivo di "DoEvents"/"FreeFile" sopra) -- a differenza di
		// "DoEvents" questa ha un effetto vero (reinizializza il seme di
		// "Rnd" sopra con l'orologio di sistema, stesso principio del
		// vero VBA), non un no-op deliberato.
		if (Lowercase(expr->name) == "randomize")
		{
			srand((unsigned)time(NULL));
			return Value(0.0);
		}

		// Fase 6g (seguito): il vero sorgente chip-8-vba scrive "Rnd"
		// SENZA parentesi anche DENTRO un'espressione ("Int(PowerOfTwo(8)
		// * Rnd)", non solo come istruzione nuda) -- arriva quindi qui,
		// non a EvalCall (che gestirebbe solo "Rnd()", mai scritto dal
		// vero sorgente), stesso principio di "FreeFile" sopra.
		if (Lowercase(expr->name) == "rnd")
			return Value((double)rand() / ((double)RAND_MAX + 1.0));

		// Fase 6h: "Now" e' un'altra Sub/funzione incorporata del vero
		// VBA scritta SENZA parentesi (stesso motivo di "DoEvents"/
		// "FreeFile"/"Rnd" sopra), stavolta serve a calcolare un ritardo
		// per "Application.OnTime" (vedi EvalCall sotto). Restituisce un
		// numero seriale in GIORNI (la convenzione VBA/Excel vera, epoca
		// 1899-12-30 -- 25569 e' la stessa costante di
		// Excel.pass1.cpp/XlsxTranslator.cpp per l'epoca non-1904),
		// DELIBERATAMENTE un Value(double) normale, NON la variante
		// taggata "time_t" di Value (Value.h) usata da NOWFunction per
		// le celle (Functions.date-time.cpp): quella variante arrotonda
		// un'addizione al SECONDO intero (Value.cpp,
		// "static_cast<time_t>"), troncando a zero una frazione di
		// giorno piccola come "0.000000011574" (~1ms) -- esattamente il
		// valore che il vero sorgente chip-8-vba aggiunge a "Now" per
		// programmare il prossimo "Application.OnTime". Un Value(double)
		// normale fa l'addizione in virgola mobile vera, come il vero
		// VBA.
		if (Lowercase(expr->name) == "now")
			return Value(CurrentVbaSerialNow());

		if (fEnv->IsStruct(expr->name))
		{
			// Fase 6d: un'istanza di Type non e' un valore scalare --
			// niente assegnazione di un'intera struct in questo
			// sottoinsieme (vedi VbaEnvironment.h). Un errore chiaro
			// invece del generico "usata prima di essere assegnata"
			// sotto (tecnicamente vero: non e' MAI in fVars), che
			// confonderebbe la causa reale.
			SetError("\"" + expr->name + "\" e' un'istanza di tipo definito dall'utente, "
				"non un valore scalare -- accedi a un suo campo con \".\"", expr->line);
			return Value(0.0);
		}
		const Value* v = fEnv->Find(expr->name);
		if (v == NULL)
		{
			// Fase 6g (seguito): un nome mai assegnato potrebbe essere una
			// chiamata a una Sub/Function SORELLA SENZA argomenti scritta
			// senza parentesi (es. "Redraw" da sola come istruzione, o
			// "InitConstants" dentro un'altra Sub) -- trovato controllando
			// il vero sorgente chip-8-vba, che lo scrive pesantemente,
			// stesso principio di "DoEvents"/"FreeFile"/"Rnd" sopra ma per
			// una Sub DEFINITA DALL'UTENTE invece che incorporata nel
			// motore. Si prova PRIMA di arrendersi con l'errore "usata
			// prima di essere assegnata" sotto, cosi' un vero errore di
			// battitura (nessuna Sub con quel nome) resta comunque
			// quell'errore, non un comportamento diverso per sbaglio --
			// "FindVbaSubByName" fallisce pulito in quel caso, si cade
			// attraverso senza alcun effetto collaterale.
			{
				// Solo un controllo di ESISTENZA qui (in qualunque
				// modulo, vedi FindVbaSubAcrossModules sopra) -- la
				// vera risoluzione/esecuzione la rifa' EvalCall sotto
				// con la sua propria "scratchTokens", "callee"/
				// "scratch" qui sono scartati apposta.
				std::unique_ptr<VbaSub> callee;
				std::vector<VbaToken> scratch;
				if (FindVbaSubAcrossModules(fEnv, expr->name, &callee, &scratch))
				{
					VbaExpr syntheticCall(vbaExprCall);
					syntheticCall.name = expr->name;
					syntheticCall.line = expr->line;
					return EvalCall(&syntheticCall);
				}
			}
			SetError("variabile \"" + expr->name + "\" usata prima di essere assegnata", expr->line);
			return Value(0.0);
		}
		return *v;
	}

	// Fase 6e: un ".Nome" dentro un "With" si risolve PRIMA come
	// possibile accesso a struct (ResolveStructScalarField/
	// ResolveStructInstanceStep, chiamati da EvalCall/EvalPropertyAccess
	// prima di arrivare qui) -- se "expr" (il segnaposto nudo) arriva
	// fino a EvalNode, vuol dire che il "With" attivo e' su un Value
	// semplice (es. "With Range(\"A1\")"), non su una struct: si legge
	// il valore cacheato una volta sola all'ingresso del blocco (vedi
	// ExecuteWith in VbaExecutor.cpp), MAI ri-valutando il target.
	Value EvalWithImplicitTarget(const VbaExpr* expr)
	{
		const VbaWithTarget* top = fEnv->GetCurrentWithTarget();
		if (!top)
		{
			SetError("\".\" fuori da un blocco \"With\"", expr->line);
			return Value(0.0);
		}
		if (top->isStruct)
		{
			// Difensivo, non dovrebbe capitare mai in pratica: se il
			// target e' una struct, ResolveStructInstanceStep l'avrebbe
			// gia' intercettato prima di arrivare qui (vedi sopra).
			SetError("il target di \"With\" e' un'istanza di tipo definito dall'utente, "
				"non un valore scalare -- accedi a un suo campo con \".\"", expr->line);
			return Value(0.0);
		}
		return top->value;
	}

	Value EvalUnary(const VbaExpr* expr)
	{
		Value operand = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		if (expr->op == "-")
			return Value(-VbaToNumber(operand));
		if (expr->op == "Not")
		{
			// Fase 6g (seguito): "Not" e' bit a bit nel vero VBA (~x), non
			// solo logico -- su un vero Boolean i due letture coincidono
			// (il motore rappresenta True/False come 1.0/0.0, non -1/0
			// come il vero VBA internamente, ma il bit 0 e' l'unico che
			// conta quando il valore e' gia' 0 o 1), quindi questo ramo
			// resta per compatibilita' con l'output booleano atteso dai
			// test esistenti; su un numero generico (es. una maschera di
			// bit) e' un vero complemento bit a bit, mai una
			// semplificazione booleana.
			if (operand.fType == eBoolData)
				return Value(!VbaToBool(operand));
			return Value((double)(~(long long)VbaToNumber(operand)));
		}
		SetError("operatore unario sconosciuto \"" + expr->op + "\"", expr->line);
		return Value(0.0);
	}

	Value EvalBinary(const VbaExpr* expr)
	{
		Value left = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		Value right = EvalNode(expr->children[1].get());
		if (fFailed)
			return Value(0.0);

		const std::string& op = expr->op;

		if (op == "&")
			return Value((VbaToText(left) + VbaToText(right)).c_str());

		if (op == "+" || op == "-" || op == "*" || op == "/" || op == "^")
		{
			double a = VbaToNumber(left), b = VbaToNumber(right);
			if (op == "+") return Value(a + b);
			if (op == "-") return Value(a - b);
			if (op == "*") return Value(a * b);
			if (op == "/")
			{
				if (b == 0.0)
				{
					SetError("divisione per zero", expr->line);
					return Value(0.0);
				}
				return Value(a / b);
			}
			return Value(pow(a, b)); // "^"
		}

		if (op == "Mod")
		{
			long long a = (long long)VbaToNumber(left), b = (long long)VbaToNumber(right);
			if (b == 0)
			{
				SetError("divisione per zero (Mod)", expr->line);
				return Value(0.0);
			}
			return Value((double)(a % b));
		}

		// Fase 6g (seguito): "\" e' la divisione INTERA di VBA (diversa
		// da "/"), trovata controllando il vero sorgente chip-8-vba per
		// estrarre bit da un'istruzione a 16 bit (es. "x \ 256").
		if (op == "\\")
		{
			long long a = (long long)VbaToNumber(left), b = (long long)VbaToNumber(right);
			if (b == 0)
			{
				SetError("divisione per zero (\\)", expr->line);
				return Value(0.0);
			}
			return Value((double)(a / b));
		}

		if (op == "And" || op == "Or" || op == "Xor")
		{
			// Fase 6g (seguito): "And"/"Or"/"Xor" sono SEMPRE bit a bit in
			// VBA, non logici -- un bug reale di questo motore prima di
			// questa fase (restituiva solo 0/1 via VbaToBool, come se
			// fossero puramente booleani), trovato controllando il vero
			// sorgente chip-8-vba, che li usa pesantemente per mascherare
			// un'istruzione a 16 bit (es. "instruction And &HF000&").
			// Risultato ancora un Boolean (True/False, non 1/0) SOLO
			// quando ENTRAMBI gli operandi sono gia' un vero Boolean --
			// bit a bit sul solo bit 0 da' comunque lo stesso esito
			// logico in quel caso, nessuna conversione speciale serve,
			// ma preserva il tipo di Value atteso da ogni test/macro che
			// usa questi operatori su condizioni (es. "If a > 0 And b > 0
			// Then").
			long long a = (long long)VbaToNumber(left), b = (long long)VbaToNumber(right);
			long long r = (op == "And") ? (a & b) : (op == "Or") ? (a | b) : (a ^ b);
			if (left.fType == eBoolData && right.fType == eBoolData)
				return Value(r != 0);
			return Value((double)r);
		}

		if (op == "=" || op == "<>" || op == "<" || op == ">" || op == "<=" || op == ">=")
		{
			int cmp = VbaCompareValues(left, right);
			if (op == "=") return Value(cmp == 0);
			if (op == "<>") return Value(cmp != 0);
			if (op == "<") return Value(cmp < 0);
			if (op == ">") return Value(cmp > 0);
			if (op == "<=") return Value(cmp <= 0);
			return Value(cmp >= 0); // ">="
		}

		SetError("operatore binario sconosciuto \"" + op + "\"", expr->line);
		return Value(0.0);
	}

	static std::string Lowercase(const std::string& s)
	{
		std::string out = s;
		for (size_t i = 0; i < out.size(); i++)
			out[i] = (char)tolower((unsigned char)out[i]);
		return out;
	}

	Value EvalCall(const VbaExpr* expr)
	{
		if (expr->target)
		{
			// Fase 6d: "ThisVM.V(indici)" o un passo intermedio come
			// "ThisVM.Screen(x,y)" (array di un campo di struct) --
			// DEVE venire prima di "Offset"/dell'errore generico sotto,
			// che altrimenti lo tratterebbero come un metodo del
			// modello a oggetti Excel.
			std::string structErr;
			Value* fieldSlot = ResolveStructScalarField(expr, fEnv, &structErr);
			if (fieldSlot)
				return *fieldSlot;
			if (!structErr.empty())
			{
				SetError(structErr, expr->line);
				return Value(0.0);
			}

			if (Lowercase(expr->name) == "offset")
				return EvalOffsetCall(expr);

			// Fase 6h: "Application.OnTime EarliestTime, Procedure" --
			// arriva qui (non a EvalPropertyAccess) perche' il parser
			// (VbaStmtParser.cpp, ParseAssignOrExprStmt) lo riscrive in
			// un vbaExprCall con "target" non appena vede un elenco di
			// argomenti dopo il "." -- stesso principio di
			// "Application.GetOpenFilename" in EvalPropertyAccess, ma
			// quello non ha mai argomenti. Solo la forma a 2 argomenti
			// del vero VBA (niente "LatestTime"/"Schedule:=False" -- il
			// vero sorgente chip-8-vba non li usa mai, vedi ROADMAP.md
			// Fase 6h).
			if (Lowercase(expr->name) == "ontime" && expr->target->kind == vbaExprVariable
				&& Lowercase(expr->target->name) == "application" && expr->children.size() == 2)
			{
				Value earliestTime = EvalNode(expr->children[0].get());
				if (fFailed)
					return Value(0.0);
				Value procedureValue = EvalNode(expr->children[1].get());
				if (fFailed)
					return Value(0.0);

				double delaySeconds = (VbaToNumber(earliestTime) - CurrentVbaSerialNow()) * 86400.0;
				if (delaySeconds < 0.0)
					delaySeconds = 0.0;

				// Il vero sorgente chip-8-vba chiama "Application.OnTime
				// Count, "CHIP8List.ProcessVM"" -- un nome QUALIFICATO
				// col modulo, mai riconosciuto da FindVbaSubByName/
				// ExecuteVbaMacro (solo nomi di Sub nudi, a prescindere
				// dal modulo): si tiene solo la parte dopo l'ULTIMO "."
				// prima di passarlo al gancio sotto, normalizzazione
				// minima, non una vera risoluzione per modulo.
				std::string macroName = VbaToText(procedureValue);
				size_t dot = macroName.find_last_of('.');
				if (dot != std::string::npos)
					macroName = macroName.substr(dot + 1);

				if (gOnTimeHook)
					gOnTimeHook(fEnv->GetHostContext(), delaySeconds, macroName);
				return Value(0.0);
			}

			SetError("il metodo \"" + expr->name + "\" del modello a oggetti Excel "
				"non e' ancora supportato in questa fase dell'interprete", expr->line);
			return Value(0.0);
		}

		std::string name = Lowercase(expr->name);
		if (name == "range")
			return EvalRangeFunction(expr);
		if (name == "cells")
			return EvalCellsFunction(expr);

		// Fase 6g: "UBound(nomeArray)" -- l'argomento e' il NOME di un
		// array, mai un valore normale (un array non e' affatto una
		// variabile scalare in fVars, vedi VbaEnvironment.h): va
		// intercettato PRIMA del ciclo generico di valutazione degli
		// argomenti sotto, stesso principio di "Range"/"Cells" appena
		// sopra. Solo un array a UNA dimensione in questo sottoinsieme
		// (Option Base 0 implicito, come ogni array di questo motore):
		// "UBound" con un secondo argomento per l'asse (il vero VBA
		// multidimensionale) non e' supportato, nessuna macro reale
		// controllata finora ne ha bisogno.
		if (name == "ubound" && expr->children.size() == 1
			&& expr->children[0]->kind == vbaExprVariable
			&& fEnv->IsArray(expr->children[0]->name))
		{
			std::vector<int> dims;
			if (!fEnv->GetArrayDims(expr->children[0]->name, &dims) || dims.empty())
			{
				SetError("\"UBound\" su un array non ancora dimensionato (manca un \"ReDim\")",
					expr->line);
				return Value(0.0);
			}
			if (dims.size() != 1)
			{
				SetError("\"UBound\" su un array multidimensionale non e' supportato in "
					"questo sottoinsieme (serve un secondo argomento per indicare l'asse, "
					"qui assente)", expr->line);
				return Value(0.0);
			}
			return Value((double)(dims[0] - 1));
		}

		std::vector<Value> args;
		for (size_t i = 0; i < expr->children.size(); i++)
		{
			args.push_back(EvalNode(expr->children[i].get()));
			if (fFailed)
				return Value(0.0);
		}

		// Fase 6c: "nomeArray(indici)" in lettura -- stesso controllo
		// di IsArray usato in scrittura (VbaExecutor.cpp), PRIMA di
		// qualunque funzione predefinita o Sub/Function sorella, cosi'
		// un array dichiarato con "Dim" ha sempre la precedenza sul
		// proprio nome.
		if (fEnv->IsArray(expr->name))
		{
			std::vector<int> indices;
			for (size_t i = 0; i < args.size(); i++)
				indices.push_back((int)VbaToNumber(args[i]));
			const Value* element = fEnv->GetArrayElement(expr->name, indices);
			if (!element)
			{
				SetError("indice fuori dai limiti dell'array \"" + expr->name + "\"", expr->line);
				return Value(0.0);
			}
			return *element;
		}

		// Fase 6g: "LOF(fileNumber)" -- la lunghezza (in byte) di un
		// file GIA' aperto con "Open" (VbaExecutor.cpp), usata dal vero
		// macro trigger per dimensionare l'array di destinazione con
		// "ReDim rom(0 To LOF(file) - 1)" prima del "Get" che segue.
		if (name == "lof" && args.size() == 1)
		{
			const VbaOpenFile* file = fEnv->FindOpenFile((int)VbaToNumber(args[0]));
			if (!file)
			{
				SetError("\"LOF\" su un numero di file non aperto", expr->line);
				return Value(0.0);
			}
			return Value((double)file->data.size());
		}

		// Fase 6g: "MsgBox espressione" -- no-op deliberato (l'argomento
		// e' comunque gia' stato valutato sopra, cosi' un eventuale
		// errore al suo interno si propaga normalmente), stesso
		// principio di ".Select"/"DoEvents": nessuna vera finestra di
		// dialogo a questo livello dell'interprete. Vedi ParseMsgBox in
		// VbaStmtParser.cpp sul perche' questa e' una forma dedicata,
		// non una chiamata-istruzione generica.
		if (name == "msgbox")
			return Value(0.0);

		if (name == "len")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value((double)VbaToText(args[0]).size());
		}
		if (name == "left")
		{
			if (!CheckArgCount(expr, args, 2)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			long long n = (long long)VbaToNumber(args[1]);
			if (n < 0) n = 0;
			if ((size_t)n > s.size()) n = (long long)s.size();
			return Value(s.substr(0, (size_t)n).c_str());
		}
		if (name == "right")
		{
			if (!CheckArgCount(expr, args, 2)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			long long n = (long long)VbaToNumber(args[1]);
			if (n < 0) n = 0;
			if ((size_t)n > s.size()) n = (long long)s.size();
			return Value(s.substr(s.size() - (size_t)n).c_str());
		}
		if (name == "mid")
		{
			if (args.size() != 2 && args.size() != 3)
			{
				SetError("Mid richiede 2 o 3 argomenti", expr->line);
				return Value(0.0);
			}
			std::string s = VbaToText(args[0]);
			long long start = (long long)VbaToNumber(args[1]); // Mid e' 1-based in VBA
			if (start < 1) start = 1;
			if ((size_t)start > s.size())
				return Value("");
			size_t startIdx = (size_t)(start - 1);
			size_t len = args.size() == 3 ? (size_t)VbaToNumber(args[2]) : (s.size() - startIdx);
			if (startIdx + len > s.size())
				len = s.size() - startIdx;
			return Value(s.substr(startIdx, len).c_str());
		}
		if (name == "ucase")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			for (size_t i = 0; i < s.size(); i++)
				s[i] = (char)toupper((unsigned char)s[i]);
			return Value(s.c_str());
		}
		if (name == "lcase")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			for (size_t i = 0; i < s.size(); i++)
				s[i] = (char)tolower((unsigned char)s[i]);
			return Value(s.c_str());
		}
		if (name == "trim")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			std::string s = VbaToText(args[0]);
			size_t start = s.find_first_not_of(" \t");
			size_t end = s.find_last_not_of(" \t");
			if (start == std::string::npos)
				return Value("");
			return Value(s.substr(start, end - start + 1).c_str());
		}
		if (name == "abs")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(fabs(VbaToNumber(args[0])));
		}
		if (name == "int")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(floor(VbaToNumber(args[0]))); // Int() di VBA arrotonda verso il basso, non tronca
		}
		if (name == "round")
		{
			if (args.size() != 1 && args.size() != 2)
			{
				SetError("Round richiede 1 o 2 argomenti", expr->line);
				return Value(0.0);
			}
			double d = VbaToNumber(args[0]);
			int digits = args.size() == 2 ? (int)VbaToNumber(args[1]) : 0;
			double factor = pow(10.0, digits);
			return Value(floor(d * factor + 0.5) / factor);
		}
		if (name == "hex")
		{
			// Fase 6g (seguito): restituisce una STRINGA esadecimale
			// (maiuscola, senza zeri iniziali), come il vero VBA Hex() --
			// usata dal vero sorgente chip-8-vba solo nei messaggi di
			// errore ("Nessuna operazione " & Hex(n) & ...). Un numero
			// negativo si legge a 32 bit (come un vero Long del vero VBA
			// farebbe in complemento a due), non con un segno meno
			// davanti -- nessun uso reale controllato finora lo richiede
			// comunque, essendo sempre applicato a maschere di bit gia'
			// non negative.
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			long long v = (long long)VbaToNumber(args[0]);
			unsigned long u = (unsigned long)(v & 0xFFFFFFFFLL);
			char buf[16];
			snprintf(buf, sizeof(buf), "%lX", u);
			return Value(buf);
		}
		if (name == "rnd")
		{
			// Fase 6g (seguito): solo la forma SENZA argomenti (l'unica
			// usata dal vero sorgente chip-8-vba, nell'opcode CXNN) --
			// un numero pseudo-casuale in [0,1), stesso generatore C
			// standard di "Randomize" (EvalVariable sotto), nessuna
			// pretesa di qualita' statistica superiore a quella di
			// rand(): sufficiente per un CHIP-8 che non ha bisogno di
			// altro.
			return Value((double)rand() / ((double)RAND_MAX + 1.0));
		}

		// ---- Fase 5c: funzioni predefinite comuni nelle macro VBA reali ----

		if (name == "isempty")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			// Caso interessante: un intervallo (Range/Cells) bare, es.
			// "IsEmpty(Range(\"A1\"))" -- ancora un Value eRangeData a
			// questo punto (args[0] non e' passato per ".Value", quindi
			// nessuna coercizione vuoto->0 e' ancora avvenuta), permette
			// di distinguere una cella VERAMENTE vuota (eNoData) da una
			// che contiene letteralmente 0. LIMITE ONESTO: "IsEmpty(
			// Range(\"A1\").Value)" (CON ".Value" esplicito) non puo'
			// distinguerle, perche' ".Value" ha gia' applicato quella
			// stessa coercizione prima che IsEmpty vedesse il risultato
			// -- scrivere la forma senza ".Value" per rilevare davvero
			// una cella vuota, lo stesso idioma comune in una macro
			// registrata. Una variabile normale non e' mai "vuota" in
			// questo sottoinsieme (Dim/Set la inizializzano sempre a
			// 0.0): restituisce sempre False in quel caso.
			if (args[0].fType == eRangeData)
			{
				range r = (range)args[0];
				if (r.TopLeft() != r.BotRight())
				{
					SetError("IsEmpty su un intervallo di piu' celle non e' supportato in "
						"questa fase", expr->line);
					return Value(0.0);
				}
				CContainer* doc = args[0].fRangeContainer;
				if (!doc)
				{
					SetError("nessun foglio di calcolo collegato a questa esecuzione VBA",
						expr->line);
					return Value(0.0);
				}
				Value cellValue;
				bool hasValue = doc->GetValue(r.TopLeft(), cellValue);
				return Value(!hasValue || cellValue.fType == eNoData);
			}
			return Value(false);
		}
		if (name == "isnumeric")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			if (args[0].fType == eNumData || args[0].fType == eBoolData)
				return Value(true);
			if (args[0].fType != eTextData)
				return Value(false);
			return Value(IsFullyNumericText(VbaToText(args[0])));
		}
		if (name == "cstr")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(VbaToText(args[0]).c_str());
		}
		if (name == "cint" || name == "clng")
		{
			// Arrotonda (stessa regola "metà in su" di Round, non il
			// "banker's rounding" del vero VBA) poi troncaa un intero
			// -- nessun controllo di overflow (CInt reale e' a 16 bit,
			// CLng a 32: qui Value e' sempre un double, non esiste un
			// vero tipo intero da far traboccare).
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			double d = VbaToNumber(args[0]);
			return Value(floor(d + 0.5));
		}
		if (name == "cbyte")
		{
			// Fase 6g (seguito): stessa regola di arrotondamento di
			// CInt/CLng sopra, poi un modulo 256 per restare
			// nell'intervallo di un vero Byte (0-255) -- nessun
			// controllo di overflow reale come il vero VBA fuori da
			// quell'intervallo: l'unico uso controllato (chip-8-vba)
			// chiama sempre CByte su un valore GIA' portato in [0,255]
			// con un "Mod 256" esplicito prima.
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			double d = VbaToNumber(args[0]);
			long long v = (long long)floor(d + 0.5);
			v = ((v % 256) + 256) % 256;
			return Value((double)v);
		}
		if (name == "cdbl")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(VbaToNumber(args[0]));
		}
		if (name == "cbool")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(VbaToBool(args[0]));
		}
		if (name == "val")
		{
			// A differenza di CDbl, Val legge solo il PREFISSO numerico
			// iniziale di un testo e si ferma al primo carattere che
			// non riconosce, senza errore (Val("123abc") = 123,
			// Val("abc") = 0) -- esattamente il comportamento di atof,
			// gia' usato da VbaToNumber per il caso testo.
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			return Value(VbaToNumber(Value(VbaToText(args[0]).c_str())));
		}
		if (name == "space")
		{
			if (!CheckArgCount(expr, args, 1)) return Value(0.0);
			long long n = (long long)VbaToNumber(args[0]);
			if (n < 0) n = 0;
			return Value(std::string((size_t)n, ' ').c_str());
		}
		if (name == "instr")
		{
			// Solo le forme a 2 argomenti ("InStr(stringa1, stringa2)")
			// e a 3 ("InStr(inizio, stringa1, stringa2)", inizio 1-based
			// come in VBA) -- niente quarto argomento "compare", sempre
			// un confronto sensibile a maiuscole/minuscole.
			if (args.size() != 2 && args.size() != 3)
			{
				SetError("InStr richiede 2 argomenti (stringa1, stringa2) o 3 (inizio, "
					"stringa1, stringa2)", expr->line);
				return Value(0.0);
			}
			std::string haystack = VbaToText(args[args.size() == 3 ? 1 : 0]);
			std::string needle = VbaToText(args[args.size() == 3 ? 2 : 1]);
			size_t start = 0;
			if (args.size() == 3)
			{
				long long startArg = (long long)VbaToNumber(args[0]);
				if (startArg < 1) startArg = 1;
				start = (size_t)(startArg - 1);
			}
			if (start > haystack.size())
				return Value(0.0);
			size_t pos = haystack.find(needle, start);
			return Value(pos == std::string::npos ? 0.0 : (double)(pos + 1)); // 1-based come VBA
		}
		if (name == "replace")
		{
			// Solo la forma a 3 argomenti: sostituisce SEMPRE tutte le
			// occorrenze (il vero VBA lo fa di default quando "count" e'
			// omesso) -- niente "start"/"count"/"compare" facoltativi.
			if (!CheckArgCount(expr, args, 3)) return Value(0.0);
			std::string text = VbaToText(args[0]);
			std::string find = VbaToText(args[1]);
			std::string replacement = VbaToText(args[2]);
			if (find.empty())
				return Value(text.c_str()); // niente da sostituire, mai un ciclo infinito
			std::string out;
			size_t pos = 0;
			for (;;)
			{
				size_t next = text.find(find, pos);
				if (next == std::string::npos)
				{
					out += text.substr(pos);
					break;
				}
				out += text.substr(pos, next - pos);
				out += replacement;
				pos = next + find.size();
			}
			return Value(out.c_str());
		}
		if (name == "strcomp")
		{
			// Solo la forma a 2 argomenti -- sempre un confronto come
			// testo (a differenza di VbaCompareValues, che sceglie fra
			// numerico e testuale in base al tipo: StrComp confronta
			// SEMPRE come testo, anche se entrambi gli argomenti
			// capitano a essere numeri).
			if (!CheckArgCount(expr, args, 2)) return Value(0.0);
			int cmp = VbaToText(args[0]).compare(VbaToText(args[1]));
			return Value((double)((cmp < 0) ? -1 : (cmp > 0) ? 1 : 0));
		}

		// Fase 6a: non e' una funzione predefinita -- prova a
		// risolverla come chiamata a una Sub sorella, PRIMA nello
		// stesso modulo (es. "DrawSprite(a, x, y, n)" dentro un'altra
		// Sub), poi -- se non trovata li' -- in ogni ALTRO modulo dello
		// stesso progetto (FindVbaSubAcrossModules sopra, chiude il gap
		// esplicitamente noto di questa stessa Fase: "una Sub di un
		// modulo diverso e' un gap separato"). "args" e' gia' valutato
		// sopra, con le stesse regole di qualunque altra chiamata.
		// Richiede che l'esecuzione sia legata a un modulo vero
		// (VbaEnvironment::SetModuleTokens, collegato da
		// ExecuteVbaMacro) -- fEnv->GetModuleTokens() e' NULL nei test
		// che costruiscono una Sub a mano, dove questo ramo
		// semplicemente non si applica e si cade nell'errore "funzione
		// sconosciuta" sotto, come prima di questa fase.
		if (fEnv->GetModuleTokens())
		{
			std::unique_ptr<VbaSub> callee;
			std::vector<VbaToken> scratchTokens; // vedi il commento su FindVbaSubAcrossModules
			const std::vector<VbaToken>* calleeTokens =
				FindVbaSubAcrossModules(fEnv, expr->name, &callee, &scratchTokens);
			if (calleeTokens)
			{
				if (callee->params.size() != args.size())
				{
					char buf[160];
					snprintf(buf, sizeof(buf), "\"%s\" richiede %zu argomento/i, ricevuti %zu",
						expr->name.c_str(), callee->params.size(), args.size());
					SetError(buf, expr->line);
					return Value(0.0);
				}
				// Guardia anti-ricorsione infinita: senza questo, una Sub
				// che richiama se stessa (direttamente o tramite altre)
				// senza una condizione di uscita esauribbe lo stack C++
				// nativo invece di produrre un errore di runtime pulito.
				const int kMaxVbaCallDepth = 200;
				if (fEnv->GetCallDepth() >= kMaxVbaCallDepth)
				{
					SetError("troppi livelli di chiamata annidata tra Sub "
						"(possibile ricorsione infinita)", expr->line);
					return Value(0.0);
				}
				VbaEnvironment childEnv;
				childEnv.SetActiveDocument(fEnv->GetActiveDocument());
				// "calleeTokens" (non sempre fEnv->GetModuleTokens()):
				// se "callee" viene da un modulo DIVERSO, una SUA
				// eventuale chiamata sorella deve risolvere nel SUO
				// modulo, mai in quello del chiamante originale.
				childEnv.SetModuleTokens(calleeTokens);
				childEnv.SetAllModules(fEnv->GetAllModules());
				childEnv.SetCallDepth(fEnv->GetCallDepth() + 1);
				// Fase 6d: il registro dei Type e' per-modulo nel vero
				// VBA, ma qui si propaga sempre quello del CHIAMANTE --
				// corretto per una chiamata sorella nello STESSO
				// modulo (il caso comune), un limite onesto non ancora
				// risolto per una Sub di un modulo DIVERSO coi suoi
				// propri Type: fuori scopo per questa chiusura del gap
				// (che riguarda la RISOLUZIONE del nome, non il
				// modello a oggetti Type per modulo).
				childEnv.SetTypeRegistry(fEnv->GetTypeRegistry()); // Fase 6d
				// Senza questo, una Sub/Function sorella non vedrebbe
				// NESSUNA variabile/Const/struct di MODULO (es. "ThisVM"
				// nel vero chip-8-vba) -- un bug reale trovato eseguendo
				// dal vivo "Step" (chiama "VMGetShort" come sorella, che
				// legge "ThisVM.Memory"): propaga lo STESSO
				// fModuleEnv del chiamante (lo stesso persistentModuleEnv
				// in cima alla catena, mai un nuovo livello di fallback),
				// cosi' "Inner" vede/modifica le STESSE variabili di
				// modulo di "Outer", come nel vero VBA.
				childEnv.SetModuleEnvironment(fEnv->GetModuleEnvironment());
				// Bug reale trovato implementando la scrittura di
				// "NomeControllo.Caption" (controlli ActiveX): senza
				// questo, QUALUNQUE gancio legato a "hostContext" (vedi
				// VbaOnTimeFn/VbaSetControlCaptionFn in VbaEvaluator.h)
				// smette di funzionare non appena invocato da una Sub
				// chiamata come SORELLA invece che come macro di primo
				// livello -- es. "CommandButton2_Click" che chiama
				// "VMSetRunning" come sorella, la quale scrive
				// ".Caption": "childEnv" partiva sempre con
				// "fHostContext" a NULL (il costruttore di default),
				// mai propagato dal chiamante come gli altri campi
				// sopra.
				childEnv.SetHostContext(fEnv->GetHostContext());
				for (size_t i = 0; i < args.size(); i++)
				{
					// SOLO ByVal in questo sottoinsieme (vedi VbaAst.h):
					// "args[i]" e' una COPIA del valore dell'argomento,
					// nessun aliasing con la variabile del chiamante.
					childEnv.Declare(callee->params[i]);
					childEnv.Set(callee->params[i], args[i]);
				}
				VbaRuntimeError callError;
				if (!ExecuteVbaSub(callee.get(), &childEnv, &callError))
				{
					SetError(callError.message, callError.line ? callError.line : expr->line);
					return Value(0.0);
				}
				// Fase 6b: una Function ha un valore di ritorno -- la
				// stessa variabile col nome della Function, dichiarata da
				// ExecuteVbaSub PRIMA di eseguire il corpo (vedi
				// VbaExecutor.cpp), quindi sempre presente qui. Una Sub
				// invece non ha alcun valore (Value(0.0) scartato dal
				// chiamante quando usata come istruzione nuda).
				if (callee->isFunction)
				{
					const Value* retVal = childEnv.Find(callee->name);
					return retVal ? *retVal : Value(0.0);
				}
				return Value(0.0);
			}
		}

		SetError("funzione sconosciuta \"" + expr->name + "\" "
			"(fuori dal sottoinsieme supportato, o un nome dell'oggetto Excel "
			"non ancora collegato -- vedi Fase 3c)", expr->line);
		return Value(0.0);
	}

	bool CheckArgCount(const VbaExpr* expr, const std::vector<Value>& args, size_t expected)
	{
		if (args.size() == expected)
			return true;
		char buf[128];
		snprintf(buf, sizeof(buf), "%s richiede esattamente %zu argomento/i, ricevuti %zu",
			expr->name.c_str(), expected, args.size());
		SetError(buf, expr->line);
		return false;
	}

	// Fase 3c, modello a oggetti Excel minimo -- vedi il commento in
	// cima al file.

	CContainer* RequireActiveDocument(int line)
	{
		CContainer* doc = fEnv->GetActiveDocument();
		if (!doc)
			SetError("nessun foglio di calcolo collegato a questa esecuzione VBA", line);
		return doc;
	}

	Value EvalRangeFunction(const VbaExpr* expr)
	{
		if (expr->children.size() != 1)
		{
			SetError("Range richiede esattamente 1 argomento (un testo come \"A1\" o \"A1:B2\")",
				expr->line);
			return Value(0.0);
		}
		Value arg = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		std::string text = VbaToText(arg);
		range r;
		if (!ParseSimpleRangeRef(text, &r))
		{
			SetError("riferimento di intervallo non valido: \"" + text + "\"", expr->line);
			return Value(0.0);
		}
		CContainer* doc = RequireActiveDocument(expr->line);
		if (fFailed)
			return Value(0.0);
		return MakeRangeValue(r, doc);
	}

	Value EvalCellsFunction(const VbaExpr* expr)
	{
		if (expr->children.size() != 2)
		{
			SetError("Cells richiede esattamente 2 argomenti (riga, colonna)", expr->line);
			return Value(0.0);
		}
		Value rowValue = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		Value colValue = EvalNode(expr->children[1].get());
		if (fFailed)
			return Value(0.0);
		int row = (int)VbaToNumber(rowValue);
		int col = (int)VbaToNumber(colValue);
		cell c(col, row);
		if (!c.IsValid())
		{
			SetError("Cells(riga, colonna) e' fuori dai limiti del foglio di calcolo", expr->line);
			return Value(0.0);
		}
		CContainer* doc = RequireActiveDocument(expr->line);
		if (fFailed)
			return Value(0.0);
		return MakeRangeValue(range(c.h, c.v, c.h, c.v), doc);
	}

	// "target.Offset(righe, colonne)": sposta un intervallo esistente
	// di un numero di righe/colonne, conservando larghezza e altezza
	// -- stesso comportamento di range::OffsetBy, gia' usato altrove
	// nel motore (es. Fase 15, riferimenti di formula).
	Value EvalOffsetCall(const VbaExpr* expr)
	{
		Value targetValue = EvalNode(expr->target.get());
		if (fFailed)
			return Value(0.0);
		if (targetValue.fType != eRangeData)
		{
			SetError("Offset richiede un intervallo (Range/Cells), non un valore semplice", expr->line);
			return Value(0.0);
		}
		if (expr->children.size() != 2)
		{
			SetError("Offset richiede esattamente 2 argomenti (righe, colonne)", expr->line);
			return Value(0.0);
		}
		Value rowOffsetValue = EvalNode(expr->children[0].get());
		if (fFailed)
			return Value(0.0);
		Value colOffsetValue = EvalNode(expr->children[1].get());
		if (fFailed)
			return Value(0.0);

		range r = (range)targetValue;
		r.OffsetBy((int)VbaToNumber(colOffsetValue), (int)VbaToNumber(rowOffsetValue));
		if (!r.IsValid())
		{
			SetError("Offset porta fuori dai limiti del foglio di calcolo", expr->line);
			return Value(0.0);
		}
		return MakeRangeValue(r, targetValue.fRangeContainer);
	}

	Value EvalPropertyAccess(const VbaExpr* expr)
	{
		// Fase 6d: un accesso a un campo di struct (es. "ThisVM.PC", o
		// "ThisVM.Screen(x,y).value") DEVE essere controllato PRIMA del
		// caso speciale ".value" sotto, che altrimenti lo tratterebbe
		// come l'accessore ".Value" di un Range.
		std::string structErr;
		Value* fieldSlot = ResolveStructScalarField(expr, fEnv, &structErr);
		if (fieldSlot)
			return *fieldSlot;
		if (!structErr.empty())
		{
			SetError(structErr, expr->line);
			return Value(0.0);
		}

		std::string name = Lowercase(expr->name);

		// Fase 6f: "Range(...).Interior.Color"/".ColorIndex" -- DUE
		// livelli di PropertyAccess (diverso da ".value", un livello
		// solo), riconosciuti direttamente qui senza che "Interior"
		// sia a sua volta una proprieta' valutabile da sola.
		if ((name == "color" || name == "colorindex")
			&& expr->target->kind == vbaExprPropertyAccess
			&& Lowercase(expr->target->name) == "interior")
		{
			Value rangeValue = EvalNode(expr->target->target.get());
			if (fFailed)
				return Value(0.0);
			Value result;
			VbaRuntimeError err;
			if (!VbaGetInteriorColor(rangeValue, name == "colorindex", &result, &err, expr->line))
			{
				SetError(err.message, err.line);
				return Value(0.0);
			}
			return result;
		}

		// ".Font.Bold"/".Font.Italic"/".Font.Size"/".Font.Name" -- stesso
		// principio esatto del blocco Interior sopra, su "Font" invece
		// di "Interior".
		if ((name == "bold" || name == "italic" || name == "size" || name == "name")
			&& expr->target->kind == vbaExprPropertyAccess
			&& Lowercase(expr->target->name) == "font")
		{
			Value rangeValue = EvalNode(expr->target->target.get());
			if (fFailed)
				return Value(0.0);
			Value result;
			VbaRuntimeError err;
			if (!VbaGetFontProperty(rangeValue, name, &result, &err, expr->line))
			{
				SetError(err.message, err.line);
				return Value(0.0);
			}
			return result;
		}

		if (name == "select")
		{
			// No-op deliberato: nessun concetto di selezione a questo
			// livello (motore puro, nessuna UI collegata) -- vedi il
			// commento in VbaEvaluator.h. Si valuta comunque "target"
			// per propagare un eventuale errore (es. un riferimento
			// fuori dai limiti), ma il risultato si scarta sempre:
			// "Select" si usa solo come istruzione nuda.
			EvalNode(expr->target.get());
			return Value(0.0);
		}

		// Fase 6g: "Application.GetOpenFilename" -- il vero macro
		// trigger (chip-8-vba) lo chiama cosi' (nessuna parentesi,
		// nessun argomento: la sintassi del vero VBA accetterebbe
		// anche filtro/titolo, fuori scopo qui, nessuna macro reale
		// controllata finora ne ha bisogno), assegnando il risultato a
		// una variabile e confrontandola con "False" per riconoscere
		// l'annullamento -- vedi VbaCompareValues sul perche' quel
		// confronto funziona GIA' correttamente sia nel caso testo
		// (percorso scelto) sia nel caso booleano (annullato), senza
		// bisogno di nessuna logica speciale qui.
		//
		// Delega a un vero selettore nativo via VbaSetOpenFileDialogHook
		// (ui/MainWindow.cpp, BFilePanel) -- NULL (nessuna UI collegata,
		// es. nei test) si comporta come un annullamento, mai un
		// crash. Solo un percorso DAVVERO scelto dall'utente si
		// approva per un "Open" successivo nella stessa esecuzione
		// (VbaEnvironment::ApproveOpenFilePath) -- la decisione di
		// sicurezza di Fase 6g, vedi ROADMAP.md.
		if (name == "getopenfilename" && expr->target->kind == vbaExprVariable
			&& Lowercase(expr->target->name) == "application")
		{
			std::string path;
			if (!gOpenFileDialogHook || !gOpenFileDialogHook(&path))
				return Value(false);
			fEnv->ApproveOpenFilePath(path);
			return Value(path.c_str());
		}
		if (name == "value")
		{
			Value target = EvalNode(expr->target.get());
			if (fFailed)
				return Value(0.0);
			Value result;
			VbaRuntimeError err;
			if (!VbaReadRangeValue(target, &result, &err, expr->line))
			{
				SetError(err.message, err.line);
				return Value(0.0);
			}
			return result;
		}
		SetError("la proprieta'/il metodo \"" + expr->name + "\" del modello a oggetti Excel "
			"non e' ancora supportato in questa fase dell'interprete", expr->line);
		return Value(0.0);
	}
};

} // namespace

bool EvaluateVbaExpression(const VbaExpr* expr, VbaEnvironment* env, Value* outResult,
	VbaRuntimeError* outError)
{
	Evaluator eval(env, outError);
	return eval.Evaluate(expr, outResult);
}
