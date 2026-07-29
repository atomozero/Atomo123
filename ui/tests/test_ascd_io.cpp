/*
	test_ascd_io.cpp

	Test di round-trip di AscdIO (SaveASCD/LoadASCD), la logica usata
	da MainWindow per "Salva con nome"/apertura di file .ascd nativi.
	Non passa dalla vera finestra (BFilePanel, menu) -- verifica solo
	che le funzioni di lettura/scrittura siano l'una l'inversa
	dell'altra, con formule, numeri e testo.
*/

#include <cstdio>
#include <cstring>

#include <File.h>

#include "AscdIO.h"
#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"

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

int main()
{
	CContainer& doc = *new CContainer(NULL, NULL);

	cell a1(1, 1), b1(2, 1), c1(3, 1), d1(4, 1);
	TryToParseString("10", a1, &doc, true);
	TryToParseString("20", b1, &doc, true);
	TryToParseString("=A1+B1", c1, &doc, true);
	TryToParseString("Ciao Atomo123", d1, &doc, true);

	doc.CalcCell(c1);
	Value beforeSave;
	doc.GetValue(c1, beforeSave);
	Check((double)beforeSave == 30.0, "la formula C1 calcola 30 prima del salvataggio");

	BFile file("tests/roundtrip.ascd", B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	Check(file.InitCheck() == B_OK, "creazione del file di test riuscita");

	status_t err = SaveASCD(&doc, &file);
	Check(err == B_OK, "SaveASCD riesce");
	doc.Release();

	BFile reopened("tests/roundtrip.ascd", B_READ_ONLY);
	Check(reopened.InitCheck() == B_OK, "riapertura del file salvato riuscita");

	CContainer& reloaded = *new CContainer(NULL, NULL);
	err = LoadASCD(&reopened, &reloaded);
	Check(err == B_OK, "LoadASCD riesce");

	char text[512];
	reloaded.GetCellFormula(a1, text, false);
	Check(strcmp(text, "10") == 0, "A1 e' 10 dopo il giro completo");

	reloaded.GetCellFormula(b1, text, false);
	Check(strcmp(text, "20") == 0, "B1 e' 20 dopo il giro completo");

	reloaded.GetCellFormula(c1, text, false);
	Check(strstr(text, "A1") != NULL && strstr(text, "B1") != NULL,
		"C1 mantiene la formula (non il valore gia' calcolato) dopo il giro completo");

	reloaded.GetCellFormula(d1, text, false);
	Check(strcmp(text, "Ciao Atomo123") == 0, "D1 mantiene il testo dopo il giro completo");

	// LoadASCD deve aver gia' ricalcolato da solo (RecalculateAll): a
	// differenza di TryToParseString, che imposta solo la formula
	// senza calcolarla, il valore deve essere gia' corretto qui,
	// PRIMA di qualunque CalcCell esplicito -- altrimenti la griglia
	// mostrerebbe celle vuote finche' l'utente non le tocca a mano.
	Value afterLoad;
	reloaded.GetValue(c1, afterLoad);
	Check((double)afterLoad == 30.0,
		"LoadASCD ricalcola gia' da solo C1 a 30, senza bisogno di un CalcCell esplicito");

	reloaded.Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
