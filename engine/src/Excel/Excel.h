/*
	Copyright 1996, 1997, 1998, 2000
	        Hekkelman Programmatuur B.V.  All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.
	3. All advertising materials mentioning features or use of this software
	   must display the following acknowledgement:
	   
	    This product includes software developed by Hekkelman Programmatuur B.V.
	
	4. The name of Hekkelman Programmatuur B.V. may not be used to endorse or
	   promote products derived from this software without specific prior
	   written permission.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
	AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
	OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/
/*
	Excel filter
*/

#include "MyError.h"
#include "FileFormat.h"
#include "Formula.h"
#include "Functions.h"
#include "EmbeddedImage.h"
//#include <ByteOrder.h>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class CCellView;
class CContainer;
class CFormula;

struct XLSHFormula
{
	short rwFirst, rwLast;
	char colFirst, colLast;
	short reserved;
	short cce;
	char *p;
};

enum
{
	xnFunction,
	xnNamedReference,
	xnOther
};

struct xlName
{
	int type;
	union
	{
		int funcNr;
		char *namedRef;
	};
	
	xlName() : type(xnOther) {}
	xlName(int funcNr) : type(xnFunction), funcNr(funcNr) {}
	xlName(const char *name) : type(xnNamedReference), namedRef(strdup(name)) {}
};

class CExcel5Filter
{
  public:
	CExcel5Filter(BPositionIO& inStream, CCellView *cellView, CContainer *container);
	virtual ~CExcel5Filter();

	void Translate();

	// Larghezze di colonna esplicite (colonna 1-based, larghezza in
	// pixel) lette da record COLINFO durante Translate() -- popolata
	// SEMPRE (non solo quando "cellView" e' vivo, vedi il commento su
	// COLINFO in Excel.pass1.cpp), cosi' un translator headless (senza
	// nessun CCellView) puo' comunque recuperarle dopo Translate() per
	// scriverle nel formato nativo (vedi WriteASCD in
	// translators/xls/XlsTranslator.cpp).
	const std::vector<std::pair<int, float> >& GetColumnWidths() const
		{ return fColWidths; }

	// Immagini incorporate (record MSODRAWINGGROUP/MSODRAWING, formato
	// binario Escher -- vedi Excel.escher.cpp) lette durante
	// Translate(), stesso principio di GetColumnWidths() sopra: un
	// translator headless le recupera dopo Translate() per scriverle
	// nel formato nativo.
	const std::vector<EmbeddedImage>& GetImages() const
		{ return fImages; }

  private:
	
	/* Niente "throw()": il corpo chiama CExcelStream::Read, che
	   lancia CErr su dati insufficienti/malformati. Una funzione
	   marcata "throw()" (equivalente a noexcept in C++17) che lancia
	   comunque causa std::terminate() immediato, bypassando ogni
	   catch a monte -- osservato concretamente importando un file
	   OLE2 con contenuto BIFF non valido. */
	status_t GetBookStream(BPositionIO& stream);
	bool LocateBof(BPositionIO& stream);
	
	class CExcelStream
	{
	  public:
		CExcelStream(BPositionIO& s) : fStream(s) {}
		
		void Read (void *buffer, int size)
		{
			if (fStream.Read(buffer, size) != size)
				THROW((errIORead));
		}
		
		CExcelStream& operator>> (char& x)
		{
			Read(&x, sizeof(x));
			return *this;
		}
		
		CExcelStream& operator>> (unsigned char& x)
		{
			Read(&x, sizeof(x));
			return *this;
		}
		
		CExcelStream& operator>> (short& x)
		{
			Read(&x, sizeof(x));
			x = B_LENDIAN_TO_HOST_INT16(x);
			return *this;
		}
		
		CExcelStream& operator>> (unsigned short& x)
		{
			Read(&x, sizeof(x));
			x = B_LENDIAN_TO_HOST_INT16(x);
			return *this;
		}
		
