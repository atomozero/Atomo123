/*
	AscdIO.cpp

	Vedi AscdIO.h.
*/

#include "AscdIO.h"

#include <cstring>
#include <fcntl.h>

#include "Cell.h"
#include "CellStyle.h"
#include "Constants.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "FontMetrics.h"
#include "Formatter.h"

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

static status_t WriteColorEntry(BPositionIO* dest, rgb_color bg, rgb_color fg)
{
	uint8 buf[8] = { bg.red, bg.green, bg.blue, bg.alpha, fg.red, fg.green, fg.blue, fg.alpha };
	return dest->Write(buf, sizeof(buf)) == (ssize_t)sizeof(buf) ? B_OK : B_IO_ERROR;
}

static bool ReadColorEntry(BPositionIO* source, rgb_color* bg, rgb_color* fg)
{
	uint8 buf[8];
	if (source->Read(buf, sizeof(buf)) != (ssize_t)sizeof(buf))
		return false;
	bg->red = buf[0]; bg->green = buf[1]; bg->blue = buf[2]; bg->alpha = buf[3];
	fg->red = buf[4]; fg->green = buf[5]; fg->blue = buf[6]; fg->alpha = buf[7];
	return true;
}

static bool ColorsEqual(rgb_color a, rgb_color b)
{
	return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

status_t SaveASCD(CContainer* doc, BPositionIO* dest,
	const std::vector<ChartObject>* charts,
	const std::vector<std::pair<int, float> >* colWidths,
	const std::vector<std::pair<int, float> >* rowHeights,
	const int* frozenRows, const int* frozenCols)
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
		char text[4096];
		doc->GetCellFormula(c, text, sizeof(text), false);

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

	// Sezione larghezze di colonna personalizzate, in coda: stesso
	// principio dei grafici sopra -- opzionale, un file scritto prima
	// di questa modifica (o senza nessuna colonna ridimensionata) non
	// ce l'ha. Il campo colonna e' int16: il numero di colonne del
	// motore (kColCount) sta ampiamente entro i suoi limiti.
	int32 colWidthCount = colWidths ? (int32)colWidths->size() : 0;
	if (dest->Write(&colWidthCount, sizeof(colWidthCount)) != (ssize_t)sizeof(colWidthCount))
		return B_IO_ERROR;

	for (int32 i = 0; i < colWidthCount; i++)
	{
		int16 col = (int16)(*colWidths)[i].first;
		float width = (*colWidths)[i].second;
		if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
			|| dest->Write(&width, sizeof(width)) != (ssize_t)sizeof(width))
			return B_IO_ERROR;
	}

	// Sezione colori di cella, in coda: letta direttamente da "doc"
	// (CContainer::GetCellStyle, gia' usata per il formato numerico),
	// non da un parametro a parte -- il colore vive gia' dentro il
	// documento, non serve un canale esterno come per colWidths sopra
	// (che invece vive solo in SheetView). Solo le celle esistenti la
	// cui coppia di colori differisce da quella predefinita (bianco/
	// nero) finiscono nel file, non tutte le celle con contenuto.
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, CellStyle> > toWrite;
		CCellIterator styleIter(doc, NULL);
		cell sc;
		while (styleIter.NextExisting(sc))
		{
			CellStyle cs;
			doc->GetCellStyle(sc, cs);
			if (!ColorsEqual(cs.fLowColor, defaultStyle.fLowColor)
				|| !ColorsEqual(cs.fHighColor, defaultStyle.fHighColor))
				toWrite.push_back(std::make_pair(sc, cs));
		}

		int32 cellColorCount = (int32)toWrite.size();
		if (dest->Write(&cellColorCount, sizeof(cellColorCount))
				!= (ssize_t)sizeof(cellColorCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < cellColorCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			const CellStyle& cs = toWrite[i].second;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| WriteColorEntry(dest, cs.fLowColor, cs.fHighColor) != B_OK)
				return B_IO_ERROR;
		}
	}

	// Sezione colori di colonna, in coda: stesso principio, letta da
	// CContainer::GetColumnStyle per ciascuna delle kColCount colonne
	// (una colonna mai impostata torna comunque il predefinito, quindi
	// non finisce nel file: nessun falso positivo).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<int, CellStyle> > toWrite;
		for (int col = 1; col <= kColCount; col++)
		{
			CellStyle cs;
			doc->GetColumnStyle(col, cs);
			if (!ColorsEqual(cs.fLowColor, defaultStyle.fLowColor)
				|| !ColorsEqual(cs.fHighColor, defaultStyle.fHighColor))
				toWrite.push_back(std::make_pair(col, cs));
		}

		int32 columnColorCount = (int32)toWrite.size();
		if (dest->Write(&columnColorCount, sizeof(columnColorCount))
				!= (ssize_t)sizeof(columnColorCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < columnColorCount; i++)
		{
			int16 col = (int16)toWrite[i].first;
			const CellStyle& cs = toWrite[i].second;
			if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| WriteColorEntry(dest, cs.fLowColor, cs.fHighColor) != B_OK)
				return B_IO_ERROR;
		}
	}

	// Sezione altezze di riga personalizzate, in coda: speculare alla
	// sezione larghezze di colonna sopra (Fase 10).
	int32 rowHeightCount = rowHeights ? (int32)rowHeights->size() : 0;
	if (dest->Write(&rowHeightCount, sizeof(rowHeightCount)) != (ssize_t)sizeof(rowHeightCount))
		return B_IO_ERROR;

	for (int32 i = 0; i < rowHeightCount; i++)
	{
		int16 row = (int16)(*rowHeights)[i].first;
		float height = (*rowHeights)[i].second;
		if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
			|| dest->Write(&height, sizeof(height)) != (ssize_t)sizeof(height))
			return B_IO_ERROR;
	}

	// Sezione Blocca riquadri, in coda: due soli interi, non una lista
	// (Fase 10) -- 0 di default se il chiamante non passa nulla, stesso
	// significato di "nessun blocco" di SheetView::SetFreezePanes.
	{
		int32 fr = frozenRows ? *frozenRows : 0;
		int32 fc = frozenCols ? *frozenCols : 0;
		if (dest->Write(&fr, sizeof(fr)) != (ssize_t)sizeof(fr)
			|| dest->Write(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
			return B_IO_ERROR;
	}

	// Sezione font di cella non predefinito, in coda (Fase 10):
	// CellStyle::fFont e' un indice VOLATILE, valido solo dentro
	// gFontSizeTable per la sessione corrente (vedi il commento su
	// CFontSizeTable::GetFontID/GetFontInfo in Fase 7) -- non si puo'
	// scrivere l'indice grezzo e rileggerlo in una sessione successiva,
	// punterebbe a una voce diversa o inesistente. Si scrive invece la
	// tripla famiglia/stile/dimensione gia' risolta (GetFontInfo), e
	// LoadASCD la registra di nuovo con GetFontID (dedup se gia'
	// esistente) per ottenere un indice valido nella sessione che
	// ricarica. Il colore del font (un campo separato dentro
	// CFontMetrics, non usato da SheetView::Draw per disegnare il
	// testo -- quello legge cs.fHighColor, gia' persistito nella
	// sezione colori sopra) non serve qui.
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, CellStyle> > toWrite;
		CCellIterator fontIter(doc, NULL);
		cell fc2;
		while (fontIter.NextExisting(fc2))
		{
			CellStyle cs;
			doc->GetCellStyle(fc2, cs);
			if (cs.fFont != defaultStyle.fFont)
				toWrite.push_back(std::make_pair(fc2, cs));
		}

		int32 fontCount = (int32)toWrite.size();
		if (dest->Write(&fontCount, sizeof(fontCount)) != (ssize_t)sizeof(fontCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < fontCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			font_family family;
			font_style style;
			float size;
			gFontSizeTable.GetFontInfo(toWrite[i].second.fFont, &family, &style, &size);
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(family, sizeof(font_family)) != (ssize_t)sizeof(font_family)
				|| dest->Write(style, sizeof(font_style)) != (ssize_t)sizeof(font_style)
				|| dest->Write(&size, sizeof(size)) != (ssize_t)sizeof(size))
				return B_IO_ERROR;
		}
	}

	// Sezione allineamento di cella non predefinito, in coda (Fase 10):
	// un solo byte per cella, nessuna risoluzione necessaria (a
	// differenza del font sopra, CellStyle::fAlignment e' gia' il
	// valore finale, non un indice).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, char> > toWrite;
		CCellIterator alignIter(doc, NULL);
		cell ac;
		while (alignIter.NextExisting(ac))
		{
			CellStyle cs;
			doc->GetCellStyle(ac, cs);
			if (cs.fAlignment != defaultStyle.fAlignment)
				toWrite.push_back(std::make_pair(ac, cs.fAlignment));
		}

		int32 alignCount = (int32)toWrite.size();
		if (dest->Write(&alignCount, sizeof(alignCount)) != (ssize_t)sizeof(alignCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < alignCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			int8 alignment = toWrite[i].second;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&alignment, sizeof(alignment)) != (ssize_t)sizeof(alignment))
				return B_IO_ERROR;
		}
	}

	// Sezione bordi di cella non predefiniti, in coda (Fase 11): un
	// byte per lato (CellStyle::fTBorderColor/fLBorderColor/
	// fBBorderColor/fRBorderColor -- 0 = nessun bordo, diverso da 0 =
	// bordo nero pieno su quel lato, vedi ROADMAP.md Fase 11 sul
	// perche' non e' un vero colore nonostante il nome del campo).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, CellStyle> > toWrite;
		CCellIterator borderIter(doc, NULL);
		cell bc;
		while (borderIter.NextExisting(bc))
		{
			CellStyle cs;
			doc->GetCellStyle(bc, cs);
			if (cs.fTBorderColor != defaultStyle.fTBorderColor
				|| cs.fLBorderColor != defaultStyle.fLBorderColor
				|| cs.fBBorderColor != defaultStyle.fBBorderColor
				|| cs.fRBorderColor != defaultStyle.fRBorderColor)
				toWrite.push_back(std::make_pair(bc, cs));
		}

		int32 borderCount = (int32)toWrite.size();
		if (dest->Write(&borderCount, sizeof(borderCount)) != (ssize_t)sizeof(borderCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < borderCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			const CellStyle& cs = toWrite[i].second;
			uint8 sides[4] = { cs.fTBorderColor, cs.fLBorderColor,
				cs.fBBorderColor, cs.fRBorderColor };
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(sides, sizeof(sides)) != (ssize_t)sizeof(sides))
				return B_IO_ERROR;
		}
	}

	// Sezione formato numero di cella non predefinito, in coda (Fase
	// 12): CellStyle::fFormat era l'unico campo di stile rimasto senza
	// persistenza nel formato nativo -- non scoperto prima perche' fino
	// a Fase 12 nessun importatore lo impostava mai (solo il menu
	// Formato lo scriveva, da sessione a sessione, mai notato perche'
	// nessun test di round-trip lo copriva). A differenza del font
	// (Fase 10), fFormat non e' un indice volatile: CFormatTable::
	// GetFormatID(int) risolve gli ID sopra eFirstNewFormat (1024) da
	// una mappa interna popolata SOLO durante l'esecuzione corrente
	// (mai persistita a sua volta) -- scriverlo e rileggerlo cosi'
	// com'e' e' quindi sicuro solo per gli ID sotto eFirstNewFormat
	// (i formati "vecchio stile": General/Valuta/Percentuale/Fisso +
	// cifre + virgole, tutti quelli che UI e import XLSX producono
	// oggi). Un ID sopra eFirstNewFormat scritto da questa sessione e
	// riletto in una sessione successiva punterebbe a una voce
	// diversa o assente: scartato silenziosamente al salvataggio (si
	// perde solo l'eventuale formato "nuovo stile", mai prodotto da
	// nessun punto del codice ad oggi) invece di corrompere la
	// lettura.
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, int32> > toWrite;
		CCellIterator formatIter(doc, NULL);
		cell fc;
		while (formatIter.NextExisting(fc))
		{
			CellStyle cs;
			doc->GetCellStyle(fc, cs);
			if (cs.fFormat != defaultStyle.fFormat && cs.fFormat < eFirstNewFormat)
				toWrite.push_back(std::make_pair(fc, (int32)cs.fFormat));
		}

		int32 formatCount = (int32)toWrite.size();
		if (dest->Write(&formatCount, sizeof(formatCount)) != (ssize_t)sizeof(formatCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < formatCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			int32 format = toWrite[i].second;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&format, sizeof(format)) != (ssize_t)sizeof(format))
				return B_IO_ERROR;
		}
	}

	// Sezione sottolineato di cella non predefinito, in coda (Fase
	// 12): CellStyle::fUnderline, un booleano a parte (BFont non ha un
	// attributo sottolineato nativo, vedi il commento sul campo in
	// CellStyle.h) -- stesso pattern dei bordi in Fase 11, un byte per
	// cella.
	{
		CellStyle defaultStyle;
		std::vector<cell> toWrite;
		CCellIterator underlineIter(doc, NULL);
		cell uc;
		while (underlineIter.NextExisting(uc))
		{
			CellStyle cs;
			doc->GetCellStyle(uc, cs);
			if (cs.fUnderline != defaultStyle.fUnderline)
				toWrite.push_back(uc);
		}

		int32 underlineCount = (int32)toWrite.size();
		if (dest->Write(&underlineCount, sizeof(underlineCount)) != (ssize_t)sizeof(underlineCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < underlineCount; i++)
		{
			int16 row = toWrite[i].v, col = toWrite[i].h;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
				return B_IO_ERROR;
		}
	}

	return B_OK;
}

