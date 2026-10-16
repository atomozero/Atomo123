/*
	test_activex_controls_export.cpp

	Verifica il round-trip COMPLETO di esportazione dei controlli
	ActiveX (Fase "ActiveX controls", Phase 2) passando DAVVERO per il
	translator XLSX installato due volte di seguito:

	  1. XLSX (vero file, 20 controlli reali) -> XLSX (ri-esportazione,
	     WriteXLSX con la nuova scrittura di <controls>/xl/activeX)
	  2. Quello stesso file appena ri-esportato -> ASCD (ri-importazione,
	     ParseWorksheetControls/ScanActiveXCaptionHeuristic sullo STESSO
	     XML/.bin che questo stesso translator ha appena scritto)

	Se i due round-trip sono simmetrici, i 20 controlli del passo 2
	devono combaciare con quelli del vero file originale (stesso test
	usato in test_activex_controls_import.cpp) -- non solo "qualcosa e'
	stato scritto", ma "quello che e' stato scritto si rilegge
	correttamente com'era prima".
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <File.h>
#include <TranslatorRoster.h>

#include "AscdIO.h"
#include "Cell.h"
#include "EmbeddedControl.h"
#include "Container.h"

static const uint32 kAtomoXlsxFormat = 'AXSX';
static const uint32 kAtomoNativeFormat = 'ASCD';

static int gFailures = 0;

static void Check(bool condition, const char* description)
{
	printf("%s   %s\n", condition ? "OK" : "FAIL", description);
	if (!condition)
		gFailures++;
}

static const EmbeddedControl* FindControl(const std::vector<EmbeddedControl>& controls,
	const char* name)
{
	for (size_t i = 0; i < controls.size(); i++)
	{
		if (controls[i].name == name)
			return &controls[i];
	}
	return NULL;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestActiveXControlsExport");

	BFile srcFile("tests/sample_activex_controls.xlsm", B_READ_ONLY);
	Check(srcFile.InitCheck() == B_OK, "apertura di tests/sample_activex_controls.xlsm riuscita");

	// Passo 1: XLSX -> XLSX (ri-esportazione).
	BMallocIO reexported;
	BMessage ext1;
	status_t err1 = BTranslatorRoster::Default()->Translate(&srcFile, NULL, &ext1, &reexported,
		kAtomoXlsxFormat);
	Check(err1 == B_OK, "Translate() XLSX -> XLSX (ri-esportazione) riesce");

	// Passo 2: quel file ri-esportato -> ASCD (ri-importazione).
	reexported.Seek(0, SEEK_SET);
	BMallocIO ascd;
	BMessage ext2;
	status_t err2 = BTranslatorRoster::Default()->Translate(&reexported, NULL, &ext2, &ascd,
		kAtomoNativeFormat);
	Check(err2 == B_OK, "Translate() del file ri-esportato -> ASCD (ri-importazione) riesce");

	ascd.Seek(0, SEEK_SET);
	bool isBook = IsASCDBookFile(&ascd);
	std::vector<AscdSheet> sheets;
	status_t loadErr = isBook ? LoadASCDBook(&ascd, &sheets) : B_BAD_DATA;
	Check(loadErr == B_OK, "LoadASCDBook legge il giro completo senza errori");

	if (sheets.empty())
	{
		printf("\nALCUNI TEST SONO FALLITI (impossibile continuare senza il primo foglio)\n");
		return 1;
	}

	const std::vector<EmbeddedControl>& controls = sheets[0].controls;
	Check(controls.size() == 20,
		"tutti e 20 i controlli ActiveX sopravvivono al giro XLSX->XLSX->ASCD completo");

	const char* kCommandButtonClassid = "{D7053240-CE69-11CD-A777-00DD01143C57}";

	static const char* kKeypadNames[] = {
		"cb0", "cb1", "cb2", "cb3", "cb4", "cb5", "cb6", "cb7",
		"cb8", "cb9", "cbA", "cbB", "cbC", "cbD", "cbE", "cbF"
	};
	static const char* kKeypadCaptions[] = {
		"0", "1", "2", "3", "4", "5", "6", "7",
		"8", "9", "A", "B", "C", "D", "E", "F"
	};
	for (size_t i = 0; i < sizeof(kKeypadNames) / sizeof(kKeypadNames[0]); i++)
	{
		const EmbeddedControl* ctrl = FindControl(controls, kKeypadNames[i]);
		char what[256];
		snprintf(what, sizeof(what), "\"%s\" sopravvive con classid CommandButton intatto",
			kKeypadNames[i]);
		Check(ctrl && ctrl->classid == kCommandButtonClassid, what);
		snprintf(what, sizeof(what), "\"%s\" sopravvive con la didascalia reale \"%s\" intatta",
			kKeypadNames[i], kKeypadCaptions[i]);
		Check(ctrl && ctrl->caption == kKeypadCaptions[i], what);
	}

	// Geometria: cb0 deve ancora avere un ancoraggio valido e una
	// dimensione positiva dopo il giro completo (conversione pixel ->
	// EMU -> pixel, PixelToColOffset/PixelToRowOffset nell'esportazione
	// seguita dalla stessa matematica SumColumnWidths/SumRowHeights
	// dell'importazione) -- non deve mai collassare a zero/negativo.
	const EmbeddedControl* cb0 = FindControl(controls, "cb0");
	if (cb0)
	{
		Check(cb0->anchor.IsValid(), "cb0 ha ancora un ancoraggio di cella valido dopo il giro completo");
		Check(cb0->width > 0 && cb0->height > 0,
			"cb0 ha ancora una larghezza/altezza positiva dopo il giro completo");
	}

	for (size_t i = 0; i < sheets.size(); i++)
		sheets[i].doc->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
