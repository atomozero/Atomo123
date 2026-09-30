# Roadmap

Atomo123 is a native Excel-style spreadsheet for Haiku OS. The
calculation engine and legacy XLS importer are extracted and
modernized from the historical BeOS **Sum-It** project (community fork
`OpenSumIt`); the UI is written from scratch on Interface/Layout Kit.

**Status: v0.5.0 released** on GitHub (the entire "Path to full Excel
parity" Tier 4 chart/data-analysis backlog closed in this cycle: a
further round of chart editor polish (type icons, row/column
orientation toggle, per-series custom colors, a range picker), What-if
Data Tables' other half, Scenario Manager, named cell styles with a
live, swappable theme palette, and chart secondary axis/trendlines/
error bars — on top of everything shipped in v0.4.2: Excel-style
formula autocomplete, which itself built on v0.4.1's circular reference
detection and formula "point mode", and v0.4.0's four "Path to full
Excel parity" Tier 4 items, real sheet-protection passwords, workbook
open-password decryption, 9 more UI languages, and a menu/icon/
packaging polish pass — see CHANGELOG.md for the full detail). Every
Tier 4 item on "Path to full Excel parity" is now done; what remains
there is exclusively the three large Tier 3 foundations (a real
dependency graph for the calc engine, Goal Seek/Solver, VBA/macros —
see below for what's next).
This file tracks project-level status and forward plan only — the
detailed, per-release history of what shipped and the real bugs found
along the way lives in `CHANGELOG.md`.

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
| Release prep (v0.2.0) | Done | Tagged on GitHub, translators bundled in the hpkg, doc rewrite, splash/About-panel polish (see CHANGELOG.md) |
| Release prep (v0.2.1) | Done | Intermediate beta-tester release, tagged on GitHub: chart import from XLSX, repeated print headers |
| Release prep (v0.2.5) | Done | Tagged on GitHub: five function batches (30 functions), print settings + preview, pivot table multi-level grouping, translator ambiguous-text parity, AutoFill (see CHANGELOG.md) |
| Release prep (v0.2.6) | Done | Tagged on GitHub: critical multi-sheet XLSX corruption fix, background file loading with a footer progress bar, ~7x faster large-file opening (see CHANGELOG.md) |
| Release prep (v0.2.7) | Done | Tagged on GitHub: XLSM macro preservation, sheet/cell protection, critical `.ascd` multi-sheet open fix, XLSX Tier 1 compatibility (array/shared formulas, named ranges) (see CHANGELOG.md) |
| Release prep (v0.2.8) | Done | Tagged on GitHub: XLSX Tier 2 compatibility complete — comments, hyperlinks, data validation, freeze panes, border color, print settings (see CHANGELOG.md) |
| Release prep (v0.2.9) | Done | Tagged on GitHub: six merged feature/fix branches, `Welcome.xlsx` demo translated to English, real `FILTER` parsing bug, cell text/number-format fixes found against real Excel (see CHANGELOG.md) |
| Release prep (v0.3.0) | Done | Tagged on GitHub (tag moved to HEAD after real 2D pivot tables landed), GitHub Release published with the hpkg attached, install-tested via a real packagefs mount before publishing (see CHANGELOG.md) |

### Phase 13 detail

Systematic gap analysis against Excel, ordered by implementation
difficulty. Done: missing text/statistics functions (TRIM, UPPER/
LOWER/PROPER, FIND/SEARCH, CONCAT, MEDIAN, MODE), sheet add/delete/
rename, cell comments, hyperlinks, INDEX/MATCH, more chart types (line,
pie), border/color styles, data validation, live conditional
formatting.

Not currently planned (large, self-contained efforts, no library or
existing code to build on):
- **Goal Seek / Solver** — needs a new iterative numeric solver
- **Legacy XLS writing** (BIFF/OLE2) — import only today; writing BIFF8
  from scratch has no library to build on, deliberately excluded (XLSX
  already covers export to the Excel ecosystem)
- **Macros/VBA** — would need an embedded scripting engine, effectively
  its own sub-project

## Current focus

**v0.2.9 tagged and released on GitHub** (2026-09-13), on top of
v0.2.8: six individually-tested feature/fix branches merged into
master after the earlier v0.2.8 GitHub push had diverged (legacy
indexed color palette, docProps export, Excel Table Total Row
exclusion, real threaded comments, XLS named ranges, openpyxl
compatibility gaps), the bundled `Welcome.xlsx` demo workbook
translated to English (previously Italian-only, `Benvenuto.xlsx`),
and a real `FILTER` formula-parsing bug found translating it
(`_xlfn._xlws.FILTER`, Excel's own double-prefixed spelling for that
one function). Also a small round of real bugs found comparing a
live file against real Excel screenshots: cell text vertical
position/size for large custom fonts, forced decimal places on
percent/currency formats, and XLSX import's default vertical
alignment. See `CHANGELOG.md` for the full detail on each, including
the real bugs found while building them.

**Real Excel pivot table round-trip is now complete**: Phase 1
(persisted, cache-based native pivot object), Phase 2 (XLSX export as
real OOXML pivot parts), and Phase 3 (XLSX import of a real
`<pivotTable>` back into that same object) are all done, closing this
Tier 3 item — see "Path to 100% XLSX standard compatibility" below for
the full detail. Conditional formatting `dataBar` and `iconSet` are
also done (app-side rule type + XLSX import), closing the three-part
color scale/data bar/icon set plan (the legacy indexed color palette
is already done too, see below).

**Range-vs-scalar comparison in a function argument** (e.g.
`FILTER(A2:A8,B2:B8>=20)`, without a helper column) is also done now,
closing the last open item in "Path to full Excel parity" Tier 2 — see
below for the full detail.

## Next: v3.0 "Consolidation" and v4.0 "Scripting"

**v3.0 is functionally complete** as of the array formulas item above
(a deliberately-scoped v1 — see CHANGELOG.md; `UNIQUE` and spilling
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

## Path to 100% XLSX standard compatibility

This is a different axis from "Path to full Excel parity" below: that
list is about **app features** Excel has that Atomo123 doesn't yet
(more functions, more chart types, Goal Seek...). This list is about
**file-format round-trip fidelity** — parts of a real `.xlsx` file
(OOXML/ECMA-376) that this translator either mis-reads, silently
drops, or never writes, even for things the app itself already
supports natively (comments, hyperlinks, freeze panes, print
settings...). A perfectly feature-complete app can still corrupt or
lose a user's data through the file format if the translator has
gaps — which is exactly the bug class this session's `.ascd` fix and
the XLSM/protection work both belong to.

Compiled 2026-08-28 by auditing `translators/xlsx/XlsxTranslator.cpp`
directly against the OOXML parts/attributes it does and doesn't
inspect (not from the outdated prose in `docs/TRANSLATORS.md`, which
still claims — incorrectly — that only `sheet1.xml` is ever read;
multi-sheet import via `xl/workbook.xml`/`_rels` has in fact worked
for a long time). `docs/TRANSLATORS.md` needs a rewrite alongside this
work, not just this roadmap.

### How this is ordered

Same principle as "Path to full Excel parity" below: silent data loss
beats an absent feature, which beats a cosmetic gap. A file that
*looks* like it opened correctly but quietly turned live formulas into
frozen numbers is worse than a file that visibly can't do something.

### Tier 1 — silent data loss on import (do first, no exceptions)

(Called "Tier" here, not "Phase", to avoid confusion with the
unrelated, already-closed numbered `Phases` table at the top of this
file — these are two separate lists.)

- ~~**Shared formulas (`<f t="shared" si="N"/>`) import as static
  numbers, not formulas.**~~ Fixed — see `CHANGELOG.md`. Was the
  single most consequential gap found in this audit (real Microsoft
  Excel writes this very commonly whenever a formula is filled/copied
  across a range). Solved without any text-level reference rewriting,
  by exploiting how the engine already encodes relative-vs-`$`-fixed
  cell references and always evaluates a formula against whatever cell
  currently holds it
- ~~**Legacy array formulas (`<f t="array" ref="...">`) have the same
  failure mode**~~ Fixed — see `CHANGELOG.md`. Same root cause as
  the shared-formula bug above, same fix shape, but no
  relative-reference shifting needed (an array formula's other cells
  all show the *same* formula, not a shifted one) — done first as the
  simpler warm-up, before the shared-formula fix above tackled the
  harder shifting problem
- ~~**Named ranges / defined names (`<definedNames>` in
  `xl/workbook.xml`) are not read or written at all**~~ Core fixed —
  see `CHANGELOG.md`. `CNameTable` (de)serialization landed in the
  engine/native format first (the real prerequisite — no format
  persisted names at all before this, not just XLSX), then
  `<definedNames>` import/export on top of it: a workbook-scoped name
  is added to every sheet's own table (the closest match to "visible
  from any sheet" without a cross-sheet name-resolution redesign), a
  `localSheetId`-scoped one only to that sheet, and Excel's reserved
  `_xlnm.*` bookkeeping names (Print Area, Print Titles, ...) are
  recognized and skipped rather than polluting the table.
  ~~**Legacy `.xls` import still discarded every named range it
  parsed**~~ Fixed — see `CHANGELOG.md`. `CExcel5Filter::Name()`
  (`engine/src/Excel/Excel.pass1.cpp`) already parsed real `NAME`
  records, but only registered them through a live `CCellView`, never
  passed by this headless translator (dead code, already documented as
  such in the code) — now also collected into `GetNamedRanges()`
  (`Excel.h`, same pattern as `GetColumnWidths()`/`GetRowHeights()`)
  and registered into the real document's name table, then persisted
  through a new trailing ASCD section (`translators/xls/
  XlsTranslator.cpp` had never written one). Fixing this exposed two
  further real bugs in `Name()` itself, never caught because this code
  path had never been exercised end to end: an area reference's column
  was read as 1 byte (BIFF5 layout) instead of BIFF8's real 2 bytes —
  the exact bug already found and fixed for cell-formula references in
  `Excel.formula.cpp`, never back-ported here — and the name string's
  leading `grbit` byte was never skipped, the same bug already found
  and fixed for font names in `Font()`. `_xlnm.Print_Area` import/
  export (see Tier 2 below) already closed the print-area half of the
  second originally-open piece here; `_xlnm.Print_Titles` remains
  unwired, see the print-settings "explicitly out of scope" note in
  Tier 2

### Tier 2 — real native features with zero XLSX round-trip

**All done** (2026-08-29) — every item below either fully round-trips
now, or (border color) round-trips as far as the underlying model
allows, with the remaining gap clearly scoped as a separate, larger
effort. Everything here already worked in the app and persisted
correctly in the native `.ascd` format; none of it survived a trip
through `.xlsx`, in either direction, before this work.

- ~~**Cell comments/notes.**~~ Fixed — see `CHANGELOG.md`.
  `CContainer::SetComment`/`GetComment` were already a real, working
  feature; the XLSX translator now really parses `<comments>` (via the
  sheet's own `_rels`, `xl/comments{N}.xml` lives directly under `xl/`,
  not a `comments/` subdirectory like drawings/tables) on import and
  writes it on export, instead of the always-empty placeholder kept
  only to keep multi-sheet `.ascd` books aligned. No legacy VML drawing
  is read or written — this app has no comment-box geometry to
  round-trip (its own indicator is drawn purely from
  `CContainer::HasComment`), so real Excel/LibreOffice both display the
  plain `<comments>` content with their own default-styled box
- ~~**Hyperlinks.**~~ Fixed — see `CHANGELOG.md`. Same shape as
  comments: the translator now parses `<hyperlinks>` (a child of
  `<worksheet>`, unlike comments which live in their own part), with
  its `r:id` → URL indirection resolved through the sheet's own
  `.rels` (`TargetMode="External"`), and writes it back on export. An
  internal link (`location="..."` instead of `r:id`) is also imported
  directly, with no `.rels` involved; this engine stores only one
  string per hyperlink, so it can't distinguish the two forms on
  export — every exported link is written as external
- ~~**Data validation.**~~ Fixed — see `CHANGELOG.md`, for the two
  shapes this engine actually models (`ValidationRule` in
  `Container.h`): a literal dropdown list (`type="list"`, a quoted
  comma-separated literal — a list sourced from a cell range has no
  equivalent here and is skipped) and a numeric range with an implicit
  or explicit `operator="between"` (`type="whole"`/`"decimal"` with
  literal `formula1`/`formula2`, not cell references). Date/time
  ranges, other operators, and custom-formula rules still silently
  vanish, since the engine has nothing to round-trip them into
- ~~**Freeze/split panes.**~~ Fixed — see `CHANGELOG.md`. Only
  `<pane state="frozen"/>` (or `"frozenSplit"`) round-trips, matching
  what this app's own feature actually is: a real freeze, not a
  draggable split. A plain split (no `state`, or `state="split"`) has
  no equivalent here — its `xSplit`/`ySplit` mean twentieths of a
  point in that case, not a row/column count, and is left at 0/0 on
  import rather than misread as a freeze
- **Print settings for XLSX specifically** (margins, scale,
  header/footer, print area), split into four steps given its size:
  - ~~**Import margins/scale.**~~ Fixed — see `CHANGELOG.md`.
    `<pageMargins>` (always inches in XLSX, converted to the cm
    `AscdPrintSettings` already uses) and `<pageSetup scale/
    fitToWidth/fitToHeight>` (honoring the sibling `<sheetPr>
    <pageSetUpPr fitToPage="1"/></sheetPr>` flag that decides whether
    `scale` or the fit-to-page mode applies, exactly like Excel itself
    does) now populate real `AscdPrintSettings` values instead of
    always defaulting to 2 cm / 100%
  - ~~**Export margins/scale.**~~ Fixed — see `CHANGELOG.md`. Writes a
    real `<pageMargins>` (cm converted back to inches) and either
    `<pageSetup scale="N"/>` for a fixed percentage, or
    `<sheetPr><pageSetUpPr fitToPage="1"/></sheetPr>` +
    `<pageSetup fitToWidth/fitToHeight>` for the three "fit" modes —
    this export wrote no `<sheetPr>`/`<pageMargins>`/`<pageSetup>` at
    all before this fix
  - ~~**Import print area** (`_xlnm.Print_Area`).~~ Fixed — see
    `CHANGELOG.md`. The raw range text was already captured while
    parsing defined names, just discarded (`continue` on every
    `_xlnm.*` name) — now applied to `AscdSheet::hasPrintArea`/
    `printArea` via the same range-parsing helper real named ranges
    already use. A multi-area value (comma-separated rectangles, rare)
    keeps only the first, matching the native model's single-range
    limit
  - ~~**Export print area.**~~ Fixed — see `CHANGELOG.md`. Writes a
    real `_xlnm.Print_Area` (always sheet-scoped, `localSheetId="0"`,
    the only sheet this export ever produces) alongside real named
    ranges from `AscdSheet::hasPrintArea`/`printArea`. All four print
    settings steps are now done — the only remaining XLSX print gaps
    are the two explicitly out-of-scope items just below
  - **Explicitly out of scope**: print header/footer text and repeated
    print titles (rows/columns) have no native-format field or UI at
    all today (`PageSetupWindow` deliberately has no such controls,
    and Excel's own `&P`/`&D`/`&F` placeholder codes have no
    equivalent syntax here) — adding either would mean designing new
    model fields and UI first, not just translator wiring like the
    four steps above
- ~~**Border color is read as presence/absence per side only**~~ Fixed
  on import — see `CHANGELOG.md`. `ParseStyles` now resolves the real
  `<color rgb="..."/>`/`theme="N"` on a border side into
  `CellStyle::fBorderColor` (one color shared by all four sides, the
  scope the engine itself already committed to — not a per-side
  color), reusing the same `ResolveColorAttrs` helper fill/font colors
  already used. ~~Export is NOT fixed~~ Fixed — see `CHANGELOG.md`.
  This translator now builds a real dynamic `styles.xml` (distinct
  `CellStyle`s deduplicated into `<fonts>`/`<fills>`/`<borders>`/
  `<numFmts>`/`<cellXfs>`), so fill, font, border color/thickness,
  alignment, and number format all export for real instead of plain
  uncolored cells — border color couldn't be closed in isolation since
  it needs the same style-table machinery as the rest. Verifying this
  end to end also turned up a separate pre-existing bug: this
  translator's own internal `ReadASCD` (used on the ASCD→XLSX export
  path) had several sections that read color/font/alignment/
  border-thickness/number-format/underline/wrap-text bytes only to
  stay aligned in the stream, without ever applying them to the
  document — fixed alongside the export work

### Tier 3 — partial fidelity, moderate value

- **Conditional formatting rule types beyond `cellIs`/
  `duplicateValues`.** `colorScale` is now imported for real (see
  "Path to full Excel parity" Tier 1 above). ~~`dataBar`~~ Fixed (Fase
  B of the same plan) — see `CHANGELOG.md`. New rule type
  `eCondDataBar` (`Container.h`): reuses `ColorScalePoint` for the
  min/max thresholds instead of a new struct, resolved the same way as
  `colorScale` (`ResolveColorScaleThreshold`), but produces a per-cell
  FRACTION (0..1 of the cell width to fill, `DataBarInfo`) instead of
  an interpolated color — `SheetView::DrawCellBand` draws it as a
  partial-width rectangle on top of the normal background, text still
  legible on top. App-side UI (`ConditionalFormatWindow`, a 4th type,
  one color) and XLSX import (`<dataBar><cfvo/><cfvo/><color/></dataBar>`,
  reusing the exact same in-line cfvo/color parsing as `<colorScale>`)
  landed in the same pass, per the plan below. Native + XLSX ASCD
  format bumped to version 6 (`dataBarColor`, one more field appended
  to the existing conditional-formatting section). ~~`iconSet`~~ Fixed
  (Fase C, the last of the three "relative to the range" types) — see
  `CHANGELOG.md`. New rule type `eCondIconSet`: the number of icons
  (3/4/5) comes from `colorScalePoints.size()` itself, never from the
  XLSX style name (`ConditionalFormatRule::iconSetStyle` is stored only
  for XLSX round-tripping, deliberately never parsed for meaning — more
  robust than needing to know every Excel style name, including ones
  never seen yet). Per-cell result is an icon index (`IconSetInfo`),
  drawn as a small colored circle (red→yellow→green across the whole
  set) — one visual style for every XLSX icon style name, not the real
  distinct shapes (arrows/flags/ratings); no "reverse" support. Found
  and fixed a real rendering bug while writing the pixel-level test for
  this: this engine's default alignment (`eAlignGeneral`) is always
  LEFT, even for numbers (unlike Excel's right-aligned numbers by
  default) — an icon drawn at the cell's left edge was silently
  overpainted by the cell's own left-aligned text, drawn in a later
  pass; moved to the right edge instead, which the pixel test caught
  before this ever shipped. App-side UI (`ConditionalFormatWindow`, a
  5th type, no color picker — always a 3-level set, matching Excel's
  own unconfigured default) and XLSX import (`<iconSet iconSet="...">
  <cfvo/>...</iconSet>`, reusing the same in-line cfvo parsing as
  `<colorScale>`/`<dataBar>` — note XLSX never writes a `<color>` here,
  unlike those two) landed in the same pass. Native + XLSX ASCD format
  bumped to version 7 (`iconSetStyle`). This closes the three-part
  "relative to the range" conditional formatting plan (Fase A/B/C).
  `cellIs` against a cell reference (not just a literal) and arbitrary
  boolean `expression` rules with relative references are also
  supported (`eCondExpression`, `compareIsCellRef` — see
  `CHANGELOG.md`, found closing a real gap on `agile-kanban-board.xlsx`).
  ~~`containsText`/`top10`/`aboveAverage` and any `cellIs` operator
  other than "equal"~~ Fixed — see `CHANGELOG.md`. `cellIs` now
  supports all 8 ECMA-376 operators, and the rest of the standard rule
  family (`containsText`/`notContainsText`/`beginsWith`/`endsWith`,
  `containsBlanks`/`notContainsBlanks`/`containsErrors`/
  `notContainsErrors`, `top10`, `aboveAverage`/`belowAverage`) is
  modeled for real, both import and export, native UI included.
  `timePeriod` rules remain explicitly out of scope (date-arithmetic-
  heavy, a separate feature, never named as a gap). ~~XLSX export of
  conditional formatting~~ Fixed — see
  `CHANGELOG.md`. `WriteXLSX` never wrote a single
  `<conditionalFormatting>`/`<dxf>` for any rule type, old or new,
  before this — every rule was silently lost on re-export. Now all six
  rule types export for real (`cellIs`/`duplicateValues`/`expression`
  via a real `<dxfs>` table, `colorScale`/`dataBar`/`iconSet` fully
  inline), closing the gap flagged separately in "Path to full Excel
  parity" Tier 1 above.
- ~~**Legacy indexed color palette** (`indexed="N"`, the fixed 64-entry
  Excel 97-2003 table)~~ Fixed — see `CHANGELOG.md`. Added as a third
  fallback in `ResolveColorAttrs` (after `rgb`/`theme`), so it applies
  everywhere that function is already called: fill/font/border colors
  and conditional-formatting (`dxf`) colors. Indices 64/65 ("System
  Foreground"/"System Background", i.e. automatic) are deliberately
  left unresolved; a custom `<colors><indexedColors>` override (rare)
  is not read, only the fixed default table
- **Real Excel pivot tables**: XLSX round-trip (`<pivotTable>`/
  `xl/pivotCache/pivotCacheDefinition*.xml`) split into three phases,
  same shape charts were historically built in (native object first,
  then XLSX import, then XLSX export).
  - ~~**Phase 1: persisted, cache-based native pivot object.**~~ Fixed
    — see `CHANGELOG.md`. Before this, the pivot feature
    (`BuildPivotTable`/`WritePivotTable` in `ui/src/Pivot.h`/`.cpp`)
    was fire-and-forget: it wrote plain calculated cells with no
    definition kept anywhere, so a reopened file's pivot table was
    indistinguishable from any other block of cells, with nothing to
    refresh and nothing a future XLSX export could turn into a real
    cache. New `PivotTableObject` (`engine/src/Cell/Container.h`,
    alongside `PivotAggFunc`/`PivotRow`, moved there from the UI layer
    so a future importer can construct them without depending on
    `ui/src/Pivot.h`) persists `sourceRange`/`destAnchor`/`aggFunc`/
    `cachedRows` in a new native `.ascd` trailing section (no version
    bump — pure EOF-tolerant append, same shape as the `CTableDef`
    section). `MainWindow::HandlePivotRequest` now persists the object
    after creating a pivot; a new `RefreshAllPivotTables()` (wired to
    "Aggiorna tabelle pivot" in the Inserisci menu) re-runs
    `BuildPivotTable` against the *current* source and rewrites both
    the sheet and the cache — matching real Excel's own behavior
    (a pivot table cache sits still until an explicit Refresh, not a
    live auto-recalculating formula). Found and fixed a real
    testability/UX bug while writing the live-refresh test:
    `HandlePivotRequest` showed a modal `BAlert::Go()` "N categories
    found" confirmation on every successful pivot creation, which
    blocks forever with no user present to click it — removed
    entirely (real Excel doesn't confirm a successful pivot creation
    either, so this was also a minor real UX wart, not just a test
    problem).
  - ~~**Phase 2: XLSX export**~~ Fixed — see `CHANGELOG.md`. A
    persisted `PivotTableObject` (Phase 1) now writes real OOXML pivot
    parts on "Save As XLSX": `pivotCacheDefinitionN.xml` (schema +
    shared items for the category field, min/max for the numeric
    field), `pivotCacheRecordsN.xml` (one `<r>` per RAW source row,
    re-read live from the document at export time — not the aggregated
    `cachedRows`, which only gives the cache's display order),
    `pivotTableN.xml` (location/pivotFields/rowFields/rowItems/
    dataFields, `rowGrandTotals="0"` to match `WritePivotTable`'s own
    output, one `subtotal` value per `PivotAggFunc`), plus the
    relationships/`[Content_Types].xml` wiring and a new
    `<pivotCaches>` element in `xl/workbook.xml`. Scoped to pivots with
    exactly one category column (`sourceRange` two columns wide) — a
    2+-column pivot still exports its already-computed cells correctly
    but gets no live pivot metadata, a documented v1 limit, not a
    silent gap (nested `<rowItems>` for multi-level grouping is
    real OOXML-validated territory this phase deliberately didn't
    attempt). A real prerequisite bug was found and fixed along the
    way: the XLSX translator's own `ReadASCD` (used internally for the
    ASCD→XLSX export round-trip) never read the pivot table trailing
    section Phase 1 added to the native format, so
    `doc->GetPivotTables()` was unconditionally empty at export time
    until this was fixed — without it, Phase 2 would have shipped
    silently inert. The OOXML pivot/cache XML shape itself was checked
    against the ECMA-376 schema via a dedicated validation pass before
    writing any code (no real Microsoft Excel is available in this
    sandbox to catch a "repair" prompt after the fact) — it caught two
    real mistakes ahead of time: an invalid `count=` attribute on
    `<sharedItems>`, and an invalid `dataField=` attribute on the
    measure's own `<pivotField>` (the Values-area association is only
    ever declared via `<dataFields><dataField fld=.../></dataFields>`).
  - ~~**Phase 3: XLSX import**~~ Fixed — see `CHANGELOG.md`. Opening a
    real `.xlsx` file with a `<pivotTable>` now reconstructs a
    `PivotTableObject`, not just its already-computed cells, when the
    shape matches Phase 2's own export shape (single category column,
    single measure). Deliberately does NOT parse `pivotCacheRecords`
    at all — `cachedRows` is recomputed by re-grouping the sheet's own
    cells (already imported by `ParseSheet` earlier in the same pass),
    the same technique `BuildPivotXmlParts` already uses on the export
    side, symmetric by design. Anything outside the supported shape
    (2+ row fields, column/page fields, 2+ data fields, an
    unrecognized `subtotal`, or a cache source on a different sheet)
    is skipped silently — the cells were already imported normally
    moments earlier regardless, so a skip never loses or corrupts
    visible data, only the "this is a live, refreshable pivot"
    metadata on top.

