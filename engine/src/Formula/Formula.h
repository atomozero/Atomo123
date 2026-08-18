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
	Formula.h
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.pch++"
 *** TPV (2000-Feb-06) Added Headers Guards
 ***/

#ifndef FORMULA_H
#define FORMULA_H

#ifndef DEFINE_CELL
	#ifndef   CELL_H
	#include "Cell.h"
	#endif
	
	#ifndef   RANGE_H
	#include "Range.h"
	#endif
#endif

#include <BeBuild.h>
#include <DataIO.h>
#include <cstddef>

struct Value;
class CContainer;
class CNameTable;
class CSet;
class CName;

enum PFToken {
	opEnd, opRaise, opMul, opDiv, opRoot,
	opPlus, opMinus, opNegate, opLT, opLE,
	opEQ, opGE, opGT, opNE, opFunc,
	valNum, valBool, valStr, valCell, valNil,
	valRange, valPerc, valTime, opNOT, opAND,
	opOR, valName, opParen,
	valXRef, valXRange,
	// Aggiunto in fondo apposta, mai in mezzo: questi valori sono
	// scritti cosi' come sono nel bytecode di CFormula (vedi
	// CFormula::UnMangle/CalcCell, che leggono fString[indx] come un
	// PFToken grezzo) -- un valore inserito in mezzo sposterebbe gli
	// ordinali di tutti quelli dopo, corrompendo silenziosamente ogni
	// formula gia' salvata in un documento .ascd esistente.
	opPercent
};

const long
	kMaxStackHeight = 25,
	// Buffer per ogni livello dello stack di CFormula::UnMangle: le
	// concatenazioni intermedie (Formula.cpp) scrivono qui senza
	// controllare la lunghezza, quindi un termine (o un risultato
	// parziale) piu' lungo del limite corrompe l'area allocata --
	// 256 (il valore storico di Sum-It) e' risultato troppo stretto
	// per formule Excel moderne con riferimenti a tabelle strutturate
	// e funzioni lunghe (es. XLOOKUP); alzato con ampio margine.
	kMaxStringLength = 4096,
	kPFWordSize = sizeof(int32),
	kPFAlignBits = kPFWordSize - 1;

struct FuncCallData {
	short funcNr;
	short argCnt;
};

// Riferimento a un'altra scheda della stessa cartella di lavoro
// (formula "NomeFoglio!Cella"/"NomeFoglio!Cella:Cella", Fase 9): il
// NOME del foglio (non un indice) e' incorporato nel bytecode come
// stringa terminata da NUL, esattamente come valName per i nomi di
// intervallo -- risolto in fase di CALCOLO (mai di parsing) tramite
// CContainer::GetSheetResolver() (vedi Container.h). Stesso principio
// del vecchio "XRef" di Sum-It (un fileNr per riferimenti fra file
// separati, mai completato: vedi i commenti "R4Hack" ancora presenti
// in Formula.cpp), riusato per fogli della stessa cartella invece che
// file esterni, ma per nome invece che per indice -- un indice
// risolto gia' in fase di parsing richiederebbe che il foglio
// referenziato esista gia' in quel momento, cosa falsa quando un
// foglio viene ri-analizzato da solo durante il caricamento di un
// file (vedi il commento su ISheetResolver in Container.h).
//
// Layout in memoria (bytecode) di valXRef: nome del foglio (NUL-
// terminato) seguito da una "cell" (allineamento non garantito, letta
// sempre byte a byte). valXRange: stesso nome, seguito da una "range".
// Nessuna struct dedicata: gli accessi vanno tramite aritmetica sui
// puntatori (vedi Formula.cpp), stesso principio gia' usato per
// valStr/valName.

class CFormula {
	friend class CFormulaIterator;
public:
	CFormula();
	CFormula(void *inString);
//	~CFormula();

	void AddToken(PFToken inToken, const void *inData, int& ioOffset);
	void Calculate(cell inLocation, Value& outResult, CContainer *inContainer) const;
	// decSepOverride/listSepOverride: se diversi da 0, forzano quel
	// separatore decimale/di elenco invece di gDecimalPoint/
	// gListSeparator (usati altrimenti quando rcStyle=false; rcStyle=
	// true forza gia' da solo '.'/',' ma passa anche a riferimenti in
	// stile R1C1, non quello che serve qui) -- decSepOverride!=0 in
	// piu' normalizza il separatore di intervallo interno ("..",
	// pensato solo per il giro testuale ASCD/barra formule) in ":"
	// (Excel/ODF vogliono quello). odfRefs: avvolge i riferimenti a
	// cella/intervallo con "[." e "]" (sintassi ODF) invece del testo
	// A1 nudo. Pensati per l'export XLSX/ODS (BuildSheetXml/
	// BuildContentXml), che ha bisogno di testo di formula portabile
	// indipendente dalle preferenze locali dell'utente -- nessun
	// chiamante esistente passa questi parametri, quindi il
	// comportamento di default resta identico.
	void UnMangle(char *outString, cell inLocation, CContainer *inContainer, bool rcStyle = false,
		char decSepOverride = 0, char listSepOverride = 0, bool odfRefs = false) const;
	// Vero se la formula contiene almeno un riferimento a un altro
	// foglio (valXRef/valXRange). Usato dall'export XLSX/ODS per
	// decidere se scrivere la formula viva o solo il suo valore gia'
	// calcolato: l'export scrive oggi un solo foglio per file, un
	// riferimento a un foglio diverso punterebbe percio' a dati che
	// nel file esportato non esistono affatto.
	bool ReferencesOtherSheet() const;
	void UpdateReferences(cell inLocation, bool horizontal, int first, int count);
	void UpdateReferences(cell inLocation, int h, int v, range src);
	void* UpdateReferences(cell inLoc, range src, range dst);
	
	bool ReduceToValue(Value& val, CContainer *inContainer) const;
	
	bool RefersToName(const char *name = NULL) const;
	bool GetNextName(int& indx, CName& name) const;
	
	bool IsFormula() const
		{ return fString != NULL; }
	bool IsConstant() const;
	
	void CollectFunctionNrs(CSet& funcs) const;
	
	void Clear();
	void* DetachString();
	void* CopyString() const;
	long StringLength() const;
	void operator=(void *inString);
	
	void Write(BPositionIO& inStream);
	void Read(BPositionIO& inStream, int *inFuncList);
	
	void ReadOldMacFormula(BPositionIO& inStream, cell inLoc);
	
	long operator[](int indx) const
		{ return fString[indx]; };

private:
	int32 *fString;
};

class CFormulaIterator {
public:
	typedef long IterData[5];

	CFormulaIterator(void*, cell inLocation);
	
	bool Next(cell&);

	void GetData(IterData& outData);
	void SetData(const IterData& inData);

private:
	int32 *fString;
	int fIndex;
	range fRange;
	cell fLocation;
};

extern bool gWithEqualSign;

#endif
