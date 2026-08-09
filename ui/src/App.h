/*
	App.h

	BApplication dell'app Atomo123: crea la finestra principale e
	inoltra i file aperti da Tracker/riga di comando (B_REFS_RECEIVED)
	a una finestra (nuova o riusata) tramite MainWindow::OpenFile. Piu'
	MainWindow possono coesistere -- l'elenco autorevole e' quello che
	BApplication tiene gia' da solo (CountWindows()/WindowAt()), niente
	puntatore singolo ne' lista duplicata qui.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef APP_H
#define APP_H

#include <Application.h>

class MainWindow;
class SplashWindow;
class BMessageRunner;

class App : public BApplication {
public:
	App();
	virtual ~App();

	virtual void ReadyToRun();
	virtual void RefsReceived(BMessage* message);
	virtual void MessageReceived(BMessage* message);

private:
	// Mostrata subito da ReadyToRun, la MainWindow arriva solo dopo
	// kSplashDelay (vedi ReadyToRun/MessageReceived): un file gia'
	// pronto all'avvio (RefsReceived, vedi sotto) puo' pero' crearne una
	// prima che il timer scada, quindi questo passo va sempre attraverso
	// lo stesso controllo "esiste gia' una MainWindow?" invece di creare
	// alla cieca.
	void ShowMainWindowIfNeeded();


	// Registra Atomo123 come applicazione preferita per i tipi MIME
	// che sa aprire (vedi Atomo123.rdef, risorsa file_types), ma SOLO
	// per un tipo che non ne ha ancora una -- mai forzare una scelta
	// gia' fatta dall'utente o da un'altra applicazione. Chiamata a
	// ogni avvio (economica, idempotente): senza questo passo Tracker
	// elenca comunque Atomo123 in "Apri con..." (grazie alla risorsa
	// file_types), ma non lo sceglierebbe mai da solo al doppio clic.
	void RegisterFileTypes();

	// Cerca fra le finestre gia' aperte una finestra "vergine" (mai
	// toccata, vedi MainWindow::IsUntouched()) da riusare per il
	// prossimo file invece di aprirne una ridondante -- pensato per il
	// solo caso "avvio con un file gia' pronto" (ReadyToRun crea una
	// finestra vuota e subito dopo arriva il B_REFS_RECEIVED dello
	// stesso avvio, in un ordine non garantito). Se l'app e' gia' in
	// uso con documenti aperti nessuna finestra risulta vergine, quindi
	// RefsReceived ne apre sempre una nuova senza mai rimpiazzare un
	// documento gia' aperto.
	MainWindow* FindReusableWindow() const;

	// Puntatore "di comodo" solo per riattivarla (portarla di nuovo
	// davanti) dopo che ShowMainWindowIfNeeded() ha mostrato la prima
	// MainWindow -- vedi il commento su B_NORMAL_WINDOW_FEEL in
	// SplashWindow.cpp. Si chiude sempre da sola (AtomGLView::_Tick()),
	// non c'e' nessun Quit()/delete esplicito da fare qui.
	SplashWindow* fSplashWindow;
	// Timer una tantum (kSplashDelay in App.cpp) che invia
	// kMsgShowMainWindow: l'oggetto BMessageRunner sopravvive al primo
	// (e unico) invio finche' non lo si cancella esplicitamente, quindi
	// va eliminato nel distruttore invece di lasciarlo penzolante per
	// tutta la durata dell'applicazione.
	BMessageRunner* fShowMainWindowTimer;
};

#endif
