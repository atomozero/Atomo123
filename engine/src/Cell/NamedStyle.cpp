/*
	NamedStyle.cpp

	Vedi NamedStyle.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "NamedStyle.h"

#include <Font.h>

#include "FontMetrics.h"

ThemePalette::ThemePalette()
{
	// Colori "Office-like" scelti a occhio per essere ben distinguibili
	// l'uno dall'altro, NON estratti pixel per pixel da un vero tema
	// Excel -- stessa approssimazione dichiarata gia' presente per
	// TableStyles.h.
	colors[eThemeText] = rgb_color{ 0, 0, 0, 255 };
	colors[eThemeBackground] = rgb_color{ 255, 255, 255, 255 };
	colors[eThemeAccent1] = rgb_color{ 68, 114, 196, 255 };  // blu
	colors[eThemeAccent2] = rgb_color{ 237, 125, 49, 255 };  // arancione
	colors[eThemeAccent3] = rgb_color{ 165, 165, 165, 255 }; // grigio
	colors[eThemeAccent4] = rgb_color{ 255, 192, 0, 255 };   // giallo/oro
	colors[eThemeAccent5] = rgb_color{ 91, 155, 213, 255 };  // azzurro
	colors[eThemeAccent6] = rgb_color{ 112, 173, 71, 255 };  // verde
}

NamedStyleDef::NamedStyleDef()
	:
	builtIn(false),
	useThemeBackground(false),
	backgroundRole(eThemeBackground),
	backgroundColor(rgb_color{ 255, 255, 255, 255 }),
	useThemeText(false),
	textRole(eThemeText),
	textColor(rgb_color{ 0, 0, 0, 255 }),
	alignment(eAlignGeneral),
	bold(false),
	italic(false),
	underline(false)
{
}

CellStyle NamedStyleDef::Resolve(const CellStyle& base, const ThemePalette& palette) const
{
	CellStyle result = base;
	result.fLowColor = useThemeBackground ? palette.colors[backgroundRole] : backgroundColor;
	result.fHighColor = useThemeText ? palette.colors[textRole] : textColor;
	result.fAlignment = alignment;
	result.fUnderline = underline;

	// Stesso principio esatto di MainWindow::ToggleBold/ApplyFontToRange
	// (ui/src/MainWindow.cpp): CellStyle::fFont e' un indice in
	// gFontSizeTable che bada a famiglia/stile/dimensione insieme, non
	// un flag grassetto/corsivo a parte -- si preserva la famiglia/
	// dimensione di "base" e si ricalcola solo la stringa di stile.
	font_family family;
	font_style style;
	float size;
	gFontSizeTable.GetFontInfo(base.fFont, &family, &style, &size);

	const char* newStyle = (bold && italic) ? "Bold Italic"
		: bold ? "Bold"
		: italic ? "Italic"
		: "Regular";
	result.fFont = (int)gFontSizeTable.GetFontID(family, newStyle, size, result.fHighColor);

	return result;
}

// Elenco built-in v1 (Tier 4): sottoinsieme curato dell'elenco Excel
// reale (~50 voci), stesso principio di scope gia' dichiarato per
// TableStyles.h ("8 nomi riconosciuti, non l'elenco Excel completo").
// Costruito qui direttamente (non in una tabella dati a parte): un solo
// posto da tenere sincronizzato invece di due elenchi paralleli
// (nomi + definizioni) che potrebbero disallinearsi nel tempo.
void NamedStyleTable::ResetToBuiltIns()
{
	fStyles.clear();

	struct Builtin {
		const char* name;
		bool useThemeBg; ThemeColorRole bgRole; rgb_color bgColor;
		bool useThemeText; ThemeColorRole textRole; rgb_color textColor;
		bool bold; bool italic;
	};
	static const Builtin kBuiltins[] = {
		{ "Normale", false, eThemeBackground, { 255, 255, 255, 255 }, false, eThemeText, { 0, 0, 0, 255 }, false, false },
		{ "Buono", false, eThemeBackground, { 198, 239, 206, 255 }, false, eThemeText, { 0, 97, 0, 255 }, false, false },
		{ "Cattivo", false, eThemeBackground, { 255, 199, 206, 255 }, false, eThemeText, { 156, 0, 6, 255 }, false, false },
		{ "Neutro", false, eThemeBackground, { 255, 235, 156, 255 }, false, eThemeText, { 156, 101, 0, 255 }, false, false },
		{ "Nota", false, eThemeBackground, { 255, 255, 204, 255 }, false, eThemeText, { 0, 0, 0, 255 }, false, true },
		{ "Titolo", false, eThemeBackground, { 255, 255, 255, 255 }, true, eThemeAccent1, { 0, 0, 0, 255 }, true, false },
		{ "Intestazione 1", false, eThemeBackground, { 255, 255, 255, 255 }, true, eThemeAccent1, { 0, 0, 0, 255 }, true, false },
		{ "Intestazione 2", false, eThemeBackground, { 255, 255, 255, 255 }, true, eThemeAccent2, { 0, 0, 0, 255 }, true, false },
		{ "Intestazione 3", false, eThemeBackground, { 255, 255, 255, 255 }, true, eThemeAccent3, { 0, 0, 0, 255 }, true, false },
		{ "Intestazione 4", false, eThemeBackground, { 255, 255, 255, 255 }, false, eThemeText, { 0, 0, 0, 255 }, true, false },
		{ "Totale", false, eThemeBackground, { 242, 242, 242, 255 }, false, eThemeText, { 0, 0, 0, 255 }, true, false },
		{ "Accent1", true, eThemeAccent1, { 0, 0, 0, 255 }, false, eThemeText, { 255, 255, 255, 255 }, false, false },
		{ "Accent2", true, eThemeAccent2, { 0, 0, 0, 255 }, false, eThemeText, { 255, 255, 255, 255 }, false, false },
		{ "Accent3", true, eThemeAccent3, { 0, 0, 0, 255 }, false, eThemeText, { 255, 255, 255, 255 }, false, false },
		{ "Accent4", true, eThemeAccent4, { 0, 0, 0, 255 }, false, eThemeText, { 255, 255, 255, 255 }, false, false },
		{ "Accent5", true, eThemeAccent5, { 0, 0, 0, 255 }, false, eThemeText, { 255, 255, 255, 255 }, false, false },
		{ "Accent6", true, eThemeAccent6, { 0, 0, 0, 255 }, false, eThemeText, { 255, 255, 255, 255 }, false, false },
	};

	for (size_t i = 0; i < sizeof(kBuiltins) / sizeof(kBuiltins[0]); i++)
	{
		const Builtin& b = kBuiltins[i];
		NamedStyleDef def;
		def.name = b.name;
		def.builtIn = true;
		def.useThemeBackground = b.useThemeBg;
		def.backgroundRole = b.bgRole;
		def.backgroundColor = b.bgColor;
		def.useThemeText = b.useThemeText;
		def.textRole = b.textRole;
		def.textColor = b.textColor;
		def.alignment = eAlignGeneral;
		def.bold = b.bold;
		def.italic = b.italic;
		def.underline = false;

		Entry entry;
		entry.def = def;
		entry.removed = false;
		fStyles.push_back(entry);
	}
}

NamedStyleTable::NamedStyleTable()
{
	ResetToBuiltIns();
}

int NamedStyleTable::FindByName(const std::string& name) const
{
	for (size_t i = 0; i < fStyles.size(); i++)
	{
		if (!fStyles[i].removed && fStyles[i].def.name == name)
			return (int)i + 1;
	}
	return -1;
}

int NamedStyleTable::AddCustom(const NamedStyleDef& def)
{
	Entry entry;
	entry.def = def;
	entry.def.builtIn = false;
	entry.removed = false;
	fStyles.push_back(entry);
	return (int)fStyles.size();
}

bool NamedStyleTable::Redefine(int styleID, const NamedStyleDef& def)
{
	if (styleID <= 0 || (size_t)styleID > fStyles.size() || fStyles[styleID - 1].removed)
		return false;
	// Il nome e il flag builtIn restano quelli originali: ridefinire
	// uno stile ne cambia l'ASPETTO, non l'identita' -- stesso
	// principio del vero Excel (rinominare uno stile e' un'azione
	// separata, non prevista in questa fase).
	NamedStyleDef updated = def;
	updated.name = fStyles[styleID - 1].def.name;
	updated.builtIn = fStyles[styleID - 1].def.builtIn;
	fStyles[styleID - 1].def = updated;
	return true;
}

bool NamedStyleTable::Remove(int styleID)
{
	if (styleID <= 0 || (size_t)styleID > fStyles.size())
		return false;
	if (fStyles[styleID - 1].removed || fStyles[styleID - 1].def.builtIn)
		return false;
	fStyles[styleID - 1].removed = true;
	return true;
}

int NamedStyleTable::Count() const
{
	int count = 0;
	for (size_t i = 0; i < fStyles.size(); i++)
	{
		if (!fStyles[i].removed)
			count++;
	}
	return count;
}

int NamedStyleTable::IDAtIndex(int index) const
{
	int count = 0;
	for (size_t i = 0; i < fStyles.size(); i++)
	{
		if (fStyles[i].removed)
			continue;
		if (count == index)
			return (int)i + 1;
		count++;
	}
	return -1;
}

const NamedStyleDef* NamedStyleTable::Get(int styleID) const
{
	if (styleID <= 0 || (size_t)styleID > fStyles.size() || fStyles[styleID - 1].removed)
		return NULL;
	return &fStyles[styleID - 1].def;
}
