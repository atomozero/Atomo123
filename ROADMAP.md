# Roadmap

Atomo123 is a native Excel-style spreadsheet for Haiku OS. The
calculation engine and legacy XLS importer are extracted and
modernized from the historical BeOS **Sum-It** project (community fork
`OpenSumIt`); the UI is written from scratch on Interface/Layout Kit.

**Status: v0.2.6 released.** All planned phases
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
| Release prep (v0.2.1) | Done | Intermediate beta-tester release, tagged on GitHub: chart import from XLSX, repeated print headers |
| Release prep (v0.2.5) | Done | Tagged on GitHub: five function batches (30 functions), print settings + preview, pivot table multi-level grouping, translator ambiguous-text parity, AutoFill (see "Current focus" below) |
| Release prep (v0.2.6) | Done | Tagged on GitHub: critical multi-sheet XLSX corruption fix, background file loading with a footer progress bar, ~7x faster large-file opening (see "Current focus" below) |

### Phase 13 detail

Systematic gap analysis against Excel, ordered by implementation
difficulty. Done: missing text/statistics functions (TRIM, UPPER/
LOWER/PROPER, FIND/SEARCH, CONCAT, MEDIAN, MODE), sheet add/delete/
rename, cell comments, hyperlinks, INDEX/MATCH, more chart types (line,
pie), border/color styles, data validation, live conditional
formatting.

Not currently planned (large, self-contained efforts, no library or
existing code to build on):
- **Full pivot tables** — multi-level grouping and Sum/Count/Average/
  Min/Max are done (see below); still no Excel-style "Columns" field
  (a second pivot axis) or multiple simultaneous measures, both would
  need a real 2D output layout, not an incremental change to the
  current flat-list one
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

What shipped in v0.2.1, on top of v0.2.0 (intermediate beta-tester release):
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
- Fixed a real crash reported by the user ("Looper must be locked"):
  opening the Chart dialog with a multi-cell selection already active
  pre-fills the data range by calling into the dialog's own BWindow
  from MainWindow's thread without locking it first — same bug class
  already fixed once for the Color/Name windows, missed here because
  this pre-fill feature was added afterward
- Embedded charts now import FROM XLSX too, not just export: bar,
  line and pie charts anchored in a real Excel file (or one exported
  by this app) are recognized and redrawn. A chart of a type this app
  doesn't draw (area, scatter, radar, ...) — or one whose data isn't a
  single contiguous category+value block — shows a dialog naming it
  instead of silently vanishing or breaking the rest of the file.
  Found and fixed two real bugs along the way: a placeholder-zero
  `<a:ext>` nested inside a chart's own `<xdr:graphicFrame>` was being
  confused with the real anchor-level size, and the image-import loop
  didn't skip chart anchors, occasionally misreading a chart's XML as
  a PNG
- Printing repeats the row/column header band on every page, not just
  the first: a multi-page sheet's later pages used to be unreadable
  (no way to tell which row/column a cell belonged to). The page-tiling
  math now lives in its own `PrintLayout.cpp`, testable without a real
  configured printer — first item from the "v3.0 consolidation" print
  backlog (see below)

What shipped in v0.2.5, on top of v0.2.1:
- Added NOT/XOR/SWITCH/IFNA/ISBLANK/ISERROR/ISNA/ISFORMULA — first
  batch of functions missing versus Excel (v3.0 consolidation, more
  batches to follow: text, date, math/stats, lookup). Found a real
  engine limitation while writing ISFORMULA (also affects the existing
  ROW()/COLUMN(), never noticed before): a bare single-cell reference
  like `B1` (no `:`) is always dereferenced to its value by the parser
  before any function sees it, so `ISFORMULA(B1)` can't work — only a
  genuine multi-cell range like `B1:B2` preserves the reference.
  Documented in the function's own comment rather than fixed (would
  need the parser to know which arguments of which functions want a
  reference instead of a value, a larger change)
- Added SUBSTITUTE/REPLACE/REPT/TEXTJOIN/VALUE/EXACT — second batch of
  functions missing versus Excel (v3.0 consolidation, more batches to
  follow: date, math/stats, lookup). SUBSTITUTE is 10 characters, one
  over the `'Func'` resource's name limit — registered internally as
  "SUBST" with the same alias trick as CEILING.MATH/CONCATENATE
- Added TODAY/NETWORKDAYS/WORKDAY/EDATE/EOMONTH/DATEDIF — third batch
  (v3.0 consolidation, more to follow: math/stats, lookup). Found a
  real bug while testing DATEDIF's "YD" unit: computing a day count by
  subtracting two raw timestamps and dividing by 86400 is off by an
  hour whenever the range crosses a daylight-saving transition — fixed
  with a pure calendar-based day-difference calculation that never
  touches `time_t` at all
