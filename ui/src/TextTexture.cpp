#include "TextTexture.h"

#include <Bitmap.h>
#include <Font.h>
#include <View.h>

#include <math.h>

TextTexture::TextTexture()
	:
	fTexture(0),
	fAspect(1.0f)
{
}

TextTexture::~TextTexture()
{
	if (fTexture != 0)
		glDeleteTextures(1, &fTexture);
}

void
TextTexture::Build(const char* text, float size, rgb_color color, bool bold)
{
	if (fTexture != 0) {
		glDeleteTextures(1, &fTexture);
		fTexture = 0;
	}

	BFont font(be_bold_font);
	if (!bold)
		font = *be_plain_font;
	font.SetSize(size);

	font_height fh;
	font.GetHeight(&fh);

	float textWidth = font.StringWidth(text);
	float textHeight = fh.ascent + fh.descent + fh.leading;

	const float pad = 6.0f;
	int width = (int)ceilf(textWidth) + (int)pad * 2;
	int height = (int)ceilf(textHeight) + (int)pad * 2;
	if (width < 2)
		width = 2;
	if (height < 2)
		height = 2;

	BBitmap* bitmap = new BBitmap(BRect(0, 0, width - 1, height - 1),
		B_RGBA32, true);
	BView* view = new BView(bitmap->Bounds(), "text", B_FOLLOW_NONE,
		B_WILL_DRAW);
	bitmap->AddChild(view);

	bitmap->Lock();

	// Sfondo completamente trasparente: B_OP_COPY scrive anche l'alpha,
	// a differenza di B_OP_OVER (default) che lascerebbe ogni pixel non
	// toccato al valore casuale della bitmap appena allocata.
	view->SetDrawingMode(B_OP_COPY);
	view->SetHighColor(0, 0, 0, 0);
	view->FillRect(view->Bounds());

	view->SetFont(&font);
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
	view->SetHighColor(color);
	view->DrawString(text, BPoint(pad, pad + fh.ascent));
	view->Sync();

	bitmap->Unlock();

	glGenTextures(1, &fTexture);
	glBindTexture(GL_TEXTURE_2D, fTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	int32 bpr = bitmap->BytesPerRow();
	glPixelStorei(GL_UNPACK_ROW_LENGTH, bpr / 4);
	// B_RGBA32 in memoria e' B,G,R,A (little-endian su Haiku); GL_BGRA
	// con GL_UNSIGNED_BYTE corrisponde esattamente a quell'ordine di
	// byte, nessuno scambio di canali a mano.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA,
		GL_UNSIGNED_BYTE, bitmap->Bits());
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	fAspect = (float)width / (float)height;

	delete bitmap; // elimina anche la BView figlia
}
