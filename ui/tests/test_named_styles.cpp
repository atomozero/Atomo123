/*
	test_named_styles.cpp

	Verifica "Stili cella con nome + tavolozza tema dal vivo" (Tier 4,
	"Path to full Excel parity"). Fasi A+B di questa funzionalita' (vedi
	ROADMAP.md): il nucleo motore -- NamedStyleTable/ThemePalette/
	NamedStyleDef::Resolve e la risoluzione DAL VIVO dentro
	CContainer::GetCellStyle -- PIU' la persistenza .ascd (Fase B).
	Nessuna MainWindow/UI coinvolta ancora (arrivera' nella Fase C).

	Punto piu' importante delle Parti 1/2: il claim "vivo" stesso,
	provato esplicitamente, non solo assunto -- ridefinire uno stile o
	cambiare un colore della tavolozza tema deve cambiare cio' che
	CContainer::GetCellStyle restituisce per una cella GIA' scritta,
	SENZA mai riscriverla (la cella viene scritta UNA sola volta, poi
	riletta piu' volte dopo ogni ridefinizione).

	Punto piu' importante della Parte 3 (persistenza): la stabilita'
	degli ID attraverso un giro salva->ricarica anche quando uno stile
	e' stato rimosso a meta' sessione (tombstone, vedi RawCount/
	ReplaceAllRaw in NamedStyle.h) -- un bug reale individuato in fase
	di progettazione, prima di scrivere la sezione .ascd.
*/

#include <cstdio>

#include <Application.h>
#include <File.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "Container.h"
#include "NamedStyle.h"

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

static bool ColorsEqual(rgb_color a, rgb_color b)
{
	return a.red == b.red && a.green == b.green && a.blue == b.blue && a.alpha == b.alpha;
}

// Test-double minimo di ISheetResolver (Container.h): solo i due nuovi
// metodi contano qui, ResolveSheetByName/FindSheetWithTable non sono
// mai chiamati da questo file (nessun riferimento incrociato fra
// fogli), ma vanno comunque implementati perche' sono puri virtuali.
class TestResolver : public ISheetResolver {
public:
	NamedStyleTable styles;
	ThemePalette theme;

	CContainer* ResolveSheetByName(const char*) override { return NULL; }
	CContainer* FindSheetWithTable(const std::string&) override { return NULL; }
	const NamedStyleTable* GetNamedStyleTable() const override { return &styles; }
	const ThemePalette* GetThemePalette() const override { return &theme; }
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestNamedStyles");

	// --- Parte 1: NamedStyleTable pura, nessun CContainer -----------------

	{
		NamedStyleTable table;
		Check(table.Count() == 17, "ResetToBuiltIns popola i 17 stili built-in previsti");
		int goodID = table.FindByName("Buono");
		Check(goodID > 0, "FindByName trova uno stile built-in per nome esatto");
		Check(table.FindByName("Non Esiste") == -1, "FindByName ritorna -1 per un nome sconosciuto");
		Check(table.Get(goodID) != NULL, "Get su un ID valido non ritorna NULL");
		Check(table.Get(goodID)->builtIn, "uno stile built-in ha builtIn=true");
		Check(table.Get(0) == NULL, "Get(0) (\"nessuno stile\") ritorna sempre NULL");
		Check(table.Get(9999) == NULL, "Get su un ID fuori range ritorna NULL, non un crash");

		NamedStyleDef custom;
		custom.name = "Il mio stile";
		int customID = table.AddCustom(custom);
		Check(customID == 18, "AddCustom aggiunge in fondo (ID 18, dopo i 17 built-in)");
		Check(table.Count() == 18, "Count riflette il nuovo stile personalizzato");
		Check(!table.Get(customID)->builtIn, "uno stile aggiunto con AddCustom ha builtIn=false");

		Check(!table.Remove(goodID), "Remove su uno stile built-in fallisce (mai eliminabile)");
		Check(table.Get(goodID) != NULL, "uno stile built-in resta presente dopo un Remove rifiutato");
		Check(table.Remove(customID), "Remove su uno stile personalizzato riesce");
		Check(table.Get(customID) == NULL, "Get su uno stile rimosso ritorna NULL");
		Check(table.Count() == 17, "Count torna a 17 dopo la rimozione (il tombstone non conta)");
		Check(!table.Remove(customID), "un secondo Remove sullo stesso ID (gia' rimosso) fallisce");

		// L'ID resta STABILE anche dopo una rimozione (mai un vero
		// erase che sposterebbe gli indici successivi) -- verificato
		// aggiungendone un altro e controllando che l'ID nuovo sia 19,
		// non 18 (il tombstone di prima).
		int secondCustomID = table.AddCustom(NamedStyleDef());
		Check(secondCustomID == 19, "un ID rimosso non viene mai riassegnato (niente riuso)");

		Check(table.IDAtIndex(0) == 1, "IDAtIndex(0) e' il primo stile definito e non rimosso");
		bool foundRemovedInEnumeration = false;
		for (int i = 0; i < table.Count(); i++)
			if (table.IDAtIndex(i) == customID)
				foundRemovedInEnumeration = true;
		Check(!foundRemovedInEnumeration, "l'enumerazione IDAtIndex non include mai un ID rimosso");
	}