### Tier 4 — rare spec corners, low priority

- `calcChain.xml`, `connections.xml`, `externalLinks/*`,
  `customXml/*` — safe to keep ignoring; Excel regenerates
  `calcChain.xml` itself and doesn't require any of these to open a
  file
- ~~**Threaded comments**~~ Fixed — see `CHANGELOG.md`. Degrades to a
  plain comment, as anticipated here: every `<threadedComment>`
  message for a cell (`xl/threadedComments/threadedCommentN.xml`) is
  concatenated in file order and replaces the legacy `<comments>`
  placeholder Excel always writes alongside for backward compatibility
  (a real file's own boilerplate text, not this app's own fallback —
  without this fix, that boilerplate was literally all that imported,
  since a real file's comment content lives only in the threaded part
  once any Excel version since 2019 wrote it). `xl/persons.xml`
  (author display names) is not read — no author concept anywhere in
  this app's own comments either
- **Sparklines, embedded OLE objects/form controls, digital
  signatures** — no support and no plan; each would need real design
  work disproportionate to how often a typical file uses them
- ~~**Password-hash sheet protection**~~ Fixed — see `CHANGELOG.md`.
  The real `<sheetProtection>` attributes (legacy `password="..."`
  4-hex-digit checksum, or the modern `algorithmName`/`hashValue`/
  `saltValue`/`spinCount` form) round-trip byte-for-byte through native
  persistence and XLSX import/export (`AscdSheetProtection`, ASCD format
  version 9), AND (a real step further, done in the same effort) this
  app's own protection is no longer an unauthenticated flag: "Proteggi
  foglio" now offers a real, optional password (ECMA-376 §18.3.1.85,
  `engine/src/Utils/ExcelPasswordHash`, this project's first
  cryptography dependency — OpenSSL), verified before unprotecting.
  **Distinct from** whole-workbook open-password encryption (the file
  itself is AES-encrypted, can't be opened at all without the
  password) — that gap is now ALSO closed, read-only, see the Tier 4
  entry below and `CHANGELOG.md`
