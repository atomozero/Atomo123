/*
	test_xls_translator.cpp

	Test del translator XLS legacy: Identify() (firma OLE2) piu' un
	test end-to-end di importazione (Translate()) sulle stringhe
	condivise BIFF8 (SST/LABELSST, vedi il commento su ReadSST in
	Excel.pass1.cpp) tramite due fixture generate con la libreria
	Python xlwt (licenza BSD, nessun problema di ridistribuzione --
	colma il gap lasciato dalla versione precedente di questo file, che
	non poteva includere il file .xls reale usato per la verifica
	manuale originale per licenza non chiara). Le fixture NON
	provengono da un file utente reale.

	Verificato anche manualmente con file .xls reali di un utente: sia
	a livello di translator (Identify/Translate) sia aprendolo dal
	vivo nell'app vera (Atomo123 file.xls) -- ha fatto emergere sei
	bug reali nel lettore BIFF/OLE2 legacy (engine/src/Excel/), tutti
	corretti, piu' la mancanza di SST/LABELSST (BIFF8/Excel97+, testata
	qui) che perdeva silenziosamente quasi tutto il testo nei file .xls
	moderni. Dettaglio completo in docs/TRANSLATORS.md e ROADMAP.md.
*/

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include <Application.h>
#include <DataIO.h>
#include <File.h>

#include "CellStyle.h"
#include "Formatter.h"
#include "Value.h"
#include "XlsTranslator.h"

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

