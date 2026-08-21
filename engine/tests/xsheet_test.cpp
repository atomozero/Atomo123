/*
	xsheet_test.cpp

	Test headless dei riferimenti fra fogli (Fase 9,
	"NomeFoglio!Cella"): parsing, calcolo e UnMangle (round-trip
	testuale, barra formule) di una formula che referenzia un
	CContainer diverso dal proprio tramite ISheetResolver -- lo stesso
	meccanismo che MainWindow (ui/) implementa per la vera cartella di
	lavoro multi-foglio, qui verificato senza bisogno di una UI.

	Il nome del foglio (non un indice) e' incorporato nel bytecode
	(vedi il commento su ISheetResolver in Container.h) e risolto SOLO
	in fase di calcolo, mai di parsing: "NomeFoglio!Cella" e' sempre
	sintatticamente un riferimento incrociato, anche se in quel momento
	nessun resolver e' collegato o nessun foglio ha ancora quel nome --
	si limita a non risolversi (eNoData), non degrada mai a un valore
	testuale come prima di questa scelta di design (bug reale scoperto
	con la risoluzione per indice: un riferimento incrociato falliva
	sempre dopo un giro salva/ricarica, perche' ogni foglio viene
	ri-analizzato da solo -- LoadASCD -- prima che tutti i fogli della
	cartella di lavoro esistano; vedi ui/tests/test_xsheet_formulas.cpp
	per la stessa verifica end-to-end su una vera MainWindow).
*/

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <DataIO.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "Formula.h"

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
	fflush(stdout);
}

// Stesso ruolo di MainWindow (ui/src/MainWindow.h/.cpp) nell'app
// vera: risolve il nome di un foglio verso il CContainer corrispondente.
class TestResolver : public ISheetResolver {
public:
	std::vector<CContainer *> sheets;
	std::vector<const char *> names;

	CContainer *ResolveSheetByName(const char *name)
	{
		for (size_t i = 0; i < names.size(); i++)
			if (strcmp(names[i], name) == 0)
				return sheets[i];
		return NULL;
	}

	// Fase 15: nessun riferimento a tabella fra fogli in questo file
	// (vedi table_refs_test.cpp per quelli) -- solo per soddisfare
	// l'interfaccia, mai chiamata qui.
	CContainer *FindSheetWithTable(const std::string &)
	{
		return NULL;
	}
};

