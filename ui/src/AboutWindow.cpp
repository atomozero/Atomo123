/*
	AboutWindow.cpp

	Vedi AboutWindow.h.
*/

#include "AboutWindow.h"

#include "ClickableStringView.h"

#include <AppFileInfo.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <File.h>
#include <Font.h>
#include <fs_attr.h>
#include <GradientLinear.h>
#include <IconUtils.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Resources.h>
#include <Roster.h>
#include <SeparatorView.h>
#include <String.h>
#include <StringView.h>
#include <View.h>

static const uint32 kMsgClose = 'clse';
static const uint32 kMsgOpenLink = 'olnk';
static const uint32 kMsgOpenCoffeeLink = 'ocfl';

static const char* kProjectUrl = "https://github.com/atomozero/Atomo123";
static const char* kCoffeeUrl = "https://buymeacoffee.com/atomozero";

// Stessa identica tavolozza della finestra Informazioni di Brube2000
// (banner blu-ardesia con sfumatura), per coerenza visiva fra le due
// app dello stesso autore.
static const rgb_color kSlate    = { 40, 50, 65, 255 };
static const rgb_color kSlateTop = { 54, 66, 84, 255 };
static const rgb_color kTitleCol = { 245, 245, 245, 255 };
static const rgb_color kSubCol   = { 180, 195, 210, 255 };
static const rgb_color kTileFill = { 90, 155, 213, 255 };


// Rilegge l'icona incorporata dell'app (risorsa BEOS:ICON, vedi
// Atomo123.rdef) e la rasterizza in una BBitmap size x size --
// proprieta' del chiamante (puo' essere NULL se qualcosa fallisce).
static BBitmap* LoadAppIcon(float size)
{
	app_info info;
	if (be_app == NULL || be_app->GetAppInfo(&info) != B_OK)
		return NULL;
	BFile file(&info.ref, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return NULL;

	uint8* data = NULL;
	size_t len = 0;
	attr_info ai;
	if (file.GetAttrInfo("BEOS:ICON", &ai) == B_OK && ai.size > 0)
	{
		data = new uint8[ai.size];
		ssize_t n = file.ReadAttr("BEOS:ICON", B_VECTOR_ICON_TYPE, 0, data, ai.size);
		if (n == (ssize_t)ai.size)
			len = ai.size;
		else
		{
			delete[] data;
			data = NULL;
		}
	}

	BResources res(&file);
	const void* rdata = NULL;
	if (data == NULL)
	{
		rdata = res.LoadResource(B_VECTOR_ICON_TYPE, 101, &len);
		if (rdata == NULL || len == 0)
			return NULL;
	}

	const uint8* icon = data != NULL ? data : (const uint8*)rdata;
	BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
	status_t err = BIconUtils::GetVectorIcon(icon, len, bitmap);
	delete[] data;
	if (err != B_OK)
	{
		delete bitmap;
		return NULL;
	}
	return bitmap;
}

// "Versione X.Y.Z", letta dalla risorsa app_version (Atomo123.rdef)
// cosi' il testo riflette sempre il binario davvero compilato, non un
// numero scritto a mano qui che si disallineerebbe alla prima release
// dimenticata.
static BString AppVersionText()
{
	BString text;
	app_info info;
	version_info vi;
	if (be_app != NULL && be_app->GetAppInfo(&info) == B_OK)
	{
		BFile file(&info.ref, B_READ_ONLY);
		BAppFileInfo appInfo(&file);
		if (appInfo.GetVersionInfo(&vi, B_APP_VERSION_KIND) == B_OK)
		{
			text.SetToFormat("Versione %" B_PRIu32 ".%" B_PRIu32 ".%" B_PRIu32,
				vi.major, vi.middle, vi.minor);
		}
	}
	if (text.IsEmpty())
		text = "Versione";
	text << "  \xC2\xB7  per Haiku";
	return text;
}

// Il banner in cima: icona dell'app su un riquadro blu arrotondato,
// titolo e versione, su sfondo sfumato -- stesso disegno del banner
// di Brube2000.
class AboutHero : public BView {
public:
	AboutHero(const char* version)
		:
		BView("hero", B_WILL_DRAW),
		fVersion(version),
		fIcon(LoadAppIcon(56))
	{
		SetViewColor(kSlate);
		SetExplicitMinSize(BSize(B_SIZE_UNSET, 104));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, 104));
	}

	virtual ~AboutHero() { delete fIcon; }

	virtual void Draw(BRect)
	{
		BRect b = Bounds();

		BGradientLinear grad(BPoint(0, b.top), BPoint(0, b.bottom));
		grad.AddColor(kSlateTop, 0.0);
		grad.AddColor(kSlate, 255.0);
		FillRect(b, grad);

		float tileSize = 72;
		BRect tile(24, (b.Height() - tileSize) / 2, 24 + tileSize,
			(b.Height() - tileSize) / 2 + tileSize);
		SetHighColor(kTileFill);
		FillRoundRect(tile, 12, 12);
		if (fIcon != NULL)
		{
			SetDrawingMode(B_OP_ALPHA);
			SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
			float ix = tile.left + (tile.Width() - fIcon->Bounds().Width()) / 2;
			float iy = tile.top + (tile.Height() - fIcon->Bounds().Height()) / 2;
			DrawBitmap(fIcon, BPoint(ix, iy));
			SetDrawingMode(B_OP_OVER);
		}

		float tx = tile.right + 18;
		BFont title(be_bold_font);
		title.SetSize(24);
		SetFont(&title);
		SetHighColor(kTitleCol);
		DrawString("Atomo123", BPoint(tx, b.Height() / 2 - 2));

		BFont sub(be_plain_font);
		sub.SetSize(12);
		SetFont(&sub);
		SetHighColor(kSubCol);
		DrawString(fVersion.String(), BPoint(tx, b.Height() / 2 + 20));
	}

