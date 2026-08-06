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
/*
	Container.h
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#ifndef CONTAINER_H
#define CONTAINER_H

/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.pch++"
 *** TPV (2000-Feb-06) Added Headers Guards
 ***/

#include <map>
#include <string>
#include <vector>

#ifndef   CELL_H
#include "Cell.h"
#endif

#ifndef   RANGE_H
#include "Range.h"
#endif

#ifndef   RUNARRAY2_H
#include "RunArray2.h"
#endif

#ifndef   CELLDATA_H
#include "CellData.h"
#endif

#include <Locker.h>

class CCellView;
class CCellIterator;
class CContainer;
class CFormula;
class CNameTable;
class CCalculateJob;
struct CellStyle;
class CSet;
class CFormula;

enum SplitType {
	noSplit, hSplit, vSplit
};

typedef std::map<cell,CellData> cellmap;
//typedef btree<cell,CellData> cellmap;

// Risolve un riferimento incrociato fra fogli di una stessa cartella
// di lavoro ("NomeFoglio!Cella", Fase 9) -- implementata dalla UI
// (MainWindow), non dal motore: l'engine isolato non sa nulla di
// "cartella di lavoro", solo di singoli CContainer. Stesso principio
// gia' esistente per CFormula::ResolveName (nomi di intervallo) ma
// separato da esso: qui la chiave e' il foglio, non un nome di
// intervallo dentro un foglio. Vedi il commento storico "R4Hack" in
// Formula.h/.cpp -- Sum-It aveva gia' previsto un meccanismo quasi
// identico (XRef con un fileNr per riferimenti fra file separati),
// mai completato: qui e' lo stesso principio, riusato per fogli della
// stessa cartella invece che file esterni.
//
// Risoluzione per NOME (non per indice numerico), sempre in fase di
// CALCOLO, mai in fase di analisi (CParser): un nome incorporato nel
// bytecode resta valido anche se, al momento in cui la formula viene
// compilata, il foglio referenziato non esiste ancora o non e' ancora
// collegato a nessun resolver -- esattamente il caso di un file
// caricato da disco, dove ogni foglio viene analizzato singolarmente
// (LoadASCD) prima che tutti i fogli della cartella di lavoro esistano
// e siano collegati fra loro (bug reale scoperto scrivendo
// ui/tests/test_xsheet_formulas.cpp: con una risoluzione per indice
// decisa dal parser, un riferimento incrociato appena ricaricato da
// disco falliva sempre, perche' nessun resolver era ancora collegato
// nel momento in cui quella singola cella veniva ri-analizzata).
class ISheetResolver {
public:
	virtual ~ISheetResolver() {}

	// NULL se nessun foglio ha (ancora, o piu') questo nome esatto
	// (case-sensitive) -- mai un errore fatale, solo un riferimento
	// che resta non risolto (vedi CFormula::Calculate, valXRef).
	virtual CContainer* ResolveSheetByName(const char* inName) = 0;
};

class CContainer : public BLocker {
	friend class CCellIterator;
	friend class CCalculateJob;

	virtual ~CContainer();

public:
	CContainer(CCellView *inPane = NULL, CNameTable *inNames = NULL);

	void Reference();
	void Release();

	virtual bool Lock();
	virtual void Unlock();
	
	bool WriteLock();
	void WriteUnlock();

	long GetCellCount()			{ return fCellData.size(); }
	void GetBounds(range& r);

/* cell aanmaak en verwijder routines */

	void NewCell(const cell& inLocation, const Value& inValue, void *inFormula);
	void DisposeCell(const cell& c);
	void CopyCell(CContainer *destContainer, const cell& srcLoc, const cell& destLoc,
			range *inFrom = NULL, bool isDragMove = false);
	void MoveCell(CContainer *destContainer, const cell& srcLoc, const cell& destLoc,
			SplitType split = noSplit, int first = 0, int count = 0);
	void ExchangeCells(const cell& a, const cell& b);

/* Celllijst manipulaties */

	bool GetNextCellInRow(cell&, bool mayBeEmpty = false);
	bool GetPreviousCellInRow(cell&, bool mayBeEmpty = false);
	bool GetNextCell(cell&, bool mayBeEmpty = false);
	bool GetPreviousCell(cell&, bool mayBeEmpty = false);

/* cell inhoud manipulaties */

	bool CalcCell(const cell&);
	int GetCellResult(const cell&, char *, size_t bufSize, bool);
	void GetCellFormula(const cell&, char *, size_t bufSize, bool rcStyle = false);
	void* GetCellFormula(const cell&);
	void SetCellFormula(const cell& inLoc, void *inFormula);
	
	void GetCellStyle(const cell&, CellStyle&);
	void SetCellStyle(const cell&, CellStyle&);
	int GetCellStyleNr(const cell&);
	void SetCellStyleNr(const cell&, int);

	void GetColumnStyle(int, CellStyle&);
	void SetColumnStyle(int, CellStyle&);
	int GetColumnStyleNr(int);
	void SetColumnStyleNr(int, int);
	
