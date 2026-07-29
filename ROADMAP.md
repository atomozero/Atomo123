# Roadmap — foglio di calcolo nativo per Haiku OS

Stato: **Fase 1, Fase 2 e Fase 3 chiuse** (translator CSV, XLS legacy,
XLSX e ODS tutti completati e testati); **Fase 4 (UI nativa) in stato
avanzato** — finestra principale, griglia, apertura file via
Translation Kit, barra formule, editing in-cella, menu Modifica
(taglia/copia/incolla/cancella/trova), Locale Kit e Print Kit tutti
funzionanti e testati dal vivo in una sessione grafica reale; mancano
ancora export, formattazione (menu Formato), sostituisci, toolbar e
icone (bloccate: galleria HVIF autorizzata risultata vuota al
controllo). **Fase 5 (packaging/compatibilità) in corso** — ricetta
HaikuDepot pronta ma non ancora buildabile (nessun repository
pubblico); test di compatibilità su corpus reale e decisione della
licenza del codice nuovo ancora da fare. Aggiornato ad ogni fase
completata.

Questo documento traccia le fasi del progetto: un'applicazione foglio di
calcolo nativa per Haiku OS (Interface/Layout Kit), compatibile con i
formati Excel (XLS/XLSX) e OpenOffice/LibreOffice (ODS), tramite il
Translation Kit di Haiku come layer di import/export plugin-based.

Il piano nasce da due fonti concrete:
1. Ricerca tecnica multi-fonte su API Haiku, librerie di formati file, e
   stato del vecchio progetto SumIt (vedi `docs/RESEARCH.md`).
2. Porting empirico, testato a mano su Haiku hrev59800/GCC 13.3.0, del
   codice storico OpenSumIt (vedi `legacy/opensumit/PORTING_STATUS.md`).

## Decisione architetturale di fondo

Non si riparte da zero e non si eredita SumIt as-is. Si **estrae e si
porta il motore di calcolo e l'importer Excel legacy** di SumIt (già
verificato solido: parser formule, grafo di dipendenze celle, import
BIFF/OLE2), disaccoppiandolo dalla UI BeOS-era (che invece va scritta
da zero in Interface/Layout Kit moderno). L'interoperabilità con i
formati file passa dal Translation Kit nativo di Haiku.

```
legacy/opensumit/   -> sorgente storico, patchato per compilare su Haiku 64bit
engine/             -> motore di calcolo estratto, isolato, testabile
translators/        -> add-on BTranslator per xlsx/ods/csv/xls
ui/                 -> applicazione nativa Interface/Layout Kit
docs/               -> ricerca, architettura, note di porting
```

## Fase 1 — Stabilizzare il build del codice storico (CHIUSA)

Obiettivo: far compilare per intero `legacy/opensumit` su Haiku moderno
a 64 bit, così da avere un motore di calcolo e un importer Excel legacy
funzionanti e testabili come base per la Fase 2.

Fatto finora (sessione di porting empirico):
- [x] `bsl` compila senza modifiche
- [x] `rez` (tool risorse legacy) — **completamente sistemato**: bug
      Makefile (mv->cp) + troncamento puntatore a 32 bit (50+ cast
      `(int)`->`(long)` nella grammatica bison/flex, propagati a
      `RState::Shift`, `RElem::FindIdentifier`, `RSValue`, `intmap`,
      `ResHeader`). Vedi `legacy/opensumit/PORTING_STATUS.md` per il
      dettaglio tecnico completo.
- [x] Generazione risorse (`.r`/`.rdef`) del progetto sum-it: funziona
      al 100% con il `rez` sistemato
- [x] Motore di calcolo (`Formula.cpp`): compila pulito (fix `isnan`)
- [x] Import Excel legacy (`Excel*.cpp`): compila pulito (fix
      `arpa/inet.h`)
- [x] `Huffman.cpp`, `MThread.cpp`, libreria `ColorPicker`: compilano
      puliti dopo fix mirati
- [x] Tutti i file rimanenti sistemati con lo stesso pattern meccanico
      (`long`/`ulong` BeOS R5 -> `int32`/`uint32`/`type_code`/`status_t`
      Haiku moderno) — dettaglio in `legacy/opensumit/PORTING_STATUS.md`
