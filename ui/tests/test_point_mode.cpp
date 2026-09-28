/*
	test_point_mode.cpp

	Verifica la "modalita' punta" durante l'editing in-cella (come
	Excel/LibreOffice Calc, richiesta esplicita dell'utente): dopo "=",
	un operatore, "(" o "," le frecce inseriscono un riferimento invece
	di spostare il cursore di testo, e Maiusc+Freccia estende a un
	intervallo. Vedi SheetView::HandlePointModeArrow.

	Stessa tecnica di test_real_input_edit.cpp (vedi il suo stesso
	commento in testa al file per il perche'): un vero
	BMessage(B_KEY_DOWN), con "raw_char"/"modifiers" esattamente come li
	metterebbe l'app_server, consegnato con BMessenger::SendMessage()
	alla vera BTextView dell'editor in-cella -- passa quindi dal vero
	ciclo di dispatch di BLooper, incluso il vero CellEditKeyFilter
	registrato su di essa (non da una chiamata C++ diretta a
	HandlePointModeArrow, che salterebbe il filtro stesso). hey/Pippo
	non e' stato usato: un'iniezione OS di frecce si e' rivelata
	inaffidabile in questo sandbox durante lo sviluppo di questa
	funzione (i byte di controllo delle frecce arrivavano come testo
	letterale "" invece che come vero tasto), lo stesso motivo
	documentato in test_real_input_edit.cpp per un problema per certi
	versi analogo.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <Roster.h>
#include <String.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "FunctionUtils.h"
#include "Globals.h"
#include "MyError.h"
#include "ResourceManager.h"
#include "SheetView.h"
#include "Value.h"

static const uint32 kMsgCellEditCommit = 'cedt';
static const uint32 kMsgCellEditCancel = 'cedc';

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

class TestWindow : public BWindow {
public:
	TestWindow()
		: BWindow(BRect(100, 100, 700, 500), "test-point-mode", B_TITLED_WINDOW, 0)
	{
	}
};

// Invia un vero B_KEY_DOWN (freccia o carattere stampabile) alla
// BTextView passata, esattamente come farebbe l'app_server -- stessa
// tecnica di test_real_input_edit.cpp.
static void SendArrowKey(BHandler* target, int32 rawChar, bool shift)
{
	BMessage keyDown(B_KEY_DOWN);
	keyDown.AddInt32("raw_char", rawChar);
	keyDown.AddInt32("modifiers", shift ? B_SHIFT_KEY : 0);
	BMessage reply;
	BMessenger(target).SendMessage(&keyDown, &reply, 2000000, 2000000);
}

static void SendCharKey(BHandler* target, char c)
{
	BMessage keyDown(B_KEY_DOWN);
	keyDown.AddInt8("byte", c);
	char bytes[2] = { c, 0 };
	keyDown.AddString("bytes", bytes);
	// Un vero B_KEY_DOWN dell'app_server porta SEMPRE "raw_char" (int32),
	// anche per un tasto stampabile normale -- non solo per le frecce.
	// Senza questo campo, CellEditKeyFilter::Filter esce subito al primo
	// FindInt32("raw_char", ...) fallito, senza mai raggiungere la
	// logica "qualunque altro tasto chiude la modalita' punta" piu' in
	// fondo: un bug reale scoperto proprio scrivendo questo test (lo
	// scenario B sotto falliva silenziosamente per questo, non per un
	// problema in HandlePointModeArrow/ExitPointMode).
	keyDown.AddInt32("raw_char", (int32)(unsigned char)c);
	keyDown.AddInt32("modifiers", 0);
	BMessage reply;
	BMessenger(target).SendMessage(&keyDown, &reply, 2000000, 2000000);
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestPointMode");

	// SUM (usata dallo Scenario C sotto) e' una funzione con nome: senza
	// questa stessa inizializzazione di App::ReadyToRun (vedi il
	// commento gemello in test_whatif.cpp), resterebbe un identificatore
	// sconosciuto e la formula fallirebbe silenziosamente il parsing.
	app_info info;
	if (app.GetAppInfo(&info) == B_OK)
	{
		BPath execPath(&info.ref);
		gAppName = execPath;
		gResourceManager.SetTo(&execPath);
		try { InitFunctions(); }
		catch (CErr&) { }
	}

	CContainer* doc = new CContainer(NULL, NULL);
	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	win->AddChild(view);

	win->Show();
	snooze(200000);

	// --- Scenario A: le frecce SOSTITUISCONO il riferimento appena
	// puntato invece di accumularne uno nuovo a ogni pressione. Editing
	// su B2 (colonna 2, riga 2): Destra->C2, Destra->D2 (sostituisce
	// C2, non "C2D2"), Giu->D3 (sostituisce D2). ---
	win->Lock();
	view->SetSelection(cell(2, 2));
	win->Unlock();

	SendCharKey(view, '=');	// apre l'editor in-cella su B2 con "="

	win->Lock();
	BTextControl* editor = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	bool editorOpen = editor != NULL;
	win->Unlock();
	Check(editorOpen, "digitare \"=\" apre l'editor in-cella");
	if (!editorOpen)
	{
		printf("\nALCUNI TEST SONO FALLITI (editor non aperto, impossibile continuare)\n");
		return 1;
	}

	BTextView* tv = editor->TextView();

	SendArrowKey(tv, B_RIGHT_ARROW, false);
	win->Lock();
	BString afterFirstRight = editor->Text();
	win->Unlock();
	Check(afterFirstRight == "=C2",
		"prima Freccia destra durante l'editing di B2 inserisce \"=C2\" (un riferimento nuovo)");

	SendArrowKey(tv, B_RIGHT_ARROW, false);
	win->Lock();
	BString afterSecondRight = editor->Text();
	win->Unlock();
	Check(afterSecondRight == "=D2",
		"seconda Freccia destra SOSTITUISCE il riferimento a \"=D2\", non lo accoda (\"=C2D2\")");

	SendArrowKey(tv, B_DOWN_ARROW, false);
	win->Lock();
	BString afterDown = editor->Text();
	win->Unlock();
	Check(afterDown == "=D3", "Freccia giu' sposta lo stesso riferimento a \"=D3\"");

	BMessage cancelA(kMsgCellEditCancel);
	win->Lock();
	view->MessageReceived(&cancelA);
	win->Unlock();

	// --- Scenario B: digitare un carattere normale (non una freccia)
	// chiude la sessione di puntamento corrente -- la freccia
	// successiva riparte dalla cella in editing, non da dove il
	// puntatore precedente si era fermato. ---
	win->Lock();
	view->SetSelection(cell(2, 2)); // di nuovo B2
	win->Unlock();

	SendCharKey(view, '=');
	win->Lock();
	editor = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	win->Unlock();
	tv = editor->TextView();

	SendArrowKey(tv, B_RIGHT_ARROW, false); // B2 -> "=C2"
	SendCharKey(tv, '+'); // chiude la sessione di puntamento
	win->Lock();
	BString afterPlus = editor->Text();
	win->Unlock();
	Check(afterPlus == "=C2+",
		"digitare \"+\" dopo aver puntato si accoda normalmente (\"=C2+\"), non sostituisce nulla");

	SendArrowKey(tv, B_LEFT_ARROW, false); // nuova sessione: da B2, non da C2
	win->Lock();
	BString afterSecondSession = editor->Text();
	win->Unlock();
	Check(afterSecondSession == "=C2+A2",
		"la freccia dopo un carattere normale riparte dalla cella in editing (B2->A2), "
		"non continua dall'ultimo riferimento puntato (C2)");

	BMessage cancelB(kMsgCellEditCancel);
	win->Lock();
	view->MessageReceived(&cancelB);
	win->Unlock();

	// --- Scenario C: Maiusc+Freccia estende a un intervallo, e il
	// risultato si commit e si calcola davvero come una vera SUM. ---
	TryToParseString("5", cell(3, 2), doc, true); // C2 = 5
	TryToParseString("7", cell(4, 2), doc, true); // D2 = 7

	win->Lock();
	view->SetSelection(cell(2, 2)); // B2
	win->Unlock();

	SendCharKey(view, '=');
	win->Lock();
	editor = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	win->Unlock();
	tv = editor->TextView();

	const char* sumPrefix = "SUM(";
	for (size_t i = 0; i < strlen(sumPrefix); i++)
		SendCharKey(tv, sumPrefix[i]);
	SendArrowKey(tv, B_RIGHT_ARROW, false); // B2 -> C2: "=SUM(C2"
	SendArrowKey(tv, B_RIGHT_ARROW, true);	// Maiusc+Destra: estende a "C2:D2"
	win->Lock();
	BString afterRangeExtend = editor->Text();
	win->Unlock();
	Check(afterRangeExtend == "=SUM(C2:D2",
		"Maiusc+Freccia destra estende il riferimento puntato a un intervallo (\"C2:D2\")");

	SendCharKey(tv, ')');

	BMessage commitC(kMsgCellEditCommit);
	win->Lock();
	view->MessageReceived(&commitC);
	win->Unlock();

	Value result;
	doc->GetValue(cell(2, 2), result);
	Check(result.fType == eNumData && (double)result == 12.0,
		"la formula \"=SUM(C2:D2)\" costruita puntando con Maiusc+Freccia calcola davvero 12 (5+7)");

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
