/*
	Excel.escher.cpp

	Immagini incorporate nel filtro XLS legacy (record MSODRAWINGGROUP/
	MSODRAWING, formato binario Escher di Microsoft Office -- la
	controparte BIFF di xl/drawings/+xl/media/ usato da XLSX, molto
	piu' semplice, gia' importato in Fase 12). Struttura verificata
	byte per byte contro una fattura reale (logo aziendale incorporato
	come JPEG) prima di scrivere questo file, non dedotta a memoria
	dalla sola specifica MS-ODRAW.

	MSODRAWINGGROUP e' un record unico per l'intera cartella di lavoro:
	contiene il "blip store" (DggContainer -> BstoreContainer -> una
	voce BSE per immagine, con i byte grezzi dell'immagine cosi' come
	nel file originale -- JPEG/PNG, direttamente decodificabili dal
	Translation Kit di Haiku senza bisogno di riconvertirli, vedi
	EmbeddedImage.h). MSODRAWING e' per-forma: contiene l'ancoraggio
	riga/colonna/scarto (ClientAnchor) piu' un indice nel blip store
	(proprieta' "pib" del record Opt) -- una forma senza questa
	proprieta' non e' un'immagine (es. le caselle di spunta della
	stessa fattura, che usano un Opt diverso, senza "pib": si escludono
	da sole, senza bisogno di controllare il tipo di forma).
*/

#include "Excel.h"
#include "XL_Biff_codes.h"

#include <cstring>

namespace {

// Un record Escher (non BIFF: gli 8 byte di intestazione sono
// verInstance(2)+fbt(2)+length(4), un formato completamente diverso
// dai 4 byte code(2)+len(2) del resto del BIFF) -- "container" quando
// i 4 bit di versione sono tutti a 1, nel qual caso "length" e' la
// lunghezza dei record Escher figli invece che di dati grezzi.
struct EscherRecord {
	unsigned short fbt;
	unsigned short instance;
	bool isContainer;
	unsigned int length;
	size_t contentOffset;
};

bool ReadEscherHeader(const unsigned char* buf, size_t bufLen, size_t pos, EscherRecord* out)
{
	if (pos + 8 > bufLen)
		return false;
	unsigned short verInst = buf[pos] | ((unsigned short)buf[pos + 1] << 8);
	out->fbt = buf[pos + 2] | ((unsigned short)buf[pos + 3] << 8);
	out->length = buf[pos + 4] | ((unsigned int)buf[pos + 5] << 8)
		| ((unsigned int)buf[pos + 6] << 16) | ((unsigned int)buf[pos + 7] << 24);
	out->instance = verInst >> 4;
	out->isContainer = (verInst & 0xF) == 0xF;
	out->contentOffset = pos + 8;
	return true;
}

const unsigned short kFbtBstoreContainer = 0xF001;
const unsigned short kFbtBSE = 0xF007;
const unsigned short kFbtSpContainer = 0xF004;
const unsigned short kFbtOpt = 0xF00B;
const unsigned short kFbtClientAnchor = 0xF010;
const unsigned short kPidBlipIndex = 0x104; // "pib", indice 1-based nel blip store

// Larghezza/altezza predefinite in pixel (kColWidth/kRowHeight di
// ui/src/SheetView.h, duplicate qui deliberatamente -- stesso
// principio di autonomia fra moduli di kExcelColorTable in
// Excel.colors.h: il motore non linka contro l'Interface Kit e non
// puo' includere un header della UI).
const float kDefaultColWidthPixels = 80.0f;
const float kDefaultRowHeightPixels = 20.0f;

// Cammina i record Escher in [start,end) chiamando "visit" per
// ciascuno (contenitore o foglia) -- profondita' limitata contro un
// file corrotto con contenitori che si annidano all'infinito.
template<typename Visitor>
void WalkEscher(const unsigned char* buf, size_t start, size_t end, Visitor visit, int depth = 0)
{
	if (depth > 32)
		return;
	size_t pos = start;
	while (pos + 8 <= end)
	{
		EscherRecord rec;
		if (!ReadEscherHeader(buf, end, pos, &rec))
			break;
		size_t contentEnd = rec.contentOffset + rec.length;
		if (contentEnd > end || contentEnd < rec.contentOffset) // overflow o oltre il buffer disponibile
			contentEnd = end;
		visit(rec, buf, rec.contentOffset, contentEnd);
		if (rec.isContainer)
			WalkEscher(buf, rec.contentOffset, contentEnd, visit, depth + 1);
		pos = contentEnd;
	}
}

// Tipi BLIP Windows (campo "btWin32" della voce BSE) per cui esiste
// un translator Haiku in grado di decodificarli direttamente cosi'
// come sono (vedi SheetView.cpp, DecodeImageBytes): JPEG e PNG. DIB/
// EMF/WMF/PICT/TIFF restano un limite dichiarato -- servirebbe
// riconvertirli (un header BMP da sintetizzare per DIB, un
// decodificatore vettoriale per EMF/WMF) per un guadagno visivo
// marginale rispetto ai casi reali osservati finora.
bool IsSupportedBlipType(int type)
{
	return type == 5 /* JPEG */ || type == 6 /* PNG */;
}

// Dimensione nativa (larghezza/altezza in pixel) di un JPEG o PNG
// grezzo -- serve per dare all'immagine incorporata una dimensione
// visualizzata ragionevole quando il rettangolo di ancoraggio Escher
// non viene tradotto in pixel esatti (vedi il commento in
// HandleMsoDrawing sotto sul perche' non si prova a farlo). Stesso
// principio di PngDimensions in XlsxTranslator.cpp, duplicato qui per
// lo stesso motivo di autonomia fra translator/filtro.
bool JpegDimensions(const std::vector<unsigned char>& data, uint32* outW, uint32* outH)
{
	if (data.size() < 4 || data[0] != 0xFF || data[1] != 0xD8)
		return false;

	size_t pos = 2;
	while (pos + 4 <= data.size())
	{
		if (data[pos] != 0xFF)
		{
			pos++;
			continue;
		}
		unsigned char marker = data[pos + 1];
		if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD9))
		{
			pos += 2;
			continue;
		}
		if (pos + 4 > data.size())
			break;
		unsigned int segLen = ((unsigned int)data[pos + 2] << 8) | data[pos + 3];
		bool isSof = (marker >= 0xC0 && marker <= 0xCF)
			&& marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
		if (isSof)
		{
			if (pos + 4 + 5 > data.size())
				return false;
			*outH = ((unsigned int)data[pos + 5] << 8) | data[pos + 6];
			*outW = ((unsigned int)data[pos + 7] << 8) | data[pos + 8];
			return true;
		}
		pos += 2 + segLen;
	}
	return false;
}

