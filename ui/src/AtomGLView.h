// AtomGLView.h
//
// Il contenuto vero e proprio dello splash: un atomo 3D (nucleo + 3
// orbite elettroniche inclinate) disegnato con OpenGL (BGLView/Mesa),
// che ruota lentamente mentre gli elettroni gli orbitano intorno, con
// la scritta "Atomo123" e il credito StudioBernardi.eu che appaiono in
// dissolvenza sopra. I colori vengono dall'unica tavolozza in
// AtomTheme.h (kTheme).

#ifndef ATOM_GL_VIEW_H
#define ATOM_GL_VIEW_H

#include <GLView.h>
#include <GL/glu.h>
#include <MessageRunner.h>

#include "AtomTheme.h"
#include "TextTexture.h"

class AtomGLView : public BGLView {
public:
							AtomGLView(BRect frame);
	virtual					~AtomGLView();

	virtual void			AttachedToWindow();
	virtual void			DetachedFromWindow();
	virtual void			Draw(BRect updateRect);
	virtual void			FrameResized(float width, float height);
	virtual void			MessageReceived(BMessage* message);
	virtual void			KeyDown(const char* bytes, int32 numBytes);

private:
	struct Star {
		float	x, y;		// normalizzato 0..1
		float	radius;
		float	phase;
		float	speed;
	};

	void					_Init();
	void					_Repaint();
	void					_Render();
	void					_Tick();
	void					_DrawBackground(float width, float height);
	void					_DrawStars(float width, float height);
	void					_DrawAtom();
	void					_DrawOrbit(int index, float radius);
	void					_DrawGlowSphere(float radius, rgb_color color,
								float alpha);
	void					_DrawText(float width, float height);
	void					_DrawTexturedQuad(const TextTexture& tex,
								float centerX, float centerY, float height,
								rgb_color tint, float alpha);
	void					_DrawFadeOut(float width, float height);

	BMessageRunner*			fPulse;
	bigtime_t				fStartTime;
	float					fElapsed;
	bool					fQuitPosted;

	GLUquadric*				fQuadric;
	TextTexture				fTitleTex;
	TextTexture				fSubtitleTex;
	// Numero di versione (es. "v0.9.0"), letto da BAppFileInfo invece
	// che scritto qui a mano -- resta sempre allineato a quello vero
	// del binario in esecuzione, non a un valore facile da dimenticare
	// di aggiornare a ogni release. Allineato a destra nel footer,
	// speculare a titolo/credito a sinistra.
	TextTexture				fVersionTex;

	static const int		kStarCount = 160;
	Star					fStars[kStarCount];
};

#endif // ATOM_GL_VIEW_H