- [x] Link finale del binario `sum-it` completo (`OpenSum-It`, ELF
      64-bit) — build pulita senza errori
- [x] Smoke test: avvio applicazione senza crash (processo resta vivo,
      un solo alert recuperabile — vedi nota aperta sotto)
- [x] **Nota permanente (non risolta, rimandata)**: `CRDialog::ConstructFromTemplate`
      (RDialog.cpp:230) solleva `errDamagedResources` per un tag non
      riconosciuto in un template di dialogo letto dalle risorse, al
      primo avvio. Tentativo di isolare quale dialogo lo scatena
      tramite lo strumento di scripting nativo `hey` (interrogazione
      delle finestre aperte via `BMessage`): il processo resta vivo ma
      non risponde allo scripting entro un tempo ragionevole (timeout),
      probabilmente perché il vecchio meccanismo di alert/dialoghi
      blocca il message loop in un modo non compatibile con lo
      scripting BMessage moderno. Non essendoci un modo rapido per
      isolare il problema senza costruire un harness di test UI
      dedicato, la questione è rimandata: non blocca la Fase 2 (il
      motore di calcolo non passa da `RDialog`), verrà ripresa se e
      quando servirà davvero la UI storica (probabilmente mai, visto
      che la Fase 4 prevede una UI nuova da zero).
- [x] Test rimandati (richiedono automazione input UI): import di un
      file `.xls` reale via UI, calcolo formula base via UI — assorbiti
      dai test dell'engine isolato in Fase 2, che verificano
      import/calcolo senza passare dalla UI storica

**Test di congruità/compatibilità di questa fase**: build pulita
(`make` senza errori) di `bsl`, `rez`, `sum-it`; avvio del binario senza
crash; import di un file `.xls` reale di test; verifica che una formula
semplice (`=SOMMA(A1:A3)`) calcoli il risultato corretto.

## Fase 2 — Estrarre il motore di calcolo come libreria isolata (CHIUSA)

Obiettivo: separare `Formula/`, `Cell/`, `Excel/` (motore + import
legacy) dal resto di `sum-it` (UI, dialoghi, grafici) in una libreria
statica autonoma, senza dipendenze da `BWindow`/`BView`, compilabile e
testabile in isolamento.

- [x] Individuate e tagliate le dipendenze dirette da classi UI BeOS
      (`CellView`, `CellWindow`) nel codice di `Formula`/`Cell`/`Excel`:
      escluse `CellCommands.*`/`CellScrollBar.*` (UI pura), create due
      classi stub minimali (`engine/src/Stubs/EngineViewStub.h`,
      `ProgressStub.h`) per i punti in cui il codice storico passa un
      puntatore opzionale a `CCellView`/`StProgress`
- [x] API pubblica disponibile tramite `CContainer` (già esistente nel
      codice storico, non serve un nuovo layer): `NewCell`/
      `TryToParseString` per scrivere, `GetValue`/`GetCellFormula` per
      leggere, `CalcCell` per ricalcolare
- [x] Scritto `engine/tests/smoke_test.cpp`: crea un documento
      headless, inserisce formule con riferimenti a celle e verifica i
      risultati calcolati — **passa** (`make test`)
- [x] Documentata l'architettura in `docs/ENGINE_API.md` (mappa file,
      stub, limitazioni note, bug trovati)

**Scoperta importante**: l'isolamento ha fatto emergere due bug reali
di corruzione di memoria silenziosa (non solo i consueti fix
meccanici `long`→`int32`), entrambi dovuti all'assunzione
`sizeof(long)==4` (vera su BeOS/PPC a 32 bit, falsa su Haiku x86_64):
`cell::operator==/</...`  leggeva 4 byte oltre una struct da 4 byte
(rompendo l'ordinamento di `std::map<cell,...>`, quindi l'inserimento/
lookup delle celle), e il formato bytecode delle formule compilate
(`kPFWordSize`/`fString`) disallineava la lettura degli opcode per lo
stesso motivo (qualunque formula con un riferimento a cella falliva).
Vedi `docs/ENGINE_API.md` per il dettaglio tecnico completo e
`legacy/opensumit/PORTING_STATUS.md` per il sospetto che lo stesso bug
affligga anche `RDialog.cpp` (non ancora verificato/corretto lì).

