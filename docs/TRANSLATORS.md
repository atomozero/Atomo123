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
`engine/src/Excel/`). Solo import, deliberatamente: l'export verso
l'ecosistema Excel passa da `translators/xlsx/` (Excel 2007+, formato
ZIP+XML molto più semplice da scrivere di BIFF/OLE2) — vedi ROADMAP.md,
Fase 5, per il ragionamento completo su questa scelta.

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

**Test automatizzato ancora limitato**: il test committato
(`tests/test_xls_translator.cpp`) verifica solo che `Identify()`
riconosca/rifiuti correttamente la firma OLE2, e che `Translate()` su
un OLE2 con contenuto BIFF non valido fallisca in modo pulito (senza
bloccarsi) — costruire a mano un flusso BIFF valido non è praticabile
per un test committato. **Verificato pero' manualmente con un file
`.xls` reale** (scaricato da un sito di file di esempio per test,
licenza non chiara per la ridistribuzione: non incluso nel
repository), sia a livello di translator sia aprendolo dal vivo
nell'app vera — vedi i tre bug reali scoperti e corretti sotto. Un
file di esempio con licenza libera da poter committare come fixture
di test resta da trovare (vedi nota "Test di congruità" nella Fase 3
di `ROADMAP.md`).

### Tre bug reali scoperti aprendo un file `.xls` autentico

Fino a questa sessione, `translators/xls` era stato testato solo con
file OLE2 costruiti a mano o deliberatamente malformati (vedi sopra):
mai un vero file `.xls` prodotto da Excel/LibreOffice, che ha una
struttura interna molto più ricca (più stream nella directory OLE2,
più tipi di record BIFF8, nomi di font in formato Unicode). Aprendo un
file scaricato del genere sono emersi tre bug distinti, tutti
preesistenti nel codice storico e mai manifestati prima:

1. **`long`/`unsigned long` a 64 bit invece di 32** in
   `Excel.OLE2.cpp` (`GetBookStream` e la struct `oleEntry`): stessa
   famiglia di bug già corretta altrove nel progetto (vedi il
   commento su `cell::operator<` in `Cell.h`) — su BeOS/PPC, per cui
   questo codice fu scritto, `long` era a 32 bit; su Haiku x86_64 è a
   64 bit. Con `oleEntry` a 64 bit i suoi campi non corrispondevano
   più ai 128 byte reali di una voce di directory OLE2, e l'array
   della FAT (`l[]`) veniva letto/indicizzato a passi sbagliati.
   **Fix**: sostituiti con `int32`/`uint32` (`SupportDefs.h`), fissi
   a 32 bit indipendentemente dalla piattaforma.
2. **La directory OLE2 non seguiva la propria catena di settori**:
   `GetBookStream` assumeva che tutte le voci della directory
   (`Root Entry`, `Workbook`, eventuali `\5SummaryInformation` ecc.)
   stessero in un singolo settore da 512 byte (4 voci da 128 byte) —
   vero solo per i file OLE2 più semplici. Un file reale, con anche
   solo qualche stream in più oltre a `Workbook`, richiede più
   settori. **Fix**: la ricerca della voce `Book`/`Workbook` ora segue
   la catena nella FAT quando il settore corrente è esaurito, esattamente
   come già faceva il codice per i settori dello stream `Workbook`
   stesso.
3. **`fCellView` nullo non controllato in `HandleXLRecordForPass1`**:
   `XlsTranslator.cpp` istanzia sempre `CExcel5Filter` con
   `cellView=NULL` (translator headless, nessuna UI collegata — commento
   già presente nel codice), ma diversi rami di
   `HandleXLRecordForPass1`/`Selection`/`Name` (record `DEFAULTROWHEIGHT`,
   `ROW`, `WINDOW2`, `DEFCOLWIDTH`, `COLINFO`, la selezione corrente, i
   nomi definiti) dereferenziavano `fCellView` senza controllarlo prima:
   dereferenziazione di puntatore nullo, che su questa Haiku si manifesta
   come blocco (debug_server intercetta il crash e resta in attesa di
   un'interazione grafica mai arrivata in un'esecuzione headless — stesso
   fenomeno già documentato sopra), non un crash immediato. Questi record
   sono comuni in qualunque foglio reale (altezza righe, larghezza
   colonne, impostazioni finestra), quindi il bug si manifestava con
   quasi ogni file `.xls` autentico, non solo con questo. **Fix**:
   aggiunto un controllo `if (fCellView)` prima di ogni uso, coerente con
   il commento già presente in `XlsTranslator.cpp` ("questi metadati
   vengono scartati in questa modalità" — l'intento era già documentato,
   mancava solo il controllo che lo rispettasse davvero).

Nessuno di questi bug ha a che fare con l'export (`translators/xls` resta
solo import): sono tutti nel percorso di lettura BIFF/OLE2 condiviso,
`engine/src/Excel/`.

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

### Export XLSX: formule vive per le celle sullo stesso foglio

A differenza di CSV, `WriteXLSX`/`BuildSheetXml` scrivono una formula
viva (`<f>FORMULA</f>`, oltre al `<v>` con il valore già calcolato)
per ogni cella con formula **che non referenzia un altro foglio**
(`CFormula::ReferencesOtherSheet()`, `engine/src/Formula/Formula.cpp`)
— questo translator esporta un solo foglio per file, quindi un
riferimento incrociato punterebbe a dati assenti nel file esportato:
in quel caso si ripiega sul comportamento precedente (solo il valore
calcolato, come CSV). Il testo della formula si ottiene con
`CFormula::UnMangle(buf, cella, doc, /*rcStyle*/false, /*decSep*/'.',
/*listSep*/',')`: i due parametri `decSepOverride`/`listSepOverride`
forzano sempre `.`/`,` (sintassi canonica ECMA-376) indipendentemente
dalle preferenze locali correnti dell'utente (es. virgola come
decimale, `;` come separatore di argomenti nell'Italia) — altrimenti
Excel non riuscirebbe a rileggere la formula scritta nel file. Una
cella con risultato testuale aggiunge anche `t="str"` sul `<c>`.

