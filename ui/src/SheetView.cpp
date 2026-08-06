/*
	SheetView.cpp

	Vedi SheetView.h.
*/

#include "SheetView.h"
#include "MainWindow.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <Alert.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <Cursor.h>
#include <DataIO.h>
#include <MenuItem.h>
#include <MessageFilter.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <ScrollBar.h>
#include <String.h>
#include <TextControl.h>
#include <TextView.h>
#include <TranslatorRoster.h>

#include "Value.h"
#include "Container.h"
#include "CellIterator.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "Constants.h"
#include "Formatter.h"
#include "FontMetrics.h"
#include "Preferences.h"

#include "AscdIO.h"

static const uint32 kMsgCellEditCommit = 'cedt';
static const uint32 kMsgCellEditCancel = 'cedc';

// Definita piu' sotto (vicino a IsDefaultBg), usata sia da
// RecalculateWrappedRowHeights che da Draw() -- dichiarata qui perche'
// la prima viene prima nel file.
static std::vector<BString> WrapTextLines(BView* view, const char* text, float maxWidth);

// Filtro applicato alla BTextView interna del BTextControl usato per
// l'editing in-cella: la BTextView interna e' quella che riceve
// davvero il fuoco tastiera (BTextControl::MakeFocus lo inoltra a
// lei), quindi e' li' che serve intercettare Escape/Invio -- una
// BTextControl non riceve KeyDown direttamente per questo motivo.
//
// Invio e' gestito qui esplicitamente (invece di affidarsi
// all'Invoke() automatico su Invio che BTextControl dovrebbe fare da
// sola) perche' quel meccanismo non scattava in modo affidabile in
// questo contesto -- segnalato dall'utente: "se scrivo un numero e
// premo invio non succede nulla, devo cliccare con il mouse per
// confermare" (il click funziona perche' SheetView::MouseDown chiama
// CommitEditing() direttamente come chiamata C++, non tramite
// Invoke()/messaggio). Stessa soluzione, applicata allo stesso modo
// di Escape: invece di dipendere dal comportamento predefinito del
// Interface Kit, si intercetta la pressione e si manda il messaggio
// di commit esplicitamente.
class CellEditKeyFilter : public BMessageFilter {
public:
	CellEditKeyFilter(BHandler* target)
		: BMessageFilter(B_KEY_DOWN), fTarget(target) {}

	virtual filter_result Filter(BMessage* message, BHandler** target)
	{
		int32 rawChar;
		if (message->FindInt32("raw_char", &rawChar) != B_OK)
			return B_DISPATCH_MESSAGE;

		if (rawChar == B_ESCAPE)
		{
			BMessenger(fTarget).SendMessage(kMsgCellEditCancel);
			return B_SKIP_MESSAGE;
		}
		if (rawChar == B_RETURN)
		{
			BMessenger(fTarget).SendMessage(kMsgCellEditCommit);
			return B_SKIP_MESSAGE;
		}
		return B_DISPATCH_MESSAGE;
	}

private:
	BHandler* fTarget;
};

SheetView::SheetView(CContainer* doc)
	:
	BView(FullCanvasFrame(), "SheetView", B_FOLLOW_NONE,
		B_WILL_DRAW | B_FRAME_EVENTS),
	fResizingColumn(0),
	fResizingRow(0),
	fResizeDragStart(0),
	fResizeStartSize(0),
	fHoverCursor(0),
	fShowGrid(gPrefs ? gPrefs->GetPrefInt("showGrid", 1) != 0 : true),
	fFrozenRows(0),
	fFrozenCols(0),
	fDoc(doc),
	fSelection(1, 1),
	fAnchor(1, 1),
	fDragging(false),
	fCharts(NULL),
	fImages(NULL),
	fDraggingImageIndex(-1),
	fResizingImageIndex(-1),
	fHasAutoFilter(false),
	fEditor(NULL),
	fEditingCell(1, 1)
{
	SetViewColor(255, 255, 255);

	// Tutte le colonne/righe partono alla larghezza/altezza
	// predefinita; fColOffsets/fRowOffsets (la somma cumulativa) si
	// costruiscono una volta sola qui e poi solo quando l'utente
	// ridimensiona davvero qualcosa (vedi RebuildColumnOffsets/
	// RebuildRowOffsets).
	fColWidths.assign(kColCount, kColWidth);
	fRowHeights.assign(kRowCount, kRowHeight);
	fRowHidden.assign(kRowCount, false);
	RebuildColumnOffsets();
	RebuildRowOffsets();

	// Senza questi limiti espliciti, BView deriva i propri suggerimenti
	// di dimensione (Min/Max/PreferredSize) dal Frame() della view --
	// qui il canvas virtuale intero (FullCanvasFrame(), ~56000x328000
	// pixel). BScrollView li interroga per capire quanto spazio offrire
	// nel layout, quindi senza porre un limite esplicito richiede
	// quella stessa dimensione enorme -- non solo alla prima passata di
	// layout (un ResizeTo() una tantum sulla BScrollView "risolveva"
	// quel caso, vedi MainWindow::MainWindow), ma a OGNI ricalcolo
	// successivo del layout, riportando la BScrollView alla dimensione
	// ereditata. Effetto pratico: Parent()->Bounds(), usato da
	// ScrollToShowSelection per sapere quanto e' davvero visibile,
	// tornava a riflettere quella dimensione enorme dopo il primo
	// ricalcolo, facendo credere che qualunque cella fosse sempre
	// gia' visibile -- lo scroll automatico verso la selezione
	// smetteva di scattare (bug segnalato dall'utente, non riprodotto
	// dal test in una finestra sintetica con una sola passata di
	// layout).
	SetExplicitMinSize(BSize(100, 100));
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
	SetExplicitPreferredSize(BSize(400, 300));
}

SheetView::~SheetView()
{
}

void SheetView::AttachedToWindow()
{
	BView::AttachedToWindow();
	MakeFocus(true);
	FixupScrollBars();
}

void SheetView::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	FixupScrollBars();
}

void SheetView::ScrollTo(BPoint where)
{
	// Le intestazioni di colonna (in cima) e di riga (a sinistra)
	// restano "incollate" al bordo della viewport (vedi Draw(),
	// headerTop = Bounds().top, rowHeaderLeft = Bounds().left): lo
	// scroll blitta pero' i pixel gia' disegnati verso la loro nuova
	// posizione a schermo (ottimizzazione standard di ScrollBy()/
	// ScrollTo(), che invaliderebbe da sola solo la striscia appena
	// esposta), quindi le bande delle intestazioni, gia' disegnate
	// alla vecchia posizione, verrebbero spostate insieme al resto
	// invece di restare ferme -- finendo "in mezzo" alle celle come
	// bande grigie fantasma, invece di sparire. Un Invalidate() di
	// tutta la vista risolverebbe il sintomo ma perderebbe il blit
	// efficiente per OGNI pixel (non solo le intestazioni), causando
	// lo sfarfallio notato dall'utente ("le celle sembrano scivolare
	// sotto la riga delle colonne"): si invalidano invece solo le
	// quattro bande (vecchia/nuova per ciascun asse) alte/larghe
	// quanto le intestazioni che servono davvero -- quelle vecchie
	// (dove il blit ha lasciato il fantasma, da ridisegnare come
	// celle normali) e quelle nuove (dove ora devono comparire le
	// intestazioni vere, altrimenti si vedrebbe il contenuto delle
	// celle scorso li' sotto).
	float oldTop = Bounds().top;
	float oldLeft = Bounds().left;
	BView::ScrollTo(where);

	BRect oldColHeaderBand(Bounds().left, oldTop, Bounds().right, oldTop + kHeaderHeight - 1);
	BRect newColHeaderBand(Bounds().left, where.y, Bounds().right, where.y + kHeaderHeight - 1);
	Invalidate(oldColHeaderBand);
	Invalidate(newColHeaderBand);

	BRect oldRowHeaderBand(oldLeft, Bounds().top, oldLeft + kHeaderWidth - 1, Bounds().bottom);
	BRect newRowHeaderBand(where.x, Bounds().top, where.x + kHeaderWidth - 1, Bounds().bottom);
	Invalidate(oldRowHeaderBand);
	Invalidate(newRowHeaderBand);

	// Blocca riquadri: le bande di celle congelate (non solo le
	// intestazioni sopra) sono anch'esse "incollate" allo schermo
	// durante lo scroll (vedi Draw()) -- stesso identico problema e
	// stessa soluzione: senza queste invalidazioni esplicite il blit
	// automatico di ScrollTo() le sposterebbe via come fantasmi
	// invece di lasciarle ferme.
	if (fFrozenRows > 0)
	{
		float frozenH = fRowOffsets[fFrozenRows];
		BRect oldBand(Bounds().left, oldTop + kHeaderHeight,
			Bounds().right, oldTop + kHeaderHeight + frozenH - 1);
		BRect newBand(Bounds().left, where.y + kHeaderHeight,
			Bounds().right, where.y + kHeaderHeight + frozenH - 1);
		Invalidate(oldBand);
		Invalidate(newBand);
	}
	if (fFrozenCols > 0)
	{
		float frozenW = fColOffsets[fFrozenCols];
		BRect oldBand(oldLeft + kHeaderWidth, Bounds().top,
			oldLeft + kHeaderWidth + frozenW - 1, Bounds().bottom);
		BRect newBand(where.x + kHeaderWidth, Bounds().top,
			where.x + kHeaderWidth + frozenW - 1, Bounds().bottom);
		Invalidate(oldBand);
		Invalidate(newBand);
	}
}

// Frame() copre l'intero intervallo virtuale del motore
// (kColCount/kRowCount in Config/Constants.h) fin dalla costruzione
// (vedi FullCanvasFrame()), non i limiti reali dei dati inseriti --
// coerente con un foglio di calcolo vero, dove si puo' sempre
// scorrere oltre l'ultima cella con contenuto.
BRect SheetView::FullCanvasFrame()
{
	return BRect(0, 0,
		kHeaderWidth + kColCount * kColWidth - 1,
		kHeaderHeight + kRowCount * kRowHeight - 1);
}

void SheetView::FixupScrollBars()
{
	BScrollBar* hsb = ScrollBar(B_HORIZONTAL);
	BScrollBar* vsb = ScrollBar(B_VERTICAL);
	if (!hsb || !vsb)
		return;

	float totalWidth = kHeaderWidth + fColOffsets[kColCount];
	float totalHeight = kHeaderHeight + fRowOffsets[kRowCount];

	// Bounds() riflette sempre la dimensione piena del Frame() (vedi
	// sopra), non la porzione effettivamente visibile: la vera area
	// visibile e' quella del genitore (la BScrollView stessa).
	BRect viewport = Parent() ? Parent()->Bounds() : Bounds();

	float maxH = totalWidth - viewport.Width();
	if (maxH < 0)
		maxH = 0;
	hsb->SetRange(0, maxH);
	hsb->SetProportion(viewport.Width() / totalWidth);
	hsb->SetSteps(kColWidth, viewport.Width());

	float maxV = totalHeight - viewport.Height();
	if (maxV < 0)
		maxV = 0;
	vsb->SetRange(0, maxV);
	vsb->SetProportion(viewport.Height() / totalHeight);
	vsb->SetSteps(kRowHeight, viewport.Height());
}

void SheetView::RebuildColumnOffsets()
{
	fColOffsets.resize(kColCount + 1);
	fColOffsets[0] = 0;
	for (int i = 0; i < kColCount; i++)
		fColOffsets[i + 1] = fColOffsets[i] + fColWidths[i];
}

void SheetView::SetColumnWidths(const std::vector<std::pair<int, float> >& widths)
{
	fColWidths.assign(kColCount, kColWidth);
	for (size_t i = 0; i < widths.size(); i++)
	{
		int col = widths[i].first;
		if (col < 1 || col > kColCount)
			continue;
		float w = widths[i].second;
		if (w < kMinColWidth)
			w = kMinColWidth;
		fColWidths[col - 1] = w;
	}
	RebuildColumnOffsets();
	UpdateCanvasSize();
	Invalidate();
}

void SheetView::SetShowGrid(bool show)
{
	fShowGrid = show;
	Invalidate();
}

void SheetView::SetFreezePanes(int rows, int cols)
{
	if (rows < 0)
		rows = 0;
	if (rows > kRowCount - 1)
		rows = kRowCount - 1;
	if (cols < 0)
		cols = 0;
	if (cols > kColCount - 1)
		cols = kColCount - 1;

	fFrozenRows = rows;
	fFrozenCols = cols;
	Invalidate();
}

void SheetView::ToggleFreezePanes()
{
	if (HasFreezePanes())
		SetFreezePanes(0, 0);
	else
		SetFreezePanes(fSelection.v - 1, fSelection.h - 1);
}

std::vector<std::pair<int, float> > SheetView::CustomColumnWidths() const
{
	std::vector<std::pair<int, float> > result;
	for (int i = 0; i < kColCount; i++)
	{
		if (fColWidths[i] != kColWidth)
			result.push_back(std::make_pair(i + 1, fColWidths[i]));
	}
	return result;
}

void SheetView::SetRowHeights(const std::vector<std::pair<int, float> >& heights)
{
	fRowHeights.assign(kRowCount, kRowHeight);
	for (size_t i = 0; i < heights.size(); i++)
	{
		int row = heights[i].first;
		if (row < 1 || row > kRowCount)
			continue;
		float h = heights[i].second;
		if (h < kMinRowHeight)
			h = kMinRowHeight;
		fRowHeights[row - 1] = h;
	}
	RebuildRowOffsets();
	UpdateCanvasSize();
	Invalidate();
}

std::vector<std::pair<int, float> > SheetView::CustomRowHeights() const
{
	std::vector<std::pair<int, float> > result;
	for (int i = 0; i < kRowCount; i++)
	{
		if (fRowHeights[i] != kRowHeight)
			result.push_back(std::make_pair(i + 1, fRowHeights[i]));
	}
	return result;
}

void SheetView::SetHiddenRows(const std::vector<int>& rows)
{
	fRowHidden.assign(kRowCount, false);
	for (size_t i = 0; i < rows.size(); i++)
	{
		int row = rows[i];
		if (row >= 1 && row <= kRowCount)
			fRowHidden[row - 1] = true;
	}
	RebuildRowOffsets();
	UpdateCanvasSize();
	Invalidate();
}

std::vector<int> SheetView::HiddenRows() const
{
	std::vector<int> result;
	for (int i = 0; i < kRowCount; i++)
		if (fRowHidden[i])
			result.push_back(i + 1);
	return result;
}

bool SheetView::IsRowHidden(int row) const
{
	if (row < 1 || row > kRowCount)
		return false;
	return fRowHidden[row - 1];
}

