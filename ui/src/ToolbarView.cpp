/*
	ToolbarView.cpp

	Vedi ToolbarView.h.
*/

#include "ToolbarView.h"

#include <Button.h>
#include <MenuItem.h>
#include <Message.h>
#include <PopUpMenu.h>
#include <SeparatorView.h>
#include <Window.h>

static const float kInset = 4;
static const float kSpacing = 4;

// Pulsante ">>" del troppopieno: invoca direttamente
// ToolbarView::ShowOverflowMenu() da un override di Invoke(), invece
// di passare da BInvoker::SetTarget()/BMessenger come i pulsanti veri
// della toolbar (che puntano tutti a MainWindow, una BLooper valida
// fin dalla costruzione). ToolbarView e' "solo" una BView: il suo
// Looper() resta NULL finche' non viene agganciata alla finestra
// (avviene DOPO che questo pulsante e' gia' stato costruito, in
// BuildToolbar()), quindi un SetTarget(this) qui sarebbe stato
// costruito con un target ancora non risolvibile -- una chiamata C++
// diretta non ha invece nessun bisogno di un Looper valido, funziona
// a prescindere da quando/se la vista e' agganciata. Bug reale
// segnalato dall'utente ("il pulsante non mi mostra le icone
// nascoste"): il clic non arrivava mai a ShowOverflowMenu().
class ToolbarOverflowButton : public BButton {
public:
	ToolbarOverflowButton(ToolbarView* owner)
		:
		BButton("toolOverflow", ">>", NULL),
		fOwner(owner)
	{
	}

	virtual status_t Invoke(BMessage* message = NULL)
	{
		fOwner->ShowOverflowMenu();
		return B_OK;
	}

private:
	ToolbarView* fOwner;
};

ToolbarView::ToolbarView(const char* name)
	:
	BView(name, B_WILL_DRAW | B_FRAME_EVENTS),
	fOverflowing(false),
	fRowHeight(28),
	fNaturalWidth(kInset * 2)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

	// ">>" invece di un'icona vera: e' l'unico pulsante di questa
	// toolbar che non corrisponde a un comando del foglio, non ha
	// senso spendere un'altra icona del catalogo HVIF per lui.
	fOverflowButton = new ToolbarOverflowButton(this);
	fOverflowButton->SetToolTip("Altri pulsanti");
	fOverflowButton->SetFlat(true);
	AddChild(fOverflowButton);
	fOverflowButton->Hide();

	float w, h;
	fOverflowButton->GetPreferredSize(&w, &h);
	if (h + kInset * 2 > fRowHeight)
		fRowHeight = h + kInset * 2;
}

ToolbarView::~ToolbarView()
{
}

void ToolbarView::AttachedToWindow()
{
	BView::AttachedToWindow();
	Layout();
}

void ToolbarView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	Layout();
}

BSize ToolbarView::MinSize()
{
	// Larghezza minima piccola apposta: sotto questa soglia il
	// troppopieno nasconde tutto tranne il pulsante ">>" stesso, non
	// c'e' un vincolo piu' stretto da rispettare (a differenza di
	// SheetTabView, che deve sempre mostrare almeno una scheda).
	float w, h;
	fOverflowButton->GetPreferredSize(&w, &h);
	return BSize(w + kInset * 2, fRowHeight);
}

BSize ToolbarView::MaxSize()
{
	return BSize(B_SIZE_UNLIMITED, fRowHeight);
}

BSize ToolbarView::PreferredSize()
{
	return BSize(fNaturalWidth, fRowHeight);
}

void ToolbarView::AddButton(BButton* button, const char* label)
{
	AddChild(button);

	Item item;
	item.view = button;
	item.isSeparator = false;
	item.label = label;
	fItems.push_back(item);

	float w, h;
	button->GetPreferredSize(&w, &h);
	if (h + kInset * 2 > fRowHeight)
		fRowHeight = h + kInset * 2;
	fNaturalWidth += w + kSpacing;
}

void ToolbarView::AddSeparator()
{
	BSeparatorView* sep = new BSeparatorView(B_VERTICAL);
	AddChild(sep);

	Item item;
	item.view = sep;
	item.isSeparator = true;
	fItems.push_back(item);

	float w, h;
	sep->GetPreferredSize(&w, &h);
	fNaturalWidth += w + kSpacing;
}

