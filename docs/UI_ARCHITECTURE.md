# Native application (`ui/`)

The Interface/Layout Kit application, written from scratch (does not
reuse the BeOS-era `CellView`/`CellWindow`) on top of the isolated
calculation engine (`engine/`) and the Translation Kit for file
interoperability (`translators/`).

## Structure

```
ui/src/App.h/.cpp          BApplication: creates the window, forwards
                            files opened from Tracker/command line
ui/src/MainWindow.h/.cpp   BWindow: File/Edit menus, toolbar, formula
                            bar, current cell, open/save
ui/src/SheetView.h/.cpp    Custom BView: grid, selection, editing
ui/src/AscdIO.h/.cpp       Native ASCD format read/write (same logic
                            duplicated in each translator, see
                            docs/TRANSLATORS.md)
ui/src/FindWindow.h/.cpp   Separate BWindow for Find & Replace
ui/Atomo123.rdef           App resources (signature, version, VICN/
                            BEOS:ICON icon), compiled by rc + xres
ui/icons/                  Source icon: atomo123.svg (hand-drawn) and
                            atomo123.hvif (exported from Icon-O-Matic,
                            embedded in Atomo123.rdef)
```

## `SheetView`'s canvas model

Not `BGridLayout` — the grid is drawn by hand in `Draw()` over a
`BScrollView`. `Frame()` covers the engine's entire virtual range from
construction (`kColCount`/`kRowCount` in
`engine/src/Config/Constants.h`, 702×16384 cells, ~56200×327700
pixels) — the classic BeOS/Haiku scrollable-view pattern: the
`BScrollView` clips and scrolls a large view, rather than resizing a
small one to fit content. Scrollbar range is computed manually
(`FixupScrollBars()`).

**Gotcha**: a `BScrollView` built the classic way (not through
`BLayoutBuilder`) inherits its *target's* size at construction time
rather than being constrained by the containing window's layout. With
a target this large, the `BScrollView` itself becomes just as large,
and `Parent()->Bounds()` (what code should treat as "the visible area")
reflects that huge size instead — code relying on it (auto-scroll to a
selected cell, layout size negotiation) silently breaks. Fixed with an
explicit one-time `scroll->ResizeTo(400, 300)` right after construction
*plus* explicit size constraints set directly on `SheetView` itself
(`SetExplicitMinSize`/`MaxSize`/`PreferredSize`) — the `ResizeTo` alone
only survives the first layout pass; without the constraints on the
target view, any later layout recalculation (e.g. from resizing the
window) re-derives the `BScrollView`'s size from `SheetView::Frame()`
again and the bug comes back.

## Opening files: Translation Kit as a real consumer

`MainWindow::OpenFile()` passes every file through
`BTranslatorRoster::Default()->Translate(&file, NULL, NULL, &ascd,
kAtomoNativeFormat)` — the roster asks each installed translator's
`Identify()` until one recognizes the format, then translates to ASCD.
The CSV translator also recognizes native ASCD by its signature, so a
single code path opens CSV/XLS/XLSX/ODS/ASCD without per-extension
branching. Translators must be installed separately (`make install` in
each `translators/*` directory) since they're runtime-loaded add-ons,
not statically linked into this binary.

Re-opening an already-native ASCD file skips the Translation Kit
entirely (`IsASCDFile()` checks the signature first) and calls
`LoadASCD` directly — routing a native file back through a translator
would silently lose anything the translators' own duplicated
`ReadASCD`/`WriteASCD` don't know about (e.g. embedded charts, a
UI-only concept).

## Editing: formula bar and in-cell editor

Two paths to the same write logic (`TryToParseString`/`CalcCell`):

- **Formula bar** (`MainWindow`): always visible, shows/edits the
  selected cell's formula, Enter commits
  (`MainWindow::CommitFormulaBar`).
- **In-cell editor** (`SheetView::StartEditing`/`CommitEditing`):
  double-click, or typing directly while a cell is selected (replaces
  content, like Excel), opens a temporary `BTextControl` positioned
  over the cell. A click elsewhere commits first
  (`CommitEditing(false)`); Escape cancels.

