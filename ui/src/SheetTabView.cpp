/*
	SheetTabView.cpp

	Vedi SheetTabView.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "SheetTabView.h"

#include <Catalog.h>
#include <ControlLook.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <Looper.h>
#include <MenuItem.h>
#include <Message.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <Window.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SheetTabView"

static const float kTabPadding = 14;
static const float kMinTabWidth = 50;
static const float kArrowWidth = 18;
static const float kTabHeight = 22;

SheetTabView::SheetTabView(const char* name, uint32 switchWhat, BHandler* target)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS),
	fActiveIndex(0),
	fFirstVisible(0),
	fScrolling(false),
	fSwitchWhat(switchWhat),
	fTarget(target)
{
	SetExplicitMinSize(BSize(20, kTabHeight));
	SetExplicitPreferredSize(BSize(B_SIZE_UNSET, kTabHeight));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, kTabHeight));
}

SheetTabView::~SheetTabView()
{
}

BSize SheetTabView::MinSize()
{
	return BSize(20, kTabHeight);
}

BSize SheetTabView::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, kTabHeight);
}

BSize SheetTabView::PreferredSize()
{
	return BSize(200, kTabHeight);
}

void SheetTabView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	Invalidate();
}

void SheetTabView::SetSheets(const std::vector<BString>& names, int activeIndex,
	const std::vector<bool>* hasColor, const std::vector<rgb_color>* colors)
{
	fTabs.clear();
	BFont font;
	GetFont(&font);
	for (size_t i = 0; i < names.size(); i++)
	{
		TabInfo t;
		t.name = names[i];
		t.width = font.StringWidth(t.name.String()) + kTabPadding * 2;
		if (t.width < kMinTabWidth)
			t.width = kMinTabWidth;
		t.hasColor = hasColor && i < hasColor->size() && (*hasColor)[i];
		t.color = { 0, 0, 0, 255 };
		if (t.hasColor && colors && i < colors->size())
			t.color = (*colors)[i];
		fTabs.push_back(t);
	}
	fActiveIndex = activeIndex;

	Layout();
	if (activeIndex >= 0 && !IsIndexVisible(activeIndex))
	{
		fFirstVisible = activeIndex;
		Layout();
	}

	Invalidate();
}

bool SheetTabView::IsIndexVisible(int index) const
{
	for (size_t i = 0; i < fVisible.size(); i++)
	{
		if (fVisible[i].index == index)
			return true;
	}
	return false;
}

BRect SheetTabView::TabRectFor(int index) const
{
	for (size_t i = 0; i < fVisible.size(); i++)
	{
		if (fVisible[i].index == index)
			return fVisible[i].rect;
	}
	return BRect(); // non visibile con l'attuale fFirstVisible: rettangolo non valido
}

BRect SheetTabView::LeftArrowRect() const
{
	BRect b = Bounds();
	return BRect(b.left, b.top, b.left + kArrowWidth, b.bottom);
}

BRect SheetTabView::RightArrowRect() const
{
	BRect b = Bounds();
	return BRect(b.left + kArrowWidth, b.top, b.left + kArrowWidth * 2, b.bottom);
}

void SheetTabView::Layout()
{
	fVisible.clear();

	if (fTabs.empty())
	{
		fScrolling = false;
		fFirstVisible = 0;
		return;
	}

	BRect b = Bounds();
	float total = 0;
	for (size_t i = 0; i < fTabs.size(); i++)
		total += fTabs[i].width;

	fScrolling = total > b.Width();

	if (!fScrolling)
		fFirstVisible = 0;
	else
	{
		if (fFirstVisible < 0)
			fFirstVisible = 0;
		if (fFirstVisible >= (int)fTabs.size())
			fFirstVisible = (int)fTabs.size() - 1;
	}

	float x = fScrolling ? kArrowWidth * 2 : 0;
	float maxX = b.right;

	for (int i = fFirstVisible; i < (int)fTabs.size(); i++)
	{
		float w = fTabs[i].width;
		// La prima scheda si disegna comunque anche se non ci sta
		// tutta (meglio tagliata che nessuna scheda visibile).
		if (x + w > maxX && !fVisible.empty())
			break;
		VisibleTab v;
		v.index = i;
		v.rect = BRect(x, b.top, x + w, b.bottom);
		fVisible.push_back(v);
		x += w;
	}
}

void SheetTabView::Draw(BRect updateRect)
{
	Layout();

	rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);

	BRect b = Bounds();
	SetHighColor(base);
	FillRect(b);

	// Frecce di scorrimento (Fase 33, BControlLook): stesso disegno
	// nativo usato da ogni ScrollBar/BTabView di sistema, invece di due
	// triangoli con colori RGB fissi che restavano grigi anche sotto un
	// tema scuro -- vedi il commento sopra SheetTabView.h sul perche'
	// non e' un vero BTabView (il resto -- scorrimento, colore per
	// foglio, menu contestuale -- non cambia).
	if (fScrolling)
	{
		BRect leftRect = LeftArrowRect();
		BRect rightRect = RightArrowRect();
		bool canLeft = fFirstVisible > 0;
		bool canRight = !fVisible.empty()
			&& fVisible.back().index < (int)fTabs.size() - 1;

		be_control_look->DrawArrowShape(this, leftRect, updateRect, base,
			BControlLook::B_LEFT_ARROW, canLeft ? 0 : BControlLook::B_DISABLED);
		be_control_look->DrawArrowShape(this, rightRect, updateRect, base,
			BControlLook::B_RIGHT_ARROW, canRight ? 0 : BControlLook::B_DISABLED);
	}

	BFont font;
	GetFont(&font);
	font_height fh;
	font.GetHeight(&fh);

	// Posizione (dentro fVisible, non l'indice assoluto del foglio) della
	// scheda attiva nella striscia CORRENTEMENTE visibile -- BControlLook
	// la usa per disegnare correttamente il bordo condiviso fra una
	// scheda attiva e le sue vicine (niente doppio bordo). -1 se la
	// scheda attiva non e' fra quelle visibili in questo momento
	// (scorsa fuori vista): nessuna "selected" in quel caso.
	int32 selectedPos = -1;
	for (size_t j = 0; j < fVisible.size(); j++)
	{
		if (fVisible[j].index == fActiveIndex)
		{
			selectedPos = (int32)j;
			break;
		}
	}

	for (size_t v = 0; v < fVisible.size(); v++)
	{
		const VisibleTab& tab = fVisible[v];
		const TabInfo& info = fTabs[tab.index];
		bool active = tab.index == fActiveIndex;

		BRect rect = tab.rect; // copia: DrawActiveTab/DrawInactiveTab la modificano (rimpiccioliscono al bordo interno)
		// "side" = B_BOTTOM_BORDER, non B_TOP_BORDER (bug reale segnalato
		// dall'utente guardando lo screenshot: le schede sembravano
		// "capovolte", attaccate al pavimento della striscia invece che
		// al soffitto): questa striscia sta in FONDO alla finestra, con
		// il foglio sopra e il footer sotto -- l'opposto di un BTabView
		// tipico (schede sopra, contenuto sotto). "side" indica il
		// bordo ESTERNO della striscia (quello lontano dal contenuto
		// vero), quindi va sul lato opposto a dove sta il foglio.
		//
		// La scheda ATTIVA in piu' omette il bordo SUPERIORE (quello
		// verso il foglio): e' il modo in cui una scheda "attaccata al
		// soffitto" si fonde visivamente col contenuto sopra di lei,
		// invece di restare un riquadro chiuso su tutti e quattro i
		// lati come le schede non attive -- cambiare solo "side" da
		// solo (tentativo precedente) non bastava, il parametro
		// "borders" con B_ALL_BORDERS non omette mai nulla di suo.
		if (active)
		{
			be_control_look->DrawActiveTab(this, rect, updateRect, base, 0,
				BControlLook::B_ALL_BORDERS & ~BControlLook::B_TOP_BORDER,
				BControlLook::B_BOTTOM_BORDER,
				(int32)v, selectedPos, 0, (int32)fVisible.size() - 1);
		}
		else
		{
			be_control_look->DrawInactiveTab(this, rect, updateRect, base, 0,
				BControlLook::B_ALL_BORDERS, BControlLook::B_BOTTOM_BORDER,
				(int32)v, selectedPos, 0, (int32)fVisible.size() - 1);
		}

		// Scheda colorata (import XLSX, <sheetPr><tabColor>): una barra
		// d'accento sotto il testo, MAI un riempimento a piena scheda
		// (proposta discussa e approvata dall'utente) -- BControlLook
		// deriva le sue sfumature da un "colore base" del tema, non da
		// un RGB arbitrario per scheda, quindi il colore-per-foglio non
		// puo' piu' sostituire il colore di sfondo nativo come faceva
		// prima. Stesso trattamento per la scheda attiva e per quelle
		// non attive, a differenza di prima (solo l'attiva aveva la
		// barra, le altre avevano il riempimento pieno).
		if (info.hasColor)
		{
			BRect bar(tab.rect.left + 3, tab.rect.bottom - 3, tab.rect.right - 3, tab.rect.bottom - 1);
			SetHighColor(info.color);
			FillRect(bar);
		}

		SetHighColor(active ? ui_color(B_PANEL_TEXT_COLOR)
			: tint_color(ui_color(B_PANEL_TEXT_COLOR), B_LIGHTEN_1_TINT));

		float textY = tab.rect.top
			+ (tab.rect.Height() - (fh.ascent + fh.descent)) / 2 + fh.ascent;
		BRect textClip = tab.rect.InsetByCopy(kTabPadding - 4, 0);
		ConstrainClippingRegion(NULL);
		BRegion clipRegion(textClip);
		ConstrainClippingRegion(&clipRegion);
		DrawString(fTabs[tab.index].name.String(),
			BPoint(tab.rect.left + kTabPadding, textY));
		ConstrainClippingRegion(NULL);
	}
}

void SheetTabView::MouseDown(BPoint where)
{
	if (fScrolling)
	{
		if (LeftArrowRect().Contains(where))
		{
			if (fFirstVisible > 0)
			{
				fFirstVisible--;
				Invalidate();
			}
			return;
		}
		if (RightArrowRect().Contains(where))
		{
			if (!fVisible.empty() && fVisible.back().index < (int)fTabs.size() - 1)
			{
				fFirstVisible++;
				Invalidate();
			}
			return;
		}
	}

	// Tasto destro: menu contestuale (Fase 13) invece del cambio scheda
	// -- stesso principio di "buttons"/"modifiers" gia' letti da
	// MainWindow su Window()->CurrentMessage() per Maiusc+click, qui
	// per il tasto secondario invece del tasto di Maiuscole. Sincrono
	// (asynchronous=false), stesso schema di
	// SheetView::ShowAutoFilterMenu: Go() blocca finche' l'utente non
	// sceglie una voce, restituendola direttamente, niente passaggio
	// di messaggi da gestire altrove.
	int32 buttons = 0;
	BMessage* msg = Window() ? Window()->CurrentMessage() : NULL;
	if (msg)
		msg->FindInt32("buttons", &buttons);

	for (size_t i = 0; i < fVisible.size(); i++)
	{
		if (fVisible[i].rect.Contains(where))
		{
			if (buttons & B_SECONDARY_MOUSE_BUTTON)
			{
				BPopUpMenu menu("sheetTabContext");
				BMenuItem* renameItem = new BMenuItem(B_TRANSLATE("Rinomina foglio" B_UTF8_ELLIPSIS), NULL);
				menu.AddItem(renameItem);
				BMenuItem* deleteItem = new BMenuItem(B_TRANSLATE("Elimina foglio"), NULL);
				menu.AddItem(deleteItem);

				BPoint screenAnchor = where;
				ConvertToScreen(&screenAnchor);
				BMenuItem* chosen = menu.Go(screenAnchor, false, false, true);

				if (chosen && fTarget)
				{
					BMessage request(chosen == renameItem
						? kMsgRenameSheetRequest : kMsgDeleteSheetRequest);
					request.AddInt32("index", fVisible[i].index);
					BMessenger(fTarget).SendMessage(&request);
				}
			}
			else if (fTarget)
			{
				BMessage switchMsg(fSwitchWhat);
				switchMsg.AddInt32("index", fVisible[i].index);
				BMessenger(fTarget).SendMessage(&switchMsg);
			}
			break;
		}
	}
}
