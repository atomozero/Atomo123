/*
	test_xlsx_translator.cpp

	Test end-to-end del translator XLSX: apre tests/sample.xlsx (un
	file XLSX reale, costruito con il comando "zip" e verificato
	apribile con "unzip -l"), lo traduce in ASCD, poi legge l'ASCD
	prodotto e verifica che i valori e la formula siano stati
	importati e calcolati correttamente dal motore.

	sample.xlsx contiene:
		A1 = 15
		B1 = 25
		C1 = formula =A1+B1 (valore già calcolato da Excel/LibreOffice: 40 —
		     il translator lo ignora e lascia che il nostro motore
		     ricalcoli la formula in modo indipendente)
		D1 = stringa condivisa "Ciao XLSX"
*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <Application.h>
#include <File.h>
#include <DataIO.h>
#include <Font.h>
#include <String.h>
#include <SupportDefs.h>

#include "XlsxTranslator.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "CellStyle.h"

static int gFailures = 0;

static void Check(bool condition, const char *what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

static const char kASCDMagicForTest[4] = { 'A', 'S', 'C', 'D' };
static const int32 kASCDVersionForTest = 1;

// Duplica la logica di WriteASCD (static in XlsxTranslator.cpp, non
// esportata) solo per costruire qui un flusso ASCD di prova da dare
// in pasto al translator nella direzione di export (ASCD -> XLSX).
static status_t WriteASCDForTest(CContainer* doc, BPositionIO* dest)
{
	range bounds;
	doc->GetBounds(bounds);

	int32 count = 0;
	CCellIterator counter(doc, NULL);
	cell c;
	while (counter.NextExisting(c))
		count++;

	if (dest->Write(kASCDMagicForTest, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kASCDVersionForTest, sizeof(kASCDVersionForTest))
		!= (ssize_t)sizeof(kASCDVersionForTest))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;

	CCellIterator iter(doc, NULL);
	while (iter.NextExisting(c))
	{
		char text[512];
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

	return B_OK;
}

// Translate(XLSX -> nativo) produce ora sempre una cartella di lavoro
// multi-foglio ("ASCB", Fase 9), anche per un file XLSX con un solo
// foglio come tests/sample.xlsx: salta l'header e il nome del primo
// foglio per arrivare al blocco "ASCD" vero e proprio -- i controlli
// qui sotto verificano solo il contenuto delle celle del primo foglio,
// non l'incapsulamento multi-foglio in se' (gia' verificato a parte in
// ui/tests/test_ascd_book.cpp/test_multisheet.cpp). Riconosce anche un
// vecchio "ASCD" nudo, per restare valido se il formato dovesse mai
// tornare a produrne uno (difensivo, non il caso atteso oggi).
static bool UnwrapFirstSheet(const unsigned char* data, size_t len,
	const unsigned char** outAscd, size_t* outLen)
{
	if (len >= 4 && memcmp(data, "ASCB", 4) == 0)
	{
		if (len < 8)
			return false;
		int32 sheetCount;
		memcpy(&sheetCount, data + 4, 4);
		if (sheetCount < 1)
			return false;

		size_t pos = 8;
		if (pos + 4 > len)
			return false;
		int32 nameLen;
		memcpy(&nameLen, data + pos, 4);
		pos += 4;
		if (nameLen < 0 || pos + (size_t)nameLen > len)
			return false;
		pos += nameLen;

		*outAscd = data + pos;
		*outLen = len - pos;
		return true;
	}

	if (len >= 4 && memcmp(data, "ASCD", 4) == 0)
	{
		*outAscd = data;
		*outLen = len;
		return true;
	}

	return false;
}

int main()
{
	// Serve da Fase 12 (import grassetto/corsivo): gFontSizeTable::
	// GetFontID risolve un BFont reale (CFontStyle::Locate ->
	// BFont::GetEscapements), una chiamata che senza una BApplication
	// viva si blocca in attesa di una risposta dall'app_server che non
	// arriva mai -- stesso motivo gia' noto in ui/tests/test_ascd_io.cpp
	// e test_persistence.cpp per lo stesso genere di chiamate.
	BApplication app("application/x-vnd.Atomo-TestXlsxTranslator");

	BTranslator *translator = make_nth_translator(0, 0, 0);
	Check(translator != NULL, "make_nth_translator crea il translator");

	BFile xlsxFile("tests/sample.xlsx", B_READ_ONLY);
	Check(xlsxFile.InitCheck() == B_OK, "apertura di tests/sample.xlsx riuscita");

	translator_info info;
	status_t err = translator->Identify(&xlsxFile, NULL, NULL, &info, 0);
	Check(err == B_OK, "Identify riconosce il file XLSX reale");
	Check(info.type == kAtomoXlsxFormat, "Identify classifica il tipo come XLSX");

	xlsxFile.Seek(0, SEEK_SET);
	BMallocIO ascdOut;
	err = translator->Translate(&xlsxFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
	Check(err == B_OK, "Translate XLSX -> ASCD riesce");

	// Rilegge l'ASCD prodotto per verificare i valori importati: si
	// riusa lo stesso formato del translator CSV, quindi basta
	// controllare che il testo delle celle (formula o valore
	// formattato) contenga quanto atteso.
	const unsigned char *rawData = (const unsigned char *)ascdOut.Buffer();
	size_t rawLen = ascdOut.BufferLength();

	const unsigned char *ascdData = NULL;
	size_t ascdLen = 0;
	bool unwrapped = UnwrapFirstSheet(rawData, rawLen, &ascdData, &ascdLen);
	Check(unwrapped, "l'output di Translate e' una cartella ASCB valida o un ASCD nudo");

	Check(ascdLen > 12 && memcmp(ascdData, "ASCD", 4) == 0,
		"il primo foglio ha l'intestazione ASCD attesa");

	if (ascdLen > 12)
	{
		int32 count;
		memcpy(&count, ascdData + 8, 4);
		Check(count == 4, "l'ASCD contiene le 4 celle del foglio di esempio");

		bool foundA1 = false, foundFormulaResult = false, foundString = false;

		// Ricostruisce un documento a partire dai dati ASCD (stesso
		// procedimento di ReadASCD nel translator CSV) per verificare
		// non solo che il testo sia quello atteso, ma che il motore
		// calcoli davvero il risultato corretto dalla formula
		// importata.
		CContainer &doc = *new CContainer(NULL, NULL);

		size_t pos = 12;
		for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
		{
			int16 row, col;
			int32 len;
			memcpy(&row, ascdData + pos, 2); pos += 2;
			memcpy(&col, ascdData + pos, 2); pos += 2;
			memcpy(&len, ascdData + pos, 4); pos += 4;
			if (pos + len > ascdLen)
				break;

			std::string text((const char *)ascdData + pos, len);
			pos += len;

			printf("  cella riga=%d colonna=%d -> \"%s\"\n", row, col, text.c_str());

			cell loc(col, row);
			TryToParseString(text.c_str(), loc, &doc, true);

			if (row == 1 && col == 1 && text == "15")
				foundA1 = true;
			// ASCD preserva la formula (non il valore gia' calcolato
			// da Excel/LibreOffice): il testo della cella deve
			// contenere il riferimento alla formula originale.
			if (row == 1 && col == 3 && text.find("A1") != std::string::npos
				&& text.find("B1") != std::string::npos)
				foundFormulaResult = true;
			if (row == 1 && col == 4 && text.find("Ciao XLSX") != std::string::npos)
				foundString = true;
		}

		Check(foundA1, "A1 importato correttamente come 15");
		Check(foundFormulaResult,
			"C1 (formula =A1+B1) importata come formula, non appiattita al valore");
		Check(foundString, "D1 (stringa condivisa) importata come \"Ciao XLSX\"");

		cell c1(3, 1);
		doc.CalcCell(c1);
		Value v;
		doc.GetValue(c1, v);
		Check((double)v == 40.0,
			"il motore ricalcola la formula importata e ottiene 40");

		// Larghezze di colonna lette da <cols> in tests/sample.xlsx:
		// colonna 1 a larghezza 20 caratteri, colonne 3-4 a larghezza 8
		// -- convertite in pixel con la formula esatta di Excel
		// (ExcelColWidthToPixels, ECMA-376 18.3.1.13, MDW=7):
		// floor(((256*w + floor(128/7)) / 256) * 7). Per w=20:
		// floor(((5120+18)/256)*7) = floor(140.49) = 140. Per w=8:
		// floor(((2048+18)/256)*7) = floor(56.49) = 56.
		// "pos" punta gia' subito dopo l'ultima cella, cioe' all'inizio
		// della sezione grafici (sempre scritta, vuota qui) seguita
		// dalla sezione larghezze di colonna.
		if (pos + 4 <= ascdLen)
		{
			int32 chartCount;
			memcpy(&chartCount, ascdData + pos, 4); pos += 4;
			Check(chartCount == 0, "nessun grafico incorporato (il translator XLSX non li gestisce)");
			pos += chartCount * (2 * 4 + 4 * 4); // salta eventuali record grafico (non attesi qui)

			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				Check(colWidthCount == 3,
					"tre colonne con larghezza esplicita (1, 3, 4 -- da min=3 max=4)");

				bool foundCol1 = false, foundCol3 = false, foundCol4 = false;
				for (int32 i = 0; i < colWidthCount && pos + 6 <= ascdLen; i++)
				{
					int16 col;
					float width;
					memcpy(&col, ascdData + pos, 2); pos += 2;
					memcpy(&width, ascdData + pos, 4); pos += 4;

					if (col == 1 && fabs(width - 140.0f) < 0.01f)
						foundCol1 = true;
					if (col == 3 && fabs(width - 56.0f) < 0.01f)
						foundCol3 = true;
					if (col == 4 && fabs(width - 56.0f) < 0.01f)
						foundCol4 = true;
				}
				Check(foundCol1, "la larghezza della colonna 1 (20 caratteri -> 140 pixel) e' importata");
				Check(foundCol3, "la larghezza della colonna 3 (8 caratteri -> 56 pixel, da min=3) e' importata");
				Check(foundCol4, "la larghezza della colonna 4 (8 caratteri -> 56 pixel, da max=4) e' importata");

				// Colori di cella: A1 (s="1" in sample.xlsx) usa fontId=1
				// (testo blu, rgb="FF0000FF") e fillId=2 (sfondo dal tema
				// -- theme="4" con tint="0.4", cioe' accent1 "4472C4"
				// schiarito con la stessa formula approssimata di
				// ApplyTint in XlsxTranslator.cpp). B1 (s="2") usa lo
				// stesso sfondo ma fontId=0 (nessun <color>, testo
				// predefinito nero) -- verifica sia la risoluzione del
				// tema con tint sia quella diretta rgb=, e che uno stile
				// senza colore testo non forzi comunque un nero
				// esplicito diverso dal predefinito.
				if (pos + 4 <= ascdLen)
				{
					int32 cellColorCount;
					memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
					Check(cellColorCount == 2,
						"due celle (A1, B1) hanno un colore esplicito (s=\"1\"/\"2\")");

					bool foundA1Color = false, foundB1Color = false;
					for (int32 i = 0; i < cellColorCount && pos + 12 <= ascdLen; i++)
					{
						int16 row, col;
						uint8 bg[4], fg[4];
						memcpy(&row, ascdData + pos, 2); pos += 2;
						memcpy(&col, ascdData + pos, 2); pos += 2;
						memcpy(bg, ascdData + pos, 4); pos += 4;
						memcpy(fg, ascdData + pos, 4); pos += 4;

						// ApplyTint(0x4472C4, 0.4) canale per canale:
						// v*0.6 + 102, arrotondato -- vedi ApplyTintToChannel.
						bool bgMatches = abs((int)bg[0] - 143) <= 1
							&& abs((int)bg[1] - 170) <= 1 && abs((int)bg[2] - 220) <= 1;

						if (row == 1 && col == 1 && bgMatches
							&& fg[0] == 0 && fg[1] == 0 && fg[2] == 255)
							foundA1Color = true;
						if (row == 1 && col == 2 && bgMatches
							&& fg[0] == 0 && fg[1] == 0 && fg[2] == 0)
							foundB1Color = true;
					}
					Check(foundA1Color,
						"A1: sfondo dal tema (accent1 schiarito) e testo blu (rgb diretto) importati");
					Check(foundB1Color,
						"B1: stesso sfondo dal tema, testo predefinito nero (il font non ne specifica uno)");

					// Sezione colori di colonna (esistente prima di
					// Fase 10/11, sample.xlsx non ne ha): saltata senza
					// verificarne il conteggio, serve solo per arrivare
					// alla posizione giusta per le sezioni sotto.
					if (pos + 4 <= ascdLen)
					{
						int32 columnColorCount;
						memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
						pos += columnColorCount * (2 + 8); // col (int16) + WriteColorEntry (8 byte)

						// Regressione reale (crash "Assert failed:
						// inIndex <= fMax" in RunArray2.cpp, riprodotto
						// aprendo un file .xlsm da 38 fogli): quando
						// SaveASCD/LoadASCD (ui/src/AscdIO.cpp) hanno
						// guadagnato le cinque sezioni sotto (Fase
						// 10/11 -- altezze di riga, Blocca riquadri,
						// font, allineamento, bordi), la copia duplicata
						// di WriteASCD in questo file non era stata
						// aggiornata: per un foglio SINGOLO lo stream
						// finiva semplicemente prima (nessun crash,
						// LoadASCD tratta un EOF pulito come "sezione
						// assente" -- perche' questo test non l'aveva
						// gia' scoperto da solo), ma in una cartella di
						// lavoro multi-foglio (WriteASCDBook, un blocco
						// ASCD dopo l'altro sullo stesso flusso) i byte
						// del foglio SUCCESSIVO venivano letti come se
						// fossero queste sezioni del foglio corrente.
						// Le cinque verifiche sotto controllano che
						// questo translator le scriva sempre, anche
						// vuote, esattamente come SaveASCD.
						// Riga 1 di sample.xlsx ha ht="30" customHeight="1"
						// (aggiunta apposta per questo test): 30 punti a
						// 96 DPI -> 30 * 4/3 = 40 pixel, stesso fattore
						// gia' usato altrove in questo progetto per
						// SheetView::kRowHeight (15pt predefiniti -> 20px).
						if (pos + 4 <= ascdLen)
						{
							int32 rowHeightCount;
							memcpy(&rowHeightCount, ascdData + pos, 4); pos += 4;
							Check(rowHeightCount == 1,
								"una riga con altezza esplicita (riga 1, ht=\"30\" customHeight=\"1\")");

							bool foundRow1Height = false;
							for (int32 i = 0; i < rowHeightCount && pos + 6 <= ascdLen; i++)
							{
								int16 row;
								float height;
								memcpy(&row, ascdData + pos, 2); pos += 2;
								memcpy(&height, ascdData + pos, 4); pos += 4;
								if (row == 1 && fabs(height - 40.0f) < 0.01f)
									foundRow1Height = true;
							}
							Check(foundRow1Height,
								"l'altezza della riga 1 (30pt -> 40px) e' importata dal file XLSX originale");
						}

						if (pos + 8 <= ascdLen)
						{
							int32 frozenRows, frozenCols;
							memcpy(&frozenRows, ascdData + pos, 4); pos += 4;
							memcpy(&frozenCols, ascdData + pos, 4); pos += 4;
							Check(frozenRows == 0 && frozenCols == 0,
								"sezione Blocca riquadri presente (nessun blocco, Fase 10)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 fontCount;
							memcpy(&fontCount, ascdData + pos, 4); pos += 4;
							Check(fontCount == 0,
								"sezione font di cella presente (vuota, Fase 10)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 alignCount;
							memcpy(&alignCount, ascdData + pos, 4); pos += 4;
							Check(alignCount == 0,
								"sezione allineamento di cella presente (vuota, Fase 10)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 borderCount;
							memcpy(&borderCount, ascdData + pos, 4); pos += 4;
							Check(borderCount == 0,
								"sezione bordi di cella presente (vuota, Fase 11)");
						}

						// Sezione formato numero (Fase 12): sample.xlsx
						// non ha nessun numFmt esplicito in styles.xml
						// (tutti gli xf hanno numFmtId="0" = General),
						// quindi 0 celle -- ma la sezione stessa deve
						// esserci (a differenza delle cinque sopra,
						// questo translator la scrive con valori REALI
						// quando presenti, non sempre vuota: qui e'
						// vuota perche' il file di prova non ha formati,
						// non perche' la sezione manchi).
						if (pos + 4 <= ascdLen)
						{
							int32 formatCount;
							memcpy(&formatCount, ascdData + pos, 4); pos += 4;
							Check(formatCount == 0,
								"sezione formato numero presente (vuota per questo file di prova, Fase 12)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 underlineCount;
							memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
							Check(underlineCount == 0,
								"sezione sottolineato presente (vuota per questo file di prova, Fase 12)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 wrapCount;
							memcpy(&wrapCount, ascdData + pos, 4); pos += 4;
							Check(wrapCount == 0,
								"sezione testo a capo presente (vuota per questo file di prova, Fase 12)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 mergeCount;
							memcpy(&mergeCount, ascdData + pos, 4); pos += 4;
							Check(mergeCount == 0,
								"sezione celle unite presente (vuota per questo file di prova, Fase 12)");
						}

						if (pos + 4 <= ascdLen)
						{
							int32 imageCount;
							memcpy(&imageCount, ascdData + pos, 4); pos += 4;
							Check(imageCount == 0,
								"sezione immagini incorporate presente (vuota per questo file di prova, Fase 12)");
						}

						// Visibilita' griglia: sample.xlsx ha
						// <sheetView showGridLines="0"/> (aggiunto
						// apposta per questo test) -- un solo byte,
						// ultima sezione del formato.
						if (pos + 1 <= ascdLen)
						{
							uint8 showGrid = ascdData[pos]; pos += 1;
							Check(showGrid == 0,
								"la griglia nascosta (showGridLines=\"0\") e' importata dal file XLSX originale");
						}

						// sample.xlsx e' un solo foglio: dopo tutte le
						// sezioni lo stream deve finire ESATTAMENTE qui,
						// non prima (sezione mancante) ne' dopo (byte
						// avanzati, altro sintomo di disallineamento).
						Check(pos == ascdLen,
							"dopo tutte le sezioni lo stream ASCD del foglio finisce esattamente "
							"alla fine del buffer, nessun byte mancante o avanzato");
					}
				}
			}
		}

		doc.Release();
	}

	// Formati numero (Fase 12): tests/sample_numfmt.xlsx ha cinque
	// celle sulla riga 1 -- A1 con un formato incorporato (numFmtId 44,
	// mai definito esplicitamente in <numFmts>, dalla tabella
	// BuiltinNumFmtCode), B1 con un altro incorporato (9 = "0%"), C1 e
	// D1 con due formati personalizzati definiti in <numFmts> (165 =
	// "0.0", 166 = "0.00;[Red]0.00" -- quest'ultimo per verificare che
	// solo la parte prima del ";" venga usata, il colore condizionale
	// del negativo scartato come da limite dichiarato in ROADMAP.md),
	// E1 senza stile esplicito (numFmtId 0 = General, nessun formato
	// da applicare). Gli ID attesi sono calcolati a mano seguendo la
	// stessa logica di CFormatter::ParseTemplate/FormatID (vedi
	// ResolveNumberFormat in XlsxTranslator.cpp).
	{
		BFile numFmtFile("tests/sample_numfmt.xlsx", B_READ_ONLY);
		Check(numFmtFile.InitCheck() == B_OK, "apertura di tests/sample_numfmt.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&numFmtFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_numfmt.xlsx");

		numFmtFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&numFmtFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_numfmt.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_numfmt.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			CContainer &doc = *new CContainer(NULL, NULL);

			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 5, "l'ASCD contiene le 5 celle di sample_numfmt.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				std::string text((const char *)ascdData + pos, len);
				pos += len;
				cell loc(col, row);
				TryToParseString(text.c_str(), loc, &doc, true);
			}

			// Salta grafici (sempre 0)/larghezze colonna (nessuna qui)
			// per arrivare alla sezione colori, poi a quella formato.
			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8); // row+col+WriteColorEntry
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			// Le cinque sezioni vuote di Fase 10/11 (altezze riga,
			// Blocca riquadri, font, allineamento, bordi).
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) { pos += 8; } // Blocca riquadri (due int32 fissi)
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; /* font: lunghezza variabile, non atteso qui (0) */ }

			// A questo punto pos e' dopo fontCount (atteso 0 per questo
			// file, nessuna riga di skip necessaria oltre i 4 byte del
			// contatore gia' consumati sopra) -- allineamento e bordi
			// sotto, entrambe attese vuote.
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+1); } // allineamento
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // bordi

			bool haveFormatCount = false;
			int32 formatCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				haveFormatCount = true;
			}
			Check(haveFormatCount && formatCount == 4,
				"sezione formato numero: 4 celle con formato esplicito (A1/B1/C1/D1, non E1)");

			int foundA1 = -1, foundB1 = -1, foundC1 = -1, foundD1 = -1;
			for (int32 i = 0; i < formatCount && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 format;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&format, ascdData + pos, 4); pos += 4;

				if (row == 1 && col == 1) foundA1 = format;
				if (row == 1 && col == 2) foundB1 = format;
				if (row == 1 && col == 3) foundC1 = format;
				if (row == 1 && col == 4) foundD1 = format;
			}

			// eFixed(3) | cifre<<4 | commas<<9: "#,##0.00" (numFmtId 44
			// incorporato) -> fisso, 2 cifre, virgola = 3|32|512 = 547.
			Check(foundA1 == 547,
				"A1 (numFmtId 44 incorporato, \"#,##0.00\") risolto a fisso/2 cifre/virgola");
			// ePercent(4), "0%" -> nessuna cifra, nessuna virgola = 4.
			Check(foundB1 == 4,
				"B1 (numFmtId 9 incorporato, \"0%\") risolto a percentuale");
			// eFixed(3) | 1<<4, "0.0" (personalizzato) -> 3|16 = 19.
			Check(foundC1 == 19,
				"C1 (numFmtId 165 personalizzato, \"0.0\") risolto a fisso/1 cifra");
			// eFixed(3) | 2<<4, "0.00;[Red]0.00" -> solo la parte prima
			// del ';' conta (colore del negativo scartato) = 3|32 = 35.
			Check(foundD1 == 35,
				"D1 (numFmtId 166 personalizzato, \"0.00;[Red]0.00\") risolto a fisso/2 cifre, "
				"colore negativo scartato come da limite dichiarato");

			doc.Release();
		}
	}

	// Grassetto/corsivo (Fase 12): tests/sample_fontstyle.xlsx ha
	// cinque celle -- A1 grassetto, B1 corsivo, C1 grassetto+corsivo a
	// dimensione 16 (esplicita, diversa dall'11 delle altre), D1 con
	// <b val="0"/> (l'elemento c'e' ma val="0" lo nega esplicitamente:
	// NON deve risultare in grassetto), E1 senza stile esplicito. Solo
	// A1/B1/C1 devono finire nella sezione font della Fase 10 (ora
	// scritta con valori reali invece che sempre vuota) -- D1/E1 non
	// hanno nessuno stile da applicare, restano col font predefinito.
	// La famiglia usata e' sempre quella di be_plain_font (il nome
	// originale "Calibri" nel file non viene cercato/installato, vedi
	// il commento su ResolveStyle in XlsxTranslator.cpp), quindi il
	// test verifica solo stile e dimensione, non la famiglia esatta.
	{
		BFile fontFile("tests/sample_fontstyle.xlsx", B_READ_ONLY);
		Check(fontFile.InitCheck() == B_OK, "apertura di tests/sample_fontstyle.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&fontFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_fontstyle.xlsx");

		fontFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&fontFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_fontstyle.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_fontstyle.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 5, "l'ASCD contiene le 5 celle di sample_fontstyle.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri

			bool haveFontCount = false;
			int32 fontCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				haveFontCount = true;
			}
			Check(haveFontCount && fontCount == 3,
				"sezione font: 3 celle con stile esplicito (A1/B1/C1, non D1/E1)");

			bool foundA1Bold = false, foundA1Italic = false;
			bool foundB1Bold = false, foundB1Italic = false;
			bool foundC1Bold = false, foundC1Italic = false;
			float foundA1Size = 0, foundC1Size = 0;

			for (int32 i = 0; i < fontCount && pos + 4 + sizeof(font_family) + sizeof(font_style) + 4 <= ascdLen; i++)
			{
				int16 row, col;
				font_family family;
				font_style style;
				float size;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(family, ascdData + pos, sizeof(font_family)); pos += sizeof(font_family);
				memcpy(style, ascdData + pos, sizeof(font_style)); pos += sizeof(font_style);
				memcpy(&size, ascdData + pos, 4); pos += 4;

				BString styleStr(style);
				bool bold = styleStr.IFindFirst("Bold") >= 0;
				bool italic = styleStr.IFindFirst("Italic") >= 0;

				if (row == 1 && col == 1) { foundA1Bold = bold; foundA1Italic = italic; foundA1Size = size; }
				if (row == 1 && col == 2) { foundB1Bold = bold; foundB1Italic = italic; }
				if (row == 1 && col == 3) { foundC1Bold = bold; foundC1Italic = italic; foundC1Size = size; }
			}

			Check(foundA1Bold && !foundA1Italic, "A1 (<b/>) importato come grassetto, non corsivo");
			Check(foundA1Size == 11.0f, "A1 usa la dimensione esplicita del file (11), non quella predefinita");
			Check(!foundB1Bold && foundB1Italic, "B1 (<i/>) importato come corsivo, non grassetto");
			Check(foundC1Bold && foundC1Italic, "C1 (<b/><i/>) importato come grassetto E corsivo");
			Check(foundC1Size == 16.0f, "C1 usa la dimensione esplicita del file (16)");
		}
	}

	// Allineamento orizzontale (Fase 12): tests/sample_align.xlsx ha
	// cinque celle -- A1 centrato, B1 a destra, C1 a sinistra
	// (esplicito, per verificare che venga comunque letto e non
	// scambiato per "nessuno stile" come General anche se il risultato
	// visivo coincide col predefinito), D1 giustificato, E1 senza
	// stile esplicito (General, non nella sezione).
	{
		BFile alignFile("tests/sample_align.xlsx", B_READ_ONLY);
		Check(alignFile.InitCheck() == B_OK, "apertura di tests/sample_align.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&alignFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_align.xlsx");

		alignFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&alignFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_align.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_align.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 5, "l'ASCD contiene le 5 celle di sample_align.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount, nessuno stile grassetto/corsivo in questo file
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}

			bool haveAlignCount = false;
			int32 alignCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				haveAlignCount = true;
			}
			Check(haveAlignCount && alignCount == 4,
				"sezione allineamento: 4 celle con stile esplicito (A1/B1/C1/D1, non E1)");

			int foundA1 = -1, foundB1 = -1, foundC1 = -1, foundD1 = -1;
			for (int32 i = 0; i < alignCount && pos + 5 <= ascdLen; i++)
			{
				int16 row, col;
				int8 alignment;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&alignment, ascdData + pos, 1); pos += 1;

				if (row == 1 && col == 1) foundA1 = alignment;
				if (row == 1 && col == 2) foundB1 = alignment;
				if (row == 1 && col == 3) foundC1 = alignment;
				if (row == 1 && col == 4) foundD1 = alignment;
			}

			Check(foundA1 == eAlignCenter, "A1 (horizontal=\"center\") importato come centrato");
			Check(foundB1 == eAlignRight, "B1 (horizontal=\"right\") importato come allineato a destra");
			Check(foundC1 == eAlignLeft,
				"C1 (horizontal=\"left\" esplicito) importato come sinistra, non scambiato per General");
			Check(foundD1 == eAlignJustify, "D1 (horizontal=\"justify\") importato come giustificato");
		}
	}

	// Bordi da stile (Fase 11 -> import XLSX in Fase 12):
	// tests/sample_borders.xlsx ha tre celle -- A1 con tutti e quattro
	// i lati (borderId 1), B1 con solo sinistro e inferiore (borderId
	// 2), C1 senza stile esplicito (borderId implicito 0, nessun
	// lato). Verifica anche che borderId si risolva correttamente
	// contro <borders> (indice separato da fontId/fillId/numFmtId
	// nello stesso <xf>).
	{
		BFile bordersFile("tests/sample_borders.xlsx", B_READ_ONLY);
		Check(bordersFile.InitCheck() == B_OK, "apertura di tests/sample_borders.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&bordersFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_borders.xlsx");

		bordersFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&bordersFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_borders.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_borders.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 3, "l'ASCD contiene le 3 celle di sample_borders.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount, nessuno stile grassetto/corsivo qui
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			if (pos + 4 <= ascdLen) // alignCount, nessun allineamento esplicito qui
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				pos += alignCount * (2 + 2 + 1);
			}

			bool haveBorderCount = false;
			int32 borderCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				haveBorderCount = true;
			}
			Check(haveBorderCount && borderCount == 2,
				"sezione bordi: 2 celle con almeno un lato esplicito (A1/B1, non C1)");

			int foundA1T = -1, foundA1L = -1, foundA1B = -1, foundA1R = -1;
			int foundB1T = -1, foundB1L = -1, foundB1B = -1, foundB1R = -1;
			for (int32 i = 0; i < borderCount && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				uint8 sides[4];
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(sides, ascdData + pos, 4); pos += 4;

				if (row == 1 && col == 1)
				{
					foundA1T = sides[0]; foundA1L = sides[1]; foundA1B = sides[2]; foundA1R = sides[3];
				}
				if (row == 1 && col == 2)
				{
					foundB1T = sides[0]; foundB1L = sides[1]; foundB1B = sides[2]; foundB1R = sides[3];
				}
			}

			Check(foundA1T == 1 && foundA1L == 1 && foundA1B == 1 && foundA1R == 1,
				"A1 (borderId 1) importato con tutti e quattro i lati");
			Check(foundB1T == 0 && foundB1L == 1 && foundB1B == 1 && foundB1R == 0,
				"B1 (borderId 2) importato con solo sinistro e inferiore");
		}
	}

	// Sottolineato (Fase 12): tests/sample_underline.xlsx ha quattro
	// celle -- A1 con <u/> (semplice), B1 con <u val="double"/>
	// (trattato comunque come sottolineato semplice, nessuna
	// distinzione di stile), C1 con <u val="none"/> esplicito (NON
	// sottolineato nonostante l'elemento presente), D1 senza stile
	// esplicito.
	{
		BFile underlineFile("tests/sample_underline.xlsx", B_READ_ONLY);
		Check(underlineFile.InitCheck() == B_OK, "apertura di tests/sample_underline.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&underlineFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_underline.xlsx");

		underlineFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&underlineFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_underline.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_underline.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 4, "l'ASCD contiene le 4 celle di sample_underline.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount, nessuno stile grassetto/corsivo qui
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			if (pos + 4 <= ascdLen) // alignCount, nessun allineamento esplicito qui
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				pos += alignCount * (2 + 2 + 1);
			}
			if (pos + 4 <= ascdLen) // borderCount, nessun bordo esplicito qui
			{
				int32 borderCount;
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				pos += borderCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // formatCount, nessun numFmt esplicito qui
			{
				int32 formatCount;
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				pos += formatCount * (2 + 2 + 4);
			}

			bool haveUnderlineCount = false;
			int32 underlineCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
				haveUnderlineCount = true;
			}
			Check(haveUnderlineCount && underlineCount == 2,
				"sezione sottolineato: 2 celle sottolineate (A1/B1, non C1/D1)");

			bool foundA1 = false, foundB1 = false;
			for (int32 i = 0; i < underlineCount && pos + 4 <= ascdLen; i++)
			{
				int16 row, col;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;

				if (row == 1 && col == 1) foundA1 = true;
				if (row == 1 && col == 2) foundB1 = true;
			}

			Check(foundA1, "A1 (<u/>) importato come sottolineato");
			Check(foundB1, "B1 (<u val=\"double\"/>) importato come sottolineato semplice");
		}
	}

	// Testo a capo (Fase 12): tests/sample_wraptext.xlsx ha tre celle
	// -- A1 con wrapText="1", B1 con wrapText="1" insieme a
	// horizontal="center" (le due proprieta' dello stesso <alignment>
	// devono convivere), C1 senza stile esplicito.
	{
		BFile wrapFile("tests/sample_wraptext.xlsx", B_READ_ONLY);
		Check(wrapFile.InitCheck() == B_OK, "apertura di tests/sample_wraptext.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&wrapFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_wraptext.xlsx");

		wrapFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&wrapFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_wraptext.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_wraptext.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 3, "l'ASCD contiene le 3 celle di sample_wraptext.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount, nessuno stile grassetto/corsivo qui
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			int foundB1Align = -1;
			if (pos + 4 <= ascdLen) // alignCount: B1 e' anche centrato
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				for (int32 i = 0; i < alignCount && pos + 5 <= ascdLen; i++)
				{
					int16 row, col;
					int8 alignment;
					memcpy(&row, ascdData + pos, 2); pos += 2;
					memcpy(&col, ascdData + pos, 2); pos += 2;
					memcpy(&alignment, ascdData + pos, 1); pos += 1;
					if (row == 1 && col == 2) foundB1Align = alignment;
				}
			}
			if (pos + 4 <= ascdLen) // borderCount, nessun bordo esplicito qui
			{
				int32 borderCount;
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				pos += borderCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // formatCount, nessun numFmt esplicito qui
			{
				int32 formatCount;
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				pos += formatCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // underlineCount, nessun sottolineato qui
			{
				int32 underlineCount;
				memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
				pos += underlineCount * (2 + 2);
			}

			bool haveWrapCount = false;
			int32 wrapCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&wrapCount, ascdData + pos, 4); pos += 4;
				haveWrapCount = true;
			}
			Check(haveWrapCount && wrapCount == 2,
				"sezione testo a capo: 2 celle con a capo attivo (A1/B1, non C1)");

			bool foundA1 = false, foundB1 = false;
			for (int32 i = 0; i < wrapCount && pos + 4 <= ascdLen; i++)
			{
				int16 row, col;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;

				if (row == 1 && col == 1) foundA1 = true;
				if (row == 1 && col == 2) foundB1 = true;
			}

			Check(foundA1, "A1 (wrapText=\"1\") importato con a capo attivo");
			Check(foundB1, "B1 (wrapText=\"1\" + horizontal=\"center\") importato con a capo attivo");
			Check(foundB1Align == eAlignCenter,
				"B1 mantiene anche l'allineamento centrato, le due proprieta' convivono");
		}
	}

	// Celle unite (Fase 12): tests/sample_merge.xlsx ha due intervalli
	// -- A1:C1 (intestazione orizzontale) e A2:A3 (verticale) -- piu'
	// D1 normale, mai coinvolta.
	{
		BFile mergeFile("tests/sample_merge.xlsx", B_READ_ONLY);
		Check(mergeFile.InitCheck() == B_OK, "apertura di tests/sample_merge.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&mergeFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_merge.xlsx");

		mergeFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&mergeFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_merge.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_merge.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 3, "l'ASCD contiene le 3 celle con contenuto di sample_merge.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			if (pos + 4 <= ascdLen) // alignCount
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				pos += alignCount * (2 + 2 + 1);
			}
			if (pos + 4 <= ascdLen) // borderCount
			{
				int32 borderCount;
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				pos += borderCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // formatCount
			{
				int32 formatCount;
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				pos += formatCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // underlineCount
			{
				int32 underlineCount;
				memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
				pos += underlineCount * (2 + 2);
			}
			if (pos + 4 <= ascdLen) // wrapCount
			{
				int32 wrapCount;
				memcpy(&wrapCount, ascdData + pos, 4); pos += 4;
				pos += wrapCount * (2 + 2);
			}

			bool haveMergeCount = false;
			int32 mergeCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&mergeCount, ascdData + pos, 4); pos += 4;
				haveMergeCount = true;
			}
			Check(haveMergeCount && mergeCount == 2,
				"sezione celle unite: 2 intervalli (A1:C1 e A2:A3)");

			bool foundHorizontal = false, foundVertical = false;
			for (int32 i = 0; i < mergeCount && pos + 8 <= ascdLen; i++)
			{
				int16 top, left, bottom, right;
				memcpy(&top, ascdData + pos, 2); pos += 2;
				memcpy(&left, ascdData + pos, 2); pos += 2;
				memcpy(&bottom, ascdData + pos, 2); pos += 2;
				memcpy(&right, ascdData + pos, 2); pos += 2;

				if (top == 1 && left == 1 && bottom == 1 && right == 3)
					foundHorizontal = true;
				if (top == 2 && left == 1 && bottom == 3 && right == 1)
					foundVertical = true;
			}

			Check(foundHorizontal, "A1:C1 importato come intervallo unito orizzontale");
			Check(foundVertical, "A2:A3 importato come intervallo unito verticale");
		}
	}

	// Immagini incorporate (Fase 12): tests/sample_image.xlsx ha A1
	// ("Testo", una cella normale che deve convivere con l'immagine)
	// piu' un logo PNG 4x3 ancorato a B2 (xdr:from col=1/row=1,
	// 0-based) con uno scarto di 95250x47625 EMU (10x5 pixel) e una
	// dimensione esplicita di 381000x285750 EMU (40x30 pixel) -- il
	// caso "gia' ridimensionata a mano", diverso dal file di gara
	// reale (cx/cy=0, dove si usa la dimensione naturale del PNG).
	{
		BFile imageFile("tests/sample_image.xlsx", B_READ_ONLY);
		Check(imageFile.InitCheck() == B_OK, "apertura di tests/sample_image.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&imageFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_image.xlsx");

		imageFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&imageFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_image.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_image.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 1, "l'ASCD contiene la sola cella con contenuto di sample_image.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				pos += cellColorCount * (2 + 2 + 8);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 columnColorCount;
				memcpy(&columnColorCount, ascdData + pos, 4); pos += 4;
				pos += columnColorCount * (2 + 8);
			}
			if (pos + 4 <= ascdLen) { int32 n; memcpy(&n, ascdData+pos, 4); pos += 4; pos += n * (2+2+4); } // altezze riga
			if (pos + 8 <= ascdLen) pos += 8; // Blocca riquadri
			if (pos + 4 <= ascdLen) // fontCount
			{
				int32 fontCount;
				memcpy(&fontCount, ascdData + pos, 4); pos += 4;
				pos += fontCount * (2 + 2 + (int32)sizeof(font_family) + (int32)sizeof(font_style) + 4);
			}
			if (pos + 4 <= ascdLen) // alignCount
			{
				int32 alignCount;
				memcpy(&alignCount, ascdData + pos, 4); pos += 4;
				pos += alignCount * (2 + 2 + 1);
			}
			if (pos + 4 <= ascdLen) // borderCount
			{
				int32 borderCount;
				memcpy(&borderCount, ascdData + pos, 4); pos += 4;
				pos += borderCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // formatCount
			{
				int32 formatCount;
				memcpy(&formatCount, ascdData + pos, 4); pos += 4;
				pos += formatCount * (2 + 2 + 4);
			}
			if (pos + 4 <= ascdLen) // underlineCount
			{
				int32 underlineCount;
				memcpy(&underlineCount, ascdData + pos, 4); pos += 4;
				pos += underlineCount * (2 + 2);
			}
			if (pos + 4 <= ascdLen) // wrapCount
			{
				int32 wrapCount;
				memcpy(&wrapCount, ascdData + pos, 4); pos += 4;
				pos += wrapCount * (2 + 2);
			}
			if (pos + 4 <= ascdLen) // mergeCount
			{
				int32 mergeCount;
				memcpy(&mergeCount, ascdData + pos, 4); pos += 4;
				pos += mergeCount * (2 * 4);
			}

			bool haveImageCount = false;
			int32 imageCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&imageCount, ascdData + pos, 4); pos += 4;
				haveImageCount = true;
			}
			Check(haveImageCount && imageCount == 1,
				"sezione immagini incorporate: 1 immagine (il logo di sample_image.xlsx)");

			if (haveImageCount && imageCount == 1 && pos + 2 + 2 + 16 + 4 <= ascdLen)
			{
				int16 row, col;
				float geom[4];
				int32 pngLen;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(geom, ascdData + pos, 16); pos += 16;
				memcpy(&pngLen, ascdData + pos, 4); pos += 4;

				Check(row == 2 && col == 2, "l'immagine e' ancorata a B2 (xdr:from col=1/row=1, 0-based)");
				Check(geom[0] == 10.0f && geom[1] == 5.0f,
					"lo scarto dall'angolo di B2 e' 10x5 pixel (95250x47625 EMU / 9525)");
				Check(geom[2] == 40.0f && geom[3] == 30.0f,
					"la dimensione esplicita e' 40x30 pixel (381000x285750 EMU / 9525), non quella naturale del PNG");
				Check(pngLen == 75, "il blob PNG incorporato ha la lunghezza originale (75 byte)");

				if (pos + (size_t)pngLen <= ascdLen && pngLen >= 8)
				{
					static const unsigned char kPngSig[8] =
						{ 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
					Check(memcmp(ascdData + pos, kPngSig, 8) == 0,
						"il blob PNG incorporato inizia con la firma PNG vera");
					pos += pngLen;
				}
			}
		}
	}

	// Tabelle strutturate (Fase 12): tests/sample_table.xlsx ha una
	// tabella A1:B4 (TableStyleMedium2, showRowStripes="1") -- A1 e'
	// l'intestazione (mai bandata), A2/B2 e A4/B4 sono la prima e
	// terza riga dati (bandate), A3/B3 la seconda (non bandata,
	// alternanza corretta).
	{
		BFile tableFile("tests/sample_table.xlsx", B_READ_ONLY);
		Check(tableFile.InitCheck() == B_OK, "apertura di tests/sample_table.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&tableFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_table.xlsx");

		tableFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&tableFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_table.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_table.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 8, "l'ASCD contiene le 8 celle di sample_table.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}

			bool haveCellColorCount = false;
			int32 cellColorCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				haveCellColorCount = true;
			}
			Check(haveCellColorCount && cellColorCount == 4,
				"sezione colori di cella: 4 celle bandate (A2/B2/A4/B4)");

			bool foundA2 = false, foundB2 = false, foundA4 = false, foundB4 = false;
			bool colorsCorrect = true;
			for (int32 i = 0; i < cellColorCount && pos + 4 + 8 <= ascdLen; i++)
			{
				int16 row, col;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				// WriteColorEntry: due rgb_color da 4 byte (bg poi fg).
				rgb_color bg;
				memcpy(&bg, ascdData + pos, 4); pos += 4;
				pos += 4; // fg, non verificato qui

				bool isBand = bg.red == 242 && bg.green == 242 && bg.blue == 242;
				if (row == 2 && col == 1) { foundA2 = true; colorsCorrect &= isBand; }
				if (row == 2 && col == 2) { foundB2 = true; colorsCorrect &= isBand; }
				if (row == 4 && col == 1) { foundA4 = true; colorsCorrect &= isBand; }
				if (row == 4 && col == 2) { foundB4 = true; colorsCorrect &= isBand; }
				if (row == 1 || row == 3)
					Check(false, "nessuna cella dell'intestazione o della riga dati pari ha un colore");
			}

			Check(foundA2 && foundB2 && foundA4 && foundB4 && colorsCorrect,
				"A2/B2 (prima riga dati) e A4/B4 (terza) hanno il colore di banda grigio chiaro");
		}
	}

	// Formattazione condizionale (Fase 12): tests/sample_condformat.xlsx
	// ha due regole -- cellIs/equal "Mancante" su A1:A3 (solo A1
	// corrisponde, dxf 0 = rgb FFC7CE) e duplicateValues su B1:B3 (B1
	// e B3 sono entrambe "X", duplicate fra loro; B2 = "Y", unica,
	// dxf 1 = rgb FFEB9C).
	{
		BFile condFile("tests/sample_condformat.xlsx", B_READ_ONLY);
		Check(condFile.InitCheck() == B_OK, "apertura di tests/sample_condformat.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&condFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_condformat.xlsx");

		condFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&condFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_condformat.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_condformat.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 6, "l'ASCD contiene le 6 celle di sample_condformat.xlsx");

			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				pos += len;
			}

			if (pos + 4 <= ascdLen)
			{
				int32 chartCount;
				memcpy(&chartCount, ascdData + pos, 4); pos += 4;
				pos += chartCount * (2 * 4 + 4 * 4);
			}
			if (pos + 4 <= ascdLen)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, ascdData + pos, 4); pos += 4;
				pos += colWidthCount * (2 + 4);
			}

			bool haveCellColorCount = false;
			int32 cellColorCount = 0;
			if (pos + 4 <= ascdLen)
			{
				memcpy(&cellColorCount, ascdData + pos, 4); pos += 4;
				haveCellColorCount = true;
			}
			Check(haveCellColorCount && cellColorCount == 3,
				"sezione colori di cella: 3 celle colorate dalla formattazione condizionale (A1/B1/B3)");

			int foundA1Bg = -1, foundB1Bg = -1, foundB3Bg = -1;
			bool foundA2 = false, foundA3 = false, foundB2 = false;
			for (int32 i = 0; i < cellColorCount && pos + 4 + 8 <= ascdLen; i++)
			{
				int16 row, col;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				rgb_color bg;
				memcpy(&bg, ascdData + pos, 4); pos += 4;
				pos += 4; // fg, non verificato qui

				int packed = (bg.red << 16) | (bg.green << 8) | bg.blue;
				if (row == 1 && col == 1) foundA1Bg = packed;
				if (row == 1 && col == 2) foundB1Bg = packed;
				if (row == 3 && col == 2) foundB3Bg = packed;
				if (row == 2 && col == 1) foundA2 = true;
				if (row == 3 && col == 1) foundA3 = true;
				if (row == 2 && col == 2) foundB2 = true;
			}

			Check(foundA1Bg == 0xFFC7CE, "A1 (\"Mancante\", cellIs/equal) ha lo sfondo del dxf 0");
			Check(foundB1Bg == 0xFFEB9C, "B1 (\"X\", duplicata con B3) ha lo sfondo del dxf 1");
			Check(foundB3Bg == 0xFFEB9C, "B3 (\"X\", duplicata con B1) ha lo sfondo del dxf 1");
			Check(!foundA2, "A2 (\"OK\", nessuna corrispondenza) resta senza colore");
			Check(!foundA3, "A3 (\"OK2\", nessuna corrispondenza) resta senza colore");
			Check(!foundB2, "B2 (\"Y\", valore unico) resta senza colore");
		}
	}

	// Formati data/ora (Fase 12): tests/sample_dates.xlsx ha tre celle
	// -- A1 (serial 45892, numFmtId 14 incorporato) e B1 (serial
	// 44197, numFmtId 165 personalizzato "dd/mm/yyyy") sono numeri
	// puri con uno stile data, D1 (100, nessuno stile data) resta un
	// numero normale. Sistema data predefinito (1899-12-30, nessun
	// <workbookPr date1904="1"/> nel file). Il testo scritto in ASCD
	// per una cella senza formula passa comunque da GetCellFormula ->
	// CFormatter::FormatValue (vedi Container.cpp), che per un Value
	// eTimeData chiama sempre FormatDate indipendentemente da
	// CellStyle::fFormat -- il valore vero e' quindi gia' verificabile
	// dal testo stesso, senza dover leggere Value/CellStyle a parte.
	{
		BFile dateFile("tests/sample_dates.xlsx", B_READ_ONLY);
		Check(dateFile.InitCheck() == B_OK, "apertura di tests/sample_dates.xlsx riuscita");

		translator_info info;
		status_t err = translator->Identify(&dateFile, NULL, NULL, &info, 0);
		Check(err == B_OK, "Identify riconosce sample_dates.xlsx");

		dateFile.Seek(0, SEEK_SET);
		BMallocIO ascdOut;
		err = translator->Translate(&dateFile, &info, NULL, kAtomoNativeFormat, &ascdOut);
		Check(err == B_OK, "Translate di sample_dates.xlsx riesce");

		const unsigned char *ascdData = NULL;
		size_t ascdLen = 0;
		bool unwrapped = UnwrapFirstSheet((const unsigned char *)ascdOut.Buffer(),
			ascdOut.BufferLength(), &ascdData, &ascdLen);
		Check(unwrapped, "l'output di Translate di sample_dates.xlsx e' un ASCD valido");

		if (unwrapped)
		{
			int32 count = 0;
			if (ascdLen > 12)
				memcpy(&count, ascdData + 8, 4);
			Check(count == 3, "l'ASCD contiene le 3 celle di sample_dates.xlsx");

			bool foundA1 = false, foundB1 = false, foundD1 = false;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= ascdLen; i++)
			{
				int16 row, col;
				int32 len;
				memcpy(&row, ascdData + pos, 2); pos += 2;
				memcpy(&col, ascdData + pos, 2); pos += 2;
				memcpy(&len, ascdData + pos, 4); pos += 4;
				if (pos + (size_t)len > ascdLen)
					break;
				std::string text((const char *)ascdData + pos, len);
				pos += len;

				if (row == 1 && col == 1 && text == "23/08/2025") foundA1 = true;
				if (row == 1 && col == 2 && text == "01/01/2021") foundB1 = true;
				if (row == 1 && col == 4 && text == "100") foundD1 = true;
			}

			Check(foundA1,
				"A1 (serial 45892, numFmtId 14 incorporato) importato come data 23/08/2025");
			Check(foundB1,
				"B1 (serial 44197, numFmtId 165 personalizzato) importato come data 01/01/2021");
			Check(foundD1, "D1 (100, nessuno stile data) resta un numero normale, non una data");
		}
	}

	// Esportazione (ASCD -> XLSX): scrive un documento con un numero,
	// una stringa e una formula, poi rilegge il file XLSX prodotto con
	// lo stesso translator (round-trip completo) per verificare che i
	// valori sopravvivano e che la formula sia diventata il suo valore
	// calcolato (scelta deliberata dell'export, come CSV/ODS).
	{
		CContainer &exportDoc = *new CContainer(NULL, NULL);
		TryToParseString("12", cell(1, 1), &exportDoc, true);       // A1 = 12
		TryToParseString("8", cell(2, 1), &exportDoc, true);        // B1 = 8
		TryToParseString("=A1+B1", cell(3, 1), &exportDoc, true);   // C1 = 20
		TryToParseString("Prova export", cell(1, 2), &exportDoc, true); // A2
		exportDoc.CalcCell(cell(3, 1));

		BMallocIO ascdIn;
		status_t saveErr = WriteASCDForTest(&exportDoc, &ascdIn);
		Check(saveErr == B_OK, "preparazione dell'ASCD di prova per l'export riesce");
		exportDoc.Release();

		ascdIn.Seek(0, SEEK_SET);
		translator_info exportInfo;
		err = translator->Identify(&ascdIn, NULL, NULL, &exportInfo, kAtomoXlsxFormat);
		Check(err == B_OK, "Identify riconosce l'ASCD come sorgente per l'export");
		Check(exportInfo.type == kAtomoNativeFormat,
			"Identify classifica il sorgente come ASCD nativo");

		ascdIn.Seek(0, SEEK_SET);
		BMallocIO xlsxOut;
		err = translator->Translate(&ascdIn, &exportInfo, NULL, kAtomoXlsxFormat, &xlsxOut);
		Check(err == B_OK, "Translate ASCD -> XLSX riesce");

		xlsxOut.Seek(0, SEEK_SET);
		translator_info reimportInfo;
		err = translator->Identify(&xlsxOut, NULL, NULL, &reimportInfo, 0);
		Check(err == B_OK && reimportInfo.type == kAtomoXlsxFormat,
			"il file XLSX appena scritto viene riconosciuto come XLSX valido rileggendolo");

		xlsxOut.Seek(0, SEEK_SET);
		BMallocIO ascdOut2;
		err = translator->Translate(&xlsxOut, &reimportInfo, NULL, kAtomoNativeFormat, &ascdOut2);
		Check(err == B_OK, "il file XLSX appena scritto si rilegge correttamente (round-trip)");

		if (err == B_OK)
		{
			const unsigned char *rawData2 = (const unsigned char *)ascdOut2.Buffer();
			size_t rawLen2 = ascdOut2.BufferLength();
			const unsigned char *data = NULL;
			size_t len = 0;
			Check(UnwrapFirstSheet(rawData2, rawLen2, &data, &len),
				"il round-trip produce anch'esso una cartella ASCB valida o un ASCD nudo");
			size_t pos = 12;
			int32 cnt;
			memcpy(&cnt, data + 8, 4);

			bool reA1 = false, reB1 = false, reC1 = false, reA2 = false;
			for (int32 i = 0; i < cnt && pos + 8 <= len; i++)
			{
				int16 row, col;
				int32 tlen;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&tlen, data + pos, 4); pos += 4;
				if (pos + tlen > len)
					break;
				std::string text((const char *)data + pos, tlen);
				pos += tlen;

				if (row == 1 && col == 1 && text == "12") reA1 = true;
				if (row == 1 && col == 2 && text == "8") reB1 = true;
				if (row == 1 && col == 3 && text == "20") reC1 = true;
				if (row == 2 && col == 1 && text == "Prova export") reA2 = true;
			}

			Check(reA1, "dopo il round-trip, A1 vale ancora 12");
			Check(reB1, "dopo il round-trip, B1 vale ancora 8");
			Check(reC1,
				"dopo il round-trip, C1 (era una formula) vale 20 -- il valore calcolato, "
				"non la formula (scelta deliberata dell'export)");
			Check(reA2, "dopo il round-trip, A2 vale ancora \"Prova export\"");
		}
	}

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
