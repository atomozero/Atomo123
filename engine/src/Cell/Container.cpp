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
	Container.c

	Copyright 1997, Hekkelman Programmatuur
	Portions Copyright 2026 Andrea Bernardi

	Part of Sum-It for the BeBox version 1.1.

*/

#include <support/Debug.h>

#include <cstring>
#include <set>
#include <vector>

#include "Cell.h"
#include "Formula.h"
#include "Value.h"
#include "CellData.h"
#include "Container.h"
#include "EngineViewStub.h"
#include "MyError.h"
#include "Formatter.h"
#include "Utils.h"
#include "StringTable.h"
#include "CellIterator.h"
#include "CellStyle.h"
#include "FontMetrics.h"
#include "NameTable.h"
#include "MAlert.h"
#include "Preferences.h"
#include <StLocker.h>

#if DEBUG
void WarnForUnlockedContainer(int lineNr);
void WarnForUnlockedContainer(int lineNr)
{
	char s[255];
	sprintf(s, "Unlocked container at line %d", lineNr);
	MAlert *a = new MWarningAlert(s);
	a->Go();
}
#define CHECKLOCK	if (!fWriteLocker.IsLocked() && fInView) WarnForUnlockedContainer(__LINE__);

//THROW((errContainerNotLocked));
#else
#define CHECKLOCK	
#endif

const long kIndexSlotCount = 256;

// Vero grafo delle dipendenze (roadmap Tier 3): dichiarate qui, definite
// piu' sotto (vicino a GetQualifiedPrecedents) -- SetCellFormula/
// DisposeCell/MoveCell, definite prima nel file, ne hanno bisogno per
// aggiornare il grafo a ogni modifica di una formula.
static void ComputeQualifiedPrecedents(void* formula, const cell& formulaLoc,
	CContainer* selfContainer, ISheetResolver* resolver,
	std::vector<QualifiedCell>& outCells,
	std::vector<QualifiedCell>& outColumns,
	std::vector<QualifiedCell>& outRows);
void ApplyPrecedentDiff(CContainer* selfContainer, const cell& c,
	const std::vector<QualifiedCell>& oldCells, const std::vector<QualifiedCell>& oldColumns,
	const std::vector<QualifiedCell>& oldRows,
	const std::vector<QualifiedCell>& newCells, const std::vector<QualifiedCell>& newColumns,
	const std::vector<QualifiedCell>& newRows);

CContainer::CContainer(CCellView *inPane, CNameTable *inNames)
	: fColumnStyles(kColCount, -1)
{
	fInView = inPane;
	fNames = inNames;
	fNewNames = false;
	fReferenceCount = 1;
	fSheetResolver = NULL;
	
	CellStyle cs;

	if (inPane)
	{
		// be_plain_font e gPrefs richiedono una vera applicazione
		// GUI collegata all'app_server: senza (motore headless)
		// questa chiamata si blocca in attesa di una risposta che
		// non arrivera' mai. Si esegue solo quando esiste davvero
		// una view (cioe' quando gira dentro l'app con la UI).
		font_family defFamily;
		font_style defStyle;
		float defSize;

		be_plain_font->GetFamilyAndStyle(&defFamily, &defStyle);
		defSize = be_plain_font->Size();

		cs.fFont = gFontSizeTable.GetFontID(
			gPrefs->GetPrefString("defdoc font family", defFamily),
			gPrefs->GetPrefString("defdoc font style", defStyle),
			gPrefs->GetPrefDouble("defdoc font size", defSize));
	}

	fDefaultCellStyle = gStyleTable.GetStyleID(cs);
}

CContainer::~CContainer()
{
	Lock();

	ASSERT(fReferenceCount == 0);

	cellmap::iterator ci;
	for (ci = fCellData.begin(); ci != fCellData.end(); ci++)
		(*ci).second.Clear();

	if (fNewNames && fNames)
	{
//		fNames->Lock();
//		fNames->Destroy();
		delete fNames;
	}
} /* ~CContainer */

void CContainer::Reference()
{
	atomic_add(&fReferenceCount, 1);
} /* CContainer::Reference */

void CContainer::Release()
{
	if (atomic_add(&fReferenceCount, -1) == 1)
		delete this;
} /* CContainer::Release */

bool CContainer::Lock()
{
	return BLocker::Lock();
} /* CContainer::Lock */
 
void CContainer::Unlock()
{
	BLocker::Unlock();
} /* CContainer::Unlock */

void CContainer::GetBounds(range& r)
{	/*CHECKLOCK*/
	if (GetCellCount())
	{
		cellmap::iterator ci;
		short maxX = 0, maxY = 0;
		
		r.left = r.top = 1;
		
		for (ci = fCellData.begin(); ci != fCellData.end(); ci++)
		{
			if ((*ci).second.mType != eNoData)
			{
				maxX = std::max(maxX, (*ci).first.h);
				maxY = (*ci).first.v;
			}
		}
		
		r.right = maxX;
		r.bottom = maxY;
	}
	else
		r.Set(0, 0, 0, 0);
} /* GetBounds */

/* cell aanmaak en verwijder routines */