A differenza di ODS (dove lo schema OpenDocument richiede contare
righe/colonne per posizionare le celle), XLSX ha un riferimento
esplicito su ogni cella (`r="A1"`), quindi `BuildSheetXml` scrive solo
le celle realmente presenti (via `CCellIterator`, non un rettangolo
completo) — niente bisogno del meccanismo di compressione delle celle
vuote di ODS.
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

### Export ODS: formule vive per le celle sullo stesso foglio

Come per XLSX (vedi sopra), `WriteODS`/`BuildContentXml` scrivono
l'attributo `table:formula="of:=FORMULA"` per ogni cella con formula
**che non referenzia un altro foglio**
(`CFormula::ReferencesOtherSheet()`) — questo translator esporta un
solo foglio per file, quindi un riferimento incrociato punterebbe a
dati assenti nel file esportato: in quel caso resta il comportamento
precedente (solo `office:value`/`<text:p>` col valore già calcolato,
come CSV). Il testo della formula si ottiene con
`CFormula::UnMangle(buf, cella, doc, /*rcStyle*/false, /*decSep*/'.',
/*listSep*/';', /*odfRefs*/true)`: `odfRefs=true` avvolge ogni
riferimento a cella/intervallo fra `[.` e `]` (sintassi OpenFormula,
l'inverso esatto di `ConvertODFFormula` in ingresso), mentre
`decSep`/`listSep` forzano sempre `.`/`;` (convenzione
OpenFormula/LibreOffice) indipendentemente dalle preferenze locali
correnti dell'utente — altrimenti la virgola decimale italiana si
confonderebbe col separatore di argomenti nello stesso `of:=...`.
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

Stesso genere di bug del test-harness ricomparso costruendo i test
dell'export con formule live (vedi sopra): `tests/test_ods_translator`
non aveva mai avuto bisogno di una `BApplication` finché nessuna
formula esportata conteneva un letterale numerico (`CFormatter::ftoa`,
chiamato da `UnMangle` per il caso `valNum`, chiama
`BFont::StringWidth`) né un nome di funzione come `SUM` (serve
`InitFunctions()` con una risorsa `'Func'`, altrimenti il parser lancia
`CParseErr` per "funzione sconosciuta") — entrambi ora aggiunti in
`main()`, stesso schema già presente in
`test_xlsx_translator.cpp`/`tests/named_functions.rsrc`.

## Collegare il salvataggio dell'app ai translator veri

`MainWindow::SaveToFile` sceglie il formato di export dall'estensione
del nome file (`.csv`/`.xlsx`/`.ods`, altrimenti ASCD nativo), poi deve
instradare `BTranslatorRoster` verso il translator giusto. Due bug
reali scoperti collegando questo percorso a un vero documento (prima
non ci si arrivava mai in pratica: senza `.xlsx`/`.ods` riconosciuti
tutto veniva scritto come ASCD nudo sotto l'estensione sbagliata):

1. **Selezione ambigua di `BTranslatorRoster`**: quando la sorgente è
   ASCD generico (non un vero file XLSX/ODS/XLS con una firma
   distintiva), `Identify()` di **tutti e quattro** i translator lo
   riconosce allo stesso modo come `kAtomoNativeFormat`, con lo stesso
   punteggio di qualità/capacità. `BTranslatorRoster::Translate(source,
   info=NULL, ext, dest, wantOutType)` sceglie il translator internamente
   in base a `Identify()`, **ignorando `wantOutType`** nella selezione
   (verificato con tracciamento a runtime: sceglie sempre lo stesso
   translator_id, indipendentemente dal formato di uscita richiesto) —
   quel translator poi rifiuta correttamente l'`outType` che non
   supporta, con `B_NO_TRANSLATOR` anche se il translator giusto è
   installato. **Fix**: enumerare a mano
   `BTranslatorRoster::GetAllTranslators()`, controllare
   `GetOutputFormats()` di ciascuno per trovare chi dichiara
   `wantOutType`, poi chiamare la variante di `Translate()` che prende
   il `translator_id` esplicito invece di lasciar scegliere il roster.
2. **CSV/ODS rifiutavano l'ASCD versione 2**: `ui/src/AscdIO.cpp` (il
   vero formato su disco dell'app) scrive sempre versione 2 (byte
   `kind` per cella, distingue formula/letterale-numero/letterale-testo
   — vedi `docs/UI_ARCHITECTURE.md`), ma i `ReadASCD` privati di CSV e
   ODS (usati come formato intermedio fra `MainWindow` e il translator)
   accettavano solo versione 1: qualunque salvataggio reale in `.csv`/
   `.ods` falliva con `B_MISMATCHED_VALUES`, non solo quelli con una
   formula. Bug invisibile finché nessun test passava per il vero
   `MainWindow::SaveToFile` con un documento v2 reale. **Fix**: un
   limite di lettura separato (`kASCDMaxReadableVersion = 2`) da
   `kASCDVersion` (che resta 1, il formato che *questi* `WriteASCD`
   continuano a scrivere) — il primo tentativo di fix aveva confuso i
   due, rendendo la condizione ancora equivalente a "solo versione 1".
