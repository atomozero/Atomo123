# Changelog

Detailed, per-release history of what shipped and the real bugs found
along the way. This is a diary, not a plan — for current status and
what's next, see `ROADMAP.md`.

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
- Added sheet protection and cell locking ("Proteggi foglio", "Blocca/
  Sblocca celle selezionate" — Dati menu). `CellStyle::fLocked` existed
  as a field but was never actually wired up: not persisted anywhere,
  not read by any UI code, and defaulted to *unlocked* — the opposite
  of Excel, where every cell is locked by default and protection alone
  makes that matter. Fixed the default, added persistence (a new
  per-cell "unlocked cells" section and a per-sheet "protected" flag in
  the ASCD/ASCB format, plus real `<protection locked="0"/>` and
  `<sheetProtection/>` reading/writing in the XLSX translator — a
  minimal two-entry `xl/styles.xml` is now written on export
  specifically to carry this, XLSX export previously wrote no style
  information at all), and a real edit guard: typing, Delete, Paste,
  Fill/AutoFill, and every formatting command (bold/italic/underline/
  colors/borders/alignment/wrap/number format) now refuse to touch a
  locked cell on a protected sheet, with an Excel-style warning.
  Unlocked cells stay editable even when the sheet is protected, same
  as Excel. Two real bugs caught along the way: both `OpenFile` code
  paths (synchronous and the background-thread one) never applied a
  freshly-opened document's protection state to the live view, and
  `SaveToFile`'s non-native export path never threaded the protection
  flag through to the writer at all — both silently would have lost
  protection on next save/reopen
- Fixed a critical regression reported by the user: a real 4-sheet
  `.ascd` file stopped opening entirely (`B_BAD_DATA`) right after the
  sections above were added. Root cause: EOF-tolerant reading of a new
  per-sheet trailing section is only safe at the true end of the whole
  stream — in a multi-sheet book, any sheet before the last one that
  predates the new sections had the "missing section" fallback
  silently consume real bytes belonging to the next sheet's block
  instead, desyncing every sheet after it. The exact same bug class as
  the chart-title and printArea trailing-section bugs above, this time
  hitting the native app format itself on real user data. Fixed
  properly rather than patched around: a new `"ASC2"` book format
  writes each per-sheet block with an explicit length prefix, so any
  future missing-section fallback is bounded to that sheet's own
  buffer and can never spill into the next one — this closes the whole
  bug class for any future section additions, not just this one. The
  old `"ASCB"` magic is still read (never written again), frozen to
  its exact pre-protection section list via a new
  `skipVbaAndProtectionSections` flag so legacy files keep parsing
  byte-for-byte as before. Verified against the user's actual file
  (all 4 sheets, correct names and content) plus the full existing
  regression suite

What shipped in v0.2.8, on top of v0.2.7:
- Fixed the first item of the new "100% XLSX standard compatibility"
  audit: a legacy array formula (`<f t="array" ref="B1:B2">FORMULA</f>`,
  entered in real Excel with Ctrl+Shift+Enter across more than one
  cell) only carries its formula text on the top-left cell of the
  range — every other cell in the range has no `<f>` of its own, only
  a frozen cached `<v>`. The XLSX parser never looked at `<f>`'s
  attributes at all, so those other cells silently imported as dead
  static numbers instead of live formulas. Fixed by recording each
  array formula's range and text when its anchor `<f>` closes, then
  reusing that same text (no relative-reference shifting — a CSE
  array formula shows the identical formula in every cell of its
  range, unlike a shared formula) for any later cell that falls inside
  that range with no formula of its own. New regression test builds a
  minimal real XLSX in memory (via the existing `CZipWriter`) with a
  deliberately wrong cached value on both cells, confirming the engine
  recalculates both independently instead of trusting the cache.
  Shared formulas (`<f t="shared" si="N"/>`, the more common and more
  consequential case in real files) are next
- Fixed the second, most consequential item of the XLSX compatibility
  audit: a shared formula (`<f t="shared" si="N" ref="B2:B20">FORMULA
  </f>` on the anchor cell, `<f t="shared" si="N"/>` empty on every
  other cell of the range) was imported the same way the array-formula
  bug above described — anchor cell fine, every other cell in the
  range silently frozen to its last cached `<v>`. Unlike an array
  formula, a shared formula's non-fixed (no `$`) references must shift
  by the offset between each cell and the anchor, while `$`-fixed
  references stay put — reusing the anchor's raw text unchanged (like
  the array-formula fix) would be wrong here. Rather than writing a
  text-level reference-rewriter (regex over column letters, tracking
  `$`, sheet-qualified refs, etc. — its own source of bugs), the fix
  leans on how this engine already encodes cell references
  (`cell::GetFormulaCell`/`GetFlatCell`, `engine/src/Cell/Cell.cpp`): a
  reference without `$` is stored as a signed delta from whatever cell
  the formula text is parsed against, a `$` reference is stored
  absolute regardless — and `CContainer::CalcCell` always evaluates a
  formula's references against whichever cell currently holds its
  bytecode, not the cell it was originally parsed against. So
  compiling the anchor's formula text with the *anchor's* location as
  the parse base, then writing the resulting bytecode into the
  *sibling* cell, makes the exact same bytecode resolve its relative
  references correctly shifted the moment it's evaluated in place —
  no manual text surgery needed, and `$`-fixed references naturally
  stay fixed for free. New helper `CompileSharedFormulaAt` in
  `XlsxTranslator.cpp` does exactly this (a decoupled version of what
  `TryToParseString` normally does in one step). New regression test
  covers two independent shared-formula groups in the same sheet, one
  purely relative and one mixing a `$A$1` fixed reference with a
  relative one in the same formula, each cell getting a deliberately
  wrong cached value to prove the engine recalculates for real. This
  closes Tier 1 of the "100% XLSX standard compatibility" plan except
  for named ranges/defined names, which needs engine-level name-table
  persistence first (see `ROADMAP.md`)
