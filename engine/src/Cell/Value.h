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
	Value.h

	Copyright 1997, Hekkelman Programmatuur
	Portions Copyright 2026 Andrea Bernardi

	Part of Sum-It for the BeBox version 1.1.

*/

/*** Revision History
 ***
 *** TPV (2000-Feb-06) Removed need for global header "sum-it.pch++"
 *** TPV (2000-Feb-06) Added Headers Guards
 ***/

#ifndef VALUE_H
#define VALUE_H

#ifndef   CELL_H
#include "Cell.h"
#endif

#ifndef   RANGE_H
#include "Range.h"
#endif

#include <cmath>
#include <ctime>

struct CellData;
class CContainer;

extern double gValueNan;

enum ValueType {
	eNoData,
	eBoolData,
	eNumData,
	eTimeData,
	eUnused,			// was ooit datedata
	eTextData,
	eRangeData,			// Niet echt een mogelijk type, maar wel handig bij berekeningen.
	// Confronto range-scalare dentro un argomento di funzione (es.
	// "FILTER(A2:A8;B2:B8>=20)", Tier 2 di "Path to full Excel parity"):
	// un booleano PER CELLA del range confrontato, mai persistito in una
	// cella vera (stesso principio di eRangeData sopra -- vedi il
	// collasso in Container.graph.cpp per una formula il cui risultato
	// FINALE e' di questo tipo), non aggiunto a kCompareTypes in
	// Value.cpp (resta a 7 righe/colonne: gli operatori di confronto
	// intercettano questo tipo PRIMA di raggiungere quella tabella).
	eBoolArrayData
};

struct Value {
	Value();
	Value(double d);
	Value(const char *s, bool inCopy = true);
	Value(bool b);
	Value(time_t t);
	Value(CellData& cd);
	// Fase 29: senza questo, il costruttore di copia GENERATO DAL
	// COMPILATORE copiava fText bit per bit (un puntatore, non una vera
	// stringa) -- due Value con fType==eTextData e fTextIsCopy==true
	// finivano per condividere lo STESSO blocco allocato, e ~Value()
	// lo liberava due volte (un vero doppio-free, non solo teorico:
	// scoperto mettendo Value dentro uno std::vector, che copia i suoi
	// elementi in giro -- es. durante una riallocazione -- ogni volta
	// che serve). operator=(const Value&) gia' faceva la copia
	// profonda giusta (STRDUP), ma un operatore di assegnazione non
	// costruisce mai un oggetto NUOVO da zero: serviva un costruttore
	// di copia a parte, che qui NON puo' chiamare Clear() come fa
	// operator= (Clear() legge fType/fText/fTextIsCopy assumendo che
	// siano gia' validi, ma su un oggetto appena allocato sono ancora
	// indefiniti).
	Value(const Value& other);
	~Value();

	void operator+=(Value &);
	void operator-=(Value &);
	void operator*=(Value &);
	void operator/=(Value &);
	void operator^=(Value &);
	void operator&=(Value &);
	void operator|=(Value &);
	
	// Confronti veri e propri (Tier 2 di "Path to full Excel parity",
	// es. "FILTER(A2:A8;B2:B8>=20)"): metodi con nome, NON operatori
	// sovraccaricati -- il C++ vieta a un operatore binario membro di
	// avere piu' di un parametro esplicito, con o senza valore
	// predefinito ([over.oper]), e qui serve "inContainer" per
	// rileggere le celle vere quando questo o l'altro operando e' un
	// range multi-cella (eRangeData) senza un fRangeContainer proprio,
	// per costruire un risultato eBoolArrayData invece del singolo
	// booleano sbagliato di prima. Il tipo di ritorno e' Value, non
	// bool, per poter rappresentare anche il caso array. Le uniche
	// chiamate a questi confronti in tutto il repository sono le sei
	// in Formula.cpp (verificato con grep) -- non c'era nessun altro
	// chiamante di "operator<" & co. da aggiornare.
	Value CompareLT(Value &v, CContainer *inContainer);
	Value CompareLE(Value &v, CContainer *inContainer);
	Value CompareGT(Value &v, CContainer *inContainer);
	Value CompareGE(Value &v, CContainer *inContainer);
	Value CompareEQ(Value &v, CContainer *inContainer);
	Value CompareNE(Value &v, CContainer *inContainer);

	void operator=(const Value &inValue);
	void operator=(const double d);
	void operator=(const char *inString);
	void operator=(const bool b);
	void operator=(const range& r);
	void operator=(const time_t& t);
	
	inline operator double() const;
	inline operator const char *() const;
	inline operator bool() const;
	inline operator range() const;
	inline operator time_t() const;
	
	inline bool IsNan();
	
	void operator=(const CellData&);

//	static Value& Error(int errno);

	void Clear();

	union {
		double		fDouble;
		char*			fText;
		bool			fBool;
		time_t		fTime;
		_range		fRange;
		// eBoolArrayData: POSSEDUTO da questa Value (allocato in new[],
		// liberato in delete[]), a differenza di fRangeContainer sotto
		// (mai posseduto) -- stessa disciplina di copia profonda gia'
		// in vigore per fText/fTextIsCopy, non quella di fRangeContainer.
		bool*			fBoolArray;
	};
	ValueType 	fType;
	bool 		fTextIsCopy;
//	ValueType 	fType			: 8;
//	bool 		fTextIsCopy		: 8;

	// Fase 15 (riferimenti a tabella strutturata fra fogli diversi):
	// NULL nel caso comune (fRange, quando valido, va letto dal
	// CContainer che la funzione/il calcolo gia' ha sotto mano -- lo
	// stesso "cells"/"inContainer" di sempre). Diventa non-NULL SOLO
	// quando CContainer::ResolveName risolve "Tabella[Colonna]" su un
	// foglio diverso da quello della formula (vedi Formula.cpp, caso
	// valName): dice a chi consuma il range (XLOOKUP/VLOOKUP/HLOOKUP/
	// MATCH/INDEX/COUNTIFS, vedi FunctionUtils::GetRangeContainer) da
	// quale documento leggere DAVVERO le celle del range, non da quello
	// in cui la formula stessa vive. Puntatore preso in prestito, MAI
	// posseduto da Value (stesso principio di CContainer::fSheetResolver).
	CContainer*	fRangeContainer;

	// Numero di elementi validi in fBoolArray quando fType==eBoolArrayData
	// -- fuori dall'union sopra per lo stesso motivo di fRangeContainer,
	// deve convivere con fBoolArray (che invece STA nell'union: sono due
	// campi indipendenti dello stesso Value, non alternative fra loro).
	int			fArrayCount;
};

/*
	Inlines:
*/

inline Value::operator double() const
{
	if (fType == eNumData)
		return fDouble;
	else
		return gValueNan;
}

inline Value::operator const char *() const
{
	if (fType == eTextData)
		return fText;
	else
		return "Error";
}

inline Value::operator bool() const
{
	if (fType == eBoolData)
		return fBool;
	else
		return false;
}

inline Value::operator time_t() const
{
	if (fType == eTimeData)
		return fTime;
	else
		return 0;
}

inline Value::operator range() const
{
	if (fType == eRangeData)
		return fRange;
	else
		return range();
}

inline bool Value::IsNan()
{
	return (fType == eNumData && std::isnan(fDouble));
}

#endif
