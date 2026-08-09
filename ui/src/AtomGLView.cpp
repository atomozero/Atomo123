// AtomGLView.cpp
//
// Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
// radice del repository).

#include "AtomGLView.h"

#include <GL/glu.h>

#include <AppFileInfo.h>
#include <Application.h>
#include <Catalog.h>
#include <File.h>
#include <Message.h>
#include <Window.h>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AtomGLView"

static const uint32 kMsgTick = 'tick';
static const float kTextFadeDuration = 1.6f;	// secondi, ingresso testo
static const float kBandHeight = 100.0f;		// striscia inferiore, senza stelle
static const float kBandPadding = 28.0f;		// margine sinistro del testo
static const float kVisibleDuration = 7.0f;	// secondi a schermo intero prima della dissolvenza
static const float kFadeOutDuration = 1.0f;	// secondi di dissolvenza a nero

static inline void
glColorRGBA(rgb_color c, float alpha)
{
	glColor4f(c.red / 255.0f, c.green / 255.0f, c.blue / 255.0f, alpha);
}

// "v0.9.0" ecc., letta dalla vera risorsa app_version del binario in
// esecuzione (Atomo123.rdef) invece che scritta qui a mano: resta
// sempre allineata alla release vera, non a un numero facile da
// dimenticare di aggiornare. Stringa vuota (nessun testo disegnato) se
// per qualche motivo l'informazione non e' disponibile (es. eseguito
// senza risorse allegate) -- non deve mai bloccare lo splash.
static void
GetVersionString(char* out, size_t outSize)
{
	out[0] = 0;
	app_info info;
	if (!be_app || be_app->GetAppInfo(&info) != B_OK)
		return;

	BFile file(&info.ref, B_READ_ONLY);
	BAppFileInfo appFileInfo(&file);
	version_info versionInfo;
	if (appFileInfo.GetVersionInfo(&versionInfo, B_APP_VERSION_KIND) != B_OK)
		return;

	snprintf(out, outSize, "v%" B_PRIu32 ".%" B_PRIu32 ".%" B_PRIu32,
		versionInfo.major, versionInfo.middle, versionInfo.minor);
}


AtomGLView::AtomGLView(BRect frame)
	:
	BGLView(frame, "atom", B_FOLLOW_ALL, B_WILL_DRAW,
		BGL_RGB | BGL_DOUBLE | BGL_DEPTH | BGL_ALPHA),
	fPulse(NULL),
	fStartTime(0),
	fElapsed(0.0f),
	fQuitPosted(false),
	fQuadric(NULL)
{
	for (int i = 0; i < kStarCount; i++) {
		fStars[i].x = (float)rand() / RAND_MAX;
		fStars[i].y = (float)rand() / RAND_MAX;
		fStars[i].radius = 0.6f + 1.8f * ((float)rand() / RAND_MAX);
		fStars[i].phase = ((float)rand() / RAND_MAX) * 6.2831853f;
		fStars[i].speed = 0.6f + 1.4f * ((float)rand() / RAND_MAX);
	}
}


AtomGLView::~AtomGLView()
{
	delete fPulse;
}


void
AtomGLView::AttachedToWindow()
{
	BGLView::AttachedToWindow();
	LockGL();
	_Init();
	UnlockGL();

	MakeFocus(true);

	fStartTime = system_time();

	BMessage tick(kMsgTick);
	fPulse = new BMessageRunner(BMessenger(this), &tick, 16000 /* ~60 fps */);
}


void
AtomGLView::DetachedFromWindow()
{
	delete fPulse;
	fPulse = NULL;
	BGLView::DetachedFromWindow();
}


