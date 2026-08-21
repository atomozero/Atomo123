# Roadmap

Atomo123 is a native Excel-style spreadsheet for Haiku OS. The
calculation engine and legacy XLS importer are extracted and
modernized from the historical BeOS **Sum-It** project (community fork
`OpenSumIt`); the UI is written from scratch on Interface/Layout Kit.

**Status: v0.2.0 released.** All planned phases
through "closing the gap with Excel" are done or in good shape; a
handful of large, optional features remain unplanned backlog items
(see "Not currently planned" below). Full history of individual fixes
and design decisions lives in `git log` — this file tracks
project-level status only.

## Phases

| Phase | Status | Summary |
|---|---|---|
| 1. Build the historical codebase | Done | Ported Sum-It/OpenSumIt to build on 64-bit Haiku |
| 2. Extract the calculation engine | Done | Isolated `engine/` static library, no UI dependency |
| 3. Translation Kit add-ons | Done | CSV/XLS/XLSX/ODS import; CSV/XLSX/ODS export added later (see below) |
| 4. Native Interface/Layout Kit UI | Done | Main window, grid, formula bar, in-cell editing, menus, toolbar |
| 5. Packaging & real-world compatibility | Done | HaikuDepot recipe, MIT license, verified against real user files |
| 6. Polish & advanced features | Done | Named functions, bar charts, pivot tables, Excel-style keyboard navigation |
| 7. Feature parity with historical Sum-It | Done | Multi-cell selection, fill, sort, undo/redo, named ranges, paste special, freeze panes, font/color/alignment formatting, preferences |
| 8. UI/UX quality | Done | Unsaved-changes protection, row/column resize, toolbar icons |
| 9. Multi-sheet support | Done | Workbook format (`ASCB`), sheet tab strip, cross-sheet formulas |
| 10. Cell/view preference persistence | Done | Every per-cell and per-view setting round-trips through save/reload |
| 11. Cell borders | Done | |
| 12. XLSX visual fidelity | Done | Number formats, bold/italic, alignment, merged cells, embedded images, conditional formatting |
| 13. Closing the gap with Excel | Mostly done | See below |
| Release prep (v0.1.0) | Done | Tagged on GitHub, hpkg packaging, license headers, English localization |
| Live formula export | Done | XLSX/ODS export now writes live formulas (not just calculated values); CSV stays value-only by design |
| Release prep (v0.2.0) | Done | Tagged on GitHub, translators bundled in the hpkg, doc rewrite, splash/About-panel polish (see "Current focus" below) |

### Phase 13 detail

Systematic gap analysis against Excel, ordered by implementation
difficulty. Done: missing text/statistics functions (TRIM, UPPER/
LOWER/PROPER, FIND/SEARCH, CONCAT, MEDIAN, MODE), sheet add/delete/
rename, cell comments, hyperlinks, INDEX/MATCH, more chart types (line,
pie), border/color styles, data validation, live conditional
formatting.

Not currently planned (large, self-contained efforts, no library or
existing code to build on):
- **Advanced pivot tables** — current implementation is intentionally
  minimal (one grouping level, Sum/Count/Average only)
- **Array formulas** (spill ranges) — touches the engine's evaluation
  model, not an incremental change
- **Goal Seek / Solver** — needs a new iterative numeric solver
- **Legacy XLS writing** (BIFF/OLE2) — import only today; writing BIFF8
  from scratch has no library to build on, deliberately excluded (XLSX
  already covers export to the Excel ecosystem)
- **Macros/VBA** — would need an embedded scripting engine, effectively
  its own sub-project

## Current focus

What shipped in v0.2.0, on top of the v0.1.0 baseline:
- XLSX/ODS export now writes live formulas for same-sheet references
  (cross-sheet references still export as a value only, since each
  format writes a single sheet per file)
- Fixed two real `BTranslatorRoster`/ASCD-versioning bugs uncovered
  while wiring that export path into `MainWindow::SaveToFile`
- All four Translation Kit add-ons (CSV/XLS/XLSX/ODS) now show an
  "About" panel in Haiku's Translators preferences, localized to
  English, previously blank