**Test di congruità/compatibilità — superato**: `make test` in
`engine/` produce `libengine.a` + esegue `tests/smoke_test`, verde;
nessun link a `BView`/`BWindow` verificato con
`nm -u libengine.a | c++filt | grep -oE "\bB[A-Z][a-zA-Z]*::"`
(unica eccezione nota: `BAlert` nel percorso di error-reporting, vedi
limitazioni in `docs/ENGINE_API.md`).

## Fase 3 — Translator Kit: import/export XLSX/ODS/CSV/XLS (CHIUSA)

Obiettivo: add-on `BTranslator` installabili, uno per formato, che
usano l'engine di Fase 2 e librerie esterne leggere.

- [x] Translator CSV: `translators/csv/`, converte CSV <-> formato
      nativo provvisorio ASCD tramite `CTextConverter` (già nel
      motore) e `CContainer::GetCellFormula`/`TryToParseString`. Test
      di round-trip verde (`make test`). Vedi `docs/TRANSLATORS.md`
      per i dettagli e i bug scoperti costruendolo (elenco completo
      sotto).
- [x] Translator XLS legacy: `translators/xls/`, riusa
      `CExcel5Filter` (già portato in Fase 1/2) per l'import, converte
      verso ASCD. Riconoscimento formato (firma OLE2) e robustezza
      (fallimento pulito su dati non validi) verificati con test verde
      (`make test`); **manca ancora un test di importazione end-to-end
      con un file `.xls` reale** (nessuno disponibile in questa
      sessione — vedi `docs/TRANSLATORS.md`). Solo import per ora, il
      motore non ha un writer per il formato binario legacy.
- [x] Translator XLSX: `translators/xlsx/`. Non usa OpenXLSX (non
      valutato il porting): un file XLSX è ZIP+XML, e su questo
      sistema erano già presenti (con header di sviluppo) **expat**
      per l'XML e **zlib** per la decompressione, ma non una libreria
      ZIP-contenitore con gli header installati — scritto quindi un
      lettore ZIP minimo dedicato (`MiniZip.h/.cpp`, sola lettura,
      senza aggiungere nuove dipendenze di sistema). Test end-to-end
      **reale**: un XLSX vero costruito con `zip`, importato e
      verificato — inclusa la ricostruzione di un documento dai dati
      importati con **ricalcolo effettivo della formula dal motore**
      (non solo verifica testuale). Nessun bug nuovo del motore
      scoperto (codice tutto nuovo, non riusa `CExcel5Filter`).
- [x] Translator ODS: `translators/ods/`. Confermata l'ipotesi
      dell'annotazione precedente — niente liborcus, ODS è ZIP+XML
      come XLSX ma con schema OpenDocument, quindi `MiniZip.h/.cpp` è
      stato riusato senza modifiche e expat con un parser dedicato per
      `content.xml`. Differenza strutturale importante da XLSX: le
      celle ODF non hanno un riferimento esplicito tipo `r="A1"`, la
      posizione va ricavata contando righe/colonne mentre si scorre il
      documento, con gli attributi `table:number-rows-repeated`/
      `table:number-columns-repeated` che comprimono gli intervalli di
      celle vuote (fino al margine del foglio) — gestiti esplicitamente
      per non generare celle fantasma nell'ASCD. Le formule ODF (stile
      `of:=[.A1]+[.B1]`) vengono convertite in formula nativa
      (`A1+B1`) con un piccolo traduttore di sintassi dedicato, non
      lasciate come testo opaco. Test end-to-end **reale**: un ODS
      vero costruito a mano (mimetype + manifest + content.xml,
      compattato con `zip`), importato e verificato — inclusa la
      ricostruzione di un documento dai dati importati con
      **ricalcolo effettivo della formula dal motore**. Nessun bug
      nuovo del motore scoperto (come XLSX, codice tutto nuovo che non
      passa da `CExcel5Filter`). Limite noto: si importa solo il primo
      foglio (`<table:table>`), e la conversione di formula non
      gestisce riferimenti multi-foglio o intervalli complessi — stesso
      tipo di limite già accettato per XLSX/sheet1.
