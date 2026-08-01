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
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include <support/Debug.h>

#include <cstring>
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

	data = inValue;
	data.mFormula = inFormula;
	data.mConstant = !inFormula || CFormula(inFormula).IsConstant();
	data.mStyle = fDefaultCellStyle;

	fCellData[inLocation] = data;
} /* NewCell */

void CContainer::DisposeCell(const cell& inLoc)
{	CHECKLOCK
	cellmap::iterator ci;

	ci = fCellData.find(inLoc);

	if (ci != fCellData.end())
	{
		(*ci).second.Clear();
		fCellData.erase(ci);
	}
} /* DisposeCell */

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
		destContainer->fCellData[destLoc] = *cd;
	else
		destContainer->DisposeCell(destLoc);
	
	if (isDragMove && cd)
		CFormula(cd->mFormula).UpdateReferences(srcLoc,
			srcLoc.h - destLoc.h, srcLoc.v - destLoc.v, *inFrom);
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
			CFormula f(fCellData[srcLoc].mFormula);
			f.UpdateReferences(srcLoc,
				split == hSplit, first, count);
		}
	}
	else if ((ci = fCellData.find(srcLoc)) != fCellData.end())
	{
		CellData cr = (*ci).second;
		fCellData.erase(ci);
		
		if (split != noSplit)
		{
			CFormula f(cr.mFormula);
			f.UpdateReferences(srcLoc, split == hSplit,
				first, count);
		}
		
		fCellData[destLoc] = cr;
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
		if ((*i).second.mFormula) free((*i).second.mFormula);
		(*i).second.mFormula = inFormula;
		(*i).second.mConstant = CFormula(inFormula).IsConstant();
	}
} /* CContainer::SetCellFormula */

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
	
	if (va < vb)
		return -1;
	else if (va == vb)
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

range CContainer::ResolveName(const char *name)
{
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