- Splash screen shows the build's commit hash under the version number
- All documentation rewritten from Italian to concise English
  (`README.md`, this file, `docs/*.md`)
- hpkg packaging now bundles all four translators alongside the app

Since v0.2.0 (not yet in a tagged release):
- Fixed a real redraw bug: editing a cell recalculated dependent
  formulas correctly but only repainted the cursor's rectangle, so a
  formula elsewhere on screen (e.g. a totals row) kept showing its old
  value until something else forced a repaint. Paste/sort/undo had the
  same narrow-invalidate bug; fixed once in
  `SheetView::RecalculateOwningWorkbook`
- Embedded images can now be deleted: click to select (persistent
  selection, not just during drag), Delete/Backspace removes it,
  undo/redo restores the full image (PNG data included)
- Embedded images can be dragged out of the app with the right mouse
  button (real Haiku drag-and-drop, `DragMessage`/`B_COPY_TARGET`) —
  code is in place and follows the documented Haiku convention (three
  real bugs found and fixed along the way), but end-to-end delivery to
  Tracker/another app was never confirmed working in the dev
  environment used to build this, and even an isolated test program
  outside Atomo123 showed the same unresolved drop — likely an
  environment/input limitation, not an app bug. **Shelved**: not being
  pursued further for now, kept as-is
- Dragging or resizing an embedded image with the left button past the
  edge of the visible area now auto-scrolls the sheet to keep it in
  view, same principle as `ScrollToShowSelection` for cell selection
- Each sheet in a multi-sheet workbook now keeps its own scroll
  position — scrolling one sheet no longer moves the others, which
  previously all shared a single `SheetView`'s scroll state. Session
  only, not saved to the ASCD/ASCB file format
- "New sheet" is now also in the Insert menu (was only under Data)
- Embedded charts (bar/line/pie) can now be moved with the mouse,
  same as embedded images — click and drag, undoable, auto-scrolls
  past the visible edge. No resize handle (not requested)
- The chart dialog window was too small for its own controls (fixed
  400x320px, no minimum size on the preview) — enlarged to 640x560
  and the chart preview now has an explicit minimum size
- Chart rendering quality: bar and line charts now show a Y-axis
  grid with value labels, plus the exact value above each bar/point;
  the pie chart shows each slice's percentage next to its legend
  entry
- Fixed a real bug in bar/line charts: a negative value produced a
  bar/point outside the drawable area instead of dropping below a
  zero line. Bar/line layout now scales to the series' actual value
  range (including negative values), with an explicit zero baseline
- Charts can now have an optional title, set in the chart dialog and
  persisted with the file (old files without one just show none)
- Bar/line charts now support multiple series: a data range with more
  than two columns (labels + one column per series) draws grouped bars
  or multiple lines, one color per series with a legend. Pie charts
  stay single-series by design. The existing two-column/single-series
  path is unchanged
- Fixed a follow-up bug from the negative-value fix above: a negative
  bar/point's value label could land exactly on top of the category
  label below it. The category label row now stays fixed in place and
  the chart floor makes room above it instead
- Multi-series charts recognize an optional header row: selecting a
  range whose first row has text in a series column names that series
  from it (Excel-style), instead of always "Serie 1", "Serie 2", ...
- Opening the chart dialog with more than one cell already selected
  now pre-fills the data range from that selection and draws the
  preview immediately, instead of always showing the fixed default
- Multi-series bar/line charts get a per-series checkbox in the chart
  dialog to show/hide that series' numeric value labels (previously
  always hidden for multi-series, to avoid clutter). Preview-only —
  not persisted on a chart already embedded in the sheet
- Fixed a real crash caused by a build issue, not application logic:
  the Makefile never tracked header dependencies, so editing a shared
  header like `Chart.h` didn't always trigger recompilation of every
  `.cpp` that includes it, producing a binary linked from object files
  built against two different versions of the same struct. Added
  `-MMD -MP` header dependency tracking so `make` now rebuilds
  correctly