void
AtomGLView::_Init()
{
	glClearColor(0, 0, 0, 1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_POINT_SMOOTH);

	fQuadric = gluNewQuadric();
	gluQuadricNormals(fQuadric, GLU_SMOOTH);

	fTitleTex.Build("Atomo123", 60.0f, (rgb_color){255, 255, 255, 255}, true);
	fSubtitleTex.Build(B_TRANSLATE("Sviluppato da StudioBernardi.eu"), 22.0f,
		(rgb_color){255, 255, 255, 255}, false);

	char versionStr[32];
	GetVersionString(versionStr, sizeof(versionStr));
	if (versionStr[0])
		fVersionTex.Build(versionStr, 22.0f, (rgb_color){255, 255, 255, 255}, false);
}


void
AtomGLView::MessageReceived(BMessage* message)
{
	if (message->what == kMsgTick) {
		_Tick();
		_Repaint();
		return;
	}
	BGLView::MessageReceived(message);
}


void
AtomGLView::_Tick()
{
	fElapsed = (system_time() - fStartTime) / 1000000.0f;

	// A differenza della demo standalone (che chiude l'intera
	// applicazione), qui lo splash e' una finestra fra le tante
	// dell'app vera: alla fine della dissolvenza si chiude solo se
	// stessa, l'app prosegue con la MainWindow gia' mostrata da
	// App::ReadyToRun.
	if (!fQuitPosted && fElapsed >= kVisibleDuration + kFadeOutDuration) {
		fQuitPosted = true;
		Window()->PostMessage(B_QUIT_REQUESTED);
	}
}


void
AtomGLView::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes == 1) {
		switch (bytes[0]) {
			case B_ESCAPE:
			case 'q':
			case 'Q':
				if (!fQuitPosted) {
					fQuitPosted = true;
					Window()->PostMessage(B_QUIT_REQUESTED);
				}
				return;
		}
	}
	BGLView::KeyDown(bytes, numBytes);
}


void
AtomGLView::FrameResized(float width, float height)
{
	BGLView::FrameResized(width, height);
	LockGL();
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	UnlockGL();
	Invalidate();
}


void
AtomGLView::Draw(BRect updateRect)
{
	_Repaint();
}


void
AtomGLView::_Repaint()
{
	// Pilotato direttamente dal messaggio di tick invece che passare da
	// Invalidate(): il meccanismo di regione sporca/update-rect di BView
	// puo' limitare il repaint di una BGLView a meno dell'intero frame,
	// lasciando pixel vecchi sullo schermo fra un fotogramma e l'altro
	// (le orbite sembravano sbavare invece di restare cerchi puliti).
	// Emettere i comandi GL direttamente dal timer evita il problema --
	// Draw() sopra resta comunque per i repaint da espositura/resize e
	// riusa questo stesso percorso.
	LockGL();
	_Render();
	SwapBuffers();
	UnlockGL();
}


