/*
	Copyright 1996, 1997, 1998, 2000
	        Hekkelman Programmatuur B.V.  All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.
	3. All advertising materials mentioning features or use of this software
	   must display the following acknowledgement:
	   
	    This product includes software developed by Hekkelman Programmatuur B.V.
	
	4. The name of Hekkelman Programmatuur B.V. may not be used to endorse or
	   promote products derived from this software without specific prior
	   written permission.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
	AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
	WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
	OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
*/
#include "Formula.h"
#include "Value.h"
#include "parser.h"
#include "MyError.h"
#include "Utils.h"
#include "EngineViewStub.h"
#include <cstdio>
#include "CellParser.h"
#include "FunctionUtils.h"
#include "Container.h"
#include "NameTable.h"
#include "StringTable.h"

CParser::CParser(CContainer *inContainer,
	char inListSep, char inDecSep, char inDateSep, char inTimeSep)
	: mArgCnt(0)
	, mLookahead(0)
	, mRelop(0)
	, mOffset(0)
	, mBracketDepth(0)
	, mNextFactorWantsRef(false)
	, mIsFormula(false)
	, mExpr(NULL)
	, mExprStart(NULL)
	, mTokenStart(NULL)
	, mContainer(inContainer)
	, mNum(0.0)
	, mTime(0)
	, mBool(false)
	, mListSep(inListSep)
	, mDecSep(inDecSep)
	, mDateSep(inDateSep)
	, mTimeSep(inTimeSep)
{
	memset( mToken, sizeof(mToken), 0 ) ;
} // CParser::CParser

CParser::~CParser()
{
	mFormula.Clear();
} // CParser::~CParser

void CParser::Match(int token)
{
	//	fprintf( stderr, "CParser::Match(%d) /%d\n", token, mLookahead ) ;
	if (mLookahead == token)
	{
		if (token != TEXT && token != END && token != IDENT)
			mIsFormula = true;
		mTokenStart = mExpr;
		
		bool mayBeTime =
			token == '('|| token == LIST;
		
		if( mLookahead != END )
			mLookahead = GetNextToken(mayBeTime);
	}
	else
	{
		char t[256];
		
		switch (token)
		{
			case END:		getindstring(t, 6, errMMEnd); break;
			case ')':		getindstring(t, 6, errMMClosingParen); break;
			case CELL:		getindstring(t, 6, errMMCell); break;
			case NUMBER:	getindstring(t, 6, errMMNumber); break;
			case LIST:		getindstring(t, 6, errMMList); break;
			default: 		sprintf(t, "%c", mLookahead); break;
		}
		
		if (mLookahead == END)
			getindstring(mToken, 6, errMMEnd);

		throw CParseErr(mTokenStart-mExprStart, strlen(mToken), errExpected, t, mToken);
	}
} // CParser::Match

bool CParser::Parse(const char *inString, cell inLocation)
{

	//	fprintf( stderr, "CParser::Parse( \"%s\", [ %hd , %hd ] )\n",
	//		inString , inLocation.h , inLocation.v ) ;
	
	mLoc = inLocation;
	mExpr = mExprStart = inString;
	mOffset = 0;
	
	bool result = true;

	try
	{
		mTokenStart = mExpr;
		mLookahead = GetNextToken(true);

		if (mLookahead == RELOP && mRelop == opEQ)
		{
			Match(RELOP);
			mIsFormula = true;
		}
		else
			mIsFormula = false;

		RelExpr();
		Match(END);

		AddToken(opEnd);
	}
	catch (CErr& e)
	{
		if (mIsFormula)
			throw;
		
		result = false;
	}
	
	return result;
} // CParser::Parse

int CParser::ScanFirstToken(const char *inExpr, const char **outPos, cell inLocation)
{
	mLoc = inLocation;
	mExpr = mExprStart = inExpr;
	mOffset = 0;
	
	int result = GetNextToken(true);
	
	*outPos = mExpr;
	
	return result;
} // CParser::ScanFirstToken

