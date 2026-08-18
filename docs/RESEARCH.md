# Initial technical research

Summary of the multi-source research (cross-checked with an adversarial
3-vote review process) done before starting the project.

## 1. Native Haiku APIs

| Kit | Role |
|---|---|
| Interface Kit + Layout Kit | Cell grid, toolbar, editing, dialogs |
| Application Kit | Lifecycle, `BMessage` for commands/undo-redo |
| Storage Kit | File I/O, BFS extended attributes for metadata |
| Locale Kit | Locale-aware number/currency/date formatting |
| Translation Kit | Plugin-based import/export (see below) |
| Print Kit | Printing/preview |

### Translation Kit

Documented as a generic data-conversion framework, not limited to
images (the historical BeBook cites a word processor using it for
HTML/PostScript/ASCII). `BTranslator` is an abstract class
(`Identify()`, `Translate()`, `InputFormats()`, `OutputFormats()`);
`BTranslatorRoster` discovers and loads add-ons from
`/system/add-ons/Translators`, `~/config/add-ons/Translators`, or a
private path — an app can ship its own translators without a
system-wide install. No built-in "spreadsheet" format group: a
custom XLSX/ODS translator registers its own MIME type.

## 2. The historical Sum-It project

- Origin: Maarten Hekkelman / Hekkelman Programmatuur B.V., BeOS,
  1996-2000. Development stopped, released as free software.
- Survives as the community fork `github.com/beos-zealot/OpenSumIt`
  (not adopted into HaikuArchives — checked directly, 404).
- License: 4-clause BSD with an advertising clause (verified by
  reading `sum-it/Docs/Licence`/`Docs/Copyright` in the repo — not
  generic "BSD" as initially assumed).
- Empirical build check (see `legacy/opensumit/PORTING_STATUS.md`):
  the calculation engine and legacy Excel importer are solid and
  portable with targeted fixes; the `rez` build tool had a 32-bit
  pointer-truncation bug, since fixed.

## 3. C/C++ libraries for file compatibility

| Library | Language | License | Read | Write | Notes |
|---|---|---|---|---|---|
| OpenXLSX | C++17 | BSD-3 | yes | yes | no charts/tables/hyperlinks; in-memory DOM |
| libxlsxwriter | ANSI C | FreeBSD | no | yes | write-only, explicitly |
| xlnt | C++14+ | MIT | partial | partial | XLSX only |
| liborcus | C++ | MPL | yes (WIP) | limited | Document Liberation Project, built for LibreOffice |

No single library covers the full read+write cycle with complete
features — would need integrating several libraries or custom
development for charts/tables/hyperlinks/legacy XLS. (In the end,
Atomo123 wrote its own minimal ZIP/XML translators instead — see
`docs/TRANSLATORS.md`.)

### Precedent: porting LibreOffice to Haiku (FOSDEM 2018)

Document Liberation Project libraries built "out of the box" on Haiku
thanks to POSIX compatibility (one minor exception: `libmwaw`/xattr).
The real historical bottleneck was integrating LibreOffice's own UI
toolkit (VCL), not file-format parsing — confirming that writing a
native Interface/Layout Kit UI (instead of porting a foreign toolkit)
was the right call here too.

## 4. Architectural recommendation

Separate a core calculation engine (formula parser + cell dependency
graph, testable in isolation) from the native Haiku UI, with the
Translation Kit as the file-format plugin layer. See `ROADMAP.md` for
how this played out in practice.
