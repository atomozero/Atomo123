/*
	EngineViewStub.h

	Stub sostitutivo per la vecchia classe CCellView (view BeOS/Haiku,
	Interface Kit) usato SOLO dalla libreria engine isolata.

	Il codice storico di Sum-It passa spesso un puntatore opzionale a
	CCellView per notificare la UI (ridisegno, selezione, progress bar)
	durante operazioni sul modello dati. Nella libreria engine questo
	puntatore e' sempre NULL (nessuna UI collegata), quindi i metodi
	reali non vengono mai invocati a runtime: questo stub esiste solo
	per soddisfare il compilatore nei rami di codice raggiungibili
	solo quando il puntatore e' non-NULL.

	Limitazione nota: l'import Excel legacy (Excel.pass1.cpp) scrive
	larghezze colonna/altezze riga/intervalli con nome direttamente
	sulla (ex) CCellView invece che sul modello CContainer. Con questo
	stub quei dati vengono scartati in silenzio quando si importa un
	file XLS in modalita' headless. La soluzione corretta e' spostare
	quei metadati su CContainer (dove concettualmente appartengono):
	vedi nota aperta in docs/ENGINE_API.md.
*/

#ifndef ENGINE_VIEW_STUB_H
#define ENGINE_VIEW_STUB_H

#include "RunArray.h"

class range;

class CCellView {
public:
	float GetColumnWidth(int) { return 0; }
	bool DoesDisplayZero() { return false; }

	void TouchLine(int) {}
	void DrawAllLines() {}
	void DrawStatus() {}
	void Invalidate() {}

	void SetSelection(const range&) {}
	void AddNamedRange(const char*, const range&) {}
	bool IsNamedRange(const char*) { return false; }
	void SetShowGrid(bool) {}
	void SetShowBorders(bool) {}

	CRunArray& GetHeights() { static CRunArray dummy(1, 0); return dummy; }
	CRunArray& GetWidths() { static CRunArray dummy(1, 0); return dummy; }

	int CountPages() { return 0; }
	int GetPageNrForCell(const class cell&) { return 0; }

	struct StubWindow { const char* Title() { return ""; } };
	StubWindow* Window() { static StubWindow w; return &w; }

	class CDocument* Doc() { return nullptr; }
};

#endif