int main()
{
	BApplication app("application/x-vnd.Atomo-TestXlsTranslator");

	BTranslator *translator = make_nth_translator(0, 0, 0);
	Check(translator != NULL, "make_nth_translator crea il translator");

	// Dati chiaramente non-OLE2: Identify deve rifiutarli.
	const char *notXls = "questo non e' un file XLS";
	BMemoryIO notXlsIn(notXls, strlen(notXls));
	translator_info info;
	status_t err = translator->Identify(&notXlsIn, NULL, NULL, &info, 0);
	Check(err != B_OK, "Identify rifiuta dati senza firma OLE2");

	// Firma OLE2 valida seguita da dati non significativi: Identify
	// deve riconoscere il formato (anche se poi Translate() fallirebbe
	// nel parsing BIFF vero e proprio, che richiede un file reale).
	unsigned char ole2Header[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
	char fakeXls[64];
	memcpy(fakeXls, ole2Header, 8);
	memset(fakeXls + 8, 0, sizeof(fakeXls) - 8);
	BMemoryIO fakeXlsIn(fakeXls, sizeof(fakeXls));

	err = translator->Identify(&fakeXlsIn, NULL, NULL, &info, 0);
	Check(err == B_OK, "Identify riconosce la firma OLE2");
	Check(info.type == kAtomoXlsFormat, "Identify classifica il tipo come XLS");

	// Translate() su un OLE2 con firma valida ma contenuto BIFF non
	// significativo deve fallire in modo pulito (eccezione catturata,
	// nessun blocco), non necessariamente produrre dati corretti: qui
	// si verifica solo la robustezza, non l'interpretazione reale.
	fakeXlsIn.Seek(0, SEEK_SET);
	BMallocIO ascdOut;
	err = translator->Translate(&fakeXlsIn, &info, NULL, kAtomoNativeFormat, &ascdOut);
	Check(err != B_OK, "Translate su OLE2 senza contenuto BIFF valido fallisce in modo pulito (non si blocca)");

	// Stringhe condivise BIFF8 (SST/LABELSST): tests/sample_sst.xls ha
	// A1/B1 ("Ciao mondo"/"Seconda stringa"), A2/A3 duplicate
	// ("Ripetuta", deve deduplicare in un'unica voce SST ma comparire
	// in entrambe le celle), B2 (42, un numero puro che non passa da
	// SST) e A4 una stringa lunga (oltre 250 caratteri, entro un solo
	// record SST) -- generata con xlwt (Python, licenza BSD), non un
	// file utente reale.
	{
		BFile sstFile("tests/sample_sst.xls", B_READ_ONLY);
		Check(sstFile.InitCheck() == B_OK, "apertura di tests/sample_sst.xls riuscita");

		translator_info sstInfo;
		status_t sstErr = translator->Identify(&sstFile, NULL, NULL, &sstInfo, 0);
		Check(sstErr == B_OK, "Identify riconosce sample_sst.xls");

		sstFile.Seek(0, SEEK_SET);
		BMallocIO sstOut;
		sstErr = translator->Translate(&sstFile, &sstInfo, NULL, kAtomoNativeFormat, &sstOut);
		Check(sstErr == B_OK, "Translate di sample_sst.xls riesce");

		if (sstErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)sstOut.Buffer();
			size_t len = sstOut.BufferLength();
			Check(len > 12 && memcmp(data, "ASCD", 4) == 0,
				"l'output di Translate di sample_sst.xls e' un ASCD valido");

			int32 count = 0;
			if (len > 12)
				memcpy(&count, data + 8, 4);
			Check(count == 6, "l'ASCD contiene le 6 celle di sample_sst.xls");

			bool foundA1 = false, foundB1 = false, foundA2 = false, foundA3 = false,
				foundB2 = false, foundA4Long = false;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= len; i++)
			{
				short row, col;
				int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len)
					break;
				std::string text((const char *)data + pos, l);
				pos += l;

				if (row == 1 && col == 1 && text == "Ciao mondo") foundA1 = true;
				if (row == 1 && col == 2 && text == "Seconda stringa") foundB1 = true;
				if (row == 2 && col == 1 && text == "Ripetuta") foundA2 = true;
				if (row == 3 && col == 1 && text == "Ripetuta") foundA3 = true;
				if (row == 2 && col == 2 && text == "42") foundB2 = true;
				if (row == 4 && col == 1 && text.size() > 250
					&& text.compare(0, 13, "Testo lungo: ") == 0)
					foundA4Long = true;
			}

			Check(foundA1, "A1 (\"Ciao mondo\", SST/LABELSST) importato correttamente");
			Check(foundB1, "B1 (\"Seconda stringa\", SST/LABELSST) importato correttamente");
			Check(foundA2 && foundA3,
				"A2/A3 (stessa stringa \"Ripetuta\", una sola voce SST) importate in entrambe le celle");
			Check(foundB2, "B2 (42, numero puro, non SST) resta un numero, non tocca fSST");
			Check(foundA4Long, "A4 (stringa lunga oltre 250 caratteri) importata per intero, non troncata");
		}
	}

	// Stessa cosa ma con una SST abbastanza grande (400 stringhe unite
	// oltre 12KB) da superare la dimensione massima di un record BIFF
	// (~8224 byte) e proseguire in uno o piu' record CONTINUE -- vedi
	// il commento su ReadSST in Excel.pass1.cpp per la stranezza BIFF8
	// per cui un CONTINUE che interrompe una stringa a meta' ricomincia
	// con un byte "grbit" tutto suo. Le stringhe sono generate con lo
	// stesso schema deterministico usato per costruire la fixture
	// ("Voce numero %04d di prova lunga", indice 0..399), cosi' il
	// test puo' ricostruirsi l'elenco atteso senza un file a parte.
	{
		BFile largeFile("tests/sample_sst_large.xls", B_READ_ONLY);
		Check(largeFile.InitCheck() == B_OK, "apertura di tests/sample_sst_large.xls riuscita");

		translator_info largeInfo;
		status_t largeErr = translator->Identify(&largeFile, NULL, NULL, &largeInfo, 0);
		Check(largeErr == B_OK, "Identify riconosce sample_sst_large.xls");

		largeFile.Seek(0, SEEK_SET);
		BMallocIO largeOut;
		largeErr = translator->Translate(&largeFile, &largeInfo, NULL, kAtomoNativeFormat, &largeOut);
		Check(largeErr == B_OK, "Translate di sample_sst_large.xls riesce");

		if (largeErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)largeOut.Buffer();
			size_t len = largeOut.BufferLength();
			int32 count = 0;
			if (len > 12)
				memcpy(&count, data + 8, 4);
			Check(count == 400, "l'ASCD contiene le 400 celle di sample_sst_large.xls");

			std::set<std::string> got;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= len; i++)
			{
				short row, col;
				int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len)
					break;
				got.insert(std::string((const char *)data + pos, l));
				pos += l;
			}

			int missing = 0;
			for (int i = 0; i < 400; i++)
			{
				char expected[64];
				snprintf(expected, sizeof(expected), "Voce numero %04d di prova lunga", i);
				if (got.find(expected) == got.end())
					missing++;
			}
			Check(missing == 0,
				"tutte le 400 stringhe di sample_sst_large.xls sopravvivono, attraversando i record CONTINUE della SST");
		}
	}

	// Codifica Windows-1252 -> UTF-8 (vedi il commento su
	// AppendCP1252Byte/AppendUnicodeAsUTF8 in Excel.h): tests/
	// sample_encoding.xls ha A1 (solo Latin-1, 0xA0-0xFF di CP1252,
	// il caso comune nei file italiani reali), B1 (caratteri della
	// sola porzione 0x80-0x9F di CP1252, diversa da Latin-1: en dash
	// e virgolette tipografiche) e C1 (un carattere fuori da CP1252,
	// che forza la stringa SST in UTF-16 "wide" invece che compressa
	// a un byte, esercitando il ramo diverso della stessa conversione).
	// Generato con xlwt (Python, licenza BSD, non un file utente reale).
	{
		BFile encFile("tests/sample_encoding.xls", B_READ_ONLY);
		Check(encFile.InitCheck() == B_OK, "apertura di tests/sample_encoding.xls riuscita");

		translator_info encInfo;
		status_t encErr = translator->Identify(&encFile, NULL, NULL, &encInfo, 0);
		Check(encErr == B_OK, "Identify riconosce sample_encoding.xls");

		encFile.Seek(0, SEEK_SET);
		BMallocIO encOut;
		encErr = translator->Translate(&encFile, &encInfo, NULL, kAtomoNativeFormat, &encOut);
		Check(encErr == B_OK, "Translate di sample_encoding.xls riesce");

		if (encErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)encOut.Buffer();
			size_t len = encOut.BufferLength();
			int32 count = 0;
			if (len > 12)
				memcpy(&count, data + 8, 4);

			bool foundA1 = false, foundB1 = false, foundC1 = false;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= len; i++)
			{
				short row, col;
				int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len)
					break;
				std::string text((const char *)data + pos, l);
				pos += l;

				if (row == 1 && col == 1
					&& text == "Modalità di pagamento: città, è, ò, à, ù")
					foundA1 = true;
				if (row == 1 && col == 2
					&& text == "Testo – con “virgolette” tipografiche")
					foundB1 = true;
				if (row == 1 && col == 3 && text == "Приве́т")
					foundC1 = true;
			}

			Check(foundA1,
				"A1 (solo Latin-1, es. \"città\"/\"è\") importato in UTF-8 corretto, non byte grezzi CP1252");
			Check(foundB1,
				"B1 (CP1252 0x80-0x9F, es. en dash/virgolette tipografiche) importato in UTF-8 corretto");
			Check(foundC1,
				"C1 (fuori CP1252, forza SST in UTF-16 \"wide\") importato in UTF-8 corretto");
		}
	}

	// Formati numero/data (record FORMAT/XF, vedi InitBuiltinFormats/
	// LooksLikeDateFormat in Excel.pass1.cpp): tests/sample_dates.xls
	// ha A1 (data con formato PERSONALIZZATO esplicito "DD/MM/YYYY"),
	// B1 (percentuale "0%" personalizzata, non deve mai passare per
	// una data), C1 (formato "0" personalizzato su un numero puro) e
	// D1 (nessuno stile, formato generico). Generato con xlwt (Python,
	// licenza BSD, non un file utente reale).
	{
		BFile datesFile("tests/sample_dates.xls", B_READ_ONLY);
		Check(datesFile.InitCheck() == B_OK, "apertura di tests/sample_dates.xls riuscita");

		translator_info datesInfo;
		status_t datesErr = translator->Identify(&datesFile, NULL, NULL, &datesInfo, 0);
		Check(datesErr == B_OK, "Identify riconosce sample_dates.xls");

		datesFile.Seek(0, SEEK_SET);
		BMallocIO datesOut;
		datesErr = translator->Translate(&datesFile, &datesInfo, NULL, kAtomoNativeFormat, &datesOut);
		Check(datesErr == B_OK, "Translate di sample_dates.xls riesce");

		if (datesErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)datesOut.Buffer();
			size_t len = datesOut.BufferLength();
			int32 count = 0;
			if (len > 12)
				memcpy(&count, data + 8, 4);

			bool foundA1 = false, foundB1 = false, foundC1 = false, foundD1 = false;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= len; i++)
			{
				short row, col;
				int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len)
					break;
				std::string text((const char *)data + pos, l);
				pos += l;

				if (row == 1 && col == 1 && text == "22/05/2018") foundA1 = true;
				if (row == 1 && col == 2 && text == "0.42") foundB1 = true;
				if (row == 1 && col == 3 && text == "43242") foundC1 = true;
				if (row == 1 && col == 4 && text == "100") foundD1 = true;
			}

			Check(foundA1,
				"A1 (data con formato personalizzato \"DD/MM/YYYY\") importata come 22/05/2018, non il seriale grezzo");
			Check(foundB1,
				"B1 (percentuale personalizzata \"0%\") resta un numero puro, mai scambiata per una data");
			Check(foundC1,
				"C1 (formato personalizzato \"0\" su un numero puro) resta un numero, non una data");
			Check(foundD1, "D1 (nessuno stile) resta un numero normale");
		}
	}

	// Formule (Excel.formula.cpp/ParseXLFormula, layout BIFF8 dei
	// riferimenti a cella): tests/sample_formula.xls ha A1=10, A2=20,
	// A3=A1+A2 (riferimento diretto), A4=SUM(A1:A2) (intervallo) --
	// verifica solo che l'importazione non si blocchi ne' fallisca e
	// che il riferimento a cella diretto (A3) si ricostruisca con i
	// nomi giusti (prova diretta che ptgRef legge la struttura BIFF8
	// giusta, non quella BIFF5 per cui questo codice era stato scritto
	// in origine -- bug reale scoperto aprendo un file .xls reale, un
	// ciclo infinito nel parser di formule). Generato con xlwt.
	{
		BFile formulaFile("tests/sample_formula.xls", B_READ_ONLY);
		Check(formulaFile.InitCheck() == B_OK, "apertura di tests/sample_formula.xls riuscita");

		translator_info formulaInfo;
		status_t formulaErr = translator->Identify(&formulaFile, NULL, NULL, &formulaInfo, 0);
		Check(formulaErr == B_OK, "Identify riconosce sample_formula.xls");

		formulaFile.Seek(0, SEEK_SET);
		BMallocIO formulaOut;
		formulaErr = translator->Translate(&formulaFile, &formulaInfo, NULL, kAtomoNativeFormat, &formulaOut);
		Check(formulaErr == B_OK,
			"Translate di sample_formula.xls riesce (non si blocca sui riferimenti a cella BIFF8)");

		if (formulaErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)formulaOut.Buffer();
			size_t len = formulaOut.BufferLength();
			int32 count = 0;
			if (len > 12)
				memcpy(&count, data + 8, 4);

			bool foundA1 = false, foundA2 = false, foundA3 = false;
			size_t pos = 12;
			for (int32 i = 0; i < count && pos + 8 <= len; i++)
			{
				short row, col;
				int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len)
					break;
				std::string text((const char *)data + pos, l);
				pos += l;

				if (row == 1 && col == 1 && text == "10") foundA1 = true;
				if (row == 2 && col == 1 && text == "20") foundA2 = true;
				if (row == 3 && col == 1 && text.find("A1") != std::string::npos
					&& text.find("A2") != std::string::npos)
					foundA3 = true;
			}

			Check(foundA1 && foundA2, "A1/A2 (numeri semplici) importati correttamente");
			Check(foundA3,
				"A3 (=A1+A2) ricostruita citando A1 e A2 -- prova diretta che ptgRef legge la struttura BIFF8 (colonna a 2 byte con i flag), non quella BIFF5");
		}
	}

	// Font/allineamento/testo a capo (WriteASCD estende CellStyle,
	// vedi il commento in XlsTranslator.cpp): tests/sample_style.xls
	// ha A1 (grassetto), B1 (centrato), C1 (testo a capo) e D1 (nessuno
	// stile esplicito). Verifica diretta che le sezioni font/
	// allineamento/testo a capo del formato nativo, finora sempre
	// vuote per XLS, ora contengano i valori REALI gia' risolti da
	// Xf()/Font() durante il parsing BIFF -- bug reale scoperto
	// confrontando visivamente con Excel vero l'importazione di una
	// fattura reale: un titolo in grassetto spariva del tutto aprendo
	// il file con Atomo123.
	{
		BFile styleFile("tests/sample_style.xls", B_READ_ONLY);
		Check(styleFile.InitCheck() == B_OK, "apertura di tests/sample_style.xls riuscita");

		translator_info styleInfo;
		status_t styleErr = translator->Identify(&styleFile, NULL, NULL, &styleInfo, 0);
		Check(styleErr == B_OK, "Identify riconosce sample_style.xls");

		styleFile.Seek(0, SEEK_SET);
		BMallocIO styleOut;
		styleErr = translator->Translate(&styleFile, &styleInfo, NULL, kAtomoNativeFormat, &styleOut);
		Check(styleErr == B_OK, "Translate di sample_style.xls riesce");

		if (styleErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)styleOut.Buffer();
			size_t len = styleOut.BufferLength();
			size_t pos = 12;

			int32 cellCount = 0;
			if (len > 12)
				memcpy(&cellCount, data + 8, 4);
			for (int32 i = 0; i < cellCount && pos + 8 <= len; i++)
			{
				short row, col; int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len) break;
				pos += l;
			}

			int32 n;
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4+4*4); } // chart
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // colWidth
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+8); } // cellColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+8); } // columnColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // rowHeight
			pos += 8; // frozen

			bool foundBoldA1 = false;
			if (pos + 4 <= len)
			{
				int32 fontCount;
				memcpy(&fontCount, data + pos, 4); pos += 4;
				for (int32 i = 0; i < fontCount && pos + 2+2+64+64+4 <= len; i++)
				{
					short row, col; char family[64], fstyle[64]; float size;
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&col, data + pos, 2); pos += 2;
					memcpy(family, data + pos, 64); pos += 64;
					memcpy(fstyle, data + pos, 64); pos += 64;
					memcpy(&size, data + pos, 4); pos += 4;
					if (row == 1 && col == 1 && strcmp(fstyle, "Bold") == 0)
						foundBoldA1 = true;
				}
			}
			Check(foundBoldA1,
				"A1 (grassetto nel file originale) importato come font Bold, non perso/sostituito con Regular");

			bool foundCenterB1 = false;
			if (pos + 4 <= len)
			{
				int32 alignCount;
				memcpy(&alignCount, data + pos, 4); pos += 4;
				for (int32 i = 0; i < alignCount && pos + 5 <= len; i++)
				{
					short row, col; int8 al;
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&col, data + pos, 2); pos += 2;
					memcpy(&al, data + pos, 1); pos += 1;
					if (row == 1 && col == 2 && al == eAlignCenter)
						foundCenterB1 = true;
				}
			}
			Check(foundCenterB1, "B1 (centrato nel file originale) importato con l'allineamento giusto");

			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+4); } // border
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+4); } // format
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2); } // underline

			bool foundWrapC1 = false;
			if (pos + 4 <= len)
			{
				int32 wrapCount;
				memcpy(&wrapCount, data + pos, 4); pos += 4;
				for (int32 i = 0; i < wrapCount && pos + 4 <= len; i++)
				{
					short row, col;
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&col, data + pos, 2); pos += 2;
					if (row == 1 && col == 3)
						foundWrapC1 = true;
				}
			}
			Check(foundWrapC1, "C1 (testo a capo nel file originale) importato con fWrapText attivo");
		}
	}

	// Larghezza colonne (record COLINFO, vedi il commento nel case
	// COLINFO di Excel.pass1.cpp): tests/sample_colwidth.xls ha la
	// colonna A molto stretta (~3 caratteri), la colonna B molto
	// larga (~40 caratteri) e la colonna C non toccata (default).
	// Prima di questa modifica WriteASCD scriveva sempre colWidthCount
	// = 0: aprendo una fattura reale con colonne dimensionate a mano
	// tutte le colonne tornavano alla larghezza predefinita.
	{
		BFile cwFile("tests/sample_colwidth.xls", B_READ_ONLY);
		Check(cwFile.InitCheck() == B_OK, "apertura di tests/sample_colwidth.xls riuscita");

		translator_info cwInfo;
		status_t cwErr = translator->Identify(&cwFile, NULL, NULL, &cwInfo, 0);
		Check(cwErr == B_OK, "Identify riconosce sample_colwidth.xls");

		cwFile.Seek(0, SEEK_SET);
		BMallocIO cwOut;
		cwErr = translator->Translate(&cwFile, &cwInfo, NULL, kAtomoNativeFormat, &cwOut);
		Check(cwErr == B_OK, "Translate di sample_colwidth.xls riesce");

		if (cwErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)cwOut.Buffer();
			size_t len = cwOut.BufferLength();
			size_t pos = 12;

			int32 cellCount = 0;
			if (len > 12)
				memcpy(&cellCount, data + 8, 4);
			for (int32 i = 0; i < cellCount && pos + 8 <= len; i++)
			{
				short row, col; int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len) break;
				pos += l;
			}

			int32 n;
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4+4*4); } // chart

			bool foundCol1 = false, foundCol2 = false;
			float widthCol1 = 0.0f, widthCol2 = 0.0f;
			if (pos + 4 <= len)
			{
				int32 colWidthCount;
				memcpy(&colWidthCount, data + pos, 4); pos += 4;
				Check(colWidthCount == 2,
					"colWidthCount == 2 (solo le colonne A e B hanno un COLINFO esplicito)");
				for (int32 i = 0; i < colWidthCount && pos + 2+4 <= len; i++)
				{
					short col; float width;
					memcpy(&col, data + pos, 2); pos += 2;
					memcpy(&width, data + pos, 4); pos += 4;
					if (col == 1) { foundCol1 = true; widthCol1 = width; }
					if (col == 2) { foundCol2 = true; widthCol2 = width; }
				}
			}
			Check(foundCol1 && widthCol1 > 0.0f && widthCol1 < widthCol2,
				"colonna A (stretta nel file originale) importata con una larghezza reale, minore della B");
			Check(foundCol2 && widthCol2 > widthCol1,
				"colonna B (larga nel file originale) importata con una larghezza reale, maggiore della A");
		}
	}

	// Colori di sfondo/testo e bordi (record XF, vedi il commento in
	// Excel.pass1.cpp): tests/sample_colors.xls ha A1 (sfondo giallo
	// pieno), B1 (testo rosso), C1 (bordo su tutti e quattro i lati),
	// D1 (solo bordo superiore) e E1 (nessuno stile, controllo
	// negativo). Prima di questa modifica Xf() si fermava agli otto
	// byte iniziali del record XF (font/formato/allineamento): sfondo
	// e bordi restavano sempre assenti, sparendo del tutto aprendo con
	// Atomo123 una fattura reale con intestazioni colorate/bordate.
	{
		BFile colorsFile("tests/sample_colors.xls", B_READ_ONLY);
		Check(colorsFile.InitCheck() == B_OK, "apertura di tests/sample_colors.xls riuscita");

		translator_info colorsInfo;
		status_t colorsErr = translator->Identify(&colorsFile, NULL, NULL, &colorsInfo, 0);
		Check(colorsErr == B_OK, "Identify riconosce sample_colors.xls");

		colorsFile.Seek(0, SEEK_SET);
		BMallocIO colorsOut;
		colorsErr = translator->Translate(&colorsFile, &colorsInfo, NULL, kAtomoNativeFormat, &colorsOut);
		Check(colorsErr == B_OK, "Translate di sample_colors.xls riesce");

		if (colorsErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)colorsOut.Buffer();
			size_t len = colorsOut.BufferLength();
			size_t pos = 12;

			int32 cellCount = 0;
			if (len > 12)
				memcpy(&cellCount, data + 8, 4);
			for (int32 i = 0; i < cellCount && pos + 8 <= len; i++)
			{
				short row, col; int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len) break;
				pos += l;
			}

			int32 n;
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4+4*4); } // chart
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // colWidth

			bool foundYellowA1 = false, foundRedTextB1 = false;
			if (pos + 4 <= len)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, data + pos, 4); pos += 4;
				Check(cellColorCount == 2,
					"cellColorCount == 2 (solo A1 e B1 hanno un colore non predefinito, non C1/D1/E1)");
				for (int32 i = 0; i < cellColorCount && pos + 2+2+8 <= len; i++)
				{
					short row, col; unsigned char buf[8];
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&col, data + pos, 2); pos += 2;
					memcpy(buf, data + pos, 8); pos += 8;
					if (row == 1 && col == 1 && buf[0] == 255 && buf[1] == 255 && buf[2] == 0)
						foundYellowA1 = true;
					if (row == 1 && col == 2 && buf[4] == 255 && buf[5] == 0 && buf[6] == 0)
						foundRedTextB1 = true;
				}
			}
			Check(foundYellowA1,
				"A1 (sfondo giallo nel file originale) importato con lo sfondo reale, non bianco");
			Check(foundRedTextB1,
				"B1 (testo rosso nel file originale) importato con il colore testo reale, non nero");

			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+8); } // columnColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // rowHeight
			pos += 8; // frozen
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+64+64+4); } // font
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+1); } // align

			bool foundFullBorderC1 = false, foundTopOnlyD1 = false;
			if (pos + 4 <= len)
			{
				int32 borderCount;
				memcpy(&borderCount, data + pos, 4); pos += 4;
				Check(borderCount == 2,
					"borderCount == 2 (solo C1 e D1 hanno un bordo, non A1/B1/E1)");
				for (int32 i = 0; i < borderCount && pos + 2+2+4 <= len; i++)
				{
					short row, col; unsigned char sides[4];
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&col, data + pos, 2); pos += 2;
					memcpy(sides, data + pos, 4); pos += 4;
					if (row == 1 && col == 3 && sides[0] && sides[1] && sides[2] && sides[3])
						foundFullBorderC1 = true;
					if (row == 1 && col == 4 && sides[0] && !sides[1] && !sides[2] && !sides[3])
						foundTopOnlyD1 = true;
				}
			}
			Check(foundFullBorderC1,
				"C1 (bordo su tutti i lati nel file originale) importato con tutti e quattro i lati presenti");
			Check(foundTopOnlyD1,
				"D1 (solo bordo superiore nel file originale) importato con solo il lato superiore presente");
		}
	}

	// Immagini incorporate (record MSODRAWINGGROUP/MSODRAWING, formato
	// binario Escher -- vedi Excel.escher.cpp): tests/sample_image.xls
	// non e' generato con xlwt (che non sa scrivere immagini) ma con
	// uno script Python dedicato che inserisce a mano, byte per byte,
	// gli stessi record Escher trovati in una fattura reale con un
	// logo incorporato (struttura verificata contro quel file prima
	// di scrivere il parser -- vedi il commento in Excel.escher.cpp),
	// con un piccolo JPEG rosso sintetico al posto del logo vero.
	// Prima di questa modifica il filtro XLS ignorava del tutto questi
	// record: un'immagine incorporata spariva sempre, non solo dal
	// file di test ma anche dalla fattura reale usata per la verifica
	// in Fase 5. Il rettangolo di ancoraggio della fixture copre
	// esattamente le colonne 0-1 e le righe 0-2 (colEnd=2/rowEnd=3,
	// nessuno scarto): la dimensione attesa e' quindi due colonne e
	// tre righe predefinite (kColWidth=80/kRowHeight=20 di
	// SheetView.h, duplicati in Excel.escher.cpp), non la dimensione
	// nativa del JPEG sintetico (8x6) -- vedi il commento su
	// SpanPixels in Excel.escher.cpp sul perche' non si usa piu' la
	// dimensione nativa.
	{
		BFile imageFile("tests/sample_image.xls", B_READ_ONLY);
		Check(imageFile.InitCheck() == B_OK, "apertura di tests/sample_image.xls riuscita");

		translator_info imageInfo;
		status_t imageErr = translator->Identify(&imageFile, NULL, NULL, &imageInfo, 0);
		Check(imageErr == B_OK, "Identify riconosce sample_image.xls");

		imageFile.Seek(0, SEEK_SET);
		BMallocIO imageOut;
		imageErr = translator->Translate(&imageFile, &imageInfo, NULL, kAtomoNativeFormat, &imageOut);
		Check(imageErr == B_OK, "Translate di sample_image.xls riesce");

		if (imageErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)imageOut.Buffer();
			size_t len = imageOut.BufferLength();
			size_t pos = 12;

			int32 cellCount = 0;
			if (len > 12)
				memcpy(&cellCount, data + 8, 4);
			for (int32 i = 0; i < cellCount && pos + 8 <= len; i++)
			{
				short row, col; int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len) break;
				pos += l;
			}

			int32 n;
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4+4*4); } // chart
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // colWidth
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+8); } // cellColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+8); } // columnColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // rowHeight
			pos += 8; // frozen
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+64+64+4); } // font
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+1); } // align
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+4); } // border
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+4); } // format
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2); } // underline
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2); } // wrap
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4); } // merge

			bool foundImageA1 = false;
			bool sizeCorrect = false;
			bool jpegSignature = false;
			if (pos + 4 <= len)
			{
				int32 imageCount;
				memcpy(&imageCount, data + pos, 4); pos += 4;
				Check(imageCount == 1, "imageCount == 1 (un solo logo incorporato nel file originale)");
				for (int32 i = 0; i < imageCount && pos + 2+2+16+4 <= len; i++)
				{
					short row, col; float geom[4]; int32 pngLen;
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&col, data + pos, 2); pos += 2;
					memcpy(geom, data + pos, 16); pos += 16;
					memcpy(&pngLen, data + pos, 4); pos += 4;
					if (row == 1 && col == 1)
					{
						foundImageA1 = true;
						sizeCorrect = (geom[2] == 160.0f && geom[3] == 60.0f);
						if (pngLen >= 4 && pos + 4 <= len)
						{
							jpegSignature = data[pos] == 0xFF && data[pos+1] == 0xD8
								&& data[pos+2] == 0xFF;
						}
					}
					if (pos + (size_t)pngLen <= len)
						pos += pngLen;
				}
			}
			Check(foundImageA1,
				"l'immagine incorporata e' ancorata ad A1, come nel file originale");
			Check(sizeCorrect,
				"l'immagine incorporata ha la dimensione del rettangolo di ancoraggio (160x60, due colonne per tre righe predefinite), non quella nativa del JPEG (8x6) ne' zero");
			Check(jpegSignature,
				"i byte dell'immagine incorporata sono un JPEG valido (firma FF D8 FF), non dati troncati/corrotti");
		}
	}

	// Formato numero personalizzato "0" ripetuto (es. "00000"): intero
	// riempito di zeri a sinistra -- tests/sample_zeropad.xls ha A1=73
	// con formato "00000" (deve rendersi "00073"), B1=0 con lo stesso
	// formato (deve rendersi "00000", non una stringa vuota), C1=123456
	// con lo stesso formato (supera la larghezza del template, resta
	// per esteso invece di troncare), D1=73 senza nessun formato
	// esplicito (controllo negativo). Prima di questa modifica
	// ParseTemplate (Formatter.template.cpp, ereditato da Sum-It) non
	// aveva nessun concetto di "riempimento intero" -- solo "$"/"%"/"."
	// per distinguere valuta/percentuale/cifre decimali -- quindi un
	// template fatto solo di zeri finiva silenziosamente nel formato
	// generico, perdendo gli zeri iniziali: bug reale scoperto
	// confrontando visivamente con Excel vero l'importazione di una
	// fattura reale ("Preavviso N. 00073" mostrato come "73").
	{
		BFile zpFile("tests/sample_zeropad.xls", B_READ_ONLY);
		Check(zpFile.InitCheck() == B_OK, "apertura di tests/sample_zeropad.xls riuscita");

		translator_info zpInfo;
		status_t zpErr = translator->Identify(&zpFile, NULL, NULL, &zpInfo, 0);
		Check(zpErr == B_OK, "Identify riconosce sample_zeropad.xls");

		zpFile.Seek(0, SEEK_SET);
		BMallocIO zpOut;
		zpErr = translator->Translate(&zpFile, &zpInfo, NULL, kAtomoNativeFormat, &zpOut);
		Check(zpErr == B_OK, "Translate di sample_zeropad.xls riesce");

		if (zpErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)zpOut.Buffer();
			size_t len = zpOut.BufferLength();
			size_t pos = 12;

			int32 cellCount = 0;
			if (len > 12)
				memcpy(&cellCount, data + 8, 4);
			for (int32 i = 0; i < cellCount && pos + 8 <= len; i++)
			{
				short row, col; int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len) break;
				pos += l;
			}

			int32 n;
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4+4*4); } // chart
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // colWidth
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+8); } // cellColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+8); } // columnColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // rowHeight
			pos += 8; // frozen
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+64+64+4); } // font
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+1); } // align
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+4); } // border

			int32 fmtA1 = -1, fmtB1 = -1, fmtC1 = -1;
			bool foundD1 = false;
			if (pos + 4 <= len)
			{
				int32 formatCount;
				memcpy(&formatCount, data + pos, 4); pos += 4;
				for (int32 i = 0; i < formatCount && pos + 2+2+4 <= len; i++)
				{
					short row, col; int32 fmt;
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&col, data + pos, 2); pos += 2;
					memcpy(&fmt, data + pos, 4); pos += 4;
					if (row == 1 && col == 1) fmtA1 = fmt;
					if (row == 1 && col == 2) fmtB1 = fmt;
					if (row == 1 && col == 3) fmtC1 = fmt;
					if (row == 1 && col == 4) foundD1 = true;
				}
			}
			Check(fmtA1 >= 0, "A1 (formato \"00000\" nel file originale) ha un fFormat non generico");
			Check(!foundD1, "D1 (nessun formato esplicito) non finisce nella sezione formati");

			char outStr[64];
			if (fmtA1 >= 0)
			{
				CFormatter nf(fmtA1);
				nf.FormatValue(Value(73.0), outStr, 0, 1e6);
				Check(strcmp(outStr, "00073") == 0,
					"A1 (73 col formato \"00000\") si rende \"00073\", non \"73\"");
			}
			if (fmtB1 >= 0)
			{
				CFormatter nf(fmtB1);
				nf.FormatValue(Value(0.0), outStr, 0, 1e6);
				Check(strcmp(outStr, "00000") == 0,
					"B1 (0 col formato \"00000\") si rende \"00000\", non una stringa vuota");
			}
			if (fmtC1 >= 0)
			{
				CFormatter nf(fmtC1);
				nf.FormatValue(Value(123456.0), outStr, 0, 1e6);
				Check(strcmp(outStr, "123456") == 0,
					"C1 (123456, oltre la larghezza del template) si rende per esteso, non troncato");
			}
		}
	}

	// Celle unite (record MERGEDCELLS 0x0E5, vedi il case omonimo in
	// Excel.pass1.cpp): tests/sample_merge.xls ha A1:B1 unite ("Unita
	// A1:B1"), C1 non unita ("Non unita", controllo negativo) e A3:A4
	// unite verticalmente ("Unita A3:A4"), generato con xlwt
	// (sheet.write_merge, licenza BSD, non un file utente reale).
	// Prima di questa modifica il filtro XLS ignorava del tutto questo
	// record: le celle unite di un'intestazione reale (es.
	// "Corrispettivi"/"Spese escluse" unite su due righe) apparivano
	// come righe separate invece che come un'unica cella centrata.
	{
		BFile mergeFile("tests/sample_merge.xls", B_READ_ONLY);
		Check(mergeFile.InitCheck() == B_OK, "apertura di tests/sample_merge.xls riuscita");

		translator_info mergeInfo;
		status_t mergeErr = translator->Identify(&mergeFile, NULL, NULL, &mergeInfo, 0);
		Check(mergeErr == B_OK, "Identify riconosce sample_merge.xls");

		mergeFile.Seek(0, SEEK_SET);
		BMallocIO mergeOut;
		mergeErr = translator->Translate(&mergeFile, &mergeInfo, NULL, kAtomoNativeFormat, &mergeOut);
		Check(mergeErr == B_OK, "Translate di sample_merge.xls riesce");

		if (mergeErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)mergeOut.Buffer();
			size_t len = mergeOut.BufferLength();
			size_t pos = 12;

			int32 cellCount = 0;
			if (len > 12)
				memcpy(&cellCount, data + 8, 4);
			for (int32 i = 0; i < cellCount && pos + 8 <= len; i++)
			{
				short row, col; int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len) break;
				pos += l;
			}

			int32 n;
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4+4*4); } // chart
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // colWidth
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+8); } // cellColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+8); } // columnColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // rowHeight
			pos += 8; // frozen
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+64+64+4); } // font
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+1); } // align
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+4); } // border
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+4); } // format
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2); } // underline
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2); } // wrap

			bool foundA1B1 = false, foundA3A4 = false;
			if (pos + 4 <= len)
			{
				int32 mergeCount;
				memcpy(&mergeCount, data + pos, 4); pos += 4;
				Check(mergeCount == 2,
					"mergeCount == 2 (A1:B1 e A3:A4 unite nel file originale, C1 no)");
				for (int32 i = 0; i < mergeCount && pos + 2*4 <= len; i++)
				{
					short top, left, bottom, right;
					memcpy(&top, data + pos, 2); pos += 2;
					memcpy(&left, data + pos, 2); pos += 2;
					memcpy(&bottom, data + pos, 2); pos += 2;
					memcpy(&right, data + pos, 2); pos += 2;
					if (top == 1 && left == 1 && bottom == 1 && right == 2)
						foundA1B1 = true;
					if (top == 3 && left == 1 && bottom == 4 && right == 1)
						foundA3A4 = true;
				}
			}
			Check(foundA1B1, "A1:B1 (unite orizzontalmente nel file originale) importate come intervallo unito");
			Check(foundA3A4, "A3:A4 (unite verticalmente nel file originale) importate come intervallo unito");
		}
	}

	// Altezza di riga esplicita (record ROW, vedi il commento nel case
	// omonimo di Excel.pass1.cpp): tests/sample_rowheight.xls ha la
	// riga 1 molto alta (800 twips, 40pt), la riga 3 molto bassa (150
	// twips, 7.5pt) e la riga 2 non toccata (l'altezza predefinita che
	// xlwt scrive comunque per ogni riga con contenuto, non un'assenza
	// di record ROW). Prima di questa modifica WriteASCD scriveva
	// sempre rowHeightCount = 0: aprendo una fattura reale con righe
	// ridimensionate a mano tutte le righe tornavano all'altezza
	// predefinita, bug reale scoperto confrontando visivamente con
	// Excel vero.
	{
		BFile rhFile("tests/sample_rowheight.xls", B_READ_ONLY);
		Check(rhFile.InitCheck() == B_OK, "apertura di tests/sample_rowheight.xls riuscita");

		translator_info rhInfo;
		status_t rhErr = translator->Identify(&rhFile, NULL, NULL, &rhInfo, 0);
		Check(rhErr == B_OK, "Identify riconosce sample_rowheight.xls");

		rhFile.Seek(0, SEEK_SET);
		BMallocIO rhOut;
		rhErr = translator->Translate(&rhFile, &rhInfo, NULL, kAtomoNativeFormat, &rhOut);
		Check(rhErr == B_OK, "Translate di sample_rowheight.xls riesce");

		if (rhErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)rhOut.Buffer();
			size_t len = rhOut.BufferLength();
			size_t pos = 12;

			int32 cellCount = 0;
			if (len > 12)
				memcpy(&cellCount, data + 8, 4);
			for (int32 i = 0; i < cellCount && pos + 8 <= len; i++)
			{
				short row, col; int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len) break;
				pos += l;
			}

			int32 n;
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4+4*4); } // chart
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // colWidth
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+2+8); } // cellColor
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+8); } // columnColor

			float heightRow1 = -1.0f, heightRow2 = -1.0f, heightRow3 = -1.0f;
			bool foundRow1 = false, foundRow2 = false, foundRow3 = false;
			if (pos + 4 <= len)
			{
				int32 rowHeightCount;
				memcpy(&rowHeightCount, data + pos, 4); pos += 4;
				Check(rowHeightCount == 3,
					"rowHeightCount == 3 (le tre righe con contenuto nel file originale hanno tutte un record ROW)");
				for (int32 i = 0; i < rowHeightCount && pos + 2+4 <= len; i++)
				{
					short row; float height;
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&height, data + pos, 4); pos += 4;
					if (row == 1) { foundRow1 = true; heightRow1 = height; }
					if (row == 2) { foundRow2 = true; heightRow2 = height; }
					if (row == 3) { foundRow3 = true; heightRow3 = height; }
				}
			}
			Check(foundRow1 && foundRow2 && foundRow3,
				"le tre righe (1, 2 e 3) sono tutte presenti nella sezione altezze di riga");
			Check(heightRow1 > heightRow2 && heightRow2 > heightRow3,
				"riga 1 (molto alta nel file originale) > riga 2 (normale) > riga 3 (molto bassa), non tutte uguali all'altezza predefinita");
		}
	}

	// Celle vuote ma formattate su una riga intera (record MULBLANK,
	// vedi il case omonimo in Excel.pass2.cpp): tests/sample_mulblank.xls
	// ha A1:F1 vuote ma con sfondo giallo, G1 non toccata (controllo
	// negativo) e A2 con del testo normale (secondo controllo
	// negativo). Il record MULBLANK termina con un campo "colLast"
	// dopo l'ultimo indice di stile -- prima di questa modifica il
	// calcolo del numero di colonne non lo escludeva, leggendo un
	// indice di stile in piu' e applicandolo per sbaglio a una cella
	// fantasma subito dopo l'intervallo vero (qui G1, che risultava
	// colorata quando in realta' e' sempre rimasta bianca nel file
	// originale) -- bug reale scoperto confrontando con Excel vero
	// una fattura reale.
	{
		BFile mbFile("tests/sample_mulblank.xls", B_READ_ONLY);
		Check(mbFile.InitCheck() == B_OK, "apertura di tests/sample_mulblank.xls riuscita");

		translator_info mbInfo;
		status_t mbErr = translator->Identify(&mbFile, NULL, NULL, &mbInfo, 0);
		Check(mbErr == B_OK, "Identify riconosce sample_mulblank.xls");

		mbFile.Seek(0, SEEK_SET);
		BMallocIO mbOut;
		mbErr = translator->Translate(&mbFile, &mbInfo, NULL, kAtomoNativeFormat, &mbOut);
		Check(mbErr == B_OK, "Translate di sample_mulblank.xls riesce");

		if (mbErr == B_OK)
		{
			const unsigned char *data = (const unsigned char *)mbOut.Buffer();
			size_t len = mbOut.BufferLength();
			size_t pos = 12;

			int32 cellCount = 0;
			if (len > 12)
				memcpy(&cellCount, data + 8, 4);
			for (int32 i = 0; i < cellCount && pos + 8 <= len; i++)
			{
				short row, col; int32 l;
				memcpy(&row, data + pos, 2); pos += 2;
				memcpy(&col, data + pos, 2); pos += 2;
				memcpy(&l, data + pos, 4); pos += 4;
				if (pos + (size_t)l > len) break;
				pos += l;
			}

			int32 n;
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2*4+4*4); } // chart
			if (pos + 4 <= len) { memcpy(&n, data + pos, 4); pos += 4; pos += n * (2+4); } // colWidth

			bool foundG1 = false;
			int32 yellowCount = 0;
			if (pos + 4 <= len)
			{
				int32 cellColorCount;
				memcpy(&cellColorCount, data + pos, 4); pos += 4;
				for (int32 i = 0; i < cellColorCount && pos + 2+2+8 <= len; i++)
				{
					short row, col; unsigned char buf[8];
					memcpy(&row, data + pos, 2); pos += 2;
					memcpy(&col, data + pos, 2); pos += 2;
					memcpy(buf, data + pos, 8); pos += 8;
					if (row == 1 && col == 7)
						foundG1 = true;
					if (row == 1 && col >= 1 && col <= 6
						&& buf[0] == 255 && buf[1] == 255 && buf[2] == 0)
						yellowCount++;
				}
				Check(cellColorCount == 6,
					"cellColorCount == 6 (solo A1:F1 hanno uno sfondo non predefinito nel file originale)");
			}
			Check(yellowCount == 6,
				"A1:F1 (sfondo giallo nel file originale) importate tutte con lo sfondo reale");
			Check(!foundG1,
				"G1 (subito dopo l'intervallo MULBLANK vero) non risulta colorata per sbaglio, resta bianca");
		}
	}

	translator->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