- ~~**`docProps/core.xml`/`app.xml`**~~ Fixed — see `CHANGELOG.md`.
  Writes the export's own timestamp (`dcterms:created`/`modified`) and
  `Application`; author/title/company/revision have no equivalent
  field in the document model, so none is written (not invented)
- **Row/column outline/grouping** (Excel's +/- expand-collapse groups)
  doesn't exist as an app feature at all yet, native or otherwise —
  this would need real engine/UI work first, not just a translator
  change, so it's out of scope for "XLSX compatibility" specifically
  until the feature exists to round-trip
- **Per-run rich text formatting inside a single cell** (part of a
  cell's text bold, another part not) collapses to plain concatenated
  text on import, by design — `CellStyle` is one style per cell, not
  per character range. Fixing this for real would mean changing the
  engine's cell model itself; noted here as an explicit, likely
  permanent limit rather than deferred work

## Path to full Excel parity (beyond v4.0)

A systematic look at what's still missing for Atomo123 to be a
drop-in Excel replacement for most real-world files, beyond the
"Not currently planned" items already called out above. This list is
about **app features**, not file-format fidelity — see "Path to 100%
XLSX standard compatibility" above for the translator-specific gaps.
Ordered by **priority tier**, not just by category or raw
implementation effort — see "How this ordering was decided" below for
the reasoning. None of this is scheduled yet, it's a reference list
for future planning sessions.

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

- ~~**Sheet/workbook protection (enforce cell locking).**~~ Shipped in
  v0.2.7 — see above. Turned out bigger than the original estimate: the
  roadmap's premise was wrong (`CellStyle::fLocked` was never actually
  persisted anywhere, and defaulted to *unlocked*, the opposite of
  Excel) — verified before implementing, per this project's own
  discipline
- ~~**XLSM round-trip preservation.**~~ Shipped in v0.2.7 — see above
- ~~**More financial functions.**~~ Fixed — see `CHANGELOG.md`. This
  item's own premise was wrong: checking `Functions.finance.cpp`
  before implementing anything (this project's own discipline) found
  `NPV`/`IRR`/`PMT`/`FV`/`PV` already fully implemented and working —
  only `RATE` (the periodic rate of the same cash-flow equation
  `PMT`/`PV`/`FV` each solve for a different unknown) was genuinely
  missing. No closed-form solution exists for `RATE`, so it uses an
  iterative secant-method solver, the same shape as the existing `IRR`
  solver right below it in the same file
- ~~**Conditional formatting: color scales.**~~ Shipped (Fase A of a
  3-phase plan — data bars and icon sets are Fase B/C, still to do).
  Two-point min/max scale (a 3rd/percentile point is modeled and
  evaluated but has no UI editor yet), live evaluation
  (`eCondColorScale` in `Container.styles.cpp`), native `.ascd`
  persistence (format version 3), and real XLSX import from
  `<colorScale>`/`<cfvo>`. ~~XLSX *export* of conditional formatting
  (any rule type, old or new)~~ Fixed — see Tier 3 below and
  `CHANGELOG.md`.
- ~~**Open a file passed on the command line** (`atomo123
  file.xlsx`).~~ Shipped: `App::ArgvReceived` converts each argument
  (skipping `argv[0]`, the executable's own path) into a `BEntry`/
  `entry_ref` and reuses the exact same window-selection logic as
  `RefsReceived` (`App::OpenOneRef`, extracted from the two functions'
  previously-duplicated loop bodies) — a nonexistent path is ignored
  rather than treated as fatal, matching the app's existing permissive
  behavior for an unsupported format. Live-verified: `./Atomo123
  file.ascd` opens the real file (confirmed via `hey Atomo123 get
  Title of Window 0`), not a blank new document

### Tier 2 — do next: moderate effort, real but narrower value

- ~~**Dynamic arrays beyond `SEQUENCE`**: `UNIQUE`, `SORT`/`SORTBY`,
  `FILTER`.~~ Shipped, one commit each: `UNIQUE` (single-row/column
  dedup, first-occurrence order), `SORT` (genuinely 2D — sorts a
  table's rows by a chosen column, each row moves as a unit),
  `SORTBY` (same but the sort key comes from a separate range), and
  `FILTER` (keeps only rows where a boolean/nonzero condition holds,
  `if_empty` fallback when nothing matches). All five spill functions
  now share the exact same mechanism `SEQUENCE` introduced
  (`CContainer::ApplySpill`) — no new plumbing needed. Declared scope
  limits, all rare in practice: no true 2D array dedup for `UNIQUE`,
  no `by_col` for `SORT`, single sort key only for `SORTBY`, no
  horizontal arrays for `SORTBY`/`FILTER`. Spilling from inside a
  nested expression stays deferred (all four collapse to a scalar —
  the first element — when nested, matching `SEQUENCE`'s own
  documented limit)
- ~~**`FILTER` unrecognized as `_xlfn._xlws.FILTER`.**~~ Fixed —
  real bug found on this app's own `Welcome.xlsx` demo workbook.
  Excel writes `FILTER` specifically with a *second* compatibility
  prefix after `_xlfn.` (`_xlfn._xlws.FILTER`, a leftover of it
  originally shipping under a different internal namespace) — every
  other dynamic-array function above only ever gets the single
  `_xlfn.` prefix. Two bugs, both needed: the lexer's dotted-identifier
  continuation state only accepted a letter after a `.`, not the
  underscore `_xlws` starts with, splitting the token at `_xlfn` alone
  before it ever reached function-name resolution; and
  `GetFunctionNr` stripped only one `_XLFN.` prefix, never a second
  `_XLWS.` one
- ~~**A range compared to a scalar inside a function argument (e.g.
  `FILTER(A2:A8,B2:B8>=20)`, the natural way to write `FILTER`'s
  condition and the form real Excel/openpyxl produce by default) does
  not evaluate to a per-cell boolean array.**~~ Fixed — see
  `CHANGELOG.md`. Scoped narrowly to comparison operators, not a
  general array-broadcast engine (arithmetic operators still don't
  understand ranges, deliberately, matching the reasoning this item
  originally called for). A new transient `eBoolArrayData` `Value`
  type carries a per-cell boolean array through expression evaluation
  — `Value::CompareLT/LE/GT/GE/EQ/NE` (renamed from the old
  `operator<` etc., which the C++ standard forbids from taking more
  than one explicit parameter, discovered only once the real build
  rejected the originally-planned default-argument design) build one
  when either comparison operand is a multi-cell range, reading real
  cell values via a `CContainer*` now threaded through from
  `CFormula::Calculate`. `FILTER` (`GetBoolArrayArgument`,
  `FunctionUtils.cpp`) tries this new form first, falling back
  unchanged to the existing literal-boolean-range path — the
  `Welcome.xlsx` helper-column workaround keeps working byte for byte,
  it's just no longer the only option. A bare whole-cell
  `=B2:B8>=20` (no `FILTER` wrapper) collapses to its first element,
  matching the identical "implicit intersection" precedent already in
  place for a bare `eRangeData` result. Scope limits, deliberate and
  documented in code: only 1D ranges (single row or column); an
  already-computed array compared again degrades to a plain `FALSE`
  rather than a second broadcast pass. `INDEX`/`INDIRECT` using `:` as
  an operand remains a separate, unaddressed gap (see "Not currently
  planned"). Every call site of the renamed comparison methods across
  the whole codebase (six in `Formula.cpp`, plus four more found only
  by attempting a full rebuild after the rename — `Container.cpp`'s
  sort comparator, `Functions.logical.cpp`'s `SWITCH`, and
  `Functions.spreadsheet.cpp`'s `UNIQUE`/`SORT`/`SORTBY` — grep alone
  had missed these) needed updating; the full existing test suite
  across `engine`/`ui`/`translators/xlsx` (500+ checks) passed
  unchanged after the fix, the strongest signal available that the
  `Value` memory-ownership changes (a new heap-owned array member
  alongside the existing `fText` deep-copy discipline) didn't regress
  anything already working
- ~~**More chart types**: scatter/XY, area, combo (bar+line sharing
  one chart).~~ Shipped, one commit each. Area reuses
  `ComputeLineLayout`/`ComputeMultiLineLayout` unchanged, filling the
  zone under the line with a semi-transparent color (multi-series
  overlap rather than stack, matching Excel's plain "Area", not
  "Stacked Area"). Scatter needed its own data shape end to end
  (`ScatterPoint`, an (x, y) pair — `ChartSeries`'s label/value can't
  represent two numeric axes); plain dots only, no connecting lines,
  and the value range does not force-include zero like the other
  types since scatter data is often far from either axis's origin.
  Combo renders the first series as bars and every subsequent series
  as lines on one shared value scale and category grid — no secondary
  Y axis. All three reuse the existing embedded-chart infrastructure
  (drag/resize/undo, native `.ascd` persistence — chart type is
  already a raw byte, any new enum value round-trips automatically);
  XLSX import/export for all three remains out of scope, a separate
  gap from the existing bar/line/pie XLSX chart support. Sparklines
  are a distinct rendering path (in-cell, no chart object) and would
  come later even in this tier
- ~~**Horizontal bar charts** (Excel's real "Bar" type — categories on
  the vertical axis, bars extending left-to-right — as opposed to this
  app's `eBarChart`, which is actually Excel's "Column").~~ Shipped,
  found opening a real user file (`money-manager-2.xlsx`, a Vertex42
  budget template) whose 3 embedded charts showed "chart type not
  supported". `ComputeHBarLayout`/`DrawHBarChart` (single-series) and
  `ComputeGroupedHBarLayout`/`DrawGroupedHBarChart` (multi-series) are
  axis-swapped mirrors of the existing vertical-bar functions, reusing
  the `ChartValueToX` helper already written for the scatter chart;
  XLSX import (`<c:barDir val="bar"/>`, previously explicitly rejected)
  and export are both wired up too — the first of the "More chart
  types" batch above to get real XLSX round-trip, not just native
  `.ascd`. A second, unrelated bug surfaced investigating that same
  file and got fixed alongside it: multi-series chart import required
  the value columns to be strictly adjacent to the category column
  (`ReconstructChartRange`), so a real "Budget vs Actual"-style chart
  with a spacer/unrelated column between its two series (e.g. category
  A, values B and D, C empty) silently failed with every data row
  dropped. `ChartObject::valueColumns` (empty = legacy contiguous
  behavior, unchanged for every existing chart) now carries the true,
  possibly non-adjacent source columns end to end — import, live
  redraw, native persistence, and XLSX export — fixing this for every
  chart type with 2+ series, not just horizontal bar
- ~~**Charts with categories/series laid out across a row instead of
  down a column** (Excel's other valid layout).~~ Shipped, found
  opening two more real files during the systematic XLSX sweep
  (`earned-value-management.xlsx`, `family-budget-planner.xlsx`),
  both previously rejected with "layout dati non compatibile" even
  after the horizontal-bar/non-adjacent-column fix above.
  `ChartObject::rowOriented`/`valueRows` is the transposition of the
  `valueColumns` non-contiguous-column support from that same fix —
  `ReconstructChartRange` now also recognizes a single-row category
  reference, with a matching row-major series builder wired through
  import, live redraw, print preview, native `.ascd` persistence, and
  XLSX export. A real, separate bug was found and fixed while
  verifying the export path end to end: the XLSX translator's own
  copy of the ASCD reader never learned about a versioned tail
  (header/footer/print-titles) the app's own native writer always
  appends after print settings, silently misaligning every section
  written after it — including embedded charts — for any document
  exported through that path
- ~~**Excel Table Total Row: import correctness**~~ Fixed — see
  `CHANGELOG.md`. `RegisterTable`/`ApplyTableBanding`
  (`translators/xlsx/XlsxTranslator.cpp`) now read `totalsRowCount`
  and exclude that many rows from the bottom of the table's data
  range and from banding. Before this fix, a real Excel totals row
  was silently treated as an ordinary data row: any structured
  reference to that column (including the totals row's own formula,
  typically `=SUBTOTAL(109,Table1[Column])`) would self-include the
  total in its own aggregate — a real correctness bug, not just a
  cosmetic gap. **Not attempted**: per-column `totalsRowFunction`/
  `totalsRowLabel` are not read at all — `CTableDef` has no field for
  them and there is no live "Total Row" UI/feature in this app that
  would consume them, so storing them would be dead data; the totals
  row's own cell value/formula already imports and calculates
  correctly through the normal per-cell pipeline
  regardless
- ~~**Formula auditing views**: Trace Precedents/Dependents, Show
  Formulas (Ctrl+\`), Watch Window.~~ Shipped, one commit each (see
  CHANGELOG.md). All read-only views over data the engine already
  computes, as expected — no new calculation logic. Show Formulas is a
  transient per-window toggle (not persisted to `.ascd`). Trace
  Precedents/Dependents draws live arrows for the active cell only
  (single level, not "precedents of precedents"), reusing
  `CFormulaIterator` — the same mechanism the calc engine already uses
  for recalculation — for precedents, and a fresh on-demand sheet scan
  for the reverse relationship (dependents), since the engine has no
  dependency graph to invert (see the Tier 3 item below). Declared
  scope limit: both are same-sheet only, since `CFormulaIterator`
  itself already skips cross-sheet references. The Watch Window follows
  `NameWindow`'s existing Lock()/setter/Unlock() pattern rather than a
  BMessage round-trip, refreshed live from `MainWindow::DocumentChanged`
  so pinned values track edits with no user action needed

### Tier 3 — needs dedicated planning: large effort, foundational or high-value

- ~~**A real dependency graph for the calc engine.**~~ Fixed — see
  `CHANGELOG.md`. `RecalculateAll`/`RecalculateWorkbook`'s brute-force
  fixed-point loop (up to 50 passes over every formula cell, on every
  edit) is replaced by an incrementally-maintained, workbook-wide
  reverse-dependency graph (`QualifiedCell`, `engine/src/Cell/
  Container.h`) and a single topological-order recalculation pass
  (`RecalculateMinimal`, `ui/src/AscdIO.cpp`) that touches only a cell's
  true transitive dependents — same-sheet or cross-sheet — exactly
  once. Covers whole-column/row references (`=SUM(A:A)`, coarse edges
  instead of one per cell) and spill-owner redirection (a cell reading
  a `SEQUENCE`/`UNIQUE`/etc. spill member depends on the owning
  formula, including when the spill's own shape grows/shrinks
  mid-recalculation). A genuine new capability falls out of the same
  mechanism: circular references spanning two different sheets are now
  detected and marked `#CIRCULAR!`, which the old same-sheet-only DFS
  detector could never see. Measured, honest benchmark on a synthetic
  20,000+ cell single sheet with a worst-case (iteration-order-reversed)
  40-link dependency chain: **125x faster** for a single-cell edit
  (`ui/tests/test_dependency_graph_recalc.cpp`, Part 11) — developed
  behind a temporary shadow-verification mode (every edit ran both the
  old and new engine and compared results) during a multi-phase bake-in
  before the old path was cut over. `RecalculateAll`/`RecalculateWorkbook`
  remain in the codebase (still exercised directly by several tests,
  not dead code) but are no longer on the live user-facing path.
  **Explicitly out of scope, not silently missing**: only the single
  most performance-sensitive call site (`SheetView::CommitEditing`, one
  keystroke commit) seeds the new engine with the precise edited cell;
  every other mutation (paste, fill, sort, row/column insert-delete,
  undo/redo, etc.) still seeds with every formula cell in the workbook
  for now — correct and still one pass instead of up to 50, but not the
  maximum possible speedup; threading a precise seed through those
  remaining call sites is a real, declared follow-up, not a bug.
  Structured-table (`Table1[Col]`) cross-sheet dependency tracking is
  also deferred (the reference is a `valName` bytecode token, invisible
  to the new cross-sheet-precedent iterator) — a table formula still
  recalculates correctly, just via the broad fallback seed rather than
  a precise graph edge. The pre-existing, unrelated bug where renaming
  a sheet doesn't rewrite other sheets' formula text referencing its
  old name (`MainWindow::RenameSheet`) is untouched by this work either
  way — the graph resolves to a live sheet object once built, so a
  rename doesn't corrupt it, but the underlying formula-text gap
  remains for a future session
- ~~**Real 2D pivot tables** (a "Columns" field, multiple simultaneous
  measures)~~ Fixed — see `CHANGELOG.md`. `PivotTableObject` gained a
  Columns axis and a list of explicit measure columns (each with its
  own aggregation), with a real 2D grid output (two header rows), a
  dynamic Columns-field/measures picker in `PivotWindow`, native
  `.ascd` persistence, and XLSX round-trip (`<colFields>`/multiple
  `<dataField>` on export, recognized on import) — additive throughout,
  every existing 1D pivot (no Columns field, one measure) is
  byte-for-byte unaffected. Scoped for now to a single row-grouping
  column when the Columns field or 2+ measures are used (2+ row-grouping
  columns together with either of those still write correct cells but
  no XLSX pivot parts, same limit as the pre-existing multi-level-row
  case)
- **Goal Seek / Solver** — needs a new iterative numeric solver from
  scratch, no existing code to build on
- **VBA / macros** — this is v4.0 already (see above), the largest
  single feature on the entire roadmap; listed here again only for
  relative-priority context against the rest of this list

### Tier 4 — low priority, niche, or cosmetic

- ~~**What-if Data Tables** (one/two-variable) **and Scenario
  Manager**~~ Fixed — see `CHANGELOG.md`. Data Tables: "Tabella dati…"
  command (Data menu, `ui/src/WhatIfWindow.h`) fills a grid of computed
  results by temporarily swapping substitute values into one or two
  input cells, recalculating, and reading back the formula cell's
  result each time — reuses the engine's existing recalculation entry
  point, no dependency graph needed. Uses this app's own simplified
  convention (formula cell always at the selection's top-left corner in
  all three cases) rather than reproducing Excel's real per-case corner
  placement quirk exactly, since this feature is native-only (no XLSX
  `{=TABLE(...)}` import/export). Scenario Manager: "Gestione
  scenari…" command (Data menu, `ui/src/ScenarioWindow.h`) saves named
  sets of substitute values for later reuse and re-application
  ("Mostra", a real undoable mutation, unlike Data Tables' own
  auto-restoring preview). Scoped to a single contiguous range of
  changing cells per scenario (not Excel's arbitrary up-to-32
  non-contiguous cells — this app's range parser/undo snapshot don't
  support non-contiguous cell lists yet); Scenario Summary (a static
  cross-scenario comparison report) is out of scope for this pass.
- ~~**Named cell styles and a swappable theme color palette**~~ Fixed —
  see `CHANGELOG.md`. A cell can reference a named style
  (`CellStyle::fNamedStyleID`, `engine/src/Cell/NamedStyle.h`) instead
  of literal formatting; redefining the style, or a color in the new
  document-wide theme palette, changes every cell using it live, at the
  next repaint, resolved fresh inside `CContainer::GetCellStyle` — no
  cell is ever rewritten. ~17 curated built-in styles (a subset of
  Excel's real ~50, same scoping as named table styles), 8 theme color
  roles (smaller than Excel's 12-slot model). A named style covers
  background, text color, alignment, and bold/italic/underline;
  borders/protection stay whatever the cell's own literal formatting
  was. "Stili cella…" (Format menu) opens the management window;
  creating a style always starts from an existing cell's formatting
  (no from-scratch editor). Persisted natively (workbook-wide,
  stored once on the first sheet, same convention as VBA project
  bytes); XLSX round-trip (`theme1.xml`/`<cellStyles>`) is out of scope
  for this pass.
- ~~**Secondary axis, trendlines, error bars on existing chart types**~~
  Fixed — see `CHANGELOG.md`. All three are per-series options
  (`ChartSeriesOptions`, `ui/src/Chart.h`) set from the same
  "Opzioni…" pop-up (secondary axis is a plain checkbox
  in the per-series row instead, one bool needs no pop-up) in the chart
  editor, for the four multi-series chart types (grouped bar, multi-line,
  multi-area, combo). Trendlines: linear (least-squares) or moving
  average, drawn in the series' own color at reduced opacity. Error
  bars: fixed value or percentage of the point's value, drawn as an
  "I-beam" at full opacity. Secondary axis: the shared value range
  every multi-series chart used to compute in one pass now splits into
  an independent primary/secondary range per series, with a label-only
  right-side axis drawn when at least one series opts in. All three
  round-trip through native `.ascd` (a single shared trailing record,
  EOF-tolerant, no format-version bump) and through XLSX
  (`<c:trendline>`, `<c:errBars>`, and a real dual-`<c:axId>`-pair
  `<c:plotArea>` shape for the secondary axis — OOXML assigns axis
  membership by which chart-type element contains a series, not by an
  attribute on the series itself). **Declared out of scope, not
  silently missing**: single-series and scatter charts (no per-series
  UI row exists for them to hang a control on); horizontal bar charts
  never get a secondary axis on export (its value axis is horizontal,
  no orientation-swapped dual-axis shape was built); a hand-crafted
  XLSX file with more than one primary+one secondary axis pair
  degrades gracefully on import rather than reconstructing correctly;
  error bars have no standard-deviation or custom-range source, only
  fixed/percentage; trendlines are linear/moving-average only, not
  Excel's logarithmic/exponential/power/polynomial family.
- ~~**Named table styles**~~ Fixed — see `CHANGELOG.md`. XLSX import now
  reads the real `<tableStyleInfo name="...">` and colors the banding
  from a small built-in list of ~8 recognized Excel style names
  (`engine/src/Cell/TableStyles.h`); unrecognized/custom names keep the
  neutral gray of before. A "Stile tabella" submenu (Data menu) lets the
  user change a registered table's style directly in the app. Persists
  through native `.ascd` round-trip; real XLSX **export** of structured
  tables (`<table>`/`tableN.xml`) does not exist in this app at all yet
  (tables only ever round-tripped through native `.ascd`, a separate
  pre-existing gap, not part of this item's scope) — so the style name
  survives import + native save/reload, but re-exporting to `.xlsx`
  does not yet write it back out
- ~~**Password-protected / encrypted workbooks**~~ Fixed (read-only) —
  see `CHANGELOG.md`. A real `.xlsx` protected with a workbook
  open-password (Agile Encryption, Excel 2010+) now opens: prompts for
  the password, decrypts the real ZIP/OOXML in memory, feeds it into the
  normal import pipeline unchanged. Scoped to Agile Encryption only (the
  legacy 2007-2010 "Standard Encryption" binary format is out of scope);
  read-only (re-exporting drops the encryption, a plain "Salva" on such
  a document warns before silently overwriting the encrypted original)
- ~~**AutoFilter persistence**~~ Fixed — see `CHANGELOG.md`, the
  prerequisite for real Slicer state: which values are excluded per
  column now survives a native `.ascd` save/reload (not just the
  resulting hidden rows), and XLSX import now reads a real
  `<filterColumn><filters>` when present (the discrete-list form only —
  comparison/top-N/dynamic/color/icon filters have no equivalent in this
  engine's model and are skipped, declared not silent). XLSX **export**
  of `<autoFilter>` does not exist in this app at all (a separate,
  larger, pre-existing gap, same shape as the structured-table export
  gap above).
- ~~**Slicers**~~ Fixed (native `.ascd` only) — see `CHANGELOG.md`. A
  slicer is a floating button-list (`ui/src/Slicer.h`) that toggles
  values of one column of the sheet's active AutoFilter — drag/resize/
  delete/undo reuse the exact same on-sheet-object machinery as charts/
  images, and a button click is exactly `SetColumnValueHidden` under the
  hood, so it stays in sync with the AutoFilter dropdown automatically.
  Scoped to pivot-table-less structured/AutoFilter data for v1 (a sheet
  has only one AutoFilter at a time in this app already); real XLSX
  slicer XML (`slicerCache*.xml`) does not exist — declared out of
  scope, not silently dropped, same shape as every other Tier 4 XLSX
  export gap in this batch

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