		// Un campo BIFF "long" e' sempre 4 byte fissi (DWORD) -- MA
		// "long"/"unsigned long" sono 8 byte su Haiku x86_64 (LP64),
		// non 4 come sull'hardware BeOS/PPC per cui questo filtro era
		// stato scritto originariamente. "Read(&x, sizeof(x))" leggeva
		// quindi 8 byte da un campo che ne ha solo 4: innocuo se e'
		// l'ultimo campo letto di un record isolato (i 4 byte in piu'
		// finiscono nel record successivo senza essere mai
		// riconsultati, dato che il ciclo esterno di Pass1/Pass2
		// riposiziona comunque il flusso in base alla lunghezza
		// dichiarata del record), ma un bug reale quando PIU' campi di
		// questo tipo si leggono in sequenza dallo stesso record (es.
		// MULRK, piu' celle in un solo record): ogni lettura di troppo
		// faceva slittare il cursore di 4 byte, corrompendo a cascata
		// ogni cella successiva nello stesso record -- bug reale
		// scoperto verificando il rilevamento dei formati data su un
		// file .xls reale (MULRK con piu' celle sulla stessa riga: la
		// prima cella corretta, le successive con valori senza senso).
		// Stessa famiglia del bug gia' corretto in Excel.OLE2.cpp
		// ("long a 64 bit invece di 32"), qui riemerso negli operatori
		// di flusso. Fix: leggere sempre esattamente 4 byte in una
		// variabile a 32 bit fissa, poi assegnare/estendere al long
		// (con segno) o unsigned long di uscita.
		CExcelStream& operator>> (long& x)
		{
			int32 v;
			Read(&v, sizeof(v));
			x = B_LENDIAN_TO_HOST_INT32(v);
			return *this;
		}

		CExcelStream& operator>> (unsigned long& x)
		{
			uint32 v;
			Read(&v, sizeof(v));
			x = B_LENDIAN_TO_HOST_INT32(v);
			return *this;
		}
		
		CExcelStream& operator>> (float& x)
		{
			Read(&x, sizeof(x));
			x = B_LENDIAN_TO_HOST_FLOAT(x);
			return *this;
		}
		
		CExcelStream& operator>> (double& x)
		{
			Read(&x, sizeof(x));
			x = B_LENDIAN_TO_HOST_DOUBLE(x);
			return *this;
		}
		
		CExcelStream& operator >> (unsigned char *p)
		{
			Read(p, sizeof(*p));
			Read(p + 1, *p);
			return *this;
		}
		
	  private:
		BPositionIO& fStream;
	};

	void Pass1();
	void Pass2();
	void HandleXLRecordForPass1(int code, int len);
	void Selection();
	void Name();
	void Font();
	void Xf();
	// Tabella delle stringhe condivise (BIFF8/Excel97+, record SST):
	// vedi il commento su SST/LABELSST in XL_Biff_codes.h. "len" e' la
	// lunghezza dichiarata del record SST stesso, necessaria per sapere
	// quando la lettura esce dai suoi confini e deve proseguire in un
	// eventuale record CONTINUE che segue (le tabelle di stringhe dei
	// file reali superano quasi sempre la dimensione massima di un
	// singolo record BIFF).
	void ReadSST(int len);

	// Le stringhe BIFF "compresse" (un byte/carattere: LABEL/RSTRING
	// dirette, e il ramo non "wide" di ReadSST) sono in Windows-1252,
	// non Latin-1/ASCII puro come assumeva il codice storico -- un
	// byte >=0x80 copiato cosi' com'e' in una stringa UTF-8 (quella
	// attesa da tutta la UI, nativa su Haiku) e' una sequenza non
	// valida, mostrata come carattere di sostituzione -- bug reale
	// scoperto confrontando visivamente con Excel vero l'importazione
	// di una fattura reale ("Modalit\xe0" invece di "Modalità").
	// AppendUnicodeAsUTF8 e' riusata anche per il ramo "wide" (UTF-16,
	// 2 byte/carattere) di ReadSST, che prima riduceva a '?' qualunque
	// punto di codice oltre il singolo byte.
	static void AppendCP1252Byte(std::string& out, unsigned char b);
	static void AppendUnicodeAsUTF8(std::string& out, unsigned int codepoint);

