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
