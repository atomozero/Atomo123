# Roadmap

Atomo123 is a native Excel-style spreadsheet for Haiku OS. The
calculation engine and legacy XLS importer are extracted and
modernized from the historical BeOS **Sum-It** project (community fork
`OpenSumIt`); the UI is written from scratch on Interface/Layout Kit.

**Status: v0.2.7 released** (XLSM macro preservation, sheet/cell
protection, a critical multi-sheet `.ascd` open bug, and Tier 1 of the
XLSX standard compatibility plan: array/shared formulas, named
ranges). All planned phases through "closing the
gap with Excel" are done or in good shape; a handful of large,
optional features remain unplanned backlog items (see "Not currently
planned" below, and "Path to full Excel parity" for what's next).
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

**v0.2.7 tagged and released on GitHub** (2026-08-28), on top of
v0.2.6: XLSM macro preservation on save, sheet protection and cell
locking (with XLSX round-trip), a critical fix for a multi-sheet
`.ascd` file that had stopped opening, and Tier 1 of the "Path to
100% XLSX standard compatibility" plan below (array/shared formulas
importing as frozen values, named-range persistence). See
`CHANGELOG.md` for the full detail on each, including the real bugs
found while building them.

**Next up: closing the remaining gaps in XLSX standard
compatibility** — see "Path to 100% XLSX standard compatibility"
below. This became the priority after this session's `.ascd` bugfix
work, which found several real, silent data-loss patterns in the XLSX
translator while investigating an unrelated bug; auditing the
translator systematically against the OOXML spec (rather than waiting
for the next one to surface in a user's file) is worth doing before
anything else.

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
  recognized and skipped rather than polluting the table. **Two
  explicit pieces still open**, not attempted in this pass: legacy
  `.xls` import still discards every named range it parses (runs
  against a no-op stub with no live document to attach to — dead code,
  already documented as such in the code); and Excel's
  `_xlnm.Print_Area`/`_xlnm.Print_Titles` are only *skipped* today, not
  wired to this app's own print-area/print-settings persistence
  (`AscdPrintSettings`, unrelated to the name table) — so a real
  Excel file's print area still doesn't survive an XLSX round-trip,
  the one piece of this item that was always XLSX-specific rather than
  blocked on the name-table prerequisite

### Tier 2 — real native features with zero XLSX round-trip

Everything here already works in the app and persists correctly in
the native `.ascd` format; none of it survives a trip through
`.xlsx`, in either direction, today.

- **Cell comments/notes.** `CContainer::SetComment`/`GetComment` are a
  real, working feature — the XLSX translator writes and reads an
  always-empty placeholder section specifically so multi-sheet books
  stay aligned (see the `.ascd` book-format fix above), but never
  actually parses `<comments>`/the legacy-drawing VML anchor on
  import, or emits them on export. A commented real Excel file loses
  every comment on import; a commented Atomo123 file loses every
  comment on export to `.xlsx`
- **Hyperlinks.** Same shape as comments exactly: `<hyperlinks>` (plus
  its `r:id` → URL indirection through `sheetN.xml.rels`) is never
  parsed on import, never written on export
- **Data validation.** `<dataValidations>` is never parsed on import
  (dropdown lists, numeric/date range rules, custom formula rules all
  silently vanish) and never written on export either — the whole
  round-trip is missing, not just one direction
- **Freeze/split panes.** The app has real freeze-pane support
  (Phase 7), but `<pane>` inside `<sheetView>` is never read or
  written — only `showGridLines` is extracted from `<sheetView>`
  today. A file with frozen headers loses that on both import and
  export
- **Print settings for XLSX specifically** (margins, scale,
  header/footer, print area). The native format has carried all of
  this per-sheet since v0.2.5 (`AscdPrintSettings`) — none of it maps
  to `<pageSetup>`/`<pageMargins>`/`<headerFooter>` or the
  `_xlnm.Print_Area`/`_xlnm.Print_Titles` defined names (see Tier 1)
  on the XLSX side. Do this together with the defined-names fix above,
  since print area needs it anyway
- **Border color is read as presence/absence per side only**, not the
  actual RGB (`ParseStyles` tracks which sides have a border, not what
  color) — a real but narrower gap than the others in this tier,
  included here because it's the same "partially wired" pattern

### Tier 3 — partial fidelity, moderate value

- **Conditional formatting rule types beyond `cellIs`/
  `duplicateValues`.** `colorScale`/`dataBar`/`iconSet`/`containsText`/
  `top10`/arbitrary-formula `expression` rules are recognized and
  safely ignored (no rule added) rather than misapplied — correct but
  incomplete. This should happen together with the identically-named
  item already in "Path to full Excel parity" Tier 1 below (icon
  sets/color scales/data bars as an app feature) — implementing the
  app-side rule type and its XLSX import in the same pass avoids
  building the evaluator twice
- **Legacy indexed color palette** (`indexed="N"`, the fixed 56-color
  Excel 97-2003 table) resolves to the engine's default color instead
  of the real one. Rare in files saved by modern Excel/LibreOffice,
  more likely in old files re-saved without a full re-color pass
- **Real Excel pivot tables** (`<pivotTable>`/
  `xl/pivotCache/pivotCacheDefinition*.xml`) have no XLSX round-trip
  at all — a pivot table in an imported file is invisible today (only
  its underlying source data imports, if that's on a separate visible
  sheet), and this app's own pivot feature never exports as a real
  OOXML pivot table, only as plain calculated cells. Large: needs its
  own design pass, likely comparable in effort to the chart
  import/export work already done, probably belongs after Tier 1/2
  land

### Tier 4 — rare spec corners, low priority

- `calcChain.xml`, `connections.xml`, `externalLinks/*`,
  `customXml/*` — safe to keep ignoring; Excel regenerates
  `calcChain.xml` itself and doesn't require any of these to open a
  file
- **Threaded comments** (`xl/threadedComments/*.xml` + `xl/persons.xml`,
  the modern "Notes vs. Comments" format Excel has used since 2019) —
  once Tier 2's legacy `<comments>` support lands, decide whether
  threaded comments need separate handling or can degrade to a plain
  comment (likely acceptable: this app has no concept of comment
  threads/replies either)
- **Sparklines, embedded OLE objects/form controls, digital
  signatures** — no support and no plan; each would need real design
  work disproportionate to how often a typical file uses them
- **Password-hash sheet protection** (Excel's actual
  `<sheetProtection password="..."/>` legacy hash, or the newer
  `algorithmName`/`hashValue`/`saltValue`/`spinCount` form) is not
  read or written — this app's own protection (shipped in v0.2.7) is
  an unauthenticated on/off flag, matching the *behavior* a casual
  user sees but not the real mechanism: opening a real password-locked
  sheet imports it as simply protected, with no password required to
  unprotect it again in this app. **Distinct from** whole-workbook
  open-password encryption (the file itself is AES-encrypted, can't be
  opened at all without the password) — that's a separate, larger gap,
  already called out in "Path to full Excel parity" Tier 4 below
- **`docProps/core.xml`/`app.xml`** (author, title, company, revision
  metadata) are never written on export — cosmetic, Excel opens the
  file fine without them
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