void CContainer::NewCell(const cell& inLocation, const Value& inValue, void *inFormula)
{	CHECKLOCK
	CellData data;

	// Se la cella esiste gia' (nuovo valore scritto su una cella che ha
	// gia' un formato -- es. digitare su una cella valuta/grassetto/
	// colorata), conserva il suo mStyle invece di azzerarlo a
	// fDefaultCellStyle: bug reale segnalato dall'utente con
	// screenshot, ogni riscrittura del valore perdeva completamente la
	// formattazione della cella. Una cella davvero nuova (nessuna voce
	// preesistente) resta col comportamento di sempre.
	//
	// lower_bound() invece di find() (Fase 34, richiesta esplicita
	// dell'utente dopo aver profilato l'apertura di un file XLSX reale
	// a 13 fogli: ~50-100 microsecondi per NewCell su fogli da centinaia
	// di migliaia di celle, il vero collo di bottiglia sia di Translate()
	// sia di LoadASCD -- entrambi passano da qui per ogni cella): il
	// vecchio codice cercava la cella due volte, una con find() qui
	// sopra e una seconda volta dentro fCellData[inLocation]="assegna
	// dentro un vero e proprio std::map (cellmap in Container.h), dove
	// ogni ricerca costa O(log n) con un fattore costante reale (nodi
	// sparsi in memoria, non contigui). lower_bound() restituisce la
	// STESSA informazione (esiste gia'? qual e' il suo iteratore?) E la
	// posizione corretta per un inserimento nuovo, quindi la seconda
	// ricerca sotto sparisce del tutto -- insert(hint, ...) con
	// quell'iteratore inserisce in tempo costante quando la cella e'
	// davvero nuova (il caso comune importando un file, celle scritte
	// in ordine crescente), o comunque non piu' lento di prima nel
	// caso peggiore.
	cellmap::iterator it = fCellData.lower_bound(inLocation);
	bool exists = it != fCellData.end() && !fCellData.key_comp()(inLocation, it->first);
	int preservedStyle = exists ? it->second.mStyle : fDefaultCellStyle;
	// Vero grafo delle dipendenze (roadmap Tier 3): letto PRIMA di
	// sovrascrivere sotto -- vedi il commento piu' in basso sul perche'
	// serve qui, non solo in SetCellFormula.
	void* oldFormula = exists ? it->second.mFormula : NULL;

	data = inValue;
	data.mFormula = inFormula;
	data.mConstant = !inFormula || CFormula(inFormula).IsConstant();
	data.mStyle = preservedStyle;

	if (exists)
		it->second = data;
	else
		fCellData.insert(it, cellmap::value_type(inLocation, data));

	// Vero grafo delle dipendenze (roadmap Tier 3): a differenza di
	// SetCellFormula (usato SOLO dall'importatore XLS legacy, vedi il
	// suo stesso commento in Container.h), QUESTO e' il vero punto di
	// scrittura di ogni cella digitata dall'utente
	// (TryToParseString -> NewCell, il percorso reale usato da
	// MainWindow) -- senza questo aggiornamento qui, il grafo resterebbe
	// vuoto per ogni formula scritta normalmente. Il controllo veloce
	// sotto evita QUALUNQUE lavoro extra per il caso comune (una cella
	// letterale che lo era gia' prima), lo stesso percorso critico per
	// le prestazioni gia' misurato e ottimizzato sopra (Fase 34, ~50-100
	// microsecondi per cella su un file XLSX reale a centinaia di
	// migliaia di celle) -- un confronto di puntatore in piu' per ogni
	// cella letterale, nessun altro costo.
	if (oldFormula || inFormula)
	{
		std::vector<QualifiedCell> oldCells, oldColumns, oldRows;
		ComputeQualifiedPrecedents(oldFormula, inLocation, this, fSheetResolver,
			oldCells, oldColumns, oldRows);
		std::vector<QualifiedCell> newCells, newColumns, newRows;
		ComputeQualifiedPrecedents(inFormula, inLocation, this, fSheetResolver,
			newCells, newColumns, newRows);
		ApplyPrecedentDiff(this, inLocation, oldCells, oldColumns, oldRows,
			newCells, newColumns, newRows);
	}
} /* NewCell */

void CContainer::DisposeCell(const cell& inLoc)
{	CHECKLOCK
	cellmap::iterator ci;

	ci = fCellData.find(inLoc);

	if (ci != fCellData.end())
	{
		// Vero grafo delle dipendenze (roadmap Tier 3): una cella
		// cancellata smette di avere QUALUNQUE precedente (i suoi bordi
		// USCENTI vanno tolti) -- stesso identico trattamento di
		// SetCellFormula(inLoc, NULL) ai soli fini del grafo, letto PRIMA
		// di Clear() perche' serve ancora la formula per calcolare i
		// vecchi precedenti. Le celle che dipendevano DA "inLoc" non
		// hanno bisogno di nessun bordo tolto qui (il loro riferimento
		// resta valido, vedra' semplicemente una cella vuota al prossimo
		// ricalcolo) -- solo di essere ricalcolate, una questione per la
		// Fase 1 (il consumatore del grafo), non per questo metodo.
		if ((*ci).second.mFormula)
		{
			std::vector<QualifiedCell> oldCells, oldColumns, oldRows;
			std::vector<QualifiedCell> noneCells, noneColumns, noneRows;
			ComputeQualifiedPrecedents((*ci).second.mFormula, inLoc, this, fSheetResolver,
				oldCells, oldColumns, oldRows);
			ApplyPrecedentDiff(this, inLoc, oldCells, oldColumns, oldRows,
				noneCells, noneColumns, noneRows);
		}

		(*ci).second.Clear();
		fCellData.erase(ci);
	}
} /* DisposeCell */

void CContainer::ClearCellContent(const cell& inLoc)
{	CHECKLOCK
	cellmap::iterator ci = fCellData.find(inLoc);
	if (ci == fCellData.end())
		return;

	CellData& data = (*ci).second;
	if (data.mType == eTextData && data.mText)
		FREE(data.mText);
	if (data.mFormula)
	{
		// Vero grafo delle dipendenze (roadmap Tier 3): stesso identico
		// motivo di DisposeCell sopra -- questo metodo libera mFormula
		// direttamente (Canc/Backspace, vedi il commento su questo
		// metodo in Container.h), senza passare da SetCellFormula.
		std::vector<QualifiedCell> oldCells, oldColumns, oldRows;
		std::vector<QualifiedCell> noneCells, noneColumns, noneRows;
		ComputeQualifiedPrecedents(data.mFormula, inLoc, this, fSheetResolver,
			oldCells, oldColumns, oldRows);
		ApplyPrecedentDiff(this, inLoc, oldCells, oldColumns, oldRows,
			noneCells, noneColumns, noneRows);
		FREE(data.mFormula);
	}

	data.mFormula = NULL;
	data.mType = eNoData;
	data.mConstant = 0;
	data.mStatus = 0;
	// data.mStyle deliberatamente intatto -- vedi il commento su questo
	// metodo in Container.h.
} /* ClearCellContent */

