/*
	ProgressWindow.h

	Finestra di avanzamento per operazioni lunghe eseguite su un thread
	separato (Fase 31: apertura di un file grande, vedi
	MainWindow::OpenFile) -- niente pulsante di chiusura, si nasconde da
	sola quando l'operazione finisce (vedi Finish() sotto). Aggiornata
	SOLO tramite messaggi (Update()/SetIndeterminate() inviano un
	BMessage a se stessa con BMessenger, mai una chiamata diretta a un
	BView da un altro thread) cosi' che il thread di lavoro possa
	chiamarle senza mai toccare l'Interface Kit da fuori del proprio
	thread -- la stessa regola di ogni altra finestra di questa app.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef PROGRESS_WINDOW_H
#define PROGRESS_WINDOW_H

#include <Messenger.h>
#include <Window.h>

class BStatusBar;
class BStringView;

class ProgressWindow : public BWindow {
public:
	ProgressWindow();

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

	// Thread-safe: puo' essere chiamata da un thread diverso da quello
	// di questa finestra (invia un BMessage a se stessa tramite
	// BMessenger invece di toccare fStatusBar direttamente). "fraction"
	// fra 0.0 e 1.0; "detail" (puo' essere NULL) sostituisce la riga
	// secondaria sotto la barra, per un progresso piu' fine dentro la
	// fase corrente (es. "Foglio 3 di 13, passata 2").
	void Update(float fraction, const char* phaseText, const char* detail = NULL);

	// Nasconde la finestra (mai Quit(): puo' essere riusata per
	// un'apertura successiva, stesso principio di FindWindow/GoToWindow).
	void Finish();

private:
	BStatusBar* fStatusBar;
	BStringView* fDetailView;
	BMessenger fSelf;
};

#endif
