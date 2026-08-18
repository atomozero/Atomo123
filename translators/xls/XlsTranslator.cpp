/*
	XlsTranslator.cpp

	Vedi XlsTranslator.h per la descrizione generale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "XlsTranslator.h"

#include <cstring>
#include <utility>
#include <vector>

#include <Cursor.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Roster.h>
#include <String.h>
#include <StringView.h>
#include <View.h>

#include "Cell.h"
#include "Value.h"
#include "CellStyle.h"
#include "Container.h"
#include "CellIterator.h"
#include "Excel.h"
#include "EngineViewStub.h"
#include "FontMetrics.h"
#include "EmbeddedImage.h"

static const translation_format sInputFormats[] = {
	{
		kAtomoXlsFormat, kAtomoSheetGroup, 0.8f, 0.8f,
		"application/vnd.ms-excel", "Microsoft Excel 97-2003 (XLS)"
	}
};

static const translation_format sOutputFormats[] = {
	{
		kAtomoNativeFormat, kAtomoSheetGroup, 1.0f, 1.0f,
		"application/x-vnd.atomo-sheet-data", "Atomo Sheet Cell Data (ASCD)"
	}
};

// Firma standard degli OLE2 Compound File Binary (usati da XLS legacy,
// DOC, e altri formati Microsoft Office pre-2007): 8 byte fissi.
static const unsigned char kOLE2Signature[8] =
	{ 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
static const int32 kASCDVersion = 1;

// Stessa serializzazione ASCD del translator CSV (vedi
// translators/csv/CsvTranslator.cpp per la descrizione completa):
// duplicata qui deliberatamente, per mantenere ogni translator un
// add-on autonomo senza dipendenze incrociate. Se in futuro servono
// piu' translator con lo stesso formato di uscita, vale la pena
// estrarla in una libreria condivisa.
static status_t WriteASCD(CContainer* doc, BPositionIO* dest,
	const std::vector<std::pair<int, float> >* colWidths = NULL,
	const std::vector<EmbeddedImage>* images = NULL,
	const std::vector<range>* mergedRanges = NULL,
	const std::vector<std::pair<int, float> >* rowHeights = NULL)
{
	// Range completo invece dei limiti di GetBounds: una cella con
	// formula non ancora calcolata (mType eNoData) verrebbe esclusa
	// dai limiti calcolati da GetBounds, e se e' anche la cella piu' a
	// destra/in basso del foglio sparirebbe del tutto dal file
	// prodotto -- bug scoperto e corretto costruendo l'export ODS
	// (vedi ROADMAP.md, Fase 5).
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

	// Da qui in poi, stesse sezioni opzionali in coda di
	// translators/xlsx/XlsxTranslator.cpp (vedi il commento su
	// ui/src/AscdIO.cpp per il motivo per cui ogni sezione va scritta
	// SEMPRE, anche vuota: LoadASCD le legge in un ordine fisso, e
	// omettere una sezione disallineerebbe la lettura di quella dopo).
	// Font/allineamento/formato numero/testo a capo sono scritti con i
	// valori REALI: CExcel5Filter (motore, vedi Excel.pass1.cpp) li
	// risolve gia' correttamente per ogni cella durante Translate()
	// tramite Xf()/Font(), esattamente come XlsxTranslator fa per XLSX
	// -- mancava solo la scrittura di queste sezioni qui, che finora
	// scartava tutto tranne il testo/valore grezzo di ogni cella (bug
	// reale: un titolo in grassetto nel file originale, o una cella
	// con formato "00000" per gli zeri iniziali, sparivano del tutto
	// aprendo il file con Atomo123). Sfondo/testo colorato, bordi,
	// celle unite e immagini incorporate sono ora scritti con i
	// valori REALI anch'essi (fLowColor/fHighColor/fTBorderColor ecc.
	// risolti da Xf(), MERGEDCELLS/MSODRAWING -- vedi i commenti in
	// Excel.pass1.cpp/Excel.escher.cpp).

	int32 chartCount = 0;
	if (dest->Write(&chartCount, sizeof(chartCount)) != (ssize_t)sizeof(chartCount))
		return B_IO_ERROR;

	// Larghezze di colonna esplicite: CExcel5Filter::GetColumnWidths(),
	// popolata da record COLINFO durante Translate() (vedi il
	// commento in Excel.pass1.cpp) -- prima sempre vuota, dato che
	// COLINFO scriveva la larghezza vera solo nella vista (fCellView,
	// sempre NULL in un translator headless), mai in un posto che
	// questo translator potesse rileggere dopo Translate().
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

	// Colori di sfondo/testo di cella non predefiniti: fLowColor viene
	// da Xf() (pattern di riempimento del record XF, vedi il commento
	// in Excel.pass1.cpp), fHighColor dal colore del FONT associato
	// (gia' risolto da Font() e conservato dentro gFontSizeTable, non
	// in CellStyle -- va quindi ripescato qui con GetFontInfo, stesso
	// indice fFont gia' usato per la sezione "font" sopra).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, CellStyle> > toWrite;
		CCellIterator colorIter(doc, NULL);
		cell cc;
		while (colorIter.NextExisting(cc))
		{
			CellStyle cs;
			doc->GetCellStyle(cc, cs);

			rgb_color fontColor = defaultStyle.fHighColor;
			font_family family;
			font_style fstyle;
			float size;
			gFontSizeTable.GetFontInfo(cs.fFont, &family, &fstyle, &size, &fontColor);
			// kExcelColorTable (Excel.colors.h) e il colore "automatico"
			// di ripiego in Font() (Excel.pass1.cpp) hanno entrambi
			// alpha=0: non e' un canale alfa vero, l'app usa sempre
			// colori opachi (vedi CellStyle::CellStyle, alpha=255 per
			// entrambi i colori). Senza normalizzarlo qui, OGNI cella
			// con un font risolto da Font() finirebbe (erroneamente)
			// nella sezione colori solo per l'alpha diverso dal
			// predefinito, anche senza alcun colore testo esplicito.
			fontColor.alpha = 255;
			cs.fHighColor = fontColor;

			if (memcmp(&cs.fLowColor, &defaultStyle.fLowColor, sizeof(rgb_color)) != 0
				|| memcmp(&cs.fHighColor, &defaultStyle.fHighColor, sizeof(rgb_color)) != 0)
				toWrite.push_back(std::make_pair(cc, cs));
		}

		int32 cellColorCount = (int32)toWrite.size();
		if (dest->Write(&cellColorCount, sizeof(cellColorCount)) != (ssize_t)sizeof(cellColorCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < cellColorCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			const CellStyle& cs = toWrite[i].second;
			uint8 buf[8] = { cs.fLowColor.red, cs.fLowColor.green, cs.fLowColor.blue, cs.fLowColor.alpha,
				cs.fHighColor.red, cs.fHighColor.green, cs.fHighColor.blue, cs.fHighColor.alpha };
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(buf, sizeof(buf)) != (ssize_t)sizeof(buf))
				return B_IO_ERROR;
		}
	}

	// Colori di colonna: non applicabile a XLS (nessun record BIFF
	// equivalente al default di colonna XLSX), sezione sempre vuota.
	int32 columnColorCount = 0;
	if (dest->Write(&columnColorCount, sizeof(columnColorCount)) != (ssize_t)sizeof(columnColorCount))
		return B_IO_ERROR;

	// Altezze di riga esplicite: CExcel5Filter::GetRowHeights(),
	// popolata da record ROW durante Translate() (vedi il commento in
	// Excel.pass1.cpp) -- prima sempre vuota, stesso motivo dichiarato
	// per le larghezze di colonna qui sopra prima del relativo fix:
	// ogni riga ridimensionata a mano nel file originale tornava
	// sempre all'altezza predefinita, bug reale scoperto confrontando
	// con Excel vero una fattura reale.
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

	int32 frozenRows = 0, frozenCols = 0;
	if (dest->Write(&frozenRows, sizeof(frozenRows)) != (ssize_t)sizeof(frozenRows)
		|| dest->Write(&frozenCols, sizeof(frozenCols)) != (ssize_t)sizeof(frozenCols))
		return B_IO_ERROR;

	// Font di cella non predefinito: famiglia/stile/dimensione gia'
	// risolti (mai l'indice grezzo di CellStyle::fFont, volatile fra
	// una sessione e l'altra -- stesso motivo di XlsxTranslator.cpp e
	// di ui/src/AscdIO.cpp).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, CellStyle> > toWrite;
		CCellIterator fontIter(doc, NULL);
		cell fc;
		while (fontIter.NextExisting(fc))
		{
			CellStyle cs;
			doc->GetCellStyle(fc, cs);
			if (cs.fFont != defaultStyle.fFont)
				toWrite.push_back(std::make_pair(fc, cs));
		}

		int32 fontCount = (int32)toWrite.size();
		if (dest->Write(&fontCount, sizeof(fontCount)) != (ssize_t)sizeof(fontCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < fontCount; i++)
		{
			int16 row = toWrite[i].first.v, col = toWrite[i].first.h;
			font_family family;
			font_style fstyle;
			float size;
			gFontSizeTable.GetFontInfo(toWrite[i].second.fFont, &family, &fstyle, &size);
			if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row)
				|| dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col)
				|| dest->Write(family, sizeof(font_family)) != (ssize_t)sizeof(font_family)
				|| dest->Write(fstyle, sizeof(font_style)) != (ssize_t)sizeof(font_style)
				|| dest->Write(&size, sizeof(size)) != (ssize_t)sizeof(size))
				return B_IO_ERROR;
		}
	}

	// Allineamento orizzontale di cella non predefinito.
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

	// Bordi di cella non predefiniti: CellStyle::fTBorderColor/
	// fLBorderColor/fBBorderColor/fRBorderColor, gia' risolti da Xf()
	// come presenza booleana per lato (0/1, nessun vero colore/
	// spessore -- vedi il commento in Excel.pass1.cpp e in
	// ui/src/AscdIO.cpp sullo stesso campo per XLSX).
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

	// Formato numero di cella non predefinito: CellStyle::fFormat,
	// gia' risolto da Xf() (built-in Sum-It o gFormatTable::GetFormatID
	// su un record FORMAT reale, vedi Excel.pass1.cpp).
	{
		CellStyle defaultStyle;
		std::vector<std::pair<cell, int32> > toWrite;
		CCellIterator formatIter(doc, NULL);
		cell fmc;
		while (formatIter.NextExisting(fmc))
		{
			CellStyle cs;
			doc->GetCellStyle(fmc, cs);
			if (cs.fFormat != defaultStyle.fFormat)
				toWrite.push_back(std::make_pair(fmc, (int32)cs.fFormat));
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

	// Sottolineato: non ancora estratto dal record FONT legacy (limite
	// dichiarato, il campo esiste nel BIFF ma questo filtro non lo
	// legge ancora), sezione sempre vuota.
	int32 underlineCount = 0;
	if (dest->Write(&underlineCount, sizeof(underlineCount)) != (ssize_t)sizeof(underlineCount))
		return B_IO_ERROR;

	// Testo a capo di cella non predefinito: CellStyle::fWrapText, bit
	// 3 del byte di allineamento del record XF, letto da Xf() insieme
	// all'allineamento orizzontale sopra (nessun record separato).
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

	// Celle unite: CExcel5Filter::GetMergedRanges() (motore, vedi il
	// case MERGEDCELLS in Excel.pass1.cpp) estrae davvero il record
	// BIFF -- va quindi scritta con i valori reali.
	{
		int32 mergeCount = mergedRanges ? (int32)mergedRanges->size() : 0;
		if (dest->Write(&mergeCount, sizeof(mergeCount)) != (ssize_t)sizeof(mergeCount))
			return B_IO_ERROR;

		for (int32 i = 0; i < mergeCount; i++)
		{
			const range& r = (*mergedRanges)[i];
			int16 top = r.top, left = r.left, bottom = r.bottom, right = r.right;
			if (dest->Write(&top, sizeof(top)) != (ssize_t)sizeof(top)
				|| dest->Write(&left, sizeof(left)) != (ssize_t)sizeof(left)
				|| dest->Write(&bottom, sizeof(bottom)) != (ssize_t)sizeof(bottom)
				|| dest->Write(&right, sizeof(right)) != (ssize_t)sizeof(right))
				return B_IO_ERROR;
		}
	}

	// Immagini incorporate: CExcel5Filter::GetImages() (motore, vedi
	// Excel.escher.cpp) estrae davvero i record MSODRAWINGGROUP/
	// MSODRAWING (Escher) -- va quindi scritta con i valori reali,
	// stesso formato dell'immagine cosi' com'e' nel file originale
	// (JPEG/PNG, decodificati dal Translation Kit al disegno, vedi
	// EmbeddedImage.h), solo JPEG/PNG supportati (limite dichiarato
	// per DIB/EMF/WMF/PICT/TIFF, mai osservati finora su un file
	// reale).
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

	return B_OK;
}

CXlsTranslator::CXlsTranslator()
	: BTranslator()
{
}

CXlsTranslator::~CXlsTranslator()
{
}

const char* CXlsTranslator::TranslatorName() const
{
	return "XLS Legacy Translator";
}

const char* CXlsTranslator::TranslatorInfo() const
{
	return "Importa fogli di calcolo dal formato binario Excel 97-2003 (XLS)";
}

int32 CXlsTranslator::TranslatorVersion() const
{
	return B_TRANSLATION_MAKE_VERSION(1, 0, 0);
}

// Link "Offrimi un caffe'" cliccabile, stesso schema/motivo di
// CCsvCoffeeLink in translators/csv/CsvTranslator.cpp (vedi quel
// commento per il perche' non e' un ClickableStringView di ui/src/).
static const char kCoffeeUrl[] = "https://buymeacoffee.com/atomozero";

class CXlsCoffeeLink : public BStringView {
public:
	CXlsCoffeeLink() : BStringView("coffeeLink", "Offrimi un caffe' \xE2\x98\x95") {}

	virtual void AttachedToWindow()
	{
		BStringView::AttachedToWindow();
		SetHighColor(40, 80, 200);
	}

	virtual void MouseDown(BPoint)
	{
		const char* arg = kCoffeeUrl;
		be_roster->Launch("application/x-vnd.Be.URL.https", 1, const_cast<char**>(&arg));
	}

	virtual void MouseMoved(BPoint, uint32, const BMessage*)
	{
		BCursor link(B_CURSOR_ID_FOLLOW_LINK);
		SetViewCursor(&link);
	}
};

// Vista "Informazioni" mostrata dal pannello Translators di Haiku,
// stesso schema/motivo di CCsvConfigView in translators/csv/
// CsvTranslator.cpp (vedi quel commento per il perche').
class CXlsConfigView : public BView {
public:
	CXlsConfigView(BRect frame)
		:
		BView(frame, "XlsConfigView", B_FOLLOW_ALL, B_WILL_DRAW)
	{
		SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

		BStringView* title = new BStringView("title", "XLS Legacy Translator");
		title->SetFont(be_bold_font);

		int32 v = B_TRANSLATION_MAKE_VERSION(1, 0, 0);
		BString versionText;
		versionText.SetToFormat("Versione %d.%d.%d", (int)B_TRANSLATION_MAJOR_VERSION(v),
			(int)B_TRANSLATION_MINOR_VERSION(v), (int)B_TRANSLATION_REVISION_VERSION(v));

		BFont small(be_plain_font);
		small.SetSize(be_plain_font->Size() - 1);

		BStringView* version = new BStringView("version", versionText.String());
		version->SetFont(&small);
		version->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.7));

		BStringView* info = new BStringView("info",
			"Importa fogli di calcolo dal formato binario\n"
			"Excel 97-2003 (XLS). Solo import: l'export verso\n"
			"l'ecosistema Excel passa dal translator XLSX.");

		BStringView* copyright = new BStringView("copyright",
			"Copyright (c) 2026 Andrea Bernardi \xC2\xB7 Licenza MIT");
		copyright->SetFont(&small);
		copyright->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.6));

		CXlsCoffeeLink* coffeeLink = new CXlsCoffeeLink();
		coffeeLink->SetFont(&small);

		BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS)
			.Add(title)
			.Add(version)
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(info)
			.AddGlue()
			.Add(coffeeLink)
			.Add(copyright)
		.End();
	}
};

status_t CXlsTranslator::MakeConfigurationView(BMessage* extension, BView** _view,
	BRect* _extent)
{
	if (_view == NULL || _extent == NULL)
		return B_BAD_VALUE;

	CXlsConfigView* view = new CXlsConfigView(BRect(0, 0, 279, 144));
	*_view = view;
	*_extent = view->Bounds();
	return B_OK;
}

const translation_format* CXlsTranslator::InputFormats(int32* _count) const
{
	*_count = 1;
	return sInputFormats;
}

const translation_format* CXlsTranslator::OutputFormats(int32* _count) const
{
	*_count = 1;
	return sOutputFormats;
}

status_t CXlsTranslator::Identify(BPositionIO* source,
	const translation_format* format, BMessage* extension,
	translator_info* info, uint32 outType)
{
	off_t pos = source->Position();
	unsigned char header[8];
	ssize_t read = source->Read(header, 8);
	source->Seek(pos, SEEK_SET);

	if (read != 8 || memcmp(header, kOLE2Signature, 8) != 0)
		return B_NO_TRANSLATOR;

	info->type = kAtomoXlsFormat;
	info->group = kAtomoSheetGroup;
	info->quality = 0.8f;
	info->capability = 0.8f;
	strlcpy(info->name, "Microsoft Excel 97-2003 (XLS)", sizeof(info->name));
	strlcpy(info->MIME, "application/vnd.ms-excel", sizeof(info->MIME));

	return B_OK;
}

status_t CXlsTranslator::Translate(BPositionIO* source,
	const translator_info* info, BMessage* extension, uint32 outType,
	BPositionIO* destination)
{
	// info puo' essere NULL (documentato nel Translation Kit: significa
	// "identifica tu stesso il formato sorgente") -- crash reale di
	// Tracker, non solo teorico: il thumbnail worker chiama
	// BTranslatorRoster::Translate() cosi' per ogni file mentre genera
	// le anteprime, senza mai passare da Identify() prima.
	translator_info localInfo;
	if (!info)
	{
		if (Identify(source, NULL, extension, &localInfo, outType) != B_OK)
			return B_NO_TRANSLATOR;
		info = &localInfo;
	}

	if (info->type != kAtomoXlsFormat)
		return B_NO_TRANSLATOR;
	if (outType != 0 && outType != kAtomoNativeFormat)
		return B_NO_TRANSLATOR;

	CContainer* doc = new CContainer(NULL, NULL);
	status_t err = B_OK;
	std::vector<std::pair<int, float> > colWidths;
	std::vector<EmbeddedImage> images;
	std::vector<range> mergedRanges;
	std::vector<std::pair<int, float> > rowHeights;

	try
	{
		// cellView=NULL: nessuna UI collegata (translator headless).
		// Il costruttore legge subito il flusso e popola doc; i nomi
		// di intervallo vengono scartati in questa modalita' -- vedi
		// la nota nello stub in engine/src/Stubs/EngineViewStub.h. Le
		// larghezze di colonna, le altezze di riga, le immagini
		// incorporate e le celle unite invece si recuperano comunque
		// da GetColumnWidths()/GetRowHeights()/GetImages()/
		// GetMergedRanges(), che CExcel5Filter popola indipendentemente
		// da "cellView" (vedi il commento in Excel.h).
		CExcel5Filter filter(*source, NULL, doc);
		filter.Translate();
		colWidths = filter.GetColumnWidths();
		images = filter.GetImages();
		mergedRanges = filter.GetMergedRanges();
		rowHeights = filter.GetRowHeights();
	}
	catch (...)
	{
		err = B_BAD_DATA;
	}

	if (err == B_OK)
		err = WriteASCD(doc, destination, &colWidths, &images, &mergedRanges, &rowHeights);

	doc->Release();
	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
		return new CXlsTranslator();
	return NULL;
}
