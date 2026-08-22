/*
	AutoFill.h

	Riconoscimento della serie per il riempimento automatico (Fase 29,
	maniglia in basso a destra della selezione, come Excel/LibreOffice
	Calc): dati i valori GIA' presenti in una riga/colonna della
	selezione, genera i valori successivi che la maniglia scrive
	trascinando.

	Logica pura, separata dalla vista (SheetView) per essere testabile
	senza sessione grafica -- stesso principio di Chart.h/Pivot.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef AUTO_FILL_H
#define AUTO_FILL_H

#include <vector>

#include "Value.h"

// Analizza sourceValues (nell'ordine in cui compaiono nel foglio, es.
// dall'alto in basso per un riempimento verticale) e genera "count"
// nuovi valori che continuano la stessa sequenza:
//  - una sola cella sorgente: ripete quel valore "count" volte (stesso
//    comportamento di Riempi a destra/in basso gia' esistenti);
//  - due o piu' celle numeriche (eNumData) con differenza costante fra
//    celle consecutive: prosegue la progressione aritmetica (1,2,3 ->
//    4,5,6; 2,4,6 -> 8,10,12);
//  - due o piu' celle data/ora (eTimeData) con lo stesso passo in
//    secondi: prosegue la progressione con lo stesso passo;
//  - qualunque altro caso (testo, tipi misti, progressione non
//    aritmetica): ripete ciclicamente i valori sorgente, stesso
//    comportamento di Excel trascinando celle senza un pattern
//    riconoscibile.
std::vector<Value> GenerateAutoFillSequence(const std::vector<Value>& sourceValues, int count);

#endif
