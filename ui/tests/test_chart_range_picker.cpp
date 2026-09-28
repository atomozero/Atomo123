/*
	test_chart_range_picker.cpp

	Verifica il selettore di intervallo dell'editor dei grafici (ultima
	delle 4 migliorie "editor piu' professionale" richieste dall'utente,
	dopo icone del tipo, interruttore righe/colonne e colori per serie):
	un pulsante "..." che arma SheetView per catturare il PROSSIMO
	MouseUp (clic o trascinamento, indifferentemente) e restituirne
	l'intervallo gia' formattato, invece di dover digitare l'intervallo
	a mano.

	Tre parti, come da piano:
	1) SheetView pura (nessuna ChartWindow/MainWindow coinvolta):
	   StartRangePicker/CancelRangePicker/IsRangePickerActive/MouseUp
	   chiamati direttamente come normali metodi C++ (stessa tecnica gia'
	   stabilita da test_chart_drag.cpp/test_chart_resize.cpp per la
	   logica del mouse in questa classe).
	2) ChartWindow in isolamento (costruita con un semplice BWindow di
	   test come "MainWindow" finto, non una vera MainWindow): un
	   kMsgRangePicked sintetico deve aggiornare il campo Intervallo e
	   far ripartire RequestDraw(), verificabile osservando il
	   kMsgChartRequest che ne risulta (stesso schema di ogni altro test
	   su questa finestra: nessun dialogo reale, solo messaggi).
	3) Giro completo tramite una vera MainWindow: kMsgChartRangePickRequest
	   (come lo manderebbe il pulsante "...") arma davvero
	   SheetView::StartRangePicker con il messenger giusto; un
	   MouseUp successivo restituisce l'intervallo al bersaglio
	   corretto; "start"=false disarma senza scegliere nulla.

	Limite noto (stesso principio gia' documentato in
	test_edit_chart.cpp per il doppio clic, e in
	feedback_pippo_click_scroll_bug per l'automazione del mouse su
	questa app): il VERO clic sul pulsante "..." e il VERO
	trascinamento sul foglio richiedono un desktop Haiku interattivo,
	non riproducibili qui. Questo file verifica la logica sottostante
	(gia' quella che conta: il pulsante stesso si limita a mandare
	kMsgChartRangePickRequest, gia' testato alla parte 3).
*/

#include <cstdio>

#include <Application.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <String.h>
#include <Window.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "Chart.h"
#include "ChartWindow.h"
#include "MainWindow.h"
#include "SheetView.h"

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

// Finestra di test generica: cattura l'ULTIMO messaggio ricevuto in
// alcuni campi semplici, cosi' i test possono verificarne il contenuto
// senza dover scrivere un vero gestore per ogni scenario. Usata sia come
// bersaglio finale di SheetView (Parte 1/3) sia come "MainWindow finta"
// per ChartWindow (Parte 2).
class CaptureWindow : public BWindow {
public:
	CaptureWindow()
		:
		BWindow(BRect(50, 50, 250, 150), "capture", B_TITLED_WINDOW, 0),
		fReceivedCount(0),
		fLastWhat(0),
		fLastStart(true)
	{
	}

	virtual void MessageReceived(BMessage* message)
	{
		fReceivedCount++;
		fLastWhat = message->what;
		fLastRange = "";
		message->FindString("range", &fLastRange);
		fLastStart = true;
		message->FindBool("start", &fLastStart);
	}

	int fReceivedCount;
	uint32 fLastWhat;
	BString fLastRange;
	bool fLastStart;
};

class TestWindow : public BWindow {
public:
	TestWindow()
		: BWindow(BRect(300, 100, 1100, 700), "test-chart-range-picker", B_TITLED_WINDOW, 0)
	{
	}
};