void CParser::RelExpr()
{
	BoolExpr();
	while (true)
	{
		switch (mLookahead)
		{
			// Excel text concatenation, not logical AND (see the
			// comment on opConcat in Formula.h for why this used to be
			// opAND and no longer is).
			case '&': Match('&'); BoolExpr(); AddToken(opConcat); break;
			case '|': Match('|'); BoolExpr(); AddToken(opOR); break;
			default: return;
		}
	}
} // CParser::RelExpr

void CParser::BoolExpr()
{
	if (mLookahead == '!')
	{
		Match('!');
		BoolExpr();
		AddToken(opNOT);
	}
	else
	{
		Expr();
		if (mLookahead == RELOP)
		{
			int savedRelop = mRelop;
			Match(RELOP);
			Expr();
			AddToken(savedRelop);
		}
	}
} // CParser::BoolExpr

void CParser::Expr()
{
	Term();
	while (true)
	{
		switch (mLookahead)
		{
			case '-': Match('-'); Term(); AddToken(opMinus); break;
			case '+': Match('+'); Term(); AddToken(opPlus); break;
			default: return;
		}
	}
} // CParser::Expr

void CParser::Term()
{
	Term2();
	while (true)
	{
		switch (mLookahead)
		{
			case '/': Match('/'); Term2(); AddToken(opDiv); break;
			case '*': Match('*'); Term2(); AddToken(opMul); break;
			default: return;
		}
	}
} // CParser::Term

void CParser::Term2()
{
	Factor();
	while (true)
	{
		switch (mLookahead)
		{
			case RAISE: Match(RAISE); Factor(); AddToken(opRaise); break;
			case '^': Match('^'); Factor(); AddToken(opRaise); break;
			default: return;
		}
	}
} // CParser::Term2

// Fase 9: "!Cella" o "!Cella:Cella" dopo un nome di foglio gia'
// riconosciuto (IDENT o QIDENT in Factor()) -- consuma "!" lei stessa
// come prossimo lookahead atteso. Il nome si incorpora cosi' com'e'
// nel bytecode (mai un indice numerico): se il foglio referenziato non
// esiste (ancora, o piu'), il riferimento semplicemente non si risolve
// in fase di calcolo (vedi CFormula::Calculate) -- risolverlo gia' qui
// richiederebbe che il foglio esista gia' in questo momento, falso
// quando un foglio viene ri-analizzato da solo durante il caricamento
// di un file (vedi ISheetResolver in Container.h -- bug reale scoperto
// proprio cosi', vedi ui/tests/test_xsheet_formulas.cpp).
bool CParser::ParseColumnOnlyToken(int& outCol, bool& outAbs)
{
	if (mLookahead != IDENT)
		return false;

	char name[256];
	strcpy(name, mToken);
	if (cell::GetFormulaColumn(name, outAbs, outCol) == 0)
		return false;

	Match(IDENT);
	return true;
} // CParser::ParseColumnOnlyToken

void CParser::MakeWholeColumnEndpoint(cell& out, int col, bool colAbs, bool isTop)
{
	out.h = colAbs ? col : col - mLoc.h;
	out.v = isTop ? 1 : kRowCount;
	out.h ^= VFIXED;
	if (colAbs)
		out.h ^= HFIXED;
} // CParser::MakeWholeColumnEndpoint

