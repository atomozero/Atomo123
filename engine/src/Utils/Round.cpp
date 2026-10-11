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
	Round.c
	
	Copyright 1997, Hekkelman Programmatuur
	
	Part of Sum-It for the BeBox version 1.1.

*/

#include "Round.h"

#include <cmath>

// Always rounds "away from zero" at the halfway point (2.5->3, not 2
// as banker's rounding to the nearest even integer would give): this
// is how real Excel's ROUND() behaves, verified while investigating
// agile-kanban-board.xlsx (=ROUND(H35*G2,0)) -- found here because
// Functions.math.cpp::ROUNDFunction never actually called this
// function at all, it had its own separate, broken duplicate
// implementation (see there for details), now fixed to call this one.
double Round(double d)
{
	double heel, deel;
	deel = modf(fabs(d), &heel);

	if (deel >= 0.5)
		heel += 1.0;

	return d < 0 ? -heel : heel;
}

double Round(double d, int digits)
{
	if (d == 0)
		return d;
	else
	{
		// Era "scale = ceil(log10(fabs(d))) + digits - 1" (un tentativo
		// di evitare perdita di precisione per numeri enormi scalando
		// il fattore in base alla grandezza di d) -- bug reale trovato
		// costruendo una demo vera (ROUND(67.76;2) dava 67.759, tre
		// decimali non due): ceil(log10(d)) aggiunge una cifra di scala
		// in piu' ogni volta che la parte intera di d ha 2 o piu' cifre
		// (qualunque d non esattamente una potenza di 10), quindi
		// ROUND arrotondava sistematicamente a digits+1 decimali per
		// la stragrande maggioranza dei numeri reali -- mascherato nei
		// test precedenti perche' con digits=0 (il caso ROUND(3.5,0)
		// gia' corretto sopra) lo scarto si annulla, e con una parte
		// intera a una sola cifra (d<10) il bug coincide per caso col
		// risultato corretto. Il fattore 10^digits diretto e' esattamente
		// la stessa cosa che Excel fa (nessuna perdita di precisione in
		// piu' rispetto al limite dei double che esiste comunque in
		// Excel stesso).
		double factor = pow(10, digits);

		return Round(d * factor) / factor;
	}
}