int main()
{
	BApplication app("application/x-vnd.Atomo-TestChartRangePicker");

	// --- Parte 1: SheetView pura -----------------------------------------
	{
		CContainer* doc = new CContainer(NULL, NULL);
		TestWindow* win = new TestWindow();
		SheetView* view = new SheetView(doc);
		BScrollView* scroll = new BScrollView("scroll", view, B_FOLLOW_ALL, 0, true, true);
		scroll->ResizeTo(700, 500);
		BLayoutBuilder::Group<>(win, B_VERTICAL, 0).Add(scroll);
		win->Show();

		CaptureWindow* target = new CaptureWindow();
		target->Show();
		snooze(200000);

		win->Lock();

		Check(!view->IsRangePickerActive(), "il selettore e' spento appena creata la vista");

		// B2:D5 (colonne 2-4, righe 2-5) -- FormatRangeRef gia' verificato
		// altrove, qui basta un intervallo multi-cella riconoscibile.
		view->SetSelection(cell(2, 2));
		view->ExtendSelection(cell(4, 5));

		view->StartRangePicker(BMessenger(target), 'test');
		Check(view->IsRangePickerActive(), "StartRangePicker arma davvero il selettore");

		// Un MouseUp qualunque (la posizione del clic non conta: la
		// selezione e' gia' quella impostata sopra, non quella sotto il
		// cursore) completa la scelta e disarma SEMPRE, un colpo solo.
		view->MouseUp(BPoint(10, 10));
		Check(!view->IsRangePickerActive(), "MouseUp disarma il selettore dopo averlo usato una volta");

		win->Unlock();
		snooze(200000);

		target->Lock();
		Check(target->fReceivedCount == 1, "il bersaglio ha ricevuto esattamente un messaggio");
		Check(target->fLastWhat == (uint32)'test', "il messaggio ricevuto ha il \"what\" passato a StartRangePicker");
		Check(target->fLastRange == "B2:D5", "il campo \"range\" e' l'intervallo impostato, gia' formattato");
		target->Unlock();

		// Un secondo MouseUp senza riarmare non manda nulla di nuovo: il
		// disarmo e' davvero un colpo solo, non un'osservazione continua.
		win->Lock();
		view->MouseDown(BPoint(10, 10));
		view->MouseUp(BPoint(20, 20));
		win->Unlock();
		snooze(200000);
		target->Lock();
		Check(target->fReceivedCount == 1, "un MouseUp senza selettore armato non manda nulla");
		target->Unlock();

		// CancelRangePicker disarma SENZA mandare nulla -- usato quando chi
		// ha chiesto la selezione (la finestra Grafico) viene chiusa prima
		// che l'utente completi un clic.
		win->Lock();
		view->StartRangePicker(BMessenger(target), 'test');
		Check(view->IsRangePickerActive(), "riarmato per il test di CancelRangePicker");
		view->CancelRangePicker();
		Check(!view->IsRangePickerActive(), "CancelRangePicker disarma");
		view->MouseUp(BPoint(30, 30));
		win->Unlock();
		snooze(200000);
		target->Lock();
		Check(target->fReceivedCount == 1,
			"un MouseUp dopo CancelRangePicker non manda nulla (nessun messaggio in piu')");
		target->Unlock();

		win->Lock();
		win->Quit();
		target->Lock();
		target->Quit();
	}

	// --- Parte 2: ChartWindow in isolamento -------------------------------
	{
		CaptureWindow* target = new CaptureWindow();
		target->Show();
		snooze(200000);

		ChartWindow* cw = new ChartWindow(BMessenger(target));
		cw->Lock();

		BMessage picked(kMsgRangePicked);
		picked.AddString("range", "C3:E9");
		cw->MessageReceived(&picked);

		cw->Unlock();
		snooze(200000);

		target->Lock();
		// RequestDraw() (chiamata dal gestore di kMsgRangePicked) manda
		// kMsgChartRequest con "range" preso da fRangeField, appena
		// impostato con il testo ricevuto -- unico modo osservabile
		// dall'esterno per verificare che il campo sia stato aggiornato
		// davvero, dato che fRangeField e' privato.
		Check(target->fReceivedCount >= 1, "ChartWindow ha mandato almeno un messaggio dopo kMsgRangePicked");
		Check(target->fLastWhat == kMsgChartRequest,
			"l'ultimo messaggio e' kMsgChartRequest (RequestDraw richiamato da kMsgRangePicked)");
		Check(target->fLastRange == "C3:E9",
			"il campo Intervallo e' stato aggiornato con il testo ricevuto da kMsgRangePicked");
		target->Unlock();

		cw->Lock();
		cw->Quit();
		target->Lock();
		target->Quit();
	}

	// --- Parte 3: giro completo tramite una vera MainWindow --------------
	{
		MainWindow* win = new MainWindow();
		win->Show();
		win->Lock();

		CaptureWindow* target = new CaptureWindow();
		target->Show();
		snooze(200000);

		SheetView* sheetView = win->GetSheetView();
		Check(!sheetView->IsRangePickerActive(), "il selettore del foglio e' spento all'avvio");

		// Chiamata diretta a MessageReceived (stesso principio di
		// HandleChartInsert/HandleChartUpdate negli altri test su questa
		// finestra: nessun vero giro di coda messaggi necessario per
		// verificare la logica di smistamento), esattamente il messaggio
		// che il pulsante "..." manderebbe.
		BMessage startRequest(kMsgChartRangePickRequest);
		startRequest.AddBool("start", true);
		startRequest.AddMessenger("replyTo", BMessenger(target));
		win->MessageReceived(&startRequest);
		Check(sheetView->IsRangePickerActive(),
			"kMsgChartRangePickRequest con start=true arma davvero SheetView");

		sheetView->SetSelection(cell(1, 1));
		sheetView->ExtendSelection(cell(2, 3));
		sheetView->MouseUp(BPoint(10, 10));
		Check(!sheetView->IsRangePickerActive(), "il MouseUp successivo disarma come sempre");

		win->Unlock();
		snooze(200000);

		target->Lock();
		Check(target->fReceivedCount == 1, "il bersaglio (\"replyTo\") ha ricevuto l'intervallo scelto");
		Check(target->fLastWhat == kMsgRangePicked, "il messaggio ricevuto e' kMsgRangePicked");
		Check(target->fLastRange == "A1:B3", "l'intervallo ricevuto e' quello impostato (A1:B3)");
		target->Unlock();

		// start=false disarma senza scegliere nulla (secondo clic sullo
		// stesso pulsante "...", o la finestra Grafico che si chiude).
		win->Lock();
		BMessage startAgain(kMsgChartRangePickRequest);
		startAgain.AddBool("start", true);
		startAgain.AddMessenger("replyTo", BMessenger(target));
		win->MessageReceived(&startAgain);
		Check(sheetView->IsRangePickerActive(), "riarmato per il test di start=false");

		BMessage cancelRequest(kMsgChartRangePickRequest);
		cancelRequest.AddBool("start", false);
		win->MessageReceived(&cancelRequest);
		Check(!sheetView->IsRangePickerActive(), "kMsgChartRangePickRequest con start=false disarma");

		sheetView->MouseUp(BPoint(15, 15));
		win->Unlock();
		snooze(200000);

		target->Lock();
		Check(target->fReceivedCount == 1,
			"dopo il disarmo, un MouseUp non manda nulla in piu' (nessun secondo kMsgRangePicked)");
		target->Unlock();

		win->Lock();
		win->Quit();
		target->Lock();
		target->Quit();
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
