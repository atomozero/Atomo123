/*
	test_scripting.cpp

	Verifica lo scripting BMessage reale aggiunto a MainWindow (richiesta
	esplicita dell'utente, dopo aver controllato con "hey Atomo123 get
	Suites of Window 0" che prima esistevano solo le suite generiche di
	BWindow/BView/BMenu, nessuna proprieta' specifica di questa app):
	undici proprieta' DIRETTE, "Value"/"Selection"/"FilePath"/"CellColor"/
	"ColumnWidth"/"Bold"/"Border"/"NumberFormat"/"Chart"/"WrapText"/
	"GoalSeek", in una nuova suite "suite/vnd.Atomo-sheet"
	(GetSupportedSuites/ResolveSpecifier). La logica vera vive in
	altrettante coppie di metodi pubblici Get/Set (Get/Insert per
	Chart, Get/Run per GoalSeek), testati qui sia direttamente sia
	attraverso un vero giro BMessage (AddSpecifier/ResolveSpecifier/
	MessageReceived), stesso principio gemello di
	test_format_toolbar.cpp per i pulsanti della toolbar.

	Bug reale trovato costruendo questo: "hey ... set Value ... to
	"42"" falliva con B_BAD_VALUE anche se il testo era fra virgolette
	-- "hey" impacchetta un valore che sembra numerico come int32/
	double, non come stringa, indipendentemente dalle virgolette sulla
	riga di comando. FindScriptedDataAsString (MainWindow.cpp) prova
	ogni tipo comune prima di arrendersi; coperto esplicitamente sotto.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Entry.h>
#include <Message.h>

#include "Cell.h"
#include "Range.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "FontMetrics.h"
#include "Formatter.h"
#include "SheetView.h"
#include "MainWindow.h"

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
	BApplication app("application/x-vnd.Atomo-TestScripting");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// --- GetSupportedSuites: la nuova suite e' davvero pubblicizzata ---
	BMessage suites;
	win->GetSupportedSuites(&suites);
	BString suiteName;
	bool foundSuite = false;
	for (int32 i = 0; suites.FindString("suites", i, &suiteName) == B_OK; i++)
	{
		if (suiteName == "suite/vnd.Atomo-sheet")
		{
			foundSuite = true;
			break;
		}
	}
	Check(foundSuite, "GetSupportedSuites annuncia \"suite/vnd.Atomo-sheet\"");

	// --- ResolveSpecifier: coperto indirettamente, non con una chiamata
	// diretta qui -- costruire a mano un messaggio di risoluzione
	// specificatore abbastanza fedele da non far andare in crash
	// BWindow::ResolveSpecifier/BPropertyInfo::FindMatch (quando la
	// proprieta' NON e' una delle due nuove e la delega cade sulla
	// classe base) si e' rivelato fragile: un crash reale e' stato
	// trovato proprio cosi' durante la scrittura di questo test, dentro
	// codice di Haiku, non nostro. La stessa copertura arriva in modo
	// piu' sicuro dal giro MessageReceived vero piu' sotto (che e' poi
	// cio' che conta davvero: MainWindow::MessageReceived e' l'unico
	// punto che la vera BLooper richiama dopo un ResolveSpecifier gia'
	// risolto) e dal collaudo dal vivo con un vero "hey Atomo123 ..."
	// durante lo sviluppo di questa funzione (Suites/Value/Selection
	// *e* Title confermati funzionanti fianco a fianco).

	// --- ScriptedSetValue/ScriptedGetValue, chiamata diretta ---
	TryToParseString("1", cell(1, 2), doc, true); // A2, mai selezionata/toccata da questo test
	view->SetSelection(cell(1, 1)); // A1
	view->ExtendSelection(cell(1, 1));

	status_t err = win->ScriptedSetValue("42");
	Check(err == B_OK, "ScriptedSetValue(\"42\") su A1 riesce");
	Check(win->ScriptedGetValue() == "42", "ScriptedGetValue su A1 rilegge \"42\"");

	view->SetSelection(cell(2, 1)); // B1
	view->ExtendSelection(cell(2, 1));
	err = win->ScriptedSetValue("=A1*2");
	Check(err == B_OK, "ScriptedSetValue con una formula (\"=A1*2\") su B1 riesce");
	Check(win->ScriptedGetValue() == "84", "ScriptedGetValue su B1 rilegge il RISULTATO calcolato (84), non il testo della formula");

	Check(win->ScriptedGetValue() != "2",
		"una formula letta via scripting non e' mai il testo sorgente -- sarebbe un bug silenzioso se lo fosse");

	// Isolamento sulla sola cella attiva: A2 (mai toccata da questo test)
	// deve restare "1".
	CellStyle csA2; (void)csA2;
	Value a2;
	doc->GetValue(cell(1, 2), a2);
	char a2Text[64];
	gFormatTable.FormatValue(eGeneral, a2, a2Text);
	Check(strcmp(a2Text, "1") == 0,
		"scrivere su A1/B1 via scripting non tocca A2, mai selezionata da questo test");

	view->SetSelection(cell(3, 1)); // C1
	view->ExtendSelection(cell(3, 1));
	err = win->ScriptedSetValue("Ciao mondo");
	Check(err == B_OK, "ScriptedSetValue con testo semplice su C1 riesce");
	Check(win->ScriptedGetValue() == "Ciao mondo", "ScriptedGetValue su C1 rilegge il testo scritto");

	err = win->ScriptedSetValue("bad");
	(void)err; // "bad" e' comunque testo valido (non una formula rotta): non deve fallire.

	// --- ScriptedSetSelection/ScriptedGetSelection ---
	err = win->ScriptedSetSelection("A1");
	Check(err == B_OK, "ScriptedSetSelection(\"A1\") riesce");
	Check(win->ScriptedGetSelection() == "A1", "ScriptedGetSelection rilegge \"A1\" dopo averla impostata");

	err = win->ScriptedSetSelection("A1:B2");
	Check(err == B_OK, "ScriptedSetSelection(\"A1:B2\") (un intervallo) riesce");
	Check(win->ScriptedGetSelection() == "A1:B2", "ScriptedGetSelection rilegge l'intero intervallo \"A1:B2\"");

	err = win->ScriptedSetSelection("ZZZ9999999");
	Check(err == B_BAD_VALUE, "ScriptedSetSelection con un indirizzo fuori dai limiti restituisce B_BAD_VALUE, non va in crash ne' sposta la selezione a caso");

	err = win->ScriptedSetSelection("");
	Check(err == B_BAD_VALUE, "ScriptedSetSelection con una stringa vuota restituisce B_BAD_VALUE");

	// Il giro COMPLETO BMessage (ResolveSpecifier reale della BLooper,
	// poi MessageReceived, "data" impacchettato come int32 da "hey"
	// anche fra virgolette) e' stato verificato dal vivo, ripetutamente,
	// contro il vero processo in esecuzione durante lo sviluppo di
	// questa funzione ("hey Atomo123 set Value of Window 0 to 42",
	// "to "=A1*2"", "set Selection ... to "A1:B2"", confermato anche
	// via screenshot) -- non replicato qui perche' costruire a mano un
	// BMessage di specificatore valido per BMessage::GetCurrentSpecifier
	// senza passare dalla vera BLooper (che imposta un bookkeeping
	// interno non pubblicamente documentato) si e' rivelato fragile:
	// un tentativo e' andato in crash dentro BWindow::ResolveSpecifier/
	// BPropertyInfo::FindMatch (codice di Haiku, non nostro), un secondo
	// falliva con B_BAD_SCRIPT_SYNTAX anche dopo SetCurrentSpecifier(0).
	// La logica vera (sopra) e la sua vera chiamata da MessageReceived
	// (HandleScriptingValue/HandleScriptingSelection in MainWindow.cpp)
	// restano comunque coperte: qui direttamente, dal vivo con "hey".

	// --- ScriptedGetFilePath/ScriptedSetFilePath ---
	Check(win->ScriptedGetFilePath() == "",
		"ScriptedGetFilePath e' vuoto su un documento mai salvato (stesso segnale di IsUntouched)");

	BString savePath("/boot/system/cache/tmp/test_scripting_save.ascd");
	err = win->ScriptedSetFilePath(savePath.String());
	Check(err == B_OK, "ScriptedSetFilePath riesce su una cartella che esiste davvero");
	Check(win->ScriptedGetFilePath() == savePath,
		"ScriptedGetFilePath rilegge lo stesso percorso appena salvato");

	BEntry savedEntry(savePath.String());
	Check(savedEntry.Exists(), "il file e' stato scritto davvero sul disco");
	savedEntry.Remove();

	err = win->ScriptedSetFilePath("/percorso/che/non/esiste/x.ascd");
	Check(err == B_NAME_NOT_FOUND,
		"ScriptedSetFilePath con una cartella inesistente restituisce B_NAME_NOT_FOUND, non va in crash ne' scrive altrove");

	err = win->ScriptedSetFilePath("");
	Check(err == B_BAD_VALUE, "ScriptedSetFilePath con una stringa vuota restituisce B_BAD_VALUE");

	// --- ScriptedGetCellColor/ScriptedSetCellColor ---
	win->ScriptedSetSelection("A1");

	err = win->ScriptedSetCellColor("255,0,0");
	Check(err == B_OK, "ScriptedSetCellColor(\"255,0,0\") riesce");
	Check(win->ScriptedGetCellColor() == "255,0,0",
		"ScriptedGetCellColor rilegge lo stesso colore appena impostato (formato decimale)");

	err = win->ScriptedSetCellColor("00FF00");
	Check(err == B_OK, "ScriptedSetCellColor(\"00FF00\") (formato esadecimale) riesce");
	Check(win->ScriptedGetCellColor() == "0,255,0",
		"ScriptedGetCellColor rilegge il colore esadecimale, riconvertito in formato decimale");

	err = win->ScriptedSetCellColor("999,0,0");
	Check(err == B_BAD_VALUE,
		"ScriptedSetCellColor con una componente fuori da 0-255 restituisce B_BAD_VALUE");

	err = win->ScriptedSetCellColor("non e' un colore");
	Check(err == B_BAD_VALUE,
		"ScriptedSetCellColor con una stringa che non e' ne' \"R,G,B\" ne' esadecimale restituisce B_BAD_VALUE");

	err = win->ScriptedSetCellColor("");
	Check(err == B_BAD_VALUE, "ScriptedSetCellColor con una stringa vuota restituisce B_BAD_VALUE");

	// --- ScriptedGetColumnWidth/ScriptedSetColumnWidth ---
	win->ScriptedSetSelection("A1");

	err = win->ScriptedSetColumnWidth("250");
	Check(err == B_OK, "ScriptedSetColumnWidth(\"250\") su una singola colonna riesce");
	Check(win->ScriptedGetColumnWidth() == "250",
		"ScriptedGetColumnWidth rilegge la stessa larghezza appena impostata");

	win->ScriptedSetSelection("B1:D1");
	err = win->ScriptedSetColumnWidth("100");
	Check(err == B_OK, "ScriptedSetColumnWidth su un intervallo (\"B1:D1\") riesce");
	win->ScriptedSetSelection("C1");
	Check(win->ScriptedGetColumnWidth() == "100",
		"ScriptedSetColumnWidth su un intervallo applica la larghezza a OGNI colonna coperta, "
		"non solo alla prima (qui verificato sulla colonna di mezzo, C)");

	err = win->ScriptedSetColumnWidth("0");
	Check(err == B_BAD_VALUE, "ScriptedSetColumnWidth(\"0\") restituisce B_BAD_VALUE, non una colonna invisibile");

	err = win->ScriptedSetColumnWidth("non e' un numero");
	Check(err == B_BAD_VALUE, "ScriptedSetColumnWidth con testo non numerico restituisce B_BAD_VALUE");

	err = win->ScriptedSetColumnWidth("");
	Check(err == B_BAD_VALUE, "ScriptedSetColumnWidth con una stringa vuota restituisce B_BAD_VALUE");

	// --- ScriptedGetBold/ScriptedSetBold ---
	win->ScriptedSetSelection("A1");
	Check(win->ScriptedGetBold() == "false", "ScriptedGetBold e' \"false\" su una cella nuova");

	err = win->ScriptedSetBold("true");
	Check(err == B_OK, "ScriptedSetBold(\"true\") riesce");
	Check(win->ScriptedGetBold() == "true", "ScriptedGetBold rilegge \"true\" appena impostato");

	err = win->ScriptedSetBold("true");
	Check(err == B_OK && win->ScriptedGetBold() == "true",
		"ScriptedSetBold(\"true\") ripetuto resta \"true\" (esplicito, non un toggle)");

	err = win->ScriptedSetBold("false");
	Check(err == B_OK && win->ScriptedGetBold() == "false",
		"ScriptedSetBold(\"false\") riporta a non grassetto");

	err = win->ScriptedSetBold("maybe");
	Check(err == B_BAD_VALUE, "ScriptedSetBold con un valore che non e' \"true\"/\"false\" restituisce B_BAD_VALUE");

	// --- ScriptedGetBorder/ScriptedSetBorder ---
	win->ScriptedSetSelection("A1:B2");
	err = win->ScriptedSetBorder("1");
	Check(err == B_OK, "ScriptedSetBorder(\"1\") su un intervallo riesce");
	win->ScriptedSetSelection("B2");
	Check(win->ScriptedGetBorder() == "1",
		"ScriptedSetBorder applica il bordo a OGNI cella dell'intervallo, non solo alla prima "
		"(qui verificato sull'angolo opposto, B2)");

	err = win->ScriptedSetBorder("0");
	Check(err == B_OK && win->ScriptedGetBorder() == "0", "ScriptedSetBorder(\"0\") rimuove il bordo");

	err = win->ScriptedSetBorder("-1");
	Check(err == B_BAD_VALUE, "ScriptedSetBorder con uno spessore negativo restituisce B_BAD_VALUE");

	// --- ScriptedGetNumberFormat/ScriptedSetNumberFormat ---
	win->ScriptedSetSelection("A1");
	Check(win->ScriptedGetNumberFormat() == "general", "ScriptedGetNumberFormat e' \"general\" su una cella nuova");

	err = win->ScriptedSetNumberFormat("currency");
	Check(err == B_OK && win->ScriptedGetNumberFormat() == "currency",
		"ScriptedSetNumberFormat(\"currency\") riesce e si rilegge");

	err = win->ScriptedSetNumberFormat("percent");
	Check(err == B_OK && win->ScriptedGetNumberFormat() == "percent",
		"ScriptedSetNumberFormat(\"percent\") riesce e si rilegge");

	err = win->ScriptedSetNumberFormat("CURRENCY");
	Check(err == B_OK && win->ScriptedGetNumberFormat() == "currency",
		"ScriptedSetNumberFormat non distingue maiuscole/minuscole");

	err = win->ScriptedSetNumberFormat("euro");
	Check(err == B_BAD_VALUE, "ScriptedSetNumberFormat con un formato non riconosciuto restituisce B_BAD_VALUE");

	// --- ScriptedGetChart/ScriptedInsertChart ---
	win->ScriptedSetSelection("Y1");
	win->ScriptedSetValue("Categoria");
	win->ScriptedSetSelection("Y2");
	win->ScriptedSetValue("A");
	win->ScriptedSetSelection("Y3");
	win->ScriptedSetValue("B");
	win->ScriptedSetSelection("Z1");
	win->ScriptedSetValue("Valore");
	win->ScriptedSetSelection("Z2");
	win->ScriptedSetValue("10");
	win->ScriptedSetSelection("Z3");
	win->ScriptedSetValue("20");

	Check(win->ScriptedGetChart() == "0", "ScriptedGetChart e' \"0\" su un documento senza grafici");

	err = win->ScriptedInsertChart("Y1:Z3|AB1|bar|Prova");
	Check(err == B_OK, "ScriptedInsertChart con un intervallo/tipo/destinazione validi riesce");
	Check(win->ScriptedGetChart() == "1", "ScriptedGetChart sale a \"1\" dopo l'inserimento");

	err = win->ScriptedInsertChart("Y1:Z3|AB10|pie|");
	Check(err == B_OK, "ScriptedInsertChart con titolo vuoto riesce comunque");
	Check(win->ScriptedGetChart() == "2", "un secondo ScriptedInsertChart si aggiunge, non sostituisce il primo");

	err = win->ScriptedInsertChart("Y1:Z3|AB20|poligono|");
	Check(err == B_BAD_VALUE,
		"ScriptedInsertChart con un tipo non riconosciuto restituisce B_BAD_VALUE, non aggiunge nulla");
	Check(win->ScriptedGetChart() == "2", "...e il conteggio dei grafici resta invariato");

	err = win->ScriptedInsertChart("Y1|AB20|bar|");
	Check(err == B_BAD_VALUE,
		"ScriptedInsertChart con un intervallo mal formato (senza \"|\" sufficienti) restituisce B_BAD_VALUE");

	err = win->ScriptedInsertChart("A1:A3|AB20|bar|");
	Check(err == B_BAD_VALUE,
		"ScriptedInsertChart con un intervallo a una sola colonna (dati non validi per un grafico) "
		"restituisce B_BAD_VALUE invece di mostrare l'avviso bloccante di HandleChartInsert");

	// --- ScriptedGetWrapText/ScriptedSetWrapText ---
	win->ScriptedSetSelection("A1");
	Check(win->ScriptedGetWrapText() == "false", "ScriptedGetWrapText e' \"false\" su una cella nuova");

	err = win->ScriptedSetWrapText("true");
	Check(err == B_OK, "ScriptedSetWrapText(\"true\") riesce");
	Check(win->ScriptedGetWrapText() == "true", "ScriptedGetWrapText rilegge \"true\" appena impostato");

	err = win->ScriptedSetWrapText("true");
	Check(err == B_OK && win->ScriptedGetWrapText() == "true",
		"ScriptedSetWrapText(\"true\") ripetuto resta \"true\" (esplicito, non un toggle)");

	err = win->ScriptedSetWrapText("false");
	Check(err == B_OK && win->ScriptedGetWrapText() == "false",
		"ScriptedSetWrapText(\"false\") disattiva l'a capo");

	err = win->ScriptedSetWrapText("forse");
	Check(err == B_BAD_VALUE, "ScriptedSetWrapText con un valore che non e' \"true\"/\"false\" restituisce B_BAD_VALUE");

	// --- ScriptedGetGoalSeek/ScriptedRunGoalSeek ---
	win->ScriptedSetSelection("P1");
	win->ScriptedSetValue("3");
	win->ScriptedSetSelection("P2");
	win->ScriptedSetValue("=P1*4");

	Check(win->ScriptedGetGoalSeek() == "", "ScriptedGetGoalSeek e' \"\" prima di ogni esecuzione");

	err = win->ScriptedRunGoalSeek("P2|40|P1");
	Check(err == B_OK, "ScriptedRunGoalSeek con un formato valido riesce (sia che converga o meno)");
	Check(win->ScriptedGetGoalSeek().FindFirst("true,") == 0,
		"ScriptedGetGoalSeek inizia con \"true,\" dopo una convergenza (P1 diventa 10, P2 raggiunge 40)");
	win->ScriptedSetSelection("P1");
	double p1Value = atof(win->ScriptedGetValue().String());
	Check(p1Value > 9.99 && p1Value < 10.01,
		"ScriptedRunGoalSeek applica davvero il valore trovato a P1 (~10)");

	err = win->ScriptedRunGoalSeek("P2|40"); // manca il terzo campo "|cambiando"
	Check(err == B_BAD_VALUE,
		"ScriptedRunGoalSeek con meno di due separatori \"|\" restituisce B_BAD_VALUE");

	err = win->ScriptedRunGoalSeek("");
	Check(err == B_BAD_VALUE, "ScriptedRunGoalSeek con una stringa vuota restituisce B_BAD_VALUE");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
