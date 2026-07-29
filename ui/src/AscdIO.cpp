/*
	AscdIO.cpp

	Vedi AscdIO.h.
*/

#include "AscdIO.h"

#include <cstring>

#include "Cell.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
static const int32 kASCDVersion = 1;

status_t SaveASCD(CContainer* doc, BPositionIO* dest)
{
	range bounds;
	doc->GetBounds(bounds);

	int32 count = 0;
	CCellIterator counter(doc, &bounds);
	cell c;
	while (counter.NextExisting(c))
		count++;

	if (dest->Write(kASCDMagic, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kASCDVersion, sizeof(kASCDVersion)) != (ssize_t)sizeof(kASCDVersion))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;

	CCellIterator iter(doc, &bounds);
	while (iter.NextExisting(c))
	{
		char text[512];
		doc->GetCellFormula(c, text, false);

		int16 row = c.v, col = c.h;
		int32 len = strlen(text);

		if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_IO_ERROR;
		if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_IO_ERROR;
		if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_IO_ERROR;
		if (len > 0 && dest->Write(text, len) != len)
			return B_IO_ERROR;
	}

	return B_OK;
}

status_t LoadASCD(BPositionIO* source, CContainer* doc)
{
	char magic[4];
	if (source->Read(magic, 4) != 4)
		return B_BAD_DATA;
	if (memcmp(magic, kASCDMagic, 4) != 0)
		return B_BAD_DATA;

	int32 version;
	if (source->Read(&version, sizeof(version)) != (ssize_t)sizeof(version))
		return B_BAD_DATA;
	if (version != kASCDVersion)
		return B_MISMATCHED_VALUES;

	int32 count;
	if (source->Read(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_BAD_DATA;

	for (int32 i = 0; i < count; i++)
	{
		int16 row, col;
		int32 len;

		if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_BAD_DATA;
		if (source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_BAD_DATA;
		if (source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_BAD_DATA;

		char text[512];
		if (len < 0 || len >= (int32)sizeof(text))
			return B_BAD_DATA;
		if (len > 0 && source->Read(text, len) != len)
			return B_BAD_DATA;
		text[len] = 0;

		cell c(col, row);
		try
		{
			TryToParseString(text, c, doc, true);
		}
		catch (...)
		{
			// Una singola cella corrotta non deve far fallire l'intero
			// caricamento: viene semplicemente saltata.
		}
	}

	RecalculateAll(doc);

	return B_OK;
}

// TryToParseString imposta la formula/il valore di ogni cella ma non
// la calcola: CFormula::Calculate legge i riferimenti ad altre celle
// con una semplice GetValue (non ricorsiva), quindi una cella che fa
// riferimento a un'altra cella non ancora calcolata leggerebbe un
// valore vuoto/NaN. Piu' passate su tutte le celle finche' nessuna
// cambia piu' valore propagano correttamente le dipendenze in
// qualunque ordine siano state inserite -- senza aver bisogno di un
// vero ordinamento topologico del grafo delle dipendenze. Limite di
// sicurezza sulle passate per non restare bloccati su un riferimento
// circolare.
void RecalculateAll(CContainer* doc)
{
	range bounds;
	doc->GetBounds(bounds);

	bool changed = true;
	int guard = 0;
	while (changed && guard < 50)
	{
		changed = false;
		CCellIterator iter(doc, &bounds);
		cell c;
		while (iter.NextExisting(c))
		{
			if (doc->CalcCell(c))
				changed = true;
		}
		guard++;
	}
}