int main()
{
	CContainer &detail = *new CContainer(NULL, NULL);
	CContainer &summary = *new CContainer(NULL, NULL);
	// Nome con spazi e un trattino, come i fogli reali del file di
	// gara che ha motivato questo lavoro (es. "BT02 - CM_Installazione"):
	// serve fra apici singoli, vedi QIDENT in parser.h/.cpp.
	CContainer &quoted = *new CContainer(NULL, NULL);

	TestResolver resolver;
	resolver.sheets.push_back(&detail);
	resolver.sheets.push_back(&summary);
	resolver.sheets.push_back(&quoted);
	resolver.names.push_back("Dettaglio");
	resolver.names.push_back("Riepilogo");
	resolver.names.push_back("BT02 - CM_Installazione");

	detail.SetSheetResolver(&resolver);
	summary.SetSheetResolver(&resolver);
	quoted.SetSheetResolver(&resolver);

	cell i56(9, 56); // colonna I (h=9), riga 56
	TryToParseString("42", i56, &detail, true);

	cell b1(2, 1);
	TryToParseString("=Dettaglio!I56+8", b1, &summary, true);
	summary.CalcCell(b1);

	Value v;
	summary.GetValue(b1, v);
	Check((double)v == 50.0, "=Dettaglio!I56+8 nel foglio Riepilogo calcola 50 (42+8)");

	// Nome fra apici (QIDENT), come la maggioranza dei riferimenti
	// incrociati del file di gara reale che ha motivato questo lavoro
	// (spazi/trattini nel nome del foglio, non validi in un
	// identificatore semplice) -- sia in ingresso ("'Nome'!Cella") sia
	// in uscita (UnMangle). Somma di due riferimenti singoli (non
	// SUM() su un intervallo fra fogli, che non e' ancora supportato,
	// vedi il commento su valXRange in Formula.cpp): esattamente lo
	// stesso schema (addizione cella per cella, non SUM su un
	// intervallo) usato dalle formule vere del file reale.
	cell i10(9, 10);
	TryToParseString("29", i10, &quoted, true);
	cell i11(9, 11);
	TryToParseString("13", i11, &quoted, true);

	cell b4(5, 1);
	TryToParseString("='BT02 - CM_Installazione'!I10+'BT02 - CM_Installazione'!I11",
		b4, &summary, true);
	summary.CalcCell(b4);
	summary.GetValue(b4, v);
	Check((double)v == 42.0,
		"'BT02 - CM_Installazione'!I10+...!I11 con un nome fra apici calcola 42 (29+13)");

	char quotedFormula[256];
	summary.GetCellFormula(b4, quotedFormula, sizeof(quotedFormula), false);
	Check(strstr(quotedFormula, "'BT02 - CM_Installazione'!I10") != NULL,
		"la formula ricostruita mostra di nuovo il nome fra apici, spazi e trattino compresi");

	// Nome di foglio che comincia per cifra, SENZA apici, come i fogli
	// veri del file che ha motivato questo test ("1P_Mandata_studio",
	// "1P_Ripresa_studio"): legale in XLSX/ECMA-376 anche senza apici
	// (LibreOffice Calc lo scrive cosi'), ma il lessico riconosceva la
	// cifra iniziale come un NUMERO a se stante e scartava il resto
	// della formula come token scollegati -- l'intera formula veniva
	// quindi lasciata come testo grezzo, mai calcolata (vedi lo stato
	// 26 in lexer.cpp). Nome scelto identico a quello del file reale.
	CContainer &digitSheet = *new CContainer(NULL, NULL);
	resolver.sheets.push_back(&digitSheet);
	resolver.names.push_back("1P_Mandata_studio");
	digitSheet.SetSheetResolver(&resolver);

	cell u14(21, 14); // colonna U (h=21, A=1...I=9...U=21), riga 14
	TryToParseString("21.3", u14, &digitSheet, true);

	cell b5(6, 1);
	TryToParseString("=1P_Mandata_studio!U14*2", b5, &summary, true);
	summary.CalcCell(b5);
	summary.GetValue(b5, v);
	Check((double)v == 42.6,
		"=1P_Mandata_studio!U14*2 (nome foglio che comincia per cifra, senza apici) calcola 42.6");

	// UnMangle: la formula si ricostruisce con "NomeFoglio!Cella" nel
	// testo (barra formule), non con un indice numerico interno. Cella
	// a parte con SOLO il riferimento incrociato, senza "+8": UnMangle
	// di un valNum (il letterale 8) passa da ftoa(), che legge la
	// larghezza di una stringa dal Font Kit (gFontSizeTable[0].Font().
	// StringWidth) -- richiede una connessione app_server che questo
	// test headless non ha (stessa categoria di limite gia' nota per
	// be_plain_font in Container.cpp, mai finora incontrata perche'
	// nessun test headless esistente chiamava GetCellFormula/UnMangle
	// su una formula con un numero). Il solo riferimento incrociato,
	// senza numeri, non tocca affatto quel percorso.
	cell b3(4, 1);
	TryToParseString("=Dettaglio!I56", b3, &summary, true);
	char formula[256];
	summary.GetCellFormula(b3, formula, sizeof(formula), false);
	Check(strstr(formula, "'Dettaglio'!I56") != NULL,
		"la formula ricostruita mostra \"'Dettaglio'!I56\" (sempre fra apici, vedi UnMangle), "
		"non un indice numerico interno");

	// Se il foglio di origine cambia dopo il calcolo, il riferimento
	// incrociato lo segue (stessa logica di CalcCell su un riferimento
	// nello stesso foglio: nessun grafo di dipendenze, serve un nuovo
	// CalcCell esplicito).
	TryToParseString("100", i56, &detail, true);
	summary.CalcCell(b1);
	summary.GetValue(b1, v);
	Check((double)v == 108.0, "dopo aver cambiato Dettaglio!I56 a 100, un nuovo CalcCell dà 108 (100+8)");

	// Un riferimento a un foglio che non esiste E' comunque sempre un
	// riferimento incrociato sintatticamente (mai testo): si limita a
	// non risolversi (eNoData), verificato con CalcCell esplicito (il
	// tipo del valore "grezzo" appena analizzato, prima di un calcolo,
	// non e' significativo).
	cell b2(2, 2);
	TryToParseString("=NonEsiste!A1", b2, &summary, true);
	summary.CalcCell(b2);
	Value stored;
	summary.GetValue(b2, stored);
	Check(stored.fType == eNoData,
		"un riferimento a un foglio inesistente resta un riferimento incrociato, solo non risolto (eNoData)");

	// Un documento non collegato a nessun ISheetResolver (caso comune:
	// un file a un solo foglio, mai parte di una cartella di lavoro)
	// analizza comunque "Dettaglio!I56" come riferimento incrociato
	// (nessun controllo sul resolver in fase di parsing, vedi
	// parser.cpp): resta semplicemente non risolto, mai un crash.
	CContainer &lone = *new CContainer(NULL, NULL);
	cell c1(3, 1);
	TryToParseString("=Dettaglio!I56", c1, &lone, true);
	lone.CalcCell(c1);
	Value loneVal;
	lone.GetValue(c1, loneVal);
	Check(loneVal.fType == eNoData,
		"in un documento senza nessun ISheetResolver, \"Dettaglio!I56\" non risolve ma non crasha (eNoData)");
	lone.Release();

	// Un riferimento incrociato GIA' compilato (valXRef nel bytecode)
	// il cui resolver viene poi rimosso -- es. un foglio scollegato
	// dalla propria cartella di lavoro -- non deve mai crashare in
	// CFormula::Calculate: si limita a non risolversi (eNoData). Si
	// usa b3 ("=Dettaglio!I56" da solo, senza "+8"): un valXRef non
	// risolto sommato a un numero letterale darebbe comunque un
	// risultato numerico, per via delle stesse regole di Value::
	// operator+= che trattano una cella vuota come 0 in un'addizione
	// (comportamento corretto e voluto, identico a "=Z999+8" con Z999
	// vuota -- non un sintomo di crash o di risoluzione riuscita).
	summary.SetSheetResolver(NULL);
	summary.CalcCell(b3);
	Value afterDetach;
	summary.GetValue(b3, afterDetach);
	Check(afterDetach.fType == eNoData,
		"un valXRef già compilato non crasha se il resolver viene rimosso dopo, si limita a non risolvere");

	// Round-trip sul formato nativo: CFormula::Write/Read (usati da
	// AscdIO::SaveASCD/LoadASCD in ui/, non ripetuti qui) devono
	// preservare un valXRef intatto -- prima di questo lavoro Write
	// scriveva il marcatore del token senza i suoi dati (stream
	// disallineato per tutto cio' che segue) e Read lanciava
	// errCorruptedFile su un file con un riferimento incrociato.
	summary.SetSheetResolver(&resolver);

	void *rawFormula = summary.GetCellFormula(b1);
	Check(rawFormula != NULL, "GetCellFormula(cell) restituisce il bytecode grezzo di b1");

	BMallocIO stream;
	CFormula toWrite(rawFormula); // non ne prende possesso, vedi sotto
	toWrite.Write(stream);

	stream.Seek(0, SEEK_SET);
	CFormula reread;
	reread.Read(stream, NULL); // NULL: b1 non contiene nessuna funzione con nome

	Value roundTripped;
	reread.Calculate(b1, roundTripped, &summary);
	Check((double)roundTripped == 108.0,
		"un valXRef sopravvive al giro Write/Read del formato nativo: =Dettaglio!I56+8 ricalcola 108 (100+8)");
	reread.Clear();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");

	detail.Release();
	summary.Release();
	quoted.Release();
	digitSheet.Release();
	return gFailures == 0 ? 0 : 1;
}
