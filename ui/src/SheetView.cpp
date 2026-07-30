/*
	SheetView.cpp

	Vedi SheetView.h.
*/

#include "SheetView.h"
#include "MainWindow.h"

#include <cstdio>
#include <cstring>

#include <MessageFilter.h>
#include <Messenger.h>
#include <Region.h>
#include <ScrollBar.h>
#include <String.h>
#include <TextControl.h>
#include <TextView.h>

#include "Value.h"
#include "CellParser.h"
#include "CellStyle.h"
#include "Container.h"
#include "Constants.h"
#include "Formatter.h"

static const uint32 kMsgCellEditCommit = 'cedt';
static const uint32 kMsgCellEditCancel = 'cedc';

// Filtro applicato alla BTextView interna del BTextControl usato per
// l'editing in-cella: la BTextView interna e' quella che riceve
// davvero il fuoco tastiera (BTextControl::MakeFocus lo inoltra a
// lei), quindi e' li' che serve intercettare Escape per annullare
// l'editing -- una BTextControl non riceve KeyDown direttamente per
// questo motivo.
class CellEditEscapeFilter : public BMessageFilter {
public:
	CellEditEscapeFilter(BHandler* target)
		: BMessageFilter(B_KEY_DOWN), fTarget(target) {}

	virtual filter_result Filter(BMessage* message, BHandler** target)
	{
		int32 rawChar;
		if (message->FindInt32("raw_char", &rawChar) == B_OK
			&& rawChar == B_ESCAPE)
		{
			BMessenger(fTarget).SendMessage(kMsgCellEditCancel);
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
	fDoc(doc),
	fSelection(1, 1),
	fCharts(NULL),
	fEditor(NULL),
	fEditingCell(1, 1)
{
	SetViewColor(255, 255, 255);

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

	float totalWidth = kHeaderWidth + kColCount * kColWidth;
	float totalHeight = kHeaderHeight + kRowCount * kRowHeight;

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

void SheetView::SetDocument(CContainer* doc)
{
	fDoc = doc;
	fSelection.Set(1, 1);
	Invalidate();
	NotifySelectionChanged();
}

void SheetView::SetSelection(cell c)
{
	if (c.h < 1)
		c.h = 1;
	if (c.v < 1)
		c.v = 1;
	if (c == fSelection)
		return;

	Invalidate(CellRect(fSelection));
	fSelection = c;
	Invalidate(CellRect(fSelection));
	ScrollToShowSelection();
	NotifySelectionChanged();
}

BRect SheetView::CellRect(cell c) const
{
	float x = kHeaderWidth + (c.h - 1) * kColWidth;
	float y = kHeaderHeight + (c.v - 1) * kRowHeight;
	return BRect(x, y, x + kColWidth, y + kRowHeight);
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

	float dx = 0, dy = 0;
	if (r.left < visible.left + kHeaderWidth)
		dx = r.left - kHeaderWidth - visible.left;
	else if (r.right > visible.right)
		dx = r.right - visible.right;

	if (r.top < visible.top + kHeaderHeight)
		dy = r.top - kHeaderHeight - visible.top;
	else if (r.bottom > visible.bottom)
		dy = r.bottom - visible.bottom;

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

void SheetView::Draw(BRect updateRect)
{
	SetHighColor(255, 255, 255);
	FillRect(updateRect);

	int firstCol = 1, lastCol = 20, firstRow = 1, lastRow = 40;
	if (updateRect.left > kHeaderWidth)
		firstCol = (int)((updateRect.left - kHeaderWidth) / kColWidth) + 1;
	lastCol = (int)((updateRect.right - kHeaderWidth) / kColWidth) + 2;
	if (updateRect.top > kHeaderHeight)
		firstRow = (int)((updateRect.top - kHeaderHeight) / kRowHeight) + 1;
	lastRow = (int)((updateRect.bottom - kHeaderHeight) / kRowHeight) + 2;
	if (firstCol < 1) firstCol = 1;
	if (firstRow < 1) firstRow = 1;

	SetHighColor(220, 220, 220);
	for (int col = firstCol; col <= lastCol; col++)
		StrokeLine(BPoint(kHeaderWidth + (col - 1) * kColWidth, updateRect.top),
			BPoint(kHeaderWidth + (col - 1) * kColWidth, updateRect.bottom));
	for (int row = firstRow; row <= lastRow; row++)
		StrokeLine(BPoint(updateRect.left, kHeaderHeight + (row - 1) * kRowHeight),
			BPoint(updateRect.right, kHeaderHeight + (row - 1) * kRowHeight));

	if (fDoc)
	{
		SetHighColor(0, 0, 0);
		SetLowColor(255, 255, 255);
		for (int row = firstRow; row <= lastRow; row++)
		{
			for (int col = firstCol; col <= lastCol; col++)
			{
				cell c(col, row);
				char text[512];
				fDoc->GetCellResult(c, text, true);
				if (text[0] == 0)
					continue;

				// Il motore formatta i numeri in modo generico
				// (CFormatter/eGeneral): per un valore puramente
				// numerico si applica invece una formattazione
				// locale-aware (separatore delle migliaia, punto o
				// virgola decimale secondo le preferenze di sistema)
				// tramite il Locale Kit, come livello di presentazione
				// sopra il testo gia' calcolato dal motore. Rispetta il
				// formato scelto per la cella (menu Formato): valuta e
				// percentuale usano le formattazioni dedicate del
				// Locale Kit, gli altri (incluso il generico) usano il
				// raggruppamento numerico semplice.
				Value val;
				if (fDoc->GetValue(c, val) && val.fType == eNumData && !val.IsNan())
				{
					CellStyle cs;
					fDoc->GetCellStyle(c, cs);

					BString formatted;
					status_t fmtErr;
					if (cs.fFormat == eCurrency)
						fmtErr = fNumberFormat.FormatMonetary(formatted, (double)val);
					else if (cs.fFormat == ePercent)
						fmtErr = fNumberFormat.FormatPercent(formatted, (double)val);
					else
						fmtErr = fNumberFormat.Format(formatted, (double)val);

					if (fmtErr == B_OK && formatted.Length() > 0)
						strlcpy(text, formatted.String(), sizeof(text));
				}

				BRect r = CellRect(c);
				BPoint pos(r.left + 3, r.bottom - 5);
				BRegion clip(r);
				ConstrainClippingRegion(&clip);
				DrawString(text, pos);
				ConstrainClippingRegion(NULL);
			}
		}
	}

	// Selezione corrente.
	SetHighColor(30, 100, 200);
	BRect sel = CellRect(fSelection);
	StrokeRect(sel);
	StrokeRect(sel.InsetByCopy(1, 1));

	// Intestazioni (posizione virtuale assoluta, non "congelate"
	// durante lo scroll -- limite noto della prima versione della UI,
	// vedi ROADMAP.md Fase 4).
	SetHighColor(230, 230, 230);
	FillRect(BRect(updateRect.left, 0, updateRect.right, kHeaderHeight - 1));
	FillRect(BRect(0, updateRect.top, kHeaderWidth - 1, updateRect.bottom));

	SetHighColor(0, 0, 0);
	for (int col = firstCol; col <= lastCol; col++)
	{
		char name[8];
		ColumnName(col, name);
		BPoint pos(kHeaderWidth + (col - 1) * kColWidth + 4, kHeaderHeight - 6);
		DrawString(name, pos);
	}
	for (int row = firstRow; row <= lastRow; row++)
	{
		char name[16];
		snprintf(name, sizeof(name), "%d", row);
		BPoint pos(4, kHeaderHeight + (row - 1) * kRowHeight + kRowHeight - 6);
		DrawString(name, pos);
	}

	// Grafici incorporati: dopo le intestazioni, cosi' restano visibili
	// per intero anche se posizionati vicino al bordo di riga/colonna
	// 1. I dati si leggono dal vivo (BuildChartSeries) a ogni ridisegno,
	// non da un'istantanea salvata -- cosi' un grafico incorporato
	// riflette sempre lo stato attuale delle celle sorgente.
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
}

void SheetView::MouseDown(BPoint where)
{
	if (fEditor)
		CommitEditing(false);

	if (where.x < kHeaderWidth || where.y < kHeaderHeight)
		return;

	int col = (int)((where.x - kHeaderWidth) / kColWidth) + 1;
	int row = (int)((where.y - kHeaderHeight) / kRowHeight) + 1;
	cell c(col, row);

	int32 clicks = 1;
	BMessage* msg = Window() ? Window()->CurrentMessage() : NULL;
	if (msg)
		msg->FindInt32("clicks", &clicks);

	SetSelection(c);
	if (clicks >= 2)
		StartEditing(c);
}

void SheetView::KeyDown(const char* bytes, int32 numBytes)
{
	if (numBytes != 1)
	{
		BView::KeyDown(bytes, numBytes);
		return;
	}

	cell c = fSelection;
	switch (bytes[0])
	{
		case B_UP_ARROW:
			c.v--;
			SetSelection(c);
			break;
		case B_DOWN_ARROW:
		case B_RETURN:
			c.v++;
			SetSelection(c);
			break;
		case B_LEFT_ARROW:
			c.h--;
			SetSelection(c);
			break;
		case B_RIGHT_ARROW:
		case B_TAB:
			c.h++;
			SetSelection(c);
			break;
		case B_BACKSPACE:
		case B_DELETE:
			if (fDoc)
			{
				fDoc->DisposeCell(fSelection);
				Invalidate(CellRect(fSelection));
				NotifySelectionChanged();
			}
			break;
		default:
			// Digitare direttamente su una cella selezionata sostituisce
			// il contenuto (come Excel/LibreOffice Calc): si apre
			// l'editor in-cella gia' con il carattere digitato.
			if ((unsigned char)bytes[0] >= 0x20 && (unsigned char)bytes[0] < 0x7f)
			{
				char initial[2] = { bytes[0], 0 };
				StartEditing(fSelection, initial);
			}
			else
				BView::KeyDown(bytes, numBytes);
			break;
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
		char text[512];
		fDoc->GetCellFormula(c, text, false);
		fEditor->SetText(text);
	}

	if (fEditor->TextView())
	{
		fEditor->TextView()->AddFilter(new CellEditEscapeFilter(this));
		int32 len = (int32)strlen(fEditor->Text());
		fEditor->TextView()->Select(len, len);
	}

	fEditor->MakeFocus(true);
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
		try
		{
			TryToParseString(editor->Text(), editedCell, fDoc, true);
		}
		catch (...)
		{
		}
		fDoc->CalcCell(editedCell);
	}

	editor->RemoveSelf();
	delete editor;

	MakeFocus(true);
	Invalidate(CellRect(editedCell));
	NotifySelectionChanged();
}

void SheetView::NotifySelectionChanged()
{
	MainWindow* win = dynamic_cast<MainWindow*>(Window());
	if (win)
		win->SelectionChanged(fSelection);
}
