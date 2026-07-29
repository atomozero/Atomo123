/*
	ProgressStub.h

	Stub sostitutivo per StProgress (Widgets/ProgressView.h), che nel
	codice storico disegna una progress bar reale (CProgressView eredita
	da BView). Nella libreria engine isolata questa classe non deve
	esistere: gli usi di StProgress nel codice copiato sono sempre
	condizionati da un puntatore a view opzionale (es. Text2Cells.cpp:
	"if (inView) progress = new StProgress(inView, ...)"), quindi a
	runtime headless questo stub non viene mai istanziato per davvero
	con una CCellView reale — esiste solo per soddisfare il
	compilatore.
*/

#ifndef ENGINE_PROGRESS_STUB_H
#define ENGINE_PROGRESS_STUB_H

class CCellView;

enum {
	pColorBlue,
	pColorRed,
	pColorYellow
};

class StProgress {
public:
	StProgress(CCellView*, int, bool) {}
	StProgress(CCellView*, int, int, bool) {}
	~StProgress() {}

	void Step() {}
	void Step(int) {}
	void NewMax(int) {}
};

#endif
