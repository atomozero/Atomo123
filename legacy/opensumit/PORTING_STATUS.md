# Stato del porting di OpenSumIt su Haiku a 64 bit

Origine: fork community `github.com/beos-zealot/OpenSumIt` del progetto storico
Sum-It (Maarten Hekkelman / Hekkelman Programmatuur B.V., 1996-2000).
Licenza originale: BSD a 4 clausole (con advertising clause) — vedi
`sum-it/Docs/Licence` e `sum-it/Docs/Copyright`.

Testato su: Haiku hrev59800, GCC 13.3.0, x86_64.

## bsl (BeOS String List tool)
Compila senza modifiche (solo warning innocui).

## rez (compilatore risorse legacy in stile CodeWarrior)
Compilava un tempo su BeOS/PPC a 32 bit. Su Haiku moderno a 64 bit falliva
per due motivi distinti, ora corretti:

1. **Bug nel Makefile**: `Build/Makefile.main` rinominava
   (`mv`) `rez_parser.hpp` in `rez_parser.cpp.h` dopo la generazione bison,
   ma il file `.cpp` generato include ancora il nome originale
   `rez_parser.hpp`. Fix: `mv` -> `cp`.

2. **Troncamento puntatore a 32 bit**: la grammatica (`rez_parser.y`) e lo
   scanner (`rez_scanner.l`) salvano puntatori (RSArray*, RSymbol*, ecc.)
   dentro lo slot generico di valore semantico del parser (`YYSTYPE`),
   usando cast espliciti `(int)ptr`. Su BeOS/PPC a 32 bit un puntatore
   stava in un `int` (4 byte); su Haiku x86_64 un puntatore è a 8 byte e
   il cast tronca l'indirizzo, corrompendo la memoria.
   `RTypes.h` definiva già correttamente `#define YYSTYPE long` (pensato
   evidentemente per essere pointer-safe), ma i cast `(int)` bypassavano
   comunque quella protezione.
   Fix applicato: sostituiti tutti i cast `(int)` con `(long)` in
   `rez_parser.y` e `rez_scanner.l`; allargati a `long` i tipi che
   veicolano questi valori lungo la catena (`RState::Shift`,
   `RElem::FindIdentifier`, `RSValue::fValue`, `RSValue::ResolveIdentifier`,
   `intmap`, `ResHeader`, `WriteResource`).
   Aggiunto anche `#include <arpa/inet.h>` dove mancava (`ntohl`/`htonl`
   non più dichiarati implicitamente dagli header Haiku moderni).

Risultato: `rez` ora compila senza warning di troncamento puntatore,
è linkato, e genera correttamente tutte le risorse (`.r`/`.rdef`) del
progetto `sum-it` (icone, menu, dialoghi, mime type sniffer).

## sum-it (l'applicazione vera e propria)
Con `bsl` e `rez` funzionanti, la build procede oltre la generazione
risorse e compila i sorgenti C++. Verificato che compilano puliti (con
piccoli fix):
- `Formula.cpp` (motore di calcolo core): fix `isnan` -> `std::isnan`
- `Excel.cpp` + `Excel.OLE2/formula/pass1/pass2.cpp` (import XLS legacy):
  fix `#include <arpa/inet.h>` mancante in `FileFormat.h`
- `Huffman.cpp` (compressione): stesso fix arpa/inet.h
- `MThread.cpp`: `long ignore` -> `status_t ignore` (BeOS R5 status_t
  era `long`, su Haiku è `int32`)
- ColorPicker/CellView/Graphic/SelectionView: `ulong btns/buttons` ->
  `uint32` per matchare la firma moderna di `BView::GetMouse()`

Test aggregato su 132 file .cpp di `sum-it/Source` con `-fsyntax-only`
dopo i primi 2 fix minimi (isnan, arpa/inet.h): **110/132 già passano**.
I 22 rimanenti falliscono quasi tutti per la STESSA classe di problema
meccanico: variabili `long`/`ulong` di stile BeOS R5 usate dove le API
Haiku moderne vogliono tipi a larghezza fissa (`int32`, `uint32`,
`type_code`, `status_t`). Non è un problema architetturale: è lavoro
meccanico, file per file.

## File ancora da sistemare (elenco al momento dello snapshot)
Source/ColorPicker/ColorSlider.cpp (parziale, GetMouse fixato)
Source/ColorPicker/HSVView.cpp (parziale, GetMouse fixato)
Source/Huffman/Huffman.cpp (FIXATO)
Source/Metrowerks/MThread.cpp (FIXATO)
Source/main/App/Sum-It.cpp (in corso: BMessage::GetInfo firma)
Source/main/Cell/Container.cpp
Source/main/Cell-UI/CellView.cpp (parziale, GetMouse fixato)
Source/main/Cell-UI/CellView.drag.cpp
Source/main/Cell-UI/CellView.mouse.cpp (parziale, GetMouse fixato)
Source/main/Cell-UI/CellView.resizing.cpp (parziale, GetMouse fixato)
Source/main/Cell-UI/CellWindow.cpp
Source/main/Formula/CalculateJob.cpp
Source/main/Functions/Functions.text.cpp
Source/main/Misc-Classes/Benaphore.cpp
Source/main/Misc-Classes/RunArray.cpp
Source/main/Plugin/GraphPlugIn.cpp
Source/main/UI-Misc/Formatter.cpp
Source/main/UI-Misc/Graphic.cpp (parziale, GetMouse fixato)
Source/main/Utils/Utils.cpp
Source/main/Widgets/MyTextControl.cpp
Source/main/Widgets/ProgressView.cpp
Source/main/Widgets/SelectionView.cpp (parziale, GetMouse fixato)

## Perché questo lavoro conta
Questo NON è più solo un'ipotesi teorica: il motore di calcolo
(parser formule, grafo celle) e l'importer Excel legacy binario
compilano già puliti su Haiku moderno con fix minimi. Sono asset
concreti e riusabili, non da riscrivere da zero. Vedi `docs/ROADMAP.md`
nella root del progetto per il piano che usa questo codice come base
per `engine/`.