void CContainer::CopyCell(CContainer *destContainer, const cell& srcLoc, const cell& destLoc,
	range *inFrom, bool isDragMove)
{	CHECKLOCK
	cellmap::iterator ci;
	CellData *cd;
	
	ci = fCellData.find(srcLoc);
	
	if (ci == fCellData.end())
		cd = NULL;
	else
	{
		CellData t = (*ci).second;
		cd = &t;
		cd->Copy();
	
		CFormula formula(cd->mFormula);
		if (this != destContainer &&
			formula.RefersToName())
		{
			if (!destContainer->fNames)
			{
				destContainer->fNames = new CNameTable;
				FailNil(destContainer->fNames);
				destContainer->fNewNames = true;
			}
			
//			BAutolock n1lock(destContainer->fNames);
//			BAutolock n2lock(fNames);
			
			int i = 0;
			CName name;
			while (formula.GetNextName(i, name))
			{
				try
				{
					CNameTable& t = *destContainer->fNames;
					if (t.find(name) == t.end())
						t[name] = (*fNames)[name];
				}
				catch(...){}
			}
		}
	}
	
	if (cd)
	{
		// Vero grafo delle dipendenze (roadmap Tier 3): questo metodo
		// scrive "fCellData[destLoc]" direttamente, mai tramite
		// SetCellFormula -- stesso trattamento prima/dopo, letto PRIMA
		// della scrittura vera perche' serve la formula ATTUALE di
		// destLoc (se c'era) per calcolare i vecchi precedenti. Il ramo
		// "cd == NULL" sotto passa gia' da DisposeCell, che si occupa
		// gia' da solo dei suoi vecchi precedenti.
		void* oldDestFormula = NULL;
		cellmap::iterator destIt = destContainer->fCellData.find(destLoc);
		if (destIt != destContainer->fCellData.end())
			oldDestFormula = destIt->second.mFormula;
		std::vector<QualifiedCell> oldCells, oldColumns, oldRows;
		ComputeQualifiedPrecedents(oldDestFormula, destLoc, destContainer,
			destContainer->fSheetResolver, oldCells, oldColumns, oldRows);

		destContainer->fCellData[destLoc] = *cd;

		if (isDragMove)
			CFormula(cd->mFormula).UpdateReferences(srcLoc,
				srcLoc.h - destLoc.h, srcLoc.v - destLoc.v, *inFrom);

		// I precedenti NUOVI si leggono DOPO l'eventuale UpdateReferences
		// sopra (che riscrive il bytecode sul posto): cd->mFormula e'
		// la STESSA memoria ormai posseduta da destContainer->fCellData
		// [destLoc] (CellData::Copy() sopra ne aveva gia' fatto una copia
		// indipendente), quindi riflette gia' i riferimenti aggiornati.
		std::vector<QualifiedCell> newCells, newColumns, newRows;
		ComputeQualifiedPrecedents(cd->mFormula, destLoc, destContainer,
			destContainer->fSheetResolver, newCells, newColumns, newRows);
		ApplyPrecedentDiff(destContainer, destLoc, oldCells, oldColumns, oldRows,
			newCells, newColumns, newRows);
	}
	else
		destContainer->DisposeCell(destLoc);
} /* CopyCell */

void CContainer::MoveCell(CContainer *destContainer, const cell& srcLoc, const cell& destLoc,
	SplitType split, int first, int count)
{	/* CHECKLOCK */
	
	cellmap::iterator ci;

	if (this != destContainer)
	{
		CopyCell(destContainer, srcLoc, destLoc);
		DisposeCell(srcLoc);
	}
	else if (srcLoc == destLoc)
	{
		if (split != noSplit)
		{
			// Vero grafo delle dipendenze (roadmap Tier 3): la formula
			// resta nella stessa cella, ma i SUOI riferimenti possono
			// spostarsi (una riga/colonna e' stata inserita/cancellata
			// nelle vicinanze) -- stessa identita' di dipendente prima e
			// dopo (srcLoc), quindi ApplyPrecedentDiff normale basta,
			// nessun bisogno del trattamento "due chiamate" del ramo
			// sotto (che invece cambia identita').
			std::vector<QualifiedCell> oldCells, oldColumns, oldRows;
			ComputeQualifiedPrecedents(fCellData[srcLoc].mFormula, srcLoc, this,
				fSheetResolver, oldCells, oldColumns, oldRows);

			CFormula f(fCellData[srcLoc].mFormula);
			f.UpdateReferences(srcLoc,
				split == hSplit, first, count);

			std::vector<QualifiedCell> newCells, newColumns, newRows;
			ComputeQualifiedPrecedents(fCellData[srcLoc].mFormula, srcLoc, this,
				fSheetResolver, newCells, newColumns, newRows);
			ApplyPrecedentDiff(this, srcLoc, oldCells, oldColumns, oldRows,
				newCells, newColumns, newRows);
		}
	}
	else if ((ci = fCellData.find(srcLoc)) != fCellData.end())
	{
		// Vero grafo delle dipendenze (roadmap Tier 3): qui la cella
		// CAMBIA identita' (srcLoc -> destLoc), quindi un ApplyPrecedentDiff
		// solo non basta -- un precedente "invariato" (presente sia prima
		// che dopo) avrebbe comunque bisogno che il suo dipendente
		// registrato passi da srcLoc a destLoc. Piu' semplice ed
		// altrettanto corretto: togliere TUTTI i bordi registrati sotto
		// (this, srcLoc), poi aggiungerli TUTTI sotto (this, destLoc) --
		// due chiamate a ApplyPrecedentDiff, non una, ognuna con un lato
		// vuoto apposta.
		std::vector<QualifiedCell> oldCells, oldColumns, oldRows;
		ComputeQualifiedPrecedents((*ci).second.mFormula, srcLoc, this,
			fSheetResolver, oldCells, oldColumns, oldRows);

		CellData cr = (*ci).second;
		fCellData.erase(ci);

		if (split != noSplit)
		{
			CFormula f(cr.mFormula);
			f.UpdateReferences(srcLoc, split == hSplit,
				first, count);
		}

		fCellData[destLoc] = cr;

		std::vector<QualifiedCell> newCells, newColumns, newRows;
		ComputeQualifiedPrecedents(cr.mFormula, destLoc, this,
			fSheetResolver, newCells, newColumns, newRows);

		static const std::vector<QualifiedCell> kEmpty;
		ApplyPrecedentDiff(this, srcLoc, oldCells, oldColumns, oldRows, kEmpty, kEmpty, kEmpty);
		ApplyPrecedentDiff(this, destLoc, kEmpty, kEmpty, kEmpty, newCells, newColumns, newRows);
	}
} /* MoveCell */

