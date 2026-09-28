/*
	test_autocomplete.cpp

	Verifica l'autocompletamento delle funzioni durante l'editing
	in-cella (come Excel/LibreOffice Calc, richiesto esplicitamente
	dall'utente dopo la modalita' punta): dopo "=", un operatore, "(" o
	"," e almeno una lettera digitata, un elenco a comparsa mostra i
	nomi di funzione che iniziano con quanto scritto finora. Vedi
	SheetView::UpdateAutocomplete/HandleAutocompleteKey.

	Stessa tecnica di test_point_mode.cpp/test_real_input_edit.cpp: un
	vero BMessage(B_KEY_DOWN)/B_MOUSE_DOWN, consegnato con
	BMessenger::SendMessage() alla vera BTextView dell'editor (o alla
	vera BListView dell'elenco), passa quindi dal vero ciclo di
	dispatch di BLooper -- incluso il messaggio di modifica che
	BTextControl manda da solo dopo ogni tasto, che e' cio' che innesca
	davvero UpdateAutocomplete in uso reale (non una chiamata C++
	diretta, che salterebbe quel meccanismo).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <InterfaceDefs.h>
#include <ListView.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <Roster.h>
#include <String.h>
#include <StringItem.h>
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
		: BWindow(BRect(100, 100, 700, 500), "test-autocomplete", B_TITLED_WINDOW, 0)
	{
	}
};

static void SendKey(BHandler* target, int32 rawChar, char byte, bool shift)
{
	BMessage keyDown(B_KEY_DOWN);
	keyDown.AddInt32("raw_char", rawChar);
	if (byte)
	{
		keyDown.AddInt8("byte", byte);
		char bytes[2] = { byte, 0 };
		keyDown.AddString("bytes", bytes);
	}
	keyDown.AddInt32("modifiers", shift ? B_SHIFT_KEY : 0);
	BMessage reply;
	BMessenger(target).SendMessage(&keyDown, &reply, 2000000, 2000000);
}

static void SendCharKey(BHandler* target, char c)
{
	SendKey(target, (int32)(unsigned char)c, c, false);
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestAutocomplete");

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

	// --- Comparsa dell'elenco e navigazione con le frecce. ---
	win->Lock();
	view->SetSelection(cell(2, 2)); // B2
	win->Unlock();

	SendCharKey(view, '=');
	win->Lock();
	BTextControl* editor = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	win->Unlock();
	Check(editor != NULL, "digitare \"=\" apre l'editor in-cella");

	BTextView* tv = editor->TextView();
	SendCharKey(tv, 'S');
	SendCharKey(tv, 'U');

	win->Lock();
	BListView* list = dynamic_cast<BListView*>(view->FindView("autocomplete"));
	int32 countAfterSU = list ? list->CountItems() : 0;
	win->Unlock();
	Check(list != NULL, "digitare \"SU\" dopo \"=\" mostra l'elenco di autocompletamento");
	Check(countAfterSU >= 2,
		"l'elenco per \"SU\" contiene almeno due funzioni (SUM, SUMIF, SUMPRODUCT... -- "
		"verifica debole apposta, l'elenco esatto dipende dalle funzioni compilate)");

	if (list)
	{
		win->Lock();
		int32 selBefore = list->CurrentSelection();
		win->Unlock();
		Check(selBefore == 0, "la prima voce dell'elenco e' selezionata di default");

		SendKey(tv, B_DOWN_ARROW, 0, false);
		win->Lock();
		int32 selAfterDown = list->CurrentSelection();
		win->Unlock();
		Check(selAfterDown == 1, "Freccia giu' sposta la selezione alla voce successiva");

		SendKey(tv, B_UP_ARROW, 0, false);
		win->Lock();
		int32 selAfterUp = list->CurrentSelection();
		win->Unlock();
		Check(selAfterUp == 0, "Freccia su' riporta la selezione alla prima voce");
	}

	// --- Escape chiude SOLO l'elenco, non annulla l'intera modifica. ---
	SendKey(tv, B_ESCAPE, 0, false);
	win->Lock();
	BListView* listAfterEscape = dynamic_cast<BListView*>(view->FindView("autocomplete"));
	BTextControl* editorAfterEscape = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	BString textAfterEscape = editorAfterEscape ? editorAfterEscape->Text() : "";
	win->Unlock();
	Check(listAfterEscape == NULL, "Escape chiude l'elenco di autocompletamento");
	Check(editorAfterEscape != NULL && textAfterEscape == "=SU",
		"Escape NON annulla l'intera modifica in corso -- l'editor resta aperto con "
		"\"=SU\" ancora scritto");

	// Pulizia di questo scenario: annulla per davvero, cosi' il prossimo
	// riparte da una cella vuota.
	BMessage cancel1(kMsgCellEditCancel);
	win->Lock();
	view->MessageReceived(&cancel1);
	win->Unlock();

	// --- Accettare un suggerimento con Invio inserisce "NOME(" e
	// chiude l'elenco, senza confermare anche l'intera cella (l'utente
	// deve poter continuare a scrivere gli argomenti). ---
	win->Lock();
	view->SetSelection(cell(2, 2));
	win->Unlock();

	SendCharKey(view, '=');
	win->Lock();
	editor = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	win->Unlock();
	tv = editor->TextView();

	SendCharKey(tv, 'S');
	SendCharKey(tv, 'U');
	SendCharKey(tv, 'M');

	win->Lock();
	list = dynamic_cast<BListView*>(view->FindView("autocomplete"));
	win->Unlock();
	Check(list != NULL, "l'elenco resta visibile mentre si continua a digitare \"SUM\"");

	// La prima voce corrispondente a "SUM" dovrebbe essere proprio SUM
	// (l'elenco e' costruito scorrendo gFuncArrayByName, gia' ordinato
	// alfabeticamente: "SUM" precede "SUMIF"/"SUMPRODUCT"/ecc.).
	win->Lock();
	BStringItem* firstItem = list ? (BStringItem*)list->ItemAt(0) : NULL;
	BString firstLabel = firstItem ? firstItem->Text() : "";
	win->Unlock();
	Check(firstLabel.FindFirst("SUM") == 0,
		"la prima voce dell'elenco per \"SUM\" inizia davvero per \"SUM\"");

	SendKey(tv, B_RETURN, 0, false);
	win->Lock();
	BListView* listAfterAccept = dynamic_cast<BListView*>(view->FindView("autocomplete"));
	BTextControl* editorAfterAccept = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	BString textAfterAccept = editorAfterAccept ? editorAfterAccept->Text() : "";
	win->Unlock();
	Check(listAfterAccept == NULL, "Invio su un suggerimento chiude l'elenco");
	Check(editorAfterAccept != NULL,
		"Invio su un suggerimento NON conferma l'intera cella -- l'editor resta aperto "
		"per scrivere gli argomenti");
	Check(textAfterAccept == "=SUM(",
		"il nome scelto sostituisce il prefisso digitato e aggiunge \"(\" -- testo finale "
		"\"=SUM(\"");

	// Completa la formula e conferma davvero, per verificare che il
	// risultato finale calcoli qualcosa di sensato (non solo che il
	// TESTO sia quello giusto).
	TryToParseString("5", cell(1, 1), doc, true); // A1
	TryToParseString("7", cell(1, 2), doc, true); // A2
	const char* rest = "A1:A2)";
	for (size_t i = 0; i < strlen(rest); i++)
		SendCharKey(editorAfterAccept->TextView(), rest[i]);

	BMessage commit(kMsgCellEditCommit);
	win->Lock();
	view->MessageReceived(&commit);
	win->Unlock();

	Value result;
	doc->GetValue(cell(2, 2), result);
	Check(result.fType == eNumData && (double)result == 12.0,
		"la formula completata a mano (\"=SUM(A1:A2)\") calcola davvero 12 (5+7)");

	// --- Un identificatore che smette di corrispondere a una funzione
	// (perche' in realta' e' un riferimento di cella) chiude l'elenco
	// da solo, senza bisogno di alcuna azione esplicita. ---
	win->Lock();
	view->SetSelection(cell(2, 3)); // B3
	win->Unlock();

	SendCharKey(view, '=');
	win->Lock();
	editor = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	win->Unlock();
	tv = editor->TextView();

	SendCharKey(tv, 'A');
	win->Lock();
	bool listShownForA = view->FindView("autocomplete") != NULL;
	win->Unlock();
	Check(listShownForA,
		"digitare \"A\" da solo dopo \"=\" mostra l'elenco (ABS/AND/AVERAGE... esistono "
		"davvero funzioni che iniziano per \"A\")");

	SendCharKey(tv, '1');
	win->Lock();
	bool listGoneForA1 = view->FindView("autocomplete") == NULL;
	BString textA1 = editor->Text();
	win->Unlock();
	Check(listGoneForA1,
		"digitare \"1\" (ora \"A1\", un riferimento di cella, nessuna funzione inizia "
		"cosi') chiude l'elenco da solo");
	Check(textA1 == "=A1", "il testo digitato resta comunque \"=A1\", scritto normalmente");

	BMessage cancel2(kMsgCellEditCancel);
	win->Lock();
	view->MessageReceived(&cancel2);
	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