bool PngDimensions(const std::vector<unsigned char>& data, uint32* outW, uint32* outH)
{
	static const unsigned char kPngSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
	if (data.size() < 24 || memcmp(&data[0], kPngSig, 8) != 0)
		return false;
	if (memcmp(&data[12], "IHDR", 4) != 0)
		return false;
	*outW = ((uint32)data[16] << 24) | ((uint32)data[17] << 16) | ((uint32)data[18] << 8) | data[19];
	*outH = ((uint32)data[20] << 24) | ((uint32)data[21] << 16) | ((uint32)data[22] << 8) | data[23];
	return true;
}

bool ImageDimensions(int blipType, const std::vector<unsigned char>& data, uint32* outW, uint32* outH)
{
	if (blipType == 6)
		return PngDimensions(data, outW, outH);
	return JpegDimensions(data, outW, outH);
}

// Dimensione visualizzata dal rettangolo di ancoraggio Escher
// (ClientAnchor: colonna/riga iniziale+scarto fino a colonna/riga
// finale+scarto, vedi il commento in HandleMsoDrawing) invece della
// dimensione nativa dell'immagine -- le due NON coincidono quasi mai:
// l'immagine puo' essere stata ridimensionata a mano in Excel dopo
// l'inserimento, e comunque il suo DPI nativo non ha alcun rapporto
// con la larghezza di colonna/altezza di riga del foglio. Usare la
// dimensione nativa (come faceva questa funzione all'inizio)
// produceva un logo visibilmente piu' grande del rettangolo che
// Excel stesso disegna -- bug reale scoperto confrontando
// visivamente con Excel vero l'importazione di una fattura reale.
// "span" somma le colonne/righe intere attraversate piu' la frazione
// di quella iniziale/finale coperta dallo scarto (in 1/1024 per le
// colonne, 1/256 per le righe, stessa unita' di offsetX/offsetY).
float SpanPixels(unsigned short start, unsigned short startFrac,
	unsigned short end, unsigned short endFrac, unsigned short fracDenom,
	float (*sizeOf)(unsigned short, void*), void* ctx)
{
	if (end == start)
		return sizeOf(start, ctx) * (endFrac - (float)startFrac) / fracDenom;

	float total = sizeOf(start, ctx) * (fracDenom - (float)startFrac) / fracDenom;
	for (unsigned short c = start + 1; c < end; c++)
		total += sizeOf(c, ctx);
	total += sizeOf(end, ctx) * (float)endFrac / fracDenom;
	return total;
}

} // namespace

