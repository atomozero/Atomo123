#include "Formula.h"

enum {
	IDENT = 258, CELL, TEXT, NUMBER, TIME, RELOP, END, RANGE, LIST, BOOL, RAISE, UMINUS, UNOT,
	// Nome di foglio fra apici singoli ('Nome Foglio', Fase 9): serve
	// quando il nome contiene spazi o altri caratteri non validi in un
	// identificatore semplice -- vedi lexer.cpp (stati 40-41, stesso
	// principio delle virgolette doppie di TEXT) e il commento su
	// CParser::ParseSheetReference sotto.
	QIDENT
};

class CParser
{
  public:
	CParser(CContainer *inContainer, char inListSep, char inDecSep, char inDateSep, char inTimeSep);
	~CParser();
	
	bool Parse(const char *expr, cell inLocation);
	int ScanFirstToken(const char *inExpr, const char **outPos, cell inLocation);
	
	operator double ()
		{ return mNum; }
	operator bool ()
		{ return mBool; }
	operator time_t ()
		{ return mTime; }
	
	const CFormula& Formula() const
		{ return mFormula; }
	
  private:
	
	// lexical analyser
	int GetNextToken(bool acceptTime);
	void Restart(int& start, int& state);
  
	// parser
	void RelExpr();
	void BoolExpr();
	void Expr();
	void Term();
	void Term2();
	void Factor();
	void ParamList();

	// Riferimento a un altro foglio della stessa cartella di lavoro
	// (Fase 9): "NomeFoglio!Cella" o "NomeFoglio!Cella:Cella", chiamato
	// da Factor() sia per un IDENT semplice sia per un QIDENT fra
	// apici -- si aspetta "!" e una CELL/RANGE gia' come prossimo
	// lookahead, esattamente come l'identificatore/il nome fra apici
	// appena riconosciuto da Factor(). Emette valXRef/valXRange col
	// nome incorporato cosi' com'e' (mai un indice, vedi il commento
	// su ISheetResolver in Container.h).
	void ParseSheetReference(const char *inName);

	void Match(int expected);
	
	void AddToken(int token, void *data = NULL)
		{ mFormula.AddToken((PFToken)token, data, mOffset); }
	
	int 			mArgCnt;
	int				mLookahead;
	int 			mRelop;
	int 			mOffset;
	
	bool			mIsFormula;
	const char		*mExpr, *mExprStart, *mTokenStart;
	char			mToken[256];
	cell 			mLoc;
	CContainer 		*mContainer;
	double 			mNum;
	cell 			mCell;
	time_t 			mTime;
	bool			mBool;
	char 			mListSep, mDecSep, mDateSep, mTimeSep;
	CFormula		mFormula;
};

