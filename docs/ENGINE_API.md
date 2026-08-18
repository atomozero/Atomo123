# Calculation engine (`engine/`)

Static library (`libengine.a`) extracting the calculation engine and
legacy Excel importer from the historical Sum-It/OpenSumIt codebase
(`legacy/opensumit/`), decoupled from its BeOS-era UI (`CCellView`/
`CCellWindow`). Builds and runs **with no Interface Kit dependency**
(no link to `BView`/`BWindow`), verified with:

```
nm -u libengine.a | c++filt | grep -oE "\bB[A-Z][a-zA-Z]*::" | sort -u
```

which reports only Application/Storage/Support Kit classes (`BList`,
`BLocker`, `BLooper`, `BMallocIO`, `BMessage`, `BPath`, `BPositionIO`,
`BResources`, `BFont`), plus one known exception (`BAlert`, see
"Known limitations" below).

## Build & test

```
cd engine
make                 # produces libengine.a
make test            # tests/smoke_test.cpp
make test-functions  # tests/named_functions_test.cpp
```

`smoke_test.cpp` builds a document (`CContainer`) with no view attached
and checks formula results, proving the engine runs headless.
`named_functions_test.cpp` additionally exercises named-function
formulas (`SUM`, `IF`, `MAX`, `SUMIF`, `COUNTIF`, `AVERAGEIF`...), which
need `InitFunctions()` and a real `'Func'` resource (compiled on the
fly by this same target with the historical `rez`/`bsl` tools).

## File map

| Directory | Contents | Notes |
|---|---|---|
| `src/Cell/` | `Cell`, `CellData`, `CellIterator`, `CellParser`, `CellStyle`, `CellUtils`, `Container*`, `Range`, `Value`, `Formatter*`, `FontMetrics`, `FontStyle` | Data model (cells, styles, value formatting). Excludes `CellCommands.*` (undo/redo tied to `CCellView`) and `CellScrollBar.*` (UI widget) |
| `src/Formula/` | `CalcLooper`, `CalcStack`, `Formula*`, `lexer`, `parser` | Formula parser/evaluator, dependency graph. Excludes `CalculateJob.*` (UI-side threading/progress wrapper) |
| `src/Functions/` | All spreadsheet functions (`Functions.*.cpp`) | `NUMPAGES`/`PAGE`/`DOCUMENT` (print/window-related) already null-safe upstream, return NaN with no view |
| `src/Excel/` | Legacy XLS importer (BIFF/OLE2) | `Excel.pass1.cpp` writes named ranges/column widths/row heights onto the (former) `CCellView` — see limitation below |
| `src/FileSys/` | `FileFormat`, `Text2Cells` | Basic text/file-format import-export |
| `src/Collections/`, `src/Misc-Classes/`, `src/Utils/`, `src/Metrowerks/` | Generic data structures, error handling, strings, threading (`MThread`), locking (`StLocker`) | `Utils.cpp` stripped of menu-building code (`BuildMenu`/`GetMenu`/`GetMBarHeight`, Interface Kit); excludes `DrawUtils.*` |
| `src/Config/` | `Constants.h`, `Globals.h`, `Config.h`, `EngineGlobals.cpp` | `EngineGlobals.cpp` is new: globals (locale separators, NaN sentinels) previously initialized in the historical `Sum-It.cpp` app |
| `src/bsl/` | Resource string ID headers (`errmsg.h`, etc.) | Numeric constants only, generated long ago by the `bsl` tool |
| `src/Stubs/` | `EngineViewStub.h`, `ProgressStub.h` | See below |

## UI stubs

Historical code often takes an optional `CCellView*` to notify the UI
(redraw, selection, progress) during model operations. In the engine
library that pointer is always `NULL`; these stubs exist only to
satisfy the compiler on branches reachable when it's non-null (never at
runtime here):

- `EngineViewStub.h` replaces `CCellView`: redraw/selection methods are
  no-ops, `GetHeights()`/`GetWidths()` return a discarded local
  `CRunArray`, `IsNamedRange()` always returns `false`.
- `ProgressStub.h` replaces `StProgress` (a real `BView`-based progress
  bar): all methods are no-ops.

## Named functions in formulas

`InitFunctions()` (`src/Functions/FunctionUtils.cpp`) populates the
globals `GetFunctionNr` (`src/Utils/Utils.cpp`) uses to recognize `SUM`,
`IF`, etc. in formulas, reading three resources attached to the running
binary via `gResourceManager` (`src/Misc-Classes/ResourceManager.h`, a
thin `BResources` wrapper):

- `'Func'` (ID 128): `FuncRec { char funcName[10]; short argCnt,
  funcNr, groupNr; }` array, one entry per function (89 total — the 86
  historical Sum-It functions plus `SUMIF`/`COUNTIF`/`AVERAGEIF`),
  compiled from `resources/funcs_by_nr.r` with `rez`.
- `'StrL'` ID 7/8: paste/description strings per function (for a future
  "Paste function" dialog, not yet in the native UI), compiled from
  `resources/FuncNames.txt`/`FuncDescs.txt` with `bsl`.

