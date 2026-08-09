// SplashWindow.h
//
// Finestra di avvio: atomo 3D rotante disegnato con OpenGL (vedi
// AtomGLView), mostrata per pochi secondi da App::ReadyToRun prima
// della MainWindow. Si chiude da sola (vedi AtomGLView::_Tick) --
// nessuno la deve chiudere dall'esterno.
//
// Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
// radice del repository).

#ifndef SPLASH_WINDOW_H
#define SPLASH_WINDOW_H

#include <Window.h>

class AtomGLView;

class SplashWindow : public BWindow {
public:
							SplashWindow();

	AtomGLView*				View() const { return fView; }

private:
	AtomGLView*				fView;
};

#endif // SPLASH_WINDOW_H
