/*
	smoke_test.cpp

	Test minimo, headless, del motore di calcolo isolato: crea un
	documento (CContainer) SENZA alcuna view collegata (nessuna UI,
	nessun app_server), inserisce alcuni valori e delle formule, e
	verifica che i risultati calcolati siano corretti.

	Dimostra concretamente che il motore di calcolo storico di
	Sum-It funziona headless su Haiku moderno a 64 bit, senza
	dipendere dall'Interface Kit (BView/BWindow).
*/

#include <cstdio>
#include <string>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"

static int gFailures = 0;

static void Check(bool condition, const char *what)
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
	// Nessuna CCellView: il documento vive completamente headless.
	// CContainer usa un distruttore privato con reference counting,
	// quindi va allocato sull'heap e rilasciato con Release().
	CContainer &doc = *new CContainer(NULL, NULL);

	cell a1(1, 1);	// colonna A (h=1), riga 1 (v=1)
	cell a2(1, 2);
	cell a3(1, 3);
	cell b1(2, 1);	// colonna B (h=2), riga 1
	cell b2(2, 2);

	TryToParseString("10", a1, &doc, true);
	TryToParseString("20", a2, &doc, true);
	TryToParseString("30", a3, &doc, true);

	TryToParseString("=A1+A2+A3", b1, &doc, true);
	doc.CalcCell(b1);

	TryToParseString("=A1*3", b2, &doc, true);
	doc.CalcCell(b2);

	Value v;
	doc.GetValue(b1, v);
	Check((double)v == 60.0, "B1 (=A1+A2+A3) calcola 60");

	doc.GetValue(b2, v);
	Check((double)v == 30.0, "B2 (=A1*3) calcola 30");

	// Operatore percentuale postfisso (es. "5%" o "A1%"): divide per
	// 100 il valore a cui si applica -- bug reale scoperto importando
	// una formula da un file .xls reale ("=D37*F37%"), dove veniva
	// silenziosamente ignorato (risultato 100 volte troppo grande).
	// Prima di questo fix la grammatica lo riconosceva solo subito
	// dopo un numero letterale ("5%"), mai dopo un riferimento a
	// cella o un'espressione qualunque, e anche in quel solo caso il
	// vecchio token valPerc non veniva mai davvero diviso per 100 in
	// fase di calcolo.
	cell b3(2, 3), b4(2, 4);
	TryToParseString("=5%", b3, &doc, true);
	doc.CalcCell(b3);
	doc.GetValue(b3, v);
	Check((double)v == 0.05, "B3 (=5%, numero letterale) calcola 0.05, non 5");

	TryToParseString("=A1%", b4, &doc, true);
	doc.CalcCell(b4);
	doc.GetValue(b4, v);
	Check((double)v == 0.1, "B4 (=A1%, un riferimento a cella, non un numero letterale) calcola 0.1 (10/100)");

	// Bug reale scoperto da un crash report dell'utente: GetFunctionNr
	// (Utils.cpp) risolveva "_xlfn.CEILING.MATH"/"CONCATENATE" (alias
	// verso CEILING/CONCAT, Fase 14) PRIMA di controllare se la tabella
	// delle funzioni fosse mai stata caricata (gFuncCount<=0, come qui
	// -- questo file non chiama mai InitFunctions() apposta) --
	// restituiva comunque un funcNr "valido", che parser.cpp indicizzava
	// subito dopo in gFuncArrayByNr[funcNr] per leggere argCnt: con
	// gFuncArrayByNr ancora NULL, un puntatore nullo dereferenziato, un
	// crash vero (non solo "funzione sconosciuta") -- capitato
	// nell'app vera quando il translator XLSX (un add-on caricato
	// dinamicamente, con la propria copia di questi globali separata
	// da quella dell'app) analizzava una formula prima che
	// App::ReadyToRun avesse gia' chiamato InitFunctions() la' dentro.
	// Qui riprodotto direttamente: nessuna InitFunctions() mai chiamata
	// in questo file, quindi gFuncCount resta 0 -- il solo fatto che
	// TryToParseString ritorni (invece di far crashare l'intero test)
	// e' gia' la prova che il fix funziona.
	try
	{
		TryToParseString("=_xlfn.CEILING.MATH(1+2)", cell(2, 5), &doc, false);
		TryToParseString("=CONCATENATE(\"a\";\"b\")", cell(2, 6), &doc, false);
		Check(true, "\"_xlfn.CEILING.MATH\"/\"CONCATENATE\" senza InitFunctions() non crashano (gFuncCount resta 0)");
	}
	catch (...)
	{
		Check(false, "\"_xlfn.CEILING.MATH\"/\"CONCATENATE\" senza InitFunctions() non crashano (gFuncCount resta 0)");
	}

	// Bug reale scoperto durante l'audit di Annulla/Ripristina (un
	// crash allo spegnimento, apparentemente scollegato, ha portato
	// fin qui): CFormula::UnMangle (chiamata da GetCellFormula, usata
	// da SaveUndoState a OGNI comando annullabile) accumulava il
	// risultato di ogni operatore (qui "&", concatenazione testo) in
	// uno slot fisso da 4096 byte (kMaxStringLength) con strcpy/strcat
	// SENZA ALCUN controllo di lunghezza -- un solo token e' limitato
	// a 255 caratteri dal lessico (MAXTOKENLENGTH, vedi lexer.cpp),
	// ma concatenarne abbastanza (qui 400 pezzi da 10 caratteri,
	// 4000+ caratteri totali) fa comunque superare i 4096 byte dello
	// slot durante il riassemblaggio, sovrascrivendo la memoria oltre
	// lo slot -- corruzione dell'heap che si manifesta molto piu'
	// tardi, altrove (esattamente il pattern del crash che ha fatto
	// scoprire questo bug). Corretto con strlcpy/strlcat ovunque in
	// UnMangle: qui una prova diretta che una concatenazione
	// lunghissima non crasha piu', anche se il risultato via
	// GetCellFormula puo' restare troncato (meglio di una corruzione
	// silenziosa).
	{
		std::string formula = "=\"AAAAAAAAAA\"";
		for (int i = 0; i < 400; i++)
			formula += "&\"AAAAAAAAAA\"";
		cell c7(2, 7);
		try
		{
			TryToParseString(formula.c_str(), c7, &doc, true);
			doc.CalcCell(c7);
			char buf[8192];
			doc.GetCellFormula(c7, buf, sizeof(buf));
			Check(true, "una formula con ~400 concatenazioni di testo (oltre 4000 caratteri assemblati) non crasha (GetCellFormula/UnMangle)");
		}
		catch (...)
		{
			Check(false, "una formula con ~400 concatenazioni di testo (oltre 4000 caratteri assemblati) non crasha (GetCellFormula/UnMangle)");
		}
	}

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	doc.Release();
	return gFailures == 0 ? 0 : 1;
}
