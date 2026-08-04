/*
	test_persistence.cpp

	Verifica la persistenza nel formato nativo delle quattro preferenze
	rimaste "solo per la sessione corrente" dopo la Fase 7 (Blocca
	riquadri, altezza di riga, font di cella -- grassetto/corsivo -- e
	allineamento, Fase 10) piu' i bordi di cella (Fase 11) e il
	formato numero (Fase 12, CellStyle::fFormat -- scoperto senza
	nessuna sezione dedicata proprio scrivendo l'import XLSX di Fase
	12: anche il menu Formato esistente da prima di questa fase perdeva
	silenziosamente il formato scelto al salvataggio/riapertura) --
	tutti salvati/ricaricati tramite sezioni opzionali in coda al
	formato ASCD, stesso principio gia' usato per larghezza di colonna
	e colori (vedi il commento in AscdIO.h). Il font e' il caso piu'
	delicato: CellStyle::fFont e' un indice VOLATILE in
	gFontSizeTable, valido solo per la sessione che l'ha creato -- si
	scrive/rilegge la tripla famiglia/stile/dimensione, non l'indice
	grezzo (vedi il commento in AscdIO.cpp).

	Stesso motivo di BApplication di test_ascd_io.cpp/test_ascd_book.cpp:
	GetCellFormula su una formula passa da BFont::StringWidth, che
	senza un'app registrata resta bloccato in attesa dell'app_server
	-- per lo stesso motivo, un font/famiglia realmente installato
	(quello di be_plain_font, non un nome inventato) evita di dover
	passare dal ripiego di CFontMetrics che referenzia gPrefs (qui
	NULL, nessuna vera App::App() in questo harness).
*/

#include <cstdio>
#include <cstring>
#include <vector>

#include <Application.h>
#include <File.h>
#include <Font.h>

#include "AscdIO.h"
#include "Cell.h"
#include "CellStyle.h"
#include "FontMetrics.h"
#include "Formatter.h"
#include "Container.h"
#include "CellParser.h"

static int gFailures = 0;