**Key handling gotcha**: a `BTextControl` doesn't receive `KeyDown` for
keys typed during editing — `MakeFocus()` forwards keyboard focus to
its internal `BTextView`, which handles the actual text. Both Escape
(cancel) and Enter (commit) are intercepted with a `BMessageFilter`
(`CellEditKeyFilter`) installed directly on that internal `BTextView`
(`BTextControl::TextView()->AddFilter(...)`), returning
`B_SKIP_MESSAGE` so the key isn't also inserted as a character. Relying
on `BTextControl`'s own automatic `Invoke()`-on-Enter proved unreliable
in this context — explicit interception avoids depending on it.
Confirming via Enter also advances the selection down a row, like
Excel; Escape does not.

## Toolbar: plain `BButton`s, not `BToolBar`

A row of text-only `BButton`s below the menu — `BToolBar` lives only
under `develop/headers/private/shared/` on this system, not the public
stable SDK, and this project deliberately sticks to public documented
APIs (Interface/Locale/Print/Translation/Clipboard Kit). Each button
posts the same `BMessage` the corresponding menu item already handles
— no new logic, just a second entry point to the same actions.

## Clipboard: the real system clipboard

Edit menu operations use Haiku's **Clipboard Kit** (`be_clipboard`),
not an app-internal buffer: copied content (a cell's formula, the same
text the formula bar shows) is written as `text/plain` inside the
`BMessage` from `be_clipboard->Data()`, between `Lock()`/`Clear()` and
`Commit()`/`Unlock()` — the standard pattern for interoperating with
other apps' clipboard content. Verified cross-checked against the
system `clipboard` command-line tool in both directions.

## Locale Kit: numbers formatted per system preferences

The calculation engine formats numbers generically (`CFormatter`/
`eGeneral`, no locale awareness — historical BeOS code). `SheetView`
adds a presentation layer on top: a numeric cell's on-grid text is
regenerated with `BNumberFormat::Format()` (system thousands
separator, decimal point/comma). The formula bar always shows the raw,
editable text (`GetCellFormula`), never the formatted version.

**Format menu** (General/Number/Currency/Percentage) sets
`CellStyle::fFormat` on the selected cell
(`MainWindow::SetCellFormat`). `SheetView::Draw()` reads that style
before applying locale-aware formatting: Currency uses
`BNumberFormat::FormatMonetary()`, Percentage uses
`FormatPercent()` (value as a fraction, e.g. 0.42 → "42%"), everything
else (including General) uses plain `Format()`.

**Not yet exposed in the UI**: control over decimal places, and date
formatting (`BDateFormat`) — the engine supports both via `CellStyle`,
but without a dedicated menu they stay at their defaults.

## Printing (`BPrintJob`)

File → Print (`MainWindow::PrintDocument`) follows the standard Haiku
pattern: `ConfigJob()` (system print dialog) → `BeginJob()` and a loop
over `SheetView::ContentRect()` (the pixel rectangle covering cells
with actual content, not the full 702×16384 virtual range) sliced into
`PrintableRect()`-sized pages, `DrawView()` + `SpoolPage()` per page →
`CommitJob()`/`CancelJob()`.

**Known limitation**: row/column headers are drawn only in the fixed
top-left band, so on a multi-page print job they only appear on the
first page, not repeated on every page.

## Find & Replace: cross-window messaging rule