void CContainer::ExchangeCells(const cell& a, const cell& b)
{	/* CHECKLOCK */
	CellData cra, crb;
	
	GetCellData(a, cra);
	GetCellData(b, crb);

	if (cra.mType != eNoData && crb.mType != eNoData)
	{
		fCellData[a] = crb;
		fCellData[b] = cra;
	}
	else if (cra.mType == eNoData)
		MoveCell(this, b, a);
	else
		MoveCell(this, a, b);
} /* ExchangeCells */

/* Celllijst manipulaties */

bool CContainer::GetNextCellInRow(cell& c, bool mayBeEmpty)
{	/*CHECKLOCK*/
	cell pc = c;
	
	if (!GetNextCell(c, mayBeEmpty) || c.v != pc.v)
	{
		c = pc;
		return false;
	}

	return true;
} /* GetNextCellInRow */

bool CContainer::GetPreviousCellInRow(cell& c, bool mayBeEmpty)
{	/*CHECKLOCK*/
	cell pc = c;
	bool result = true;
	
	if (!GetPreviousCell(c, mayBeEmpty) || c.v != pc.v)
	{
		c = pc;
		result = false;
	}

	return result;
} /* GetPreviousCellInRow */

bool CContainer::GetNextCell(cell& c, bool mayBeEmpty)
{	/*CHECKLOCK*/
	if (GetCellCount() == 0)
		return false;

	cellmap::iterator iter;
	
	iter = fCellData.upper_bound(c);

	if (iter != fCellData.end())
	{
		if (!mayBeEmpty)
		{
			while ((*iter).second.mType == eNoData)
			{
				iter++;
				if (iter == fCellData.end())
					return false;
			}
		}

		c = (*iter).first;
		return true;
	}

	return false;
} /* GetNextCell */

bool CContainer::GetPreviousCell(cell& c, bool mayBeEmpty)
{	/*CHECKLOCK*/
	if (GetCellCount() == 0)
		return false;

	cellmap::reverse_iterator riter(fCellData.lower_bound(c));
	
	while (riter != fCellData.rend() && c <= (*riter).first)
		riter++;

	if (riter != fCellData.rend())
	{
		if (!mayBeEmpty)
		{
			while ((*riter).second.mType == eNoData)
			{
				riter++;
				if (riter == fCellData.rend())
					return false;
			}
		}

		c = (*riter).first;
		return true;
	}
	
	return false;
} /* GetPreviousCell */

/* cell inhoud manipulaties */

// FormatValue e CFormula::UnMangle scrivono nel buffer che ricevono
// senza controllarne la dimensione (Formatter.cpp, caso eTextData:
// strcpy diretto del testo della cella; Formula.cpp: concatenazioni
// dirette in fase di ricostruzione della formula) -- vanno sempre
// invocate su un buffer di appoggio abbastanza grande, MAI sul buffer
// del chiamante, il cui contenuto va poi copiato con un limite tramite
// strlcpy. Bug scoperto aprendo un file .xlsm reale con una cella di
// testo di circa 2900 caratteri (una nota introduttiva), che mandava
// in fondo allo stack un "char text[512]" tipico dei chiamanti
// (traduttori, SheetView, ecc.) e corrompeva lo stack -- non un
// crash pulito ma un blocco apparente (stesso pattern, gia' visto in
// questa sessione, per cui debug_server intercetta la corruzione).
static size_t TextBufferSizeFor(const Value& v)
{
	return (v.fType == eTextData && v.fText) ? strlen(v.fText) + 4 : 128;
}

// Limite del buffer di appoggio per la formula "smanglata": generoso
// rispetto a qualunque formula reale (le piu' lunghe viste finora,
// import XLSX con XLOOKUP/riferimenti tra fogli, restano sotto i 100
// caratteri), ma comunque un limite esplicito invece di scrivere
// senza controllo nel buffer del chiamante.
static const size_t kMaxUnmangledFormulaLength = 16384;

int CContainer::GetCellResult(const cell& inLoc, char *s, size_t bufSize, bool ignoreWidth)
{	/*CHECKLOCK*/
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
	{
		float w;
		w = (ignoreWidth || !fInView) ? 1e6 : fInView->GetColumnWidth(inLoc.h);

		CellStyle cs = gStyleTable[(*i).second.mStyle];

		Value v((*i).second);
		std::vector<char> buf(TextBufferSizeFor(v));
		int result = static_cast<int>(gFormatTable.FormatValue(cs.fFormat, v, buf.data(),
			cs.fFont, w));
		strlcpy(s, buf.data(), bufSize);
		return result;
	}
	else
	{
		s[0] = 0;
		return 0;
	}
}

void CContainer::GetCellFormula(const cell& inLoc, char *s, size_t bufSize, bool rcStyle)
{	/*CHECKLOCK*/
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
	{
		CFormula form((*i).second.mFormula);

		if (form.IsFormula())
		{
			char buf[kMaxUnmangledFormulaLength];
			form.UnMangle(buf, inLoc, this, rcStyle);
			strlcpy(s, buf, bufSize);
		}
		else
		{
			Value v((*i).second);
			std::vector<char> buf(TextBufferSizeFor(v));
			gFormatTable.FormatValue(eGeneral, v, buf.data());
			strlcpy(s, buf.data(), bufSize);
		}
	}
	else
		s[0] = 0;
}

void* CContainer::GetCellFormula(const cell& inLoc)
{	/*CHECKLOCK*/
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
		return (*i).second.mFormula;
	else
		return NULL;
} /* GetCellFormula */