void CParser::ParseSheetReference(const char *inName)
{
	Match('!');

	range r;
	bool wholeCol = false;
	int col1;
	bool col1Abs;

	if (mLookahead == CELL)
	{
		r.TopLeft() = mCell;
		Match(CELL);
	}
	else if (ParseColumnOnlyToken(col1, col1Abs))
	{
		// Riferimento a colonna intera su un altro foglio
		// ("Foglio!$A:$A", Fase 15) -- il caso piu' comune nei file
		// XLSX reali per questa sintassi (INDEX/MATCH su un'intera
		// colonna di un foglio dati separato), vedi memoria
		// project_xlsx_formula_gaps_20260808.
		MakeWholeColumnEndpoint(r.TopLeft(), col1, col1Abs, true);
		wholeCol = true;
	}
	else
		Match(CELL); // nessuno dei due: stesso errore "atteso CELL" di prima

	mIsFormula = true;

	char buf[256 + sizeof(range)];
	size_t nameLen = strlen(inName) + 1;
	memcpy(buf, inName, nameLen);

	if (mLookahead == RANGE)
	{
		Match(RANGE);

		if (wholeCol)
		{
			int col2;
			bool col2Abs;
			if (!ParseColumnOnlyToken(col2, col2Abs))
				throw CParseErr(mTokenStart - mExprStart, strlen(mToken),
					errSyntaxError, mToken);
			MakeWholeColumnEndpoint(r.BotRight(), col2, col2Abs, false);
		}
		else
		{
			r.BotRight() = mCell;
			Match(CELL);
		}

		memcpy(buf + nameLen, &r, sizeof(range));
		AddToken(valXRange, buf);
	}
	else
	{
		if (wholeCol)
			// "Foglio!A" da sola (senza ":altra colonna") non e'
			// sintassi valida: una colonna intera esiste solo come
			// intervallo, mai come singola cella.
			throw CParseErr(mTokenStart - mExprStart, strlen(mToken),
				errSyntaxError, mToken);

		cell c = r.TopLeft();
		memcpy(buf + nameLen, &c, sizeof(cell));
		AddToken(valXRef, buf);
	}
} // CParser::ParseSheetReference

// Fase 14/16: "Tabella12[Codice]" -- il nome della colonna e' gia' un
// token TBLCOL completo (lookahead corrente, contenuto in mToken senza
// le parentesi quadre, vedi lexer.cpp stati 60-61) quando questo
// metodo viene chiamato da Factor(). Combina tabella e colonna in
// un'unica stringa "Tabella12[Codice]" che viaggia come un normale
// valName -- vedi il commento su ParseTableReference in parser.h per
// il perche'.
//
// Fase 16: due sintassi composte in piu' (entrambe trovate in file
// XLSX reali, 554 celle in un solo file -- vedi memoria
// project_xlsx_formula_gaps_20260808):
//   "Tabella12[[#This Row],[Prezzo]]" -- riga della formula stessa,
//   colonna specifica (una sola cella, non l'intera colonna).
//   "Tabella12[[Col1]:[Col2]]" -- intervallo multi-colonna (tutte le
//   righe dati, da Col1 a Col2).
// Grazie al conteggio di profondita' in lexer.cpp (stato 60), mToken
// per questi due casi contiene ancora le parentesi quadre INTERNE,
// gia' spogliato solo di quella esterna: "[#This Row],[Prezzo]" o
// "[Col1]:[Col2]". Qui si riconosce la forma e si ricodifica in un
//'unica stringa piu' semplice che CContainer::ResolveName sa
// interpretare senza dover rifare l'analisi sintattica: "#Prezzo" per
// riga corrente + colonna (il "#" iniziale non e' mai un vero nome di
// colonna, ne' in Excel ne' qui), "Col1:Col2" per l'intervallo (i nomi
// di colonna reali non contengono mai ":"). Il caso semplice "[Col]"
// (mToken non inizia con "[") resta invariato.
void CParser::ParseTableReference(const char *inName)
{
	char buf[512];

	if (mToken[0] == '[')
	{
		const char *p = mToken + 1;
		const char *end1 = strchr(p, ']');
		if (!end1 || (end1[1] != ':' && end1[1] != ',') || end1[2] != '[')
			throw CParseErr(mTokenStart - mExprStart, strlen(mToken),
				errSyntaxError, mToken);

		std::string part1(p, end1 - p);
		char sep = end1[1];
		const char *p2 = end1 + 3;
		const char *end2 = strchr(p2, ']');
		if (!end2)
			throw CParseErr(mTokenStart - mExprStart, strlen(mToken),
				errSyntaxError, mToken);
		std::string part2(p2, end2 - p2);

		if (sep == ':')
			// "[[Col1]:[Col2]]" -> "Tabella12[Col1:Col2]"
			snprintf(buf, sizeof(buf), "%s[%s:%s]", inName, part1.c_str(), part2.c_str());
		else if (strcasecmp(part1.c_str(), "#This Row") == 0)
			// "[[#This Row],[Col]]" -> "Tabella12[#Col]"
			snprintf(buf, sizeof(buf), "%s[#%s]", inName, part2.c_str());
		else
			// Altri selettori speciali di Excel ("[#Headers]",
			// "[#Totals]", "[#Data]" abbinati a una colonna, o un
			// riferimento "[@Colonna]" scritto per esteso come
			// "[[Colonna1],[Colonna2]]" per piu' colonne): nessuno di
			// questi e' comparso nei file reali che hanno motivato
			// questo lavoro (vedi memoria project_xlsx_formula_gaps_
			// 20260808) -- non implementati, errore di sintassi
			// esplicito invece di un risultato silenziosamente
			// sbagliato.
			throw CParseErr(mTokenStart - mExprStart, strlen(mToken),
				errSyntaxError, mToken);
	}
	else
		snprintf(buf, sizeof(buf), "%s[%s]", inName, mToken);

	mIsFormula = true;
	Match(TBLCOL);
	AddToken(valName, buf);
} // CParser::ParseTableReference

