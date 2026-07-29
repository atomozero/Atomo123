# Translator Translation Kit (`translators/`)

Add-on per il Translation Kit di Haiku (`BTranslator`/`BTranslatorRoster`)
che convertono tra formati file esterni e i dati del motore di calcolo
isolato (`engine/`). Ogni translator vive nella propria cartella sotto
`translators/`, con lo stesso schema: un `BTranslator` concreto che
implementa `Identify()`/`Translate()`/`InputFormats()`/`OutputFormats()`,
un `Makefile` che lo compila come shared object linkato contro
`engine/libengine.a`, e un test che dimostra la conversione end-to-end.

## translators/csv — CSV e formato nativo ASCD

Converte tra testo CSV (`kAtomoCsvFormat`, MIME `text/csv`) e un
formato dati nativo minimo definito qui, **ASCD** ("Atomo Sheet Cell
Data", `kAtomoNativeFormat`, MIME `application/x-vnd.atomo-sheet-data`).

**ASCD è una base di partenza, non il formato documento definitivo
dell'app.** Serializza solo le celle non vuote di un `CContainer`
(riga, colonna, testo — la formula con `=` se presente, altrimenti il
valore formattato, riusando `CContainer::GetCellFormula`), preservando
le formule attraverso un round-trip nativo. Quando la Fase 4 (UI)
definirà il vero formato documento dell'app, ASCD verrà probabilmente
ripreso/esteso o sostituito — per ora serve a dimostrare in modo
concreto e testabile che l'infrastruttura translator funziona
end-to-end con il motore di calcolo isolato.

L'export verso CSV vero e proprio (`CTextConverter::ConvertToText`,
già nel motore) usa invece il **valore calcolato** delle celle, non la
formula: è il comportamento corretto per un formato che non ha alcun
concetto di formula.

### Build, test, installazione

```
cd translators/csv
make            # compila l'add-on CsvTranslator (shared object)
make test       # compila ed esegue il test di round-trip
make install    # copia l'add-on in ~/config/non-packaged/add-ons/Translators
```

Il test (`tests/test_csv_translator.cpp`) parte da un CSV di esempio,
lo traduce in ASCD, poi ritraduce l'ASCD in CSV e verifica che i
valori (numerici e testuali) sopravvivano al giro completo.

### Dipendenze di link

Oltre a `engine/libengine.a` e `libbe.so`, un add-on `BTranslator`
richiede **`libtranslation.so`** (fornisce l'implementazione base
della classe `BTranslator`/`BArchivable` — non basta `libbe.so` da
solo, si ottiene altrimenti un errore di link "undefined reference to
BTranslator::BTranslator()" e simili).

## Bug scoperti costruendo questo translator

Costruire e testare un translator concreto (non solo il motore in
isolamento, come nella Fase 2) ha fatto emergere ulteriori bug reali
del motore, tutti nella stessa famiglia "codice mai eseguito senza una
vera UI/app_server collegati". Elenco completo con dettaglio tecnico
in `docs/ENGINE_API.md`. In sintesi, in ordine di scoperta:

1. `CFontMetrics::operator[]`/`StringWidth` chiamavano
   `be_plain_font->StringWidth()` senza controllare se esisteva
   davvero un font caricato — bloccava (vedi sotto) in qualunque
   formattazione di valore numerico.
2. `CFontSizeTable::operator[]` accedeva a un `std::vector` che nella
   libreria engine resta sempre vuoto (nessuna vera view popola mai la
   tabella font) senza controllo dei limiti.
3. `GetFunctionNr` (ricerca binaria per riconoscere nomi di funzione
   nelle formule) opera su una tabella (`gFuncArrayByName`,
   `gFuncCount`) che nel codice storico viene popolata da
   `InitFunctions()` leggendo una risorsa `'Func'` — funzione mai
   chiamata nella libreria engine, quindi la tabella resta vuota/nulla:
   la ricerca dereferenzia un puntatore nullo.
4. Lo stesso identificatore di funzione, se non trovato (`-1`), veniva
   comunque usato per indicizzare `gFuncArrayByNr` **prima** di
   controllare se fosse valido — indice negativo su un array C.

**Nota importante**: i fix (2-4) rendono il codice sicuro (nessun
crash/blocco), ma **le funzioni con nome nelle formule (SOMMA, SE,
ecc.) non sono ancora realmente utilizzabili**, perché la tabella
delle funzioni non viene mai popolata: manca ancora la generazione e
il caricamento della risorsa `'Func'` (compilabile con `bsl`/`rez`,
già sistemati in Fase 1) più una chiamata a `InitFunctions()` in fase
di inizializzazione dell'engine. Questo è un gap noto, da colmare in
una fase futura prima che le formule con funzioni possano funzionare
davvero. Fino ad allora, formule con soli operatori aritmetici e
riferimenti a cella (`=A1+A2*3`) funzionano correttamente; nomi non
riconosciuti come identificatori (funzione sconosciuta o intervallo
con nome) vengono correttamente trattati come testo letterale, non
più con un blocco.

### Perché sembrava un blocco infinito invece di un crash

Diversi di questi bug sono dereferenziazioni di puntatori nulli/quasi
nulli, che normalmente terminerebbero il processo con un segmentation
fault immediato. Su questa Haiku, il processo restava invece bloccato
indefinitamente (richiedendo `kill -9` per essere terminato). L'ipotesi
più probabile è che `debug_server` di Haiku intercetti il crash e
sospenda il thread in attesa di una decisione dell'utente (debug o
termina) tramite un'interfaccia grafica — interazione che non arriva
mai in un'esecuzione da riga di comando/headless, facendo apparire il
processo bloccato piuttosto che terminato. Va tenuto presente
scrivendo altri test headless: un timeout con `timeout N ./binario` è
indispensabile per non restare bloccati indefinitamente in questi casi.