void CContainer::SetCellFormula(const cell& inLoc, void *inFormula)
{	/* CHECKLOCK */
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
	{
		// Vero grafo delle dipendenze (roadmap Tier 3): i precedenti
		// VECCHI vanno letti PRIMA di liberare la formula attuale (e'
		// ancora la sua memoria), quelli NUOVI dalla formula appena
		// installata -- ApplyPrecedentDiff aggiorna solo la differenza,
		// cosi' una modifica che non tocca affatto i riferimenti (es.
		// solo il valore costante cambia) non fa nessun lavoro extra sul
		// grafo.
		std::vector<QualifiedCell> oldCells, oldColumns, oldRows;
		std::vector<QualifiedCell> newCells, newColumns, newRows;
		ComputeQualifiedPrecedents((*i).second.mFormula, inLoc, this, fSheetResolver,
			oldCells, oldColumns, oldRows);

		if ((*i).second.mFormula) free((*i).second.mFormula);
		(*i).second.mFormula = inFormula;
		(*i).second.mConstant = CFormula(inFormula).IsConstant();

		ComputeQualifiedPrecedents(inFormula, inLoc, this, fSheetResolver,
			newCells, newColumns, newRows);
		ApplyPrecedentDiff(this, inLoc, oldCells, oldColumns, oldRows,
			newCells, newColumns, newRows);
	}
} /* CContainer::SetCellFormula */

void CContainer::GetPrecedents(const cell& c, std::vector<cell>& out)
{	/* CHECKLOCK */
	out.clear();

	void* formula = GetCellFormula(c);
	if (!formula)
		return;

	std::set<cell> seen;
	CFormulaIterator iter(formula, c);
	cell ref;
	while (iter.Next(ref))
	{
		if (seen.insert(ref).second)
			out.push_back(ref);
	}
} /* CContainer::GetPrecedents */

void CContainer::GetDependents(const cell& c, std::vector<cell>& out)
{	/* CHECKLOCK */
	out.clear();

	CCellIterator iter(this);
	cell candidate;
	std::vector<cell> candidatePrecedents;
	while (iter.NextExisting(candidate))
	{
		if (!GetCellFormula(candidate))
			continue;

		GetPrecedents(candidate, candidatePrecedents);
		for (size_t i = 0; i < candidatePrecedents.size(); i++)
		{
			if (candidatePrecedents[i] == c)
			{
				out.push_back(candidate);
				break;
			}
		}
	}
} /* CContainer::GetDependents */

// Reindirizza (container, loc) al vero proprietario dello spill se "loc"
// e' una cella spillata di "container" (ApplySpill/fSpillOwnerOf) --
// una cella spillata e' sempre un VALORE, mai una formula propria, il
// suo contenuto cambia solo come effetto collaterale del ricalcolo del
// proprietario. Funzione libera (non un metodo): opera su un container
// generico, non necessariamente "this", perche' un riferimento
// incrociato puo' puntare a una cella spillata di UN ALTRO foglio.
static QualifiedCell RedirectSpillMember(CContainer* container, const cell& loc)
{
	QualifiedCell qc;
	qc.container = container;
	qc.loc = container->IsSpillMember(loc) ? container->GetSpillOwner(loc) : loc;
	return qc;
}

// Corpo vero di GetQualifiedPrecedents, estratto in una funzione libera
// che prende il puntatore alla formula DIRETTAMENTE invece di rileggerlo
// da fCellData: serve a CContainer::SetCellFormula/DisposeCell/MoveCell
// per calcolare i precedenti PRIMA e DOPO una modifica (il vecchio
// puntatore alla formula, in quel momento, non e' piu' quello che
// fCellData[inLoc].mFormula restituirebbe una volta sostituito -- vedi
// i commenti li'). "selfContainer" sostituisce il "this" implicito che
// avrebbe un metodo membro, per i riferimenti stesso-foglio.
static void ComputeQualifiedPrecedents(void* formula, const cell& formulaLoc,
	CContainer* selfContainer, ISheetResolver* resolver,
	std::vector<QualifiedCell>& outCells,
	std::vector<QualifiedCell>& outColumns,
	std::vector<QualifiedCell>& outRows)
{
	outCells.clear();
	outColumns.clear();
	outRows.clear();

	if (!formula)
		return;

	std::set<QualifiedCell> cellsSeen, columnsSeen, rowsSeen;
	CFormulaIterator iter(formula, formulaLoc);
	RawFormulaRef ref;

	while (iter.NextQualified(ref))
	{
		// Riferimenti a tabella strutturata fra fogli (Tabella12[Col]):
		// compaiono nel bytecode come un semplice valName (una stringa),
		// che NextQualified salta come ogni altro token non terminale --
		// non ancora tracciati da questo grafo, vedi il commento nel
		// piano di Fase 0 ("rimandato, non bloccante"). Una formula che
		// usa SOLO un riferimento a tabella su un altro foglio, senza
		// nessun altro riferimento a cella/intervallo, produce percio'
		// oggi zero precedenti qualificati -- non un crash, solo un
		// grafo temporaneamente incompleto per quel caso raro, finche'
		// una fase successiva non lo aggiunge.

		CContainer* target = selfContainer;
		if (ref.isCrossSheet)
		{
			target = resolver ? resolver->ResolveSheetByName(ref.sheetName.c_str()) : NULL;
			if (!target)
				// Nome di foglio non risolvibile (rinominato/cancellato
				// dopo che la formula e' stata scritta): un bordo in
				// meno, non un errore -- stesso comportamento permissivo
				// di CFormula::Calculate per lo stesso caso.
				continue;
		}

		if (!ref.isRange)
		{
			QualifiedCell qc = RedirectSpillMember(target, ref.loc);
			if (cellsSeen.insert(qc).second)
				outCells.push_back(qc);
			continue;
		}

		range r = ref.rangeVal;
		if (r.IsWholeColumn())
		{
			for (int col = r.left; col <= r.right; col++)
			{
				QualifiedCell qc;
				qc.container = target;
				qc.loc = cell(col, 0);
				if (columnsSeen.insert(qc).second)
					outColumns.push_back(qc);
			}
		}
		else if (r.IsWholeRow())
		{
			for (int row = r.top; row <= r.bottom; row++)
			{
				QualifiedCell qc;
				qc.container = target;
				qc.loc = cell(0, row);
				if (rowsSeen.insert(qc).second)
					outRows.push_back(qc);
			}
		}
		else
		{
			for (int row = r.top; row <= r.bottom; row++)
				for (int col = r.left; col <= r.right; col++)
				{
					QualifiedCell qc = RedirectSpillMember(target, cell(col, row));
					if (cellsSeen.insert(qc).second)
						outCells.push_back(qc);
				}
		}
	}
} /* ComputeQualifiedPrecedents */