void CParser::Factor()
{
	switch (mLookahead)
	{
		case '-':
			Match('-');
			Factor();
			AddToken(opNegate);
			break;

		case '+':
			// Piu' unario (Excel lo accetta come prefisso no-op davanti a
			// un numero o un riferimento: "+A1", "+'Foglio con spazi'!A1",
			// "+SUM(F27)"...) -- comunissimo nei file XLSX reali (abitudine
			// ereditata da vecchie versioni di Excel/Lotus 1-2-3). Prima
			// di questo case mancava del tutto: qualunque formula con
			// questo prefisso falliva il parsing e TryToParseString la
			// degradava silenziosamente a testo puro invece che a formula
			// -- bug reale segnalato dall'utente (formule mostrate come
			// testo letterale invece che calcolate), trovato in un file
			// reale dove riguardava il 59% di TUTTE le formule del file.
			// A differenza di "-" sopra, non emette nessun operatore: un
			// "+" davanti a un valore non lo nega ne' lo raddoppia, serve
			// solo a consumare il token e continuare ad analizzare il
			// fattore che segue.
			Match('+');
			Factor();
			break;

		case NUMBER:
		{
			double d = mNum;
			Match(NUMBER);
			AddToken(valNum, &d);
			break;
		}
		
		case '(':
			Match('(');
			RelExpr();
			Match(')');
			AddToken(opParen);
			break;
		
		case CELL:
		{
			range r;

			// Consumato SUBITO e rimesso a false: si applica solo al
			// fattore CORRENTE (il primo argomento di OFFSET), non a un
			//'eventuale lato destro di ":" analizzato piu' sotto
			// tramite una chiamata ricorsiva a Factor() -- vedi il
			// commento su mNextFactorWantsRef in parser.h.
			bool wantsRef = mNextFactorWantsRef;
			mNextFactorWantsRef = false;

			r.TopLeft() = mCell;

			Match(CELL);

			mIsFormula = true;

			if (mLookahead == RANGE)
			{
				Match(RANGE);

				if (mLookahead == CELL)
				{
					// "H11:H15": il caso letterale comune, invariato --
					// stesso identico bytecode di sempre (valRange), per
					// non rischiare nessuna regressione sui file gia'
					// salvati. wantsRef si applica anche qui (OFFSET con
					// un intervallo letterale come primo argomento, es.
					// "OFFSET(H11:H15,1,0)"): un intervallo di piu' celle
					// produce gia' eRangeData con valRange, ma un
					// intervallo DEGENERE scritto a mano ("H11:H11")
					// altrimenti collasserebbe al valore della cella
					// (vedi CFormula::Calculate) -- valRefRange evita
					// l'eccezione, si comporta allo stesso modo sia per
					// "H11" da solo sia per "H11:H11".
					r.BotRight() = mCell;
					Match(CELL);
					AddToken(wantsRef ? valRefRange : valRange, &r);
				}
				else
				{
					// Intervallo dinamico: il lato destro di ":" non e'
					// una cella letterale (es.
					// "H11:OFFSET(H15,-1,0)") -- il lato sinistro
					// diventa un riferimento a se stante (valRefRange,
					// mai valCell: qui serve un riferimento, non il
					// valore della cella), poi si analizza il lato
					// destro con una normale chiamata ricorsiva a
					// Factor() (gestisce OFFSET(...) o qualunque altro
					// fattore sappia gia' produrre un riferimento), e
					// infine si combinano i due con opRangeOp (il vero
					// operatore ":" applicato a due riferimenti
					// qualunque -- vedi il commento in Formula.h).
					// r.BotRight() = r.TopLeft() rende "r" un intervallo
					// degenere valido di una sola cella: senza questo
					// resterebbe (0,0), il valore di default del
					// costruttore di range, mai impostato altrimenti in
					// questo ramo (a differenza del ramo "H11:H15" sopra,
					// che imposta BotRight() da se').
					r.BotRight() = r.TopLeft();
					AddToken(valRefRange, &r);
					Factor();
					AddToken(opRangeOp);
				}
			}
			else if (wantsRef)
			{
				// Stesso motivo del ramo sopra: un intervallo degenere
				// vero, non (0,0).
				r.BotRight() = r.TopLeft();
				AddToken(valRefRange, &r);
			}
			else
				AddToken(valCell, &r.TopLeft());
			break;
		}
		
		case IDENT:
		{
			// Consumato subito, come in case CELL sopra: se questo
			// identificatore non e' davvero OFFSET, un flag ereditato da
			// un chiamante (il lato destro di un intervallo dinamico, vedi
			// il ramo dopo AddToken(opFunc,...) piu' sotto) non deve
			// restare armato per sbaglio sul primo argomento di UN'ALTRA
			// funzione qualunque.
			mNextFactorWantsRef = false;

			char name[256];
			strcpy(name, mToken);
			int s = mTokenStart - mExprStart;

			FuncCallData fcd;
			fcd.funcNr = GetFunctionNr(name);
			/* fcd.funcNr e' -1 se il nome non corrisponde a nessuna
			   funzione nota (incluso il caso in cui la tabella delle
			   funzioni non e' stata caricata, vedi GetFunctionNr):
			   in tal caso non si deve indicizzare gFuncArrayByNr con
			   un indice negativo. expectedArgs = -1 e' gia' il valore
			   che piu' avanti nel codice significa "sconosciuta". */
			int expectedArgs = (fcd.funcNr >= 0) ? gFuncArrayByNr[fcd.funcNr].argCnt : -1;

			Match(IDENT);

			if (mLookahead == '(')
			{
				Match('(');
				// Intervalli dinamici (Fase 36): il primo argomento di
				// OFFSET e' un vero riferimento, non un valore -- vedi
				// il commento su mNextFactorWantsRef in parser.h.
				// Controllo per nome (stesso idioma di TRUE/FALSE poco
				// sotto), non per numero di funzione: piu' semplice, e
				// se "name" non risultasse davvero una funzione nota
				// l'analisi fallisce comunque subito dopo (fcd.funcNr
				// < 0), armare il flag qui non cambia nulla in quel caso.
				if (strcasecmp(name, "OFFSET") == 0)
					mNextFactorWantsRef = true;
				ParamList();
				Match(')');

				if (fcd.funcNr < 0)
					throw CParseErr(s, strlen(name), errUnknownFunction, name);

				if (expectedArgs == -1 || expectedArgs == mArgCnt ||
					(expectedArgs < 0 || expectedArgs <= -fcd.funcNr))
				{
					fcd.argCnt = mArgCnt;
				}
				else if (expectedArgs < 0)
					throw CParseErr(s, strlen(name), errIncorrectNrOfArgs2,
							expectedArgs, mArgCnt);
				else
					throw CParseErr(s, strlen(name), errIncorrectNrOfArgs,
							expectedArgs, mArgCnt);

				AddToken(opFunc, &fcd);

				// Intervallo dinamico che comincia con una chiamata di
				// funzione, non una cella letterale (es.
				// "OFFSET(H11,1,0):OFFSET(H15,-1,0)" oppure
				// "OFFSET(H11,1,0):H15") -- stesso principio del ramo
				// generale in case CELL sopra: il lato destro puo' essere
				// qualunque cosa Factor() sappia gia' analizzare. A
				// differenza del ramo in case CELL (dove il lato destro
				// non e' MAI una singola cella letterale, quel caso va
				// gia' nel percorso veloce case CELL/case CELL), qui SI'
				// puo' esserlo (es. "OFFSET(H11,1,0):H15") -- serve armare
				// mNextFactorWantsRef anche per questo lato, altrimenti
				// case CELL emetterebbe valCell (il VALORE di H15) invece
				// di valRefRange (un riferimento a H15), e opRangeOp
				// sotto fallirebbe silenziosamente (i due operandi devono
				// essere entrambi eRangeData).
				if (mLookahead == RANGE)
				{
					Match(RANGE);
					mNextFactorWantsRef = true;
					Factor();
					AddToken(opRangeOp);
				}
			}
			else if (mLookahead == '!')
			{
				// Riferimento a un'altra scheda della stessa cartella
				// di lavoro (Fase 9): "NomeFoglio!Cella" oppure
				// "NomeFoglio!Cella:Cella". "IDENT !" non ha nessun
				// altro significato in questa grammatica (! come NOT
				// e' solo un operatore prefisso, mai postfisso a un
				// identificatore, vedi BoolExpr sopra), quindi
				// trattarlo sempre come riferimento incrociato non e'
				// ambiguo. Per un nome che contiene spazi o altri
				// caratteri non validi in un identificatore semplice
				// vedi il caso QIDENT sotto ('Nome Foglio'!A1).
				ParseSheetReference(name);
			}
			else if (mLookahead == TBLCOL)
			{
				// Riferimento a tabella strutturata di Excel
				// ("Tabella12[Codice]", Fase 14): "IDENT [" non ha
				// nessun altro significato in questa grammatica (le
				// parentesi quadre non compaiono mai altrove fuori
				// dalla sintassi R1C1, che il lessico riconosce solo
				// subito dopo una "R"/"C", vedi lexer.cpp), quindi
				// trattarlo sempre come riferimento a tabella non e'
				// ambiguo -- stessa precedenza di "!" sopra.
				ParseTableReference(name);
			}
			else if (mLookahead == RANGE)
			{
				// Riferimento a colonna intera ("A:A", "$A:$C", Fase
				// 15): "IDENT :" non ha nessun altro significato
				// valido in questa grammatica quando "name" e' un
				// nome di colonna puro (solo lettere, "$" opzionale
				// davanti, senza numero di riga) -- un vero
				// intervallo con nome seguito da un altro operatore
				// non e' comunque sintassi valida, quindi trattarlo
				// sempre cosi' non e' ambiguo. 795 celle interessate
				// in un file xlsx reale (vedi memoria
				// project_xlsx_formula_gaps_20260808). Le righe
				// dell'intervallo risultante sono sempre 1..kRowCount
				// (VFIXED, come se fossero scritte "$1"/"$16384"):
				// vedi range::GetFormulaName per come si ristampano
				// come "A:A" invece che "A1..A16384".
				bool col1Abs;
				int col1;
				if (cell::GetFormulaColumn(name, col1Abs, col1) == 0)
					throw CParseErr(s, strlen(name), errSyntaxError, name);

				Match(RANGE);

				int col2;
				bool col2Abs;
				if (!ParseColumnOnlyToken(col2, col2Abs))
					throw CParseErr(mTokenStart - mExprStart, strlen(mToken),
						errSyntaxError, mToken);

				range r;
				MakeWholeColumnEndpoint(r.TopLeft(), col1, col1Abs, true);
				MakeWholeColumnEndpoint(r.BotRight(), col2, col2Abs, false);

				AddToken(valRange, &r);
			}
			else if (fcd.funcNr >= 0 && (strcasecmp(name, "TRUE") == 0 || strcasecmp(name, "FALSE") == 0))
			{
				// TRUE/FALSE nudi, senza parentesi, restano un'eccezione
				// voluta: nel vero Excel sono letterali booleani
				// utilizzabili senza "()" (es. "=IF(A1=FALSE;...)",
				// "=TEXTJOIN(\"-\";TRUE;...)"), e diverse formule reali
				// di questo stesso progetto (test compresi) fanno
				// affidamento su questa forma -- a differenza di OGNI
				// altra funzione (vedi il ramo sotto), che non deve mai
				// essere chiamata implicitamente senza parentesi.
				fcd.argCnt = 0;
				AddToken(opFunc, &fcd);
			}
			else
			{
				// Fase 7 (esteso qui): un identificatore NUDO, senza le
				// parentesi che lo renderebbero inequivocabilmente una
				// chiamata di funzione, e' SEMPRE trattato come un
				// possibile riferimento a un intervallo con nome, MAI
				// come una chiamata implicita a zero argomenti -- anche
				// se "name" corrisponde per caso al nome di una
				// funzione nota (fcd.funcNr >= 0), TRUE/FALSE a parte
				// (vedi il ramo sopra). Incorporato cosi' com'e', MAI
				// verificato qui se esiste davvero (CContainer::
				// GetNameTable/IsNamedRange tramite il vecchio
				// GetOwner()/CCellView, sempre NULL nella UI reale, e'
				// stato rimosso). Se il nome non risulta definito nella
				// tabella del documento in fase di CALCOLO
				// (CContainer::ResolveName, mai di parsing), il
				// riferimento semplicemente non si risolve (gNameNan)
				// -- stesso principio gia' scelto per i riferimenti fra
				// fogli (vedi ParseSheetReference sopra): un nome
				// definito DOPO che la formula e' stata scritta (o
				// ri-analizzata da un file caricato, prima che l'utente
				// riapra la finestra Intervalli con nome) deve comunque
				// restare una formula, non degradare silenziosamente a
				// testo puro.
				//
				// PRIMA di questo fix, un identificatore nudo che
				// corrispondeva a una funzione con "expectedArgs" pari
				// a -1 o 0 veniva invece chiamato implicitamente con
				// zero argomenti (AddToken(opFunc, ...)) -- bug reale
				// (segnalato dall'utente, scoperto scrivendo un
				// catalogo di funzioni con nome: digitare la sola
				// parola "TODAY", "CONCAT", "IF" o "XOR" come normale
				// ETICHETTA di testo, senza alcun "=" davanti, veniva
				// silenziosamente CALCOLATO come se fosse stata scritta
				// la formula corrispondente). expectedArgs=-1 e' la
				// stessa sentinella usata per "funzione sconosciuta"
				// qui sopra, ma un TRONCAMENTO involontario (argCnt e'
				// un short in FuncRec, 65535 = "argomenti variabili"
				// diventa bit per bit -1) la faceva scattare anche per
				// funzioni note come SUM/IF/CONCAT/XOR/AND/OR, non solo
				// per le poche VERAMENTE a zero argomenti fissi
				// (TODAY/NOW/PI/...). Nessun'altra formula reale in
				// questo progetto fa affidamento su una chiamata
				// implicita senza parentesi (stesso principio del vero
				// Excel, dove "=OGGI" da solo non esiste, serve sempre
				// "=OGGI()") -- solo TRUE/FALSE, gestiti a parte sopra,
				// ne avevano davvero bisogno.
				AddToken(valName, name);
			}
			break;
		}

		case QIDENT:
		{
			// Nome di foglio fra apici singoli (Fase 9): a differenza
			// di IDENT sopra, un QIDENT e' SEMPRE un riferimento a un
			// altro foglio -- non puo' mai essere un nome di funzione
			// (le funzioni non hanno mai bisogno delle virgolette) ne'
			// un nome di intervallo (stesso motivo), quindi deve
			// sempre essere seguito da "!Cella"; se non lo e', Match
			// sotto solleva un errore di parsing appropriato da solo.
			char name[256];
			strcpy(name, mToken);
			Match(QIDENT);
			// Deve sempre essere seguito da "!Cella": ParseSheetReference
			// si aspetta "!" come prossimo lookahead (lo consuma lei
			// stessa con Match), esattamente come per il caso IDENT
			// sopra -- se manca, solleva da sola un errore di parsing
			// appropriato.
			ParseSheetReference(name);
			break;
		}

		case TEXT:
			AddToken(valStr, mToken);
			Match(TEXT);
			break;
		
		case TIME:
			AddToken(valTime, &mTime);
			Match(TIME);
			break;
		
		case BOOL:
			AddToken(valBool, &mBool);
			Match(BOOL);
			break;
		
		default:
			Match(NUMBER);
	}

	// Operatore percentuale postfisso di Excel (es. "F37%", "(A1+A2)%"):
	// non si applica solo a un numero letterale come "5%" (l'unico
	// caso gestito prima d'ora, dentro il case NUMBER sopra, col
	// vecchio token valPerc che pero' non divideva mai per 100 in
	// fase di calcolo -- vedi CFormula::CalcCell) ma a QUALUNQUE
	// fattore, esattamente come "-" e' un operatore prefisso generico
	// sopra. Qui dopo l'intero switch cosi' si applica in modo
	// uniforme a ogni caso (CELL, '(', IDENT/funzione, ecc.), non solo
	// a NUMBER -- bug reale scoperto importando una formula da un file
	// .xls reale ("=D37*F37%", un riferimento a cella seguito da "%",
	// non un numero letterale): la grammatica capiva solo "numero%",
	// quindi ri-analizzare il testo della formula esportata falliva e
	// la cella restava vuota.
	if (mLookahead == '%')
	{
		Match('%');
		AddToken(opPercent);
	}
} // CParser::Factor

void CParser::ParamList()
{
	int args = 0;
	
	mIsFormula = true;
	
	while (mLookahead != ')')
	{
		if (mLookahead != LIST)
			RelExpr();
		else
			AddToken(valNil);

		// mNextFactorWantsRef (Fase 36, intervalli dinamici) si applica
		// SOLO al primo argomento (l'unico che il chiamante puo' aver
		// armato, vedi case IDENT sopra) -- rimesso a false qui
		// incondizionatamente, anche se il primo argomento non era
		// affatto una CELL (case CELL lo consuma gia' da solo appena
		// trova una cella, ma un argomento di forma diversa non lo
		// tocca affatto): senza questo resterebbe armato e si
		// applicherebbe per errore a una cella dentro il SECONDO/TERZO
		// argomento.
		mNextFactorWantsRef = false;

		if (mLookahead != ')')
			Match(LIST);

		args++;
	}
	
	mArgCnt = args;
} // CParser::ParamList

