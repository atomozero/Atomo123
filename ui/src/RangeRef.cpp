/*
	RangeRef.cpp

	Vedi RangeRef.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "RangeRef.h"

#include <cstdio>
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

// Stessa logica di MainWindow::ColumnName/SheetView::ColumnName (vedi
// il commento li' per il perche' della duplicazione: e' una manciata
// di righe, non vale la pena condividerla per un dettaglio cosi'
// piccolo attraverso tre file diversi).
static void ColumnName(int col, char* out)
{
	char buf[8];
	int n = 0;
	while (col > 0)
	{
		int rem = (col - 1) % 26;
		buf[n++] = 'A' + rem;
		col = (col - 1) / 26;
	}
	for (int i = 0; i < n; i++)
		out[i] = buf[n - 1 - i];
	out[n] = 0;
}

void FormatRangeRef(const range& r, char* out, size_t outSize)
{
	char topLeft[16];
	ColumnName(r.left, topLeft);
	int len = (int)strlen(topLeft);
	snprintf(topLeft + len, sizeof(topLeft) - len, "%d", r.top);

	if (r.left == r.right && r.top == r.bottom)
	{
		snprintf(out, outSize, "%s", topLeft);
		return;
	}

	char botRight[16];
	ColumnName(r.right, botRight);
	len = (int)strlen(botRight);
	snprintf(botRight + len, sizeof(botRight) - len, "%d", r.bottom);

	snprintf(out, outSize, "%s:%s", topLeft, botRight);
}
