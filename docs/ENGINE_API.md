# Motore di calcolo isolato (`engine/`)

Libreria statica (`libengine.a`) che estrae il motore di calcolo e
l'importer Excel legacy dal codice storico Sum-It/OpenSumIt
(`legacy/opensumit/`), disaccoppiati dalla UI BeOS-era (`CCellView`/
`CCellWindow`). Compila e funziona **senza alcuna dipendenza
dall'Interface Kit** (nessun link a `BView`/`BWindow`), verificato con:

```
nm -u libengine.a | c++filt | grep -oE "\bB[A-Z][a-zA-Z]*::" | sort -u
```

che riporta solo classi Application/Storage/Support Kit (`BList`,
`BLocker`, `BLooper`, `BMallocIO`, `BMessage`, `BPath`, `BPositionIO`,
`BResources`, `BFont`) più una eccezione nota (`BAlert`, vedi
"Limitazioni note" sotto).

## Build e test

```
cd engine
make          # produce libengine.a
make test     # compila ed esegue tests/smoke_test.cpp
```

`tests/smoke_test.cpp` crea un documento (`CContainer`) senza alcuna
view collegata, inserisce valori e formule (`=A1+A2+A3`, `=A1*3`) e
verifica che i risultati calcolati siano corretti — dimostra che il
motore funziona headless.

## Mappa dei file

| Cartella | Contenuto | Note |
|---|---|---|
| `src/Cell/` | `Cell`, `CellData`, `CellIterator`, `CellParser`, `CellStyle`, `CellUtils`, `Container*`, `Range`, `Value`, `Formatter*`, `FontMetrics`, `FontStyle` | Modello dati (celle, stili, formattazione valori). Esclusi `CellCommands.*` (comandi undo/redo legati a `CCellView`) e `CellScrollBar.*` (widget UI) |
| `src/Formula/` | `CalcLooper`, `CalcStack`, `Formula*`, `lexer`, `parser` | Parser e motore di valutazione formule, grafo di dipendenze. Escluso `CalculateJob.*` (wrapper di threading/progress-bar UI-side) |
| `src/Functions/` | Tutte le funzioni foglio di calcolo (`Functions.*.cpp`) | Le funzioni `NUMPAGES`/`PAGE`/`DOCUMENT` (legate a stampa/finestra) sono già scritte con pattern null-safe nel codice originale e restituiscono NaN in assenza di una view |
| `src/Excel/` | Importer XLS legacy (BIFF/OLE2) | `Excel.pass1.cpp` scrive named range/larghezze colonna/altezze riga sulla (ex) `CCellView` — vedi limitazione sotto |
| `src/FileSys/` | `FileFormat`, `Text2Cells` | Import/export testo, formati file base |
| `src/Collections/`, `src/Misc-Classes/`, `src/Utils/`, `src/Metrowerks/` | Strutture dati generiche, gestione errori, stringhe, threading (`MThread`), locking (`StLocker`) | `Utils.cpp` privato della sola parte di costruzione menu (`BuildMenu`/`GetMenu`/`GetMBarHeight`, Interface Kit); escluso `DrawUtils.*` (disegno UI) |
| `src/Config/` | `Constants.h`, `Globals.h`, `Config.h`, `EngineGlobals.cpp` | `EngineGlobals.cpp` è nuovo: definisce le variabili globali (separatori locale, sentinelle NaN) che nel codice storico venivano inizializzate in `Sum-It.cpp` (l'app, non parte dell'engine) |
| `src/bsl/` | Header con ID delle stringhe risorsa (`errmsg.h` ecc.) | Generati un tempo dal tool `bsl`, qui solo le costanti numeriche |
| `src/Stubs/` | `EngineViewStub.h`, `ProgressStub.h` | Vedi sotto |

## Stub per rimpiazzare la UI storica

Il codice storico passa spesso un puntatore opzionale a `CCellView`
per notificare la UI (ridisegno, selezione, progress bar) durante
operazioni sul modello. Nella libreria engine questo puntatore è
sempre `NULL`; gli stub esistono solo per soddisfare il compilatore
nei rami di codice raggiungibili quando il puntatore non è nullo (mai
a runtime, nella libreria engine):

- `EngineViewStub.h` — sostituisce `CCellView`: metodi di ridisegno/
  selezione no-op, `GetHeights()`/`GetWidths()` restituiscono un
  `CRunArray` locale scartato, `IsNamedRange()` restituisce sempre
  `false`.
- `ProgressStub.h` — sostituisce `StProgress` (Widgets/ProgressView.h,
  che disegna una vera progress bar `BView`-based): tutti i metodi
  sono no-op.

## Bug reali trovati durante l'estrazione (non solo porting meccanico)

Oltre ai fix meccanici già documentati in
`legacy/opensumit/PORTING_STATUS.md` (variabili `long`/`ulong` BeOS R5
da allargare a `int32`/`uint32`), l'isolamento dell'engine ha fatto
emergere bug di **corruzione di memoria silenziosa** dovuti ad
assunzioni implicite su `sizeof(long)==4` (vere su BeOS/PPC a 32 bit,
false su Haiku x86_64 dove `sizeof(long)==8`):