private:
	BString fVersion;
	BBitmap* fIcon;
};


AboutWindow::AboutWindow()
	:
	BWindow(BRect(0, 0, 460, 320), "Informazioni su Atomo123", B_TITLED_WINDOW,
		B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS)
{
	AboutHero* hero = new AboutHero(AppVersionText().String());

	BStringView* tagline = new BStringView("tagline", "Foglio di calcolo nativo per Haiku OS.");

	BStringView* features = new BStringView("features",
		"Formule e funzioni \xC2\xB7 import/export XLSX/XLSM/ODS/CSV/XLS \xC2\xB7 "
		"grafici e tabelle pivot \xC2\xB7 piu' fogli");
	BFont small(be_plain_font);
	small.SetSize(be_plain_font->Size() - 1);
	features->SetFont(&small);
	features->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.7));

	BStringView* author = new BStringView("author", "di Andrea Bernardi");
	BFont bold(be_bold_font);
	author->SetFont(&bold);

	ClickableStringView* link = new ClickableStringView("link", kProjectUrl);
	link->SetClickMessage(new BMessage(kMsgOpenLink));

	// Stesso link "Buy Me A Coffee" gia' presente nel README, stesso
	// meccanismo click-per-aprire del link al progetto sopra.
	ClickableStringView* coffeeLink = new ClickableStringView("coffeeLink",
		"Offrimi un caffe' \xE2\x98\x95");
	coffeeLink->SetClickMessage(new BMessage(kMsgOpenCoffeeLink));

	BStringView* thanks = new BStringView("thanks",
		"Icone della barra strumenti da hvif-store.art \xE2\x80\x94 grazie!");
	thanks->SetFont(&small);
	thanks->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.7));

	BStringView* license = new BStringView("license",
		"Codice nuovo su licenza MIT \xC2\xB7 motore derivato da Sum-It "
		"(BSD, Hekkelman Programmatuur).");
	license->SetFont(&small);
	license->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.6));

	BButton* ok = new BButton("ok", "OK", new BMessage(kMsgClose));
	ok->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(hero)
		.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS)
			.Add(tagline)
			.Add(features)
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(author)
			.Add(link)
			.Add(coffeeLink)
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(thanks)
			.Add(license)
			.AddStrut(B_USE_DEFAULT_SPACING)
			.Add(new BSeparatorView(B_HORIZONTAL))
			.AddGroup(B_HORIZONTAL, 0)
				.AddGlue()
				.Add(ok)
			.End()
		.End();

	// Invio diretto dalla tastiera oltre che dal pulsante: Esc chiude
	// come OK, stesso principio gia' usato dalle altre finestre di
	// utilita' di questo progetto (es. GoToWindow).
	AddShortcut(B_ESCAPE, 0, new BMessage(kMsgClose));
	CenterOnScreen();
}

void AboutWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgClose:
			Quit();
			break;

		case kMsgOpenLink:
		{
			const char* arg = kProjectUrl;
			be_roster->Launch("application/x-vnd.Be.URL.https", 1, const_cast<char**>(&arg));
			break;
		}

		case kMsgOpenCoffeeLink:
		{
			const char* arg = kCoffeeUrl;
			be_roster->Launch("application/x-vnd.Be.URL.https", 1, const_cast<char**>(&arg));
			break;
		}

		default:
			BWindow::MessageReceived(message);
	}
}