int ToolbarView::VisibleButtonCount() const
{
	int count = 0;
	for (size_t i = 0; i < fItems.size(); i++)
	{
		if (!fItems[i].isSeparator && !fItems[i].view->IsHidden())
			count++;
	}
	return count;
}

int ToolbarView::HiddenButtonCount() const
{
	int count = 0;
	for (size_t i = 0; i < fItems.size(); i++)
	{
		if (!fItems[i].isSeparator && fItems[i].view->IsHidden())
			count++;
	}
	return count;
}

void ToolbarView::Layout()
{
	float availWidth = Bounds().Width();
	if (availWidth <= 0 || fItems.empty())
		return;

	// Prima passata: larghezza naturale di ogni elemento e totale
	// complessivo, per sapere se serve il troppopieno prima di
	// decidere dove tagliare.
	std::vector<BSize> sizes(fItems.size());
	float total = kInset;
	for (size_t i = 0; i < fItems.size(); i++)
	{
		float w, h;
		fItems[i].view->GetPreferredSize(&w, &h);
		sizes[i] = BSize(w, h);
		total += w;
		if (i + 1 < fItems.size())
			total += kSpacing;
	}

	fOverflowing = total > availWidth;

	float limit = availWidth - kInset;
	if (fOverflowing)
	{
		float ow, oh;
		fOverflowButton->GetPreferredSize(&ow, &oh);
		limit -= (ow + kSpacing);
	}

	float x = kInset;
	size_t firstHidden = fItems.size();
	for (size_t i = 0; i < fItems.size(); i++)
	{
		float w = sizes[i].Width();
		if (fOverflowing && x + w > limit)
		{
			firstHidden = i;
			break;
		}
		fItems[i].view->MoveTo(x, kInset);
		fItems[i].view->ResizeTo(w, sizes[i].Height());
		if (fItems[i].view->IsHidden())
			fItems[i].view->Show();
		x += w + kSpacing;
	}

	// Un separatore proprio subito prima del taglio non separa piu'
	// niente (quello che seguiva e' nascosto): lo si nasconde anche
	// lui, invece di lasciare un divisore penzolante appena prima del
	// pulsante ">>".
	while (firstHidden > 0 && fItems[firstHidden - 1].isSeparator)
		firstHidden--;

	for (size_t i = firstHidden; i < fItems.size(); i++)
	{
		if (!fItems[i].view->IsHidden())
			fItems[i].view->Hide();
	}

	if (fOverflowing)
	{
		// x e' rimasta ferma all'ultimo elemento visibile se il primo
		// nascosto era un separatore appena tolto sopra: ricalcolarla
		// dall'ultimo elemento davvero visibile, non serve rifare
		// tutto il ciclo per un caso cosi' raro.
		if (firstHidden > 0)
		{
			BRect lastRect = fItems[firstHidden - 1].view->Frame();
			x = lastRect.right + kSpacing;
		}
		else
			x = kInset;

		float ow, oh;
		fOverflowButton->GetPreferredSize(&ow, &oh);
		fOverflowButton->MoveTo(x, kInset);
		fOverflowButton->ResizeTo(ow, oh);
		if (fOverflowButton->IsHidden())
			fOverflowButton->Show();
	}
	else if (!fOverflowButton->IsHidden())
	{
		fOverflowButton->Hide();
	}
}

void ToolbarView::ShowOverflowMenu()
{
	BPopUpMenu* menu = new BPopUpMenu("toolbarOverflow", false, false);

	for (size_t i = 0; i < fItems.size(); i++)
	{
		if (fItems[i].isSeparator || !fItems[i].view->IsHidden())
			continue;

		BButton* button = static_cast<BButton*>(fItems[i].view);
		if (!button->Message())
			continue;

		BMenuItem* item = new BMenuItem(fItems[i].label.String(),
			new BMessage(*button->Message()));
		item->SetTarget(button->Target());
		menu->AddItem(item);
	}

	if (menu->CountItems() == 0)
	{
		delete menu;
		return;
	}

	// Asincrono (l'ultimo "true" in Go) senza SetAsyncAutoDestruct()
	// non elimina l'oggetto da solo alla chiusura: perderebbe memoria
	// a ogni apertura del menu, non solo alla prima.
	menu->SetAsyncAutoDestruct(true);
	BPoint screenPoint = fOverflowButton->ConvertToScreen(
		fOverflowButton->Bounds().LeftBottom());
	menu->Go(screenPoint, true, true, true);
}
