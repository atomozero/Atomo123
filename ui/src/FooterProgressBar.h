/*
	FooterProgressBar.h

	Barra di avanzamento compatta a UNA riga, disegnata a mano invece
	di usare BStatusBar (Fase 33, richiesta esplicita dell'utente
	guardando l'app dal vivo: "posso avere la barra e il testo sulla
	stessa riga" -- BStatusBar riserva sempre una riga di testo SOPRA
	la barra, anche senza nessuna etichetta, quindi non puo' mai stare
	sulla stessa riga di un testo accanto). Usata nel footer di
	MainWindow insieme a una BStringView separata per il testo della
	fase (vedi MainWindow::fFooterProgressLabel).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef FOOTER_PROGRESS_BAR_H
#define FOOTER_PROGRESS_BAR_H

#include <View.h>

class FooterProgressBar : public BView {
public:
	FooterProgressBar(const char* name);

	virtual void Draw(BRect updateRect);
	virtual void GetPreferredSize(float* width, float* height);
	virtual BSize MinSize();
	virtual BSize MaxSize();
	virtual BSize PreferredSize();

	// Fra 0.0 e 1.0, ridisegna solo se il valore cambia davvero (evita
	// un Invalidate() a vuoto a ogni "impulso" che non sposta la barra
	// di un pixel visibile).
	void SetFraction(float fraction);
	float Fraction() const { return fFraction; }

private:
	float fFraction;
};

#endif