## translators/xls — import XLS legacy (BIFF/OLE2)

Importa il formato binario storico di Excel 97-2003 (`kAtomoXlsFormat`,
MIME `application/vnd.ms-excel`) verso ASCD, riusando l'importer
BIFF/OLE2 già portato ed estratto nel motore (`CExcel5Filter`,
`engine/src/Excel/`). Solo import per ora: il motore non include un
writer per il formato binario legacy (l'export verso Excel moderno
passerà dal futuro translator XLSX).

`Identify()` riconosce il formato tramite la firma standard degli OLE2
Compound File Binary (8 byte fissi: `D0 CF 11 E0 A1 B1 1A E1`), usata
da tutti i formati Microsoft Office pre-2007 (non solo XLS).

### Build, test, installazione

```
cd translators/xls
make            # compila l'add-on XlsTranslator
make test       # compila ed esegue il test
make install    # copia l'add-on in ~/config/non-packaged/add-ons/Translators
```

**Test incompleto**: il test attuale (`tests/test_xls_translator.cpp`)
verifica solo che `Identify()` riconosca/rifiuti correttamente la
firma OLE2, e che `Translate()` su un OLE2 con contenuto BIFF non
valido fallisca in modo pulito (senza bloccarsi). **Manca un test di
importazione end-to-end** con un file `.xls` reale generato da Excel o
LibreOffice Calc, che verifichi che valori e formule vengano importati
correttamente — costruire a mano un flusso BIFF valido non è
praticabile, serve un file di esempio autentico. Da aggiungere quando
disponibile (vedi nota "Test di congruità" nella Fase 3 di
`ROADMAP.md`).

### Bug scoperto: `throw()` che non manteneva la promessa

`CExcel5Filter::GetBookStream` era dichiarata `throw()` — la vecchia
sintassi C++ per dichiarare "questa funzione non lancia mai eccezioni"
(equivalente a `noexcept` in C++17, standard con cui questo progetto
compila). Il suo corpo però chiama `CExcelStream::Read`, che lancia
`CErr` quando lo stream non ha abbastanza dati validi — esattamente
quello che succede aprendo un file OLE2 con contenuto BIFF non valido
o troncato (comportamento assolutamente normale da gestire, non un
caso limite raro).

Una funzione dichiarata `noexcept`/`throw()` che lancia comunque
un'eccezione fa chiamare **`std::terminate()` immediatamente**,
bypassando ogni blocco `try`/`catch` più in alto nella catena di
chiamate — anche quando quel `catch` avvolge direttamente la chiamata
alla funzione che viola la promessa. Sintomo osservato: il translator
XLS terminava il processo ("terminate called after throwing an
instance of 'CErr'") nonostante `Translate()` avesse un
`catch (...)` che avvolgeva correttamente la costruzione di
`CExcel5Filter`. **Fix**: rimossa la specifica `throw()`, sia nella
dichiarazione (`Excel.h`) sia nella definizione (`Excel.OLE2.cpp`).

Questo bug non ha nulla a che fare con l'headless/app_server (a
differenza degli altri bug di questa sessione) — è un problema di
correttezza C++ puro, preesistente nel codice storico ma mai
manifestatosi finché nessuno aveva testato l'importer con un file
realmente malformato/incompleto.
