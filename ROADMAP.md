# Roadmap

Atomo123 is a native Excel-style spreadsheet for Haiku OS. The
calculation engine and legacy XLS importer are extracted and
modernized from the historical BeOS **Sum-It** project (community fork
`OpenSumIt`); the UI is written from scratch on Interface/Layout Kit.

**Status: v0.2.6 released; v0.2.7 functionally complete, not yet
tagged** (XLSM macro preservation, sheet/cell protection, a critical
multi-sheet `.ascd` open bug). All planned phases through "closing the
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

v0.2.7 is functionally complete, on top of v0.2.6: XLSM macro
preservation on save, sheet protection and cell locking (with XLSX
round-trip), and a critical fix for a multi-sheet `.ascd` file that
had stopped opening. Not yet tagged as a release. See `CHANGELOG.md`
for the full detail on each, including the real bugs found while
building them.

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