	// Formati numero (record FORMAT/XF, BIFF5-8): "fFormats" era prima
	// un vector indicizzato per ORDINE DI APPARIZIONE dei record
	// FORMAT nel file, mentre l'indice "ifmt" che XF referenzia davvero
	// e' un identificativo assoluto -- built-in (0-163, es. 14 = data
	// "mm-dd-yy") o personalizzato (164 in su), quasi mai coincidente
	// con l'ordine di apparizione. Un file reale che usa un formato
	// built-in (frequente: data, percentuale) senza mai dichiararlo
	// con un proprio record FORMAT (il caso comune, dato che e'
	// built-in) faceva leggere una voce sbagliata o fuori indice --
	// bug reale scoperto confrontando visivamente con Excel vero
	// l'importazione di una fattura reale (una data mostrata come
	// "43242", il numero seriale grezzo). Sostituito con una mappa
	// ifmt->formatID, precaricata con i formati built-in piu' comuni
	// da InitBuiltinFormats() prima di leggere il file: un record
	// FORMAT esplicito nel file sovrascrive semplicemente la voce
	// gia' precaricata per lo stesso ifmt, se il file la personalizza.
	void InitBuiltinFormats();
	// ifmt per cui il formato (built-in o personalizzato via record
	// FORMAT) e' un formato data/ora -- serve per Pass2, che deve
	// costruire un Value(time_t) invece del numero seriale grezzo,
	// dato che CFormatter::FormatValue formatta come data solo un
	// Value gia' di tipo eTimeData, mai un eNumData con un fFormat che
	// "sembra" una data (vedi il commento in Formatter.cpp).
	std::set<unsigned short> fDateIfmts;
	// Vero se il template di un formato personalizzato "sembra" una
	// data (euristica sulle lettere y/m/d/h/s fuori da apici e
	// parentesi quadre, stessa idea di LooksLikeDateFormat in
	// XlsxTranslator.cpp, duplicata qui per lo stesso motivo per cui
	// ogni translator/filtro duplica la propria serializzazione: ogni
	// pezzo di questo progetto resta autonomo senza dipendenze
	// incrociate fra motore/translator).
	static bool LooksLikeDateFormat(const std::string& tmpl);
	// Offset epoca Excel -> Unix, stesso principio di
	// ExcelSerialToTime in XlsxTranslator.cpp (Fase 12): 25569 giorni
	// (sistema 1900, predefinito) o 24107 (sistema 1904, "f1904" gia'
	// letto dal record 1904 del file).
	static time_t ExcelSerialToTime(double serial, bool date1904);

	// Immagini incorporate (Escher/MSODRAWING, vedi Excel.escher.cpp):
	// MSODRAWINGGROUP e' un record unico per l'intera cartella di
	// lavoro (contiene il "blip store" con i byte grezzi di ogni
	// immagine, indicizzato per posizione), MSODRAWING e' per-cella/
	// per-forma (contiene l'ancoraggio riga/colonna piu' l'indice nel
	// blip store) -- stesso rapporto uno-a-molti di FORMAT/XF sopra.
	struct EscherBlip { int type; std::vector<unsigned char> data; };
	std::vector<EscherBlip> fBlips;
	void HandleMsoDrawingGroup(int len);
	void HandleMsoDrawing(int len);
	// Larghezza in pixel di una colonna (1-based): quella esplicita
	// di fColWidths se il file la dichiara (record COLINFO), altrimenti
	// il valore predefinito -- usata per l'ancoraggio/dimensione delle
	// immagini incorporate (vedi SpanPixels in Excel.escher.cpp).
	float ColWidthPixels(int col) const;
	// Legge "len" byte del record corrente, poi attraversa in modo
	// trasparente uno o piu' record CONTINUE che seguono subito dopo
	// (stesso principio di ReadSST sopra, fattorizzato qui perche'
	// serve identico sia per MSODRAWINGGROUP che per MSODRAWING).
	std::vector<unsigned char> ReadRecordWithContinues(int len);

	void HandleXLRecordForPass2(int code, int len);
	void ParseXLFormula(CFormula& formula, cell loc, cell shared,
		const void *data, int len);

	bool f1904;
	int fNewFunctionNr;
	BMallocIO fBook;
	std::vector<xlName> fNames;
	std::vector<int> fFonts, fStyles;
	std::vector<std::pair<int, float> > fColWidths;
	std::map<unsigned short, int> fFormats;
	// Un booleano per XF processato, parallelo a "fStyles" sopra
	// (stesso indice: lo "style" letto da ogni record cella in Pass2
	// e' proprio l'indice del suo XF, nell'ordine in cui Xf() li ha
	// visti) -- Pass2 lo controlla direttamente invece di risalire
	// dallo stile registrato in gStyleTable al suo fFormat e poi
	// rifare la stessa euristica: piu' semplice e non serve rifare la
	// classificazione data/non data due volte.
	std::vector<bool> fXfIsDate;
	std::vector<XLSHFormula> fSharedFormulas;
	std::vector<std::string> fSST;
	std::vector<EmbeddedImage> fImages;

	CCellView *fCellView;
	CContainer *fContainer;
};