// Applica ai grafi dei container coinvolti la differenza fra i
// precedenti VECCHI e NUOVI della cella "c" di "selfContainer" (rimuove
// "c" come dipendente da ogni precedente sparito, lo aggiunge a ogni
// precedente nuovo) -- unico punto usato da SetCellFormula/DisposeCell/
// MoveCell, cosi' i tre scelgono solo COME calcolare vecchio/nuovo
// (prima/dopo una modifica), non come applicarli al grafo.
void ApplyPrecedentDiff(CContainer* selfContainer, const cell& c,
	const std::vector<QualifiedCell>& oldCells, const std::vector<QualifiedCell>& oldColumns,
	const std::vector<QualifiedCell>& oldRows,
	const std::vector<QualifiedCell>& newCells, const std::vector<QualifiedCell>& newColumns,
	const std::vector<QualifiedCell>& newRows)
{
	QualifiedCell me;
	me.container = selfContainer;
	me.loc = c;

	std::set<QualifiedCell> oldCellSet(oldCells.begin(), oldCells.end());
	std::set<QualifiedCell> newCellSet(newCells.begin(), newCells.end());
	std::set<QualifiedCell> oldColSet(oldColumns.begin(), oldColumns.end());
	std::set<QualifiedCell> newColSet(newColumns.begin(), newColumns.end());
	std::set<QualifiedCell> oldRowSet(oldRows.begin(), oldRows.end());
	std::set<QualifiedCell> newRowSet(newRows.begin(), newRows.end());

	for (std::set<QualifiedCell>::iterator it = oldCellSet.begin(); it != oldCellSet.end(); it++)
	{
		if (newCellSet.count(*it))
			continue;
		std::map<cell, std::set<QualifiedCell> >& deps = it->container->fDependents;
		std::map<cell, std::set<QualifiedCell> >::iterator found = deps.find(it->loc);
		if (found != deps.end())
		{
			found->second.erase(me);
			if (found->second.empty())
				deps.erase(found);
		}
	}
	for (std::set<QualifiedCell>::iterator it = newCellSet.begin(); it != newCellSet.end(); it++)
	{
		if (oldCellSet.count(*it))
			continue;
		it->container->fDependents[it->loc].insert(me);
	}

	for (std::set<QualifiedCell>::iterator it = oldColSet.begin(); it != oldColSet.end(); it++)
	{
		if (newColSet.count(*it))
			continue;
		std::map<int, std::set<QualifiedCell> >& deps = it->container->fColumnDependents;
		std::map<int, std::set<QualifiedCell> >::iterator found = deps.find(it->loc.h);
		if (found != deps.end())
		{
			found->second.erase(me);
			if (found->second.empty())
				deps.erase(found);
		}
	}
	for (std::set<QualifiedCell>::iterator it = newColSet.begin(); it != newColSet.end(); it++)
	{
		if (oldColSet.count(*it))
			continue;
		it->container->fColumnDependents[it->loc.h].insert(me);
	}

	for (std::set<QualifiedCell>::iterator it = oldRowSet.begin(); it != oldRowSet.end(); it++)
	{
		if (newRowSet.count(*it))
			continue;
		std::map<int, std::set<QualifiedCell> >& deps = it->container->fRowDependents;
		std::map<int, std::set<QualifiedCell> >::iterator found = deps.find(it->loc.v);
		if (found != deps.end())
		{
			found->second.erase(me);
			if (found->second.empty())
				deps.erase(found);
		}
	}
	for (std::set<QualifiedCell>::iterator it = newRowSet.begin(); it != newRowSet.end(); it++)
	{
		if (oldRowSet.count(*it))
			continue;
		it->container->fRowDependents[it->loc.v].insert(me);
	}
} /* ApplyPrecedentDiff */

void CContainer::GetQualifiedPrecedents(const cell& c, ISheetResolver* resolver,
	std::vector<QualifiedCell>& outCells,
	std::vector<QualifiedCell>& outColumns,
	std::vector<QualifiedCell>& outRows)
{
	ComputeQualifiedPrecedents(GetCellFormula(c), c, this, resolver, outCells, outColumns, outRows);
} /* CContainer::GetQualifiedPrecedents */

void CContainer::PurgeDependenciesOn(CContainer* dying)
{
	for (std::map<cell, std::set<QualifiedCell> >::iterator it = fDependents.begin();
			it != fDependents.end(); )
	{
		for (std::set<QualifiedCell>::iterator dep = it->second.begin(); dep != it->second.end(); )
		{
			if (dep->container == dying)
				it->second.erase(dep++);
			else
				++dep;
		}
		if (it->second.empty())
			fDependents.erase(it++);
		else
			++it;
	}

	for (std::map<int, std::set<QualifiedCell> >::iterator it = fColumnDependents.begin();
			it != fColumnDependents.end(); )
	{
		for (std::set<QualifiedCell>::iterator dep = it->second.begin(); dep != it->second.end(); )
		{
			if (dep->container == dying)
				it->second.erase(dep++);
			else
				++dep;
		}
		if (it->second.empty())
			fColumnDependents.erase(it++);
		else
			++it;
	}

	for (std::map<int, std::set<QualifiedCell> >::iterator it = fRowDependents.begin();
			it != fRowDependents.end(); )
	{
		for (std::set<QualifiedCell>::iterator dep = it->second.begin(); dep != it->second.end(); )
		{
			if (dep->container == dying)
				it->second.erase(dep++);
			else
				++dep;
		}
		if (it->second.empty())
			fRowDependents.erase(it++);
		else
			++it;
	}
} /* CContainer::PurgeDependenciesOn */

void CContainer::RegisterDependencyEdges(const cell& c,
	const std::vector<QualifiedCell>& precedentCells,
	const std::vector<QualifiedCell>& precedentColumns,
	const std::vector<QualifiedCell>& precedentRows)
{
	static const std::vector<QualifiedCell> kEmpty;
	ApplyPrecedentDiff(this, c, kEmpty, kEmpty, kEmpty,
		precedentCells, precedentColumns, precedentRows);
} /* CContainer::RegisterDependencyEdges */