void
AtomGLView::_Render()
{
	BRect bounds = Bounds();
	float width = bounds.Width() + 1;
	float height = bounds.Height() + 1;
	if (width < 1)
		width = 1;
	if (height < 1)
		height = 1;

	glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	_DrawBackground(width, height);
	if (kTheme.stars)
		_DrawStars(width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(42.0, width / height, 0.1, 100.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(0.0, 1.1, 7.2, 0.0, -0.2, 0.0, 0.0, 1.0, 0.0);

	glEnable(GL_DEPTH_TEST);
	_DrawAtom();
	glDisable(GL_DEPTH_TEST);

	_DrawText(width, height);
	_DrawFadeOut(width, height);
}


void
AtomGLView::_DrawBackground(float width, float height)
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	float bandTop = height - kBandHeight;

	glBegin(GL_QUADS);
	glColor3ub(kTheme.bgTop.red, kTheme.bgTop.green, kTheme.bgTop.blue);
	glVertex2f(0, 0);
	glVertex2f(width, 0);
	glColor3ub(kTheme.bgBottom.red, kTheme.bgBottom.green,
		kTheme.bgBottom.blue);
	glVertex2f(width, bandTop);
	glVertex2f(0, bandTop);
	glEnd();

	// Striscia inferiore: riempimento piatto (niente gradiente, niente
	// stelle) cosi' titolo/sottotitolo poggiano su una fascia calma e
	// leggibile invece che sul cielo.
	glColor3ub(kTheme.band.red, kTheme.band.green, kTheme.band.blue);
	glBegin(GL_QUADS);
	glVertex2f(0, bandTop);
	glVertex2f(width, bandTop);
	glVertex2f(width, height);
	glVertex2f(0, height);
	glEnd();

	glColorRGBA(kTheme.bandLine, kTheme.bandLine.alpha / 255.0f);
	glBegin(GL_LINES);
	glVertex2f(0, bandTop);
	glVertex2f(width, bandTop);
	glEnd();
}


void
AtomGLView::_DrawStars(float width, float height)
{
	float starAreaHeight = height - kBandHeight;
	glPointSize(1.6f);
	glBegin(GL_POINTS);
	for (int i = 0; i < kStarCount; i++) {
		float tw = 0.55f + 0.45f * sinf(fElapsed * fStars[i].speed
			+ fStars[i].phase);
		glColor4f(1.0f, 1.0f, 1.0f, 0.15f + 0.55f * tw);
		glVertex2f(fStars[i].x * width, fStars[i].y * starAreaHeight);
	}
	glEnd();
}


void
AtomGLView::_DrawAtom()
{
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_NORMALIZE);

	GLfloat lightPos[] = { 3.0f, 4.0f, 5.0f, 0.0f };
	GLfloat ambient[] = { 0.30f, 0.30f, 0.33f, 1.0f };
	GLfloat diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat specular[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 48.0f);

	glPushMatrix();
	// L'intero atomo ruota lentamente su se stesso, indipendentemente
	// dal moto orbitale di ciascun elettrone qui sotto.
	glRotatef(fElapsed * 11.0f, 0.0f, 1.0f, 0.0f);
	glRotatef(sinf(fElapsed * 0.35f) * 9.0f, 1.0f, 0.0f, 0.0f);

	glColorRGBA(kTheme.nucleus, 1.0f);
	gluSphere(fQuadric, 0.62, 28, 28);

	glDisable(GL_LIGHTING);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	_DrawGlowSphere(0.95f, kTheme.glow, 0.20f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE);
	glEnable(GL_LIGHTING);

	_DrawOrbit(0, 2.15f);
	_DrawOrbit(1, 2.15f);
	_DrawOrbit(2, 2.15f);

	glPopMatrix();

	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);
	glDisable(GL_COLOR_MATERIAL);
}


void
AtomGLView::_DrawGlowSphere(float radius, rgb_color color, float alpha)
{
	glColorRGBA(color, alpha);
	gluSphere(fQuadric, radius, 20, 20);
}


void
AtomGLView::_DrawOrbit(int index, float radius)
{
	static const float kElectronSpeed[3] = { 95.0f, -118.0f, 132.0f };
	static const float kPhase[3] = { 0.0f, 2.1f, 4.3f };

	glPushMatrix();
	glRotatef(60.0f * index, 0.0f, 0.0f, 1.0f);
	glRotatef(68.0f, 1.0f, 0.0f, 0.0f);

	glDisable(GL_LIGHTING);
	glLineWidth(1.8f);
	glColorRGBA(kTheme.orbit, 0.55f);
	glBegin(GL_LINE_LOOP);
	for (int a = 0; a < 360; a += 3) {
		float rad = a * (3.14159265f / 180.0f);
		glVertex3f(radius * cosf(rad), radius * sinf(rad), 0.0f);
	}
	glEnd();
	glEnable(GL_LIGHTING);

	float eAngle = fElapsed * kElectronSpeed[index] * (3.14159265f / 180.0f)
		+ kPhase[index];
	float ex = radius * cosf(eAngle);
	float ey = radius * sinf(eAngle);

	glPushMatrix();
	glTranslatef(ex, ey, 0.0f);
	glColorRGBA(kTheme.electron, 1.0f);
	gluSphere(fQuadric, 0.15, 16, 16);

	glDisable(GL_LIGHTING);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	_DrawGlowSphere(0.32f, kTheme.glow, 0.35f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_TRUE);
	glEnable(GL_LIGHTING);

	glPopMatrix();
	glPopMatrix();
}


