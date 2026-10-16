/*
	test_activex_controls_import.cpp

	Verifica l'import reale dei controlli ActiveX (Fase "ActiveX
	controls", EmbeddedControl.h/XlsxTranslator.cpp) passando DAVVERO
	per il translator XLSX installato (BTranslatorRoster, stesso
	percorso di MainWindow::OpenFile) -- stesso principio di
	test_translator_multisheet_import.cpp, qui sul parsing di
	<controls>/activeX invece che sul giro fogli multipli.

	tests/sample_activex_controls.xlsm e' il vero file usato per
	progettare/verificare questa funzionalita' (github.com/rndtrash/
	chip-8-vba, licenza WTFPL -- vedi LICENSE in quel repository):
	un emulatore CHIP-8 con 20 pulsanti Forms 2.0 CommandButton reali
	(tastierino esadecimale cb0-cbF piu' 4 comandi). Usato qui come
	l'unico vero banco di prova end-to-end con 20 controlli, lo stesso
	file che ha guidato tutta la progettazione di questa fase.

	Scoperta reale durante la stesura di questo test: la prima lettura
	della struttura OOXML aveva scambiato i due r:id -- e' quello
	dell'elemento <control> ESTERNO che risolve verso xl/activeX/
	activeXN.xml, NON quello di <controlPr> (che risolve invece verso
	l'immagine VML legacy). Verificato qui su tutti e 20 i controlli
	reali, non solo ipotizzato: senza questo test, l'euristica di
	didascalia e il classid sarebbero rimasti silenziosamente vuoti per
	OGNI controllo di un file reale (il codice tratta "nessun classid
	risolto" come "tipo non supportato", mai un errore, quindi il bug
	non si sarebbe mai manifestato con un messaggio d'errore).
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
	BApplication app("application/x-vnd.Atomo-TestActiveXControlsImport");

	BFile srcFile("tests/sample_activex_controls.xlsm", B_READ_ONLY);
	Check(srcFile.InitCheck() == B_OK, "apertura di tests/sample_activex_controls.xlsm riuscita");

	BMallocIO ascd;
	BMessage ext;
	status_t translateErr = BTranslatorRoster::Default()->Translate(&srcFile, NULL, &ext, &ascd,
		kAtomoNativeFormat);
	Check(translateErr == B_OK, "Translate() del file XLSM con controlli ActiveX riesce");

	ascd.Seek(0, SEEK_SET);
	bool isBook = IsASCDBookFile(&ascd);

	std::vector<AscdSheet> sheets;
	status_t loadErr = isBook ? LoadASCDBook(&ascd, &sheets) : B_BAD_DATA;
	Check(loadErr == B_OK, "LoadASCDBook legge l'intera cartella tradotta senza errori");
	Check(sheets.size() == 2, "il file reale ha due fogli (il secondo senza nessun controllo)");

	if (sheets.size() < 1)
	{
		printf("\nALCUNI TEST SONO FALLITI (impossibile continuare senza il primo foglio)\n");
		return 1;
	}

	const std::vector<EmbeddedControl>& controls = sheets[0].controls;
	Check(controls.size() == 20,
		"tutti e 20 i controlli ActiveX del primo foglio sono stati importati");

	const char* kCommandButtonClassid = "{D7053240-CE69-11CD-A777-00DD01143C57}";

	// cb0-cbF (tastierino esadecimale) piu' i 4 comandi: ogni nome deve
	// essere presente, con lo stesso classid Forms 2.0 CommandButton --
	// il bug reale descritto in cima al file lasciava classid vuoto per
	// tutti, anche quando nome/ancoraggio erano comunque importati.
	static const char* kExpectedNames[] = {
		"cbClearScreen", "CommandButton1", "CommandButton2", "cbStep",
		"cb0", "cb1", "cb2", "cb3", "cb4", "cb5", "cb6", "cb7",
		"cb8", "cb9", "cbA", "cbB", "cbC", "cbD", "cbE", "cbF"
	};
	for (size_t i = 0; i < sizeof(kExpectedNames) / sizeof(kExpectedNames[0]); i++)
	{
		const EmbeddedControl* ctrl = FindControl(controls, kExpectedNames[i]);
		char what[256];
		snprintf(what, sizeof(what), "il controllo \"%s\" e' stato importato", kExpectedNames[i]);
		Check(ctrl != NULL, what);
		if (ctrl)
		{
			snprintf(what, sizeof(what),
				"\"%s\" ha il classid Forms 2.0 CommandButton risolto (non vuoto)", kExpectedNames[i]);
			Check(ctrl->classid == kCommandButtonClassid, what);
		}
	}

	// Un controllo reale e noto: cb0 (tasto "0" del tastierino) deve
	// avere un ancoraggio valido e una dimensione positiva -- la stessa
	// matematica EMU/SumColumnWidths gia' usata dai grafici a due celle.
	const EmbeddedControl* cb0 = FindControl(controls, "cb0");
	if (cb0)
	{
		Check(cb0->anchor.IsValid(), "cb0 ha un ancoraggio di cella valido");
		Check(cb0->width > 0 && cb0->height > 0,
			"cb0 ha una larghezza/altezza positiva (calcolata dall'ancoraggio a due celle <to>)");
	}

	// L'euristica di didascalia (ScanActiveXCaptionHeuristic) trova
	// davvero il carattere giusto per i 16 tasti del tastierino
	// esadecimale: non solo "non vuota", ma il VERO testo del vero
	// file -- verificato a mano confrontando con il contenuto reale
	// di ogni activeXN.bin (bug reale trovato e corretto scrivendo
	// questo test: il percorso del .bin era costruito male, "xl/" +
	// target invece di "<cartella di activeXN.xml>/" + target, quindi
	// ReadEntry falliva sempre e la didascalia ricadeva SEMPRE sul
	// nome, per ogni controllo di ogni file reale, senza nessun errore
	// visibile).
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
		snprintf(what, sizeof(what), "\"%s\" ha la didascalia reale \"%s\" (non il ripiego sul nome)",
			kKeypadNames[i], kKeypadCaptions[i]);
		Check(ctrl && ctrl->caption == kKeypadCaptions[i], what);
	}

	// I 4 comandi con una didascalia piu' lunga (testo cirillico
	// multi-carattere) NON sono estratti da questa euristica
	// (limitazione dichiarata, non conformita' piena a [MS-OFORMS]):
	// verifica solo che il ripiego sia sicuro (il nome stesso, mai una
	// stringa vuota o corrotta), non che l'euristica li estragga.
	static const char* kLongCaptionNames[] = {
		"cbClearScreen", "CommandButton1", "CommandButton2", "cbStep"
	};
	for (size_t i = 0; i < sizeof(kLongCaptionNames) / sizeof(kLongCaptionNames[0]); i++)
	{
		const EmbeddedControl* ctrl = FindControl(controls, kLongCaptionNames[i]);
		char what[256];
		snprintf(what, sizeof(what),
			"\"%s\" (didascalia lunga, fuori dall'euristica) ricade sul proprio nome, non su una stringa vuota/corrotta",
			kLongCaptionNames[i]);
		Check(ctrl && ctrl->caption == kLongCaptionNames[i], what);
	}

	for (size_t i = 0; i < sheets.size(); i++)
		sheets[i].doc->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