bool CContainer::GetCellData(const cell& inLoc, CellData& outData)
{	/*CHECKLOCK*/
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
	{
		outData = (*i).second;
		return true;
	}
	else
	{
		outData.mType = eNoData;
		outData.mStyle = fDefaultCellStyle;
		outData.mFormula = (void *)NULL;
		outData.mConstant = false;
		return false;
	}
} /* GetCellData */

bool CContainer::GetValue(const cell& inCell, Value& outValue)
{	/*CHECKLOCK*/
	cellmap::iterator i;

	if (!inCell.IsValid())
	{
		outValue = gRefNan;
		return false;
	}
	else if ((i = fCellData.find(inCell)) != fCellData.end())
	{
		outValue = (*i).second;
		return true;
	}
	else
	{
		outValue.fType = eNoData;
		return false;
	}
} /* GetValue */

int CContainer::GetType(const cell& inLoc)
{
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
		return (*i).second.mType;
	else
		return eNoData;
} /* CContainer::GetType */

double CContainer::GetDouble(const cell& inLoc)
{
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end() && (*i).second.mType == eNumData)
		return (*i).second.mDouble;
	else
		return gValueNan;
} /* CContainer::GetDouble */

void CContainer::SetValue(const cell& inLoc, const Value& inData)
{	/* CHECKLOCK */;
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
		(*i).second = inData;
	else
		NewCell(inLoc, inData, NULL);
} /* SetCellData */

int CContainer::CompareValue(const cell& a, const cell& b)
{	/*CHECKLOCK*/
	Value va, vb;
	
	GetValue(a, va);
	GetValue(b, vb);
	
	if (va.CompareLT(vb, this))
		return -1;
	else if (va.CompareEQ(vb, this))
		return 0;
	else
		return 1;
} /* CompareCellData */

int CContainer::GetCellDepth(const cell& inLoc)
{	/*CHECKLOCK*/
	return fCellData[inLoc].mStatus;
} /* GetCellDepth */

void CContainer::SetCellDepth(const cell& inLoc, int inDepth)
{	/* CHECKLOCK */
	fCellData[inLoc].mStatus = inDepth;
} /* SetCellDepth */

bool CContainer::CellIsConstant(const cell& inLoc)
{	/*CHECKLOCK*/
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
		return (*i).second.mConstant;
	else
		return true;
} /* CellIsConstant */

bool CContainer::CellHasFormula(const cell& inLoc)
{	/*CHECKLOCK*/
	cellmap::iterator i;

	if ((i = fCellData.find(inLoc)) != fCellData.end())
		return (*i).second.mFormula != NULL;
	else
		return true;
} /* CellHasFormula */

bool CContainer::RefersToNamedRange(const char *inName)
{	/*CHECKLOCK*/
	cellmap::iterator i;
	
	for (i = fCellData.begin(); i != fCellData.end(); i++)
	{
		CFormula form((*i).second.mFormula);
		if (form.RefersToName(inName))
			return true;
	}
	
	return false;
} /* CContainer::RefersToNamedRange */

range CContainer::ResolveName(const char *name, cell inLocation, CContainer **outOwner)
{
	if (outOwner)
		*outOwner = this;

	// (*fNames)[name] (std::map::operator[]) inserirebbe silenziosamente
	// una voce vuota per un nome inesistente invece di segnalare
	// l'errore -- find() e' l'unico modo corretto di interrogare la
	// tabella senza l'effetto collaterale di crearne una voce fantasma.
	// Bug scoperto in Fase 7 scrivendo ui/tests/test_names.cpp: "Vai a"
	// su un nome appena eliminato finiva per risolversi silenziosamente
	// su range(0,0,0,0) (cella non valida) invece di segnalare che il
	// nome non esiste piu'.
	if (fNames)
	{
		CNameTable::iterator i = fNames->find(name);
		if (i != fNames->end())
			return i->second;
	}

	// Riferimento a tabella strutturata di Excel ("Tabella12[Codice]",
	// Fase 14): riconosciuto dalla presenza di "[" -- un nome di
	// intervallo normale non puo' mai contenerne uno (nel lessico, "["
	// avvia sempre e solo un token TBLCOL, vedi lexer.cpp). Il nome
	// della tabella e' tutto cio' che precede "[", il nome della
	// colonna tutto cio' che sta fra "[" e "]" (CParser::
	// ParseTableReference li ha gia' concatenati esattamente cosi').
	const char *bracket = strchr(name, '[');
	if (bracket)
	{
		std::string tableName(name, bracket - name);
		std::map<std::string, CTableDef>::const_iterator ti = fTables.find(tableName);
		if (ti != fTables.end())
		{
			std::string colSpec(bracket + 1);
			if (!colSpec.empty() && colSpec[colSpec.size() - 1] == ']')
				colSpec.erase(colSpec.size() - 1);

			const CTableDef& def = ti->second;

			// "Tabella[#Col]" (Fase 16, codifica di
			// "Tabella[[#This Row],[Col]]" fatta da
			// CParser::ParseTableReference): riga della FORMULA
			// stessa (inLocation), non l'intera colonna -- "#" non e'
			// mai il primo carattere di un vero nome di colonna (ne'
			// in Excel ne' qui), quindi non e' ambiguo. Se la riga
			// della formula cade fuori dai dati della tabella (la
			// formula e' stata copiata/spostata fuori dalla tabella),
			// resta semplicemente non risolto, come un nome
			// inesistente.
			if (!colSpec.empty() && colSpec[0] == '#')
			{
				std::string colName = colSpec.substr(1);
				if (inLocation.v >= def.dataRange.top && inLocation.v <= def.dataRange.bottom)
				{
					for (size_t ci = 0; ci < def.columnNames.size(); ci++)
					{
						if (colName == def.columnNames[ci])
						{
							int col = def.dataRange.left + (int)ci;
							return range(col, inLocation.v, col, inLocation.v);
						}
					}
				}
			}
			else
			{
				// "Tabella[Col1:Col2]" (Fase 16, codifica di
				// "Tabella[[Col1]:[Col2]]"): intervallo multi-colonna,
				// tutte le righe dati della tabella -- un vero nome di
				// colonna non contiene mai ":", quindi la presenza di
				// ":" distingue senza ambiguita' questo caso dalla
				// colonna singola sotto.
				size_t colon = colSpec.find(':');
				if (colon != std::string::npos)
				{
					std::string col1Name = colSpec.substr(0, colon);
					std::string col2Name = colSpec.substr(colon + 1);
					int idx1 = -1, idx2 = -1;
					for (size_t ci = 0; ci < def.columnNames.size(); ci++)
					{
						if (col1Name == def.columnNames[ci])
							idx1 = (int)ci;
						if (col2Name == def.columnNames[ci])
							idx2 = (int)ci;
					}
					if (idx1 >= 0 && idx2 >= 0)
					{
						range r = def.dataRange;
						r.left = def.dataRange.left + std::min(idx1, idx2);
						r.right = def.dataRange.left + std::max(idx1, idx2);
						return r;
					}
				}
				else
				{
					// Colonna singola (Fase 14, comportamento originale).
					for (size_t ci = 0; ci < def.columnNames.size(); ci++)
					{
						if (colSpec == def.columnNames[ci])
						{
							range r = def.dataRange;
							int col = r.left + (int)ci;
							r.left = r.right = col;
							return r;
						}
					}
				}
			}
		}
		// Tabella non registrata su QUESTO foglio (Fase 15): a
		// differenza di un nome/intervallo normale (sempre locale al
		// foglio, vedi fNames sopra), il nome di una tabella Excel e'
		// unico in tutta la cartella di lavoro -- una formula puo'
		// legittimamente referenziarla da un foglio riepilogativo
		// diverso da quello che possiede davvero i dati (bug reale:
		// XLOOKUP su un foglio "Indice" verso una tabella definita su
		// un foglio dati separato restituiva sempre NaN). fSheetResolver
		// e' NULL per un documento a un solo foglio (mai collegato a
		// una cartella multi-foglio): in quel caso, come sempre, il
		// riferimento semplicemente non si risolve, nessun crash.
		if (fSheetResolver)
		{
			CContainer *owner = fSheetResolver->FindSheetWithTable(tableName);
			if (owner && owner != this)
				return owner->ResolveName(name, inLocation, outOwner);
		}
	}

	// Tabella o colonna inesistenti (su questo foglio o su ogni altro
	// foglio collegato): stesso errKeyNotFound di un nome di intervallo
	// non trovato sopra, che risale a gNameNan in Formula.cpp -- nessun
	// nuovo tipo di errore, un riferimento a una tabella non ancora
	// importata/rinominata resta comunque una formula, non degrada a
	// testo puro.
	THROW((errKeyNotFound));
} /* CContainer::ResolveName */