- [x] Ogni translator dichiara `Identify()`/`Translate()`/
      `InputFormats()`/`OutputFormats()` secondo il framework
      `BTranslatorRoster` (pattern stabilito con il translator CSV,
      riusato per i successivi)

**Fase 3 chiusa**: i quattro translator (CSV, XLS legacy, XLSX, ODS)
sono tutti implementati e testati con `make test` verde. Rimane un
gap esplicito: i test usano file di esempio costruiti per questa
sessione (non un corpus di file reali generati da Excel/LibreOffice in
condizioni non controllate) — la verifica di interoperabilità reale
su larga scala è rimandata alla Fase 5 (che ha già un item dedicato).

**Test di congruità/compatibilità**: round-trip per ogni formato
(esporta un documento di test, reimportalo, verifica che i dati
coincidano) — fatto per tutti e quattro; import di file reali generati
da Excel e da LibreOffice Calc, non solo dal nostro export, per una
vera interoperabilità end-to-end — rimandato alla Fase 5.

### Bug di blocco headless scoperti in Fase 2 e Fase 3

Costruire un translator concreto (non solo il motore in isolamento)
ha fatto emergere altri bug oltre a quelli già chiusi in Fase 2.
Elenco completo, in ordine di scoperta (dettaglio tecnico completo in
`docs/ENGINE_API.md` e `docs/TRANSLATORS.md`):

1. **Fase 2** — `be_plain_font`/`gPrefs` non protetti nel costruttore
   di `CContainer`: si bloccavano senza una vera applicazione GUI.
2. **Fase 2** — `kPFWordSize = sizeof(long)` invece di un tipo a
   larghezza fissa nel formato bytecode delle formule compilate:
   disallineava la lettura degli opcode su Haiku a 64 bit, facendo
   fallire qualunque formula con un riferimento a cella.
3. **Fase 3** — `CFontMetrics::operator[]`/`StringWidth` chiamavano
   `be_plain_font->StringWidth()` senza controllare se esisteva un
   font reale caricato.
4. **Fase 3** — `CFontSizeTable::operator[]` accedeva a un
   `std::vector` sempre vuoto nella libreria engine senza controllo
   dei limiti.
5. **Fase 3** — `GetFunctionNr` eseguiva una ricerca binaria su una
   tabella funzioni (`gFuncArrayByName`/`gFuncCount`) mai inizializzata
   nella libreria engine (richiederebbe `InitFunctions()`, mai
   chiamata), dereferenziando un puntatore nullo.
6. **Fase 3** — subito dopo, `parser.cpp` indicizzava
   `gFuncArrayByNr` con l'identificatore di funzione **prima** di
   controllare se fosse valido (-1 = non trovata) — indice negativo
   su un array C.
7. **Fase 3** — `CExcel5Filter::GetBookStream` era dichiarata
   `throw()` (equivalente a `noexcept` in C++17) ma il suo corpo può
   lanciare `CErr` su dati non validi: una funzione `noexcept` che
   lancia comunque causa `std::terminate()` immediato, bypassando
   qualunque `catch` a monte (anche uno che avvolge direttamente la
   chiamata). A differenza degli altri bug di questa lista, non è
   legato all'assenza di app_server/UI — è un problema di correttezza
   C++ puro, preesistente nel codice storico.

**Nota aperta importante**: i fix 3-6 rendono il codice sicuro (nessun
crash/blocco), ma la causa di fondo dei bug 5-6 (tabella funzioni mai
caricata) resta: **le formule con funzioni con nome (SOMMA, SE, ecc.)
non sono ancora realmente utilizzabili**. Serve generare/allegare la
risorsa `'Func'` (con `bsl`/`rez`, già pronti dalla Fase 1) e chiamare
`InitFunctions()` all'avvio dell'engine. Da risolvere prima che le
formule con funzioni possano funzionare — per ora funzionano solo
formule con operatori aritmetici e riferimenti a cella.