"Find & Replace…" opens `FindWindow`, a separate `BWindow` with its own
`BLooper` (a different thread from `MainWindow`'s). It never touches
`MainWindow`'s document directly — it sends search/replace requests as
`BMessage`s through a `BMessenger` back to `MainWindow`, which owns the
document and processes them on its own thread. **This is the general
rule for any cross-window interaction in this codebase**: never call a
method that touches another window's `BView`s or document from a
different thread — always go through `PostMessage()`/`SendMessage()`.
The same rule was violated once between `BApplication` and
`MainWindow` (see "Notable fixes" below) and applies identically
between `MainWindow` and `ChartWindow`/`PivotWindow`.

`MainWindow::FindNext()` does a fresh linear scan
(`CCellIterator` + case-insensitive substring match) from the current
selection every time, wrapping to the start if nothing else matches —
no persisted iterator between searches, avoiding any risk of an
iterator invalidated by a document edit between two "Find Next"
clicks. `ReplaceAll()` first collects matching cells into a
`std::vector<cell>`, then mutates them in a second pass, since
`TryToParseString` can add/remove cells while `CCellIterator` is
walking the same map.

## Charts and pivot tables

Insert menu, reading a two-column range typed by the user (e.g.
`A1:B5` — the grid only supports single-cell selection today, no
drag-select range).

`Chart.cpp` (data + bar-layout math) and `Pivot.cpp` (grouping/
aggregation) never touch `BView`/`BWindow` — pure, headless-testable
logic (`make test-chart`, `make test-pivot`), same principle as the
engine itself.

`ChartWindow`/`PivotWindow` are separate `BWindow`s, so the
cross-thread messaging rule above applies:

- **Pivot**: `PivotWindow` sends the source range/destination/
  aggregation to `MainWindow` in one message; the result is written
  directly into the sheet (no data needs to flow back).
- **Chart**: bidirectional — `MainWindow` reads the document on its own
  thread, extracts plain data (labels/values, never a document
  pointer) and sends it back to `ChartWindow` in a second message,
  which then updates `ChartView` on its own thread. `DrawBarChart()`
  itself takes only already-extracted data, never a `CContainer*`, so
  it's safe to call from either window's thread.

**Embedded charts**: `MainWindow::fCharts` (`std::vector<ChartObject>`,
`{ range dataRange; BRect frame; }`) holds no data snapshot — each
redraw re-reads `dataRange` live from the document. Inserted via
"Insert into sheet" in `ChartWindow`; persisted in `AscdIO.cpp` as an
optional trailing section (old files without it just get an empty
vector back, not an error). Translators' own duplicated `ReadASCD`/
`WriteASCD` don't know about this section — deliberate, since charts
are a UI concept, not part of the generic data format.

## Frozen headers

The column-letter row stays fixed at the top during vertical scroll
(but follows horizontal scroll); the row-number column stays fixed on
the left during horizontal scroll. Implementation: `SheetView::Draw()`
draws the header bands at `Bounds().top`/`Bounds().left` (the visible
viewport's edges) instead of a fixed `(0,0)` in the virtual canvas,
with the column header drawn last so it stays above cell content and
the row header. A naive version of this flickered — `ScrollBy`/
`ScrollTo` only blit already-drawn pixels and invalidate the newly
exposed strip, so an already-drawn header band got dragged along with
the scroll instead of staying put. Fixed by overriding
`SheetView::ScrollTo(BPoint)` (the single interception point for
*every* scroll trigger) to invalidate just the old and new header
bands, not the whole view.

## Application icon

`ui/icons/atomo123.svg` (hand-drawn, flat SVG for Icon-O-Matic import
compatibility) → `ui/icons/atomo123.hvif` (exported from Icon-O-Matic)
→ embedded as a `VICN`/`BEOS:ICON` resource in `ui/Atomo123.rdef`,
compiled by `rc` and attached with `xres` as part of the normal build.

## Known limitations

- Format menu covers General/Number/Currency/Percentage only — no
  control over decimal places, font, color, border, alignment or date
  formatting from a dedicated menu (the engine supports all of these
  via `CellStyle`, just not exposed yet where noted above).
- No drag-select range on the grid — chart/pivot ranges are typed
  manually.

## Notable fixes found building this UI

- **`BApplication`/`BWindow` thread violation**: `App::RefsReceived`
  (application thread) called `MainWindow::OpenFile()` directly, which
  touches `BView`s that live on the window's own `BLooper` thread —
  corrupting state under a concurrent `Draw()`/`Invalidate()` from the
  correct thread. Fixed by forwarding the message with `PostMessage()`
  instead of calling directly — the rule now documented above.
- **Formulas never recalculated on load**: `TryToParseString` (used by
  `LoadASCD` and the CSV translator's `ReadASCD`) sets a cell's
  formula but doesn't calculate it — any file opened with formula
  cells showed them blank until the user touched them by hand. Fixed
  with a new `RecalculateAll()` that iterates and calls `CalcCell` on
  every cell, repeating until nothing changes (dependency order isn't
  guaranteed by insertion order).
- **Grid not filling the window**: `SheetView` was constructed with a
  fixed placeholder `Frame()` (100×100) never resized afterward — an
  app_server `Draw()` update rect can never exceed a view's own
  `Frame()`, so only that tiny corner ever rendered regardless of
  window size. Fixed by giving `SheetView` the full virtual-canvas
  `Frame()` from construction (see canvas model above).
- **Enter not committing in-cell edits**: relying on `BTextControl`'s
  automatic `Invoke()` on Enter proved unreliable; fixed by
  intercepting `B_RETURN` explicitly with the same key-filter mechanism
  used for Escape (see "Editing" above).