BRect SheetView::AutoFilterArrowRect(int col) const
{
	BRect r = CellRect(cell(col, fAutoFilterRange.top));
	return BRect(r.right - 12, r.bottom - 10, r.right - 4, r.bottom - 4);
}

void SheetView::SetAutoFilter(range headerRange)
{
	fHasAutoFilter = true;
	fAutoFilterRange = headerRange;
	fFilterHiddenValues.clear();
	Invalidate();
}

void SheetView::ClearAutoFilter()
{
	fHasAutoFilter = false;
	fFilterHiddenValues.clear();
	Invalidate();
}

int SheetView::AutoFilterDataBottom() const
{
	if (!fDoc)
		return fAutoFilterRange.top;
	range bounds;
	fDoc->GetBounds(bounds);
	int bottom = bounds.bottom;
	if (bottom < fAutoFilterRange.top)
		bottom = fAutoFilterRange.top;
	if (bottom > kRowCount)
		bottom = kRowCount;
	return bottom;
}

std::vector<BString> SheetView::UniqueColumnValues(int col) const
{
	std::vector<BString> result;
	if (!fHasAutoFilter || !fDoc || col < fAutoFilterRange.left || col > fAutoFilterRange.right)
		return result;

	int bottom = AutoFilterDataBottom();
	for (int row = fAutoFilterRange.top + 1; row <= bottom; row++)
	{
		char text[4096];
		fDoc->GetCellResult(cell(col, row), text, sizeof(text), true);
		BString value(text);

		bool found = false;
		for (size_t i = 0; i < result.size(); i++)
		{
			if (result[i] == value)
			{
				found = true;
				break;
			}
		}
		if (!found)
			result.push_back(value);
	}
	return result;
}

bool SheetView::IsColumnValueVisible(int col, const BString& value) const
{
	std::map<int, std::vector<BString> >::const_iterator it = fFilterHiddenValues.find(col);
	if (it == fFilterHiddenValues.end())
		return true;
	for (size_t i = 0; i < it->second.size(); i++)
	{
		if (it->second[i] == value)
			return false;
	}
	return true;
}

void SheetView::SetColumnValueHidden(int col, const BString& value, bool hidden)
{
	std::vector<BString>& hiddenValues = fFilterHiddenValues[col];

	int existingIndex = -1;
	for (size_t i = 0; i < hiddenValues.size(); i++)
	{
		if (hiddenValues[i] == value)
		{
			existingIndex = (int)i;
			break;
		}
	}

	if (hidden && existingIndex < 0)
		hiddenValues.push_back(value);
	else if (!hidden && existingIndex >= 0)
		hiddenValues.erase(hiddenValues.begin() + existingIndex);

	RecomputeAutoFilterVisibility();
}

void SheetView::ClearColumnFilters()
{
	fFilterHiddenValues.clear();
	RecomputeAutoFilterVisibility();
}

// Ripassa ogni riga dei dati filtrati e decide se va nascosta in base
// a TUTTI i criteri per colonna attivi contemporaneamente (AND fra
// colonne, come Excel vero: una riga resta nascosta se anche una sola
// colonna filtrata la esclude) -- richiamata dopo OGNI cambio di
// criterio invece di limitarsi alle sole righe toccate, altrimenti un
// criterio su una seconda colonna non potrebbe mai "aggiungersi" a
// quello gia' attivo su un'altra senza ricalcolare da zero.
void SheetView::RecomputeAutoFilterVisibility()
{
	if (!fHasAutoFilter || !fDoc)
		return;

	int bottom = AutoFilterDataBottom();
	for (int row = fAutoFilterRange.top + 1; row <= bottom; row++)
	{
		bool hide = false;
		for (std::map<int, std::vector<BString> >::const_iterator it = fFilterHiddenValues.begin();
			it != fFilterHiddenValues.end() && !hide; ++it)
		{
			if (it->second.empty())
				continue;
			char text[4096];
			fDoc->GetCellResult(cell(it->first, row), text, sizeof(text), true);
			BString value(text);
			for (size_t i = 0; i < it->second.size(); i++)
			{
				if (it->second[i] == value)
				{
					hide = true;
					break;
				}
			}
		}
		fRowHidden[row - 1] = hide;
	}
	RebuildRowOffsets();
	UpdateCanvasSize();
	Invalidate();
}

void SheetView::ShowAutoFilterMenu(int col, BPoint screenAnchor)
{
	std::vector<BString> values = UniqueColumnValues(col);

	BPopUpMenu menu("autofilter");
	std::vector<BMenuItem*> valueItems;
	for (size_t i = 0; i < values.size(); i++)
	{
		BMenuItem* item = new BMenuItem(values[i].String(), NULL);
		item->SetMarked(IsColumnValueVisible(col, values[i]));
		menu.AddItem(item);
		valueItems.push_back(item);
	}
	if (!values.empty())
		menu.AddSeparatorItem();
	BMenuItem* clearItem = new BMenuItem("Mostra tutto", NULL);
	menu.AddItem(clearItem);

	ConvertToScreen(&screenAnchor);
	// Sincrono (asynchronous=false): Go() blocca finche' l'utente non
	// sceglie una voce o chiude il menu, restituendo direttamente la
	// voce scelta (o NULL) -- niente passaggio di messaggi da gestire
	// altrove in MessageReceived, tutta la logica resta qui.
	BMenuItem* chosen = menu.Go(screenAnchor, false, false, true);
	if (!chosen)
		return;

	if (chosen == clearItem)
	{
		ClearColumnFilters();
		return;
	}
	for (size_t i = 0; i < valueItems.size(); i++)
	{
		if (valueItems[i] == chosen)
		{
			// Un clic su una voce sempre visibile la nasconde e
			// viceversa -- lo stesso significato di "spuntato" nel
			// menu (IsColumnValueVisible sopra, usato anche per
			// SetMarked qualche riga fa).
			bool wasVisible = IsColumnValueVisible(col, values[i]);
			SetColumnValueHidden(col, values[i], wasVisible);
			break;
		}
	}
}

void SheetView::RecalculateWrappedRowHeights()
{
	if (!fDoc)
		return;

	std::vector<int> neededLines(kRowCount + 1, 0); // 1-based, indice 0 inutilizzato

	CCellIterator iter(fDoc, NULL);
	cell c;
	while (iter.NextExisting(c))
	{
		// Celle unite (Fase 12): solo la cella in alto a sinistra ha
		// testo proprio (stesso principio di DrawCellBand -- le altre
		// nell'intervallo, anche se anch'esse marcate a capo,
		// restano sempre vuote qui sotto), ma il calcolo dell'a capo
		// deve usare la larghezza dell'INTERO intervallo unito, non
		// solo la prima colonna: una descrizione lunga in una cella
		// unita su piu' colonne (comune per un'intestazione
		// descrittiva in una sola colonna stretta) andava a capo su
		// molte piu' righe del necessario contando solo quella prima
		// colonna -- bug reale scoperto confrontando con Excel vero
		// una fattura reale: righe altissime dove Excel le mostra a
		// una riga sola, perche' la cella e' in realta' unita su
		// sei colonne.
		range mergedRange;
		bool isMerged = fDoc->GetMergedRange(c, &mergedRange);
		if (isMerged && (mergedRange.top != c.v || mergedRange.left != c.h))
			continue;

		CellStyle cs;
		fDoc->GetCellStyle(c, cs);
		if (!cs.fWrapText)
			continue;

		char text[4096];
		fDoc->GetCellResult(c, text, sizeof(text), true);
		if (text[0] == 0)
			continue;

		// Stesso font della cella per una misura coerente con Draw()
		// (StringWidth dipende dal font attivo sulla view).
		gFontSizeTable.SetFontID(this, cs.fFont);

		float width;
		if (isMerged)
		{
			width = 0;
			for (int col = mergedRange.left; col <= mergedRange.right; col++)
				width += fColWidths[col - 1];
		}
		else
			width = fColWidths[c.h - 1];
		width -= 6;

		int lines = (int)WrapTextLines(this, text, width).size();
		if (lines > neededLines[c.v])
			neededLines[c.v] = lines;
	}
	SetFont(be_plain_font);

	bool changed = false;
	for (int row = 1; row <= kRowCount; row++)
	{
		if (neededLines[row] == 0)
			continue;
		float needed = neededLines[row] * kRowHeight;
		if (needed > fRowHeights[row - 1])
		{
			fRowHeights[row - 1] = needed;
			changed = true;
		}
	}

	if (changed)
	{
		RebuildRowOffsets();
		UpdateCanvasSize();
		Invalidate();
	}
}

void SheetView::RebuildRowOffsets()
{
	fRowOffsets.resize(kRowCount + 1);
	fRowOffsets[0] = 0;
	for (int i = 0; i < kRowCount; i++)
	{
		// Una riga nascosta (AutoFilter o "hidden" da XLSX) occupa zero
		// spazio a schermo, esattamente come in Excel -- fRowHeights[i]
		// resta intatta (la sua "altezza vera", ripristinata quando
		// torna visibile), solo il contributo alla somma cumulativa
		// diventa zero.
		float h = fRowHidden[i] ? 0 : fRowHeights[i];
		fRowOffsets[i + 1] = fRowOffsets[i] + h;
	}
}

int SheetView::ColumnAtX(float x) const
{
	// upper_bound trova il primo offset STRETTAMENTE maggiore di x:
	// l'indice prima di quello (meno 1, per passare da "confine" a
	// "colonna") e' la colonna che contiene x. Bloccato dentro
	// [1, kColCount] perche' fColOffsets copre solo quell'intervallo
	// (x negativo o oltre l'ultima colonna non deve uscire dall'array).
	std::vector<float>::const_iterator it =
		std::upper_bound(fColOffsets.begin(), fColOffsets.end(), x);
	int col = (int)(it - fColOffsets.begin());
	if (col < 1) col = 1;
	if (col > kColCount) col = kColCount;
	return col;
}

int SheetView::RowAtY(float y) const
{
	std::vector<float>::const_iterator it =
		std::upper_bound(fRowOffsets.begin(), fRowOffsets.end(), y);
	int row = (int)(it - fRowOffsets.begin());
	if (row < 1) row = 1;
	if (row > kRowCount) row = kRowCount;
	return row;
}

int SheetView::ColumnBoundaryAt(float x) const
{
	int col = ColumnAtX(x);
	// Bordo destro di "col" (che e' anche l'inizio di "col + 1").
	if (fabs(x - fColOffsets[col]) <= kResizeGrip)
		return col;
	// Bordo sinistro di "col" (che e' il bordo destro di "col - 1").
	if (col > 1 && fabs(x - fColOffsets[col - 1]) <= kResizeGrip)
		return col - 1;
	return 0;
}

int SheetView::RowBoundaryAt(float y) const
{
	int row = RowAtY(y);
	if (fabs(y - fRowOffsets[row]) <= kResizeGrip)
		return row;
	if (row > 1 && fabs(y - fRowOffsets[row - 1]) <= kResizeGrip)
		return row - 1;
	return 0;
}

void SheetView::UpdateCanvasSize()
{
	ResizeTo(kHeaderWidth + fColOffsets[kColCount] - 1,
		kHeaderHeight + fRowOffsets[kRowCount] - 1);
}

void SheetView::SetDocument(CContainer* doc)
{
	fDoc = doc;
	fSelection.Set(1, 1);
	fAnchor.Set(1, 1);
	// Le istantanee di Annulla/Ripeti si riferiscono al documento
	// precedente: applicarle a questo (nuovo/appena aperto) scambierebbe
	// il contenuto di celle senza relazione.
	fUndoStack.clear();
	fRedoStack.clear();
	Invalidate();
	NotifySelectionChanged();
}

// Rettangolo (in coordinate cella, non pixel) che contiene sia
// l'ultima selezione sia quella nuova: e' l'area minima da
// invalidare quando la selezione cambia -- invalidare le due sole
// celle (vecchia e nuova) non basta piu' come bastava a selezione
// singola, perche' un range che si accorcia deve comunque far
// scomparire l'evidenziazione delle celle che ne uscivano.
static range UnionRange(const range& a, const range& b)
{
	range r;
	r.left = std::min(a.left, b.left);
	r.top = std::min(a.top, b.top);
	r.right = std::max(a.right, b.right);
	r.bottom = std::max(a.bottom, b.bottom);
	return r;
}

void SheetView::SetSelection(cell c)
{
	if (c.h < 1)
		c.h = 1;
	if (c.v < 1)
		c.v = 1;

	range oldRange = SelectionRange();
	if (c == fSelection && c == fAnchor)
		return;

	// Catturato PRIMA di sovrascrivere fSelection: se la vecchia cella
	// attiva era l'angolo di una cella unita, il riquadro disegnato da
	// Draw() era esteso a tutto l'intervallo (vedi ActiveCellRect) --
	// invalidare solo CellRect(oldRange) grezzo lascerebbe fuori quella
	// parte, un "fantasma" del vecchio riquadro blu mai ripulito. Bug
	// reale segnalato dall'utente subito dopo il fix del clic sulle
	// celle unite.
	BRect oldActive = ActiveCellRect(fSelection);

	fSelection = c;
	fAnchor = c;

	range newRange = SelectionRange();
	BRect invalid = CellRect(UnionRange(oldRange, newRange).TopLeft());
	invalid = invalid | CellRect(UnionRange(oldRange, newRange).BotRight());
	invalid = invalid | oldActive | ActiveCellRect(fSelection);
	Invalidate(invalid);

	ScrollToShowSelection();
	NotifySelectionChanged();
}

void SheetView::ExtendSelection(cell c)
{
	if (c.h < 1)
		c.h = 1;
	if (c.v < 1)
		c.v = 1;
	if (c == fSelection)
		return;

	range oldRange = SelectionRange();
	// Vedi il commento in SetSelection sopra, stesso motivo.
	BRect oldActive = ActiveCellRect(fSelection);
	fSelection = c;
	range newRange = SelectionRange();

	BRect invalid = CellRect(UnionRange(oldRange, newRange).TopLeft());
	invalid = invalid | CellRect(UnionRange(oldRange, newRange).BotRight());
	invalid = invalid | oldActive | ActiveCellRect(fSelection);
	Invalidate(invalid);

	ScrollToShowSelection();
	NotifySelectionChanged();
}

range SheetView::SelectionRange() const
{
	range r;
	r.left = std::min(fAnchor.h, fSelection.h);
	r.right = std::max(fAnchor.h, fSelection.h);
	r.top = std::min(fAnchor.v, fSelection.v);
	r.bottom = std::max(fAnchor.v, fSelection.v);
	return r;
}

