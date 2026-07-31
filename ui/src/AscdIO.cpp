/*
	AscdIO.cpp

	Vedi AscdIO.h.
*/

#include "AscdIO.h"

#include <cstring>
#include <fcntl.h>

#include "Cell.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
static const int32 kASCDVersion = 1;
static const char kASCDBookMagic[4] = { 'A', 'S', 'C', 'B' };

bool IsASCDFile(BPositionIO* source)
{
	off_t pos = source->Position();

	char magic[4];
	bool isAscd = source->Read(magic, 4) == 4 && memcmp(magic, kASCDMagic, 4) == 0;

	source->Seek(pos, SEEK_SET);
	return isAscd;
}

status_t SaveASCD(CContainer* doc, BPositionIO* dest,
	const std::vector<ChartObject>* charts)
{
	// Range completo invece dei limiti di GetBounds: una cella con
	// formula non ancora calcolata (mType eNoData, es. appena
	// impostata via TryToParseString/SetCellFormula senza un CalcCell
	// esplicito) verrebbe esclusa dai limiti calcolati da GetBounds, e
	// se e' anche la cella piu' a destra/in basso del foglio
	// sparirebbe del tutto dal file salvato. Stesso ragionamento di
	// RecalculateAll sotto.
	int32 count = 0;
	CCellIterator counter(doc, NULL);
	cell c;
	while (counter.NextExisting(c))
		count++;

	if (dest->Write(kASCDMagic, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kASCDVersion, sizeof(kASCDVersion)) != (ssize_t)sizeof(kASCDVersion))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;

	CCellIterator iter(doc, NULL);
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

	// Sezione grafici incorporati, in coda: opzionale, un file scritto
	// da una versione precedente di questo formato semplicemente non
	// ce l'ha (vedi il commento in LoadASCD sotto).
	int32 chartCount = charts ? (int32)charts->size() : 0;
	if (dest->Write(&chartCount, sizeof(chartCount)) != (ssize_t)sizeof(chartCount))
		return B_IO_ERROR;

	for (int32 i = 0; i < chartCount; i++)
	{
		const ChartObject& obj = (*charts)[i];
		int16 left = obj.dataRange.left, top = obj.dataRange.top;
		int16 right = obj.dataRange.right, bottom = obj.dataRange.bottom;
		float frame[4] = { obj.frame.left, obj.frame.top,
			obj.frame.right, obj.frame.bottom };

		if (dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
			|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
			|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right)
			|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
			|| dest->Write(frame, sizeof(frame)) != (ssize_t)sizeof(frame))
			return B_IO_ERROR;
	}

	return B_OK;
}

status_t LoadASCD(BPositionIO* source, CContainer* doc,
	std::vector<ChartObject>* charts)
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

	// Sezione grafici incorporati, in coda al formato: puo' non
	// esserci affatto (file scritto prima che questa sezione
	// esistesse). Read() restituisce 0 se lo stream e' gia' finito
	// esattamente li' (formato vecchio, nessun grafico: non un
	// errore); un valore diverso da 0 ma minore di sizeof(chartCount)
	// e' invece un file davvero troncato/corrotto.
	if (charts)
	{
		charts->clear();
		int32 chartCount = 0;
		ssize_t got = source->Read(&chartCount, sizeof(chartCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(chartCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < chartCount; i++)
			{
				int16 left, top, right, bottom;
				float frame[4];

				if (source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right)
					|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
					|| source->Read(frame, sizeof(frame)) != (ssize_t)sizeof(frame))
					return B_BAD_DATA;

				ChartObject obj;
				obj.dataRange.Set(left, top, right, bottom);
				obj.frame.Set(frame[0], frame[1], frame[2], frame[3]);
				charts->push_back(obj);
			}
		}
	}

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
//
// L'iteratore usa il range completo del foglio (non i limiti
// restituiti da GetBounds) perche' GetBounds esclude le celle con
// mType eNoData -- esattamente lo stato di una formula appena
// analizzata da TryToParseString e non ancora calcolata. Se quella
// cella e' anche la piu' a destra/in basso del foglio (nessun'altra
// cella "reale" oltre di lei), i limiti calcolati la escluderebbero e
// non verrebbe mai visitata da NextExisting, restando vuota per
// sempre -- bug reale anche nell'uso live (CommitEditing chiama
// RecalculateAll dopo ogni conferma: digitare una nuova formula
// nell'angolo in basso a destra del foglio la lascerebbe vuota).
// NextExisting resta comunque efficiente su un range pieno: salta
// direttamente da una cella esistente alla successiva tramite la
// mappa, senza scandire le celle vuote in mezzo.
void RecalculateAll(CContainer* doc)
{
	bool changed = true;
	int guard = 0;
	while (changed && guard < 50)
	{
		changed = false;
		CCellIterator iter(doc, NULL);
		cell c;
		while (iter.NextExisting(c))
		{
			if (doc->CalcCell(c))
				changed = true;
		}
		guard++;
	}
}

