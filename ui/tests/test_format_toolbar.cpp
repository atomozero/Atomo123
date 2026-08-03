/*
	test_format_toolbar.cpp

	Verifica i nove pulsanti del gruppo "Formato" nella toolbar
	(Grassetto/Corsivo/Sottolineato/Allinea sinistra-centro-destra/A
	capo automatico/Colore testo/Colore sfondo), aggiunti quando il
	catalogo HVIF ha finalmente coperto anche queste funzioni gia'
	esistenti (fino ad ora solo voci del menu Formato, mai promosse a
	pulsante per mancanza di un'icona reale -- vedi Atomo123_icons/
	ATOMO123.md e kFormatToolbarButtons in MainWindow.cpp). Non basta
	che l'icona sia quella giusta: il BMessage dietro il pulsante deve
	produrre lo stesso effetto del comando di menu equivalente, gia'
	verificato in test_format.cpp con una chiamata diretta a
	MainWindow::ToggleBold()/SetAlignment() ecc. -- qui si passa invece
	dal vero BButton (FindView + Message()), recapitato a
	MainWindow::MessageReceived() come farebbe la BLooper reale.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Button.h>
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

static bool StyleHas(CContainer* doc, cell c, const char* what)
{
	CellStyle cs;
	doc->GetCellStyle(c, cs);
	font_family family;
	font_style style;
	float size;
	gFontSizeTable.GetFontInfo(cs.fFont, &family, &style, &size);
	return strstr(style, what) != NULL;
}

static BButton* FindToolButton(MainWindow* win, const char* name)
{
	return dynamic_cast<BButton*>(win->FindView(name));
}

static void ClickButton(MainWindow* win, BButton* button)
{
	if (button && button->Message())
	{
		BMessage msg(*button->Message());
		win->MessageReceived(&msg);
	}
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestFormatToolbar");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	TryToParseString("1", cell(1, 1), doc, true); // A1
	TryToParseString("2", cell(1, 2), doc, true); // A2, mai selezionata/formattata
	TryToParseString("3", cell(2, 1), doc, true); // B1, mai selezionata/formattata

	static const char* kNames[] = {
		"toolBold", "toolItalic", "toolUnderline",
		"toolAlignLeft", "toolAlignCenter", "toolAlignRight",
		"toolWrapText", "toolTextColor", "toolHighlight",
	};
	bool allFound = true;
	bool allTargeted = true;
	for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++)
	{
		BButton* button = FindToolButton(win, kNames[i]);
		if (!button)
		{
			allFound = false;
			continue;
		}
		BLooper* looper = NULL;
		BHandler* target = button->Target(&looper);
		if (target != win)
			allTargeted = false;
	}
	Check(allFound, "i nove pulsanti del gruppo Formato esistono tutti nella toolbar");
	Check(allTargeted, "tutti hanno MainWindow come target (SetTarget), non un handler a caso");

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 1));

	ClickButton(win, FindToolButton(win, "toolBold"));
	Check(StyleHas(doc, cell(1, 1), "Bold"),
		"il pulsante \"toolBold\" mette in grassetto A1, come il comando di menu equivalente");

	// Bug reale segnalato dall'utente: su un documento dove nessun
	// font era ancora stato richiesto tramite CFontSizeTable::GetFontID
	// (il caso comune, un documento nuovo), mettere in grassetto UNA
	// sola cella selezionata metteva in grassetto anche A2/B1, mai
	// selezionate ne' formattate -- il font "Bold" appena creato
	// finiva per occupare per errore l'indice 0 di gFontSizeTable,
	// lo stesso indice implicito di CellStyle::fFont per ogni cella
	// mai formattata esplicitamente (CellStyle::CellStyle() azzera
	// l'intera struct). Fix in CFontSizeTable::ReserveDefaultSlot()
	// (FontMetrics.cpp): l'indice 0 e' ora sempre riservato a un font
	// segnaposto "di default" prima che un font specifico possa
	// riceverlo per sbaglio.
	Check(!StyleHas(doc, cell(1, 2), "Bold"),
		"mettere in grassetto A1 (selezione singola) non tocca A2, mai selezionata");
	Check(!StyleHas(doc, cell(2, 1), "Bold"),
		"mettere in grassetto A1 (selezione singola) non tocca B1, mai selezionata");

	ClickButton(win, FindToolButton(win, "toolItalic"));
	Check(StyleHas(doc, cell(1, 1), "Italic"),
		"il pulsante \"toolItalic\" mette in corsivo A1 (in aggiunta al grassetto gia' attivo)");

	ClickButton(win, FindToolButton(win, "toolUnderline"));
	CellStyle cs;
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fUnderline, "il pulsante \"toolUnderline\" sottolinea A1");

	// I tre pulsanti di allineamento condividono lo stesso comando
	// (kMsgSetAlignment), si distinguono solo per il parametro
	// "alignment" incorporato nel BMessage di ciascuno -- verificarne
	// due in sequenza conferma che il parametro giusto viaggia col
	// pulsante giusto, non solo che il comando generico funziona.
	ClickButton(win, FindToolButton(win, "toolAlignRight"));
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fAlignment == eAlignRight, "il pulsante \"toolAlignRight\" allinea A1 a destra");

	ClickButton(win, FindToolButton(win, "toolAlignLeft"));
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fAlignment == eAlignLeft,
		"il pulsante \"toolAlignLeft\" (stesso comando, parametro diverso) allinea A1 a sinistra, non piu' a destra");

	ClickButton(win, FindToolButton(win, "toolWrapText"));
	doc->GetCellStyle(cell(1, 1), cs);
	Check(cs.fWrapText, "il pulsante \"toolWrapText\" attiva l'a capo automatico su A1");

	// Colore testo/sfondo aprono una vera ColorWindow (comando
	// asincrono guidato dall'utente, non un valore immediato sulla
	// cella): qui si verifica solo che il pulsante esista con un
	// messaggio valido, non l'effetto finale -- serve interazione
	// reale con ColorWindow, fuori scopo per questo test.
	BButton* textColorButton = FindToolButton(win, "toolTextColor");
	BButton* highlightButton = FindToolButton(win, "toolHighlight");
	Check(textColorButton && highlightButton
		&& textColorButton->Message() && highlightButton->Message(),
		"\"toolTextColor\"/\"toolHighlight\" esistono con un messaggio valido");

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