**Perché sembravano blocchi infiniti invece di crash**: molti di
questi bug sono dereferenziazioni di puntatori nulli, che normalmente
darebbero un crash immediato. Su questa Haiku il processo restava
bloccato indefinitamente invece di terminare — ipotesi più probabile:
`debug_server` di Haiku intercetta il crash e sospende il thread in
attesa di un'interazione utente (debug/termina) che non arriva mai in
un'esecuzione headless da riga di comando. Usare sempre `timeout N`
nei test headless per non restare bloccati.

## Fase 4 — UI nativa Interface/Layout Kit (IN CORSO)

Obiettivo: applicazione con griglia celle, editing, formattazione,
grafici base, scritta da zero (non riusando `CellView`/`CellWindow`
BeOS-era), che usa l'engine di Fase 2 e i translator di Fase 3. Vedi
`docs/UI_ARCHITECTURE.md` per il dettaglio tecnico completo
(architettura, editing in-cella, bug trovati, test, limiti noti).

- [x] Finestra principale, griglia celle (vista custom `SheetView`,
      non `BGridLayout`: la griglia è disegnata a mano in `Draw()`
      sopra una `BScrollView`, con calcolo manuale del range di
      scroll — `BGridLayout` è pensato per un numero fisso di
      sotto-view, non per un foglio virtualmente enorme con celle
      sparse). Vedi `ui/` (nuova cartella): `App`/`MainWindow`/
      `SheetView`/`AscdIO`.
- [x] Apertura file tramite `BTranslatorRoster`: sceglie
      automaticamente il translator installato adatto (CSV/XLS/XLSX/
      ODS/ASCD nativo) in base al contenuto reale del file, non
      all'estensione — stesso meccanismo Translation Kit di Fase 3,
      ora usato da un consumer reale.
- [x] Editing tramite barra formule: selezione con mouse/tastiera, la
      barra formula mostra/modifica la formula della cella corrente,
      Invio conferma e ricalcola.
- [x] Editing in-cella: doppio click su una cella (rilevato dal campo
      "clicks" del messaggio di mouse down) o si inizia direttamente a
      digitare mentre una cella è selezionata (come Excel/LibreOffice
      Calc: il carattere digitato sostituisce il contenuto) aprono un
      `BTextControl` temporaneo posizionato sopra la cella
      (`SheetView::StartEditing`/`CommitEditing`). Invio o perdita di
      selezione (click altrove) confermano; Escape annulla. Vedi nota
      tecnica sotto sul perché Escape richiede un `BMessageFilter`
      sulla `BTextView` interna invece di un semplice `KeyDown`.
- [ ] Test end-to-end del doppio click/digitazione diretta: verificato
      con test di non-regressione (apertura file reale con il nuovo
      codice presente, nessun crash) e con revisione manuale del
      codice, ma non con un click/tasto realmente sintetizzato — questo
      ambiente di test non ha uno strumento di iniezione mouse/tastiera
      (a differenza di `B_REFS_RECEIVED`/`B_SAVE_REQUESTED`, che si
      possono simulare costruendo il `BMessage` a mano). Verifica
      interattiva manuale rimandata a un utente reale o a un ambiente
      con tale strumento.
- [x] Menu Modifica: Taglia/Copia/Incolla/Cancella, passano dagli
      appunti di sistema veri (`be_clipboard`, Clipboard Kit) e non da
      un buffer privato dell'app — il contenuto copiato (la formula,
      come mostrata dalla barra formule) è testo piano
      (`text/plain`/`B_MIME_TYPE`) condivisibile con qualunque altra
      applicazione Haiku. Verificato dal vivo con
      `ui/tests/test_clipboard.cpp` (`cd ui && make test-clipboard`,
      richiede una sessione grafica) e con una prova incrociata reale
      usando il tool a riga di comando di sistema `clipboard`: scrittura
      dal nostro binario → lettura con `clipboard -p`, e scrittura con
      `clipboard -c` → lettura dal nostro binario, in entrambe le
      direzioni con esito corretto.
