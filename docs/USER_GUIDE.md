# User Guide

For using Atomo123, not for developing it — see `docs/ENGINE_API.md`,
`docs/TRANSLATORS.md` and `docs/UI_ARCHITECTURE.md` for internals, and
`ROADMAP.md` for current project status.

## Quick start

Atomo123 is a native spreadsheet for Haiku OS, compatible with Excel
(XLS/XLSX) and OpenOffice/LibreOffice Calc (ODS/CSV) files — double-click
one of those file types in Tracker to open it directly.

```
cd ui
make
./Atomo123
```

To open or save CSV/XLS/XLSX/ODS files (not just the native ASCD
format), install the corresponding Translation Kit add-ons once:

```
cd translators/csv && make && make install
cd translators/xls && make && make install
cd translators/xlsx && make && make install
cd translators/ods && make && make install
```

`make install` copies each add-on to
`~/config/non-packaged/add-ons/Translators/`, where Atomo123 finds it
automatically.

**The essentials:**
- **Typing in a cell**: click to select, then type directly (replaces
  existing content). Enter confirms and moves down, like Excel; Esc
  cancels.
- **Formulas**: start with `=`, e.g. `=SUM(A1:A3)` — function names are
  in English, not localized. Multiple arguments are separated with a
  **semicolon** (`;`), not a comma.
- **Saving**: File → Save As…, with the format chosen from the file
  extension (`.xlsx`, `.ods`, `.csv`, or none for the native ASCD
  format).
- **Multiple sheets**: tabs at the bottom of the window work like
  Excel/LibreOffice Calc — click to switch, right-click to add/rename/
  delete.
- **The footer**: bottom-left shows editing state ("Ready"/"Editing");
  bottom-right shows selection statistics (Sum/Average/Count by
  default) — right-click to choose which ones to show.

Main shortcuts: Ctrl+C/X/V (copy/cut/paste), Ctrl+Z/Y (undo/redo),
Ctrl+F (find & replace), Tab/Shift+Tab and Enter/Shift+Enter (move
between cells), Ctrl+Home/Ctrl+End (first/last cell with data).

## The main window

Top: **File**, **Edit**, **Format**, **Insert** and **Data** menus,
then the selected cell's reference (e.g. "A1") and the formula bar.
Below that, the sheet grid.

- **Selecting a cell**: click, or arrow keys.
- **Viewing/editing a formula**: the formula bar always shows the raw
  content (the formula, not the calculated value) of the selected
  cell. Edit there and confirm with Enter.
- **Editing directly on the grid**: double-click a cell, or start
  typing while it's selected (replaces existing content, like Excel).
  Enter or clicking another cell confirms (and, like Excel, moves the
  selection down); Esc cancels without moving the selection.
- **Clearing a cell**: Delete/Backspace, or Edit → Clear.
- **Frozen headers**: the column-letter row stays visible while
  scrolling down; the row-number column stays visible while scrolling
  right.
- **Additional shortcuts**: Tab/Shift+Tab move right/left; Enter/
  Shift+Enter move down/up (also outside editing); Home goes to column
  A of the current row, Ctrl+Home always goes to A1; Ctrl+End goes to
  the last cell with content; Page Up/Down move a full screen.

### The footer

- **Left**: editing-state indicator — "Ready" at rest, "Editing" while
  typing (in a cell or the formula bar), like Excel.
- **Right**: statistics for the current selection (Average, Count, Sum
  shown by default when the selection has at least one numeric value;
  Count only for text). **Right-click** to choose which statistics to
  show — also Numeric Count, Min and Max, off by default like Excel's
  real status bar. The choice is remembered across sessions.

## Formulas

Start with `=`, e.g. `=A1+B1*2`. Arithmetic operators, cell/range
references (`A1:A3`) and named functions are supported, e.g.
`=SUM(A1:A3)` or `=IF(A1>5;100;200)` — function names are in English
(`SUM`, `IF`, `MAX`, `AVG`, ...), inherited directly from the original
Sum-It calculation engine.

**Argument separator**: use a **semicolon** (`;`) between a function's
arguments, not a comma — `=IF(A1>5;100;200)`, not `=IF(A1>5,100,200)`
(a formula error). The comma remains the thousands separator in number
formatting (Format → Number), a separate setting.

**Conditional aggregation**: `=SUMIF(A1:A10;"Rome";B1:B10)` sums
B1:B10 where the matching cell in A1:A10 is "Rome"; `=COUNTIF(A1:A10;
"Rome")` counts matches instead. `=AVERAGEIF(...)` works like `SUMIF`
but averages. The third argument (the range to sum/average) is
optional — if omitted, the first range is used. The criterion can be a
number, text (case-insensitive), or a comparison with `>`, `>=`, `<`,
`<=` or `<>` followed by a number, e.g. `=SUMIF(B1:B10;">100")`.

**Spilling formulas**: `=SEQUENCE(rows;[columns];[start];[step])`
generates a block of numbers directly from a single formula, the same
way Excel's dynamic arrays work — type it into one cell and it fills
the cells below/to the right by itself (Excel-style light gray border
around the whole filled area). `=SEQUENCE(5;1)` fills 5 rows with
1,2,3,4,5; `=SEQUENCE(2;3)` fills 2 rows × 3 columns, numbering across
each row first; `=SEQUENCE(3;1;10;-2)` starts at 10 and counts down by
2 (10, 8, 6). Only works when `SEQUENCE(...)` is the *entire* content
of the cell — wrapping it in another formula (`=SUM(SEQUENCE(3;1))`)
just uses its first value, without filling anything. If the cells
SEQUENCE would fill already contain a formula of their own, it backs
off and shows only its first value instead of overwriting that
formula (a plain typed value in the way gets overwritten, unlike real
Excel).

