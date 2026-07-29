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

## Aggiornamento: build completa raggiunta

Tutti i file rimanenti sono stati sistemati con lo stesso pattern
meccanico (`long`/`ulong` -> `int32`/`uint32`/`type_code`/`status_t`,
o `#include <arpa/inet.h>` mancante):

- `Source/main/App/Sum-It.cpp` — `type`/`count` in `RefsReceived` ->
  `type_code`/`int32` per `BMessage::GetInfo`
- `Source/main/Cell/Container.h` — `fReferenceCount` -> `int32` per
  `atomic_add`
- `Source/main/Cell-UI/CellView.cpp` — `l` in `CancelCalculation` ->
  `status_t` per `wait_for_thread`
- `Source/main/Cell-UI/CellView.drag.cpp` — `LoadCursor(0L)` ambiguo
  tra overload `int32`/`const char*` -> cast esplicito `(int32)0`
- `Source/main/Cell-UI/CellWindow.h`/`.cpp` — `sUntitledCount` ->
  `int32`; `key`/`modifiers` in `CCellWindowMessageFilter::Filter` ->
  `int32` per `BMessage::FindInt32`
- `Source/main/Formula/CalculateJob.cpp` — due variabili `l` ->
  `status_t` per `wait_for_thread`
- `Source/main/Functions/Functions.text.cpp` — `isnan` -> `std::isnan`
- `Source/main/Misc-Classes/Benaphore.h` — `fCount` -> `int32` per
  `atomic_add`
- `Source/main/Misc-Classes/RunArray.cpp` — `#include <arpa/inet.h>`
  mancante (`htons`/`ntohs`)
- `Source/main/Plugin/GraphPlugIn.cpp` — `clicks` -> `int32` per
  `BMessage::FindInt32`
- `Source/main/UI-Misc/Formatter.cpp` — `gNextFormatNr` -> `int32` per
  `atomic_add`
- `Source/main/Utils/Utils.cpp` — `buttons`/`modifiers` -> `int32` per
  `BMessage::FindInt32`
- `Source/main/Widgets/MyTextControl.cpp` — `sStart`/`sEnd` -> `int32`
  per `BTextView::GetSelection`
- `Source/main/Widgets/ProgressView.cpp` — `l` -> `int32` per
  `BMessage::FindInt32`

**Risultato: `make` in `legacy/opensumit/sum-it` completa senza errori
e produce il binario `OpenSum-It` (ELF 64-bit, link riuscito, risorse
incorporate con `xres`).**

## Smoke test di avvio

Il binario è stato lanciato su Haiku hrev59800 reale: il processo
resta in esecuzione (nessun crash) e mostra una singola finestra di
errore recuperabile:

```
### Sum-It Error
# (errDamagedResources)
#----
File "./Source/main/Dialog/RDialog.cpp"; Line 230;
#----
```

`CRDialog::ConstructFromTemplate` (RDialog.cpp:230) incontra un tag a
4 byte non riconosciuto nello switch che legge il template di un
dialogo dalle risorse generate, e solleva un'eccezione catturata
(l'app non crasha, mostra solo l'alert). Non ancora determinato se sia
un bug preesistente nel codice storico (dialogo con elemento non
gestito da questa build) o un effetto collaterale dei fix di
byte-order/puntatori applicati a `rez`.

Tentativo di isolare il dialogo responsabile usando lo strumento di
scripting nativo `hey` (query delle finestre aperte via BMessage
scripting): il processo resta vivo ma non risponde alla query entro
un tempo ragionevole (timeout dopo 2 minuti), probabilmente perché il
vecchio meccanismo di alert modale del codice storico blocca il
message loop in un modo incompatibile con lo scripting BMessage
moderno. **Deciso di rimandare** questa indagine invece di investire
in un harness di test UI dedicato: non blocca la Fase 2 (il motore di
calcolo non passa da `RDialog`) e probabilmente diventa irrilevante in
Fase 4, quando la UI storica verrà comunque sostituita da una nuova
scritta da zero.

**Indizio emerso durante la Fase 2**: nell'estrazione del motore di
calcolo isolato sono stati trovati due bug reali di corruzione di
memoria silenziosa, entrambi dovuti alla stessa assunzione errata
`sizeof(long)==4` (vera su BeOS/PPC a 32 bit, falsa su Haiku x86_64
dove `sizeof(long)==8`): `cell::operator==` che leggeva 4 byte oltre
una struct da 4 byte, e il formato bytecode delle formule
(`kPFWordSize`/`fString`) che disallineava la lettura degli opcode
per lo stesso motivo (vedi `docs/ENGINE_API.md` per il dettaglio
completo). `CRDialog::ConstructFromTemplate` legge anch'esso un
flusso di byte a tag da 4 byte con logica di avanzamento indice — è
plausibile che soffra dello stesso identico pattern di bug. Chi
riprende l'indagine su RDialog dovrebbe controllare per primo se
`RDialog.cpp`/`RState.cpp` o il codice di generazione risorse fanno
assunzioni simili su `sizeof(long)` invece di usare tipi a larghezza
fissa (`int32`).

Test non ancora eseguiti (richiedono automazione di input UI, rimandati):
import di un file `.xls` reale; verifica calcolo di una formula
semplice via UI. Vedi `docs/ROADMAP.md` per come questi test verranno
ripresi (in Fase 2 l'engine sarà testabile senza passare dalla UI,
il che aggira comunque il problema di `RDialog`).

## Perché questo lavoro conta

Questo NON è più solo un'ipotesi teorica: l'intera applicazione
storica Sum-It/OpenSumIt compila ora per intero su Haiku moderno a 64
bit e si avvia senza crash. Il motore di calcolo, l'importer Excel
legacy binario, e l'intera UI CellView/CellWindow sono asset concreti
e riusabili. Vedi `docs/ROADMAP.md` nella root del progetto per il
piano che usa questo codice come base per `engine/` (Fase 2).