These three resource source files are unmodified copies of the
historical ones in `legacy/opensumit/sum-it/Resources/` (same 4-clause
BSD license as the rest of Sum-It) plus the three added entries.
Content is indexed **by position** against the enum in
`src/Functions/Functions.h` — a new function must always be **appended**
(never inserted mid-list), updating all three files plus the enum and
`SetupDefaultFuncs()` together, or the positional indexing breaks.

`InitFunctions()` is called from `ui/src/App.cpp`
(`App::ReadyToRun()`), before the main window is created, inside a
`try`/`catch (CErr&)` — a failure (binary without resources, unexpected
path) doesn't block startup, it just falls back to no named functions.
`gAppName` must be set to the running binary's path **before** calling
`InitFunctions()`: `LoadPlugIns()` uses it to look for optional add-ons
in `Functions/` next to the binary, and dereferences a null pointer if
left unset.

**Argument separator**: the parser uses `gListSeparator`
(`src/Config/EngineGlobals.cpp`, default `;`), not `,` — no UI call
path passes an explicit separator to `TryToParseString`, so it applies
app-wide. See `docs/USER_GUIDE.md`.

## Known limitations

- **Real error alerts**: `CErr::DoError()` (`MyError.cpp`) creates a
  real `BAlert` to show errors — may misbehave headless, without a
  `BApplication`. A headless caller should catch the `CErr` exceptions
  the engine already throws instead of letting them reach `DoError()`.
- **Named ranges not resolved headless**: `EngineViewStub::IsNamedRange`
  always returns `false`, and Excel import (`Excel.pass1.cpp`) writes
  named ranges, column widths and row heights onto the (former)
  `CCellView` rather than `CContainer` — silently discarded headless.
  Should move onto the model, not the view.
- **Display formatting**: `Formatter`/`FontMetrics` handle numeric/
  currency/date formatting (needed because it's tied to `CellStyle`),
  but the method that actually draws colored text on a real `BView`
  (`CFontMetrics::SetFontSizeColor`) is a no-op — on-screen drawing is
  the UI's job.

## Notable fixes found while isolating the engine

Beyond the mechanical `long`→`int32` porting already covered in
`legacy/opensumit/PORTING_STATUS.md`, isolating the engine surfaced
real bugs, mostly from `sizeof(long)==4` assumptions (true on BeOS/PPC
32-bit, false on Haiku x86_64 where `sizeof(long)==8`):

- `cell::operator==`/`<` compared raw `*(long*)` reinterpretations of
  the 4-byte `cell` struct — read past the struct on 64-bit,
  corrupting `std::map<cell,CellData>` ordering silently. Fixed with a
  direct field comparison.
- `CFontMetrics::operator==` had the same pattern on `rgb_color`. Fixed
  with a sized `memcmp`.
- The compiled-formula bytecode format used `long *fString` with
  `kPFWordSize = sizeof(long)`; on 64-bit, `sizeof(cell)/kPFWordSize`
  truncated to 0, desynchronizing every opcode read after a cell
  reference (`errIllPFString` on any formula like `=A1+A2+A3`). Fixed
  by using fixed-width `int32` throughout.
- `CFontMetrics::operator[]`/`StringWidth` called `be_plain_font->
  StringWidth()` when no real font was loaded, blocking forever
  without an app_server connection. Fixed with a fixed fallback (8px/
  character).
- `CFontSizeTable::operator[]` indexed an always-empty `std::vector` in
  the engine library — undefined behavior. Fixed with a bounds check.
- `GetFunctionNr`'s binary search dereferenced `gFuncArrayByName[0]`
  even with `gFuncCount == 0` (table never loaded in the engine
  library, since `InitFunctions()` was never called there). Fixed with
  an explicit `gFuncCount <= 0` guard.
- `parser.cpp`'s `Factor()` used `GetFunctionNr`'s result to index
  `gFuncArrayByNr` before checking it was `>= 0` — negative index into
  a raw C array. Fixed by only computing `expectedArgs` when valid.
- Adding `SUMIF`/`COUNTIF`/`AVERAGEIF` uncovered an off-by-one in
  `GetFunctionNr`'s length check that silently rejected any 9-character
  function name (`AVERAGEIF`) — none of the 86 original names were that
  long, so it had never triggered. Fixed.
- `CExcel5Filter::GetBookStream` was declared `throw()`/`noexcept` but
  called `CExcelStream::Read`, which throws `CErr` on a malformed/
  truncated XLS file (a normal case, not an edge case) — violating a
  `noexcept` contract calls `std::terminate()` immediately, bypassing
  every `try`/`catch` up the stack. Fixed by removing the `throw()`
  specifier.

Several of these null-pointer bugs appeared as an indefinite hang
rather than an immediate crash on this Haiku setup — likely
`debug_server` intercepting the fault and waiting for a GUI
interaction that never comes in a headless run. Always run headless
tests with `timeout N ./binary`.
