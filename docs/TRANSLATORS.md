# Translation Kit add-ons (`translators/`)

Haiku Translation Kit add-ons (`BTranslator`/`BTranslatorRoster`)
converting between external file formats and the isolated calculation
engine (`engine/`). Each lives in its own directory under
`translators/`, same shape: a concrete `BTranslator` implementing
`Identify()`/`Translate()`/`InputFormats()`/`OutputFormats()`, a
`Makefile` building it as a shared object linked against
`engine/libengine.a`, and an end-to-end test.

Link dependency beyond `engine/libengine.a` and `libbe.so`: every
add-on also needs **`libtranslation.so`** (base `BTranslator`/
`BArchivable` implementation).

Each add-on implements `MakeConfigurationView` to show an "About" panel
in Haiku's Translators preferences (name, version, description) —
localized to English via its own embedded Locale Kit catalog, resolved
at runtime through the `image_id` `make_nth_translator` receives (an
add-on isn't a `BApplication` and can't use a host app's catalog).

## `translators/csv` — CSV and the native ASCD format

Converts between CSV text (`kAtomoCsvFormat`, MIME `text/csv`) and a
minimal native format defined here, **ASCD** ("Atomo Sheet Cell Data",
`kAtomoNativeFormat`). ASCD serializes only a `CContainer`'s non-empty
cells (row, column, text — the formula if present, else the formatted
value), preserving formulas through a native round-trip; it predates
the real app document format (`ui/src/AscdIO.cpp`) and now mainly
serves as the intermediate format each translator's `ReadASCD`/
`WriteASCD` uses to talk to `MainWindow::SaveToFile`/`OpenFile`.

Real CSV export (`CTextConverter::ConvertToText`) uses each cell's
**calculated value**, not its formula — correct for a format with no
formula concept. `ReadASCD` recalculates every formula cell after
populating them, since `TryToParseString` sets a formula without
evaluating it.

```
cd translators/csv
make            # build the CsvTranslator add-on
make test       # round-trip test
make install    # copy to ~/config/non-packaged/add-ons/Translators
```

## `translators/xls` — legacy XLS import (BIFF/OLE2)

Imports the historical Excel 97-2003 binary format
(`kAtomoXlsFormat`) to ASCD, reusing the BIFF/OLE2 importer already
extracted into the engine (`CExcel5Filter`, `engine/src/Excel/`).
Import only, deliberately — export to the modern Excel ecosystem goes
through `translators/xlsx/` instead (ZIP+XML is far simpler to write
than BIFF/OLE2 from scratch). `Identify()` recognizes the format via
the standard OLE2 Compound File signature (`D0 CF 11 E0 A1 B1 1A E1`).

```
cd translators/xls
make && make test && make install
```

**Test coverage is limited**: the committed test only checks
`Identify()`'s signature handling and that `Translate()` fails cleanly
on invalid BIFF content — a valid BIFF stream isn't practical to build
by hand for a committed fixture. Manually verified against a real
`.xls` file (not redistributable, not included in the repo).

## `translators/xlsx` — XLSX import/export (Excel 2007+, OOXML)

Imports **and exports** modern XLSX (`kAtomoXlsxFormat`) to/from ASCD.
An XLSX file is a ZIP archive of XML (`xl/worksheets/sheet1.xml`,
`xl/sharedStrings.xml`, `[Content_Types].xml`).

**No new system dependencies**: reuses **expat** (XML parsing) and
**zlib** (decompression/CRC32), both already present on Haiku. No
system package ships ZIP container development headers
(`libzip`/`minizip` lack `_devel` packages), so a minimal, purpose-built
ZIP reader/writer lives in `MiniZip.h`/`.cpp` (no ZIP64/encryption
support — enough for XLSX from standard tools). `CZipWriter` (export)
writes "stored" (uncompressed) entries only, verified against system
`unzip` as well as its own reader.

XML parsing handles cell references (`r="A1"`), numeric values, shared
strings (`t="s"`) and inline strings (`t="inlineStr"`), and formulas
(`<f>...</f>`) — imported as text with a leading `=` via
`TryToParseString` so the engine recalculates them independently
instead of trusting Excel's cached `<v>`. Assumes the first sheet is
always `xl/worksheets/sheet1.xml` — a real multi-sheet mapping via
`xl/workbook.xml`/`_rels` isn't implemented for import or export.

**Export writes live formulas** (`<f>FORMULA</f>` plus the cached
`<v>`) for any formula cell that doesn't reference another sheet
(`CFormula::ReferencesOtherSheet()`, `engine/src/Formula/Formula.cpp`)
— this translator writes one sheet per file, so a cross-sheet
reference would point at data absent from the exported file; those
cells fall back to a plain calculated value, like CSV. Formula text
comes from `CFormula::UnMangle(buf, cell, doc, /*rcStyle*/false,
/*decSep*/'.', /*listSep*/',')`, which always forces canonical
ECMA-376 separators (`.`/`,`) regardless of the user's current locale
preferences — otherwise Excel couldn't parse the formula back. A
text-result formula cell also gets `t="str"`.

```
cd translators/xlsx
make && make test && make install
```

**Test**: `tests/sample.xlsx` is a real hand-built XLSX (verifiable
with `unzip -l`) with a value, a formula and a shared string — the
test imports it, confirms the formula (not its cached value) round-trips
and the engine recalculates it independently, then separately verifies
export: an ASCD document exports to XLSX and re-imports correctly
(formula-cell round-trip, cross-sheet-reference fallback, canonical
separators regardless of current locale prefs).

## `translators/ods` — ODS import/export (OpenDocument Spreadsheet)

Imports **and exports** LibreOffice/OpenOffice Calc's ODS
(`kAtomoOdsFormat`). Like XLSX, a ZIP archive of XML, but OpenDocument
schema instead of OOXML: one `content.xml` for all sheets, a
`META-INF/manifest.xml`, and an uncompressed `mimetype` entry.

Reuses `MiniZip.h`/`.cpp` (same ZIP container as XLSX) and expat, with
a dedicated OpenDocument parser. `CZipWriter` (added for export) writes
uncompressed entries and a proper central directory, verified against
system `unzip`.

**Structural difference from XLSX**: ODF cells have no explicit `r=`
reference — position is derived by counting `<table:table-row>`/
`<table:table-cell>` elements while parsing. Empty ranges are
compressed with `table:number-rows/columns-repeated="N"` (a single
element standing for N identical rows/columns); the parser advances
row/column counters by that amount but never materializes cells for
them.

**Formula syntax conversion**: OpenDocument formulas use an `of:=`
prefix and bracket-with-dot cell references (`[.A1]`, `[.$A$1]`).
`ConvertODFFormula()` strips the prefix and brackets/`$` on import.
**Known limit**: doesn't handle cross-sheet references
(`[Sheet2.A1]`) or complex bracketed ranges — enough for the common
case of same-sheet arithmetic formulas. Only the first `<table:table>`
(first sheet) is imported, same limitation as XLSX/sheet1.

**Export writes live formulas** the same way as XLSX: a
`table:formula="of:=FORMULA"` attribute for same-sheet formulas
(`CFormula::ReferencesOtherSheet()` gates it), via
`CFormula::UnMangle(buf, cell, doc, false, '.', ';', /*odfRefs*/true)`
— `odfRefs=true` wraps references in `[.`/`]`, and `decSep`/`listSep`
are always forced to `.`/`;` (OpenFormula/LibreOffice convention)
regardless of the user's current locale preferences.

```
cd translators/ods
make && make test && make install
```

**Test**: same shape as XLSX — a real hand-built `tests/sample.ods`
(verifiable with `unzip -l`), import/recalculation check, plus export
round-trip covering the cross-sheet-reference fallback and canonical
separators.

## Wiring real saves through `MainWindow::SaveToFile`

`MainWindow::SaveToFile` picks the export format from the file
extension (`.csv`/`.xlsx`/`.ods`, else native ASCD) and must route
`BTranslatorRoster` to the right translator.

```
cd ui && make test-export-formats
```

## Notable fixes found building these translators

- **Font/UI-dependent engine calls hanging headless** (all four
  translators, same root causes as `docs/ENGINE_API.md`'s "Notable
  fixes"): `CFontMetrics::StringWidth`/`operator[]`, `GetFunctionNr`
  null-table dereference, and a negative-index use in `parser.cpp` —
  all fixed in the engine itself, first surfaced by exercising it
  through a real translator instead of the isolated smoke test.
- **`CExcel5Filter::GetBookStream` declared `throw()`/`noexcept` but
  threw anyway** on malformed BIFF input, calling `std::terminate()`
  immediately and bypassing every `catch` up the stack (including one
  directly wrapping the call). Fixed by removing the `throw()`
  specifier.
- **Three bugs found opening a real `.xls` file** (never exercised
  before — only hand-built/malformed OLE2 fixtures existed): 64-bit
  `long`/`unsigned long` fields in `oleEntry`/`GetBookStream`
  misaligned OLE2 directory parsing (same `sizeof(long)` class of bug
  as elsewhere, fixed with fixed-width `int32`/`uint32`); the OLE2
  directory reader assumed a single 512-byte sector instead of
  following the FAT chain for files with more streams than that;
  several BIFF record handlers dereferenced a null `fCellView`
  (always null in this headless translator) without checking it first
  — common records in any real sheet, so this hit nearly every real
  `.xls` file.
- **Ambiguous `BTranslatorRoster` selection**: when the source is
  generic ASCD (no distinctive real-format signature), all four
  translators' `Identify()` score it identically as
  `kAtomoNativeFormat`. `BTranslatorRoster::Translate(source, info=NULL,
  ext, dest, wantOutType)` picks a translator from `Identify()` alone,
  **ignoring `wantOutType`** — it always resolves to the same
  translator regardless of the requested output format, which then
  correctly rejects an `outType` it doesn't support
  (`B_NO_TRANSLATOR`), even with the right translator installed. Fixed
  by manually enumerating `GetAllTranslators()`/`GetOutputFormats()` to
  find a translator that declares `wantOutType`, then calling the
  by-`translator_id` `Translate()` overload directly.
- **CSV/ODS rejected ASCD version 2**: `ui/src/AscdIO.cpp` (the real
  on-disk app format) always writes version 2 (a per-cell `kind` byte),
  but CSV's and ODS's own private `ReadASCD` (the intermediate format
  between `MainWindow` and the translator) only accepted version 1 —
  any real save to `.csv`/`.ods` failed with `B_MISMATCHED_VALUES`,
  invisible until `MainWindow::SaveToFile` was wired to a real
  document. Fixed with a read-version ceiling
  (`kASCDMaxReadableVersion = 2`) kept separate from `kASCDVersion`
  (still 1, what these `WriteASCD`s emit).
