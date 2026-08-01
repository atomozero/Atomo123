/*
	test_persistence.cpp

	Verifica la persistenza nel formato nativo delle quattro preferenze
	rimaste "solo per la sessione corrente" dopo la Fase 7 (Blocca
	riquadri, altezza di riga, font di cella -- grassetto/corsivo -- e
	allineamento, Fase 10) piu' i bordi di cella (Fase 11) -- tutti
	salvati/ricaricati tramite sezioni opzionali in coda al formato
	ASCD, stesso principio gia' usato per larghezza di colonna e
	colori (vedi il commento in AscdIO.h). Il font e' il caso piu'
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

	{
		BFile file(path, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCD(doc, &file, NULL, NULL, &rowHeights, &frozenRows, &frozenCols);
		Check(err == B_OK, "SaveASCD con altezze di riga e Blocca riquadri riesce");
	}
	doc->Release();

	CContainer* reloaded = new CContainer(NULL, NULL);
	std::vector<std::pair<int, float> > loadedHeights;
	int loadedFrozenRows = -1, loadedFrozenCols = -1;

	{
		BFile file(path, B_READ_ONLY);
		status_t err = LoadASCD(&file, reloaded, NULL, NULL,
			&loadedHeights, &loadedFrozenRows, &loadedFrozenCols);
		Check(err == B_OK, "LoadASCD dallo stesso file riesce");
	}

	Check(loadedFrozenRows == 2 && loadedFrozenCols == 1,
		"Blocca riquadri (2 righe, 1 colonna) sopravvive al giro di salvataggio/ricarica");

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
	{
		BFile file(oldPath, B_READ_ONLY);
		status_t err = LoadASCD(&file, oldStyleReloaded, NULL, NULL,
			&oldHeights, &oldFrozenRows, &oldFrozenCols);
		Check(err == B_OK, "un file senza le nuove sezioni resta leggibile");
	}
	Check(oldFrozenRows == 0 && oldFrozenCols == 0 && oldHeights.empty(),
		"e chi chiede i nuovi campi riceve i valori predefiniti (nessun blocco/altezza)");

	doc = NULL; // gia' rilasciato sopra
	reloaded->Release();
	oldStyleReloaded->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