float CExcel5Filter::ColWidthPixels(int col) const
{
	for (size_t i = 0; i < fColWidths.size(); i++)
	{
		if (fColWidths[i].first == col)
			return fColWidths[i].second;
	}
	return kDefaultColWidthPixels;
}

std::vector<unsigned char> CExcel5Filter::ReadRecordWithContinues(int len)
{
	std::vector<unsigned char> buf((unsigned short)len);
	if (len > 0)
		fBook.Read(&buf[0], (unsigned short)len);

	for (;;)
	{
		off_t savedPos = fBook.Position();
		if (savedPos + 4 > fBook.BufferLength())
			break;

		short nextCode, nextLen;
		CExcelStream peek(fBook);
		peek >> nextCode >> nextLen;
		if (nextCode != CONTINUE)
		{
			fBook.Seek(savedPos, SEEK_SET);
			break;
		}

		size_t oldSize = buf.size();
		buf.resize(oldSize + (unsigned short)nextLen);
		if (nextLen > 0)
			fBook.Read(&buf[oldSize], (unsigned short)nextLen);
	}

	return buf;
}

void CExcel5Filter::HandleMsoDrawingGroup(int len)
{
	// Il blip store di TUTTA la cartella di lavoro supera quasi
	// sempre la dimensione massima di un record BIFF anche per una
	// sola immagine di piccole dimensioni -- confermato aprendo una
	// fattura reale (un logo di 9 KB, un solo record CONTINUE).
	std::vector<unsigned char> buf = ReadRecordWithContinues(len);

	fBlips.clear();
	if (buf.empty())
		return;

	const unsigned char* base = &buf[0];
	size_t total = buf.size();

	WalkEscher(base, 0, total, [this](const EscherRecord& rec, const unsigned char* b, size_t contentStart, size_t contentEnd)
	{
		if (rec.fbt != kFbtBSE)
			return;

		// BSE (Blip Store Entry), intestazione fissa a 36 byte:
		// btWin32(1) btMacOS(1) rgbUid(16) tag(2) size(4) cRef(4)
		// foDelay(4) usage(1) cbName(1) unused(2) -- poi il record
		// BLIP vero (propria intestazione Escher a 8 byte).
		if (contentEnd < contentStart || contentEnd - contentStart < 36)
			return;
		int blipType = b[contentStart];

		EscherRecord blipRec;
		if (!ReadEscherHeader(b, contentEnd, contentStart + 36, &blipRec))
			return;
		size_t blipContentEnd = blipRec.contentOffset + blipRec.length;
		if (blipContentEnd > contentEnd || blipContentEnd < blipRec.contentOffset)
			blipContentEnd = contentEnd;

		// rgbUid(16) + tag(1) prima dei byte grezzi dell'immagine --
		// la variante a un solo UID, l'unica osservata sui file reali
		// finora (la variante "due UID" e' rara: una BSE con quella
		// variante perderebbe solo la propria immagine, non l'intero
		// import, dato che ogni voce del blip store e' indipendente).
		size_t rawStart = blipRec.contentOffset + 17;
		if (rawStart > blipContentEnd)
			return;

		EscherBlip blip;
		blip.type = blipType;
		blip.data.assign(b + rawStart, b + blipContentEnd);
		fBlips.push_back(blip);
	});
}

