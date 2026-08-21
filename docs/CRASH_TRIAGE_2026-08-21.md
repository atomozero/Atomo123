# Crash report triage — 2026-08-21

Seven `.report` files found on the Desktop were analyzed (7 parallel
investigation agents, one per report) to determine whether Atomo123 is
responsible for each, and what to do about it. This file is the
resulting punch list — not permanent project documentation, just a
working record of this triage pass.

## Fixed

### 1. `Atomo123-544` and `Atomo123-8aa7aee` — same bug, hit twice

**"Looper must be locked" crash opening the Chart dialog with a
multi-cell selection active.** `MainWindow::ShowChartWindow` called
`fChartWindow->LoadRange(...)` (a separate `BWindow`, its own thread)
without locking it first. Same bug class already fixed once for
Color/Name windows (commit `2a28228`), never applied to this call site
because the chart-selection-prefill feature was added later.

**Status: fixed, commit `2f79407`.**

## Not our bug

### 2. `Tracker-146`

Crash entirely inside Haiku's own `libtranslation.so`
(`BTranslatorRoster::Private::_CheckHints`), before any translator's
`Identify()`/`Translate()` is even called. No Atomo123 code appears
anywhere in the stack trace. Nothing to fix here; if reproducible,
report upstream to Haiku, not this project.

## Confirmed non-issues

### 3 & 4. `debug_open-11505` and `debug_open-11527`

Both crashes are inside `debug_open`, a **throwaway diagnostic tool**
built earlier in a Claude Code session (in a scratch directory, never
part of the shipped app) to debug an unrelated XLSX-opening issue. Root
cause: the tool calls `TryToParseString("=PI()", ..., inWarnIfError=true)`
directly with no surrounding `try/catch`, and its own minimal harness
never initializes the engine's function table (the real app does this
via a `'Func'` resource baked into the actual executable, which the
throwaway tool never had) — so the parse genuinely fails and the
exception (correctly, by design) propagates and aborts the process.
Not a defect in `engine/`, `ui/src/AscdIO.cpp`, or any translator.
No action needed.

## Needs further investigation (not fixed — inconclusive)

### 5 & 6. `Tracker-7102` and `Tracker-7970`

Both crash during Tracker's thumbnail-generation path
(`BTranslatorRoster::Translate` → `BTranslationUtils::GetBitmap` →
Tracker's icon cache), with the fault landing inside
`XlsxTranslator.so`. This is the same *general area* as the
already-fixed "`Translate()` with NULL `info`" bug (commit `1696d4a`),
but that specific guard is already present at HEAD and doesn't match
these symptoms.

**Why unresolved**: both reports' installed `XlsxTranslator.so` was
rebuilt/reinstalled *after* the crash actually happened (confirmed via
file timestamps vs. report timestamps), so the debugger's symbol/line
resolution is unreliable — it resolves stale return addresses against
today's binary, not whatever was actually running at crash time. One
agent's report even attributed a `Tracker-7102` frame to
`BuildMultiChartDataXlsx`, a function that provably did not exist yet
at that crash's timestamp (it was added ~9.5 hours later in commit
`a66e02c`) — a false lead from stale symbolication, not a real
connection to the chart-export work.

**What was checked and ruled out**: `CZipReader::Open`/
`ReadCentralDirectory` (`translators/xlsx/MiniZip.cpp`) were suspected
of missing bounds validation on `nameLen`/`extraLen`/`commentLen`
against file size. On inspection this is **not** an actual
memory-safety bug — `ReadAt`'s return value is checked for the 46-byte
header read (the only place that matters for `pos` advancement
correctness), field values are bounded `uint16` (max 65535, trivial
allocation size), and buffers are zero-initialized before any
possibly-short read. A corrupted ZIP could produce a garbled filename
string, not an out-of-bounds access.

**Next step, if this is worth chasing further**: reproduce with a
freshly-built `XlsxTranslator.so` (matching current source) and an
`.xlsx` file containing an embedded chart, sitting in a Tracker
folder/context-menu path, then capture a *new* crash report against
that exact binary so the stack trace can be trusted. Don't reuse these
two old reports for further diagnosis — their symbol data is stale.

## Summary

| Report | Verdict | Action |
|---|---|---|
| `Atomo123-544` | Real bug, ours | **Fixed**, commit `2f79407` |
| `Atomo123-8aa7aee` | Same bug as above | Fixed by the same commit |
| `debug_open-11505` | Not real (throwaway tool) | None |
| `debug_open-11527` | Not real (throwaway tool) | None |
| `Tracker-146` | Not ours (Haiku `libtranslation.so`) | None |
| `Tracker-7102` | Unclear, possibly ours | Needs fresh reproduction |
| `Tracker-7970` | Unclear, possibly ours | Needs fresh reproduction |
