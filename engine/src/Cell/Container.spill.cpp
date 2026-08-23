/*
	Container.spill.c

	Formule array/spill range (Fase 29, backlog v3.0 "Consolidation"):
	vedi il commento su ApplySpill/ClearSpill in Container.h per il
	design completo. File separato dal resto di Container.cpp per lo
	stesso motivo di Container.graph.cpp -- una responsabilita' isolata,
	facile da trovare e da testare a parte.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "Cell.h"
#include "Value.h"
#include "Container.h"

void CContainer::ClearSpill(const cell& owner)
{
	std::map<cell, range>::iterator rangeIt = fSpillRangeOf.find(owner);
	if (rangeIt == fSpillRangeOf.end())
		return;

	range old = rangeIt->second;
	for (int row = old.top; row <= old.bottom; row++)
	{
		for (int col = old.left; col <= old.right; col++)
		{
			cell c(col, row);
			if (c == owner)
				continue; // l'owner tiene la propria formula, non e' mai da svuotare qui

			std::map<cell, cell>::iterator memberIt = fSpillOwnerOf.find(c);
			// Solo se "c" e' ANCORA segnata come membro di QUESTO owner:
			// se nel frattempo un altro spill (di un altro owner) si e'
			// esteso fin li' (caso raro ma possibile dopo una modifica
			// intermedia), non e' piu' nostra da toccare.
			if (memberIt != fSpillOwnerOf.end() && memberIt->second == owner)
			{
				DisposeCell(c);
				fSpillOwnerOf.erase(memberIt);
			}
		}
	}
	fSpillRangeOf.erase(rangeIt);
}

bool CContainer::ApplySpill(const cell& owner, int rows, int cols, const std::vector<Value>& values)
{
	ClearSpill(owner);

	if (rows <= 0 || cols <= 0 || (int)values.size() != rows * cols)
		return false;

	range target(owner.h, owner.v, owner.h + cols - 1, owner.v + rows - 1);

	// Collisione = una cella bersaglio (diversa dall'owner) ha una
	// FORMULA PROPRIA -- vedi il commento esteso in Container.h sul
	// perche' non basta "una cella qualunque con contenuto".
	for (int row = target.top; row <= target.bottom; row++)
	{
		for (int col = target.left; col <= target.right; col++)
		{
			cell c(col, row);
			if (c == owner)
				continue;

			cellmap::iterator existing = fCellData.find(c);
			if (existing != fCellData.end() && existing->second.mFormula)
				return false;
		}
	}

	for (int row = target.top; row <= target.bottom; row++)
	{
		for (int col = target.left; col <= target.right; col++)
		{
			cell c(col, row);
			int idx = (row - target.top) * cols + (col - target.left);
			if (c != owner)
				NewCell(c, values[idx], NULL);
			fSpillOwnerOf[c] = owner;
		}
	}
	fSpillRangeOf[owner] = target;
	return true;
}

cell CContainer::GetSpillOwner(const cell& c) const
{
	std::map<cell, cell>::const_iterator it = fSpillOwnerOf.find(c);
	return (it != fSpillOwnerOf.end()) ? it->second : c;
}

range CContainer::GetSpillRange(const cell& owner) const
{
	std::map<cell, range>::const_iterator it = fSpillRangeOf.find(owner);
	return (it != fSpillRangeOf.end()) ? it->second : range();
}
