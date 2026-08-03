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
			case '&': Match('&'); BoolExpr(); AddToken(opAND); break;
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
void CParser::ParseSheetReference(const char *inName)
{
	Match('!');

	range r;
	r.TopLeft() = mCell;
	Match(CELL);
	mIsFormula = true;

	char buf[256 + sizeof(range)];
	size_t nameLen = strlen(inName) + 1;
	memcpy(buf, inName, nameLen);

	if (mLookahead == RANGE)
	{
		Match(RANGE);
		r.BotRight() = mCell;
		Match(CELL);
		memcpy(buf + nameLen, &r, sizeof(range));
		AddToken(valXRange, buf);
	}
	else
	{
		cell c = r.TopLeft();
		memcpy(buf + nameLen, &c, sizeof(cell));
		AddToken(valXRef, buf);
	}
} // CParser::ParseSheetReference

void CParser::Factor()
{
	switch (mLookahead)
	{
		case '-':
			Match('-');
			Factor();
			AddToken(opNegate);
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
			
			r.TopLeft() = mCell;
			
			Match(CELL);
			
			mIsFormula = true;
			
			if (mLookahead == RANGE)
			{
				Match(RANGE);
				r.BotRight() = mCell;
				Match(CELL);
				AddToken(valRange, &r);
			}
			else
				AddToken(valCell, &r.TopLeft());
			break;
		}
		
		case IDENT:
		{
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
			else
			{
				if (fcd.funcNr >= 0)
				{
					if (expectedArgs != -1 && expectedArgs != 0)
						throw CParseErr(s, strlen(name), errIncorrectNrOfArgs,
							expectedArgs, 0);

					fcd.argCnt = 0;

					AddToken(opFunc, &fcd);
				}
				else
				{
					// Fase 7: un identificatore che non e' un nome di
					// funzione conosciuta e' sempre un possibile
					// riferimento a un intervallo con nome, incorporato
					// cosi' com'e' -- MAI verificato qui se esiste
					// davvero (CContainer::GetNameTable/IsNamedRange
					// tramite il vecchio GetOwner()/CCellView, sempre
					// NULL nella UI reale, e' stato rimosso). Se il
					// nome non risulta definito nella tabella del
					// documento in fase di CALCOLO
					// (CContainer::ResolveName, mai di parsing), il
					// riferimento semplicemente non si risolve
					// (gNameNan) -- stesso principio gia' scelto per i
					// riferimenti fra fogli (vedi ParseSheetReference
					// sopra): un nome definito DOPO che la formula e'
					// stata scritta (o ri-analizzata da un file
					// caricato, prima che l'utente riapra la finestra
					// Intervalli con nome) deve comunque restare una
					// formula, non degradare silenziosamente a testo
					// puro.
					AddToken(valName, name);
				}
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
		
		if (mLookahead != ')')
			Match(LIST);
		
		args++;
	}
	
	mArgCnt = args;
} // CParser::ParamList

