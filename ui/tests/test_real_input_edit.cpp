/*
	test_real_input_edit.cpp

	Chiude il punto rimasto aperto in Fase 4 (vedi ROADMAP.md, "Test
	end-to-end del doppio click/digitazione diretta"): finora
	l'apertura dell'editor in-cella era verificata solo chiamando
	SheetView::KeyDown()/MouseDown() direttamente (test_editing.cpp e
	simili, per i motivi spiegati nei loro commenti -- hey/Pippo non
	affidabili, un episodio di clic finito su un'altra finestra di
	un'altra sessione sullo stesso desktop condiviso), mai con un vero
	B_MOUSE_DOWN/B_KEY_DOWN sintetizzato e fatto passare dal vero ciclo
	di dispatch di BLooper.

	Qui si usa una terza via, diversa sia dalla chiamata diretta sia da
	hey/Pippo: un vero BMessage(B_MOUSE_DOWN)/BMessage(B_KEY_DOWN),
	con esattamente i campi che l'app_server metterebbe in un evento
	reale, consegnato con BMessenger::SendMessage() (sincrono: aspetta
	che il thread della finestra lo abbia davvero smistato prima di
	continuare) a SheetView -- BView::DispatchMessage() lo riconosce e
	chiama MouseDown()/KeyDown() esattamente come farebbe con un evento
	vero dell'app_server, compresa la lettura di "clicks"/"modifiers"
	da Window()->CurrentMessage() dentro quelle funzioni (con una
	chiamata diretta sarebbe sempre NULL, vedi il commento in
	test_navigation.cpp). Resta tutto in-process, senza passare da
	nessuno strumento esterno di iniezione: non ha i problemi di
	targeting cross-finestra/cross-sessione gia' documentati con hey/
	Pippo su un desktop condiviso.

	Il nome del campo delle coordinate del mouse in vista locale
	("be:view_where", non "where") e' stato verificato empiricamente
	con un piccolo programma di prova prima di scrivere questo test:
	"where" da solo restava a (0,0), "screen_where" veniva convertito
	sottraendo l'origine schermo della finestra, "be:view_where" e'
	l'unico che arriva in MouseDown() gia' nelle coordinate locali
	attese da CellRect()/CellAt().
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <Messenger.h>
#include <String.h>
#include <TextControl.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "Value.h"

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
		: BWindow(BRect(100, 100, 700, 500), "test-real-input-edit", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestRealInputEdit");

	CContainer* doc = new CContainer(NULL, NULL);
	TestWindow* win = new TestWindow();
	SheetView* view = new SheetView(doc);
	win->AddChild(view);

	// C1 ha gia' un contenuto: il doppio click deve aprire l'editor
	// su quel valore esistente (posizionando il cursore), non
	// sostituirlo -- comportamento diverso dalla digitazione diretta
	// sotto, dove il carattere digitato rimpiazza il contenuto.
	TryToParseString("42", cell(3, 1), doc, true);

	win->Show();
	// Da' tempo al thread della finestra (avviato da Show()) di
	// entrare nel proprio ciclo di messaggi prima del primo
	// SendMessage sincrono -- stesso principio gia' verificato nel
	// programma di prova standalone.
	snooze(200000);

	BMessenger toView(view);

	// --- Digitazione diretta: un vero B_KEY_DOWN su una cella vuota
	// selezionata (B1) deve aprire l'editor con solo il carattere
	// digitato, esattamente come premendo un tasto stampabile mentre
	// il fuoco e' sulla griglia in una sessione reale. ---
	win->Lock();
	view->SetSelection(cell(2, 1));	// B1, vuota
	win->Unlock();

	BMessage keyDown(B_KEY_DOWN);
	keyDown.AddInt8("byte", '7');
	keyDown.AddString("bytes", "7");
	keyDown.AddInt32("modifiers", 0);
	BMessage keyReply;
	status_t keySt = toView.SendMessage(&keyDown, &keyReply, 2000000, 2000000);
	Check(keySt == B_OK, "il B_KEY_DOWN sintetizzato viene consegnato e smistato dal vero ciclo della finestra");

	win->Lock();
	BTextControl* typedEditor = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	bool typedEditorOpen = typedEditor != NULL;
	BString typedText;
	if (typedEditorOpen)
		typedText = typedEditor->Text();
	win->Unlock();

	Check(typedEditorOpen,
		"un vero tasto premuto (B_KEY_DOWN) su una cella selezionata apre l'editor in-cella");
	Check(typedText == "7",
		"l'editor aperto da un vero tasto premuto contiene solo il carattere digitato (sostituisce, non accoda)");

	// Bug reale segnalato dall'utente: digitando un numero di piu'
	// cifre su una cella vuota (es. "78"), il primo tasto premuto DOPO
	// che l'editor si e' aperto ("8") sostituiva l'intero contenuto
	// invece di aggiungersi -- "1" seguito da "2" dava "2", non "12".
	// Causa: SheetView::StartEditing() chiamava Select(len, len) PRIMA
	// di MakeFocus(true), ma BTextView seleziona tutto il proprio
	// testo quando acquisisce il fuoco per la prima volta, sovrascrivendo
	// subito quella posizione del cursore. Va verificato con un vero
	// secondo B_KEY_DOWN indirizzato a chi ha DAVVERO il fuoco tastiera
	// a questo punto -- l'editor appena aperto (la sua BTextView
	// interna), non piu' SheetView -- altrimenti non si passa dal
	// percorso di codice reale che ha causato il bug (un test con
	// KeyDown() chiamato due volte direttamente su SheetView non lo
	// avrebbe mai scoperto).
	BMessenger toEditorText(typedEditor->TextView());
	BMessage secondKeyDown(B_KEY_DOWN);
	secondKeyDown.AddInt8("byte", '8');
	secondKeyDown.AddString("bytes", "8");
	secondKeyDown.AddInt32("modifiers", 0);
	BMessage secondKeyReply;
	status_t secondKeySt = toEditorText.SendMessage(&secondKeyDown, &secondKeyReply, 2000000, 2000000);
	Check(secondKeySt == B_OK, "il secondo B_KEY_DOWN sintetizzato (a chi ha davvero il fuoco: l'editor) viene consegnato");

	win->Lock();
	BString textAfterSecondKey = typedEditor->Text();
	win->Unlock();
	Check(textAfterSecondKey == "78",
		"il secondo tasto premuto si aggiunge al primo (\"78\"), non lo sostituisce (\"8\")");

	// Conferma con lo stesso messaggio che BTextControl manda da solo
	// premendo Invio al suo interno (stessa tecnica gia' in uso in
	// test_editing.cpp per la conferma, non e' quello che questo test
	// vuole verificare -- qui il punto e' come si APRE l'editor).
	static const uint32 kMsgCellEditCommit = 'cedt';
	BMessage commit(kMsgCellEditCommit);
	win->Lock();
	view->MessageReceived(&commit);
	win->Unlock();

	Value b1;
	doc->GetValue(cell(2, 1), b1);
	Check(b1.fType == eNumData && (double)b1 == 78.0,
		"confermando, il valore digitato con due veri tasti viene scritto per intero nel documento (B1=78, non 8)");

	// --- Doppio click: un vero B_MOUSE_DOWN con clicks=2 su C1 (che
	// contiene gia' "42") deve selezionare quella cella e aprirne
	// l'editor sul contenuto esistente. ---
	win->Lock();
	BRect c1Rect = view->CellRect(cell(3, 1));
	win->Unlock();
	BPoint c1Center(c1Rect.left + c1Rect.Width() / 2, c1Rect.top + c1Rect.Height() / 2);

	BMessage mouseDown(B_MOUSE_DOWN);
	mouseDown.AddPoint("be:view_where", c1Center);
	mouseDown.AddInt32("buttons", B_PRIMARY_MOUSE_BUTTON);
	mouseDown.AddInt32("clicks", 2);
	mouseDown.AddInt32("modifiers", 0);
	BMessage mouseReply;
	status_t mouseSt = toView.SendMessage(&mouseDown, &mouseReply, 2000000, 2000000);
	Check(mouseSt == B_OK, "il B_MOUSE_DOWN sintetizzato (doppio click) viene consegnato e smistato dal vero ciclo della finestra");

	win->Lock();
	cell selectionAfterClick = view->Selection();
	BTextControl* clickedEditor = dynamic_cast<BTextControl*>(view->FindView("celledit"));
	bool clickedEditorOpen = clickedEditor != NULL;
	BString clickedText;
	if (clickedEditorOpen)
		clickedText = clickedEditor->Text();
	win->Unlock();

	Check(selectionAfterClick.h == 3 && selectionAfterClick.v == 1,
		"un vero doppio click sposta la selezione sulla cella cliccata (C1)");
	Check(clickedEditorOpen,
		"un vero doppio click (B_MOUSE_DOWN con clicks=2) apre l'editor in-cella");
	Check(clickedText == "42",
		"l'editor aperto da un vero doppio click mostra il contenuto esistente della cella (non lo sostituisce)");

	// Pulizia: annulla invece di confermare, cosi' il valore di C1
	// (gia' verificato) non viene ri-scritto per un motivo estraneo al
	// test.
	BMessage cancel(kMsgCellEditCancel);
	win->Lock();
	view->MessageReceived(&cancel);
	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