void
AtomGLView::_DrawText(float width, float height)
{
	float fade = fElapsed / kTextFadeDuration;
	if (fade > 1.0f)
		fade = 1.0f;
	// Ease-out: il testo si assesta invece di arrivare linearmente.
	fade = 1.0f - (1.0f - fade) * (1.0f - fade);

	float rise = (1.0f - fade) * 14.0f; // leggera deriva verso l'alto durante la dissolvenza

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// Allineato a sinistra nella striscia inferiore (vedi
	// _DrawBackground): titolo/sottotitolo restano a un margine fisso
	// dal bordo sinistro invece che centrati, per leggersi come un
	// credito a piè di pagina.
	float bandTop = height - kBandHeight;
	const float titleHeight = 54.0f;
	const float subtitleHeight = 22.0f;

	_DrawTexturedQuad(fTitleTex,
		kBandPadding + titleHeight * fTitleTex.AspectRatio() * 0.5f,
		bandTop + 32.0f + rise, titleHeight, kTheme.title, fade);
	_DrawTexturedQuad(fSubtitleTex,
		kBandPadding + subtitleHeight * fSubtitleTex.AspectRatio() * 0.5f,
		bandTop + 71.0f + rise * 0.6f, subtitleHeight, kTheme.subtitle, fade);

	// Versione, speculare al titolo/credito: allineata a destra invece
	// che a sinistra, stesso margine kBandPadding, centrata in verticale
	// nella striscia invece di impilata su due righe (e' una sola riga
	// corta, non ha bisogno di altro spazio).
	if (fVersionTex.IsValid())
	{
		const float versionHeight = 22.0f;
		float w = versionHeight * fVersionTex.AspectRatio();
		_DrawTexturedQuad(fVersionTex, width - kBandPadding - w * 0.5f,
			bandTop + kBandHeight * 0.5f + rise * 0.6f, versionHeight, kTheme.subtitle, fade);
	}

	glDisable(GL_TEXTURE_2D);
}


void
AtomGLView::_DrawTexturedQuad(const TextTexture& tex, float centerX,
	float centerY, float pixelHeight, rgb_color tint, float alpha)
{
	if (!tex.IsValid())
		return;

	float h = pixelHeight;
	float w = h * tex.AspectRatio();

	glBindTexture(GL_TEXTURE_2D, tex.TextureID());
	glColorRGBA(tint, alpha);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(centerX - w * 0.5f, centerY - h * 0.5f);
	glTexCoord2f(1, 0); glVertex2f(centerX + w * 0.5f, centerY - h * 0.5f);
	glTexCoord2f(1, 1); glVertex2f(centerX + w * 0.5f, centerY + h * 0.5f);
	glTexCoord2f(0, 1); glVertex2f(centerX - w * 0.5f, centerY + h * 0.5f);
	glEnd();
}


void
AtomGLView::_DrawFadeOut(float width, float height)
{
	float t = fElapsed - kVisibleDuration;
	if (t <= 0.0f)
		return;

	float alpha = t / kFadeOutDuration;
	if (alpha > 1.0f)
		alpha = 1.0f;

	// Semplice overlay nero su tutto il frame -- il modo piu' semplice
	// per far sparire tutto insieme (atomo, striscia, testo) senza
	// toccare l'alpha di ogni singola chiamata di disegno; la vera
	// chiusura della finestra avviene in _Tick() quando l'overlay
	// arriva a coprire tutto.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glColor4f(0.0f, 0.0f, 0.0f, alpha);
	glBegin(GL_QUADS);
	glVertex2f(0, 0);
	glVertex2f(width, 0);
	glVertex2f(width, height);
	glVertex2f(0, height);
	glEnd();
}
