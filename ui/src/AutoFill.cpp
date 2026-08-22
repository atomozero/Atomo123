/*
	AutoFill.cpp

	Vedi AutoFill.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "AutoFill.h"

#include <cmath>

// Tolleranza per il confronto fra passi in virgola mobile: una
// progressione scritta a mano (es. 0.1, 0.2, 0.3) puo' avere un passo
// che non e' esattamente identico bit per bit da una coppia all'altra
// per via dell'arrotondamento in base 2 -- confrontare con "==" la
// rifiuterebbe quasi sempre come "non costante".
static const double kAutoFillEpsilon = 1e-9;

std::vector<Value> GenerateAutoFillSequence(const std::vector<Value>& sourceValues, int count)
{
	std::vector<Value> result;
	if (count <= 0 || sourceValues.empty())
		return result;

	size_t n = sourceValues.size();

	// Una sola cella sorgente: nessuna progressione possibile da una
	// lunghezza 1, si ripete lo stesso valore -- comportamento
	// invariato rispetto a Riempi a destra/in basso gia' esistenti
	// (SheetView::FillRight/FillDown).
	if (n < 2)
	{
		for (int i = 0; i < count; i++)
			result.push_back(sourceValues[0]);
		return result;
	}

	// Progressione aritmetica di numeri: tutte le celle sorgente sono
	// eNumData e la differenza fra celle consecutive e' sempre la
	// stessa (entro kAutoFillEpsilon).
	bool allNum = true;
	for (size_t i = 0; i < n && allNum; i++)
		allNum = (sourceValues[i].fType == eNumData);

	if (allNum)
	{
		double step = (double)sourceValues[1] - (double)sourceValues[0];
		bool constantStep = true;
		for (size_t i = 2; i < n && constantStep; i++)
		{
			double d = (double)sourceValues[i] - (double)sourceValues[i - 1];
			if (std::fabs(d - step) > kAutoFillEpsilon)
				constantStep = false;
		}
		if (constantStep)
		{
			double last = (double)sourceValues[n - 1];
			for (int i = 0; i < count; i++)
				result.push_back(Value(last + step * (i + 1)));
			return result;
		}
	}

	// Progressione di date/ore: stesso principio ma su eTimeData -- il
	// passo e' in secondi, tipicamente un multiplo di 86400 per una
	// progressione di giorni interi, ma qualunque passo costante (una
	// settimana, un'ora) viene comunque riconosciuto.
	bool allTime = true;
	for (size_t i = 0; i < n && allTime; i++)
		allTime = (sourceValues[i].fType == eTimeData);

	if (allTime)
	{
		time_t step = (time_t)sourceValues[1] - (time_t)sourceValues[0];
		bool constantStep = true;
		for (size_t i = 2; i < n && constantStep; i++)
		{
			time_t d = (time_t)sourceValues[i] - (time_t)sourceValues[i - 1];
			if (d != step)
				constantStep = false;
		}
		if (constantStep)
		{
			time_t last = (time_t)sourceValues[n - 1];
			for (int i = 0; i < count; i++)
				result.push_back(Value((time_t)(last + step * (i + 1))));
			return result;
		}
	}

	// Nessun pattern riconosciuto (testo, tipi misti, o una
	// progressione non aritmetica): ripete ciclicamente i valori
	// sorgente, come farebbe Excel trascinando celle senza un pattern
	// riconoscibile -- con una sola forma "reale" di valore nel
	// sorgente, questo produce comunque una ripetizione semplice.
	for (int i = 0; i < count; i++)
		result.push_back(sourceValues[i % n]);
	return result;
}