- [x] Dialogo Trova: nuova finestra `FindWindow` (campo di ricerca +
      pulsante "Trova successivo"), voce "Trova…" nel menu Modifica.
      Non esegue la ricerca da sé (le celle appartengono al documento
      di `MainWindow`, su un altro thread/`BLooper`): inoltra il testo
      cercato con un `BMessage` a un `BMessenger`, stessa regola del
      bug di thread `BApplication`/`BWindow` già corretto sopra. La
      ricerca (`MainWindow::FindNext`) scandisce le celle esistenti
      confrontando il testo (case-insensitive, sottostringa) e sceglie
      il primo risultato dopo la cella selezionata, con
      "wrap-around" se non ce n'è nessuno. **Non ancora fatto**:
      "Sostituisci", nessun menu Formato.
- [ ] Toolbar — per ora solo i menu File (Nuovo/Apri/Salva con
      nome/Stampa/Esci) e Modifica (Taglia/Copia/Incolla/Cancella/Trova)
- [ ] Export: "Salva con nome" scrive solo in ASCD nativo (nessun
      translator ha ancora un writer per CSV/XLS/XLSX/ODS, coerente
      col limite "solo import" già documentato in Fase 3)
- [x] Locale Kit: i valori numerici nella griglia (non nella barra
      formule, che mostra sempre il testo grezzo modificabile) sono
      formattati con `BNumberFormat` secondo le preferenze di sistema
      (separatore delle migliaia, punto/virgola decimale) — livello di
      presentazione sopra il testo già calcolato dal motore
      (`CFormatter`/`eGeneral`, generico e non locale-aware). Verificato
      dal vivo aprendo un file con un numero grande (1234567.89): la
      griglia mostra "1.234.567,89" (convenzione italiana, punto per le
      migliaia) mentre la barra formule mostra il valore grezzo
      "1234567.89". Formattazione valuta/data non ancora esposta (il
      motore distingue i tipi ma la UI non offre ancora un modo per
      impostare il formato di una cella).
- [x] Print Kit: voce "Stampa…" nel menu File, tramite `BPrintJob`
      (`ConfigJob` per il dialogo di sistema stampante/opzioni,
      `BeginJob`/`DrawView`/`SpoolPage`/`CommitJob` per la stampa
      vera e propria). Si stampa solo l'area del foglio con dati
      (`SheetView::ContentRect`, nuovo metodo pubblico), suddivisa in
      pagine in base all'area stampabile scelta — non l'intero
      intervallo virtuale del motore. **Limite noto**: le intestazioni
      di riga/colonna compaiono solo sulla prima pagina (sono
      disegnate da `SheetView::Draw` solo nella banda fissa
      0–kHeaderWidth/kHeaderHeight, non ripetute per ogni pagina
      stampata). **Verificato**: build pulita, e test di
      non-regressione (apertura di un file reale col nuovo codice
      presente, nessun crash) — non è stato possibile un test
      end-to-end di stampa reale in questa sessione: `BPrintJob::
      ConfigJob()` apre un dialogo di sistema che richiede una scelta
      utente (stampante/opzioni/OK), non simulabile con un `BMessage`
      costruito a mano come `B_REFS_RECEIVED` (stesso limite già
      documentato per `B_SAVE_REQUESTED` e per il doppio click/
      digitazione diretta in-cella — nessuno strumento di iniezione
      mouse/tastiera in questo ambiente).
- [ ] Icone: autorizzato l'uso del portale www.hvif-store.art (formato
      HVIF nativo Haiku) come fonte per le icone dell'applicazione —
      **bloccato**: verificato il 2026-07-29, la galleria del sito
      risultava vuota ("0 icons found" in home, i percorsi tentati
      /icons e /search?q=... restituivano 404). Non essendo disponibile
      contenuto scaricabile dalla fonte autorizzata, l'item resta
      aperto invece di disegnare un'icona da zero non richiesta —
      da riprovare in una sessione futura (magari il sito verrà
      popolato) o da chiedere indicazioni dirette all'utente.

**Limite noto — intestazioni non "congelate"**: le lettere di colonna
e i numeri di riga sono disegnati alla loro posizione virtuale
assoluta, non ancorati al bordo della viewport durante lo scroll:
scorrendo oltre la prima schermata le intestazioni scorrono via
insieme al contenuto invece di restare fisse. Scelta deliberata per
questa prima versione (evitare la complessità di viste multiple
sincronizzate/flicker da `CopyBits`); da rivedere in un secondo
momento.