status_t LoadASCD(BPositionIO* source, CContainer* doc,
	std::vector<ChartObject>* charts,
	std::vector<std::pair<int, float> >* colWidths,
	std::vector<std::pair<int, float> >* rowHeights,
	int* frozenRows, int* frozenCols)
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

		char text[4096];
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

	// Sezione grafici incorporati, in coda al formato: puo' non esserci
	// affatto (file scritto prima che questa sezione esistesse). Read()
	// restituisce 0 se lo stream e' gia' finito esattamente li' (formato
	// vecchio, nessun grafico: non un errore); un valore diverso da 0 ma
	// minore di sizeof(chartCount) e' invece un file davvero troncato/
	// corrotto. I byte vanno SEMPRE consumati se presenti, anche se il
	// chiamante passa charts=NULL (non gli interessano i grafici): resta
	// dopo questa sezione la sezione larghezze di colonna (sotto), e se
	// questa chiamata fa parte di una cartella di lavoro multi-foglio
	// (LoadASCDBook, che legge piu' blocchi ASCD in sequenza sullo
	// stesso flusso) lasciare byte non consumati qui disallineerebbe la
	// lettura di ogni foglio successivo -- bug reale gia' scoperto per
	// XlsxTranslator::WriteASCD, vedi ROADMAP.md Fase 9.
	{
		std::vector<ChartObject> discardedCharts;
		std::vector<ChartObject>* out = charts ? charts : &discardedCharts;
		out->clear();

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
				out->push_back(obj);
			}
		}
	}

	// Sezione larghezze di colonna personalizzate, in coda: stesso
	// principio della sezione grafici sopra, stessa cautela sul
	// consumare sempre i byte anche con colWidths=NULL.
	{
		std::vector<std::pair<int, float> > discardedWidths;
		std::vector<std::pair<int, float> >* out = colWidths ? colWidths : &discardedWidths;
		out->clear();

		int32 colWidthCount = 0;
		ssize_t got = source->Read(&colWidthCount, sizeof(colWidthCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(colWidthCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < colWidthCount; i++)
			{
				int16 col;
				float width;
				if (source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&width, sizeof(width)) != (ssize_t)sizeof(width))
					return B_BAD_DATA;

				out->push_back(std::make_pair((int)col, width));
			}
		}
	}

	// Sezione colori di cella, in coda: applicata direttamente a "doc"
	// tramite CContainer::SetCellStyle (i byte vanno comunque sempre
	// consumati se presenti, stessa cautela delle sezioni sopra --
	// qui pero' non serve un "out" alternativo: applicarla anche
	// quando il chiamante non ha chiesto nessun parametro dedicato non
	// ha controindicazioni, dato che il colore fa gia' parte del
	// documento stesso).
	{
		int32 cellColorCount = 0;
		ssize_t got = source->Read(&cellColorCount, sizeof(cellColorCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(cellColorCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < cellColorCount; i++)
			{
				int16 row, col;
				rgb_color bg, fg;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| !ReadColorEntry(source, &bg, &fg))
					return B_BAD_DATA;

				CellStyle cs;
				cell loc(col, row);
				doc->GetCellStyle(loc, cs);
				cs.fLowColor = bg;
				cs.fHighColor = fg;
				doc->SetCellStyle(loc, cs);
			}
		}
	}

	// Sezione colori di colonna, in coda: stesso principio, applicata
	// tramite CContainer::SetColumnStyle.
	{
		int32 columnColorCount = 0;
		ssize_t got = source->Read(&columnColorCount, sizeof(columnColorCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(columnColorCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < columnColorCount; i++)
			{
				int16 col;
				rgb_color bg, fg;
				if (source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| !ReadColorEntry(source, &bg, &fg))
					return B_BAD_DATA;

				CellStyle cs;
				doc->GetColumnStyle(col, cs);
				cs.fLowColor = bg;
				cs.fHighColor = fg;
				doc->SetColumnStyle(col, cs);
			}
		}
	}

	// Sezione altezze di riga personalizzate, in coda: stessa cautela
	// delle sezioni sopra sul consumare sempre i byte se presenti
	// (Fase 10).
	{
		std::vector<std::pair<int, float> > discardedHeights;
		std::vector<std::pair<int, float> >* out = rowHeights ? rowHeights : &discardedHeights;
		out->clear();

		int32 rowHeightCount = 0;
		ssize_t got = source->Read(&rowHeightCount, sizeof(rowHeightCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(rowHeightCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < rowHeightCount; i++)
			{
				int16 row;
				float height;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&height, sizeof(height)) != (ssize_t)sizeof(height))
					return B_BAD_DATA;

				out->push_back(std::make_pair((int)row, height));
			}
		}
	}

	// Sezione Blocca riquadri, in coda: due soli interi (Fase 10),
	// stessa cautela delle sezioni sopra.
	{
		int32 fr = 0, fc = 0;
		ssize_t got = source->Read(&fr, sizeof(fr));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(fr)
				|| source->Read(&fc, sizeof(fc)) != (ssize_t)sizeof(fc))
				return B_BAD_DATA;
		}
		if (frozenRows) *frozenRows = (int)fr;
		if (frozenCols) *frozenCols = (int)fc;
	}

	// Sezione font di cella non predefinito, in coda (Fase 10): vedi
	// il commento in SaveASCD -- registra di nuovo la tripla famiglia/
	// stile/dimensione con GetFontID (dedup se gia' esistente) per
	// ottenere un indice valido nella sessione che ricarica.
	{
		int32 fontCount = 0;
		ssize_t got = source->Read(&fontCount, sizeof(fontCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(fontCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < fontCount; i++)
			{
				int16 row, col;
				font_family family;
				font_style style;
				float size;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(family, sizeof(font_family)) != (ssize_t)sizeof(font_family)
					|| source->Read(style, sizeof(font_style)) != (ssize_t)sizeof(font_style)
					|| source->Read(&size, sizeof(size)) != (ssize_t)sizeof(size))
					return B_BAD_DATA;

				CellStyle cs;
				cell loc(col, row);
				doc->GetCellStyle(loc, cs);
				cs.fFont = (int)gFontSizeTable.GetFontID(family, style, size);
				doc->SetCellStyle(loc, cs);
			}
		}
	}

	// Sezione allineamento di cella non predefinito, in coda (Fase 10).
	{
		int32 alignCount = 0;
		ssize_t got = source->Read(&alignCount, sizeof(alignCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(alignCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < alignCount; i++)
			{
				int16 row, col;
				int8 alignment;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&alignment, sizeof(alignment)) != (ssize_t)sizeof(alignment))
					return B_BAD_DATA;

				CellStyle cs;
				cell loc(col, row);
				doc->GetCellStyle(loc, cs);
				cs.fAlignment = (char)alignment;
				doc->SetCellStyle(loc, cs);
			}
		}
	}

	// Sezione bordi di cella non predefiniti, in coda (Fase 11): vedi
	// il commento in SaveASCD.
	{
		int32 borderCount = 0;
		ssize_t got = source->Read(&borderCount, sizeof(borderCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(borderCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < borderCount; i++)
			{
				int16 row, col;
				uint8 sides[4];
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(sides, sizeof(sides)) != (ssize_t)sizeof(sides))
					return B_BAD_DATA;

				CellStyle cs;
				cell loc(col, row);
				doc->GetCellStyle(loc, cs);
				cs.fTBorderColor = sides[0];
				cs.fLBorderColor = sides[1];
				cs.fBBorderColor = sides[2];
				cs.fRBorderColor = sides[3];
				doc->SetCellStyle(loc, cs);
			}
		}
	}

	// Sezione formato numero di cella non predefinito, in coda (Fase
	// 12): vedi il commento in SaveASCD.
	{
		int32 formatCount = 0;
		ssize_t got = source->Read(&formatCount, sizeof(formatCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(formatCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < formatCount; i++)
			{
				int16 row, col;
				int32 format;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&format, sizeof(format)) != (ssize_t)sizeof(format))
					return B_BAD_DATA;

				CellStyle cs;
				cell loc(col, row);
				doc->GetCellStyle(loc, cs);
				cs.fFormat = (int)format;
				doc->SetCellStyle(loc, cs);
			}
		}
	}

	// Sezione sottolineato di cella non predefinito, in coda (Fase
	// 12): vedi il commento in SaveASCD -- solo riga/colonna, nessun
	// valore da leggere (la sola presenza nell'elenco vuol dire true).
	{
		int32 underlineCount = 0;
		ssize_t got = source->Read(&underlineCount, sizeof(underlineCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(underlineCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < underlineCount; i++)
			{
				int16 row, col;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col))
					return B_BAD_DATA;

				CellStyle cs;
				cell loc(col, row);
				doc->GetCellStyle(loc, cs);
				cs.fUnderline = true;
				doc->SetCellStyle(loc, cs);
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
// Una singola passata su "doc" (usata sia da RecalculateAll che da
// RecalculateWorkbook sotto): true se almeno una cella ha cambiato
// valore, cioe' se serve un'altra passata per raggiungere la
// convergenza.
static bool RecalculatePass(CContainer* doc)
{
	bool changed = false;
	CCellIterator iter(doc, NULL);
	cell c;
	while (iter.NextExisting(c))
	{
		if (doc->CalcCell(c))
			changed = true;
	}
	return changed;
}

void RecalculateAll(CContainer* doc)
{
	bool changed = true;
	int guard = 0;
	while (changed && guard < 50)
	{
		changed = RecalculatePass(doc);
		guard++;
	}
}

// Stesso principio di RecalculateAll, esteso a TUTTI i fogli di una
// cartella di lavoro invece di un solo CContainer: una formula puo'
// referenziare un foglio diverso dal proprio (Fase 9, vedi
// ISheetResolver in Container.h), quindi il ricalcolo per
// convergenza deve considerare le celle di ogni foglio a ogni
// passata, non i fogli uno alla volta in sequenza -- altrimenti un
// foglio B che referenzia un foglio A ricalcolato PRIMA di lui nella
// stessa passata leggerebbe comunque un valore aggiornato (va bene),
// ma l'inverso (A referenzia B) alla stessa passata leggerebbe
// ancora il valore vecchio di B, richiedendo un'altra passata
// completa per propagarsi -- esattamente cio' che il ciclo esterno
// "changed" gia' gestisce, come per un singolo foglio.
void RecalculateWorkbook(std::vector<AscdSheet>& sheets)
{
	bool changed = true;
	int guard = 0;
	while (changed && guard < 50)
	{
		changed = false;
		for (size_t i = 0; i < sheets.size(); i++)
		{
			if (RecalculatePass(sheets[i].doc))
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

		status_t err = SaveASCD(sheet.doc, dest, &sheet.charts, &sheet.colWidths,
			&sheet.rowHeights, &sheet.frozenRows, &sheet.frozenCols);
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

		status_t err = LoadASCD(source, sheet.doc, &sheet.charts, &sheet.colWidths,
			&sheet.rowHeights, &sheet.frozenRows, &sheet.frozenCols);
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