bool IsASCDBookFile(BPositionIO* source)
{
	off_t pos = source->Position();

	char magic[4];
	bool isBook = source->Read(magic, 4) == 4 && memcmp(magic, kASCDBookMagic, 4) == 0;

	source->Seek(pos, SEEK_SET);
	return isBook;
}

status_t SaveASCDBook(const std::vector<AscdSheet>& sheets, BPositionIO* dest)
{
	if (dest->Write(kASCDBookMagic, 4) != 4)
		return B_IO_ERROR;

	int32 sheetCount = (int32)sheets.size();
	if (dest->Write(&sheetCount, sizeof(sheetCount)) != (ssize_t)sizeof(sheetCount))
		return B_IO_ERROR;

	for (int32 i = 0; i < sheetCount; i++)
	{
		const AscdSheet& sheet = sheets[i];

		int32 nameLen = sheet.name.Length();
		if (dest->Write(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
			return B_IO_ERROR;
		if (nameLen > 0 && dest->Write(sheet.name.String(), nameLen) != nameLen)
			return B_IO_ERROR;

		status_t err = SaveASCD(sheet.doc, dest, &sheet.charts);
		if (err != B_OK)
			return err;
	}

	return B_OK;
}

status_t LoadASCDBook(BPositionIO* source, std::vector<AscdSheet>* outSheets)
{
	outSheets->clear();

	char magic[4];
	if (source->Read(magic, 4) != 4)
		return B_BAD_DATA;
	if (memcmp(magic, kASCDBookMagic, 4) != 0)
		return B_BAD_DATA;

	int32 sheetCount;
	if (source->Read(&sheetCount, sizeof(sheetCount)) != (ssize_t)sizeof(sheetCount))
		return B_BAD_DATA;
	if (sheetCount < 0)
		return B_BAD_DATA;

	for (int32 i = 0; i < sheetCount; i++)
	{
		int32 nameLen;
		if (source->Read(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
			return B_BAD_DATA;

		char nameBuf[256];
		if (nameLen < 0 || nameLen >= (int32)sizeof(nameBuf))
			return B_BAD_DATA;
		if (nameLen > 0 && source->Read(nameBuf, nameLen) != nameLen)
			return B_BAD_DATA;
		nameBuf[nameLen] = 0;

		AscdSheet sheet;
		sheet.name = nameBuf;
		sheet.doc = new CContainer(NULL, NULL);

		status_t err = LoadASCD(source, sheet.doc, &sheet.charts);
		if (err != B_OK)
		{
			sheet.doc->Release();
			// Rilascia anche i fogli gia' letti prima di questo, per non
			// perdere memoria restituendo un errore a meta' lettura.
			for (size_t j = 0; j < outSheets->size(); j++)
				(*outSheets)[j].doc->Release();
			outSheets->clear();
			return err;
		}

		outSheets->push_back(sheet);
	}

	return B_OK;
}