1. **`cell::operator==/!=/</<=`** (`Cell.h`): confrontavano
   `*((long*)this)` con `*((long*)&altro)`, trattando la struct `cell`
   (due `short`, 4 byte) come se fosse grande quanto un `long`. Su
   64 bit questo legge 4 byte oltre la struct (undefined behavior),
   rendendo instabile l'ordinamento di `std::map<cell,CellData>` usato
   per tutte le celle del foglio: le celle sembravano inserirsi
   correttamente ma sparivano dalla lookup successiva. **Fix**:
   confronto diretto dei campi `v`/`h`.

2. **`CFontMetrics::operator==`** (`FontMetrics.cpp`): stesso pattern
   su `rgb_color` (struct da 4 byte). **Fix**: `memcmp` sulla
   dimensione reale della struct.

3. **Formato bytecode delle formule compilate** (`kPFWordSize`,
   `Formula.h`/`.cpp`/`.IO.cpp`/`.iter.cpp`): l'intero formato
   "postfix" con cui una formula compilata viene salvata (`fString`,
   un array il cui tipo di elemento doveva rappresentare una "parola"
   da 4 byte) era dichiarato `long *fString` con
   `kPFWordSize = sizeof(long)`. Su BeOS a 32 bit coincideva (4 byte).
   Su Haiku x86_64, `sizeof(long)==8`: il calcolo
   `sizeof(cell) / kPFWordSize` (per sapere quante "parole" occupa un
   riferimento di cella nel flusso di byte) diventava `4/8=0` per
   troncamento intero, quindi l'indice di lettura non avanzava più
   leggendo un riferimento a cella, disallineando la lettura di *tutti*
   gli opcode successivi. Sintomo osservato: qualunque formula con un
   riferimento a cella (es. `=A1+A2+A3`) falliva con
   `errIllPFString` ("A strange and unknown error occured"). **Fix**:
   `fString` ridichiarato `int32 *` (tipo a larghezza fissa, sempre
   4 byte su qualunque piattaforma) e `kPFWordSize = sizeof(int32)`,
   con tutti i cast/allocazioni (`Formula.cpp`, `Formula.IO.cpp`,
   `Formula.iter.cpp`) aggiornati di conseguenza.

Questa classe di bug (assumere `sizeof(long)==4` per impacchettare
dati binari) è probabilmente presente anche altrove nel codice non
ancora isolato/testato in `legacy/opensumit/sum-it` — vedi nota in
`legacy/opensumit/PORTING_STATUS.md`.

## Bug trovati costruendo il primo translator concreto (Fase 3)

Testare il motore attraverso un vero translator (`translators/csv/`,
non solo tramite `engine/tests/smoke_test.cpp`) ha esercitato percorsi
di codice mai toccati prima (formattazione di valori, riconoscimento
di identificatori nelle formule), facendo emergere altri bug reali
della stessa famiglia "codice che assume una vera UI/app_server
collegati":

4. **`CFontMetrics::operator[]`/`StringWidth`** (`FontMetrics.cpp`):
   quando non c'è un font reale caricato (`fFontStyle == NULL`, sempre
   vero nella libreria engine), chiamavano `be_plain_font->StringWidth()`
   — richiede una connessione all'app_server, che nella libreria
   engine non esiste: la chiamata si bloccava indefinitamente. **Fix**:
   fallback fisso (8 pixel/carattere) quando non c'è un font reale.

5. **`CFontSizeTable::operator[]`** (`FontMetrics.h`): accede a
   `fFonts[indx]`, uno `std::vector` che nella libreria engine resta
   sempre vuoto (nessuna vera view popola mai la tabella font tramite
   `GetFontID`) — un accesso fuori dai limiti su vettore vuoto è
   undefined behavior. **Fix**: bound-check che restituisce un
   `CFontMetrics` di default (già sicuro grazie al fix precedente)
   quando l'indice non è valido.

6. **`GetFunctionNr`** (`Utils.cpp`): esegue una ricerca binaria su
   `gFuncArrayByName`/`gFuncCount` per riconoscere nomi di funzione
   nelle formule (es. per distinguere `SOMMA(...)` da un riferimento a
   intervallo con nome). Questa tabella viene normalmente popolata da
   `InitFunctions()`, che legge una risorsa `'Func'` compilata dal
   testo in `Resources/funcs_by_nr.txt` — funzione mai chiamata nella
   libreria engine (nessuna risorsa allegata), quindi `gFuncCount`
   resta `0` e i puntatori restano nulli. Con `gFuncCount=0`,
   `R = gFuncCount - 1 = -1`, e il ciclo `do...while` della ricerca
   binaria esegue comunque almeno un'iterazione dereferenziando
   `gFuncArrayByName[0]` (puntatore nullo). **Fix**: guardia esplicita
   `if (gFuncCount <= 0) return -1;` in testa alla funzione, che tratta
   "tabella non caricata" allo stesso modo di "nome non trovato" (dato
   che è comunque vero che nessuna funzione è riconoscibile in
   quello stato).

