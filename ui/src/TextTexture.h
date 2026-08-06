// TextTexture.h
//
// Rasterizza una stringa su una BBitmap offscreen passando da una vera
// BView (per avere l'hinting/antialiasing dei font di Haiku) e carica
// il risultato come texture OpenGL RGBA. AtomGLView disegna
// titolo/sottotitolo come quad texturizzati sopra la scena 3D usando
// queste texture -- non c'e' disegno di testo via BView possibile una
// volta che BGLView possiede l'intera finestra.

#ifndef TEXT_TEXTURE_H
#define TEXT_TEXTURE_H

#include <GL/gl.h>
#include <GraphicsDefs.h>

class TextTexture {
public:
							TextTexture();
							~TextTexture();

	// Disegna `text` a `size` pt nel colore `color`, costruisce la
	// texture GL. Va chiamato con il contesto GL bloccato/corrente
	// (stesso thread che poi fara' Bind()/Draw con questa texture).
	void					Build(const char* text, float size,
								rgb_color color, bool bold);

	GLuint					TextureID() const { return fTexture; }
	float					AspectRatio() const { return fAspect; }
	bool					IsValid() const { return fTexture != 0; }

private:
	GLuint					fTexture;
	float					fAspect; // width / height, per dimensionare il quad
};

#endif // TEXT_TEXTURE_H
