/*
	test_xlsx_named_functions_dynamic.cpp

	Verifica un bug reale, grave, scoperto SOLO aprendo un file XLSX
	reale dall'app vera (segnalato dall'utente con uno screenshot):
	ogni formula con funzione con nome (SUM, IF, XLOOKUP, ecc.)
	importata da XLSX restava testo grezzo invece di calcolata,
	nonostante App::ReadyToRun() chiami InitFunctions().

	Causa: engine/ e' una libreria STATICA (.a). L'eseguibile
	principale e ogni translator .so (XlsxTranslator, caricato a parte
	da MainWindow::OpenFile tramite il vero BTranslatorRoster) la
	collegano ciascuno per conto proprio, ottenendo ciascuno la
	PROPRIA copia indipendente di gFuncCount/gFuncArrayByNr -- mai
	condivisa. App::ReadyToRun() inizializza solo la copia
	dell'eseguibile; il translator aveva sempre la propria copia a
	zero. Corretto con EnsureFunctionsInitialized() (engine/src/
	Functions/FunctionUtils.h/.cpp), chiamata da ogni translator prima
	di analizzare qualunque formula.

	QUESTO e' esattamente il motivo per cui questo test esiste come
	test UI (non nella suite gia' esistente translators/xlsx/tests/
	test_xlsx_translator.cpp): quella suite COMPILA XlsxTranslator.cpp
	DIRETTAMENTE nel proprio eseguibile di prova (vedi il Makefile,
	SOURCES nel link finale), quindi condivide UN'UNICA copia dei
	globali con XlsxTranslator.cpp -- mascherando esattamente questo
	bug, che si manifesta SOLO quando il translator e' un vero add-on
	caricato a parte, come lo e' sempre nell'app reale. Qui si apre il
	file con una vera MainWindow::OpenFile, che usa il vero
	BTranslatorRoster e quindi il vero XlsxTranslator.so installato in
	~/config/non-packaged/add-ons/Translators/ -- l'UNICO modo per
	verificare automaticamente questa intera classe di bug.

	Riusa lo stesso file di prova gia' presente in translators/xlsx/
	tests/sample_formulas.xlsx (fixture ZIP minima scritta a mano con
	uno script Python, nessun Excel/LibreOffice disponibile per
	generarla) invece di duplicarlo.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Entry.h>
#include <Path.h>
#include <Roster.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "SheetView.h"
#include "MainWindow.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "ResourceManager.h"
#include "MyError.h"

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
	BApplication app("application/x-vnd.Atomo-TestXlsxNamedFunctionsDynamic");

	// Stessa identica sequenza di App::ReadyToRun() nella vera app:
	// senza questa, il RICALCOLO del foglio (dopo l'importazione, nel
	// contesto dell'ESEGUIBILE principale, non del translator .so)
	// non trova nessuna funzione con nome nella PROPRIA copia di
	// gFuncCount -- distinto dal bug qui verificato (che riguarda
	// invece l'ANALISI della formula, nel translator).
	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	BPath fixturePath("../translators/xlsx/tests/sample_formulas.xlsx");
	BEntry entry(fixturePath.Path());
	entry_ref ref;
	status_t err = entry.GetRef(&ref);
	Check(err == B_OK, "il file di prova sample_formulas.xlsx esiste");
	if (err != B_OK)
	{
		printf("\nALCUNI TEST SONO FALLITI\n");
		return 1;
	}

	MainWindow* win = new MainWindow();
	win->Lock();
	win->OpenFile(ref);

	CContainer* doc = win->GetSheetView()->Document();
	Check(doc != NULL, "OpenFile (vero BTranslatorRoster) apre il file XLSX senza crashare");

	if (doc)
	{
		// D1=IF(C1<>2,"vero","falso") con C1=2 -> "falso"
		Value d1;
		doc->GetValue(cell(4, 1), d1);
		Check(d1.fType == eTextData && strcmp((const char*)d1, "falso") == 0,
			"D1 (IF con funzione con nome) e' calcolata come \"falso\", non testo grezzo "
			"(bug reale: il translator .so caricato a parte non inizializzava la propria "
			"tabella delle funzioni)");

		// D2=VLOOKUP(2,A1:B3,2,0) -> "due"
		Value d2;
		doc->GetValue(cell(4, 2), d2);
		Check(d2.fType == eTextData && strcmp((const char*)d2, "due") == 0,
			"D2 (VLOOKUP) e' calcolata come \"due\", non testo grezzo");

		// D3=IFERROR(1/0,99) -> 99
		Value d3;
		doc->GetValue(cell(4, 3), d3);
		Check(d3.fType == eNumData && (double)d3 == 99.0,
			"D3 (IFERROR) e' calcolata come 99, non testo grezzo");

		char formula[256];
		doc->GetCellFormula(cell(4, 1), formula, sizeof(formula), false);
		Check(strstr(formula, "IF(") != NULL,
			"la formula di D1 si ridisegna come una vera formula (IF(...)), "
			"non come testo letterale col segno \"=\" davanti");
	}

	win->Unlock();
	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