void CExcel5Filter::HandleMsoDrawing(int len)
{
	std::vector<unsigned char> buf = ReadRecordWithContinues(len);
	if (buf.empty())
		return;

	const unsigned char* base = &buf[0];
	size_t total = buf.size();

	WalkEscher(base, 0, total, [this](const EscherRecord& rec, const unsigned char* b, size_t contentStart, size_t contentEnd)
	{
		if (rec.fbt != kFbtSpContainer)
			return;

		bool hasPib = false;
		unsigned int pib = 0;
		bool hasAnchor = false;
		unsigned short colStart = 0, dxStart = 0, rowStart = 0, dyStart = 0;
		unsigned short colEnd = 0, dxEnd = 0, rowEnd = 0, dyEnd = 0;

		WalkEscher(b, contentStart, contentEnd,
			[&](const EscherRecord& inner, const unsigned char* b2, size_t innerStart, size_t innerEnd)
			{
				if (inner.fbt == kFbtOpt)
				{
					size_t pp = innerStart;
					while (pp + 6 <= innerEnd)
					{
						unsigned short opid = b2[pp] | ((unsigned short)b2[pp + 1] << 8);
						unsigned int value = b2[pp + 2] | ((unsigned int)b2[pp + 3] << 8)
							| ((unsigned int)b2[pp + 4] << 16) | ((unsigned int)b2[pp + 5] << 24);
						if ((opid & 0x3FFF) == kPidBlipIndex)
						{
							hasPib = true;
							pib = value;
						}
						pp += 6;
					}
				}
				else if (inner.fbt == kFbtClientAnchor)
				{
					// flag(2) colStart(2) dxStart(2) rowStart(2)
					// dyStart(2) colEnd(2) dxEnd(2) rowEnd(2) dyEnd(2):
					// il rettangolo intero, non solo l'angolo in alto a
					// sinistra -- serve anche per la dimensione
					// visualizzata (vedi SpanPixels sopra), non solo per
					// la posizione.
					if (innerEnd >= innerStart + 18)
					{
						colStart = b2[innerStart + 2] | ((unsigned short)b2[innerStart + 3] << 8);
						dxStart = b2[innerStart + 4] | ((unsigned short)b2[innerStart + 5] << 8);
						rowStart = b2[innerStart + 6] | ((unsigned short)b2[innerStart + 7] << 8);
						dyStart = b2[innerStart + 8] | ((unsigned short)b2[innerStart + 9] << 8);
						colEnd = b2[innerStart + 10] | ((unsigned short)b2[innerStart + 11] << 8);
						dxEnd = b2[innerStart + 12] | ((unsigned short)b2[innerStart + 13] << 8);
						rowEnd = b2[innerStart + 14] | ((unsigned short)b2[innerStart + 15] << 8);
						dyEnd = b2[innerStart + 16] | ((unsigned short)b2[innerStart + 17] << 8);
						hasAnchor = true;
					}
				}
			});

		if (!hasPib || !hasAnchor || pib < 1 || pib > fBlips.size())
			return;

		const EscherBlip& blip = fBlips[pib - 1];
		if (!IsSupportedBlipType(blip.type))
			return;

		float colWidthPixels = ColWidthPixels(colStart + 1);

		EmbeddedImage img;
		img.anchor = cell(colStart + 1, rowStart + 1);
		img.offsetX = (dxStart / 1024.0f) * colWidthPixels;
		img.offsetY = (dyStart / 256.0f) * kDefaultRowHeightPixels;

		// Larghezza/altezza calcolate dal rettangolo di ancoraggio
		// (colStart/dxStart fino a colEnd/dxEnd, in colonne+righe
		// vere del foglio, vedi SpanPixels sopra) -- NON dalla
		// dimensione nativa dell'immagine, che non ha alcun rapporto
		// con quanto Excel disegna davvero (l'immagine puo' essere
		// stata ridimensionata a mano dopo l'inserimento). L'altezza
		// di riga usa sempre il valore predefinito: questo filtro non
		// importa ancora le altezze di riga personalizzate per XLS
		// (a differenza delle larghezze di colonna, gia' disponibili
		// in fColWidths), un'approssimazione che vale solo se nessuna
		// riga toccata dall'immagine e' stata ridimensionata a mano.
		img.width = SpanPixels(colStart, dxStart, colEnd, dxEnd, 1024,
			[](unsigned short c, void* self) { return ((CExcel5Filter*)self)->ColWidthPixels(c + 1); },
			this);
		img.height = SpanPixels(rowStart, dyStart, rowEnd, dyEnd, 256,
			[](unsigned short, void*) { return kDefaultRowHeightPixels; }, this);

		if (img.width <= 0 || img.height <= 0)
		{
			// Rettangolo di ancoraggio degenere (angolo finale prima
			// di quello iniziale, o coincidente: file corrotto, o una
			// combinazione mai vista finora) -- la dimensione nativa
			// dell'immagine resta un fallback ragionevole piuttosto
			// che disegnarla a dimensione zero/invisibile.
			uint32 natW = 0, natH = 0;
			if (!ImageDimensions(blip.type, blip.data, &natW, &natH) || natW == 0 || natH == 0)
				return;
			img.width = (float)natW;
			img.height = (float)natH;
		}

		img.pngData.assign(blip.data.begin(), blip.data.end());

		fImages.push_back(img);
	});
}
