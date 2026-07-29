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
