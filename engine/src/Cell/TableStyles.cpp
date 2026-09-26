/*
	TableStyles.cpp

	Vedi TableStyles.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "TableStyles.h"

#include <cstring>

#include "Cell.h"
#include "CellStyle.h"
#include "Container.h"

const TableStyleDef kTableStyles[] = {
	{ "TableStyleLight1",   { 242, 242, 242, 255 } }, // grigio chiaro, uguale al default di sempre
	{ "TableStyleLight9",   { 226, 239, 218, 255 } }, // verde chiaro
	{ "TableStyleLight21",  { 242, 242, 242, 255 } }, // grigio, variante chiara
	{ "TableStyleMedium2",  { 197, 217, 241, 255 } }, // blu -- lo stile di default reale di Excel per una nuova tabella
	{ "TableStyleMedium3",  { 253, 233, 217, 255 } }, // arancione chiaro
	{ "TableStyleMedium4",  { 255, 242, 204, 255 } }, // giallo chiaro
	{ "TableStyleMedium6",  { 221, 235, 247, 255 } }, // azzurro
	{ "TableStyleMedium9",  { 247, 224, 164, 255 } }, // oro/ambra
};
const int kTableStyleCount = sizeof(kTableStyles) / sizeof(kTableStyles[0]);

bool FindTableStyleBandColor(const std::string& name, rgb_color* outColor)
{
	if (name.empty())
		return false;
	for (int i = 0; i < kTableStyleCount; i++)
	{
		if (name == kTableStyles[i].name)
		{
			*outColor = kTableStyles[i].bandColor;
			return true;
		}
	}
	return false;
}

static bool ColorsEqual(rgb_color a, rgb_color b)
{
	return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

// Vedi il commento in TableStyles.h. Identica alla ApplyTableBanding che
// esisteva solo dentro XlsxTranslator.cpp prima che questa funzione
// diventasse condivisa (Tier 4 "named table styles", passo UI): "solo
// alle celle che non hanno gia' un colore di sfondo esplicito" evita di
// coprire uno sfondo scelto apposta dall'utente, sia in importazione
// XLSX sia -- il motivo per cui questa funzione e' stata spostata qui --
// quando l'utente cambia lo stile di una tabella gia' esistente da menu:
// riapplicare la banda non deve mai toccare una cella colorata a mano.
void ApplyTableStyleBanding(CContainer* doc, const range& tableRange,
	int totalsRowCount, const std::string& styleName)
{
	rgb_color bandColor = { 242, 242, 242, 255 };
	FindTableStyleBandColor(styleName, &bandColor);
	CellStyle defaultStyle;

	int lastDataRow = tableRange.bottom - (totalsRowCount > 0 ? totalsRowCount : 0);
	for (int row = tableRange.top + 1; row <= lastDataRow; row++)
	{
		// Stessa logica "indice 0-based della riga dati" gia' verificata
		// dal test XLSX originale -- vedi il commento gemello che c'era
		// in XlsxTranslator.cpp prima dello spostamento.
		if ((row - tableRange.top - 1) % 2 != 0)
			continue;

		for (int col = tableRange.left; col <= tableRange.right; col++)
		{
			cell c(col, row);
			CellStyle cs;
			doc->GetCellStyle(c, cs);
			if (!ColorsEqual(cs.fLowColor, defaultStyle.fLowColor))
				continue;
			cs.fLowColor = bandColor;
			doc->SetCellStyle(c, cs);
		}
	}
}
