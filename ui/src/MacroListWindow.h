/*
	MacroListWindow.h

	Finestra "Esegui macro" (v4.0 "Scripting", Fase 4 -- UI
	dell'interprete VBA su cui FindVbaSubByName/ExecuteVbaMacro/
	ListVbaMacroNames, engine/src/Vba/VbaMacroRunner.h, sono gia'
	costruiti): un elenco (BListView) dei nomi di Sub pubbliche trovate
	nel progetto VBA del documento attivo, piu' un pulsante "Esegui".
	Stesso schema di NameWindow/GoalSeekWindow: un vero BWindow, non
	tocca mai CContainer o il blob vbaProject.bin direttamente, inoltra
	solo il nome scelto a MainWindow via BMessage -- l'esecuzione vera
	(MainWindow::RunVbaMacro) e il controllo del "trust gate"
	(MainWindow::PromptEnableMacros/fMacrosTrustedThisSession) vivono
	entrambi li', non qui.

	MainWindow richiama SetMacroNames() prima di ogni Show() (l'elenco
	dipende dal documento attivo, puo' cambiare da un'apertura di file
	all'altra) -- stesso principio di NameWindow::SetNames().

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef MACRO_LIST_WINDOW_H
#define MACRO_LIST_WINDOW_H

#include <Messenger.h>
#include <Window.h>

#include <string>
#include <vector>

const uint32 kMsgRunVbaMacro = 'vbrm';

class BListView;

class MacroListWindow : public BWindow {
public:
	MacroListWindow(BMessenger target);

	virtual void MessageReceived(BMessage* message);
	virtual bool QuitRequested();

	// "names" sono gia' nomi di Sub pubbliche (ListVbaMacroNames ha
	// gia' escluso le "Private Sub" e non chiama mai ParseVbaSub, vedi
	// il commento in VbaMacroRunner.h) -- questa finestra le mostra
	// cosi' come sono, senza altro filtro.
	void SetMacroNames(const std::vector<std::string>& names);

private:
	void RunSelected();

	BListView* fMacroList;
	BMessenger fTarget;
};

#endif
