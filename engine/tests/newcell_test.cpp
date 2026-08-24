/*
	newcell_test.cpp

	Verifica CContainer::NewCell dopo la riscrittura per evitare due
	ricerche nell'albero al posto di una sola (Fase 34, richiesta
	esplicita dell'utente dopo aver profilato l'apertura di un file XLSX
	reale a 13 fogli: NewCell e' il percorso comune a Translate()/
	LoadASCD per OGNI cella importata, find()+operator[] costava due
	discese nell'albero invece di una sola con lower_bound()+insert(hint,
	...) -- vedi il commento in Container.cpp).

	Le due proprieta' che la riscrittura deve preservare esattamente
	come prima:
	1. una cella davvero nuova si inserisce con lo stile predefinito;
	2. riscrivere il VALORE di una cella che ha gia' un suo stile
	   (es. digitare su una cella valuta/grassetto/colorata) conserva
	   quello stile invece di azzerarlo -- bug reale gia' corretto una
	   volta (vedi project_newcell_style_loss_fixed nella memoria di
	   progetto), il rischio piu' concreto di una riscrittura di questa
	   funzione.
*/

#include <cstdio>
#include <cstring>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellStyle.h"

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
	CContainer* doc = new CContainer(NULL, NULL);

	// Una cella nuova (mai esistita prima) prende lo stile predefinito.
	doc->NewCell(cell(1, 1), Value(42.0), NULL);
	Value v;
	doc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 42, "una cella nuova (A1) ha il valore scritto");

	// Applica uno stile esplicito (grassetto/colorato), poi riscrive
	// SOLO il valore -- lo stile deve sopravvivere.
	CellStyle bold;
	doc->GetCellStyle(cell(1, 1), bold);
	bold.fLowColor.red = 200;
	bold.fLowColor.green = 50;
	bold.fLowColor.blue = 50;
	bold.fLowColor.alpha = 255;
	doc->SetCellStyle(cell(1, 1), bold);

	doc->NewCell(cell(1, 1), Value(100.0), NULL); // riscrive SOLO il valore, come digitare sopra

	doc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 100,
		"riscrivere il valore di A1 (100) aggiorna davvero il contenuto");

	CellStyle afterRewrite;
	doc->GetCellStyle(cell(1, 1), afterRewrite);
	Check(afterRewrite.fLowColor.red == 200 && afterRewrite.fLowColor.green == 50
			&& afterRewrite.fLowColor.blue == 50,
		"lo stile (colore di sfondo) di A1 sopravvive alla riscrittura del valore -- "
		"bug reale gia' corretto una volta, il rischio concreto di questa riscrittura "
		"di NewCell (lower_bound()+insert(hint,...) al posto di find()+operator[])");

	// Una SECONDA cella, mai toccata prima, in mezzo a quella gia'
	// esistente e una successiva (B1 dopo A1, prima di un'eventuale
	// C1) -- verifica che l'hint di insert() passato a lower_bound()
	// non danneggi un inserimento fuori ordine, non solo il caso
	// sequenziale crescente.
	doc->NewCell(cell(3, 1), Value(300.0), NULL); // C1
	doc->NewCell(cell(2, 1), Value(200.0), NULL); // B1, inserita DOPO C1 ma prima nell'ordinamento

	doc->GetValue(cell(1, 1), v);
	Check(v.fType == eNumData && (double)v == 100, "A1 resta 100 dopo aver inserito B1/C1");
	doc->GetValue(cell(2, 1), v);
	Check(v.fType == eNumData && (double)v == 200,
		"B1 (inserita fuori ordine, dopo C1) ha il suo valore corretto");
	doc->GetValue(cell(3, 1), v);
	Check(v.fType == eNumData && (double)v == 300, "C1 ha il suo valore corretto");
	Check(doc->GetCellCount() == 3, "il documento ha esattamente 3 celle (A1, B1, C1), nessun duplicato");

	doc->Release();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
