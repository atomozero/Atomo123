/*
	RangeRef.cpp

	Vedi RangeRef.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "RangeRef.h"

#include <cstring>

#include "Cell.h"
#include "Range.h"

// "A1:B5" -> due riferimenti a cella separati da ':', analizzati con
// cell::GetCell (lo stesso parser gia' usato dal motore per i
// riferimenti nelle formule, cosi' si accetta esattamente la stessa
// sintassi). "A1" da solo e' un intervallo di una sola cella.
bool ParseRangeRef(const char* text, range& outRange)
{
	if (!text || !text[0])
		return false;

	const char* colon = strchr(text, ':');

	cell a, b;
	if (!cell::GetCell(text, a))
		return false;

	if (colon)
	{
		if (!cell::GetCell(colon + 1, b))
			return false;
	}
	else
	{
		b = a;
	}

	outRange.left = a.h < b.h ? a.h : b.h;
	outRange.right = a.h > b.h ? a.h : b.h;
	outRange.top = a.v < b.v ? a.v : b.v;
	outRange.bottom = a.v > b.v ? a.v : b.v;
	return true;
}
