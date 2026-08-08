#include "Formula.h"

enum {
	IDENT = 258, CELL, TEXT, NUMBER, TIME, RELOP, END, RANGE, LIST, BOOL, RAISE, UMINUS, UNOT,
	// Nome di foglio fra apici singoli ('Nome Foglio', Fase 9): serve
	// quando il nome contiene spazi o altri caratteri non validi in un
	// identificatore semplice -- vedi lexer.cpp (stati 40-41, stesso
	// principio delle virgolette doppie di TEXT) e il commento su
	// CParser::ParseSheetReference sotto.
	QIDENT,
	// Riferimento a tabella strutturata di Excel ("Tabella12[Codice]"):
	// il nome della colonna fra "[" "]", che puo' contenere spazi o
	// punti (es. "u.m.") -- stesso principio di QIDENT sopra (testo
	// grezzo delimitato), vedi lexer.cpp stati 60-61 e
	// CParser::ParseTableReference.
	TBLCOL
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

	// Riferimento a tabella strutturata di Excel ("Tabella12[Codice]",
	// Fase 14): chiamato da Factor() con il nome della tabella gia'
	// riconosciuto (IDENT) e il nome della colonna come prossimo
	// lookahead (TBLCOL, vedi lexer.cpp stati 60-61) -- stesso schema
	// di ParseSheetReference sopra, ma il risultato e' un normale
	// valName ("Tabella12[Codice]" cosi' com'e'), non un valXRef/
	// valXRange: la tabella si risolve solo in fase di calcolo (vedi
	// CContainer::ResolveName), zero altre modifiche necessarie a
	// Formula.IO.cpp/Formula.iter.cpp/Formula.refs.cpp, che trattano
	// valName come una stringa opaca.
	void ParseTableReference(const char *inName);

	// Riferimento a colonna intera ("A:A", "$A:$C", Fase 15): usati
	// sia da Factor() (per "A:A" semplice) sia da ParseSheetReference
	// (per "Foglio!$A:$A", il caso piu' comune nei file XLSX reali --
	// vedi memoria project_xlsx_formula_gaps_20260808) per evitare di
	// duplicare la stessa analisi due volte. ParseColumnOnlyToken
	// consuma il lookahead SOLO se e' davvero un riferimento a
	// colonna pura (torna false senza consumare nulla altrimenti, per
	// lasciare al chiamante la scelta del messaggio d'errore).
	// MakeWholeColumnEndpoint costruisce l'estremo di range risultante
	// (riga 1 o kRowCount, sempre assoluta -- vedi il commento nel
	// ramo RANGE di Factor() per il perche').
	bool ParseColumnOnlyToken(int& outCol, bool& outAbs);
	void MakeWholeColumnEndpoint(cell& out, int col, bool colAbs, bool isTop);

	void Match(int expected);
	
	void AddToken(int token, void *data = NULL)
		{ mFormula.AddToken((PFToken)token, data, mOffset); }
	
	int 			mArgCnt;
	int				mLookahead;
	int 			mRelop;
	int 			mOffset;
	// Profondita' di annidamento delle parentesi quadre mentre si
	// scandisce un token TBLCOL (stato 60 in lexer.cpp): un
	// riferimento a tabella strutturata composto ("[[#This Row],[Col]]"
	// o "[[Col1]:[Col2]]", Fase 16) ha parentesi annidate, a differenza
	// del semplice "[Col]" gia' supportato -- vedi il commento in
	// lexer.cpp sullo stato 60 per i dettagli.
	int				mBracketDepth;
	
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

