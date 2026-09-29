/*
	test_named_styles.cpp

	Verifica "Stili cella con nome + tavolozza tema dal vivo" (Tier 4,
	"Path to full Excel parity"). Fase A di questa funzionalita' (vedi
	ROADMAP.md): solo il nucleo motore -- NamedStyleTable/ThemePalette/
	NamedStyleDef::Resolve e la risoluzione DAL VIVO dentro
	CContainer::GetCellStyle -- nessuna MainWindow/UI coinvolta ancora
	(arriveranno nelle fasi B/C).

	Punto piu' importante di questo file: il claim "vivo" stesso, provato
	esplicitamente, non solo assunto -- ridefinire uno stile o cambiare
	un colore della tavolozza tema deve cambiare cio' che
	CContainer::GetCellStyle restituisce per una cella GIA' scritta,
	SENZA mai riscriverla (vedi gli scenari "dal vivo" sotto: la cella
	viene scritta UNA sola volta in tutto il file, poi riletta piu'
	volte dopo ogni ridefinizione).
*/

#include <cstdio>

#include <Application.h>

#include "Cell.h"
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

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
