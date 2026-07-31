/*
	App.h

	BApplication dell'app Atomo123: crea la finestra principale e
	inoltra i file aperti da Tracker/riga di comando (B_REFS_RECEIVED)
	a una finestra (nuova o riusata) tramite MainWindow::OpenFile. Piu'
	MainWindow possono coesistere -- l'elenco autorevole e' quello che
	BApplication tiene gia' da solo (CountWindows()/WindowAt()), niente
	puntatore singolo ne' lista duplicata qui.
*/

#ifndef APP_H
#define APP_H

#include <Application.h>

class MainWindow;

class App : public BApplication {
public:
	App();

	virtual void ReadyToRun();
	virtual void RefsReceived(BMessage* message);

private:
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
};

#endif