### Bug scoperto: violazione di thread fra `BApplication` e `BWindow`

Il primo test end-to-end (apertura di un file XLSX reale mentre l'app
gira dentro una vera sessione grafica, non headless) si è chiuso con
un vero crash intercettato da `debug_server` — a differenza di quasi
tutti i bug delle fasi precedenti, qui *c'era* un `app_server`
collegato, quindi non poteva trattarsi della stessa famiglia di bug
"codice mai eseguito senza UI".

Causa: `App::RefsReceived` (chiamato sul thread di `BApplication`)
invocava direttamente `MainWindow::OpenFile()`, un metodo che tocca
le `BView` della finestra (`Invalidate()`, `BTextControl::SetText()`,
ecc.) — ma `BWindow` e le sue `BView` vivono sul thread del *loro*
`BLooper`, non su quello dell'applicazione, e non sono sicure da
toccare senza prima acquisire il lock della finestra
(`BWindow::Lock()`). Chiamare quel metodo direttamente dal thread
sbagliato, senza lock, corrompe lo stato mentre l'app_server
potrebbe contemporaneamente elaborare un `Draw()`/`Invalidate()` sullo
stesso oggetto dal thread corretto — una race condition classica
nell'Application Kit di Haiku/BeOS.

**Fix**: `App::RefsReceived` ora si limita a inoltrare il `BMessage`
alla finestra con `fWindow->PostMessage(message)`, lasciando che sia
`MainWindow::MessageReceived` (già scritto per gestire
`B_REFS_RECEIVED`) a processarlo — così l'elaborazione avviene sul
thread corretto, con il lock preso automaticamente dal ciclo dei
messaggi del `BLooper`. Regola generale per il resto della Fase 4:
mai chiamare direttamente un metodo che tocca le `BView` di una
finestra da un altro thread — sempre passare da un `BMessage`
inoltrato con `PostMessage()`/`SendMessage()`.

**Verifica**: dopo il fix, l'app è stata rilanciata in una sessione
grafica reale (`app_server`/Deskbar/Tracker attivi su questo sistema)
e testata inviando veri `B_REFS_RECEIVED` (simulando un trascinamento
da Tracker) per un file XLSX e un file ODS reali — l'app resta viva,
importa e mostra i valori correttamente (verificato sia via
screenshot sia interrogando la finestra dal vivo con lo strumento di
scripting nativo `hey`), e un percorso file non valido mostra il
proprio `BAlert` di errore invece di andare in crash.

### Nota tecnica: perché Escape nell'editor in-cella serve un `BMessageFilter`