	// --- Parte 2: risoluzione dal vivo tramite CContainer::GetCellStyle ---

	{
		CContainer* doc = new CContainer(NULL, NULL);
		TestResolver resolver;
		doc->SetSheetResolver(&resolver);

		// Stile personalizzato: sfondo legato al ruolo tema Accent1
		// (segue il tema), testo letterale fisso (non segue il tema).
		NamedStyleDef def;
		def.name = "Prova";
		def.useThemeBackground = true;
		def.backgroundRole = eThemeAccent1;
		def.useThemeText = false;
		def.textColor = rgb_color{ 10, 10, 10, 255 };
		def.bold = true;
		int styleID = resolver.styles.AddCustom(def);

		// La cella viene scritta UNA sola volta: ogni verifica "dal vivo"
		// sotto rilegge la STESSA cella dopo aver toccato solo lo stile/
		// il tema, mai la cella stessa di nuovo.
		cell c(1, 1);
		CellStyle cs; // stile di default fresco
		cs.fNamedStyleID = styleID;
		doc->SetCellStyle(c, cs);

		CellStyle resolved;
		doc->GetCellStyle(c, resolved);
		Check(ColorsEqual(resolved.fLowColor, resolver.theme.colors[eThemeAccent1]),
			"lo sfondo risolto e' il colore CORRENTE del ruolo tema Accent1");
		Check(ColorsEqual(resolved.fHighColor, rgb_color{ 10, 10, 10, 255 }),
			"il testo risolto e' il colore letterale dello stile (non legato al tema)");

		// --- Il claim "vivo", scenario 1: swap del tema. ---
		rgb_color newAccent1 = { 200, 30, 30, 255 };
		resolver.theme.colors[eThemeAccent1] = newAccent1;
		CellStyle afterThemeSwap;
		doc->GetCellStyle(c, afterThemeSwap); // stessa cella, MAI riscritta sopra
		Check(ColorsEqual(afterThemeSwap.fLowColor, newAccent1),
			"DAL VIVO: cambiare il colore del ruolo tema cambia lo sfondo risolto SENZA riscrivere la cella");

		// --- Il claim "vivo", scenario 2: ridefinizione dello stile. ---
		NamedStyleDef redefined = def;
		redefined.useThemeBackground = false;
		redefined.backgroundColor = rgb_color{ 5, 5, 5, 255 };
		Check(resolver.styles.Redefine(styleID, redefined), "Redefine su uno stile esistente riesce");
		CellStyle afterRedefine;
		doc->GetCellStyle(c, afterRedefine); // stessa cella, ancora mai riscritta
		Check(ColorsEqual(afterRedefine.fLowColor, rgb_color{ 5, 5, 5, 255 }),
			"DAL VIVO: ridefinire lo stile cambia lo sfondo risolto SENZA riscrivere la cella");
		Check(resolver.styles.Get(styleID)->name == "Prova",
			"Redefine non cambia il nome dello stile (identita' preservata)");
		Check(resolver.styles.Get(styleID)->builtIn == false,
			"Redefine non cambia il flag builtIn dello stile");

		// --- Rimozione: la cella ricade sull'aspetto letterale, senza crash. ---
		resolver.styles.Remove(styleID);
		CellStyle afterRemove;
		doc->GetCellStyle(c, afterRemove);
		Check(afterRemove.fNamedStyleID == styleID,
			"fNamedStyleID memorizzato nella cella resta invariato anche dopo la rimozione dello stile");
		CellStyle freshDefault;
		Check(ColorsEqual(afterRemove.fLowColor, freshDefault.fLowColor),
			"uno stile rimosso fa ricadere sull'aspetto letterale gia' scritto, non un crash ne' un errore");

		// --- Nessun resolver collegato: stesso comportamento di prima di questa fase. ---
		CContainer* standalone = new CContainer(NULL, NULL);
		cell c2(1, 1);
		CellStyle cs2;
		cs2.fNamedStyleID = 5; // un ID qualunque, nessun resolver per interpretarlo
		standalone->SetCellStyle(c2, cs2);
		CellStyle resolvedStandalone;
		standalone->GetCellStyle(c2, resolvedStandalone);
		Check(ColorsEqual(resolvedStandalone.fLowColor, cs2.fLowColor),
			"senza un resolver collegato (CContainer autonomo, come ogni test/esempio esistente), "
			"fNamedStyleID non impostato a 0 non causa un crash: ricade sul letterale");
		standalone->Release();

		doc->Release();
	}

