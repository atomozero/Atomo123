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
concetto di formula. Perché questo funzioni davvero quando la sorgente
è ASCD (es. "Salva con nome" dell'app che esporta in `.csv`),
`ReadASCD` deve ricalcolare tutte le celle con formula dopo averle
popolate — `TryToParseString` imposta la formula ma non la calcola,
un bug reale scoperto costruendo l'export CSV dall'app (vedi
`docs/UI_ARCHITECTURE.md`, sezione "Export CSV e bug scoperto").

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

**Nota**: i fix (2-4) rendevano il codice sicuro (nessun
crash/blocco), ma a quel tempo le funzioni con nome nelle formule
(`SUM`, `IF`, ecc.) non erano ancora realmente utilizzabili, perché la
tabella delle funzioni non veniva mai popolata. Risolto in Fase 6
generando e caricando la risorsa `'Func'` con `bsl`/`rez` (vedi
`docs/ENGINE_API.md`, sezione "Funzioni con nome nelle formule") —
formule come `=SUM(A1:A3)` o `=IF(A1>5;100;200)` funzionano ora
correttamente. Un identificatore ancora non riconosciuto (funzione
sconosciuta o intervallo con nome) continua a essere trattato come
testo letterale, non con un blocco.

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

## translators/xlsx — import/export XLSX (Excel 2007+, OOXML)

Importa **e ora anche esporta** il formato XLSX moderno
(`kAtomoXlsxFormat`, MIME
`application/vnd.openxmlformats-officedocument.spreadsheetml.sheet`)
da/verso ASCD. Un file XLSX è semplicemente un archivio ZIP contenente
XML (`xl/worksheets/sheet1.xml` per i dati del foglio,
`xl/sharedStrings.xml` per la tabella di stringhe condivise,
`[Content_Types].xml` come marcatore del formato).

**Nessuna nuova dipendenza di sistema**: invece di scrivere da zero un
parser XML o affidarsi a `OpenXLSX` (valutato nella ricerca iniziale
ma con dipendenze non ancora verificate su Haiku), si riusano due
librerie già presenti su questo sistema (header e binari installati):
**expat** (parser XML leggero, C) per l'XML e **zlib** (già usato
altrove nell'ecosistema Haiku) per la decompressione/CRC32. Per il
contenitore ZIP vero e proprio (non solo la compressione, ma la
struttura dell'archivio: central directory, header locali) non c'è
una libreria con gli header di sviluppo già installati (`libzip`/
`minizip` mancano dei pacchetti `_devel`); invece di richiedere
l'installazione di un nuovo pacchetto, si è scritto un lettore/
scrittore ZIP minimo e mirato (**`MiniZip.h`/`.cpp`**, senza supporto
ZIP64/cifratura — sufficiente per gli XLSX generati da strumenti
standard), in linea con l'approccio di dipendenze minime già seguito
nel resto del progetto. `CZipWriter` (aggiunto per l'export) scrive
solo voci "stored", senza compressione — stessa scelta di `CZipWriter`
in `translators/ods/`, verificata anche lì contro `unzip` di sistema.

Il parsing XML (`XlsxTranslator.cpp`) gestisce: celle con riferimento
`r="A1"`, valori numerici, stringhe condivise (`t="s"`, indice nella
tabella `sharedStrings.xml`), stringhe inline (`t="inlineStr"`,
`<is><t>...</t></is>` — aggiunto insieme all'export, che le scrive
invece di costruire una tabella di stringhe condivise separata), e
formule (`<f>...</f>`) — importate come testo con `=` davanti tramite
`TryToParseString`, così il nostro motore le ricalcola in modo
indipendente invece di fidarsi ciecamente del valore già calcolato da
Excel/LibreOffice (`<v>` accanto a `<f>`, che viene ignorato). Per
semplicità si assume che il primo foglio sia sempre
`xl/worksheets/sheet1.xml` (vero per i documenti con un solo foglio
generati da strumenti standard; un documento con più fogli
richiederebbe leggere `xl/workbook.xml` e i relativi `_rels` per la
mappatura nome-foglio → file XML — non ancora implementato, ne'
in lettura ne' in scrittura).

### Export XLSX: solo valori calcolati, non formule

Come per CSV/ODS, `WriteXLSX`/`BuildSheetXml` scrivono solo i
**valori calcolati** di ogni cella, non le formule. A differenza di
ODS (dove lo schema OpenDocument richiede contare righe/colonne per
posizionare le celle), XLSX ha un riferimento esplicito su ogni cella
(`r="A1"`), quindi `BuildSheetXml` scrive solo le celle realmente
presenti (via `CCellIterator`, non un rettangolo completo) — niente
bisogno del meccanismo di compressione delle celle vuote di ODS.
`Identify()` riconosce anche un sorgente ASCD nativo in ingresso (oltre
al vero XLSX) in modo da poter instradare `BTranslatorRoster` nella
direzione ASCD → XLSX; `Translate()` decide la direzione in base a
`info->type`/`outType`, con un `ReadASCD`/`WriteASCD` che rispecchiano
quelli degli altri translator.

### Build, test, installazione

```
cd translators/xlsx
make            # compila l'add-on XlsxTranslator
make test       # compila ed esegue il test end-to-end (import + export)
make install    # copia l'add-on in ~/config/non-packaged/add-ons/Translators
```

**Test end-to-end reale**: `tests/sample.xlsx` è un file XLSX vero,
costruito con il comando `zip` (XML scritti a mano, poi compressi —
verificabile con `unzip -l`), contenente due valori, una formula e una
stringa condivisa. Il test importa il file, verifica che i valori e la
formula (non il suo valore già calcolato) siano stati importati
correttamente, poi **ricostruisce un documento dai dati ASCD prodotti
e verifica che il motore ricalcoli autonomamente la formula ottenendo
il risultato corretto** — prova concreta che l'intera catena
ZIP → XML → motore di calcolo funziona, non solo che il testo
sopravvive al giro. Un secondo blocco verifica l'export con lo stesso
schema di ODS: un documento ASCD con un numero, una stringa e una
formula viene tradotto in XLSX, poi riletto dallo stesso translator
(round-trip completo ASCD → XLSX → ASCD), verificando che i valori
sopravvivano e che la formula sia diventata il suo valore calcolato.

Nessun bug nuovo del motore scoperto costruendo questo translator
(gli stessi due bug del ricalcolo/`GetBounds` e del test headless
senza `BApplication` erano già stati scoperti e corretti costruendo
l'export ODS, vedi sopra — qui si è solo applicato lo stesso fix, già
noto, a `ReadASCD`/`WriteASCD` di questo translator).

## translators/ods — import/export ODS (OpenDocument Spreadsheet)

Importa **e ora anche esporta** il formato di LibreOffice/OpenOffice
Calc (`kAtomoOdsFormat`, MIME
`application/vnd.oasis.opendocument.spreadsheet`) da/verso ASCD. Un
file ODS è, come XLSX, un archivio ZIP contenente XML — ma con schema
OpenDocument invece di OOXML: un unico `content.xml` con tutti i fogli
(non un file XML per foglio), più `META-INF/manifest.xml` come
marcatore del formato e un file `mimetype` (non compresso, primo
elemento dell'archivio nello standard).

**Riuso diretto di `MiniZip.h`/`.cpp`** (stessa copia del translator
XLSX per la parte di lettura, nessuna modifica necessaria: il
contenitore ZIP è identico) ed **expat** per il parsing di
`content.xml`, con un parser dedicato allo schema OpenDocument
(`OdsTranslator.cpp`). Per l'export si è aggiunto **`CZipWriter`** allo
stesso `MiniZip.h`/`.cpp` (simmetrico a `CZipReader`, sola scrittura,
solo voci "stored" senza compressione — più semplice e robusto del
"deflate" per le dimensioni tipiche di un foglio esportato, a costo di
un file leggermente più grande): scrive gli header locali, calcola il
CRC32 con `crc32()` di zlib, poi al `Close()` scrive la central
directory e l'End Of Central Directory. Verificato non solo contro il
proprio `CZipReader` ma anche contro `unzip` di sistema (`unzip -l`/
`unzip -p`), per escludere che l'archivio fosse valido solo per
coincidenza con il proprio lettore.

### Differenza strutturale importante rispetto a XLSX

Le celle XLSX hanno un riferimento esplicito (`<c r="A1">`); le celle
ODF **no**. Lo schema OpenDocument rappresenta il foglio come una
sequenza di `<table:table-row>` contenenti `<table:table-cell>`, e la
posizione (riga, colonna) va ricavata contando gli elementi mentre si
scorre il documento. Per comprimere gli intervalli di celle vuote
(comuni fino al margine destro/in fondo al foglio, spesso migliaia),
lo schema usa gli attributi `table:number-rows-repeated` e
`table:number-columns-repeated`: una singola riga/cella XML con
`repeated="500"` rappresenta 500 righe/colonne identiche. Il parser
avanza i contatori di riga/colonna di questo valore, ma **non genera
celle nell'ASCD per gli intervalli vuoti ripetuti** (solo per celle
con contenuto reale) — altrimenti un foglio LibreOffice tipico
produrrebbe decine di migliaia di celle vuote inutili nell'export.

### Conversione delle formule ODF

Le formule OpenDocument hanno una sintassi diversa da quella nativa:
prefisso `of:=` (namespace "OpenFormula") e riferimenti a cella tra
parentesi quadre con punto (`[.A1]`, `[.$A$1]` per riferimenti
assoluti). A differenza di XLSX (dove le formule Excel sono già
compatibili con la sintassi del nostro parser, basta togliere il `=`
iniziale... anzi va aggiunto), qui serve una conversione di sintassi:
`ConvertODFFormula()` toglie il prefisso `of:=`, e per ogni
riferimento tra `[.` e `]` rimuove le parentesi e i simboli `$`,
producendo `A1+B1` a partire da `of:=[.A1]+[.B1]`. **Limite noto**:
non gestisce riferimenti a fogli diversi (`[Foglio2.A1]`) né
intervalli complessi dentro le parentesi — sufficiente per formule
aritmetiche semplici con riferimenti nello stesso foglio, che sono il
caso comune.

Come per XLSX, le formule vengono importate come testo (con `=`
davanti) tramite `TryToParseString`, non il valore già calcolato da
LibreOffice (`office:value` sulla cella con `table:formula`, che viene
ignorato) — così il nostro motore le ricalcola in modo indipendente.

Si importa solo il primo `<table:table>` (primo foglio): un documento
multi-foglio richiederebbe iterare tutte le tabelle e decidere quale
esporre — stesso tipo di limite già accettato per XLSX/sheet1.

### Export ODS: solo valori calcolati, non formule

Come per l'export CSV (vedi sopra), `WriteODS`/`BuildContentXml`
scrivono solo i **valori calcolati** di ogni cella (`office:value-type`
`"float"`/`"string"`), non le formule — stessa scelta, stesso motivo:
niente sintassi ODF arbitraria da ricostruire per casi non gestiti
(l'inverso di `ConvertODFFormula` non è banale per formule qualsiasi).
`Identify()` riconosce anche un sorgente ASCD nativo in ingresso (oltre
al vero ODS) in modo da poter instradare `BTranslatorRoster` nella
direzione ASCD → ODS; `Translate()` decide la direzione in base a
`info->type`/`outType`, con un `ReadASCD`/`WriteASCD` che rispecchiano
quelli degli altri translator (stessa duplicazione deliberata invece di
condividerli, per non introdurre dipendenze tra translator).

### Build, test, installazione

```
cd translators/ods
make            # compila l'add-on OdsTranslator
make test       # compila ed esegue il test end-to-end (import + export)
make install    # copia l'add-on in ~/config/non-packaged/add-ons/Translators
```

**Test end-to-end reale**, come XLSX: `tests/sample.ods` è un file ODS
vero, costruito a mano (`mimetype` + `META-INF/manifest.xml` +
`content.xml` scritti a mano, compattati con `zip -X` — verificabile
con `unzip -l`), contenente due valori, una formula in sintassi ODF e
una stringa, più un blocco di celle vuote compresso con
`table:number-columns-repeated` (per verificare che non generi celle
fantasma). Il test importa il file, verifica che i valori e la formula
(convertita in sintassi nativa, non il suo valore già calcolato) siano
stati importati correttamente, poi **ricostruisce un documento dai
dati ASCD prodotti e verifica che il motore ricalcoli autonomamente la
formula ottenendo il risultato corretto**. Un secondo blocco verifica
l'export: un documento ASCD con un numero, una stringa e una formula
viene tradotto in ODS, poi il file ODS appena scritto viene riletto
dallo stesso translator (round-trip completo ASCD → ODS → ASCD),
verificando che i valori sopravvivano e che la formula sia diventata il
suo valore calcolato (non appiattita per errore alle altre celle, né
sparita).

Due bug reali del motore scoperti costruendo l'export ODS — nessuno
specifico del formato ODS (si manifestano identicamente in CSV/ASCD):
vedi in `ROADMAP.md`, Fase 5, "Bug scoperto: `GetBounds` esclude le
celle non ancora calcolate..." e "Bug scoperto (falso allarme, ma
test-harness da correggere): `GetCellFormula` di una formula con una
costante numerica si blocca senza un `BApplication`".