CNameTable *CContainer::GetOrCreateNameTable()
{
	if (!fNames)
	{
		fNames = new CNameTable;
		fNewNames = true;
	}
	return fNames;
} /* CContainer::GetOrCreateNameTable */

long CContainer::CountCells(range *inRange)
{
	StLocker<CContainer> lock(this);

	range all(1, 1, kColCount, kRowCount);
	if (!inRange)
		inRange = &all;
	
	CCellIterator iter(this, inRange);
	
	long result = 0;
	cell c;
	while (iter.NextExisting(c))
		result++;
	
	return result;
} /* CContainer::CountCells */

bool CContainer::Exists(const cell& inLoc)
{
	return fCellData.find(inLoc) != fCellData.end();
} /* CContainer::Exists */

bool CContainer::GetMergedRange(cell c, range* outRange) const
{
	for (size_t i = 0; i < fMergedRanges.size(); i++)
	{
		if (fMergedRanges[i].Contains(c))
		{
			*outRange = fMergedRanges[i];
			return true;
		}
	}
	return false;
} /* CContainer::GetMergedRange */

void CContainer::CollectFunctionNrs(CSet& funcs)
{	/*CHECKLOCK*/
	cellmap::iterator i;
	
	for (i = fCellData.begin(); i != fCellData.end(); i++)
	{
		CFormula form((*i).second.mFormula);
		form.CollectFunctionNrs(funcs);
	}
} /* CContainer::CollectFunctionNrs */

int CContainer::CollectFontList(int *fontList)
{	/*CHECKLOCK*/
	int result = 0;
	cellmap::iterator i;
	
	for (i = fCellData.begin(); i != fCellData.end(); i++)
	{
		int fontNr = gStyleTable[(*i).second.mStyle].fFont;
		bool isNew = true;
		
		for (int i = 0; i < result && isNew; i++)
			if (fontNr == fontList[i])
				isNew = false;
		
		if (isNew)
			fontList[result++] = fontNr;
	}
	
	for (int i = 1; i < kColCount; i++)
	{
		CellStyle cs;
		GetColumnStyle(i, cs);
		int fontNr = cs.fFont;
		bool isNew = true;
		
		for (int i = 0; i < result && isNew; i++)
			if (fontNr == fontList[i])
				isNew = false;
		
		if (isNew)
			fontList[result++] = fontNr;
	}
	
	return result;
} // CContainer::CollectFontList

int CContainer::CollectFormats(int *formatList)
{	/*CHECKLOCK*/
	int result = 0;
	cellmap::iterator i;
	
	for (i = fCellData.begin(); i != fCellData.end(); i++)
	{
		int formatNr = gStyleTable[(*i).second.mStyle].fFormat;
		bool isNew = true;
		
		for (int i = 0; i < result && isNew; i++)
			if (formatNr == formatList[i])
				isNew = false;
		
		if (isNew)
			formatList[result++] = formatNr;
	}

	for (int i = 1; i < kColCount; i++)
	{
		CellStyle cs;
		GetColumnStyle(i, cs);
		int formatNr = cs.fFormat;
		bool isNew = true;
		
		for (int i = 0; i < result && isNew; i++)
			if (formatNr == formatList[i])
				isNew = false;
		
		if (isNew)
			formatList[result++] = formatNr;
	}
	
	return result;
} // CContainer::CollectFormats