static void Check(bool condition, const char* what)
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
	BApplication app("application/x-vnd.Atomo-TestPersistence");

	const char* path = "/tmp/test_persistence.ascd";

	CContainer* doc = new CContainer(NULL, NULL);
	TryToParseString("10", cell(1, 1), doc, true); // A1
	TryToParseString("20", cell(1, 2), doc, true); // A2, in grassetto sotto
	TryToParseString("30", cell(1, 3), doc, true); // A3, allineata a destra sotto

	std::vector<std::pair<int, float> > rowHeights;
	rowHeights.push_back(std::make_pair(1, 40.0f)); // riga 1 alta il doppio
	rowHeights.push_back(std::make_pair(3, 10.0f)); // riga 3 al minimo

	int frozenRows = 2, frozenCols = 1;
	bool showGrid = false; // non il default (true): per essere sicuri che sia DAVVERO riletto, non solo lasciato al valore di partenza
	bool hasTabColor = true;
	rgb_color tabColor = { 0, 176, 80, 255 }; // verde, lo stesso tabColor del file reale che ha motivato questa fase

	// Font non predefinito su A2 (grassetto), sulla famiglia REALE del
	// font di sistema (vedi il commento in cima al file sul perche').
	font_family sysFamily;
	font_style sysStyle;
	be_plain_font->GetFamilyAndStyle(&sysFamily, &sysStyle);
	int boldFontID = (int)gFontSizeTable.GetFontID(sysFamily, "Bold", 14.0f);
	{
		CellStyle cs;
		doc->GetCellStyle(cell(1, 2), cs);
		cs.fFont = boldFontID;
		doc->SetCellStyle(cell(1, 2), cs);
	}

	// Allineamento non predefinito su A3 (a destra).
	{
		CellStyle cs;
		doc->GetCellStyle(cell(1, 3), cs);
		cs.fAlignment = eAlignRight;
		doc->SetCellStyle(cell(1, 3), cs);
	}

	// Bordi (Fase 11) su A2: sinistro e inferiore, non gli altri due.
	{
		CellStyle cs;
		doc->GetCellStyle(cell(1, 2), cs);
		cs.fLBorderColor = 1;
		cs.fBBorderColor = 1;
		doc->SetCellStyle(cell(1, 2), cs);
	}

	// Sottolineato (Fase 12) su A2, oltre a font/bordi gia' impostati
	// sopra: CellStyle::fUnderline, un booleano a parte (BFont non ha
	// un attributo sottolineato nativo).
	{
		CellStyle cs;
		doc->GetCellStyle(cell(1, 2), cs);
		cs.fUnderline = true;
		doc->SetCellStyle(cell(1, 2), cs);
	}

	// Testo a capo (Fase 12) su A2 pure: CellStyle::fWrapText, stesso
	// principio del sottolineato -- l'altezza di riga necessaria non
	// viene ricalcolata qui (SaveASCD/LoadASCD non hanno una view/font
	// vivi), solo il campo booleano viaggia.
	{
		CellStyle cs;
		doc->GetCellStyle(cell(1, 2), cs);
		cs.fWrapText = true;
		doc->SetCellStyle(cell(1, 2), cs);
	}

	// Celle unite (Fase 12): un intervallo D1:E2, indipendente dalle
	// preferenze per-cella sopra (CContainer::AddMergedRange, un
	// elenco di rettangoli per foglio, non un campo per cella).
	doc->AddMergedRange(range(4, 1, 5, 2)); // D1:E2

	// Immagini incorporate (Fase 12): vivono fuori da "doc" (come i
	// grafici, non un campo di CContainer -- vedi EmbeddedImage.h),
	// quindi viaggiano come parametro a parte di SaveASCD/LoadASCD,
	// stesso principio di "charts". Blob PNG minimo (non un vero PNG
	// valido: la persistenza non lo decodifica, solo lo trasporta a
	// byte).
	std::vector<EmbeddedImage> images;
	{
		EmbeddedImage img;
		img.anchor = cell(2, 2); // B2
		img.offsetX = 10; img.offsetY = 5;
		img.width = 40; img.height = 30;
		img.pngData.push_back(0x89);
		img.pngData.push_back('P');
		img.pngData.push_back('N');
		img.pngData.push_back('G');
		images.push_back(img);
	}

	// Formato numero (Fase 12) su A3, oltre all'allineamento gia'
	// impostato sopra: stesso principio dello slittamento
	// "menu Formato -> CellStyle::fFormat" gia' usato dall'app live
	// (MainWindow::SetCellFormat), non un CFormatter costruito da un
	// template come nell'import XLSX -- entrambi i percorsi finiscono
	// comunque nello stesso campo.
	{
		CellStyle cs;
		doc->GetCellStyle(cell(1, 3), cs);
		cs.fFormat = eCurrency;
		doc->SetCellStyle(cell(1, 3), cs);
	}

	{
		BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCD(doc, &file, NULL, NULL, &rowHeights, &frozenRows, &frozenCols,
			&images, &showGrid, &hasTabColor, &tabColor);
		Check(err == B_OK, "SaveASCD con altezze di riga e Blocca riquadri riesce");
	}
	doc->Release();

	CContainer* reloaded = new CContainer(NULL, NULL);
	std::vector<std::pair<int, float> > loadedHeights;
	int loadedFrozenRows = -1, loadedFrozenCols = -1;
	std::vector<EmbeddedImage> loadedImages;
	bool loadedShowGrid = true; // opposto del valore scritto, per essere sicuri che sia DAVVERO riletto
	bool loadedHasTabColor = false;
	rgb_color loadedTabColor = { 0, 0, 0, 255 };

	{
		BFile file(path, B_READ_ONLY);
		status_t err = LoadASCD(&file, reloaded, NULL, NULL,
			&loadedHeights, &loadedFrozenRows, &loadedFrozenCols, &loadedImages, &loadedShowGrid,
			&loadedHasTabColor, &loadedTabColor);
		Check(err == B_OK, "LoadASCD dallo stesso file riesce");
	}

	Check(loadedFrozenRows == 2 && loadedFrozenCols == 1,
		"Blocca riquadri (2 righe, 1 colonna) sopravvive al giro di salvataggio/ricarica");

	Check(!loadedShowGrid, "la griglia nascosta (showGrid=false) sopravvive al giro di salvataggio/ricarica");

	Check(loadedHasTabColor && loadedTabColor.red == 0 && loadedTabColor.green == 176
			&& loadedTabColor.blue == 80,
		"il colore della linguetta del foglio sopravvive al giro di salvataggio/ricarica");

	Check(loadedHeights.size() == 2, "entrambe le altezze di riga personalizzate sopravvivono");
	bool foundRow1 = false, foundRow3 = false;
	for (size_t i = 0; i < loadedHeights.size(); i++)
	{
		if (loadedHeights[i].first == 1 && loadedHeights[i].second == 40.0f)
			foundRow1 = true;
		if (loadedHeights[i].first == 3 && loadedHeights[i].second == 10.0f)
			foundRow3 = true;
	}
	Check(foundRow1, "l'altezza della riga 1 (40) e' quella corretta dopo il giro");
	Check(foundRow3, "l'altezza della riga 3 (10) e' quella corretta dopo il giro");

	// Font: A2 e' ancora in grassetto (indice DIVERSO da prima --
	// l'indice grezzo non e' portabile, vedi il commento in cima al
	// file -- ma la stessa famiglia/dimensione e "Bold" nello stile).
	{
		CellStyle cs;
		reloaded->GetCellStyle(cell(1, 2), cs);
		font_family family;
		font_style style;
		float size;
		gFontSizeTable.GetFontInfo(cs.fFont, &family, &style, &size);
		Check(strcmp(family, sysFamily) == 0 && strstr(style, "Bold") != NULL && size == 14.0f,
			"il font di A2 (famiglia, Bold, dimensione) sopravvive al giro, "
			"anche se l'indice grezzo cambia");
	}

	// A1 (mai toccata) resta col font predefinito: la sezione non
	// scrive/sovrascrive celle che non l'avevano mai avuto.
	{
		CellStyle cs;
		reloaded->GetCellStyle(cell(1, 1), cs);
		CellStyle defaultStyle;
		Check(cs.fFont == defaultStyle.fFont,
			"A1 (mai messa in grassetto) resta col font predefinito dopo il giro");
	}

	// Allineamento: A3 e' ancora allineata a destra, A1/A2 restano
	// generiche (mai toccate).
	{
		CellStyle cs;
		reloaded->GetCellStyle(cell(1, 3), cs);
		Check(cs.fAlignment == eAlignRight, "l'allineamento a destra di A3 sopravvive al giro");

		reloaded->GetCellStyle(cell(1, 1), cs);
		Check(cs.fAlignment == eAlignGeneral,
			"A1 (mai allineata) resta con l'allineamento generico dopo il giro");
	}

	// Bordi: A2 ha ancora sinistro e inferiore, non gli altri due; A1
	// (mai toccata) resta senza nessun bordo.
	{
		CellStyle cs;
		reloaded->GetCellStyle(cell(1, 2), cs);
		Check(cs.fLBorderColor != 0 && cs.fBBorderColor != 0
			&& cs.fTBorderColor == 0 && cs.fRBorderColor == 0,
			"i bordi sinistro e inferiore di A2 sopravvivono al giro, gli altri due restano assenti");

		reloaded->GetCellStyle(cell(1, 1), cs);
		Check(cs.fTBorderColor == 0 && cs.fLBorderColor == 0
			&& cs.fBBorderColor == 0 && cs.fRBorderColor == 0,
			"A1 (mai toccata) resta senza nessun bordo dopo il giro");
	}

	// Sottolineato: A2 e' ancora sottolineata, A1 (mai toccata) no.
	{
		CellStyle cs;
		reloaded->GetCellStyle(cell(1, 2), cs);
		Check(cs.fUnderline, "il sottolineato di A2 sopravvive al giro");

		reloaded->GetCellStyle(cell(1, 1), cs);
		Check(!cs.fUnderline, "A1 (mai sottolineata) resta senza sottolineato dopo il giro");
	}

	// Testo a capo: A2 ce l'ha ancora, A1 (mai toccata) no.
	{
		CellStyle cs;
		reloaded->GetCellStyle(cell(1, 2), cs);
		Check(cs.fWrapText, "il testo a capo di A2 sopravvive al giro");

		reloaded->GetCellStyle(cell(1, 1), cs);
		Check(!cs.fWrapText, "A1 (mai impostata a capo) resta senza testo a capo dopo il giro");
	}

	// Celle unite: l'intervallo D1:E2 sopravvive al giro.
	{
		const std::vector<range>& merged = reloaded->GetMergedRanges();
		Check(merged.size() == 1, "un solo intervallo unito sopravvive al giro");
		bool found = merged.size() == 1 && merged[0].left == 4 && merged[0].top == 1
			&& merged[0].right == 5 && merged[0].bottom == 2;
		Check(found, "l'intervallo unito D1:E2 e' quello corretto dopo il giro");
	}

	// Immagini incorporate: l'ancoraggio, lo scarto/dimensione e il
	// blob PNG sopravvivono tutti al giro.
	{
		Check(loadedImages.size() == 1, "una sola immagine incorporata sopravvive al giro");
		if (loadedImages.size() == 1)
		{
			const EmbeddedImage& img = loadedImages[0];
			Check(img.anchor.h == 2 && img.anchor.v == 2,
				"l'immagine resta ancorata a B2 dopo il giro");
			Check(img.offsetX == 10 && img.offsetY == 5 && img.width == 40 && img.height == 30,
				"scarto e dimensione dell'immagine sopravvivono al giro");
			Check(img.pngData.size() == 4 && img.pngData[0] == 0x89 && img.pngData[1] == 'P'
				&& img.pngData[2] == 'N' && img.pngData[3] == 'G',
				"il blob PNG incorporato sopravvive byte per byte al giro");
		}
	}

	// Formato numero: A3 e' ancora Valuta, A1 (mai toccata) resta al
	// formato predefinito.
	{
		CellStyle cs;
		reloaded->GetCellStyle(cell(1, 3), cs);
		Check(cs.fFormat == eCurrency, "il formato Valuta di A3 sopravvive al giro");

		CellStyle defaultStyle;
		reloaded->GetCellStyle(cell(1, 1), cs);
		Check(cs.fFormat == defaultStyle.fFormat,
			"A1 (mai formattata) resta col formato predefinito dopo il giro");
	}

	// Un file scritto SENZA queste sezioni (chiamante che passa NULL,
	// come tutte le chiamate a SaveASCD/LoadASCD esistenti prima di
	// questa fase) resta leggibile: nessuna sezione, nessun errore,
	// il chiamante che chiede i nuovi campi riceve semplicemente i
	// valori "nessun blocco/nessuna riga personalizzata".
	const char* oldPath = "/tmp/test_persistence_old.ascd";
	CContainer* oldStyleDoc = new CContainer(NULL, NULL);
	TryToParseString("5", cell(1, 1), oldStyleDoc, true);
	{
		BFile file(oldPath, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		SaveASCD(oldStyleDoc, &file); // nessuna delle sezioni nuove
	}
	oldStyleDoc->Release();

	CContainer* oldStyleReloaded = new CContainer(NULL, NULL);
	int oldFrozenRows = -1, oldFrozenCols = -1;
	std::vector<std::pair<int, float> > oldHeights;
	bool oldShowGrid = false; // opposto del default (true): per essere sicuri che il ripiego scatti davvero
	bool oldHasTabColor = true; // opposto del default (false): stesso motivo
	{
		BFile file(oldPath, B_READ_ONLY);
		status_t err = LoadASCD(&file, oldStyleReloaded, NULL, NULL,
			&oldHeights, &oldFrozenRows, &oldFrozenCols, NULL, &oldShowGrid, &oldHasTabColor);
		Check(err == B_OK, "un file senza le nuove sezioni resta leggibile");
	}
	Check(oldFrozenRows == 0 && oldFrozenCols == 0 && oldHeights.empty(),
		"e chi chiede i nuovi campi riceve i valori predefiniti (nessun blocco/altezza)");
	Check(oldShowGrid, "un file senza la sezione griglia riceve il default (griglia visibile)");
	Check(!oldHasTabColor, "un file senza la sezione colore linguetta riceve il default (nessun colore)");

	doc = NULL; // gia' rilasciato sopra
	reloaded->Release();
	oldStyleReloaded->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