7. **`parser.cpp`** (`Factor()`, caso `IDENT`): anche dopo il fix
   precedente, `GetFunctionNr` può legittimamente restituire `-1`
   (nome non riconosciuto) — ma il codice usava subito quel valore per
   indicizzare `gFuncArrayByNr[fcd.funcNr]` **prima** di controllare
   se fosse `>= 0`, cioè con un indice negativo su un array C
   (`FuncRec*` grezzo, non un container con bound-check). **Fix**:
   calcolare `expectedArgs` solo se `fcd.funcNr >= 0`, altrimenti `-1`
   (valore sentinella già usato altrove nel codice per "numero di
   argomenti sconosciuto").

**Nota aperta importante**: i fix 6-7 eliminano il crash/blocco, ma
non risolvono il problema di fondo — **la libreria engine non carica
mai la tabella delle funzioni**, quindi le formule con funzioni con
nome (`SOMMA`, `SE`, `MEDIA`, ecc.) non sono ancora realmente
utilizzabili: vengono correttamente trattate come "identificatore
sconosciuto" (e quindi come testo letterale se non c'è un `=` iniziale,
o come errore di formula se c'è). Serve, in una fase futura: generare
la risorsa `'Func'` con gli strumenti già pronti dalla Fase 1
(`bsl`/`rez` su `Resources/funcs_by_nr.txt`), allegarla al binario che
usa l'engine (o caricarla da un percorso noto), e chiamare
`InitFunctions()` una volta all'avvio.

**Perché sembravano blocchi infiniti invece di crash**: i bug 6-7 (e
altri di questa sessione) sono dereferenziazioni di puntatori nulli,
che normalmente causerebbero un crash immediato (SIGSEGV). Su questa
Haiku il processo restava invece bloccato, richiedendo `kill -9` per
essere terminato. L'ipotesi più probabile è che `debug_server`
intercetti il crash e sospenda il thread in attesa di
un'interazione utente (debug/termina) tramite interfaccia grafica —
interazione che non arriva mai in un'esecuzione headless da riga di
comando. Per questo ogni test headless in questo progetto va sempre
lanciato con `timeout N ./binario`.

## Bug trovato costruendo il translator XLS legacy (Fase 3)

8. **`CExcel5Filter::GetBookStream`** (`Excel.h`/`Excel.OLE2.cpp`):
   dichiarata `throw()` (nessuna eccezione permessa — equivalente a
   `noexcept` in C++17, lo standard usato da questo progetto), ma il
   suo corpo chiama `CExcelStream::Read`, che lancia `CErr` quando lo
   stream non ha abbastanza dati validi (caso normale con un file XLS
   malformato o troncato, non un caso limite raro). Una funzione
   `noexcept`/`throw()` che lancia comunque fa chiamare
   **`std::terminate()` immediatamente**, bypassando qualunque
   `try`/`catch` più in alto nella catena di chiamate — anche un
   `catch(...)` che avvolge direttamente la chiamata. Sintomo: il
   translator XLS terminava il processo con "terminate called after
   throwing an instance of 'CErr'" nonostante `Translate()` avesse un
   `catch(...)` attorno alla costruzione di `CExcel5Filter`. **Fix**:
   rimossa la specifica `throw()` da dichiarazione e definizione. A
   differenza degli altri bug di questa sessione, non è legato
   all'assenza di app_server/UI: è un problema di correttezza C++
   puro, preesistente ma mai manifestatosi prima di testare
   l'importer con dati realmente malformati.

## Limitazioni note

- **Alert di errore reali**: `CErr::DoError()` (`MyError.cpp`) crea
  una vera `BAlert` (`MStopAlert`/`MWarningAlert`, Metrowerks/MAlert.*)
  per mostrare gli errori. In un contesto headless senza
  `BApplication` questo potrebbe non funzionare come atteso (o
  bloccarsi). Gli errori nell'engine vanno gestiti tramite le
  eccezioni C++ (`CErr`) che l'engine già lancia — il chiamante
  headless deve intercettarle con un `try/catch` invece di lasciare
  che arrivino a `DoError()`. Andrebbe rivisto perché l'engine non
  apra mai finestre da solo (separare "genera errore" da "mostra
  errore" — compito della UI, Fase 4).
- **Named range non risolti headless**: `EngineViewStub::IsNamedRange`
  restituisce sempre `false`, e l'import Excel (`Excel.pass1.cpp`)
  scrive nomi di intervallo, larghezze colonna e altezze riga
  direttamente sulla (ex) `CCellView` invece che su `CContainer`. In
  modalità headless questi dati vengono scartati in silenzio. Fix
  corretto: spostare questi metadati sul modello (`CContainer`), non
  sulla view — lavoro per una fase successiva.
- **Formattazione display**: l'engine include `Formatter`/
  `FontMetrics` per la formattazione numerica/valuta/data (necessaria
  perché intrecciata con `CellStyle`), ma il metodo che disegna
  effettivamente testo colorato su una `BView` reale
  (`CFontMetrics::SetFontSizeColor`) è stato reso no-op: il disegno a
  schermo resta compito della UI (Fase 4).