	void GetDefaultCellStyle(CellStyle&);
	void SetDefaultCellStyle(CellStyle&);
	bool GetValue(const cell&, Value&);
	void SetValue(const cell&, const Value&);
	int GetType(const cell&);
	double GetDouble(const cell&);
	int CompareValue(const cell&, const cell&);
	int GetCellDepth(const cell&);
	void SetCellDepth(const cell&, int);
	bool CellIsConstant(const cell&);
	bool CellHasFormula(const cell&);
	bool RefersToNamedRange(const char *inName);
	void CollectFunctionNrs(CSet& funcs);
	int CollectFontList(int *fontList);
	int CollectFormats(int *formatList);
	int CollectStyles(int *styleList);

/* accessors */
	CCellView* GetOwner() const			{ return fInView; };
	CRunArray2& GetColumnStyles()		{ return fColumnStyles; }
	cell CalculatingCell() const		{ return fCalculatingCell; }
	CNameTable *GetNameTable() const	{ return fNames; }

	// Fase 7: crea la tabella dei nomi (vuota) se questo documento non
	// ne ha ancora una -- GetNameTable() da sola resta NULL finche'
	// nessuno definisce il primo intervallo con nome (nessun documento
	// nuovo/appena aperto ne alloca una a priori). Chiamata dalla UI
	// (MainWindow) prima di aggiungere/modificare una voce nella
	// mappa che GetNameTable() restituisce -- CNameTable eredita
	// pubblicamente da std::map<CName,range>, quindi non serve
	// nessun metodo dedicato per leggere/scrivere le singole voci.
	// fNewNames=true segna che e' CContainer stesso a possederla (la
	// libera nel proprio distruttore), a differenza di una passata
	// esplicitamente al costruttore.
	CNameTable *GetOrCreateNameTable();

	range ResolveName(const char *name);

	long CountCells(range *inRange = NULL);
	bool Exists(const cell& c);

	// Fase 9: chi possiede questo foglio (MainWindow) collega qui il
	// proprio ISheetResolver quando fa parte di una cartella di
	// lavoro multi-foglio -- NULL per un documento a un solo foglio
	// (mai collegato, o non ancora). Puntatore preso in prestito, MAI
	// posseduto/cancellato da CContainer: la cartella di lavoro vive
	// piu' a lungo di ogni singolo foglio aperto/chiuso al suo
	// interno, e piu' fogli condividono lo stesso resolver.
	void SetSheetResolver(ISheetResolver *inResolver)	{ fSheetResolver = inResolver; }
	ISheetResolver *GetSheetResolver() const			{ return fSheetResolver; }

	// Celle unite (Fase 12): un elenco di rettangoli per foglio, non un
	// campo per-cella -- una cella unita e' un'unica entita' logica che
	// occupa piu' coordinate, non N celle indipendenti con uno stile
	// condiviso (a differenza di colori/font/bordi/ecc, gia' tutti
	// per-cella). Nessuna infrastruttura preesistente (verificato anche
	// nel Sum-It storico), ne' nel motore ne' nella UI.
	void AddMergedRange(const range& r)	{ fMergedRanges.push_back(r); }
	void ClearMergedRanges()				{ fMergedRanges.clear(); }
	const std::vector<range>& GetMergedRanges() const	{ return fMergedRanges; }

	// true se "c" fa parte di un intervallo unito (compresa la cella in
	// alto a sinistra, che lo "possiede" agli effetti del contenuto
	// disegnato/modificabile) -- "outRange" riceve l'intervallo intero,
	// utile sia per disegnare (SheetView) sia per rimappare un clic
	// sulla cella in alto a sinistra.
	bool GetMergedRange(cell c, range* outRange) const;

	// Commenti/note per cella (Fase 13): std::map sparso come
	// fMergedRanges sopra, non un campo diretto in CellData -- quella
	// struct porta gia' un'unione+puntatori per OGNI cella, anche le
	// migliaia che non avranno mai un commento, stesso ragionamento
	// gia' scritto per le celle unite. std::string, non BString:
	// Cell.h/Range.h/CellData.h non hanno mai avuto una dipendenza dal
	// Support Kit finora, restare cosi' anche qui. Un testo vuoto
	// rimuove il commento invece di lasciarne uno vuoto in giro (un
	// commento vuoto e "nessun commento" sono la stessa cosa agli
	// effetti pratici).
	void SetComment(cell c, const std::string& text)
	{
		if (text.empty())
			fComments.erase(c);
		else
			fComments[c] = text;
	}
	std::string GetComment(cell c) const
	{
		std::map<cell, std::string>::const_iterator it = fComments.find(c);
		return (it != fComments.end()) ? it->second : std::string();
	}
	bool HasComment(cell c) const { return fComments.find(c) != fComments.end(); }
	const std::map<cell, std::string>& GetComments() const { return fComments; }

private:
	void Visit(const cell&, void*);
	bool GetCellData(const cell&, CellData&);
	
	long DoCalculate();
	static long Calculate(void *inData);
	
/* en de fields */
	BLocker fWriteLocker;
	int32 fReferenceCount;
	cellmap fCellData;
	CCellView *fInView;
	CNameTable *fNames;
	bool fNewNames;
	int fDefaultCellStyle;
	CRunArray2 fColumnStyles;
	cell fCalculatingCell;
	ISheetResolver *fSheetResolver;
	std::vector<range> fMergedRanges;
	std::map<cell, std::string> fComments;
};

inline bool CContainer::WriteLock()
{
	return fWriteLocker.Lock();
}

inline void CContainer::WriteUnlock()
{
	fWriteLocker.Unlock();
}

class StWriteLock {
public:
	StWriteLock(CContainer *container)
		{ fContainer = container; container->WriteLock(); };
	~StWriteLock()
		{ fContainer->WriteUnlock(); };
private:
	CContainer *fContainer;
};

#endif
