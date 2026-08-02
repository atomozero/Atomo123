/*
	test_currency_format.cpp

	Verifica la riformattazione locale-aware di valuta/percentuale
	(SheetView::FormattedCellText, estratta dal ciclo di disegno di
	DrawCellBand apposta per essere testabile, stesso principio di
	CellRect/ExpandOverflowRect). Confrontando con Excel vero la stessa
	fattura reale, ogni importo in valuta ("€ 450,00") si rendeva come
	un numero puro senza simbolo ne' decimali ("450"): il confronto
	`cs.fFormat == eCurrency` (e lo stesso per ePercent) funziona SOLO
	per un formato senza cifre decimali ne' separatore delle migliaia,
	ma CFormatter::FormatID() impacchetta quei due dettagli negli stessi
	bit sopra l'enum (vedi Formatter.h) -- un formato valuta REALE
	importato da un file (quasi sempre con 2 cifre decimali) non risulta
	quindi mai uguale a eCurrency cosi' com'e'. Serve mascherare con
	"& 0x000F" come gia' fatto per eZeroPad poco sopra nello stesso
	punto del codice.

	Le stringhe esatte prodotte da BNumberFormat dipendono dalla
	locale/valuta configurata nel Locale Kit (non da questo test): le
	verifiche sotto controllano solo la STRUTTURA del risultato (cifre
	decimali forzate per valuta, simbolo "%" per percentuale), non il
	simbolo di valuta esatto -- vedi ROADMAP.md Fase 5 per la nota sulla
	dipendenza dalla configurazione locale di questo ambiente.
*/

#include <cstdio>

#include <Application.h>

#include "Cell.h"
#include "Container.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "Formatter.h"
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

static bool HasDecimalSeparator(const BString& s)
{
	return s.FindFirst(',') >= 0 || s.FindFirst('.') >= 0;
}

int main()
{
	BApplication app("application/x-vnd.Atomo-TestCurrencyFormat");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	// Formato Valuta "pulito" (nessuna cifra/virgola impacchettata,
	// come applicato a mano dal menu Formato): funzionava gia' prima
	// di questo fix, controllo di base.
	{
		TryToParseString("450", cell(1, 1), doc, true); // A1
		CellStyle cs;
		doc->GetCellStyle(cell(1, 1), cs);
		cs.fFormat = eCurrency;
		doc->SetCellStyle(cell(1, 1), cs);

		BString text = view->FormattedCellText(cell(1, 1));
		Check(HasDecimalSeparator(text),
			"Valuta \"pulita\" (senza cifre impacchettate) su 450 forza comunque i decimali (FormatMonetary)");
	}

	// Formato Valuta REALE come quello importato da un file .xls/.xlsx
	// (2 cifre decimali, separatore delle migliaia): stesso valore di
	// prima, ma con fFormat impacchettato esattamente come
	// CFormatter::FormatID() (vedi Formatter.h) -- questo e' il caso
	// che il bug del confronto diretto perdeva.
	{
		TryToParseString("450", cell(1, 2), doc, true); // A2
		CellStyle cs;
		doc->GetCellStyle(cell(1, 2), cs);
		cs.fFormat = eCurrency | (2 << 4) | (1 << 9); // 2 cifre, virgola
		doc->SetCellStyle(cell(1, 2), cs);

		BString text = view->FormattedCellText(cell(1, 2));
		Check(HasDecimalSeparator(text),
			"Valuta impacchettata (2 cifre + virgola, come un formato importato reale) su 450 mostra i decimali, non \"450\" nudo");
	}

	// Stesso valore, formato Generico (nessun formato esplicito): il
	// motore/Locale Kit non forza mai decimali per un intero, resta
	// "450" -- controllo negativo, prova che il test sopra distingue
	// davvero i due casi.
	{
		TryToParseString("450", cell(1, 3), doc, true); // A3
		BString text = view->FormattedCellText(cell(1, 3));
		Check(!HasDecimalSeparator(text),
			"450 senza nessun formato esplicito resta un intero semplice, senza decimali forzati");
	}

	// Percentuale impacchettata (2 cifre decimali), stesso principio
	// di sopra per eCurrency.
	{
		TryToParseString("0.05", cell(1, 4), doc, true); // A4
		CellStyle cs;
		doc->GetCellStyle(cell(1, 4), cs);
		cs.fFormat = ePercent | (2 << 4);
		doc->SetCellStyle(cell(1, 4), cs);

		BString text = view->FormattedCellText(cell(1, 4));
		Check(text.FindFirst('%') >= 0,
			"Percentuale impacchettata (2 cifre) su 0.05 mostra il simbolo \"%\", non un numero puro");
	}

	// Formato Fisso impacchettato (non valuta ne' percentuale): non
	// deve mai prendere per sbaglio il ramo percentuale/valuta solo
	// perche' ha anch'esso cifre/virgola impacchettate nello stesso
	// modo.
	{
		TryToParseString("1234.5", cell(1, 5), doc, true); // A5
		CellStyle cs;
		doc->GetCellStyle(cell(1, 5), cs);
		cs.fFormat = eFixed | (2 << 4) | (1 << 9);
		doc->SetCellStyle(cell(1, 5), cs);

		BString text = view->FormattedCellText(cell(1, 5));
		Check(text.FindFirst('%') < 0,
			"Fisso impacchettato (2 cifre + virgola) su 1234.5 non prende per sbaglio il ramo percentuale");
	}

	// Cella senza contenuto: stringa vuota, non un crash ne' "0".
	{
		BString text = view->FormattedCellText(cell(9, 9));
		Check(text.Length() == 0, "una cella vuota (mai toccata) restituisce una stringa vuota");
	}

	win->Unlock();

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
