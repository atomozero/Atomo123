/*
	IconCatalog.cpp

	Vedi IconCatalog.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "IconCatalog.h"

#include <Bitmap.h>
#include <IconUtils.h>
#include <Rect.h>

namespace {
	const int kIconSize = 16;
}

BBitmap* IconCatalog::Render(const IconData& icon)
{
	BBitmap* bitmap = new BBitmap(BRect(0, 0, kIconSize - 1, kIconSize - 1), B_RGBA32);
	if (BIconUtils::GetVectorIcon(icon.bytes, icon.length, bitmap) != B_OK)
	{
		delete bitmap;
		return NULL;
	}
	return bitmap;
}
