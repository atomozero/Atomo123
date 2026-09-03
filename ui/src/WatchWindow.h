/*
	WatchWindow.h

	Finestra "Finestra di controllo" (Formula auditing views, terzo e
	ultimo pezzo dopo Mostra formule e Traccia precedenti/dipendenti):
	un elenco (BListView) di celle "appuntate" dall'utente, ognuna
	mostrata come "Foglio!Cella   =formula   valore" e riaggiornata dal
	vivo a ogni modifica del documento (vedi MainWindow::DocumentChanged/
	RefreshWatchWindow). Stessa regola di NameWindow/CommentWindow: non
	tocca mai CContainer/cell direttamente, riceve solo testo gia'
	pronto via SetRows() (chiamato da MainWindow dopo Lock(), mai da un
	altro thread senza lock) e manda solo una richiesta di rimozione via
	BMessage -- MainWindow tiene l'unico elenco vero (foglio+cella per
	riga) e questa finestra ne mostra soltanto una vista testuale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef WATCH_WINDOW_H
#define WATCH_WINDOW_H

#include <Messenger.h>
#include <String.h>
#include <Window.h>
#include <vector>

// "row" (int32): indice della riga selezionata da rimuovere, nello
// stesso ordine dell'ultimo SetRows().
const uint32 kMsgWatchRemoveRow = 'wtrr';

class BListView;

class WatchWindow : public BWindow {
public:
	WatchWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

	// Sostituisce l'intero elenco visualizzato -- MainWindow la richiama
	// dopo ogni Aggiungi/Rimuovi e a ogni DocumentChanged() (vedi il
	// commento in cima al file per la disciplina di lock).
	void SetRows(const std::vector<BString>& rows);

private:
	BListView* fList;
	BMessenger fTarget;
};

#endif