- The per-series checkbox row (see above) now sits in a titled box
  ("Mostra valori per serie") with a one-line hint explaining that
  unchecking a series hides only its numbers, not the series itself —
  it had no label or explanation before, which was confusing
- Embedded charts can now be resized (bottom-right handle), same as
  embedded images — previously move-only
- "Salva con nome" in the File menu is now a submenu with one entry
  per output format (native/.csv/.xlsx/.ods) — the format could only
  be chosen before by typing the extension by hand, with no indication
  that was even possible
- Embedded charts can now be deleted: click to select (persistent, a
  blue outline like embedded images), Delete/Backspace removes it,
  full undo/redo. Selecting a chart deselects a selected image and
  vice versa, so Delete is never ambiguous
- Fixed a real regression: any multi-sheet XLSX file failed to open
  ("i dati risultanti non sono validi"). The XLSX translator keeps its
  own copy of the ASCD writer and hadn't been updated for the new
  chart-title section added alongside optional chart titles above —
  the reader's EOF-tolerant skip for that section only works at the
  true end of the stream, so in a multi-sheet workbook every non-final
  sheet's block desynchronized the read that followed it
- Fixed a real bug affecting any XLSX file exported by LibreOffice
  Calc: Calc writes boolean attributes spelled out as "true"/"false"
  (valid per the XLSX spec, but different from Excel's usual "1"/"0"),
  and the translator's parsing only recognized the numeric form for
  wrapText, showGridLines, row customHeight/hidden, and table
  showRowStripes. A cell with wrapText explicitly turned off was
  misread as wrapped, showing a long single-line text broken into a
  narrow, unreadably tall column instead of overflowing across the
  empty cells to its right
- Fixed a real formula-parsing bug: a sheet name starting with a digit
  (e.g. "1P_Mandata_studio", legal in XLSX without quotes) was
  tokenized as a standalone number followed by disconnected leftover
  text, so any formula referencing that sheet failed to parse and was
  left as raw, uncalculated text
- Added Excel's LOG10() function (alias for the existing single-argument
  LOG, which already computes base-10 log): the missing function was
  failing to parse entirely, on the same real file above, cascading
  NaN through every formula that depended on it
- Delete/Backspace on a cell now clears only its content (value,
  formula), matching Excel/LibreOffice Calc — it previously wiped the
  cell's formatting (color, borders, etc.) too, since it shared the
  same code path as deleting the cell record entirely
- Added a plain "Salva" (Save) command, separate from "Salva con
  nome" (Save As): rewrites the already-open/already-saved file
  directly, no dialog. Previously only Save As existed, so every save
  — even of an already-named file — showed a file picker. A never-saved
  document still falls back to the Save As dialog
- Added automatic saving: Preferences > "Salvataggio automatico",
  enabled by default every 5 minutes (both configurable). Starts only
  after the first manual save, and always writes a "<name>.bak"
  backup file next to the document — never the original file itself,
  same principle as AutoCAD's backup. The backup's format follows the
  live document's format (an open .xlsx gets an .xlsx.bak written by
  the XLSX translator)
- Embedded charts (bar/line/pie) now export to XLSX: previously they
  simply vanished when saving as .xlsx (only cell data was ever
  written). Now real DrawingML chart parts are generated — cell
  references with cached values, correct chart type, title, single-
  or multi-series grouping matching the same rules the app itself
  uses to draw them

Next: no specific item queued — see "Not currently planned" above for
the larger backlog.

## Related work

A separate concept proposal by Jürgen Ihlau ("Haiku Office UI
Scaffold", August 2026) explores a shared project generator and UI
conventions across independently-developed native Haiku office apps —
this project and his own **LetterPro** are cited as examples. Not
adopted; kept here as a pointer for future discussion.

## Documentation

Each closed phase's design decisions and architecture are documented in
`docs/ENGINE_API.md`, `docs/TRANSLATORS.md` and `docs/UI_ARCHITECTURE.md`.
User-facing features are documented in `docs/USER_GUIDE.md`.