Un primo tentativo di gestire Escape per annullare l'editing in-cella
sovrascrivendo `KeyDown` in una sottoclasse di `BTextControl` non
avrebbe funzionato: `BTextControl::MakeFocus()` inoltra il fuoco
tastiera alla sua `BTextView` interna (che gestisce davvero
l'inserimento testo), quindi è quella a ricevere gli eventi tastiera,
non il `BTextControl` contenitore — un `KeyDown` sovrascritto sul
contenitore non verrebbe mai chiamato durante la digitazione normale.
Soluzione: un `BMessageFilter` installato direttamente sulla
`BTextView` interna (ottenuta con `BTextControl::TextView()`), che
intercetta `B_KEY_DOWN` con `raw_char == B_ESCAPE` e la trasforma in
un messaggio di annullamento per `SheetView`, restituendo
`B_SKIP_MESSAGE` per impedire che l'Escape venga anche inserito come
carattere.

### Test di round-trip di AscdIO (Fase 4)

`ui/tests/test_ascd_io.cpp` (`cd ui && make test`, non richiede una
sessione grafica) verifica che `SaveASCD`/`LoadASCD` — la stessa
logica usata da "Salva con nome" e dall'apertura di file `.ascd`
nativi — siano l'una l'inversa dell'altra: numeri, testo e una
formula (verificata anche nel suo ricalcolo dal motore, non solo nel
testo) sopravvivono a un giro salva→ricarica completo. Non passa dalla
vera finestra (niente `BFilePanel`): un tentativo di simulare
`B_SAVE_REQUESTED` con un messaggio costruito a mano e inviato
all'applicazione non ha funzionato, perché — a differenza di
`B_REFS_RECEIVED` (per cui `App::RefsReceived` inoltra esplicitamente
alla finestra) — non c'è nessun inoltro automatico per
`B_SAVE_REQUESTED` quando arriva alla `BApplication`: nell'uso reale
il `BFilePanel` lo manda direttamente alla finestra (è il target
impostato nel suo costruttore), non passa mai dall'applicazione.

**Test di congruità/compatibilità**: build pulita (`cd ui && make`);
`cd ui && make test` verde (round-trip ASCD, vedi sopra); avvio in una
sessione grafica reale (non headless, a differenza di
engine/translator: qui serve `app_server` per definizione); apertura
di file reali XLSX/ODS tramite un vero `B_REFS_RECEIVED` senza crash,
con valori mostrati correttamente nella griglia e nella barra formule
(ripetuto anche dopo aver aggiunto l'editor in-cella, come test di
non-regressione); verifica che nessun'altra chiamata cross-thread
diretta esista (ispezione manuale di `App.cpp`/`MainWindow.cpp` —
unico punto di contatto fra i due thread era `RefsReceived`, già
corretto). Rimane da fare un vero test interattivo del doppio
click/digitazione diretta in-cella (vedi nota sopra sulla mancanza di
uno strumento di iniezione mouse/tastiera in questo ambiente).

## Fase 5 — Integrazione, packaging, compatibilità reale (IN CORSO)

- [x] Ricetta pacchetto per HaikuDepot: `packaging/atomo123-0.1.0.recipe`,
      formato HaikuPorter reale (verificato contro `haikubench-1.2.0.recipe`,
      un'altra recipe dello stesso autore già funzionante su questo
      sistema, e contro `HaikuPorter/Port.py` per i nomi esatti delle
      variabili di installazione — `$appsDir`, `$addOnsDir`).
      `BUILD()` compila `engine/`, i quattro translator e `ui/` con gli
      stessi Makefile già testati nelle fasi precedenti; `INSTALL()`
      copia il binario in `$appsDir` con collegamento Deskbar
      (`addAppDeskbarSymlink`) e i quattro translator in
      `$addOnsDir/Translators`. **Non ancora utilizzabile per una
      build reale**: `SOURCE_URI`/`CHECKSUM_SHA256` sono segnaposto
      perché il progetto non è ancora pubblicato su un repository
      pubblico raggiungibile da `haikuporter` (nessun remote git
      configurato) — da completare quando/se il progetto verrà
      pubblicato. **Licenza non ancora chiarita** (vedi nota nella
      recipe): il codice storico riusato in `engine/` porta con sé la
      clausola pubblicitaria BSD a 4 clausole di Sum-It, ma il codice
      nuovo (`translators/`, `ui/`) non ha ancora una licenza
      dichiarata dall'utente — non si è assunta una licenza non
      richiesta, il campo `LICENSE` della recipe è esplicitamente
      marcato come segnaposto in attesa di una decisione.
- [ ] Test di compatibilità con corpus di file reali: Excel (xls/xlsx
      di varie versioni), LibreOffice Calc (ods), OpenOffice legacy —
      finora testato solo con file costruiti a mano per questo
      progetto (vedi `translators/*/tests/`), non un vero corpus
      eterogeneo generato da applicazioni reali in condizioni non
      controllate
- [ ] Verifica licenze — vedi nota sopra, decisione della licenza per
      il codice nuovo non ancora presa

## Fase 6 — Polish e funzionalità avanzate

- [ ] Grafici, tabelle pivot base, funzioni aggiuntive
- [ ] Ottimizzazione ricalcolo su fogli grandi
- [ ] Documentazione utente

---

Ogni fase, a completamento, aggiorna questo file (checkbox + eventuale
nuova sotto-fase emersa) e `docs/PORTING_NOTES.md`/`docs/ENGINE_API.md`
pertinenti, prima di iniziare la fase successiva.
