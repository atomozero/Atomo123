/*
	IconCatalog.cpp

	Vedi IconCatalog.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "IconCatalog.h"

#include <cstring>

#include <Bitmap.h>
#include <IconUtils.h>
#include <Rect.h>
#include <View.h>

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

// Estratta da MainWindow.cpp (era un RenderCustomIcon file-locale usato
// solo dai pittogrammi della toolbar): stessa identica implementazione,
// ora riusabile anche da ChartWindow per le icone del tipo di grafico
// (vedi il commento in IconCatalog.h sul perche').
BBitmap* IconCatalog::RenderCustom(void (*draw)(BView*))
{
	const int kSize = 16;
	const int kSuper = 4;
	const int kBigSize = kSize * kSuper;

	BBitmap* big = new BBitmap(BRect(0, 0, kBigSize - 1, kBigSize - 1),
		B_RGBA32, true);
	if (!big || big->InitCheck() != B_OK) {
		delete big;
		return NULL;
	}
	uint8* bigBits = (uint8*)big->Bits();
	if (!bigBits) {
		delete big;
		return NULL;
	}
	memset(bigBits, 0, big->BitsLength());

	BView* painter = new BView(big->Bounds(), "customIconBig",
		B_FOLLOW_NONE, B_SUBPIXEL_PRECISE);
	if (!painter) {
		delete big;
		return NULL;
	}
	big->AddChild(painter);
	if (big->Lock()) {
		painter->SetScale(kSuper);
		draw(painter);
		painter->Sync();
		big->Unlock();
	}
	big->RemoveChild(painter);
	delete painter;

	BBitmap* bitmap = new BBitmap(BRect(0, 0, kSize - 1, kSize - 1),
		B_RGBA32, true);
	if (!bitmap || bitmap->InitCheck() != B_OK) {
		delete big;
		delete bitmap;
		return NULL;
	}
	uint8* bits = (uint8*)bitmap->Bits();
	if (!bits) {
		delete big;
		delete bitmap;
		return NULL;
	}
	int32 bigStride = big->BytesPerRow();
	int32 stride = bitmap->BytesPerRow();
	// Media pesata sull'alpha (non una semplice media RGB): un pixel
	// completamente trasparente nel blocco 4x4 non deve schiarire il
	// colore del pittogramma, solo abbassarne la copertura (l'alpha
	// del pixel risultante) -- altrimenti i bordi vengono grigiastri
	// invece che di un nero pieno che sfuma verso il trasparente.
	for (int y = 0; y < kSize; y++)
	{
		for (int x = 0; x < kSize; x++)
		{
			uint32 sumB = 0, sumG = 0, sumR = 0, sumA = 0;
			for (int sy = 0; sy < kSuper; sy++)
			{
				const uint8* row = bigBits
					+ (y * kSuper + sy) * bigStride + (x * kSuper) * 4;
				for (int sx = 0; sx < kSuper; sx++)
				{
					const uint8* p = row + sx * 4;
					uint8 a = p[3];
					sumB += p[0] * a;
					sumG += p[1] * a;
					sumR += p[2] * a;
					sumA += a;
				}
			}
			uint8* out = bits + y * stride + x * 4;
			if (sumA > 0)
			{
				out[0] = (uint8)(sumB / sumA);
				out[1] = (uint8)(sumG / sumA);
				out[2] = (uint8)(sumR / sumA);
			}
			else
				out[0] = out[1] = out[2] = 0;
			out[3] = (uint8)(sumA / (kSuper * kSuper));
		}
	}
	delete big;
	return bitmap;
}
