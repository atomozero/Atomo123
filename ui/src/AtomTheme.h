// AtomTheme.h
//
// Color palette for the splash screen. Variant "Blu Profondo", scelta
// dopo averla confrontata dal vivo con altre due varianti (Studio
// Chiaro, Notte Ambra) poi scartate.
//
// Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
// radice del repository).

#ifndef ATOM_THEME_H
#define ATOM_THEME_H

#include <GraphicsDefs.h>

struct AtomTheme {
	// Full-screen background gradient (top -> bottom).
	rgb_color	bgTop;
	rgb_color	bgBottom;

	// The atom itself.
	rgb_color	nucleus;
	rgb_color	orbit;		// ring color
	rgb_color	electron;
	rgb_color	glow;		// additive glow tint shared by nucleus/electrons

	// Text.
	rgb_color	title;
	rgb_color	subtitle;

	// Bottom band: a starfield-free footer strip holding the title/
	// subtitle, left-aligned, separated from the starfield above it by
	// a thin line in bandLine.
	rgb_color	band;
	rgb_color	bandLine;

	bool		stars;		// faint starfield dots, only looks right on dark bg
};

extern const AtomTheme kTheme;

#endif // ATOM_THEME_H