void SheetView::SelectAll()
{
	if (!fDoc)
		return;

	range bounds;
	fDoc->GetBounds(bounds);

	range oldRange = SelectionRange();
	if (bounds.right >= 1 && bounds.bottom >= 1)
	{
		fAnchor.Set(1, 1);
		fSelection.Set(bounds.right, bounds.bottom);
	}
	else
	{
		fAnchor.Set(1, 1);
		fSelection.Set(1, 1);
	}

	range newRange = SelectionRange();
	BRect invalid = CellRect(UnionRange(oldRange, newRange).TopLeft());
	invalid = invalid | CellRect(UnionRange(oldRange, newRange).BotRight());
	Invalidate(invalid);

	NotifySelectionChanged();
}

void SheetView::ClearSelection()
{
	if (!fDoc)
		return;

	range sel = SelectionRange();
	SaveUndoState(sel);

	CCellIterator iter(fDoc, &sel);
	cell c;
	while (iter.NextExisting(c))
		fDoc->DisposeCell(c);

	RecalculateOwningWorkbook();
	Invalidate(CellRect(sel.TopLeft()) | CellRect(sel.BotRight()));
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

void SheetView::FillDown()
{
	if (!fDoc)
		return;

	range sel = SelectionRange();
	if (sel.top == sel.bottom)
		return; // una sola riga: niente da riempire

	SaveUndoState(sel);

	for (int col = sel.left; col <= sel.right; col++)
	{
		cell src(col, sel.top);
		for (int row = sel.top + 1; row <= sel.bottom; row++)
			fDoc->CopyCell(fDoc, src, cell(col, row));
	}

	RecalculateOwningWorkbook();
	Invalidate(CellRect(sel.TopLeft()) | CellRect(sel.BotRight()));
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

void SheetView::FillRight()
{
	if (!fDoc)
		return;

	range sel = SelectionRange();
	if (sel.left == sel.right)
		return; // una sola colonna: niente da riempire

	SaveUndoState(sel);

	for (int row = sel.top; row <= sel.bottom; row++)
	{
		cell src(sel.left, row);
		for (int col = sel.left + 1; col <= sel.right; col++)
			fDoc->CopyCell(fDoc, src, cell(col, row));
	}

	RecalculateOwningWorkbook();
	Invalidate(CellRect(sel.TopLeft()) | CellRect(sel.BotRight()));
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

namespace {
	// Una riga dell'intervallo da ordinare: il testo grezzo di ogni
	// cella (stesso formato di GetCellFormula/TryToParseString, cosi'
	// spostare una riga non tocca ne' appiattisce le formule che
	// contiene) piu' il valore della colonna chiave, gia' estratto una
	// volta sola per non dover rileggere il documento a ogni confronto
	// durante l'ordinamento.
	struct SortRow {
		std::vector<std::string> cellText;
		bool keyIsNum;
		double keyNum;
		std::string keyText;
		int originalIndex; // per l'ordinamento stabile
	};

	bool CompareSortRows(const SortRow& a, const SortRow& b, bool ascending)
	{
		int cmp;
		if (a.keyIsNum && b.keyIsNum)
			cmp = (a.keyNum < b.keyNum) ? -1 : (a.keyNum > b.keyNum) ? 1 : 0;
		else if (a.keyIsNum != b.keyIsNum)
			// I numeri vengono sempre prima del testo, indipendentemente
			// dalla direzione -- stessa convenzione di Excel/LibreOffice
			// Calc per un confronto tra tipi diversi.
			cmp = a.keyIsNum ? -1 : 1;
		else
			cmp = strcasecmp(a.keyText.c_str(), b.keyText.c_str());

		if (cmp == 0)
			// Ordinamento stabile: a parita' di chiave, mantiene
			// l'ordine originale invece di uno arbitrario (std::sort
			// non e' stabile di suo).
			return a.originalIndex < b.originalIndex;

		return ascending ? (cmp < 0) : (cmp > 0);
	}
}

void SheetView::SortSelection(bool ascending)
{
	if (!fDoc)
		return;

	range sel = SelectionRange();
	if (sel.top == sel.bottom)
		return; // una sola riga: niente da ordinare

	SaveUndoState(sel);

	int numRows = sel.bottom - sel.top + 1;
	int numCols = sel.right - sel.left + 1;

	std::vector<SortRow> rows(numRows);
	for (int i = 0; i < numRows; i++)
	{
		int row = sel.top + i;
		rows[i].cellText.resize(numCols);
		rows[i].originalIndex = i;

		for (int j = 0; j < numCols; j++)
		{
			char text[4096];
			fDoc->GetCellFormula(cell(sel.left + j, row), text, sizeof(text), false);
			rows[i].cellText[j] = text;
		}

		Value keyVal;
		fDoc->GetValue(cell(sel.left, row), keyVal);
		if (keyVal.fType == eNumData && !keyVal.IsNan())
		{
			rows[i].keyIsNum = true;
			rows[i].keyNum = (double)keyVal;
		}
		else
		{
			rows[i].keyIsNum = false;
			rows[i].keyText = rows[i].cellText[0];
		}
	}

	std::stable_sort(rows.begin(), rows.end(),
		[ascending](const SortRow& a, const SortRow& b) {
			return CompareSortRows(a, b, ascending);
		});

	for (int i = 0; i < numRows; i++)
	{
		int row = sel.top + i;
		for (int j = 0; j < numCols; j++)
		{
			cell c(sel.left + j, row);
			const std::string& text = rows[i].cellText[j];
			if (text.empty())
				fDoc->DisposeCell(c);
			else
			{
				try
				{
					TryToParseString(text.c_str(), c, fDoc, true);
				}
				catch (...)
				{
				}
			}
		}
	}

	RecalculateOwningWorkbook();
	Invalidate(CellRect(sel.TopLeft()) | CellRect(sel.BotRight()));
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

void SheetView::InsertRows()
{
	if (!fDoc)
		return;

	range sel = SelectionRange();
	int count = sel.bottom - sel.top + 1;
	int first = sel.top;

	// Se ci sono gia' dati nelle ultime "count" righe del foglio,
	// inserire le spingerebbe oltre kRowCount (il limite fisso del
	// motore): si rifiuta invece di perderle silenziosamente, come
	// faceva anche Sum-It storico (errCellsWouldFallOf).
	range overflow(1, kRowCount - count + 1, kColCount, kRowCount);
	CCellIterator overflowIter(fDoc, &overflow);
	cell dummy;
	if (overflowIter.NextExisting(dummy))
	{
		BAlert* alert = new BAlert("Inserisci riga",
			"Non si puo' inserire: alcune celle in fondo al foglio uscirebbero dal limite.",
			"OK");
		alert->Go();
		return;
	}

	range bounds;
	fDoc->GetBounds(bounds);
	if (bounds.right >= 1 && bounds.bottom >= 1)
		SaveUndoState(bounds);

	// Scandisce le righe dal basso verso l'alto: una formula sopra il
	// punto di inserimento puo' riferirsi a una cella sotto (che si
	// sposta), quindi va toccata anche se lei stessa non si sposta --
	// MoveCell con split diverso da noSplit aggiorna i riferimenti
	// della formula anche quando destLoc == srcLoc. Scendere dal basso
	// evita di sovrascrivere celle non ancora spostate (la
	// destinazione ha sempre v maggiore o uguale alla sorgente).
	for (int v = kRowCount - count; v >= 1; v--)
	{
		cell c(0, v);
		while (fDoc->GetNextCellInRow(c, true))
		{
			cell newLoc = c;
			if (newLoc.v >= first)
				newLoc.v += count;
			fDoc->MoveCell(fDoc, c, newLoc, vSplit, first, count);
		}
	}

	RecalculateOwningWorkbook();
	Invalidate();
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

void SheetView::InsertColumns()
{
	if (!fDoc)
		return;

	range sel = SelectionRange();
	int count = sel.right - sel.left + 1;
	int first = sel.left;

	range overflow(kColCount - count + 1, 1, kColCount, kRowCount);
	CCellIterator overflowIter(fDoc, &overflow);
	cell dummy;
	if (overflowIter.NextExisting(dummy))
	{
		BAlert* alert = new BAlert("Inserisci colonna",
			"Non si puo' inserire: alcune celle in fondo al foglio uscirebbero dal limite.",
			"OK");
		alert->Go();
		return;
	}

	range bounds;
	fDoc->GetBounds(bounds);
	if (bounds.right >= 1 && bounds.bottom >= 1)
		SaveUndoState(bounds);

	// Le righe sono indipendenti fra loro per uno spostamento di
	// colonne, quindi l'ordine fra righe non conta -- conta pero'
	// l'ordine ALL'INTERNO di ogni riga: da destra verso sinistra
	// (GetPreviousCellInRow, partendo oltre l'ultima colonna), stessa
	// ragione di InsertRows ma sull'asse orizzontale.
	for (int v = 1; v <= kRowCount; v++)
	{
		cell c(kColCount + 1, v);
		while (fDoc->GetPreviousCellInRow(c, true))
		{
			cell newLoc = c;
			if (newLoc.h >= first)
				newLoc.h += count;
			fDoc->MoveCell(fDoc, c, newLoc, hSplit, first, count);
		}
	}

	RecalculateOwningWorkbook();
	Invalidate();
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

void SheetView::DeleteRows()
{
	if (!fDoc)
		return;

	range sel = SelectionRange();
	int count = sel.bottom - sel.top + 1;
	int first = sel.top;
	int last = sel.bottom;

	range bounds;
	fDoc->GetBounds(bounds);
	if (bounds.right < 1 || bounds.bottom < 1)
		return; // foglio vuoto: niente da eliminare

	SaveUndoState(bounds);

	// Le celle dentro le righe eliminate spariscono (nessuna
	// destinazione valida per loro, a differenza di quelle sotto, che
	// si spostano in su).
	{
		range deletedZone(1, first, kColCount, last);
		CCellIterator deletedIter(fDoc, &deletedZone);
		cell toDelete;
		while (deletedIter.NextExisting(toDelete))
			fDoc->DisposeCell(toDelete);
	}

	// Scandisce dall'alto verso il basso stavolta: la destinazione ha
	// sempre v minore o uguale alla sorgente per una cancellazione,
	// quindi l'ordine ascendente evita di sovrascrivere celle non
	// ancora spostate (speculare a InsertRows).
	for (int v = 1; v <= kRowCount; v++)
	{
		cell c(0, v);
		while (fDoc->GetNextCellInRow(c, true))
		{
			cell newLoc = c;
			if (newLoc.v > last)
				newLoc.v -= count;
			fDoc->MoveCell(fDoc, c, newLoc, vSplit, first, -count);
		}
	}

	RecalculateOwningWorkbook();
	Invalidate();
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

void SheetView::DeleteColumns()
{
	if (!fDoc)
		return;

	range sel = SelectionRange();
	int count = sel.right - sel.left + 1;
	int first = sel.left;
	int last = sel.right;

	range bounds;
	fDoc->GetBounds(bounds);
	if (bounds.right < 1 || bounds.bottom < 1)
		return; // foglio vuoto: niente da eliminare

	SaveUndoState(bounds);

	{
		range deletedZone(first, 1, last, kRowCount);
		CCellIterator deletedIter(fDoc, &deletedZone);
		cell toDelete;
		while (deletedIter.NextExisting(toDelete))
			fDoc->DisposeCell(toDelete);
	}

	for (int v = 1; v <= kRowCount; v++)
	{
		cell c(0, v);
		while (fDoc->GetNextCellInRow(c, true))
		{
			cell newLoc = c;
			if (newLoc.h > last)
				newLoc.h -= count;
			fDoc->MoveCell(fDoc, c, newLoc, hSplit, first, -count);
		}
	}

	RecalculateOwningWorkbook();
	Invalidate();
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

SheetView::UndoSnapshot SheetView::CaptureSnapshot(range r) const
{
	UndoSnapshot snap;
	snap.r = r;

	CCellIterator iter(fDoc, &r);
	cell c;
	while (iter.NextExisting(c))
	{
		char text[4096];
		fDoc->GetCellFormula(c, text, sizeof(text), false);
		snap.cells.push_back(std::make_pair(c, std::string(text)));
	}

	return snap;
}

void SheetView::ApplySnapshot(const UndoSnapshot& snap)
{
	// Svuota prima tutto cio' che esiste ORA nell'intervallo (non solo
	// le celle catturate): una cella vuota al momento della cattura ma
	// scritta nel frattempo deve tornare vuota, non restare com'e'.
	{
		range r = snap.r;
		CCellIterator iter(fDoc, &r);
		cell c;
		while (iter.NextExisting(c))
			fDoc->DisposeCell(c);
	}

	for (size_t i = 0; i < snap.cells.size(); i++)
	{
		const cell& c = snap.cells[i].first;
		const std::string& text = snap.cells[i].second;
		if (!text.empty())
		{
			try
			{
				TryToParseString(text.c_str(), c, fDoc, true);
			}
			catch (...)
			{
			}
		}
	}

	RecalculateOwningWorkbook();
}

void SheetView::SaveUndoState(range affected)
{
	if (!fDoc)
		return;

	fUndoStack.push_back(CaptureSnapshot(affected));
	// Una nuova modifica rende irraggiungibile la storia dei "ripeti"
	// precedenti (come in Excel/LibreOffice Calc: annullare, poi
	// modificare qualcosa di diverso, invalida il "ripeti" rimasto).
	fRedoStack.clear();
}

void SheetView::SaveUndoState(cell affected)
{
	SaveUndoState(range(affected.h, affected.v, affected.h, affected.v));
}

void SheetView::Undo()
{
	if (fUndoStack.empty() || !fDoc)
		return;

	UndoSnapshot toRestore = fUndoStack.back();
	fUndoStack.pop_back();

	fRedoStack.push_back(CaptureSnapshot(toRestore.r));
	ApplySnapshot(toRestore);

	SetSelection(toRestore.r.TopLeft());
	ExtendSelection(toRestore.r.BotRight());
	Invalidate(CellRect(toRestore.r.TopLeft()) | CellRect(toRestore.r.BotRight()));
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

void SheetView::Redo()
{
	if (fRedoStack.empty() || !fDoc)
		return;

	UndoSnapshot toRestore = fRedoStack.back();
	fRedoStack.pop_back();

	fUndoStack.push_back(CaptureSnapshot(toRestore.r));
	ApplySnapshot(toRestore);

	SetSelection(toRestore.r.TopLeft());
	ExtendSelection(toRestore.r.BotRight());
	Invalidate(CellRect(toRestore.r.TopLeft()) | CellRect(toRestore.r.BotRight()));
	NotifySelectionChanged();
	NotifyDocumentChanged();
}

BRect SheetView::CellRect(cell c) const
{
	int col = c.h;
	int row = c.v;
	if (col < 1) col = 1;
	if (col > kColCount) col = kColCount;
	if (row < 1) row = 1;
	if (row > kRowCount) row = kRowCount;

	float x = kHeaderWidth + fColOffsets[col - 1];
	float y = kHeaderHeight + fRowOffsets[row - 1];
	return BRect(x, y, x + fColWidths[col - 1], y + fRowHeights[row - 1]);
}

cell SheetView::CellAt(BPoint where) const
{
	float x = where.x - kHeaderWidth;
	float y = where.y - kHeaderHeight;

	// Blocca riquadri: un punto che ricade nella banda congelata (ferma
	// sullo schermo, vedi Draw()/PinnedCellRect) va prima "riportato"
	// in spazio virtuale sottraendo l'offset di scroll corrente --
	// l'esatto inverso di come Draw() la disegna (xOrigin/yOrigin =
	// Bounds().left/.top). Senza questo, un clic sulla banda congelata
	// dopo aver scorso il foglio selezionerebbe la cella sbagliata
	// (quella che si troverebbe li' SENZA il blocco, non quella
	// davvero disegnata sotto il dito).
	if (fFrozenCols > 0)
	{
		float frozenW = fColOffsets[fFrozenCols];
		if (x >= Bounds().left && x < Bounds().left + frozenW)
			x -= Bounds().left;
	}
	if (fFrozenRows > 0)
	{
		float frozenH = fRowOffsets[fFrozenRows];
		if (y >= Bounds().top && y < Bounds().top + frozenH)
			y -= Bounds().top;
	}

	return cell(ColumnAtX(x), RowAtY(y));
}

BRect SheetView::ContentRect() const
{
	range bounds;
	if (fDoc)
		fDoc->GetBounds(bounds);

	// Foglio senza celle (o nessun documento): un solo rettangolo di
	// cella, non l'intero intervallo virtuale del motore.
	if (bounds.right < 1 || bounds.bottom < 1)
		return BRect(0, 0, kHeaderWidth + kColWidth, kHeaderHeight + kRowHeight);

	BRect botRight = CellRect(bounds.BotRight());
	return BRect(0, 0, botRight.right, botRight.bottom);
}

void SheetView::ScrollToShowSelection()
{
	BRect r = CellRect(fSelection);

	// Bounds() riflette sempre la dimensione piena del Frame() (vedi
	// FullCanvasFrame()), non la porzione effettivamente visibile:
	// origine dello scroll da Bounds() (l'unica parte che ScrollBy
	// aggiorna davvero), dimensioni dalla vera area visibile del
	// genitore (la BScrollView).
	BRect b = Bounds();
	BRect viewportSize = Parent() ? Parent()->Bounds() : b;
	BRect visible(b.left, b.top, b.left + viewportSize.Width(),
		b.top + viewportSize.Height());

	// Blocca riquadri: una cella congelata e' sempre visibile, ferma
	// sullo schermo (vedi Draw()) -- non serve mai scorrere per
	// "raggiungerla", altrimenti selezionarla forzerebbe lo scroll a
	// tornare sempre in cima/a sinistra, vanificando il blocco. Per
	// una cella NON congelata, la banda occupata dalle celle congelate
	// va invece considerata come le intestazioni (kHeaderWidth/
	// kHeaderHeight sopra): senza sommarla, una cella appena rivelata
	// dallo scroll potrebbe restare parzialmente coperta dalla banda
	// bloccata.
	bool colFrozen = fFrozenCols > 0 && fSelection.h <= fFrozenCols;
	bool rowFrozen = fFrozenRows > 0 && fSelection.v <= fFrozenRows;
	float frozenW = fFrozenCols > 0 ? fColOffsets[fFrozenCols] : 0;
	float frozenH = fFrozenRows > 0 ? fRowOffsets[fFrozenRows] : 0;

	float dx = 0, dy = 0;
	if (!colFrozen)
	{
		if (r.left < visible.left + kHeaderWidth + frozenW)
			dx = r.left - kHeaderWidth - frozenW - visible.left;
		else if (r.right > visible.right)
			dx = r.right - visible.right;
	}

	if (!rowFrozen)
	{
		if (r.top < visible.top + kHeaderHeight + frozenH)
			dy = r.top - kHeaderHeight - frozenH - visible.top;
		else if (r.bottom > visible.bottom)
			dy = r.bottom - visible.bottom;
	}

	if (dx != 0 || dy != 0)
		ScrollBy(dx, dy);
}

// Converte un numero di colonna 1-based in nome stile foglio di
// calcolo (1->A, 26->Z, 27->AA, ...).
static void ColumnName(int col, char* out)
{
	char buf[8];
	int n = 0;
	while (col > 0)
	{
		int rem = (col - 1) % 26;
		buf[n++] = 'A' + rem;
		col = (col - 1) / 26;
	}
	for (int i = 0; i < n; i++)
		out[i] = buf[n - 1 - i];
	out[n] = 0;
}

// Bianco/nero, gli stessi valori del costruttore predefinito di
// CellStyle (CellStyle.cpp): confrontarli qui invece di costruire un
// CellStyle solo per questo evita l'overhead per ogni cella visibile,
// il caso comune (la stragrande maggioranza delle celle non ha un
// colore personalizzato).
static bool IsDefaultBg(rgb_color c)
{
	return c.red == 255 && c.green == 255 && c.blue == 255;
}

// Testo a capo (Fase 12): a-capo semplice per parola (spazio come
// unico punto di rottura, nessuna sillabazione) -- il font attivo
// sulla view DEVE gia' essere quello giusto per la cella (chi chiama
// applica gFontSizeTable.SetFontID prima), StringWidth qui sotto
// dipende da quello. Una singola parola piu' larga di maxWidth resta
// comunque sulla propria riga (nessun taglio a meta' parola), per non
// rischiare un ciclo infinito o testo illeggibile.
static std::vector<BString> WrapTextLines(BView* view, const char* text, float maxWidth)
{
	std::vector<BString> lines;
	BString current;
	BString word;

	for (const char* p = text; ; p++)
	{
		if (*p == ' ' || *p == 0)
		{
			if (word.Length() > 0)
			{
				BString candidate = current;
				if (candidate.Length() > 0)
					candidate << " ";
				candidate << word;

				if (current.Length() > 0 && view->StringWidth(candidate.String()) > maxWidth)
				{
					lines.push_back(current);
					current = word;
				}
				else
					current = candidate;

				word = "";
			}
			if (*p == 0)
				break;
		}
		else
			word << *p;
	}

	if (current.Length() > 0)
		lines.push_back(current);
	if (lines.empty())
		lines.push_back(BString(text));

	return lines;
}

// Testo che trabocca (Fase 5, confronto diretto con Excel vero su una
// fattura reale: l'intestazione dello studio, piu' larga della
// propria colonna, veniva troncata al bordo di cella
// invece di espandersi visivamente sulle colonne vuote a destra, come
// mostra invece Excel). Cammina cella per cella nella riga di "c",
// nella direzione dettata dall'allineamento, finche' il testo ci
// entra o si incontra una cella non vuota o gia' unita (che blocca
// l'espansione, esattamente come in Excel) o il bordo del foglio
// (kColCount, vedi Constants.h). "r" arriva gia' nelle coordinate
// NON scontate di CellRect (l'offset di Blocca riquadri va applicato
// dopo da chi chiama, come per il rettangolo delle celle unite poco
// sopra). Non chiamata per celle unite (hanno gia' il proprio
// rettangolo) ne' per il testo a capo (va gia' a capo dentro la
// colonna, non deve traboccare).
BRect SheetView::ExpandOverflowRect(BRect r, cell c, char alignment, float textWidth) const
{
	if (!fDoc || textWidth <= r.Width() - 6)
		return r;

	if (alignment == eAlignRight)
	{
		int col = c.h;
		while (r.Width() < textWidth + 6 && col > 1)
		{
			cell neighbor(col - 1, c.v);
			range dummy;
			char nbText[4096];
			fDoc->GetCellResult(neighbor, nbText, sizeof(nbText), true);
			if (nbText[0] != 0 || fDoc->GetMergedRange(neighbor, &dummy))
				break;
			col--;
			r.left = CellRect(neighbor).left;
		}
	}
	else if (alignment == eAlignCenter)
	{
		int leftCol = c.h, rightCol = c.h;
		bool leftBlocked = false, rightBlocked = false;
		while (r.Width() < textWidth + 6 && (!leftBlocked || !rightBlocked))
		{
			if (!rightBlocked)
			{
				if (rightCol >= kColCount)
					rightBlocked = true;
				else
				{
					cell neighbor(rightCol + 1, c.v);
					range dummy;
					char nbText[4096];
					fDoc->GetCellResult(neighbor, nbText, sizeof(nbText), true);
					if (nbText[0] != 0 || fDoc->GetMergedRange(neighbor, &dummy))
						rightBlocked = true;
					else
					{
						rightCol++;
						r.right = CellRect(neighbor).right;
					}
				}
			}
			if (r.Width() >= textWidth + 6)
				break;
			if (!leftBlocked)
			{
				if (leftCol <= 1)
					leftBlocked = true;
				else
				{
					cell neighbor(leftCol - 1, c.v);
					range dummy;
					char nbText[4096];
					fDoc->GetCellResult(neighbor, nbText, sizeof(nbText), true);
					if (nbText[0] != 0 || fDoc->GetMergedRange(neighbor, &dummy))
						leftBlocked = true;
					else
					{
						leftCol--;
						r.left = CellRect(neighbor).left;
					}
				}
			}
		}
	}
	else // eAlignGeneral/eAlignLeft: il testo si allinea a sinistra e
	     // trabocca verso destra, lo stesso comportamento di Excel per
	     // il testo (a differenza dei numeri, sempre allineati a
	     // destra e mai traboccanti in Excel -- qui non e' un problema
	     // pratico dato che i numeri di solito entrano nella colonna).
	{
		int col = c.h;
		while (r.Width() < textWidth + 6 && col < kColCount)
		{
			cell neighbor(col + 1, c.v);
			range dummy;
			char nbText[4096];
			fDoc->GetCellResult(neighbor, nbText, sizeof(nbText), true);
			if (nbText[0] != 0 || fDoc->GetMergedRange(neighbor, &dummy))
				break;
			col++;
			r.right = CellRect(neighbor).right;
		}
	}

	return r;
}

// Testo effettivo che verra' disegnato per una cella (stringa vuota se
// la cella non ha contenuto): il testo grezzo del motore
// (CFormatter/eGeneral) con sopra una riformattazione locale-aware
// (separatore delle migliaia, punto o virgola decimale secondo le
// preferenze di sistema) tramite il Locale Kit, per un valore
// puramente numerico. Rispetta il formato scelto per la cella (menu
// Formato o import): valuta e percentuale usano le formattazioni
// dedicate del Locale Kit, gli altri (incluso il generico) usano il
// raggruppamento numerico semplice. La riformattazione sotto sostituisce
// SEMPRE il testo gia' calcolato dal motore per un valore numerico
// puro, tranne per un formato come "00000" (riempimento di zeri a
// sinistra, vedi CFormatter::eZeroPad in Formatter.h) che ha gia' un
// testo ESATTO da CFormatter -- sostituirlo scarterebbe gli zeri
// iniziali, bug reale scoperto confrontando visivamente con Excel vero
// l'importazione di una fattura reale ("Preavviso N. 00073" mostrato
// come "73"). "& 0x000F" isola il tipo di formato dai bit di
// cifre/virgole impacchettati insieme (vedi CFormatter::FormatID): un
// confronto diretto con l'enum funziona solo per un formato senza
// cifre decimali ne' separatore delle migliaia (rarissimo in pratica)
// -- un formato valuta/percentuale REALE (es. "€ ###,###.00", 2 cifre
// e virgola) impacchetta quei bit sopra l'enum e non risulta mai
// uguale a eCurrency/ePercent cosi' com'e', altro bug reale scoperto
// allo stesso modo: ogni importo in valuta ("€ 450,00") si rendeva
// come un numero puro senza simbolo ne' decimali ("450"). Pubblico
// apposta per essere testabile direttamente, stesso principio di
// CellRect/ExpandOverflowRect sopra.
BString SheetView::FormattedCellText(cell c)
{
	if (!fDoc)
		return BString();

	char text[4096];
	fDoc->GetCellResult(c, text, sizeof(text), true);
	if (text[0] == 0)
		return BString();

	CellStyle cs;
	fDoc->GetCellStyle(c, cs);

	Value val;
	if (fDoc->GetValue(c, val) && val.fType == eNumData && !val.IsNan()
		&& (cs.fFormat & 0x000F) != eZeroPad)
	{
		int formatKind = cs.fFormat & 0x000F;
		BString formatted;
		status_t fmtErr;
		if (formatKind == eCurrency)
			fmtErr = fNumberFormat.FormatMonetary(formatted, (double)val);
		else if (formatKind == ePercent)
			fmtErr = fNumberFormat.FormatPercent(formatted, (double)val);
		else
			fmtErr = fNumberFormat.Format(formatted, (double)val);

		if (fmtErr == B_OK && formatted.Length() > 0)
			return formatted;
	}

	return BString(text);
}

// Vedi il commento su fFrozenRows/fFrozenCols in SheetView.h: disegna
// sfondo, griglia e testo per un blocco di celle [firstCol,lastCol] x
// [firstRow,lastRow], spostato di (xOrigin, yOrigin) -- (0,0) per il
// riquadro scorrevole normale (comportamento identico a prima di
// Blocca riquadri), (Bounds().left, 0)/(0, Bounds().top) per una
// banda congelata. clipRect e' gia' in coordinate sullo schermo
// (comprensive dell'origine): limita sia il disegno sia cosa
// StrokeLine/DrawString toccano fuori dal blocco.
void SheetView::DrawCellBand(BRect clipRect, int firstCol, int lastCol,
	int firstRow, int lastRow, float xOrigin, float yOrigin)
{
	if (firstCol > lastCol || firstRow > lastRow)
		return;

	BRegion clip(clipRect);
	ConstrainClippingRegion(&clip);

	// Sfondo colorato per cella (letto dal file importato o impostato
	// a mano -- vedi CellStyle::fLowColor), disegnato PRIMA delle righe
	// della griglia cosi' restano visibili sopra, come in Excel/
	// LibreOffice Calc invece di sparire sotto un blocco di colore.
	if (fDoc)
	{
		for (int row = firstRow; row <= lastRow; row++)
		{
			// Riga nascosta (AutoFilter o "hidden" da XLSX): non
			// disegna NULLA, esattamente come Excel -- vedi il
			// commento sul ciclo del testo piu' sotto per il motivo
			// (bug reale: senza questo salto il testo di una riga
			// nascosta restava comunque disegnato, sovrapposto alla
			// riga visibile successiva che ne aveva preso il posto).
			if (fRowHidden[row - 1])
				continue;
			for (int col = firstCol; col <= lastCol; col++)
			{
				cell c(col, row);
				CellStyle cs;
				fDoc->GetCellStyle(c, cs);
				if (!IsDefaultBg(cs.fLowColor))
				{
					SetHighColor(cs.fLowColor);
					FillRect(CellRect(c).OffsetByCopy(xOrigin, yOrigin));
				}
			}
		}
	}

	if (fShowGrid)
	{
		SetHighColor(220, 220, 220);
		for (int col = firstCol; col <= lastCol; col++)
			StrokeLine(BPoint(xOrigin + kHeaderWidth + fColOffsets[col - 1], clipRect.top),
				BPoint(xOrigin + kHeaderWidth + fColOffsets[col - 1], clipRect.bottom));
		for (int row = firstRow; row <= lastRow; row++)
		{
			if (fRowHidden[row - 1])
				continue;
			StrokeLine(BPoint(clipRect.left, yOrigin + kHeaderHeight + fRowOffsets[row - 1]),
				BPoint(clipRect.right, yOrigin + kHeaderHeight + fRowOffsets[row - 1]));
		}
	}

	// Bordi di cella (Fase 11): un lato alla volta, un byte booleano
	// ciascuno (CellStyle::fTBorderColor/fLBorderColor/fBBorderColor/
	// fRBorderColor -- il nome "colore" e' ereditato dal codice storico
	// di Sum-It, mai davvero implementato ne' li' ne' qui prima d'ora,
	// vedi ROADMAP.md Fase 11: 0 = nessun bordo, diverso da 0 = un
	// bordo nero pieno su quel lato). Ciclo a parte, non dentro quello
	// del testo sotto: una cella vuota puo' comunque avere un bordo
	// (lo stesso motivo per cui lo sfondo colorato, sopra, non salta le
	// celle senza contenuto). Disegnato DOPO la griglia sottile (sopra)
	// cosi' un bordo nero pieno si distingue sempre da un confine di
	// griglia qualunque, PRIMA del testo (sotto) cosi' un bordo non
	// copre mai un carattere che vi si sovrapponesse per un pixel.
	if (fDoc)
	{
		SetHighColor(0, 0, 0);
		for (int row = firstRow; row <= lastRow; row++)
		{
			if (fRowHidden[row - 1])
				continue;
			for (int col = firstCol; col <= lastCol; col++)
			{
				cell c(col, row);
				CellStyle cs;
				fDoc->GetCellStyle(c, cs);
				if (!cs.fTBorderColor && !cs.fLBorderColor
					&& !cs.fBBorderColor && !cs.fRBorderColor)
					continue;

				BRect r = CellRect(c).OffsetByCopy(xOrigin, yOrigin);
				if (cs.fTBorderColor)
					StrokeLine(r.LeftTop(), r.RightTop());
				if (cs.fBBorderColor)
					StrokeLine(r.LeftBottom(), r.RightBottom());
				if (cs.fLBorderColor)
					StrokeLine(r.LeftTop(), r.LeftBottom());
				if (cs.fRBorderColor)
					StrokeLine(r.RightTop(), r.RightBottom());
			}
		}
	}

	// Celle unite (Fase 12): riempie l'intero rettangolo unito con lo
	// sfondo della cella in alto a sinistra (che "possiede" il
	// contenuto e lo stile, vedi CContainer::GetMergedRange) --
	// cancella cosi' sia le linee di griglia sia gli eventuali bordi
	// di cella (Fase 11) gia' tracciati sopra all'INTERNO
	// dell'intervallo, poi ridisegna solo il contorno ESTERNO: griglia
	// sottile se fShowGrid, o il bordo nero pieno se la cella in alto
	// a sinistra ne ha uno esplicito su quel lato (stesso significato
	// booleano per lato di Fase 11). Piu' semplice che modificare i
	// cicli sopra per saltare selettivamente le linee interne di un
	// rettangolo di forma qualunque.
	if (fDoc)
	{
		const std::vector<range>& merged = fDoc->GetMergedRanges();
		for (size_t i = 0; i < merged.size(); i++)
		{
			const range& m = merged[i];
			if (m.right < firstCol || m.left > lastCol
				|| m.bottom < firstRow || m.top > lastRow)
				continue;

			cell topLeft(m.left, m.top);
			CellStyle cs;
			fDoc->GetCellStyle(topLeft, cs);

			BRect full = CellRect(topLeft);
			full = full | CellRect(cell(m.right, m.bottom));
			full.OffsetBy(xOrigin, yOrigin);

			SetHighColor(cs.fLowColor);
			FillRect(full);

			if (fShowGrid)
			{
				SetHighColor(220, 220, 220);
				StrokeRect(full);
			}

			SetHighColor(0, 0, 0);
			if (cs.fTBorderColor)
				StrokeLine(full.LeftTop(), full.RightTop());
			if (cs.fBBorderColor)
				StrokeLine(full.LeftBottom(), full.RightBottom());
			if (cs.fLBorderColor)
				StrokeLine(full.LeftTop(), full.LeftBottom());
			if (cs.fRBorderColor)
				StrokeLine(full.RightTop(), full.RightBottom());
		}
	}

	if (fDoc)
	{
		for (int row = firstRow; row <= lastRow; row++)
		{
			// Riga nascosta: DrawString non e' vincolato all'altezza
			// del rettangolo della cella come FillRect/StrokeLine sopra
			// -- disegna comunque al punto richiesto anche se quel
			// rettangolo e' ormai alto zero pixel (collassato da
			// RebuildRowOffsets), finendo sovrapposto al testo della
			// riga visibile successiva, che ne ha preso il posto. Bug
			// reale, non solo teorico: scoperto su un file reale con
			// righe nascoste, dove il testo di molte righe consecutive
			// appariva illeggibile, ammassato su poche righe visibili.
			if (fRowHidden[row - 1])
				continue;
			for (int col = firstCol; col <= lastCol; col++)
			{
				cell c(col, row);

				// Celle unite: solo la cella in alto a sinistra
				// disegna il proprio contenuto (le altre, anche se
				// non vuote, restano nascoste sotto -- stesso
				// comportamento di Excel), ma con un rettangolo esteso
				// a tutto l'intervallo unito per allineamento/a capo.
				range mergedRange;
				bool isMerged = fDoc->GetMergedRange(c, &mergedRange);
				if (isMerged && (mergedRange.top != row || mergedRange.left != col))
					continue;

				// Indicatore di commento (Fase 13): piccolo triangolo
				// rosso nell'angolo in alto a destra, come
				// Excel/LibreOffice Calc -- disegnato PRIMA di
				// "continue" per una cella senza testo, perche' una
				// cella puo' avere un commento senza avere nessun
				// altro contenuto (il "continue" sotto e' solo per il
				// resto del disegno, testo compreso).
				if (fDoc->HasComment(c))
				{
					BRect cr = CellRect(c);
					cr.OffsetBy(xOrigin, yOrigin);
					SetHighColor(200, 40, 40);
					BPoint p1(cr.right - 6, cr.top + 1);
					BPoint p2(cr.right, cr.top + 1);
					BPoint p3(cr.right, cr.top + 7);
					FillTriangle(p1, p2, p3);
				}

				BString cellTextStr = FormattedCellText(c);
				if (cellTextStr.Length() == 0)
					continue;
				char text[4096];
				strlcpy(text, cellTextStr.String(), sizeof(text));

				CellStyle cs;
				fDoc->GetCellStyle(c, cs);

				// Il colore passato a SetLowColor deve combaciare con
				// quello davvero disegnato sotto (sopra): DrawString lo
				// usa per l'antialiasing dei glifi, un valore sbagliato
				// lascerebbe un alone bianco intorno al testo su una
				// cella colorata. cs.fLowColor e' gia' bianco puro
				// (255,255,255) per una cella senza colore personalizzato
				// (il valore predefinito di CellStyle), quindi si puo'
				// usare direttamente senza distinguere i due casi.
				SetHighColor(cs.fHighColor);
				SetLowColor(cs.fLowColor);

				// Collegamenti ipertestuali (Fase 13): blu e
				// sottolineato, come Excel/LibreOffice Calc -- solo per
				// il disegno di questa cella, non tocca cs.fHighColor/
				// cs.fUnderline realmente memorizzati (un collegamento
				// rimosso in seguito deve poter tornare all'aspetto
				// originale della cella).
				bool isHyperlink = fDoc->HasHyperlink(c);
				if (isHyperlink)
					SetHighColor(20, 80, 200);

				// Grassetto/Corsivo (Fase 7): CellStyle::fFont e' un
				// indice in gFontSizeTable (mai un flag diretto), quindi
				// va applicato alla vista prima di disegnare -- e prima
				// di misurare la stringa qui sotto, dato che Allinea al
				// centro/a destra ne ha bisogno per calcolare la
				// posizione e un font diverso cambia la larghezza del
				// testo.
				gFontSizeTable.SetFontID(this, cs.fFont);

				// Celle unite: il rettangolo del testo copre l'intero
				// intervallo (non solo la cella in alto a sinistra),
				// cosi' l'allineamento/a capo/clip usano la larghezza
				// vera visibile sullo schermo. Altrimenti (cella
				// singola, non a capo automatico) il rettangolo puo'
				// espandersi sulle colonne vuote vicine se il testo
				// trabocca (vedi ExpandOverflowRect sopra).
				BRect r;
				if (isMerged)
				{
					r = CellRect(cell(mergedRange.left, mergedRange.top))
						| CellRect(cell(mergedRange.right, mergedRange.bottom));
				}
				else
				{
					r = CellRect(c);
					if (!cs.fWrapText)
						r = ExpandOverflowRect(r, c, cs.fAlignment, StringWidth(text));
				}
				r.OffsetBy(xOrigin, yOrigin);

				// Allineamento (Fase 7): il generico (eAlignGeneral, il
				// valore predefinito di CellStyle su ogni cella mai
				// toccata dal menu Formato) resta sempre a sinistra,
				// comportamento invariato -- solo le celle che l'utente
				// ha esplicitamente allineato cambiano posizione.
				BRegion textClip(r);
				ConstrainClippingRegion(&textClip);

				// Testo a capo (Fase 12): una riga sola (comportamento
				// invariato) o piu' righe impilate dall'alto della
				// cella, ciascuna alta kRowHeight -- stessa unita' usata
				// da RecalculateWrappedRowHeights per far crescere la
				// riga, altrimenti disegno e layout andrebbero fuori
				// sincrono (righe tagliate o spazio vuoto in eccesso).
				std::vector<BString> lines;
				if (cs.fWrapText)
					lines = WrapTextLines(this, text, r.Width() - 6);
				else
					lines.push_back(BString(text));

				for (size_t li = 0; li < lines.size(); li++)
				{
					const char* lineText = lines[li].String();
					float textX = r.left + 3;
					if (cs.fAlignment == eAlignCenter || cs.fAlignment == eAlignRight)
					{
						float textWidth = StringWidth(lineText);
						if (cs.fAlignment == eAlignCenter)
							textX = r.left + (r.Width() - textWidth) / 2.0f;
						else
							textX = r.right - textWidth - 3;
					}

					BPoint pos(textX, r.top + kRowHeight * (li + 1) - 5);
					DrawString(lineText, pos);

					// Sottolineato (Fase 12): BFont non ha un
					// attributo sottolineato nativo (vedi il
					// commento su CellStyle::fUnderline in
					// CellStyle.h), disegnata qui a mano come una
					// linea sotto ogni riga -- stesso colore del
					// testo (fHighColor, gia' impostato sopra con
					// SetHighColor), non l'intera larghezza della
					// cella come i bordi in DrawCellBand, solo la
					// larghezza del testo davvero disegnato su
					// quella riga.
					if ((cs.fUnderline || isHyperlink) && lineText[0] != 0)
					{
						float textWidth = StringWidth(lineText);
						float y = pos.y + 2;
						StrokeLine(BPoint(pos.x, y), BPoint(pos.x + textWidth, y));
					}
				}
			}
		}

		// Ripristina il font predefinito: senza, l'ULTIMO font applicato
		// sopra (magari in grassetto/corsivo per l'ultima cella disegnata)
		// resterebbe attivo per qualunque altra cosa disegnata dopo in
		// Draw() -- le intestazioni di riga/colonna, che usano DrawString
		// a loro volta ma non devono mai ereditare il font di una cella.
		SetFont(be_plain_font);

		// Bug reale scoperto scrivendo l'AutoFilter sotto (la freccia a
		// discesa non compariva MAI, ne' su una vera finestra ne' su una
		// bitmap offscreen, pur essendo il codice che la disegna
		// geometricamente corretto e sicuramente raggiunto -- verificato
		// con FillRect() al posto di FillTriangle() e persino con un
		// rettangolo enorme a copertura di quasi tutta la vista, sempre
		// senza alcun effetto visibile). Causa: il ciclo del testo sopra
		// restringe il ritaglio a "textClip" (il solo rettangolo della
		// cella corrente, riga per riga) a ogni cella non vuota, ma non
		// lo ripristinava mai fra un'iterazione e l'altra ne' all'uscita
		// dal ciclo -- restava quindi attivo il ritaglio dell'ULTIMA
		// cella non vuota disegnata, invisibile finche' nessun codice
		// disegnava piu' nulla dopo quel ciclo in questa funzione (mai
		// successo prima d'ora: la freccia sotto e' il primo caso).
		// Senza questo ripristino esplicito, qualunque cosa disegnata più
		// sotto (non solo la freccia) sarebbe rimasta invisibile fuori da
		// quel piccolo rettangolo residuo.
		ConstrainClippingRegion(NULL);
	}

	// AutoFilter (import XLSX <autoFilter ref="...">): una piccola
	// freccia a discesa nell'angolo in basso a destra di ogni cella di
	// intestazione nell'intervallo di colonne del filtro -- disegnata
	// SOLO quando la riga di intestazione (fAutoFilterRange.top) cade
	// dentro la banda corrente, esattamente come Excel la mostra solo
	// li', non su ogni riga sottostante. MouseDown sotto riconosce un
	// clic su quest'area con lo stesso rettangolo.
	if (fHasAutoFilter && fDoc
		&& fAutoFilterRange.top >= firstRow && fAutoFilterRange.top <= lastRow)
	{
		int col1 = std::max(firstCol, (int)fAutoFilterRange.left);
		int col2 = std::min(lastCol, (int)fAutoFilterRange.right);
		SetHighColor(90, 90, 90);
		for (int col = col1; col <= col2; col++)
		{
			BRect r = AutoFilterArrowRect(col).OffsetByCopy(xOrigin, yOrigin);
			FillTriangle(BPoint(r.left, r.top), BPoint(r.right, r.top),
				BPoint((r.left + r.right) / 2, r.bottom));
		}
	}

	ConstrainClippingRegion(NULL);
}

BRect SheetView::PinnedCellRect(cell c) const
{
	BRect r = CellRect(c);
	if (fFrozenCols > 0 && c.h <= fFrozenCols)
		r.OffsetBy(Bounds().left, 0);
	if (fFrozenRows > 0 && c.v <= fFrozenRows)
		r.OffsetBy(0, Bounds().top);
	return r;
}

// Rettangolo della cella attiva, esteso a tutto l'intervallo se "c" e'
// l'angolo di una cella unita -- un solo posto per la formula, usato
// sia da Draw() per disegnare il riquadro di selezione sia da
// SetSelection/ExtendSelection per invalidare la zona giusta quando la
// selezione si sposta. Prima di questo metodo condiviso, l'invalidazione
// usava sempre PinnedCellRect grezzo (una sola cella) anche quando
// Draw() disegnava un riquadro esteso all'intervallo unito: spostando
// la selezione via da una cella unita, la parte del vecchio riquadro
// fuori dalla zona invalidata (larga una sola cella) non veniva mai
// ridisegnata, lasciando un "fantasma" del riquadro blu sullo schermo
// -- bug reale segnalato dall'utente subito dopo il fix del clic sulle
// celle unite.
BRect SheetView::ActiveCellRect(cell c) const
{
	BRect r = PinnedCellRect(c);
	if (fDoc)
	{
		range mergedRange;
		if (fDoc->GetMergedRange(c, &mergedRange))
		{
			r = PinnedCellRect(cell(mergedRange.left, mergedRange.top))
				| PinnedCellRect(cell(mergedRange.right, mergedRange.bottom));
		}
	}
	return r;
}

BRect SheetView::ImageFrame(const EmbeddedImage& img) const
{
	BRect anchorRect = CellRect(img.anchor);
	return BRect(anchorRect.left + img.offsetX, anchorRect.top + img.offsetY,
		anchorRect.left + img.offsetX + img.width - 1,
		anchorRect.top + img.offsetY + img.height - 1);
}

// Piccolo quadrato nell'angolo in basso a destra di ImageFrame, stesso
// principio di AutoFilterArrowRect: un solo posto per la formula usata
// sia da Draw() per disegnarla sia da MouseDown per riconoscere il clic.
BRect SheetView::ImageResizeHandle(const EmbeddedImage& img) const
{
	BRect frame = ImageFrame(img);
	const float kHandleSize = 8;
	return BRect(frame.right - kHandleSize, frame.bottom - kHandleSize,
		frame.right, frame.bottom);
}

// Decodifica un blob PNG (o qualunque altro formato per cui esista un
// translator installato) in una BBitmap pronta per DrawBitmap -- di
// proprieta' del chiamante, che deve fare "delete". Nessuna cache:
// il file di gara reale che ha motivato questa fase ne ha una sola,
// piccola (vedi ROADMAP.md Fase 12), decodificarla a ogni ridisegno
// resta trascurabile.
static BBitmap* DecodeImageBytes(const std::vector<uint8>& data)
{
	if (data.empty())
		return NULL;

	BMemoryIO memIO(&data[0], data.size());
	BBitmapStream stream;
	status_t err = BTranslatorRoster::Default()->Translate(&memIO, NULL, NULL,
		&stream, B_TRANSLATOR_BITMAP);
	if (err != B_OK)
		return NULL;

	BBitmap* bitmap = NULL;
	stream.DetachBitmap(&bitmap);
	return bitmap;
}

void SheetView::Draw(BRect updateRect)
{
	SetHighColor(255, 255, 255);
	FillRect(updateRect);

	// Riquadro scorrevole: sempre le colonne/righe DOPO quelle
	// congelate (fFrozenCols/fFrozenRows sono 0 quando Blocca riquadri
	// non e' attivo, quindi qui non cambia nulla rispetto a prima).
	int firstCol = std::max(fFrozenCols + 1, ColumnAtX(updateRect.left - kHeaderWidth));
	int lastCol = std::min((int)kColCount, ColumnAtX(updateRect.right - kHeaderWidth) + 1);
	int firstRow = std::max(fFrozenRows + 1, RowAtY(updateRect.top - kHeaderHeight));
	int lastRow = std::min((int)kRowCount, RowAtY(updateRect.bottom - kHeaderHeight) + 1);
	DrawCellBand(updateRect, firstCol, lastCol, firstRow, lastRow, 0, 0);

	// Blocca riquadri: le bande congelate si disegnano SEMPRE per
	// intero (non ritagliate su updateRect come il riquadro scorrevole
	// sopra) -- sono in genere poche righe/colonne, il costo e'
	// trascurabile, ed evita di dover calcolare quale porzione di una
	// banda "incollata allo schermo" (vedi ScrollTo()) ricade
	// davvero nell'updateRect corrente, che userebbe uno spazio di
	// coordinate diverso da quello (virtuale) di ColumnAtX/RowAtY.
	BRect bounds = Bounds();
	float frozenW = fFrozenCols > 0 ? fColOffsets[fFrozenCols] : 0;
	float frozenH = fFrozenRows > 0 ? fRowOffsets[fFrozenRows] : 0;

	if (fFrozenRows > 0)
	{
		// Righe congelate, colonne scorrevoli (banda in alto a destra):
		// ferma sull'asse verticale (yOrigin = Bounds().top), segue
		// comunque lo scroll orizzontale normalmente.
		BRect band(bounds.left, bounds.top + kHeaderHeight,
			bounds.right, bounds.top + kHeaderHeight + frozenH - 1);
		DrawCellBand(band, firstCol, lastCol, 1, fFrozenRows, 0, bounds.top);
	}
	if (fFrozenCols > 0)
	{
		// Colonne congelate, righe scorrevoli (banda in basso a
		// sinistra): speculare alla precedente.
		BRect band(bounds.left + kHeaderWidth, bounds.top,
			bounds.left + kHeaderWidth + frozenW - 1, bounds.bottom);
		DrawCellBand(band, 1, fFrozenCols, firstRow, lastRow, bounds.left, 0);
	}
	if (fFrozenRows > 0 && fFrozenCols > 0)
	{
		// Angolo (righe E colonne congelate): ferma su entrambi gli
		// assi, non scorre mai.
		BRect band(bounds.left + kHeaderWidth, bounds.top + kHeaderHeight,
			bounds.left + kHeaderWidth + frozenW - 1, bounds.top + kHeaderHeight + frozenH - 1);
		DrawCellBand(band, 1, fFrozenCols, 1, fFrozenRows, bounds.left, bounds.top);
	}

	// Selezione corrente: un rettangolo di piu' celle (trascinamento
	// del mouse, Maiusc+frecce, Ctrl+A) mostra una tinta leggera su
	// tutto l'intervallo (esclusa la cella attiva, lasciata bianca
	// cosi' resta leggibile) piu' un bordo attorno al rettangolo
	// intero; una selezione di una sola cella (il caso comune) resta
	// visivamente identica a prima -- il rettangolo dell'intervallo
	// coincide con quello della cella attiva, quindi il riempimento
	// "esclude se stesso" e non lascia alcuna tinta visibile.
	range selRange = SelectionRange();
	BRect selOuter = PinnedCellRect(selRange.TopLeft()) | PinnedCellRect(selRange.BotRight());
	// ActiveCellRect (non PinnedCellRect grezzo): se fSelection e'
	// l'angolo di una cella unita, il riquadro si estende a tutto
	// l'intervallo (vedi il commento sul metodo) -- altrimenti resta
	// largo una sola colonna/riga anche se la cella e' visivamente
	// unita con altre, facendola sembrare mai unita. Per il caso comune
	// di selezione a singola cella, selOuter deve combaciare con lo
	// stesso rettangolo esteso (altrimenti i due StrokeRect sotto
	// disegnerebbero due riquadri annidati invece di uno solo).
	BRect activeRect = ActiveCellRect(fSelection);
	if (selRange.left == selRange.right && selRange.top == selRange.bottom)
		selOuter = activeRect;

	if (selRange.left != selRange.right || selRange.top != selRange.bottom)
	{
		// B_OP_ALPHA/B_PIXEL_ALPHA con un colore ad alfa parziale
		// mescola la tinta con cio' che DrawCellBand ha gia' disegnato
		// sotto (sfondo e testo), invece di sostituirlo -- con la
		// modalita' di disegno predefinita B_OP_COPY (colore opaco) la
		// tinta copriva per intero il contenuto delle celle selezionate,
		// nascondendolo: bug reale segnalato dall'utente ("l'area
		// selezionata non e' trasparente e non mostra il contenuto delle
		// celle"). Ripristinata subito dopo: StrokeRect() sotto e il
		// resto di Draw() si aspettano B_OP_COPY.
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		SetHighColor(80, 130, 220, 60);
		BRegion tint(selOuter);
		tint.Exclude(activeRect);
		FillRegion(&tint);
		SetDrawingMode(B_OP_COPY);
	}

	SetHighColor(30, 100, 200);
	StrokeRect(selOuter);
	StrokeRect(activeRect);
	StrokeRect(activeRect.InsetByCopy(1, 1));

	// Intestazione di riga (numeri): "congelata" durante lo scroll
	// orizzontale -- stessa tecnica e stessa richiesta dell'utente
	// dell'intestazione di colonna sotto, ma sull'altro asse
	// (Bounds().left invece di Bounds().top). I numeri restano
	// comunque quelli delle righe davvero visibili, che seguono lo
	// scroll verticale normalmente (solo la coordinata orizzontale e'
	// "agganciata" al bordo sinistro della viewport).
	float rowHeaderLeft = Bounds().left;
	SetHighColor(230, 230, 230);
	FillRect(BRect(rowHeaderLeft, updateRect.top,
		rowHeaderLeft + kHeaderWidth - 1, updateRect.bottom));

	// Blocca riquadri: le etichette delle righe congelate (1..
	// fFrozenRows) restano "incollate" alla cima della viewport
	// (yOrigin = Bounds().top) insieme alla banda di celle che
	// etichettano, invece di scorrere via -- altrimenti la banda
	// congelata perderebbe la sua intestazione durante lo scroll
	// verticale. row/lastRow sopra partono gia' da fFrozenRows+1, quindi
	// questo ciclo copre l'intervallo complementare, mai sovrapposto.
	SetHighColor(0, 0, 0);
	for (int row = 1; row <= fFrozenRows; row++)
	{
		// Riga nascosta: nessuna etichetta (Excel non ne mostra una per
		// una riga che non occupa spazio a schermo). Bug reale: questa
		// formula usava fRowHeights[row-1] (l'altezza VERA/di
		// ripristino della riga, non il suo contributo effettivo alla
		// somma cumulativa quando nascosta, sempre zero) per posizionare
		// l'etichetta -- per una riga nascosta che era stata alta
		// (es. testo a capo su piu' righe) l'etichetta successiva
		// finiva spinta molto piu' in basso di dove sarebbe dovuta
		// essere, sovrapposta ad altre etichette gia' disegnate.
		if (fRowHidden[row - 1])
			continue;
		char name[16];
		snprintf(name, sizeof(name), "%d", row);
		BPoint pos(rowHeaderLeft + 4,
			Bounds().top + kHeaderHeight + fRowOffsets[row - 1] + fRowHeights[row - 1] - 6);
		DrawString(name, pos);
	}
	for (int row = firstRow; row <= lastRow; row++)
	{
		if (fRowHidden[row - 1])
			continue;
		char name[16];
		snprintf(name, sizeof(name), "%d", row);
		BPoint pos(rowHeaderLeft + 4,
			kHeaderHeight + fRowOffsets[row - 1] + fRowHeights[row - 1] - 6);
		DrawString(name, pos);
	}

	// Puntini sul confine fra due righe, nella colonna delle
	// intestazioni: indizio visivo di dove si puo' trascinare per
	// ridimensionare la riga (segnalato dall'utente dopo aver provato
	// il ridimensionamento senza nessun riferimento visivo -- prima
	// c'era solo la funzionalita', senza modo di scoprirla guardando
	// lo schermo). Non disegnato dopo l'ultima riga (row == kRowCount),
	// che non ha un confine "dopo" di se'.
	SetHighColor(140, 140, 140);
	for (int row = 1; row <= fFrozenRows && row < kRowCount; row++)
	{
		float y = Bounds().top + kHeaderHeight + fRowOffsets[row];
		float midX = rowHeaderLeft + kHeaderWidth / 2.0f;
		FillEllipse(BPoint(midX - 4, y), 1, 1);
		FillEllipse(BPoint(midX, y), 1, 1);
		FillEllipse(BPoint(midX + 4, y), 1, 1);
	}
	for (int row = firstRow; row <= lastRow && row < kRowCount; row++)
	{
		float y = kHeaderHeight + fRowOffsets[row];
		float midX = rowHeaderLeft + kHeaderWidth / 2.0f;
		FillEllipse(BPoint(midX - 4, y), 1, 1);
		FillEllipse(BPoint(midX, y), 1, 1);
		FillEllipse(BPoint(midX + 4, y), 1, 1);
	}

	// Grafici incorporati: prima dell'intestazione di colonna (sotto),
	// cosi' quest'ultima -- "congelata", quindi sempre sopra qualunque
	// altra cosa -- copre un eventuale grafico scorso proprio sotto di
	// lei, invece di comparire dietro. I dati si leggono dal vivo
	// (BuildChartSeries) a ogni ridisegno, non da un'istantanea
	// salvata -- cosi' un grafico incorporato riflette sempre lo stato
	// attuale delle celle sorgente.
	if (fCharts && fDoc)
	{
		for (size_t i = 0; i < fCharts->size(); i++)
		{
			const ChartObject& obj = (*fCharts)[i];
			if (!obj.frame.Intersects(updateRect))
				continue;

			std::vector<ChartSeries> series;
			BuildChartSeries(fDoc, obj.dataRange, series);
			DrawBarChart(this, obj.frame, series);
		}
	}

	// Immagini incorporate (Fase 12): stesso principio dei grafici
	// sopra ma ancorate a una cella (anchor+scarto in pixel, come
	// l'anchor XLSX originale) invece di un BRect assoluto -- cosi'
	// seguono la cella se righe/colonne cambiano dimensione, esattamente
	// come in Excel vero (vedi il commento su EmbeddedImage in
	// EmbeddedImage.h).
	if (fImages && fDoc)
	{
		for (size_t i = 0; i < fImages->size(); i++)
		{
			const EmbeddedImage& img = (*fImages)[i];
			BRect frame = ImageFrame(img);
			if (!frame.Intersects(updateRect))
				continue;

			BBitmap* bitmap = DecodeImageBytes(img.pngData);
			if (bitmap)
			{
				// Bug reale: senza questo cambio di modalita' resta
				// attiva B_OP_COPY (l'impostazione predefinita di
				// BView, mai toccata fin qui in questo Draw()), che
				// scrive il colore del bitmap cosi' com'e' ignorando
				// il canale alpha -- un PNG con sfondo trasparente
				// mostrerebbe uno sfondo pieno/nero invece della
				// cella sottostante. Stesso schema gia' in uso sopra
				// per il tinteggio della selezione (righe 1832-1838).
				SetDrawingMode(B_OP_ALPHA);
				SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
				DrawBitmap(bitmap, bitmap->Bounds(), frame);
				SetDrawingMode(B_OP_COPY);
				delete bitmap;
			}

			// Maniglia di ridimensionamento: sempre visibile (non solo al
			// passaggio del mouse), stesso motivo gia' scritto per i
			// puntini di ridimensionamento riga/colonna -- senza un
			// indizio visivo permanente l'interazione non sarebbe
			// scopribile guardando lo schermo.
			SetHighColor(80, 80, 80);
			FillRect(ImageResizeHandle(img));
		}
	}

	// Intestazione di colonna (lettere): "congelata" durante lo scroll
	// verticale -- disegnata sempre all'inizio dell'area visibile
	// (Bounds().top, che segue la posizione corrente di scroll), non a
	// una posizione fissa nel canvas virtuale (com'era prima, e come
	// resta l'intestazione di riga sopra) -- richiesta esplicita
	// dell'utente dopo aver visto la prima versione della UI. Segue
	// comunque lo scroll orizzontale normalmente (le lettere restano
	// allineate alle colonne vere): solo la coordinata verticale e'
	// "agganciata" alla cima della viewport, non quella orizzontale.
	// Disegnata per ultima cosi' resta sopra a tutto il resto
	// (contenuto delle celle, grafici) che le scorre sotto.
	float headerTop = Bounds().top;
	SetHighColor(230, 230, 230);
	FillRect(BRect(updateRect.left, headerTop, updateRect.right, headerTop + kHeaderHeight - 1));

	// Blocca riquadri: speculare al blocco analogo dell'intestazione di
	// riga sopra, sull'altro asse -- le lettere delle colonne congelate
	// restano incollate al bordo sinistro della viewport.
	SetHighColor(0, 0, 0);
	for (int col = 1; col <= fFrozenCols; col++)
	{
		char name[8];
		ColumnName(col, name);
		BPoint pos(Bounds().left + kHeaderWidth + fColOffsets[col - 1] + 4,
			headerTop + kHeaderHeight - 6);
		DrawString(name, pos);
	}
	for (int col = firstCol; col <= lastCol; col++)
	{
		char name[8];
		ColumnName(col, name);
		BPoint pos(kHeaderWidth + fColOffsets[col - 1] + 4, headerTop + kHeaderHeight - 6);
		DrawString(name, pos);
	}

	// Puntini sul confine fra due colonne, speculari a quelli
	// dell'intestazione di riga sopra -- stesso motivo (indizio
	// visivo del ridimensionamento, altrimenti scopribile solo per
	// caso trascinando alla cieca).
	SetHighColor(140, 140, 140);
	for (int col = 1; col <= fFrozenCols && col < kColCount; col++)
	{
		float x = Bounds().left + kHeaderWidth + fColOffsets[col];
		float midY = headerTop + kHeaderHeight / 2.0f;
		FillEllipse(BPoint(x, midY - 4), 1, 1);
		FillEllipse(BPoint(x, midY), 1, 1);
		FillEllipse(BPoint(x, midY + 4), 1, 1);
	}
	for (int col = firstCol; col <= lastCol && col < kColCount; col++)
	{
		float x = kHeaderWidth + fColOffsets[col];
		float midY = headerTop + kHeaderHeight / 2.0f;
		FillEllipse(BPoint(x, midY - 4), 1, 1);
		FillEllipse(BPoint(x, midY), 1, 1);
		FillEllipse(BPoint(x, midY + 4), 1, 1);
	}
}

void SheetView::MouseDown(BPoint where)
{
	if (fEditor)
		CommitEditing(false);

	// Ridimensionamento riga/colonna: trascinando il confine fra due
	// intestazioni si allarga/stringe quella colonna/riga. Controllato
	// PRIMA di tutto il resto (che oggi non fa nulla per un clic
	// sull'intestazione, vedi sotto), cosi' la maniglia ha sempre la
	// precedenza sul resto. Le bande delle intestazioni sono
	// "congelate" durante lo scroll (vedi Draw(): headerTop/
	// rowHeaderLeft seguono Bounds().top/Bounds().left, non 0 fisso),
	// quindi il confronto usa Bounds() per restare corretto anche a
	// foglio scorso, non solo appena aperto.
	BRect bounds = Bounds();
	if (where.y >= bounds.top && where.y < bounds.top + kHeaderHeight
		&& where.x >= bounds.left + kHeaderWidth)
	{
		int col = ColumnBoundaryAt(where.x - kHeaderWidth);
		if (col > 0)
		{
			fResizingColumn = col;
			fResizeDragStart = where.x;
			fResizeStartSize = fColWidths[col - 1];
			SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
			return;
		}

		// Non su una maniglia di ridimensionamento: un clic altrove
		// sull'intestazione di colonna seleziona l'intera colonna
		// (Maiusc la estende dall'ancora corrente fino alla colonna
		// cliccata, come un clic normale su una cella) -- come Excel/
		// LibreOffice Calc. Mancava del tutto prima d'ora: bug
		// segnalato dall'utente ("non posso selezionare l'intera
		// colonna o l'intera riga").
		//
		// L'intervallo di righe e' sempre [1, kRowCount] (tutta la
		// colonna) indipendentemente dalla riga dell'ancora corrente:
		// SetSelection() qui sotto fissa entrambi gli angoli sulla
		// riga kRowCount, poi ExtendSelection() sposta solo l'angolo
		// "attivo" alla riga 1 -- la cella attiva finale (fSelection)
		// resta quindi in cima, visibile, invece di trascinare la
		// vista fino in fondo al foglio (16384 righe) come farebbe
		// lasciandola sull'ultima riga.
		int clickedCol = ColumnAtX(where.x - kHeaderWidth);
		int32 mods = 0;
		BMessage* msg = Window() ? Window()->CurrentMessage() : NULL;
		if (msg)
			msg->FindInt32("modifiers", &mods);
		int anchorCol = (mods & B_SHIFT_KEY) ? fAnchor.h : clickedCol;
		SetSelection(cell(anchorCol, kRowCount));
		ExtendSelection(cell(clickedCol, 1));
		return;
	}
	if (where.x >= bounds.left && where.x < bounds.left + kHeaderWidth
		&& where.y >= bounds.top + kHeaderHeight)
	{
		int row = RowBoundaryAt(where.y - kHeaderHeight);
		if (row > 0)
		{
			fResizingRow = row;
			fResizeDragStart = where.y;
			fResizeStartSize = fRowHeights[row - 1];
			SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
			return;
		}

		// Stesso principio dell'intestazione di colonna sopra, per la
		// riga intera (colonne [1, kColCount] sempre complete, cella
		// attiva finale a sinistra invece che sull'ultima colonna).
		int clickedRow = RowAtY(where.y - kHeaderHeight);
		int32 mods = 0;
		BMessage* msg = Window() ? Window()->CurrentMessage() : NULL;
		if (msg)
			msg->FindInt32("modifiers", &mods);
		int anchorRow = (mods & B_SHIFT_KEY) ? fAnchor.v : clickedRow;
		SetSelection(cell(kColCount, anchorRow));
		ExtendSelection(cell(1, clickedRow));
		return;
	}

	// Bug scoperto implementando Blocca riquadri: le bande delle
	// intestazioni sono "congelate" (vedi sopra), quindi il confronto
	// deve usare bounds.left/bounds.top come i controlli di
	// ridimensionamento appena sopra, non 0 fisso -- altrimenti un
	// clic sull'intestazione dopo aver scorso il foglio (ma non su una
	// maniglia di ridimensionamento) non veniva scartato: proseguiva
	// fino a CellAt(), selezionando una cella quasi a caso invece di
	// non fare nulla.
	if (where.x < bounds.left + kHeaderWidth || where.y < bounds.top + kHeaderHeight)
		return;

	// Ridimensionamento di un'immagine incorporata: la maniglia
	// (ImageResizeHandle, angolo in basso a destra) e' un bersaglio piu'
	// piccolo e piu' specifico del corpo dell'immagine, quindi va
	// controllata PRIMA dello spostamento sotto -- altrimenti un clic
	// sulla maniglia sposterebbe l'immagine invece di ridimensionarla,
	// dato che la maniglia e' sempre contenuta nel suo ImageFrame.
	// Stesso ciclo all'INDIETRO del blocco sotto, stesso motivo.
	if (fImages)
	{
		for (int i = (int)fImages->size() - 1; i >= 0; i--)
		{
			const EmbeddedImage& img = (*fImages)[i];
			if (ImageResizeHandle(img).Contains(where))
			{
				fResizingImageIndex = i;
				fResizeImageStart = where;
				fResizeImageStartWidth = img.width;
				fResizeImageStartHeight = img.height;
				SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
				return;
			}
		}
	}

	// Trascinamento di un'immagine incorporata: un clic dentro il suo
	// rettangolo (ImageFrame, lo stesso usato da Draw() per disegnarla)
	// la afferra per spostarla invece di selezionare la cella sotto --
	// stessa precedenza delle maniglie di ridimensionamento sopra,
	// controllato PRIMA della selezione normale. Ciclo all'INDIETRO:
	// un'immagine disegnata sopra un'altra (ultima nell'elenco, vedi
	// Draw()) deve avere la precedenza su un clic nell'area dove si
	// sovrappongono.
	if (fImages)
	{
		for (int i = (int)fImages->size() - 1; i >= 0; i--)
		{
			const EmbeddedImage& img = (*fImages)[i];
			if (ImageFrame(img).Contains(where))
			{
				fDraggingImageIndex = i;
				fDragImageStart = where;
				fDragImageStartOffsetX = img.offsetX;
				fDragImageStartOffsetY = img.offsetY;
				SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
				return;
			}
		}
	}

	cell c = CellAt(where);

	// AutoFilter: un clic sulla freccia a discesa dell'intestazione (non
	// genericamente sulla cella) apre il menu dei valori invece di
	// selezionare la cella come al solito -- controllato PRIMA della
	// normale gestione della selezione sotto, stessa precedenza delle
	// maniglie di ridimensionamento piu' sopra.
	if (fHasAutoFilter && c.v == fAutoFilterRange.top
		&& c.h >= fAutoFilterRange.left && c.h <= fAutoFilterRange.right
		&& AutoFilterArrowRect(c.h).Contains(where))
	{
		ShowAutoFilterMenu(c.h, where);
		return;
	}

	// Celle unite: un clic in un punto qualunque dell'intervallo unito
	// deve selezionare la stessa cella logica (l'angolo in alto a
	// sinistra, l'unica che porta davvero contenuto/stile -- stesso
	// principio gia' in uso per il disegno, vedi Draw() sotto) invece
	// della cella fisica sotto il dito: senza questo, un clic su E4 di
	// un intervallo unito D4:F4 selezionava E4 (vuota, nessun valore)
	// invece di D4, facendo sembrare la cella mai unita. Bug reale
	// segnalato dall'utente su un file reale con celle unite.
	if (fDoc)
	{
		range mergedRange;
		if (fDoc->GetMergedRange(c, &mergedRange))
			c = cell(mergedRange.left, mergedRange.top);
	}

	int32 clicks = 1;
	int32 mods = 0;
	BMessage* msg = Window() ? Window()->CurrentMessage() : NULL;
	if (msg)
	{
		msg->FindInt32("clicks", &clicks);
		msg->FindInt32("modifiers", &mods);
	}

	// Collegamenti ipertestuali (Fase 13): Ctrl+click apre il
	// collegamento della cella, come LibreOffice Calc -- non un click
	// semplice, che resterebbe altrimenti impossibile da usare per
	// selezionare/modificare una cella con un URL senza lanciare ogni
	// volta il browser. Seleziona comunque la cella prima di uscire,
	// stesso comportamento di un click normale.
	if ((mods & B_CONTROL_KEY) && fDoc && fDoc->HasHyperlink(c))
	{
		SetSelection(c);
		MainWindow* win = dynamic_cast<MainWindow*>(Window());
		if (win)
			win->OpenCellHyperlink(c.v, c.h);
		return;
	}

	// Maiusc+click estende la selezione dall'ancora corrente (come
	// Excel/LibreOffice Calc); un click semplice la fa ripartire da
	// qui. Il trascinamento successivo (MouseMoved) estende sempre
	// dall'ancora impostata qui, indipendentemente da Maiusc.
	if (mods & B_SHIFT_KEY)
		ExtendSelection(c);
	else
		SetSelection(c);

	// SetMouseEventMask, non un ciclo di tracking bloccante: MouseMoved
	// continua a essere richiamato dal Interface Kit con le coordinate
	// aggiornate finche' il bottone resta premuto, anche se il mouse
	// esce dai confini della vista (utile scorrendo la selezione oltre
	// il bordo visibile).
	if (clicks < 2)
	{
		fDragging = true;
		SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);
	}

	if (clicks >= 2)
		StartEditing(c);
}

void SheetView::MouseUp(BPoint where)
{
	fDragging = false;
	fResizingColumn = 0;
	fResizingRow = 0;
	// A differenza del ridimensionamento colonna/riga sopra (una
	// preferenza di sola visualizzazione, mai salvata), spostare
	// un'immagine CAMBIA il documento -- lo scarto e' gia' scritto
	// nell'elemento del vettore (MouseMoved sotto lo fa direttamente,
	// niente da ricopiare qui), manca solo segnalarlo cosi' il titolo
	// mostra l'asterisco e "Salva" lo scrive davvero nel file.
	if (fDraggingImageIndex >= 0)
		NotifyDocumentChanged();
	fDraggingImageIndex = -1;
	// Stesso motivo del blocco sopra: ridimensionare un'immagine cambia
	// width/height nel documento, non solo una preferenza di
	// visualizzazione.
	if (fResizingImageIndex >= 0)
		NotifyDocumentChanged();
	fResizingImageIndex = -1;
	BView::MouseUp(where);
}

void SheetView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	// Trascinamento di un'immagine incorporata in corso (armato da
	// MouseDown): la nuova posizione e' quella di partenza piu' lo
	// spostamento del mouse dall'inizio del trascinamento, stesso
	// principio del ridimensionamento colonna/riga sotto. Scrive
	// direttamente sull'elemento del vettore di MainWindow (vedi il
	// commento su SetImages in SheetView.h), quindi il nuovo scarto e'
	// gia' "nel modello" a ogni fotogramma, non solo al rilascio del
	// mouse.
	if (fDraggingImageIndex >= 0 && fImages
		&& fDraggingImageIndex < (int)fImages->size())
	{
		EmbeddedImage& img = (*fImages)[fDraggingImageIndex];
		img.offsetX = fDragImageStartOffsetX + (where.x - fDragImageStart.x);
		img.offsetY = fDragImageStartOffsetY + (where.y - fDragImageStart.y);
		Invalidate();
		return;
	}

	// Ridimensionamento di un'immagine incorporata in corso (armato da
	// MouseDown tramite ImageResizeHandle): stesso principio esatto dello
	// spostamento appena sopra, ma su width/height invece di offsetX/
	// offsetY -- mai sotto un minimo (altrimenti l'immagine sparirebbe
	// insieme alla maniglia per ringrandirla, stesso motivo del minimo
	// colonna/riga sotto).
	if (fResizingImageIndex >= 0 && fImages
		&& fResizingImageIndex < (int)fImages->size())
	{
		const float kMinImageSize = 10;
		EmbeddedImage& img = (*fImages)[fResizingImageIndex];
		float newWidth = fResizeImageStartWidth + (where.x - fResizeImageStart.x);
		float newHeight = fResizeImageStartHeight + (where.y - fResizeImageStart.y);
		img.width = std::max(kMinImageSize, newWidth);
		img.height = std::max(kMinImageSize, newHeight);
		Invalidate();
		return;
	}

	// Trascinamento di ridimensionamento in corso (armato da
	// MouseDown): la nuova larghezza/altezza e' quella di partenza piu'
	// lo spostamento del mouse dall'inizio del trascinamento, mai sotto
	// il minimo (altrimenti la colonna/riga sparirebbe insieme alla
	// maniglia per riallargarla). Non e' annullabile ne' marcata come
	// "documento modificato": e' una preferenza di sola visualizzazione
	// per questa sessione, non salvata nel file (vedi il commento sui
	// campi fColWidths/fRowHeights in SheetView.h).
	if (fResizingColumn > 0)
	{
		float newWidth = fResizeStartSize + (where.x - fResizeDragStart);
		if (newWidth < kMinColWidth)
			newWidth = kMinColWidth;
		fColWidths[fResizingColumn - 1] = newWidth;
		RebuildColumnOffsets();
		UpdateCanvasSize();
		Invalidate();
		return;
	}
	if (fResizingRow > 0)
	{
		float newHeight = fResizeStartSize + (where.y - fResizeDragStart);
		if (newHeight < kMinRowHeight)
			newHeight = kMinRowHeight;
		fRowHeights[fResizingRow - 1] = newHeight;
		RebuildRowOffsets();
		UpdateCanvasSize();
		Invalidate();
		return;
	}

	// Cursore a doppia freccia passando sopra un confine ridimensionabile
	// (anche senza trascinare): indizio visivo in piu' oltre ai puntini
	// disegnati in Draw(), segnalato dall'utente dopo aver provato il
	// ridimensionamento senza nessun riferimento visivo. B_EXITED_VIEW
	// riporta subito al cursore normale, altrimenti resterebbe
	// "incollato" a doppia freccia una volta che il mouse esce dalla
	// vista. fHoverCursor evita di richiamare SetViewCursor a ogni
	// singolo MouseMoved quando non e' cambiato nulla.
	int hoverCursor = 0;
	if (code != B_EXITED_VIEW)
	{
		BRect bounds = Bounds();
		if (where.y >= bounds.top && where.y < bounds.top + kHeaderHeight
			&& where.x >= bounds.left + kHeaderWidth
			&& ColumnBoundaryAt(where.x - kHeaderWidth) > 0)
			hoverCursor = 1;
		else if (where.x >= bounds.left && where.x < bounds.left + kHeaderWidth
			&& where.y >= bounds.top + kHeaderHeight
			&& RowBoundaryAt(where.y - kHeaderHeight) > 0)
			hoverCursor = 2;
		else if (fImages)
		{
			// Immagine incorporata: stesso indizio visivo del
			// ridimensionamento sopra, per lo stesso motivo (segnalato
			// dall'utente per il ridimensionamento, vale identico qui --
			// senza, trascinare un'immagine non sarebbe scopribile
			// guardando lo schermo).
			for (size_t i = 0; i < fImages->size() && hoverCursor == 0; i++)
			{
				if (ImageResizeHandle((*fImages)[i]).Contains(where))
					hoverCursor = 4;
				else if (ImageFrame((*fImages)[i]).Contains(where))
					hoverCursor = 3;
			}
		}
	}

	if (hoverCursor != fHoverCursor)
	{
		fHoverCursor = hoverCursor;
		if (hoverCursor == 1)
		{
			BCursor cursor(B_CURSOR_ID_RESIZE_EAST_WEST);
			SetViewCursor(&cursor);
		}
		else if (hoverCursor == 2)
		{
			BCursor cursor(B_CURSOR_ID_RESIZE_NORTH_SOUTH);
			SetViewCursor(&cursor);
		}
		else if (hoverCursor == 3)
		{
			BCursor cursor(B_CURSOR_ID_MOVE);
			SetViewCursor(&cursor);
		}
		else if (hoverCursor == 4)
		{
			BCursor cursor(B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST);
			SetViewCursor(&cursor);
		}
		else
		{
			BCursor cursor(B_CURSOR_ID_SYSTEM_DEFAULT);
			SetViewCursor(&cursor);
		}
	}

	if (!fDragging)
	{
		BView::MouseMoved(where, code, dragMessage);
		return;
	}

	// A differenza di MouseDown, qui non si scarta un punto sopra le
	// intestazioni: trascinare fin li' deve comunque estendere la
	// selezione fino al bordo del foglio (colonna/riga 1), non
	// interrompere il trascinamento.
	ExtendSelection(CellAt(where));
}

void SheetView::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes != 1)
	{
		BView::KeyDown(bytes, numBytes);
		return;
	}

	// Ctrl/Shift non sono nel byte stesso (bytes[0] resta lo stesso
	// tasto, es. B_HOME, indipendentemente dai modificatori): vanno
	// letti dal messaggio B_KEY_DOWN corrente, stesso meccanismo gia'
	// usato in MouseDown per "clicks".
	int32 mods = 0;
	BMessage* msg = Window() ? Window()->CurrentMessage() : NULL;
	if (msg)
		msg->FindInt32("modifiers", &mods);

	if (!HandleKey(bytes[0], (mods & B_CONTROL_KEY) != 0, (mods & B_SHIFT_KEY) != 0))
		BView::KeyDown(bytes, numBytes);
}

bool SheetView::HandleKey(char key, bool ctrl, bool shift)
{
	cell c = fSelection;
	switch (key)
	{
		case B_UP_ARROW:
			c.v--;
			if (shift) ExtendSelection(c); else SetSelection(c);
			return true;
		case B_DOWN_ARROW:
			c.v++;
			if (shift) ExtendSelection(c); else SetSelection(c);
			return true;
		case B_RETURN:
			// Come Excel/LibreOffice Calc: Invio da soli (fuori
			// dall'editing in-cella, gia' gestito a parte da
			// CommitEditing) sposta comunque la selezione in basso;
			// Maiusc+Invio la sposta in alto.
			if (shift)
				c.v--;
			else
				c.v++;
			SetSelection(c);
			return true;
		case B_LEFT_ARROW:
			c.h--;
			if (shift) ExtendSelection(c); else SetSelection(c);
			return true;
		case B_RIGHT_ARROW:
			c.h++;
			if (shift) ExtendSelection(c); else SetSelection(c);
			return true;
		case B_TAB:
			// Maiusc+Tab sposta a sinistra invece che a destra, come
			// in Excel/LibreOffice Calc.
			if (shift)
				c.h--;
			else
				c.h++;
			SetSelection(c);
			return true;
		case B_HOME:
			// Come Excel: Inizio da solo va alla colonna A della riga
			// corrente; Ctrl+Inizio va sempre ad A1.
			if (ctrl)
				c.Set(1, 1);
			else
				c.h = 1;
			SetSelection(c);
			return true;
		case B_END:
			// Come Excel: Ctrl+Fine va all'ultima cella con contenuto
			// (angolo in basso a destra dei dati). Senza Ctrl non ha
			// un comportamento utile in questa griglia (in Excel
			// dipende dalla modalita' "scroll lock", non applicabile
			// qui) -- nessuna azione, ma il tasto e' comunque
			// "gestito" (non passa a BView::KeyDown).
			if (ctrl && fDoc)
			{
				range bounds;
				fDoc->GetBounds(bounds);
				if (bounds.right >= 1 && bounds.bottom >= 1)
				{
					c.Set(bounds.right, bounds.bottom);
					SetSelection(c);
				}
			}
			return true;
		case B_PAGE_UP:
		case B_PAGE_DOWN:
		{
			// Una "pagina" e' alta quanto l'area visibile della
			// BScrollView (stessa fonte di verita' di
			// ScrollToShowSelection/FixupScrollBars), non un numero
			// fisso di righe: coerente con quante righe scompaiono
			// davvero scorrendo di una schermata.
			BRect viewport = Parent() ? Parent()->Bounds() : Bounds();
			int pageRows = (int)(viewport.Height() / kRowHeight);
			if (pageRows < 1)
				pageRows = 1;
			c.v += (key == B_PAGE_DOWN) ? pageRows : -pageRows;
			SetSelection(c);
			return true;
		}
		case B_BACKSPACE:
		case B_DELETE:
			ClearSelection();
			return true;
		default:
			// Niente scorciatoia da tastiera per "seleziona tutto" qui:
			// su Haiku B_HOME vale 0x01, lo stesso byte che Ctrl+A
			// genera (vedi InterfaceDefs.h: "B_HOME = 0x01, // Ctrl +
			// A") -- i due sono indistinguibili leggendo solo
			// bytes[0]/"key" del messaggio B_KEY_DOWN, quindi un tasto
			// Ctrl+A qui finirebbe per attivare invece Ctrl+Inizio (o
			// viceversa). SelectAll() resta comunque disponibile
			// pubblicamente, esposta dal menu Modifica > "Seleziona
			// tutto" in MainWindow, che non ha questa ambiguita'.
			// Digitare direttamente su una cella selezionata sostituisce
			// il contenuto (come Excel/LibreOffice Calc): si apre
			// l'editor in-cella gia' con il carattere digitato.
			if (!ctrl && (unsigned char)key >= 0x20 && (unsigned char)key < 0x7f)
			{
				char initial[2] = { key, 0 };
				StartEditing(fSelection, initial);
				return true;
			}
			return false;
	}
}

void SheetView::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgCellEditCommit:
			CommitEditing(false);
			break;
		case kMsgCellEditCancel:
			CommitEditing(true);
			break;
		default:
			BView::MessageReceived(message);
			break;
	}
}

void SheetView::StartEditing(cell c, const char* initialText)
{
	if (!fDoc)
		return;

	if (fEditor)
		CommitEditing(false);

	fEditingCell = c;

	BRect r = CellRect(c);
	fEditor = new BTextControl(r, "celledit", NULL, "",
		new BMessage(kMsgCellEditCommit), B_FOLLOW_NONE, B_WILL_DRAW | B_NAVIGABLE);
	fEditor->SetDivider(0);
	fEditor->SetTarget(this);
	AddChild(fEditor);

	if (initialText)
		fEditor->SetText(initialText);
	else
	{
		char text[4096];
		fDoc->GetCellFormula(c, text, sizeof(text), false);

		// Stesso motivo di MainWindow::SelectionChanged (barra della
		// formula): GetCellFormula non antepone mai "=", quindi senza
		// questo il doppio clic su una formula la mostrerebbe qui
		// identica a un numero/testo normale invece che come "=A1+B1".
		// TryToParseString (CommitEditing sotto) accetta comunque
		// entrambe le forme, quindi aggiungerla qui e' sicuro.
		if (fDoc->GetCellFormula(c) != NULL)
		{
			char withEquals[4097];
			snprintf(withEquals, sizeof(withEquals), "=%s", text);
			strlcpy(text, withEquals, sizeof(text));
		}

		fEditor->SetText(text);
	}

	if (fEditor->TextView())
		fEditor->TextView()->AddFilter(new CellEditKeyFilter(this));

	// MakeFocus() PRIMA di Select(): BTextView seleziona tutto il
	// proprio testo quando acquisisce il fuoco per la prima volta (lo
	// stesso comportamento "seleziona tutto" di un campo di testo
	// standard) -- chiamare Select(len, len) prima di MakeFocus()
	// veniva quindi subito sovrascritto dalla selezione automatica.
	// Bug reale segnalato dall'utente: digitando un numero di piu'
	// cifre (es. "123" su una cella vuota, che apre l'editor gia' con
	// "1" digitato), il primo tasto successivo ("2") sostituiva
	// l'intero contenuto selezionato invece di aggiungersi, dando
	// "23" invece di "123" -- solo il terzo tasto in poi si comportava
	// correttamente, perche' a quel punto non c'era piu' nessuna
	// selezione residua.
	fEditor->MakeFocus(true);

	if (fEditor->TextView())
	{
		int32 len = (int32)strlen(fEditor->Text());
		fEditor->TextView()->Select(len, len);
	}
}

void SheetView::CommitEditing(bool cancel)
{
	if (!fEditor)
		return;

	BTextControl* editor = fEditor;
	cell editedCell = fEditingCell;
	fEditor = NULL;

	if (!cancel && fDoc)
	{
		SaveUndoState(editedCell);
		try
		{
			TryToParseString(editor->Text(), editedCell, fDoc, true);
		}
		catch (...)
		{
		}
		RecalculateOwningWorkbook();
		NotifyDocumentChanged();
	}

	editor->RemoveSelf();
	delete editor;

	MakeFocus(true);

	if (!cancel)
	{
		// Come Excel/LibreOffice Calc: confermando con Invio la
		// selezione avanza alla cella sotto, invece di restare ferma
		// -- altrimenti, dopo aver scritto un valore, sembra che non
		// sia successo nulla (segnalato dall'utente) anche se il
		// valore e' stato scritto correttamente. fSelection e'
		// ancora editedCell a questo punto (mai cambiata durante
		// l'editing), quindi SetSelection si occupa gia' da sola di
		// invalidare/notificare/scorrere se la riga sotto e' fuori
		// dall'area visibile -- non serve piu' farlo qui a mano.
		cell next(editedCell.h, editedCell.v + 1);
		SetSelection(next);
	}
	else
	{
		Invalidate(CellRect(editedCell));
		NotifySelectionChanged();
	}
}

void SheetView::NotifySelectionChanged()
{
	MainWindow* win = dynamic_cast<MainWindow*>(Window());
	if (win)
		win->SelectionChanged(fSelection);
}

void SheetView::NotifyDocumentChanged()
{
	MainWindow* win = dynamic_cast<MainWindow*>(Window());
	if (win)
		win->DocumentChanged();
}

void SheetView::RecalculateOwningWorkbook()
{
	MainWindow* win = dynamic_cast<MainWindow*>(Window());
	if (win)
		win->RecalculateActiveWorkbook();
	else
		RecalculateAll(fDoc);
}