- Added SUMPRODUCT/AVERAGEIFS/MAXIFS/MINIFS/RANK/LARGE/SMALL/SUBTOTAL
  — fourth batch (v3.0 consolidation, one more to follow: lookup).
  SUMPRODUCT/AVERAGEIFS are 10 characters, one over the `'Func'`
  resource's name limit — registered internally as "SUMPROD"/"AVGIFS"
  with the same alias trick as CEILING.MATH/CONCATENATE/SUBSTITUTE.
  SUBTOTAL only supports the six most common aggregations (AVERAGE/
  COUNT/COUNTA/MAX/MIN/SUM); it doesn't exclude nested SUBTOTAL calls
  or distinguish filter-hidden rows from visible ones
- Added INDIRECT/ADDRESS/XMATCH — fifth and last function batch. All
  20 functions across the five batches (NOT/XOR/SWITCH/IFNA/ISBLANK/
  ISERROR/ISNA/ISFORMULA/SUBSTITUTE/REPLACE/REPT/TEXTJOIN/VALUE/EXACT/
  TODAY/NETWORKDAYS/WORKDAY/EDATE/EOMONTH/DATEDIF/SUMPRODUCT/
  AVERAGEIFS/MAXIFS/MINIFS/RANK/LARGE/SMALL/SUBTOTAL/INDIRECT/ADDRESS/
  XMATCH — 30 in total) are now shipped, closing the "functions still
  missing versus Excel" item of the v3.0 backlog. Found a real bug
  writing INDIRECT's test: a single-cell reference returned a
  one-cell range instead of its value, unlike how the engine's own
  bytecode interpreter treats the same case for a reference written
  directly in a formula
- Added print settings, closing the rest of the v3.0 print backlog:
  a "Imposta pagina" dialog for margins (cm, converted to device
  pixels via the chosen printer's real resolution) and scale (a fixed
  percentage, or "fit" to a page's width/height/both, Excel-style —
  never enlarges content that already fits), plus "Imposta area di
  stampa"/"Cancella area di stampa" in the File menu to print only the
  active sheet's current selection instead of every cell with content.
  Orientation/paper size stay in the existing system print dialog
  (`BPrintJob::ConfigJob`), not duplicated here. The fit-scale math
  lives in `PrintLayout.cpp` next to the existing page-tiling code,
  same "testable without a real printer" principle
- Added a live print preview to "Imposta pagina": a real render of the
  page, with page-by-page navigation (capped at 20 pages) that updates
  whenever a margin/scale control changes. A "Stampa…" button in the
  dialog applies pending settings and prints in one step. The first
  implementation captured `SheetView`'s real drawing into a `BPicture`
  (`BeginPicture`/`Draw`/`EndPicture`, the same primitive `BPrintJob`
  itself uses) — reproducible crash in Haiku's `app_server`
  (`ServerWindow::_DispatchPictureMessage`), confirmed via a debug
  report generated live in this environment. Replaced with a
  self-contained renderer that draws each cell's background/text
  directly from `CContainer` data (no borders, merged cells, or text
  wrap — a deliberate simplification, not pixel parity with the real
  print output)
- Pivot tables now support multi-level grouping (more than one
  category column — the last column stays the value; grouping happens
  on the combination of all category columns) and two new
  aggregations, Min/Max, alongside Sum/Count/Average. Still a flat
  output list, not a real 2D pivot layout (see "Not currently planned"
  below)
- The ambiguous-text-becomes-a-NaN-formula bug (fixed for XLSX/ASCD
  earlier, commit 670425b) is now fixed for CSV, ODS and XLS too — a
  text value like "P-EL-a" (several undefined names joined by "-",
  syntactically a valid expression) no longer risks being silently
  reparsed into a live formula on import or through the native ASCD
  round-trip. Two distinct causes, both real: CSV/ODS/XLS's own
  internal ASCD bridge only ever *wrote* the older 1-byte-shorter
  format (no per-cell formula/literal marker) even though the reader
  side already understood the newer one; ODS's real XML import
  additionally ran every formula-less cell value through the same
  general-purpose expression parser XLSX used to. XLS's legacy BIFF
  importer never had this problem (it already writes typed values
  directly) — only its ASCD bridge needed the fix
