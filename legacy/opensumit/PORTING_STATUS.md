# Porting OpenSumIt to 64-bit Haiku

Source: community fork `github.com/beos-zealot/OpenSumIt` of the
historical Sum-It project (Maarten Hekkelman / Hekkelman Programmatuur
B.V., 1996-2000). Original license: 4-clause BSD with advertising
clause — see `sum-it/Docs/Licence`/`Docs/Copyright`.

Tested on: Haiku hrev59800, GCC 13.3.0, x86_64.

## `bsl` (BeOS String List tool)

Builds unmodified (harmless warnings only).

## `rez` (legacy CodeWarrior-style resource compiler)

Built on 32-bit BeOS/PPC; failed on 64-bit Haiku for two reasons, both
fixed:

1. **Makefile bug**: `Build/Makefile.main` renamed (`mv`) the
   bison-generated `rez_parser.hpp` to `rez_parser.cpp.h`, but the
   generated `.cpp` still `#include`s the original name. Fixed by using
   `cp` instead of `mv`.
2. **32-bit pointer truncation**: the grammar/scanner stored pointers
   in the parser's generic semantic-value slot via explicit `(int)ptr`
   casts — safe when a pointer fit in 4 bytes (BeOS/PPC), corrupting on
   Haiku x86_64 where pointers are 8 bytes. `YYSTYPE` was already
   correctly defined as `long`, but the `(int)` casts bypassed that.
   Fixed by changing all such casts to `(long)` and widening the types
   that carry these values along the chain.

Result: `rez` now builds without pointer-truncation warnings and
correctly generates all of `sum-it`'s resources.

## `sum-it` (the application itself)

With `bsl`/`rez` working, the C++ sources build with small fixes —
almost entirely the same mechanical pattern: BeOS R5-style `long`/
`ulong` variables where modern Haiku APIs expect fixed-width types
(`int32`, `uint32`, `type_code`, `status_t`), plus a few missing
`#include <arpa/inet.h>` (`htonl`/`ntohl` no longer implicitly declared
by modern Haiku headers). A first pass with just those two fixes
already got 110 of 132 `.cpp` files in `sum-it/Source` past
`-fsyntax-only`; the rest needed the same pattern file by file —
e.g. `Container.h`'s `fReferenceCount` for `atomic_add`, `CellView.cpp`'s
thread-wait status variables, `Utils.cpp`'s `BMessage::FindInt32`
targets. Not an architectural problem, just mechanical, file-by-file
work.

**Result: `make` in `legacy/opensumit/sum-it` completes without errors
and produces the `OpenSum-It` binary** (64-bit ELF, resources embedded
via `xres`).

## Startup smoke test

Launched on real Haiku hrev59800: the process stays running (no crash)
but shows one recoverable error window:

```
### Sum-It Error
# (errDamagedResources)
#----
File "./Source/main/Dialog/RDialog.cpp"; Line 230;
#----
```

`CRDialog::ConstructFromTemplate` hits an unrecognized 4-byte tag while
reading a dialog template from generated resources, and throws a
caught exception (no crash, just the alert). Not yet determined
whether this is a pre-existing bug in a dialog this build never
exercised, or a side effect of the `rez` byte-order/pointer fixes.

**Likely cause, found later during the engine extraction (Phase 2)**:
two real silent memory-corruption bugs were found there from the same
`sizeof(long)==4` assumption (true on BeOS/PPC, false on Haiku
x86_64) — see `docs/ENGINE_API.md`. `RDialog::ConstructFromTemplate`
reads a similar 4-byte-tagged byte stream with index-advancing logic,
so it plausibly has the same bug class. Not pursued further: it
doesn't block Phase 2 (the calculation engine never goes through
`RDialog`), and becomes moot once the UI is replaced from scratch in
Phase 4.

Import of a real `.xls` file and formula-calculation-via-UI checks
were deferred — the isolated engine (Phase 2) is testable without going
through the UI at all, sidestepping the `RDialog` issue entirely.

## Why this matters

This isn't just a theoretical port anymore — the entire historical
Sum-It/OpenSumIt application builds on modern 64-bit Haiku and starts
without crashing. The calculation engine, the legacy binary Excel
importer, and the full CellView/CellWindow UI are concrete, reusable
assets. See `ROADMAP.md` for how this codebase became the basis for
`engine/` (Phase 2).