	// --- Parte 3: round-trip AscdIO (Fase B) -------------------------------

	{
		CContainer& saveDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), &saveDoc, true);

		NamedStyleTable savedStyles;
		// Uno stile personalizzato legato a un ruolo tema.
		NamedStyleDef def;
		def.name = "Prova";
		def.useThemeBackground = true;
		def.backgroundRole = eThemeAccent2;
		def.bold = true;
		int customID = savedStyles.AddCustom(def);
		// Un secondo, poi rimosso -- il tombstone deve sopravvivere al
		// giro (vedi il commento su RawCount/ReplaceAllRaw in
		// NamedStyle.h): senza, l'ID del terzo stile sotto si
		// sposterebbe dopo il giro salva->ricarica.
		int removedID = savedStyles.AddCustom(NamedStyleDef());
		savedStyles.Remove(removedID);
		int thirdID = savedStyles.AddCustom(NamedStyleDef());

		ThemePalette savedTheme;
		savedTheme.colors[eThemeAccent2] = rgb_color{ 77, 88, 99, 255 };

		CellStyle cs;
		cs.fNamedStyleID = customID;
		saveDoc.SetCellStyle(cell(2, 2), cs);

		BFile styleFile("tests/roundtrip_named_styles.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		status_t err = SaveASCD(&saveDoc, &styleFile,
			NULL, // charts
			NULL, // colWidths
			NULL, // rowHeights
			NULL, // frozenRows
			NULL, // frozenCols
			NULL, // images
			NULL, // showGrid
			NULL, // hasTabColor
			NULL, // tabColor
			NULL, // hiddenRows
			NULL, // hasAutoFilter
			NULL, // autoFilterRange
			NULL, // hasPrintArea
			NULL, // printArea
			NULL, // printSettings
			NULL, // vbaProject
			NULL, // isProtected
			NULL, // protection
			NULL, // filterHiddenValues
			NULL, // slicers
			NULL, // scenarios
			&savedStyles, // namedStyles
			&savedTheme); // themePalette
		Check(err == B_OK, "SaveASCD con stili con nome + tema riesce");
		saveDoc.Release();

		BFile styleReopened("tests/roundtrip_named_styles.ascd", B_READ_ONLY);
		CContainer& loadDoc = *new CContainer(NULL, NULL);
		NamedStyleTable loadedStyles;
		ThemePalette loadedTheme;
		bool hasStyles = false, hasTheme = false;
		err = LoadASCD(&styleReopened, &loadDoc,
			NULL, // charts
			NULL, // colWidths
			NULL, // rowHeights
			NULL, // frozenRows
			NULL, // frozenCols
			NULL, // images
			NULL, // showGrid
			NULL, // hasTabColor
			NULL, // tabColor
			NULL, // hiddenRows
			NULL, // hasAutoFilter
			NULL, // autoFilterRange
			NULL, // hasPrintArea
			NULL, // printArea
			NULL, // printSettings
			false, // skipInitialRecalc
			NULL, // vbaProject
			NULL, // isProtected
			false, // skipVbaAndProtectionSections
			NULL, // protection
			NULL, // filterHiddenValues
			NULL, // slicers
			NULL, // scenarios
			&hasStyles, &loadedStyles, // hasNamedStyles, namedStyles
			&hasTheme, &loadedTheme); // hasThemePalette, themePalette
		Check(err == B_OK, "LoadASCD con stili con nome + tema riesce");
		Check(hasStyles, "hasNamedStyles diventa true quando il file porta davvero questa sezione");
		Check(hasTheme, "hasThemePalette diventa true quando il file porta davvero questa sezione");

		Check(loadedStyles.Get(customID) != NULL, "lo stile personalizzato sopravvive con lo stesso ID");
		if (loadedStyles.Get(customID) != NULL)
		{
			Check(loadedStyles.Get(customID)->name == "Prova", "il nome sopravvive");
			Check(loadedStyles.Get(customID)->useThemeBackground, "useThemeBackground sopravvive");
			Check(loadedStyles.Get(customID)->backgroundRole == eThemeAccent2, "backgroundRole sopravvive");
			Check(loadedStyles.Get(customID)->bold, "bold sopravvive");
		}
		Check(loadedStyles.Get(removedID) == NULL,
			"lo stile rimosso resta rimosso dopo il giro (tombstone preservato)");
		Check(loadedStyles.Get(thirdID) != NULL && loadedStyles.Get(thirdID) == &loadedStyles.RawDefAt(thirdID - 1),
			"l'ID del terzo stile NON si sposta dopo il giro nonostante il tombstone in mezzo");
		Check(ColorsEqual(loadedTheme.colors[eThemeAccent2], rgb_color{ 77, 88, 99, 255 }),
			"il colore del tema modificato sopravvive al giro");

		// Collega un resolver (come farebbe MainWindow::AttachSheetResolver)
		// perche' GetCellStyle possa risolvere fNamedStyleID: senza,
		// loadedStyles/loadedTheme appena ricaricati non sarebbero mai
		// consultati, stesso comportamento di "nessun resolver ancora"
		// gia' verificato nella Parte 2.
		TestResolver loadResolver;
		loadResolver.styles = loadedStyles;
		loadResolver.theme = loadedTheme;
		loadDoc.SetSheetResolver(&loadResolver);

		CellStyle loadedCellStyle;
		loadDoc.GetCellStyle(cell(2, 2), loadedCellStyle);
		Check(ColorsEqual(loadedCellStyle.fLowColor, rgb_color{ 77, 88, 99, 255 }),
			"DAL VIVO anche dopo il giro salva->ricarica: la cella risolve col colore tema aggiornato");
		loadDoc.Release();

		// --- Compatibilita' con un file scritto PRIMA di questa sezione ---
		//
		// La sezione stili con nome + tema e' l'ULTIMA cosa scritta da
		// SaveASCD (subito dopo la sezione ID di stile per cella, che a
		// sua volta e' subito dopo gli scenari). Per un documento senza
		// nessuna cella con fNamedStyleID e senza namedStyles/
		// themePalette passati, le due sezioni sono: 4 byte (conteggio
		// celle con stile = 0) + 1 byte (hasStyles = 0) + 1 byte
		// (hasTheme = 0) = 6 byte finali. Troncarli equivale a un file
		// scritto da una build PRIMA che questa fase esistesse.
		CContainer& oldDoc = *new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), &oldDoc, true);
		BFile oldFile("tests/roundtrip_named_styles_old.ascd",
			B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		err = SaveASCD(&oldDoc, &oldFile);
		Check(err == B_OK, "SaveASCD di riferimento (nessuno stile/tema) riesce");
		oldDoc.Release();

		off_t size = 0;
		oldFile.GetSize(&size);
		Check(size > 6, "il file di riferimento e' abbastanza grande da poter troncare 6 byte");
		oldFile.SetSize(size - 6);

		BFile oldReopened("tests/roundtrip_named_styles_old.ascd", B_READ_ONLY);
		CContainer& oldReloaded = *new CContainer(NULL, NULL);
		bool oldHasStyles = false, oldHasTheme = false;
		NamedStyleTable oldLoadedStyles;
		ThemePalette oldLoadedTheme;
		err = LoadASCD(&oldReopened, &oldReloaded,
			NULL, // charts
			NULL, // colWidths
			NULL, // rowHeights
			NULL, // frozenRows
			NULL, // frozenCols
			NULL, // images
			NULL, // showGrid
			NULL, // hasTabColor
			NULL, // tabColor
			NULL, // hiddenRows
			NULL, // hasAutoFilter
			NULL, // autoFilterRange
			NULL, // hasPrintArea
			NULL, // printArea
			NULL, // printSettings
			false, // skipInitialRecalc
			NULL, // vbaProject
			NULL, // isProtected
			false, // skipVbaAndProtectionSections
			NULL, // protection
			NULL, // filterHiddenValues
			NULL, // slicers
			NULL, // scenarios
			&oldHasStyles, &oldLoadedStyles, // hasNamedStyles, namedStyles
			&oldHasTheme, &oldLoadedTheme); // hasThemePalette, themePalette
		Check(err == B_OK,
			"un file troncato (che simula una build precedente a questa fase) si carica comunque");
		Check(!oldHasStyles && !oldHasTheme,
			"un file senza questa sezione lascia hasNamedStyles/hasThemePalette false, non un errore");
		Check(oldLoadedStyles.Count() == 17,
			"senza una sezione nel file, la tabella resta ai soli 17 built-in predefiniti");
		oldReloaded.Release();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