## Files: open, save, new

- **File → New**: blank sheet. If the current document has unsaved
  changes, a confirmation dialog asks before discarding them.
- **File → Open…**: any CSV, XLS (Excel 97-2003), XLSX (Excel 2007+)
  or ODS (LibreOffice/OpenOffice Calc) file for which the matching
  translator is installed, plus the native ASCD format. The format is
  recognized from file content, not extension.
  - Formulas that reference another sheet by name are supported.
- **File → Save As…**: the output format is chosen from the file
  extension — `.xlsx` and `.ods` export to Excel 2007+ and
  OpenDocument respectively, including **live formulas** (not just
  calculated values) for same-sheet references — a formula referencing
  another sheet exports as its calculated value only, since each of
  these formats writes a single sheet per file; `.csv` exports
  calculated values only (no formula concept in that format); any
  other extension (or none) writes the native ASCD format. Legacy XLS
  (Excel 97-2003) is import-only — that translator doesn't write
  `.xls` yet.

## Cut, copy, paste

Uses Haiku's system clipboard (not a private one) — you can copy from
Atomo123 and paste into a text editor, and back. The copied content is
the cell's formula (the same text the formula bar shows), not the
already-calculated value.

## AutoFill

Select two or more cells in a row or column, then drag the small
square handle at the bottom-right corner of the selection into the
cells you want to fill (same convention as Excel/LibreOffice Calc).
Atomo123 extends the pattern it detects in the source cells:

- **Numbers with a constant step**: `1, 2, 3` continues `4, 5, 6`;
  `10, 8, 6` continues `4, 2`; a fractional step (`0.1, 0.2, 0.3`)
  keeps working despite floating-point rounding.
- **Dates/times with a constant step**: continues with the same step
  (a day, a week, or any other interval between the source cells).
- **A single selected cell**: repeats that value, same as the
  existing "Fill Down"/"Fill Right" commands.
- **Anything else** (text, mixed types, or numbers with no constant
  step): cycles through the source values in order, the same fallback
  Excel uses.

Dragging down or right grows the selection; the direction follows
whichever way you actually drag, and can change mid-drag.

## Find & replace

Edit → Find & Replace… opens a small window with a "Find:" field, a
"Replace with:" field and three buttons. Search (case-insensitive)
starts from the currently selected cell, wrapping to the beginning if
nothing else is found.

- **Find Next**: selects the next cell containing the search text.
- **Replace**: replaces all occurrences in the currently selected
  cell, then moves to the next match.
- **Replace All**: replaces every occurrence in every cell in the
  document and reports how many cells changed.

## Printing

File → Print… opens the system print dialog. The area of the sheet
containing data is printed, split across multiple pages automatically
if needed. Even without a physical printer, Haiku ships "Preview" and
"Save as PDF" print transports, usable from the same dialog to check
the result without printing.

**Known limitation**: row/column headers appear only on the first page
of a multi-page print job, not repeated on every page.

## Number formatting

Numbers in the grid follow the system's formatting preferences
(thousands separator, decimal point/comma) and update automatically
with system Locale preferences. The formula bar always shows the raw,
unformatted value.

The **Format** menu applies a display style to the selected cell:

- **General**: the default behavior above.
- **Number**: plain number.
- **Currency**: currency symbol from system preferences (e.g.
  "$1,234.50").
- **Percentage**: value × 100 with a "%" sign (0.42 shows as "42%").

Bold/italic/underline, text/background color, borders, text wrap and
alignment are applied from the **Format** menu (or the toolbar), not
from a number style.

Dates imported from XLSX display correctly automatically (no manual
formatting needed), but there's no menu command to apply a date/time
style yourself, and no control over the number of decimal places
shown.

## Comments, hyperlinks, data validation, conditional formatting

- **Insert → Cell Comment…**: attaches a note to the selected cell,
  shown with a small red triangle in the cell's corner.
- **Insert → Hyperlink…**: attaches a clickable link (opened with the
  system's default handler) to the selected cell.
- **Data → Data Validation…**: restricts what a cell accepts (a
  dropdown list or a numeric range).
- **Format → Conditional Formatting…**: applies a style to a range
  based on a live rule (re-evaluated automatically as data changes).

## Charts and pivot tables

The **Insert** menu offers two features that read a **two-column**
range (first column: text label/category; second column: numeric
value) — the range is typed manually in the dialog (e.g. `A1:B5`), not
selected by dragging on the grid (which today only supports one cell
at a time).

- **Chart…**: pick bar, line or pie, type the range and click "Draw"
  for a preview (doesn't auto-update — press "Draw" again after
  changing data). **"Insert into sheet"**, with a destination cell,
  embeds the chart in the grid for real: once inserted, it updates
  itself whenever the source range's data changes, and is saved/
  reloaded with the document (native format only — a CSV export is
  text-only and doesn't carry it).
- **Pivot Table…**: pick the source range, a destination cell and an
  aggregation (Sum, Count or Average). Rows sharing the same category
  are grouped and aggregated; the result (two columns: category,
  aggregated value) is written starting at the destination cell, which
  can't overlap the source range. Unlike the embedded chart, this is a
  one-time write (plain cells) — it does not auto-update if the source
  data changes later; re-run "Create" for a fresh result.

**Current limits**: the pivot table groups by a single category and
aggregates a single value column (not a multi-dimensional pivot); an
embedded chart has a fixed size (no resize/move after insertion, and
no way to remove one from the grid yet).

## What's not yet supported

See `ROADMAP.md` for the full, current list. In short: writing legacy
XLS (import only), advanced multi-dimensional pivot tables, array
formulas, Goal Seek/Solver, and macros/VBA.
