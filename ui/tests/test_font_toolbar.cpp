/*
	test_font_toolbar.cpp

	Verifica il selettore famiglia/dimensione carattere nella toolbar
	(fFontFamilyField/fFontSizeField, MainWindow.cpp -- richiesta
	esplicita dell'utente: "come per Excel, un menu sulla barra delle
	icone per selezionare il font e l'altezza del testo"). Stessa
	ragione headless-ma-con-vera-MainWindow di test_format_toolbar.cpp:
	si passa dal vero BMenuField/BMenuItem (FindView + Menu() +
	Message()), recapitato a MainWindow::MessageReceived() come farebbe
	la BLooper reale, non da una chiamata diretta a SetFontFamily/
	SetFontSize (gia' verificata indirettamente da questo stesso
	percorso).
*/

#include <cstdio>
#include <cstring>
#include <math.h>

#include <Application.h>
#include <Font.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Message.h>

#include "Cell.h"
#include "Range.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "FontMetrics.h"
#include "SheetView.h"
#include "MainWindow.h"

static int gFailures = 0;

static void Check(bool condition, const char* what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

static void GetFontInfo(CContainer* doc, cell c, font_family* family,
	font_style* style, float* size)
{
	CellStyle cs;
	doc->GetCellStyle(c, cs);
	rgb_color color;
	gFontSizeTable.GetFontInfo(cs.fFont, family, style, size, &color);
}

// Trova la voce di menu la cui etichetta e' "label" (famiglia) e la
// invia a MainWindow come farebbe un vero clic sul menu a tendina.
static void PickMenuItemByLabel(MainWindow* win, BMenuField* field, const char* label)
{
	BMenu* menu = field->Menu();
	for (int32 i = 0; i < menu->CountItems(); i++)
	{
		BMenuItem* item = menu->ItemAt(i);
		if (strcmp(item->Label(), label) == 0)
		{
			BMessage msg(*item->Message());
			win->MessageReceived(&msg);
			return;
		}
	}
}

// Trova la voce di menu la cui dimensione (campo "size" del BMessage)
// e' quella data e la invia, stesso principio di PickMenuItemByLabel.
static void PickMenuItemBySize(MainWindow* win, BMenuField* field, float size)
{
	BMenu* menu = field->Menu();
	for (int32 i = 0; i < menu->CountItems(); i++)
	{
		BMenuItem* item = menu->ItemAt(i);
		float itemSize;
		if (item->Message() && item->Message()->FindFloat("size", &itemSize) == B_OK
			&& itemSize == size)
		{
			BMessage msg(*item->Message());
			win->MessageReceived(&msg);
			return;
		}
	}
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestFontToolbar");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("Ciao", cell(1, 1), doc, true); // A1
	TryToParseString("Mondo", cell(1, 2), doc, true); // A2, mai selezionata/formattata

	BMenuField* familyField = dynamic_cast<BMenuField*>(win->FindView("fontFamilyField"));
	BMenuField* sizeField = dynamic_cast<BMenuField*>(win->FindView("fontSizeField"));
	Check(familyField != NULL && sizeField != NULL,
		"fFontFamilyField/fFontSizeField esistono davvero nella toolbar");
	Check(familyField->Menu()->CountItems() == count_font_families(),
		"il menu famiglia contiene esattamente le famiglie installate su questo sistema (count_font_families)");
	Check(sizeField->Menu()->CountItems() == 14,
		"il menu dimensione contiene la lista fissa di 14 taglie (come Excel)");

	// be_plain_font e' sempre installato (e' il font predefinito del
	// sistema stesso): scelta sicura per una voce che esiste di sicuro
	// nel menu famiglia, senza dipendere da quali font specifici questo
	// sandbox ha installato.
	font_family plainFamily;
	font_style plainStyle;
	be_plain_font->GetFamilyAndStyle(&plainFamily, &plainStyle);

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));

	PickMenuItemByLabel(win, familyField, plainFamily);
	font_family family;
	font_style style;
	float size;
	GetFontInfo(doc, cell(1, 1), &family, &style, &size);
	Check(strcmp(family, plainFamily) == 0,
		"scegliere una famiglia dal menu la applica davvero ad A1");
	Check(!strstr(style, "Bold") && !strstr(style, "Italic"),
		"cambiare famiglia non tocca stile/dimensione gia' presenti su A1 (preservati da SetFontFamily)");

	PickMenuItemBySize(win, sizeField, 24);
	GetFontInfo(doc, cell(1, 1), &family, &style, &size);
	Check(size == 24, "scegliere \"24\" dal menu dimensione lo applica davvero ad A1");
	Check(strcmp(family, plainFamily) == 0,
		"cambiare dimensione non tocca la famiglia appena scelta sopra (preservata da SetFontSize)");

	// Stesso principio di ToggleBold in test_format_toolbar.cpp: una
	// selezione di una sola cella (A1) non deve toccare A2, mai
	// selezionata/formattata.
	GetFontInfo(doc, cell(1, 2), &family, &style, &size);
	Check(size != 24,
		"cambiare la dimensione di A1 (selezione singola) non tocca A2, mai selezionata");

	// UpdateFontFields (chiamato da SelectionChanged): il menu deve
	// riflettere il font della cella appena selezionata, non restare
	// fermo sull'ultima scelta fatta altrove.
	PickMenuItemBySize(win, sizeField, 18);
	view->SetSelection(cell(2, 1)); // B1, mai toccata: resta al font di default
	view->ExtendSelection(cell(2, 1));
	win->SelectionChanged(cell(2, 1));

	font_family defaultFamily;
	font_style defaultStyle;
	float defaultSize;
	GetFontInfo(doc, cell(2, 1), &defaultFamily, &defaultStyle, &defaultSize);

	BMenuItem* markedFamily = familyField->Menu()->FindMarked();
	Check(markedFamily != NULL && strcmp(markedFamily->Label(), defaultFamily) == 0,
		"spostare la selezione su B1 (mai formattata) aggiorna il menu famiglia al suo font reale");

	BMenuItem* markedSize = sizeField->Menu()->FindMarked();
	float markedSizeValue = -1;
	if (markedSize && markedSize->Message())
		markedSize->Message()->FindFloat("size", &markedSizeValue);
	Check(markedSize != NULL && fabs(markedSizeValue - defaultSize) < 0.5f,
		"spostare la selezione su B1 aggiorna anche il menu dimensione al suo valore reale");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