- Added Excel-style AutoFill: a small handle at the bottom-right corner
  of the current selection, drag it to extend a detected series into
  the cells you drag over (Excel/LibreOffice Calc convention).
  Recognizes a constant-step numeric progression (1,2,3 → 4,5,6; also
  negative/fractional steps), a constant-step date/time progression,
  and otherwise cycles the selected values (or repeats a single value)
  the same way Excel falls back for non-arithmetic/mixed-type
  selections. Direction (horizontal/vertical) follows whichever axis
  the drag moves further along, and can change mid-drag. Found and
  fixed a real latent double-free in the engine's `Value` class along
  the way: it had a custom destructor and assignment operator but no
  explicit copy constructor, so the compiler-generated one shallow-
  copied a text cell's heap pointer — invisible until this feature
  became the first code in the project to put `Value` in a
  `std::vector` (which copies elements around, e.g. on reallocation)
- Merging/splitting cells is now undoable: previously a documented,
  deliberate gap (`fMergedRanges` is per-sheet, not per-cell, and
  didn't fit the dense per-cell `UndoSnapshot` format). Fixed the same
  way Data Validation/Conditional Formatting were: capture the whole
  merged-range list before the mutation, restore it whole on Undo/Redo
- File-type icons in Tracker: `.ods`/`.csv`/`.ascd` (native) files now
  show a distinct icon instead of the generic placeholder, registered
  on the specific MIME type via `BMimeType::SetIcon` (never overwriting
  an icon another app or the user already set). `.xlsx`/`.xlsm`/`.xls`
  share a generic file icon: the authorized icon source (the official
  Haiku FileTypes HVIF set) has no dedicated "Excel" icon, since it's a
  proprietary Microsoft format — a real gap, not a silent omission
- Array formulas, v1: `SEQUENCE(rows, [columns], [start], [step])`
  spills its result into a block of cells, Excel-style — the first
  (and so far only) "spill" function. Deliberately scoped down:
  spilling only happens when the call is the ENTIRE formula of its
  cell (`=SEQUENCE(3,1)`); nested inside another expression (`=SUM(
  SEQUENCE(3,1))`) it behaves as a plain scalar (its `start` value)
  with no side effect on neighboring cells — detected by walking the
  compiled bytecode directly (mirroring `CFormula::AddToken`'s
  per-opcode word sizes) rather than decompiling to text, and for a
  real reason: `CFormula::UnMangle` calls `ftoa()`, which calls
  `Font().StringWidth()` — that blocks forever in any headless context
  (no live `BApplication`/app_server connection), including every
  automated test in this project. This was a real, if latent, bug
  simply never triggered before because nothing else in the engine or
  its test suite had ever called `GetCellFormula`'s text form on a
  formula containing a numeric literal outside of the full GUI app.
  Collision detection only blocks on another cell's own formula, not
  on a plain typed value (Excel blocks on any existing content) — a
  deliberate simplification that makes reopening a saved file
  self-healing without needing new ASCD persistence: the spilled
  cells' values already round-trip as plain literals, and the first
  recalculation after loading silently re-derives the same values.
  `UNIQUE` (the other commonly-requested dynamic array function) is
  not implemented yet — same spill mechanism, future work. Also fixed
  a real, unrelated build bug found along the way: the engine's own
  Makefile never tracked header dependencies (`-MMD -MP`, matching a
  fix already applied to the UI's Makefile once before) — editing
  `Container.h` could silently leave stale object files built against
  a different, incompatible layout of `CContainer` linked together
- Print area, margins and scale ("Imposta pagina") are now saved in
  the file, per sheet — previously session-only (explicitly documented
  as such), lost on reopening. Margins/scale used to be a single
  GLOBAL app preference shared by every open document; on explicit
  request this became per-sheet instead, matching how the print area
  already worked and how Excel itself scopes page setup. A sheet
  without its own saved settings still falls back to the app-wide
  default, unchanged from before. New `AscdPrintSettings` struct and
  two new EOF-tolerant trailing sections in the ASCD/ASCB format
  (mirroring the existing AutoFilter section's exact shape), so files
  written before this change stay fully readable
- Fixed a real bug reported by the user: long chart labels (category
  names under a bar/point, legend entries) ran past the edge of the
  chart instead of wrapping — `BView::DrawString` alone never
  truncates or wraps. Added a shared word-wrap helper (up to 2 lines,
  the last one ellipsis-truncated if it still doesn't fit) used by
  every chart type's category labels and legend, plus a truncation
  guard on the chart title. Category labels are now centered under
  their bar/point instead of left-aligned, which reads better wrapped
  across two lines
- Fixed a real bug found while building the `ui/examples/
  generate_cda_report.cpp` function catalog: `MATCH`/`XMATCH`/
  `XLOOKUP` signaled "not found" with `#REF!` instead of `#N/A`, so
  `IFNA(MATCH(...), fallback)` — a very common real-world pattern —
  silently never caught it (`IFNA` only checks the specific `#N/A` tag
  inside the error, not a generic NaN). Fixed at the source in all
  three functions; `VLOOKUP`/`HLOOKUP` (older, Sum-It-heritage
  functions) were deliberately left as-is, out of scope
- Fixed a serious, real data-integrity bug reported by the user with a
  screenshot: plain text typed into a cell that happened to match a
  function name — no leading `=`, no parentheses, e.g. just typing
  `TODAY` as a column label — was silently CALCULATED as if that
  formula had been entered (`TODAY` became today's date, `CONCAT`
  became an empty string, `IF`/`XOR`/`AND`/`OR`/`SUM` and every other
  variable-argument function became a miscalculated value), instead of
  staying as the literal text the user typed. This affected the real
  app directly (`SheetView::CommitEditing`, the normal cell-typing
  path), not just file generation. Root cause: two compounding bugs in
  the parser — a `short` field truncating the "variable argument
  count" resource value (`65535`) into `-1`, which happened to collide
  with the unrelated "unknown function" sentinel, and a bare-word
  fallback that implicitly called ANY function whose (corrupted or
  genuine) expected-arg-count was `0` or `-1` with zero arguments,
  with no exception ever thrown to trigger the existing "fall back to
  literal text" path. Fixed by never implicitly calling a bare
  function name without explicit `()` — except `TRUE`/`FALSE`, kept as
  a deliberate exception since real Excel treats them as boolean
  literal keywords usable without parentheses, and existing formulas
  in this project already relied on that specific case
- Fixed a real performance bug reported by the user (opening a file
  felt slow, one CPU core busy): `MainWindow::OpenFile` recalculates
  the whole workbook once after linking all sheets together
  (`RecalculateWorkbook`, needed so cross-sheet formulas resolve), but
  `LoadASCD`/`LoadASCDBook` were ALSO recalculating each sheet on its
  own right after reading it — work that's provably wasted for this
  call path (cross-sheet references can't resolve yet, no
  `ISheetResolver` attached at that point) and, worse, never actually
  converges: the per-sheet pass hit its full 50-iteration safety cap
  every time instead of settling early. Measured on a real 4-sheet,
  700-row workbook: opening it dropped from ~2.36s to ~1.52s (35%)
  after adding an opt-in `skipInitialRecalc` parameter that
  `MainWindow::OpenFile`'s three load paths now pass — every other
  caller (tests included) keeps the old default behavior unchanged.
  The remaining ~1.5s is the engine's brute-force whole-sheet
  recalculation itself (no dependency graph, by design) doing real
  work across ~11,000+ cells and several full-column `SUMIF`/
  `AVERAGEIFS`-style formulas — a much larger, riskier change than a
  quick fix, not attempted here
- Fixed a critical data-corruption bug found while investigating a
  real user file that wouldn't open (a 13-sheet, ~11MB XLSX):
  `translators/xlsx/XlsxTranslator.cpp`'s own hand-duplicated ASCD
  writer (`WriteASCD`, never linked against `ui/src/AscdIO.cpp` by
  design) was never updated with the `printArea`/`printSettings`
  trailing sections that `AscdIO.cpp`'s `SaveASCD`/`LoadASCD` gained
  in an earlier phase — a recurrence of the exact bug class fixed
  once before (see the chart-title trailing-section fix earlier in
  this list). For a single-sheet file this is invisible (EOF-tolerant
  reading treats the missing section as "absent, we're at the end of
  the file"), but `WriteASCDBook` concatenates one ASCD block per
  sheet on the same stream: the missing 54 bytes for every sheet
  except the last silently swallow the start of the NEXT sheet's own
  header, permanently desyncing every sheet after the first. This
  broke opening ANY multi-sheet XLSX with more than one sheet. Fixed
  by adding the two missing sections (always "absent", matching this
  translator's current scope — it doesn't parse print area/settings
  from XLSX yet) to both the writer and its matching reader. Verified
  with a new end-to-end regression test
  (`ui/tests/test_translator_multisheet_import.cpp`) that goes through
  the real installed translator + real engine reader together (the
  only way to catch a format mismatch between the two independently
  duplicated implementations) — confirmed to fail without the fix and
  pass with it. The other three translators (CSV/XLS/ODS) don't have
  a `WriteASCDBook` equivalent (no multi-sheet concatenation), so
  they can't hit this bug class
- Added a progress window for opening large files (Fase 31, measured
  on the same 13-sheet real user file above: ~3 minutes total —
  translate, load, recalculate — with the window completely frozen
  and unresponsive to even scripting messages the whole time, since
  everything ran synchronously on the window's own thread). The real
  interactive open paths (menu, drag & drop, "Apri recenti") now go
  through a new `MainWindow::OpenFileAsync`, which does the same work
  on a separate thread and shows a `ProgressWindow` (phase text +
  percentage, updated live during the recalculation phase with a
  "sheet X of N, pass Y" detail line) while the window keeps
  responding normally — verified live: `hey` round-trips in ~100ms
  while the file is still loading, versus never returning before this
  fix. `MainWindow::OpenFile` (synchronous) is kept, unchanged, as a
  separate method: every existing test calls it while holding the
  window's lock and checks the result immediately after — a real
  background thread delivering its result through the same lock would
  deadlock against that pattern, so the two paths intentionally
  duplicate the load logic instead of sharing it (same reasoning as
  the translator/engine duplication elsewhere in this project). New
  test `ui/tests/test_open_async.cpp` covers the async path
  specifically, including that a cross-sheet formula resolves
  correctly even though the recalculation happens against a
  temporary, worker-thread-local `ISheetResolver` before the sheets
  are handed to the window
- Optimized `RecalculateWorkbook` with "dirty cell" tracking (Fase 32,
  same 13-sheet real file as above): it re-scanned EVERY cell with
  content — including purely literal ones, which can never change from
  a recalculation, only from an explicit user edit — on every one of
  up to 50 convergence passes. One sheet in the real file is a
  ~500KB/8000-row lookup table with almost no formulas; re-visiting
  every one of those literal rows on every pass was close to pure
  waste. Now `CollectFormulaCells` builds the list of formula-bearing
  cells ONCE, before any pass, and every pass only re-evaluates that
  list. Safe with respect to array formulas (`SEQUENCE` etc.): a
  spilled neighbor cell is written as a plain VALUE, never a formula
  of its own, so the set of formula cells in a document never changes
  during these passes, only their values — verified with a dedicated
  new test, `ui/tests/test_recalc_dirty_cells.cpp`, combining exactly
  that (an array-formula spill, a cross-sheet reference, and hundreds
  of literal cells) on a multi-sheet workbook. Measured on the same
  real file: `RecalculateWorkbook` dropped from 90.9s to 2.08s (about
  44x). `RecalculateAll` (the single-document path used after every
  interactive edit, not just at file-open time) is deliberately left
  untouched — out of scope for this fix, and a much more
  frequently-exercised code path where any subtle behavior change
  would be far more disruptive to get wrong
- Moved the file-opening progress indicator into the main window's own
  footer (Fase 33, requested live by the user after trying the
  separate `ProgressWindow` from the previous phase): removed
  `ProgressWindow` entirely, replaced by `fFooterProgressLabel`
  (phase text) and a new hand-drawn `FooterProgressBar` view sharing
  the same footer row as `fCellMode`/`fSelectionStats` (hidden while
  loading, restored afterward). Two follow-up rounds of live feedback
  from the user while watching it run on the real 13-sheet file:
  (1) "it's ugly that the bar stays still" during `Translate()` — a
  single opaque call with no real intermediate progress — fixed with
  a periodic 150ms "pulse" (`BMessageRunner`) that eases the bar
  toward a capped ceiling between real updates, so it's always
  visibly moving even with zero real progress data; (2) "bar and text
  on the same line, full width" — `BStatusBar` always reserves a
  separate text line above its bar even with no label, which can't be
  made to share a line with adjacent text, so it was replaced with a
  minimal custom-drawn `FooterProgressBar` (single line, same height
  as the surrounding footer text) placed right next to the phase
  label, both getting a large layout weight so they fill the row once
  `fCellMode`/`fSelectionStats` are hidden. Verified live via
  screenshots at each iteration, plus new coverage in
  `test_open_async.cpp` (`IsFooterProgressVisible()`, a new public
  getter, checked both right after `OpenFileAsync()` starts and after
  it finishes)
- Found and fixed the real bottleneck behind the remaining ~86s of
  `Translate()`+`LoadASCDBook` on the same 13-sheet real file (Fase
  34): profiling showed a strikingly uniform ~50-90 microseconds per
  imported cell regardless of sheet content (plain text lookup tables
  and formula-heavy sheets alike), which turned out to have nothing to
  do with algorithms — **the entire project (engine, UI, all four
  translators) had never been compiled with any optimization flag,
  only `-g`**. Adding `-O2` everywhere (kept alongside `-g`, so crash
  reports stay readable) measured directly on this file: `Translate()`
  53.9s → 17.1s, `LoadASCDBook` 30.0s → 9.2s, `RecalculateWorkbook`
  2.0s → 0.26s — roughly 3x across the board on top of the earlier
  dirty-cell win, for a total pipeline of ~180s → ~27s since the start
  of this investigation. Also fixed, alongside this: `CContainer::
  NewCell` (the function both `Translate()` and `LoadASCD` funnel every
  single imported cell through) did two full tree descents per call
  (`find()` then a second implicit lookup inside `fCellData[loc] = `)
  instead of one — rewritten with `lower_bound()` once, reused both to
  check existence and as the insertion hint. New test `engine/tests/
  newcell_test.cpp` pins the two behaviors this rewrite must never
  break: a brand-new cell inserts correctly, and overwriting an
  existing cell's value preserves its style (a real bug fixed once
  before, the most concrete risk of touching this function). Also
  fixed along the way: `-O2` surfaced real `-Wmaybe-uninitialized`
  warnings on every field of `XlsxTranslator.cpp`'s `ResolvedStyle`
  struct (no default member initializers) — harmless in practice
  (every read is already guarded by its own `has*` flag) but free to
  fix properly with explicit defaults

What shipped since v0.2.6, not yet in a tagged release:
- Sheet tabs now use `BControlLook::DrawActiveTab`/`DrawInactiveTab`
  (the same drawing primitives `BTabView` itself uses internally)
  instead of hand-drawn flat-color rectangles — real user feedback (a
  couple of users asked about it), evaluated `BTabView`/`BTab` first
  and ruled it out: it manages one content view per tab, but every
  sheet here shares the same `SheetView` instance, and it has no
  built-in overflow scrolling for a workbook with dozens of sheets
  (both already solved by the existing custom `SheetTabView`, kept
  as-is). `BControlLook` gets the native theme-aware chrome (dark
  mode, accent colors, consistent bevels) without touching any of
  that architecture — only the fill/stroke calls inside `Draw()`
  changed. One deliberate design change along the way: a sheet's tab
  color (`<sheetPr><tabColor>`) used to fill a non-active tab's whole
  area and only show as a thin accent bar on the active one —
  `BControlLook` derives its shading from a single theme-aware "base"
  color, not an arbitrary per-tab RGB, so a full-tab fill isn't
  possible anymore. Both active and inactive colored tabs now show
  the same thin accent bar instead, closer to modern Excel's own tab
  color treatment. The scroll arrows also moved to `BControlLook::
  DrawArrowShape`, from hand-drawn triangles. `tests/
  test_sheet_tabs.cpp`'s pixel-level checks updated to match (verifies
  the accent bar's presence/color, not an exact background shade that
  is now theme-dependent by design)
- Fixed a real follow-up bug in the BControlLook tabs above, caught by
  the user from a screenshot right after: the active tab looked
  "flipped", boxed in on all four sides like the inactive ones instead
  of visually merging with the sheet above it. Two things were wrong,
  both needed together — changing only the first has no visible effect
  on its own: the `side` parameter passed to `DrawActiveTab`/
  `DrawInactiveTab` was `B_TOP_BORDER`, correct for a typical
  top-anchored `BTabView` (content below the strip) but backwards for
  this strip, which sits at the *bottom* of the window with the sheet
  above and the footer below — fixed to `B_BOTTOM_BORDER`. That alone
  changed nothing visible, because the `borders` bitmask was always
  `B_ALL_BORDERS` for both tab states, never omitting the one border
  where a folder-tab is supposed to fuse with its content — fixed by
  omitting the active tab's top border specifically (`B_ALL_BORDERS &
  ~B_TOP_BORDER`), so it now blends into the sheet above it the way
  Excel's own active tab does, while inactive tabs keep all four
  borders as distinct boxes

What shipped in v0.2.6, on top of v0.2.5:
- Fixed a critical bug: any XLSX with more than one sheet failed to
  open with "i dati risultanti non sono validi", because the XLSX
  translator's own duplicated ASCD writer had never been updated with
  the printArea/printSettings trailing sections the engine's real
  reader expects, silently desyncing every sheet after the first
- Opening a file no longer freezes the window: `MainWindow::
  OpenFileAsync` moves translation/loading/recalculation to a
  background thread, with a live progress bar and phase text built
  into the main window's own footer (not a separate window) — full
  width, single line, with a pulsing animation so it never looks
  stuck even during the one long opaque translation step that has no
  real intermediate progress to report
- Recalculating a freshly-opened multi-sheet workbook is now ~44x
  faster by only re-visiting cells that actually contain a formula
  instead of every cell with any content, up to 50 times per open
- The entire project (engine, UI, all four translators) now builds
  with `-O2` — it never had any optimization flag before, only `-g`.
  Combined with the fixes above, opening a real 13-sheet, ~11MB XLSX
  file dropped from about 3 minutes to about 25-30 seconds
- Also fixed along the way: a redundant double tree-lookup in the
  engine's hot cell-insertion path (`CContainer::NewCell`), and a set
  of uninitialized struct fields in the XLSX translator's style
  resolver that `-O2`'s stricter warnings surfaced
- Added a "path to full Excel parity" section to this roadmap,
  ordered by priority tier (data-safety issues and small-effort/
  high-value items first, foundational architecture work like a real
  calc-engine dependency graph given its own tier rather than being
  buried at the bottom by raw size) rather than just grouped by
  feature category

What shipped in v0.2.7, on top of v0.2.6 (in progress):
- Fixed a real data-loss bug: opening an XLSM (macro-enabled workbook)
  and resaving it used to silently drop the entire VBA project — the
  XLSX translator never read `xl/vbaProject.bin` on import, and
  `MainWindow::SaveToFile`'s extension dispatch didn't even recognize
  `.xlsm` (it fell through to the native ASCB writer, which would have
  written raw native-format bytes under a `.xlsm` name). Fixed blind
  (bytes stored and carried through, never parsed or executed): the
  macro project is now read on import, threaded through the ASCD/ASCB
  format as a new optional trailing section (workbook-level, attached
  to the first sheet — same "always write, EOF-tolerant read" pattern
  as every other optional section in this format), and written back
  correctly (including the macro-enabled content-type override Excel
  itself checks, `application/vnd.ms-excel.sheet.macroEnabled.main+xml`,
  and the `xl/vbaProject.bin` relationship) whenever the user saves
  with a `.xlsm` extension specifically. Saving the same document as
  plain `.xlsx` still strips the macros on purpose, matching Excel's
  own behavior when it resaves a macro workbook without them enabled

## Next: v3.0 "Consolidation" and v4.0 "Scripting"

**v3.0 is functionally complete** as of the array formulas item above
(a deliberately-scoped v1 — see "Current focus"; `UNIQUE` and spilling
inside a nested expression remain explicit future work, not blocking
gaps). Every other v3.0 backlog item is also done: the "functions
still missing versus Excel" item (30 functions across five batches),
live formula export (XLSX and ODS symmetrically), the print settings
backlog (margins/scale/print area), the ambiguous-text translator
parity gap, and file-type icons from the HVIF store (www.hvif-store.art
— full per-format distinction where the source icon set actually has
one; .xlsx/.xlsm/.xls share a generic icon by necessity, documented
gap). XLS has no export path at all, deliberately, not a gap.

**v4.0** — scripting: expose the app to Haiku's native BHandler/
BMessage scripting protocol, with macro execution provided by an
existing embeddable VBA-compatible library if a suitable one is found
(a research spike, not yet started — no known standalone embeddable
VBA engine has been confirmed to exist; LibreOffice's Basic is the
only mature open implementation and is deeply coupled to its own UNO
API, not extractable as-is) — falls back to a self-written VBA-subset
interpreter if the spike finds nothing usable.

## Path to full Excel parity (beyond v4.0)

A systematic look at what's still missing for Atomo123 to be a
drop-in Excel replacement for most real-world files, beyond the
"Not currently planned" items already called out above. Ordered by
**priority tier**, not just by category or raw implementation effort
— see "How this ordering was decided" below for the reasoning. None
of this is scheduled yet, it's a reference list for future planning
sessions.

### How this ordering was decided

A naive "easiest first" ordering breaks down in three places, so this
list deliberately deviates from pure effort-sorting:

- **Data-loss risk beats missing features.** An XLSM round-trip that
  silently destroys the user's macros on save isn't a "gap" the same
  way a missing chart type is — it's the same class of bug as the
  print-area/multi-sheet corruption bugs already fixed in this
  project, and gets the same urgency, ahead of anything merely
  *absent*.
- **Data model already done ≠ low value.** A field like
  `CellStyle::fLocked` sitting unused because nothing enforces it in
  the UI is a small amount of remaining work for a feature users
  explicitly expect ("protect sheet"), so it's not ranked by how hard
  *building* it would be from scratch.
- **Foundational work earns its place ahead of its apparent size.**
  The calc engine's missing dependency graph is the single largest
  item on this whole list, but it's ranked in Tier 3 (not last)
  because it's the only fix that touches *every* interactive edit on
  a large sheet, not just file-open — it quietly undercuts the value
  of shipping more features on top of the current brute-force
  recalculation before it's addressed.

### Tier 1 — do first: small effort, hits most users, low risk

- **Sheet/workbook protection (enforce cell locking).**
  `CellStyle::fLocked` already exists as a field and already
  round-trips through every file format — nothing in the UI currently
  *reads* it. A "Proteggi foglio" toggle plus a locked-cell edit guard
  is mostly wiring, not new design
- ~~**XLSM round-trip preservation.**~~ Shipped in v0.2.7 — see above
- **More financial functions** (`NPV`, `IRR`, `PMT`, `FV`, `PV`,
  `RATE`). Same shape as the five function batches already shipped in
  v3.0 — register, implement, test, no engine changes needed. High
  real-world hit rate (budgeting/loan sheets) for low effort
- **Conditional formatting: icon sets, color scales, data bars.**
  Only two rule types exist today (`eCondCellIsEqual`,
  `eCondDuplicateValues`); these three are Excel's *other* built-in
  families and arguably more commonly used in real files than either
  of the two already done. The live-evaluation framework already
  exists, this is new rule types plus new per-cell rendering, not a
  new subsystem

### Tier 2 — do next: moderate effort, real but narrower value

- **Dynamic arrays beyond `SEQUENCE`**: `UNIQUE`, `SORT`/`SORTBY`,
  `FILTER`. The spill mechanism is already built (see the array
  formulas v1 entry above) — this is more functions that produce a
  range instead of a scalar, not a new mechanism. Spilling from
  inside a nested expression stays deferred either way
- **More chart types**: scatter/XY, area, combo (bar+line sharing one
  chart). The embedded-chart infrastructure (import/export/drag/
  resize/undo) already exists for bar/line/pie; this extends it, not
  a rebuild. Sparklines are a distinct rendering path (in-cell, no
  chart object) and would come later even in this tier
- **Excel Table Total Row** (per-column aggregation functions on a
  structured table). Structured references and cross-sheet table
  lookups already work; this is one more row type on infrastructure
  that already exists
- **Formula auditing views**: Trace Precedents/Dependents, Show
  Formulas (Ctrl+\`), Watch Window. All read-only views over data the
  engine already computes — no new calculation logic, "just" new UI,
  likely the cheapest-per-feature items on this whole list, but
  ranked here rather than Tier 1 because they're power-user tools a
  typical user won't reach for

### Tier 3 — needs dedicated planning: large effort, foundational or high-value

- **A real dependency graph for the calc engine.** Today
  `RecalculateAll`/`RecalculateWorkbook` is a brute-force fixed-point
  loop over every formula cell, up to 50 passes, on *every* edit —
  not just at file-open (where the "dirty cell" fix already helps).
  A file large enough to need that fix will still feel sluggish while
  being edited interactively. Fixing this properly (track which cells
  reference which, only re-evaluate what actually changed) is the
  biggest architectural investment on this list, touches the core of
  the engine, and needs its own dedicated design pass — but it also
  makes every future feature built on top of recalculation cheaper to
  ship well, so it belongs here and not at the bottom
- **Real 2D pivot tables** (a "Columns" field, multiple simultaneous
  measures) — already called out as not planned; still the biggest
  gap in the pivot feature specifically, needs a real 2D output layout
- **Goal Seek / Solver** — needs a new iterative numeric solver from
  scratch, no existing code to build on
- **VBA / macros** — this is v4.0 already (see above), the largest
  single feature on the entire roadmap; listed here again only for
  relative-priority context against the rest of this list

### Tier 4 — low priority, niche, or cosmetic

- What-if Data Tables (one/two-variable) and Scenario Manager — no
  work started, narrow audience even among spreadsheet power users
- Named cell styles and a swappable theme color palette — today
  formatting is always a literal, one-off style per cell; a gallery/
  theme system is a real usability nicety, not a blocker for any file
  opening or calculating correctly
- Secondary axis, trendlines, error bars on existing chart types
- Named table styles (banded rows are the only styling today) —
  cosmetic
- Password-protected / encrypted workbooks — real gap (a locked file
  can't be opened at all today) but likely a small fraction of actual
  users hit it day to day; no design started
- Slicers (for pivot tables and structured tables) — depends on the
  Tier 3 2D-pivot-table gap for the pivot case anyway

### Explicitly out of scope, not just "not yet"

- Collaboration features (track changes, shared/co-authored
  workbooks, comment threads with replies) — this is a native
  single-user desktop app, not a sync-backed one; no design makes
  sense without a server component that doesn't exist
- External data connections (Power Query, ODBC/database links, web
  queries) — same reasoning as VBA in the "Not currently planned"
  list: effectively its own sub-project, no existing library to build
  from on Haiku

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