- Added named-range persistence, closing the core of the last Tier 1
  item. `CContainer`'s name table (`CNameTable`, the engine structure
  behind "Intervalli con nome") worked correctly at runtime — a name
  defined in one session correctly resolved in formulas — but was
  never written to any file format, not even the native one: closing
  and reopening any file lost every named range, XLSX or otherwise.
  Fixed at the source: `ui/src/AscdIO.cpp`'s `SaveASCD`/`LoadASCD`
  gained a new trailing section (name → range, EOF-tolerant like every
  other optional section in this format) so the native `.ascd`/`.ascb`
  round-trip finally carries names at all. `translators/xlsx/
  XlsxTranslator.cpp` then wires `<definedNames>` in `xl/workbook.xml`
  to that same mechanism in both directions: import parses each
  `<definedName>`, skips Excel's own reserved bookkeeping names
  (prefix `_xlnm.`, e.g. `_xlnm.Print_Area`) rather than polluting the
  name table with them, and adds a workbook-scoped name (no
  `localSheetId`) to every sheet's own table — the closest match to
  "visible from any sheet" this engine's per-sheet-only name
  resolution can offer without a larger cross-sheet redesign; a
  sheet-scoped name (`localSheetId` present) goes only to that sheet.
  Export writes every name back out the same way, workbook-scoped
  (this export path is single-sheet only, nothing to disambiguate).
  Two duplicated copies of the ASCD writer/reader needed the identical
  new section (the translator's own private `WriteASCD`/`ReadASCD`,
  separate from `ui/src/AscdIO.cpp` by design, same reasoning as every
  other section already duplicated there) — a gap in only one of the
  two would have silently dropped every name on that specific code
  path, the same bug class fixed for real earlier this session (the
  `.ascd` multi-sheet corruption). New regression tests cover the
  native round-trip (`ui/tests/test_names.cpp`) and both XLSX
  directions including the reserved-name exclusion
  (`translators/xlsx/tests/test_xlsx_translator.cpp`) — the export
  side needed its own from-scratch minimal ASCD fixture builder, since
  real named-range data has to land at the very end of the format,
  after every other optional section, all of which had to be
  represented too (empty) to keep the fixture aligned.
  **Not done in this pass, explicitly deferred**: legacy `.xls`
  import still discards any named ranges it parses (runs against a
  no-op stub with no live document to attach to, already documented in
  the code as dead code); Excel's `_xlnm.Print_Area`/`_xlnm.
  Print_Titles` reserved names are recognized and skipped but not yet
  wired to this app's own print-area/print-settings persistence — see
  `ROADMAP.md`
- Closed Tier 1 of "100% XLSX standard compatibility" and completed
  the whole of Tier 2 (six items, twelve commits, code and docs kept
  separate throughout): cell comments, hyperlinks, data validation,
  freeze panes, border color, and the full print-settings plan
  (margins/scale and print area, both import and export). Every item
  followed the same pattern found while auditing the translator: this
  app's own native feature already worked and persisted correctly in
  `.ascd`, but the XLSX translator's `WriteASCD`/`ReadASCD` (its own
  private, duplicated copy of the ASCD reader/writer, kept separate
  from `ui/src/AscdIO.cpp` by design) had a hardcoded stub for that
  section — always writing "absent", always discarding what it read —
  so the data silently vanished the moment it passed through XLSX,
  even though the underlying app feature was never broken.
  - **Cell comments**: `xl/comments{N}.xml` lives directly under `xl/`,
    not a `comments/` subdirectory like drawings/tables — a wrong
    assumption caught immediately by the first import test. No legacy
    VML drawing is read or written; this app has no comment-box
    position/visibility to round-trip, only the text content, which
    survives fully without it
  - **Hyperlinks**: `<hyperlinks>` lives inside `<worksheet>` itself
    (unlike comments), with `r:id` resolved to a real URL through the
    sheet's own `.rels` (`TargetMode="External"`); an internal link
    (`location="..."` instead of `r:id`) is also imported directly.
    This engine stores only one string per hyperlink, so every
    exported link is written as external even if it started as an
    internal same-workbook reference
  - **Data validation**: scoped to the two shapes `ValidationRule`
    actually models — a literal comma-separated dropdown list
    (`type="list"` with a quoted literal, not a cell-range source) and
    a numeric range with an implicit or explicit `operator="between"`.
    Date/time ranges, other operators, a list sourced from a cell
    range, and custom-formula rules have no engine equivalent and are
    silently skipped, the same way conditional formatting already
    skips rule types it can't model
  - **Freeze panes**: only `<pane state="frozen"/>` (or
    `"frozenSplit"`) round-trips, matching what this app's own feature
    actually is — a real freeze, not a draggable split, whose
    `xSplit`/`ySplit` would mean twentieths of a point instead of a
    row/column count in that other case
  - **Border color**: import only. `ParseStyles` now resolves the real
    `<color rgb="..."/>`/`theme="N"` on a border side (reusing the
    same color-resolution helper fill/font colors already used) into
    one color shared by all four sides, the scope the engine's own
    model already committed to. Export is **not** fixed: this
    translator writes no dynamic `styles.xml` at all yet (fill and
    font colors aren't exported either) — a separate, larger effort
  - **Print settings**, the largest of the six, split into four steps:
    margins/scale import (`<pageMargins>`, always inches in XLSX,
    converted to the centimeters `AscdPrintSettings` already uses;
    `<pageSetup scale/fitToWidth/fitToHeight>`, honoring the sibling
    `<sheetPr><pageSetUpPr fitToPage="1"/></sheetPr>` flag exactly like
    Excel itself does), margins/scale export (the reverse conversion,
    plus a bonus fix for the XLSX-to-XLSX re-export path that had been
    silently dropping print settings too), print-area import
    (`_xlnm.Print_Area`'s raw range text was already captured while
    parsing defined names, just discarded like every other `_xlnm.*`
    name — a multi-area value, rare in practice, keeps only the first
    rectangle), and print-area export (written as a real, always
    sheet-scoped defined name alongside real named ranges). Print
    header/footer text and repeated print titles (rows/columns) are
    explicitly out of scope: neither has a native-format field or UI
    at all today, and Excel's own `&P`/`&D`/`&F` placeholder codes have
    no equivalent syntax here — both would need new modeling and UI
    before any translator work makes sense, unlike everything else on
    this list
  - Confirmed live in the running app (2026-08-29): opened a real
    `.xlsx` with custom margins/scale and checked "Imposta pagina"
    showed the real imported values, not the previous always-default
    2cm/100%. Also found, while testing this: `atomo123 file.xlsx`
    from the command line doesn't open the file at all (`App.cpp` has
    no `ArgvReceived` override, only drag-and-drop/Tracker/the app's
    own File menu work) — tracked as a small, separate item in "Path
    to full Excel parity"
