/*
	AscdIO.cpp

	Vedi AscdIO.h.
*/

#include "AscdIO.h"

#include <cstring>
#include <fcntl.h>
#include <map>
#include <string>

#include "Cell.h"
#include "CellStyle.h"
#include "Constants.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "FontMetrics.h"
#include "Formatter.h"

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
// Versione 2 (era 1): aggiunge un byte "kind" per cella subito dopo
// "len" (vedi SaveASCD/LoadASCD sotto) -- un file versione 1 resta
// leggibile esattamente come prima (ramo a parte in LoadASCD), non
// serve alcuna migrazione. Bug reale: un valore TESTO che assomiglia
// a un'espressione valida (es. "P-EL-a", tre nomi non definiti
// concatenati da un "meno") veniva scritto come testo grezzo e poi
// SEMPRE ri-analizzato da TryToParseString in lettura, che lo
// trasformava silenziosamente in una formula viva che calcola NaN --
// scoperto verificando XLOOKUP su una tabella strutturata reale, la
// cui colonna Codice conteneva proprio questo genere di codici. Il
// byte "kind" toglie l'ambiguita' alla radice: dice a LoadASCD se la
// cella era davvero una formula (analizzarla come sempre) o un
// valore testo letterale (scriverlo cosi' com'e', mai passarlo per
// Parse()) -- stesso principio gia' in uso in XlsxTranslator.cpp per
// lo stesso identico bug all'importazione diretta da un file XLSX.
static const int32 kASCDVersion = 2;
enum { kAscdCellFormula = 0, kAscdCellLiteralOther = 1, kAscdCellLiteralText = 2 };
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
	const int* frozenRows, const int* frozenCols,
	const std::vector<EmbeddedImage>* images,
	const bool* showGrid,
	const bool* hasTabColor, const rgb_color* tabColor,
	const std::vector<int>* hiddenRows,
	const bool* hasAutoFilter, const range* autoFilterRange)
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

		// "kind" (Fase 15, versione 2 del formato -- vedi il commento
		// su kASCDVersion sopra): GetCellFormula(cell) (l'overload che
		// restituisce il puntatore grezzo, non il testo) e' l'unico
		// modo affidabile di sapere se questa cella e' davvero una
		// formula, esattamente come gia' fa MainWindow::UpdateSelection
		// per la barra della formula -- il TESTO da solo (senza "=",
		// perche' gWithEqualSign resta sempre a false in questo porting)
		// non lo direbbe mai in modo certo.
		uint8 kind;
		if (doc->GetCellFormula(c) != NULL)
			kind = kAscdCellFormula;
		else
		{
			Value v;
			doc->GetValue(c, v);
			kind = (v.fType == eTextData) ? kAscdCellLiteralText : kAscdCellLiteralOther;
		}

		if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_IO_ERROR;
		if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_IO_ERROR;
		if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_IO_ERROR;
		if (dest->Write(&kind, sizeof(kind)) != (ssize_t)sizeof(kind))
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

	// Sezione testo a capo di cella non predefinito, in coda (Fase
	// 12): CellStyle::fWrapText, stesso pattern del sottolineato sopra
	// (un booleano a parte, solo riga/colonna, nessun valore da
	// scrivere). L'altezza di riga necessaria a contenere il testo a
	// capo NON viene ricalcolata qui (SaveASCD non ha una view/font
	// vivi per misurare il testo): resta quella gia' persistita dalla
	// sezione altezze di riga di Fase 10, ricalcolata di nuovo al
	// prossimo caricamento da SheetView::RecalculateWrappedRowHeights.
	{
		CellStyle defaultStyle;
		std::vector<cell> toWrite;
		CCellIterator wrapIter(doc, NULL);
		cell wc;
		while (wrapIter.NextExisting(wc))
		{
			CellStyle cs;
			doc->GetCellStyle(wc, cs);
			if (cs.fWrapText != defaultStyle.fWrapText)
				toWrite.push_back(wc);
		}

		int32 wrapCount = (int32)toWrite.size();
		if (dest->Write(&wrapCount, sizeof(wrapCount)) != (ssize_t)sizeof(wrapCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < wrapCount; i++)
		{
			int16 row = toWrite[i].v, col = toWrite[i].h;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
				return B_IO_ERROR;
		}
	}

	// Sezione celle unite, in coda (Fase 12): un elenco di rettangoli
	// per il foglio (CContainer::GetMergedRanges), non un campo per
	// cella come tutte le sezioni sopra -- una cella unita e' un'unica
	// entita' logica che occupa piu' coordinate.
	{
		const std::vector<range>& merged = doc->GetMergedRanges();
		int32 mergeCount = (int32)merged.size();
		if (dest->Write(&mergeCount, sizeof(mergeCount)) != (ssize_t)sizeof(mergeCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < mergeCount; i++)
		{
			int16 top = merged[i].top, left = merged[i].left;
			int16 bottom = merged[i].bottom, right = merged[i].right;
			if (dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
				|| dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
				|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
				|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right))
				return B_IO_ERROR;
		}
	}

	// Sezione immagini incorporate, in coda (Fase 12): stesso principio
	// opzionale delle sezioni sopra -- un elenco di record a lunghezza
	// variabile (il blob PNG grezzo, vedi EmbeddedImage.h), non un
	// campo per cella o un rettangolo fisso come le sezioni precedenti.
	{
		int32 imageCount = images ? (int32)images->size() : 0;
		if (dest->Write(&imageCount, sizeof(imageCount)) != (ssize_t)sizeof(imageCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < imageCount; i++)
		{
			const EmbeddedImage& img = (*images)[i];
			int16 row = img.anchor.v, col = img.anchor.h;
			float geom[4] = { img.offsetX, img.offsetY, img.width, img.height };
			int32 pngLen = (int32)img.pngData.size();

			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(geom, sizeof(geom)) != (ssize_t)sizeof(geom)
				|| dest->Write(&pngLen, sizeof(pngLen)) != (ssize_t)sizeof(pngLen))
				return B_IO_ERROR;
			if (pngLen > 0 && dest->Write(&img.pngData[0], pngLen) != pngLen)
				return B_IO_ERROR;
		}
	}

	// Sezione visibilita' griglia, in coda: un solo booleano per
	// foglio (vedi il commento su AscdSheet::showGrid in AscdIO.h),
	// stesso principio opzionale delle sezioni sopra -- true di
	// default se il chiamante non passa nulla.
	{
		uint8 sg = (showGrid ? *showGrid : true) ? 1 : 0;
		if (dest->Write(&sg, sizeof(sg)) != (ssize_t)sizeof(sg))
			return B_IO_ERROR;
	}

	// Sezione colore della linguetta del foglio, in coda: vedi il
	// commento su AscdSheet::hasTabColor/tabColor in AscdIO.h -- un
	// byte "presente si'/no" seguito da tre byte RGB (sempre scritti,
	// stesso principio delle altre sezioni sopra).
	{
		uint8 has = (hasTabColor && *hasTabColor) ? 1 : 0;
		rgb_color color = { 0, 0, 0, 255 };
		if (has && tabColor)
			color = *tabColor;
		uint8 rgb[3] = { color.red, color.green, color.blue };
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
			return B_IO_ERROR;
	}

	// Sezione righe nascoste, in coda: un elenco sparso di indici di
	// riga (SheetView::HiddenRows), stesso principio di colWidths/
	// rowHeights sopra ma senza un valore associato -- la sola presenza
	// nell'elenco vuol dire nascosta.
	{
		int32 hiddenCount = hiddenRows ? (int32)hiddenRows->size() : 0;
		if (dest->Write(&hiddenCount, sizeof(hiddenCount)) != (ssize_t)sizeof(hiddenCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < hiddenCount; i++)
		{
			int16 row = (int16)(*hiddenRows)[i];
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row))
				return B_IO_ERROR;
		}
	}

	// Sezione AutoFilter, in coda: un byte "presente si'/no" seguito da
	// quattro interi (l'intervallo, vedi SheetView::AutoFilterRange) --
	// sempre scritti, stesso principio delle altre sezioni "singolo
	// valore" sopra (Blocca riquadri). I CRITERI del filtro (quali
	// valori sono esclusi per colonna) NON sono qui: vedi il commento
	// su AscdSheet::hiddenRows in AscdIO.h, solo il risultato
	// sopravvive al giro salvataggio/ricarica.
	{
		uint8 has = (hasAutoFilter && *hasAutoFilter) ? 1 : 0;
		range r = (has && autoFilterRange) ? *autoFilterRange : range();
		int16 top = r.top, left = r.left, bottom = r.bottom, right = r.right;
		if (dest->Write(&has, sizeof(has)) != (ssize_t)sizeof(has)
			|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
			|| dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
			|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
			|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right))
			return B_IO_ERROR;
	}

	// Sezione commenti/note per cella, in coda (Fase 13): stesso
	// principio delle celle unite sopra -- CContainer::GetComments,
	// non un campo per cella nella sezione principale (la stragrande
	// maggioranza delle celle non avra' mai un commento, vedi il
	// commento su CContainer::fComments in Container.h).
	{
		const std::map<cell, std::string>& comments = doc->GetComments();
		int32 commentCount = (int32)comments.size();
		if (dest->Write(&commentCount, sizeof(commentCount)) != (ssize_t)sizeof(commentCount))
			return B_IO_ERROR;

		for (std::map<cell, std::string>::const_iterator it = comments.begin();
			it != comments.end(); ++it)
		{
			int16 row = it->first.v, col = it->first.h;
			int32 len = (int32)it->second.size();
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(it->second.data(), len) != len)
				return B_IO_ERROR;
		}
	}

	// Sezione collegamenti ipertestuali, in coda (Fase 13): stesso
	// schema esatto dei commenti sopra.
	{
		const std::map<cell, std::string>& links = doc->GetHyperlinks();
		int32 linkCount = (int32)links.size();
		if (dest->Write(&linkCount, sizeof(linkCount)) != (ssize_t)sizeof(linkCount))
			return B_IO_ERROR;

		for (std::map<cell, std::string>::const_iterator it = links.begin();
			it != links.end(); ++it)
		{
			int16 row = it->first.v, col = it->first.h;
			int32 len = (int32)it->second.size();
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(it->second.data(), len) != len)
				return B_IO_ERROR;
		}
	}

	// Sezione tipo di grafico incorporato, in coda (Fase 13): un byte
	// per grafico, nello STESSO ordine dell'array "charts" scritto
	// sopra -- non un campo dentro il record fisso di ogni grafico
	// (che vive PRIMA di altre sezioni nel formato, vedi sopra):
	// inserire un campo li' romperebbe l'allineamento byte per byte
	// dei file .ascd gia' scritti con un grafico, che oggi hanno solo
	// grafici a barre. Un file .ascd scritto PRIMA di questa modifica
	// (o senza questa sezione) lascia ogni ChartObject al tipo
	// predefinito (eBarChart, gia' impostato dal costruttore in
	// Chart.h), stesso schema EOF-tollerante delle altre sezioni
	// opzionali sopra.
	{
		int32 chartTypeCount = charts ? (int32)charts->size() : 0;
		if (dest->Write(&chartTypeCount, sizeof(chartTypeCount)) != (ssize_t)sizeof(chartTypeCount))
			return B_IO_ERROR;
		for (int32 i = 0; i < chartTypeCount; i++)
		{
			int8 type = (int8)(*charts)[i].type;
			if (dest->Write(&type, sizeof(type)) != (ssize_t)sizeof(type))
				return B_IO_ERROR;
		}
	}

	// Sezione colore del bordo di cella non predefinito, in coda (Fase
	// 13): UN colore condiviso da tutti i lati del bordo di una cella
	// (CellStyle::fBorderColor), non un colore diverso per lato --
	// stessa scelta di scope gia' fatta per i grafici, vedi
	// ROADMAP.md. Sezione SEPARATA da quella dei bordi sopra (che
	// restano un byte di SPESSORE per lato, semantica appena estesa da
	// presenza/assenza a sottile/medio/spesso ma stesso formato byte
	// per byte di prima) per non toccare l'allineamento dei file .ascd
	// gia' scritti.
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, rgb_color> > toWrite;
		CCellIterator borderColorIter(doc, NULL);
		cell bcc;
		while (borderColorIter.NextExisting(bcc))
		{
			CellStyle cs;
			doc->GetCellStyle(bcc, cs);
			if (!ColorsEqual(cs.fBorderColor, defaultStyle.fBorderColor))
				toWrite.push_back(std::make_pair(bcc, cs.fBorderColor));
		}

		int32 borderColorCount = (int32)toWrite.size();
		if (dest->Write(&borderColorCount, sizeof(borderColorCount)) != (ssize_t)sizeof(borderColorCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < borderColorCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			rgb_color color = toWrite[i].second;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&color, sizeof(color)) != (ssize_t)sizeof(color))
				return B_IO_ERROR;
		}
	}

	// Sezione convalida dati, in coda (Fase 13): stesso schema sparso
	// di commenti/collegamenti sopra -- eNoValidation (nessuna regola)
	// non finisce mai in questa mappa, vedi CContainer::SetValidation.
	{
		const std::map<cell, ValidationRule>& validations = doc->GetValidations();
		int32 validationCount = (int32)validations.size();
		if (dest->Write(&validationCount, sizeof(validationCount)) != (ssize_t)sizeof(validationCount))
			return B_IO_ERROR;

		for (std::map<cell, ValidationRule>::const_iterator it = validations.begin();
			it != validations.end(); ++it)
		{
			int16 row = it->first.v, col = it->first.h;
			int8 type = (int8)it->second.type;
			int32 len = (int32)it->second.list.size();
			double min = it->second.min, max = it->second.max;
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(&type, sizeof(type)) != (ssize_t)sizeof(type)
				|| dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
				return B_IO_ERROR;
			if (len > 0 && dest->Write(it->second.list.data(), len) != len)
				return B_IO_ERROR;
			if (dest->Write(&min, sizeof(min)) != (ssize_t)sizeof(min)
				|| dest->Write(&max, sizeof(max)) != (ssize_t)sizeof(max))
				return B_IO_ERROR;
		}
	}

	// Sezione formattazione condizionale VIVA, in coda (Fase 13): a
	// differenza di commenti/collegamenti/convalida sopra, non e' un
	// dato per cella ma un elenco di REGOLE, ognuna con uno o piu'
	// intervalli (stesso schema di ChartObject::dataRange) -- stesso
	// principio EOF-tollerante delle altre sezioni opzionali.
	{
		const std::vector<ConditionalFormatRule>& rules = doc->GetConditionalFormatRules();
		int32 ruleCount = (int32)rules.size();
		if (dest->Write(&ruleCount, sizeof(ruleCount)) != (ssize_t)sizeof(ruleCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < ruleCount; i++)
		{
			const ConditionalFormatRule& rule = rules[i];
			int8 type = (int8)rule.type;
			int32 valueLen = (int32)rule.compareValue.size();
			if (dest->Write(&type, sizeof(type)) != (ssize_t)sizeof(type)
				|| dest->Write(&valueLen, sizeof(valueLen)) != (ssize_t)sizeof(valueLen))
				return B_IO_ERROR;
			if (valueLen > 0 && dest->Write(rule.compareValue.data(), valueLen) != valueLen)
				return B_IO_ERROR;
			if (dest->Write(&rule.bgColor, sizeof(rule.bgColor)) != (ssize_t)sizeof(rule.bgColor))
				return B_IO_ERROR;

			int32 rangeCount = (int32)rule.ranges.size();
			if (dest->Write(&rangeCount, sizeof(rangeCount)) != (ssize_t)sizeof(rangeCount))
				return B_IO_ERROR;
			for (int32 r = 0; r < rangeCount; r++)
			{
				const range& rg = rule.ranges[r];
				int16 left = rg.left, top = rg.top, right = rg.right, bottom = rg.bottom;
				if (dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right)
					|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
					return B_IO_ERROR;
			}
		}
	}

	// Sezione tabelle strutturate di Excel, in coda (Fase 14,
	// "Tabella12[Codice]"): stesso schema EOF-tollerante delle sezioni
	// sopra. Senza questa sezione, un file .xlsm con XLOOKUP su una
	// Tabella Excel calcolerebbe correttamente SOLO nella sessione di
	// importazione in memoria (dove CContainer::AddTable e' gia' stato
	// chiamato dal traduttore) -- MainWindow::OpenFile pero' passa
	// SEMPRE dai byte ASCD (anche per un file .xlsm appena tradotto,
	// non solo un .ascd nativo riaperto), che ricreano un CContainer
	// del tutto nuovo: senza persistere fTables qui, quel nuovo
	// documento non avrebbe alcuna tabella registrata, e ogni
	// riferimento "Tabella12[Colonna]" tornerebbe a calcolare gNameNan
	// (visibile come "non trovato") invece del valore vero -- bug reale
	// che avrebbe vanificato tutto il resto di questa fase.
	{
		const std::map<std::string, CTableDef>& tables = doc->GetTables();
		int32 tableCount = (int32)tables.size();
		if (dest->Write(&tableCount, sizeof(tableCount)) != (ssize_t)sizeof(tableCount))
			return B_IO_ERROR;

		for (std::map<std::string, CTableDef>::const_iterator it = tables.begin();
			it != tables.end(); ++it)
		{
			int32 nameLen = (int32)it->first.size();
			if (dest->Write(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
				return B_IO_ERROR;
			if (nameLen > 0 && dest->Write(it->first.data(), nameLen) != nameLen)
				return B_IO_ERROR;

			const CTableDef& def = it->second;
			int16 left = def.dataRange.left, top = def.dataRange.top,
				right = def.dataRange.right, bottom = def.dataRange.bottom;
			if (dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
				|| dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
				|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right)
				|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
				return B_IO_ERROR;

			int32 columnCount = (int32)def.columnNames.size();
			if (dest->Write(&columnCount, sizeof(columnCount)) != (ssize_t)sizeof(columnCount))
				return B_IO_ERROR;
			for (int32 c = 0; c < columnCount; c++)
			{
				int32 colLen = (int32)def.columnNames[c].size();
				if (dest->Write(&colLen, sizeof(colLen)) != (ssize_t)sizeof(colLen))
					return B_IO_ERROR;
				if (colLen > 0 && dest->Write(def.columnNames[c].data(), colLen) != colLen)
					return B_IO_ERROR;
			}
		}
	}

	return B_OK;
}

status_t LoadASCD(BPositionIO* source, CContainer* doc,
	std::vector<ChartObject>* charts,
	std::vector<std::pair<int, float> >* colWidths,
	std::vector<std::pair<int, float> >* rowHeights,
	int* frozenRows, int* frozenCols,
	std::vector<EmbeddedImage>* images,
	bool* showGrid,
	bool* hasTabColor, rgb_color* tabColor,
	std::vector<int>* hiddenRows,
	bool* hasAutoFilter, range* autoFilterRange)
{
	char magic[4];
	if (source->Read(magic, 4) != 4)
		return B_BAD_DATA;
	if (memcmp(magic, kASCDMagic, 4) != 0)
		return B_BAD_DATA;

	int32 version;
	if (source->Read(&version, sizeof(version)) != (ssize_t)sizeof(version))
		return B_BAD_DATA;
	// versione 1 (mai il byte "kind" per cella) e versione 2 (Fase 15,
	// vedi il commento su kASCDVersion sopra) restano ENTRAMBE
	// leggibili -- un file scritto da una versione precedente di
	// questo formato non deve smettere di aprirsi solo perche' questo
	// binario e' piu' recente.
	if (version != 1 && version != kASCDVersion)
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

		uint8 kind = kAscdCellFormula;
		if (version >= 2 && source->Read(&kind, sizeof(kind)) != (ssize_t)sizeof(kind))
			return B_BAD_DATA;

		char text[4096];
		if (len < 0 || len >= (int32)sizeof(text))
			return B_BAD_DATA;
		if (len > 0 && source->Read(text, len) != len)
			return B_BAD_DATA;
		text[len] = 0;

		cell c(col, row);

		// Un valore TESTO letterale (kind scritto da una versione 2 di
		// SaveASCD, vedi sopra) non passa MAI per TryToParseString/
		// Parse(): un codice come "P-EL-a" (tre nomi non definiti
		// concatenati da un "meno") e' un'espressione sintatticamente
		// valida quanto una vera formula, e Parse() non ha modo di
		// distinguerli guardando solo il testo -- da qui il bisogno del
		// byte "kind", scritto quando l'informazione (la cella era o
		// non era una formula) era ancora disponibile. Un file versione
		// 1 non ha questo byte (kind resta kAscdCellFormula sopra) e
		// passa quindi per il ramo TryToParseString esattamente come
		// sempre, stesso comportamento identico a prima di questo fix.
		if (kind == kAscdCellLiteralText)
		{
			Value v(text);
			doc->NewCell(c, v, NULL);
		}
		else
		{
			try
			{
				// inWarnIfError=false, non true: un valore TESTO scritto da
				// GetCellFormula/FormatValue (sopra in WriteASCD) non porta
				// mai apici o segni che lo marchino come "sicuramente testo"
				// -- se assomiglia abbastanza a un numero/data da superare
				// l'analisi grammaticale del parser ma poi fallisce a
				// ridursi a un valore (es. "01.11.10", tre gruppi separati
				// da punti, un codice ATECO reale), con inWarnIfError=true
				// TryToParseString rilancia l'eccezione invece di ripiegare
				// sul testo originale: il catch sotto la intercetta e la
				// cella sparisce del tutto, silenziosamente, invece di
				// mostrare "01.11.10" come testo. Bug reale scoperto aprendo
				// una tabella di codici ATECO reale (colonna A vuota dopo
				// l'apertura, screenshot dell'utente) -- stesso principio
				// gia' in uso correttamente da ParseSheet nei tre translator
				// (XLSX/ODS/CSV) per lo stesso identico motivo quando
				// importano testo da un formato esterno.
				TryToParseString(text, c, doc, false);
			}
			catch (...)
			{
				// Una singola cella corrotta non deve far fallire l'intero
				// caricamento: viene semplicemente saltata.
			}
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
				if (!loc.IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
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
				if (col < 1 || col > kColCount)
					return B_BAD_DATA; // "col" grezzo dal file, non ancora validato

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
				if (!loc.IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
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
				if (!loc.IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
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
				if (!loc.IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
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
				if (!loc.IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
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
				if (!loc.IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
				doc->GetCellStyle(loc, cs);
				cs.fUnderline = true;
				doc->SetCellStyle(loc, cs);
			}
		}
	}

	// Sezione testo a capo di cella non predefinito, in coda (Fase
	// 12): vedi il commento in SaveASCD -- solo riga/colonna, nessun
	// valore da leggere.
	{
		int32 wrapCount = 0;
		ssize_t got = source->Read(&wrapCount, sizeof(wrapCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(wrapCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < wrapCount; i++)
			{
				int16 row, col;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col))
					return B_BAD_DATA;

				CellStyle cs;
				cell loc(col, row);
				if (!loc.IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
				doc->GetCellStyle(loc, cs);
				cs.fWrapText = true;
				doc->SetCellStyle(loc, cs);
			}
		}
	}

	// Sezione celle unite, in coda (Fase 12): vedi il commento in
	// SaveASCD.
	{
		int32 mergeCount = 0;
		ssize_t got = source->Read(&mergeCount, sizeof(mergeCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(mergeCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < mergeCount; i++)
			{
				int16 top, left, bottom, right;
				if (source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
					|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right))
					return B_BAD_DATA;

				doc->AddMergedRange(range(left, top, right, bottom));
			}
		}
	}

	// Sezione immagini incorporate, in coda (Fase 12): vedi il
	// commento in SaveASCD. Stesso principio "got != 0" delle sezioni
	// sopra per distinguere un file scritto prima di questa sezione da
	// uno davvero troncato/corrotto.
	{
		std::vector<EmbeddedImage> discardedImages;
		std::vector<EmbeddedImage>* out = images ? images : &discardedImages;
		out->clear();

		int32 imageCount = 0;
		ssize_t got = source->Read(&imageCount, sizeof(imageCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(imageCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < imageCount; i++)
			{
				int16 row, col;
				float geom[4];
				int32 pngLen;

				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(geom, sizeof(geom)) != (ssize_t)sizeof(geom)
					|| source->Read(&pngLen, sizeof(pngLen)) != (ssize_t)sizeof(pngLen))
					return B_BAD_DATA;
				if (!cell(col, row).IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
				// Tetto ampio ma finito (200 MiB): un PNG incorporato
				// legittimo non si avvicina lontanamente a questa
				// dimensione, ma senza un tetto un "pngLen" enorme in un
				// file corrotto/malevolo forzerebbe un tentativo di
				// allocazione gigantesco (bad_alloc) prima ancora che la
				// Read() sottostante possa fallire da sola.
				if (pngLen < 0 || pngLen > 200 * 1024 * 1024)
					return B_BAD_DATA;

				EmbeddedImage img;
				img.anchor = cell(col, row);
				img.offsetX = geom[0];
				img.offsetY = geom[1];
				img.width = geom[2];
				img.height = geom[3];
				if (pngLen > 0)
				{
					img.pngData.resize(pngLen);
					if (source->Read(&img.pngData[0], pngLen) != pngLen)
						return B_BAD_DATA;
				}

				out->push_back(img);
			}
		}
	}

	// Sezione visibilita' griglia, in coda: un solo byte (Fase
	// successiva a Fase 12), stesso principio "got != 0" delle sezioni
	// sopra per distinguere un file scritto prima di questa sezione da
	// uno davvero troncato/corrotto.
	{
		uint8 sg = 1;
		ssize_t got = source->Read(&sg, sizeof(sg));
		if (got != 0 && got != (ssize_t)sizeof(sg))
			return B_BAD_DATA;
		if (showGrid) *showGrid = sg != 0;
	}

	// Sezione colore della linguetta del foglio, in coda: vedi il
	// commento in SaveASCD -- stesso principio "got != 0" delle sezioni
	// sopra.
	{
		uint8 has = 0;
		uint8 rgb[3] = { 0, 0, 0 };
		ssize_t got = source->Read(&has, sizeof(has));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(has)
				|| source->Read(rgb, sizeof(rgb)) != (ssize_t)sizeof(rgb))
				return B_BAD_DATA;
		}
		if (hasTabColor) *hasTabColor = has != 0;
		if (tabColor)
		{
			tabColor->red = rgb[0];
			tabColor->green = rgb[1];
			tabColor->blue = rgb[2];
			tabColor->alpha = 255;
		}
	}

	// Sezione righe nascoste, in coda: vedi il commento in SaveASCD.
	{
		std::vector<int> discardedHiddenRows;
		std::vector<int>* out = hiddenRows ? hiddenRows : &discardedHiddenRows;
		out->clear();

		int32 hiddenCount = 0;
		ssize_t got = source->Read(&hiddenCount, sizeof(hiddenCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(hiddenCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < hiddenCount; i++)
			{
				int16 row;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row))
					return B_BAD_DATA;
				out->push_back(row);
			}
		}
	}

	// Sezione AutoFilter, in coda: vedi il commento in SaveASCD.
	{
		uint8 has = 0;
		int16 top = 0, left = 0, bottom = 0, right = 0;
		ssize_t got = source->Read(&has, sizeof(has));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(has)
				|| source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
				|| source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
				|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
				|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right))
				return B_BAD_DATA;
		}
		if (hasAutoFilter) *hasAutoFilter = has != 0;
		if (autoFilterRange)
			*autoFilterRange = range(left, top, right, bottom);
	}

	// Sezione commenti/note per cella, in coda: vedi il commento in
	// SaveASCD. Stesso principio "got != 0" delle altre sezioni
	// opzionali sopra (un file scritto prima che questa sezione
	// esistesse si rilegge comunque senza errori, senza commenti).
	{
		int32 commentCount = 0;
		ssize_t got = source->Read(&commentCount, sizeof(commentCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(commentCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < commentCount; i++)
			{
				int16 row, col;
				int32 len;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
					return B_BAD_DATA;
				if (!cell(col, row).IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
				// Tetto ampio ma finito (16 MiB): nessun commento vero si
				// avvicina a questa dimensione, ma senza un tetto un
				// "len" enorme in un file corrotto forzerebbe
				// un'allocazione gigantesca prima che la Read()
				// sottostante possa fallire da sola (stesso principio
				// del tetto su pngLen piu' sopra).
				if (len < 0 || len > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string text;
				if (len > 0)
				{
					text.resize(len);
					if (source->Read(&text[0], len) != len)
						return B_BAD_DATA;
				}
				doc->SetComment(cell(col, row), text);
			}
		}
	}

	// Sezione collegamenti ipertestuali, in coda: stesso schema esatto
	// dei commenti sopra.
	{
		int32 linkCount = 0;
		ssize_t got = source->Read(&linkCount, sizeof(linkCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(linkCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < linkCount; i++)
			{
				int16 row, col;
				int32 len;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
					return B_BAD_DATA;
				if (!cell(col, row).IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
				// Stesso tetto e stesso motivo della sezione commenti sopra.
				if (len < 0 || len > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string text;
				if (len > 0)
				{
					text.resize(len);
					if (source->Read(&text[0], len) != len)
						return B_BAD_DATA;
				}
				doc->SetHyperlink(cell(col, row), text);
			}
		}
	}

	// Sezione tipo di grafico incorporato, in coda: stesso schema
	// EOF-tollerante delle sezioni sopra. Assegna nello STESSO ordine
	// dell'array charts gia' popolato piu' sopra in questa funzione
	// (vedi il commento gemello in SaveASCD) -- un file scritto prima
	// di questa modifica (o senza questa sezione) lascia ogni
	// ChartObject al tipo predefinito (eBarChart).
	{
		int32 chartTypeCount = 0;
		ssize_t got = source->Read(&chartTypeCount, sizeof(chartTypeCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(chartTypeCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < chartTypeCount; i++)
			{
				int8 type;
				if (source->Read(&type, sizeof(type)) != (ssize_t)sizeof(type))
					return B_BAD_DATA;
				if (charts && i < (int32)charts->size())
					(*charts)[i].type = (ChartType)type;
			}
		}
	}

	// Sezione colore del bordo di cella non predefinito, in coda:
	// stesso schema EOF-tollerante delle sezioni sopra (vedi il
	// commento gemello in SaveASCD). Un file scritto prima di questa
	// modifica lascia ogni bordo nero (CellStyle::fBorderColor
	// predefinito), stesso aspetto della Fase 11.
	{
		int32 borderColorCount = 0;
		ssize_t got = source->Read(&borderColorCount, sizeof(borderColorCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(borderColorCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < borderColorCount; i++)
			{
				int16 row, col;
				rgb_color color;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&color, sizeof(color)) != (ssize_t)sizeof(color))
					return B_BAD_DATA;

				cell c(col, row);
				CellStyle cs;
				doc->GetCellStyle(c, cs);
				cs.fBorderColor = color;
				doc->SetCellStyle(c, cs);
			}
		}
	}

	// Sezione convalida dati, in coda: stesso schema EOF-tollerante
	// delle sezioni sopra (vedi il commento gemello in SaveASCD).
	{
		int32 validationCount = 0;
		ssize_t got = source->Read(&validationCount, sizeof(validationCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(validationCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < validationCount; i++)
			{
				int16 row, col;
				int8 type;
				int32 len;
				if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row)
					|| source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col)
					|| source->Read(&type, sizeof(type)) != (ssize_t)sizeof(type)
					|| source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
					return B_BAD_DATA;
				if (!cell(col, row).IsValid())
					return B_BAD_DATA; // "col"/"row" grezzi dal file, non ancora validati
				// Stesso tetto e stesso motivo della sezione commenti piu' sopra.
				if (len < 0 || len > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string list;
				if (len > 0)
				{
					list.resize(len);
					if (source->Read(&list[0], len) != len)
						return B_BAD_DATA;
				}

				double min, max;
				if (source->Read(&min, sizeof(min)) != (ssize_t)sizeof(min)
					|| source->Read(&max, sizeof(max)) != (ssize_t)sizeof(max))
					return B_BAD_DATA;

				ValidationRule rule;
				rule.type = (ValidationType)type;
				rule.list = list;
				rule.min = min;
				rule.max = max;
				doc->SetValidation(cell(col, row), rule);
			}
		}
	}

	// Sezione formattazione condizionale VIVA, in coda: stesso schema
	// EOF-tollerante delle sezioni sopra (vedi il commento gemello in
	// SaveASCD).
	{
		int32 ruleCount = 0;
		ssize_t got = source->Read(&ruleCount, sizeof(ruleCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(ruleCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < ruleCount; i++)
			{
				int8 type;
				int32 valueLen;
				if (source->Read(&type, sizeof(type)) != (ssize_t)sizeof(type)
					|| source->Read(&valueLen, sizeof(valueLen)) != (ssize_t)sizeof(valueLen))
					return B_BAD_DATA;
				// Stesso tetto e stesso motivo della sezione commenti piu' sopra.
				if (valueLen < 0 || valueLen > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string compareValue;
				if (valueLen > 0)
				{
					compareValue.resize(valueLen);
					if (source->Read(&compareValue[0], valueLen) != valueLen)
						return B_BAD_DATA;
				}

				rgb_color bgColor;
				if (source->Read(&bgColor, sizeof(bgColor)) != (ssize_t)sizeof(bgColor))
					return B_BAD_DATA;

				int32 rangeCount;
				if (source->Read(&rangeCount, sizeof(rangeCount)) != (ssize_t)sizeof(rangeCount))
					return B_BAD_DATA;

				ConditionalFormatRule rule;
				rule.type = (CondFormatRuleType)type;
				rule.compareValue = compareValue;
				rule.bgColor = bgColor;

				for (int32 r = 0; r < rangeCount; r++)
				{
					int16 left, top, right, bottom;
					if (source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
						|| source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
						|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right)
						|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
						return B_BAD_DATA;
					rule.ranges.push_back(range(left, top, right, bottom));
				}

				doc->AddConditionalFormatRule(rule);
			}
		}
	}

	// Sezione tabelle strutturate di Excel, in coda: stesso schema
	// EOF-tollerante delle sezioni sopra (vedi il commento gemello in
	// SaveASCD sul perche' questa sezione e' essenziale, non solo per
	// coerenza col resto del formato).
	{
		int32 tableCount = 0;
		ssize_t got = source->Read(&tableCount, sizeof(tableCount));
		if (got != 0)
		{
			if (got != (ssize_t)sizeof(tableCount))
				return B_BAD_DATA;

			for (int32 i = 0; i < tableCount; i++)
			{
				int32 nameLen;
				if (source->Read(&nameLen, sizeof(nameLen)) != (ssize_t)sizeof(nameLen))
					return B_BAD_DATA;
				// Stesso tetto e stesso motivo della sezione commenti piu' sopra.
				if (nameLen < 0 || nameLen > 16 * 1024 * 1024)
					return B_BAD_DATA;

				std::string name;
				if (nameLen > 0)
				{
					name.resize(nameLen);
					if (source->Read(&name[0], nameLen) != nameLen)
						return B_BAD_DATA;
				}

				int16 left, top, right, bottom;
				if (source->Read(&left, sizeof(left)) != (ssize_t)sizeof(left)
					|| source->Read(&top, sizeof(top)) != (ssize_t)sizeof(top)
					|| source->Read(&right, sizeof(right)) != (ssize_t)sizeof(right)
					|| source->Read(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom))
					return B_BAD_DATA;

				int32 columnCount;
				if (source->Read(&columnCount, sizeof(columnCount)) != (ssize_t)sizeof(columnCount))
					return B_BAD_DATA;
				// Stesso tetto delle celle "count" all'inizio del file: una
				// tabella con milioni di colonne dichiarate e' certamente un
				// file manomesso/corrotto, non un'allocazione da tentare.
				if (columnCount < 0 || columnCount > kColCount)
					return B_BAD_DATA;

				CTableDef def;
				def.dataRange = range(left, top, right, bottom);
				for (int32 c = 0; c < columnCount; c++)
				{
					int32 colLen;
					if (source->Read(&colLen, sizeof(colLen)) != (ssize_t)sizeof(colLen))
						return B_BAD_DATA;
					if (colLen < 0 || colLen > 16 * 1024 * 1024)
						return B_BAD_DATA;

					std::string colName;
					if (colLen > 0)
					{
						colName.resize(colLen);
						if (source->Read(&colName[0], colLen) != colLen)
							return B_BAD_DATA;
					}
					def.columnNames.push_back(colName);
				}

				doc->AddTable(name, def);
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
			&sheet.rowHeights, &sheet.frozenRows, &sheet.frozenCols, &sheet.images,
			&sheet.showGrid, &sheet.hasTabColor, &sheet.tabColor,
			&sheet.hiddenRows, &sheet.hasAutoFilter, &sheet.autoFilterRange);
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
			&sheet.rowHeights, &sheet.frozenRows, &sheet.frozenCols, &sheet.images,
			&sheet.showGrid, &sheet.hasTabColor, &sheet.tabColor,
			&sheet.hiddenRows, &sheet.hasAutoFilter, &sheet.autoFilterRange);
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
