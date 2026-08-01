# Roadmap — foglio di calcolo nativo per Haiku OS

Stato: **Fase 1, Fase 2, Fase 3 e Fase 4 chiuse**. Fase 4 (UI nativa):
finestra principale, griglia, apertura file via Translation Kit, barra
formule, editing in-cella, menu Modifica (taglia/copia/incolla/
cancella/trova e sostituisci), menu Formato (Generale/Numero/Valuta/
Percentuale), toolbar, Locale Kit, Print Kit, icona applicazione
(HVIF, disegnata da zero) ed export CSV — tutti fatti e testati dal
vivo in una sessione grafica reale. **Fase 5 (packaging/compatibilità)
in corso** — ricetta HaikuDepot pronta ma non ancora buildabile
(nessun repository pubblico); licenza **MIT** decisa per il codice
nuovo (vedi LICENSE — il codice storico Sum-It/`engine/` resta sotto
la sua licenza BSD originale); export ODS e XLSX aggiunti (oltre a
CSV) — export XLS legacy escluso deliberatamente (nessun writer
BIFF/OLE2 su cui appoggiarsi, XLSX copre già l'export verso
l'ecosistema Excel); resta il test di compatibilità su un corpus di
file reali, bloccato in attesa di file campione o autorizzazione a
installare software. **Fase 6 chiusa**: guida utente, funzioni con
nome nelle formule, grafici a barre e tabelle pivot di base, editing
in-cella e navigazione da tastiera in stile Excel, SUMIF/COUNTIF/
AVERAGEIF, correzione della propagazione del ricalcolo alle celle
dipendenti — tutti i punti pianificati fatti (nuovi bug/richieste
dell'utente possono comunque emergere e aggiungersi). **Fase 7 (recupero
funzionalità rispetto a Sum-It storico) in corso**: confronto puntuale
completato (menu/comandi/dialoghi di Sum-It vs. Atomo123), selezione
multi-cella fatta (Maiusc+frecce, trascinamento del mouse, Maiusc+click,
Seleziona tutto, Canc su un intervallo intero), Riempi in basso/a destra
fatto (menu Dati, sposta i riferimenti relativi nelle formule), Ordina
crescente/decrescente fatto (menu Dati, ordinamento stabile per righe
intere; nel verificarlo è emerso e risolto un bug generico di doppio
free in `Value::Value(CellData&)`), Annulla/Ripeti fatto (menu
Modifica, Ctrl+Z/Ctrl+Y, una sola pila di istantanee per intervallo
condivisa da tutte le operazioni che mutano il documento), Taglia/
Copia/Incolla e Formato numerico estesi dalla sola cella attiva
all'intero intervallo selezionato (formato TSV sugli appunti,
compatibile con Excel/LibreOffice Calc), Inserisci/Elimina riga e
colonna fatto (menu Dati, riusa `CContainer::MoveCell` gia' presente
nel motore ereditato ma mai esposto dalla UI nuova, aggiorna i
riferimenti anche nelle celle che non si spostano fisicamente);
intervalli con nome, Incolla speciale, Vai a, un vero Blocca riquadri
(le righe/colonne bloccate restano ferme sullo schermo durante lo
scroll, non solo le intestazioni), formattazione font/colore/
allineamento e una finestra Preferenze fatti (dettaglio nella sezione
Fase 7) — tutti i punti individuati nel confronto con Sum-It storico
recuperati. **Fase 8
(qualità UI/UX) chiusa**: protezione dalle modifiche non salvate
(Nuovo/Apri/Esci chiedono conferma solo se ci sono modifiche in
sospeso) con titolo finestra e indicatore di modifica,
ridimensionamento di righe e colonne (trascinando il confine fra due
intestazioni, con puntini e cursore come indizio visivo), icone sulla
toolbar (disegnate a codice, non da HVIF — il sito autorizzato
risultava ancora vuoto) — tutti e quattro i punti scelti dall'utente
fatti (nuovi bug/richieste possono comunque emergere e aggiungersi).
**Fase 9 (supporto multi-foglio) chiusa**: cartella di lavoro nativa
multi-foglio, formule che attraversano i fogli (risoluzione per nome,
non per indice — vedi la sezione dedicata per il perché — con
supporto ai nomi di foglio fra apici richiesti da spazi/trattini),
apertura automatica dal doppio clic in Tracker, una finestra per ogni
file aperto invece di una sola, toolbar con icone HVIF vere raggruppate
per categoria (il sito autorizzato per le icone si è nel frattempo
popolato) — tutti i punti pianificati fatti, verificato anche contro
il file di gara reale da 38 fogli che ha motivato l'intera fase.
**Fase 7 (recupero funzionalità rispetto a Sum-It storico) chiusa**:
intervalli con nome, Incolla speciale, Vai a, un vero Blocca riquadri,
formattazione font/colore/allineamento e una finestra Preferenze
fatti (oltre a Selezione multi-cella/Riempi/Ordina/Inserisci-Elimina
riga e colonna già completati nella prima parte della fase) — tutti i
punti individuati nel confronto con Sum-It storico recuperati.
**Fase 10 (persistenza completa delle preferenze) chiusa**: Blocca
riquadri, altezza di riga, font e allineamento — tutte e quattro le
preferenze rimaste "solo per sessione" dopo la Fase 7 — sopravvivono
ora al salvataggio/riapertura nel formato nativo. **Fase 11 (bordi
delle celle) chiusa**: l'ultimo campo di `CellStyle` mai esposto dalla
UI, mai implementato nemmeno nel Sum-It storico — un bordo nero
semplice per lato, con UI dedicata e persistenza nel formato nativo.
**Fase 12 (fedeltà visiva import XLSX) in corso**: aprire un file
Excel reale e complesso (bordi, celle unite, formati numero, tabelle,
formattazione condizionale, immagini) deve somigliare a quello che si
vede aprendolo con Excel vero su Windows, non solo importare valori e
colori grezzi — richiesto esplicitamente dall'utente dopo aver
riaperto il file di gara reale da 38 fogli e trovato la resa "ancora
carente". Aggiornato ad ogni fase completata.

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

**Nota** (aggiornata in Fase 6): i fix 3-6 rendevano il codice sicuro
(nessun crash/blocco), ma la causa di fondo dei bug 5-6 (tabella
funzioni mai caricata) restava: le formule con funzioni con nome non
erano ancora realmente utilizzabili. Risolto generando/allegando la
risorsa `'Func'` (con `bsl`/`rez`) e chiamando `InitFunctions()`
all'avvio dell'app — vedi Fase 6 sotto per il dettaglio.

**Perché sembravano blocchi infiniti invece di crash**: molti di
questi bug sono dereferenziazioni di puntatori nulli, che normalmente
darebbero un crash immediato. Su questa Haiku il processo restava
bloccato indefinitamente invece di terminare — ipotesi più probabile:
`debug_server` di Haiku intercetta il crash e sospende il thread in
attesa di un'interazione utente (debug/termina) che non arriva mai in
un'esecuzione headless da riga di comando. Usare sempre `timeout N`
nei test headless per non restare bloccati.

## Fase 4 — UI nativa Interface/Layout Kit (CHIUSA)

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
- [x] Dialogo Trova e sostituisci: finestra `FindWindow` (campo di
      ricerca, campo "Sostituisci con:", pulsanti "Trova successivo"/
      "Sostituisci"/"Sostituisci tutto"), voce "Trova e sostituisci…"
      nel menu Modifica. Non esegue la ricerca/sostituzione da sé (le
      celle appartengono al documento di `MainWindow`, su un altro
      thread/`BLooper`): inoltra i testi con un `BMessage` a un
      `BMessenger`, stessa regola del bug di thread
      `BApplication`/`BWindow` già corretto sopra. La ricerca
      (`MainWindow::FindNext`) scandisce le celle esistenti
      confrontando il testo (case-insensitive, sottostringa) e sceglie
      il primo risultato dopo la cella selezionata, con "wrap-around"
      se non ce n'è nessuno. "Sostituisci" sostituisce tutte le
      occorrenze nella cella selezionata poi passa al risultato
      successivo; "Sostituisci tutto" lo fa su ogni cella del
      documento che contiene il testo cercato, con un riepilogo del
      numero di celle modificate. **Non ancora fatto**: nessun menu
      Formato.
- [x] Toolbar: riga di `BButton` semplici (Nuovo/Apri/Salva/Stampa/
      Taglia/Copia/Incolla/Trova) sotto il menu, non `BToolBar` — quella
      classe vive solo sotto `develop/headers/private/shared/` su
      questo sistema, non nell'SDK pubblico stabile, e il progetto usa
      solo API pubbliche (nessuna nuova dipendenza da icone: pulsanti
      di solo testo, non icone-più-testo come un vero `BToolBar`).
      Ogni pulsante invia lo stesso `BMessage` già gestito dal menu
      corrispondente — nessuna logica nuova, solo un secondo punto di
      accesso alle stesse azioni. Verificato dal vivo: la barra compare
      correttamente con tutti e otto i pulsanti (screenshot).
- [x] Menu Formato: Generale/Numero/Valuta/Percentuale applicati alla
      cella selezionata, agendo su `CellStyle::fFormat`
      (`CContainer::GetCellStyle`/`SetCellStyle`, già esistenti nel
      motore) — nessuna nuova API dell'engine servita. `SheetView::Draw`
      ora controlla il formato della cella prima di applicare la
      formattazione locale-aware: valuta e percentuale usano
      `BNumberFormat::FormatMonetary`/`FormatPercent` (Locale Kit),
      generale/numero usano il raggruppamento numerico semplice già
      presente. **Verificato dal vivo** (invocando il menu con `hey`,
      non simulando `BMessage`): incollata una cella con "1234.5",
      applicato Valuta dal menu, la griglia mostra "1.234,50 €"
      mentre la barra formule resta sul valore grezzo "1234.5".
      **Non ancora fatto**: controllo del numero di decimali/font/
      colore/bordo (il motore li supporta tramite `CellStyle`, ma
      senza una UI dedicata restano fissi ai valori predefiniti).
      Con questo la Fase 4 è considerata sostanzialmente completa.
- [x] Export CSV: "Salva con nome" sceglie il formato dall'estensione
      del nome file (".csv" esporta in CSV, altrimenti resta sul
      formato nativo ASCD — non c'è ancora un selettore di formato
      dedicato). Il translator CSV aveva già entrambe le direzioni
      (`CTextConverter::ConvertToText`/`ConvertFromText`) fin dalla
      Fase 3: mancava solo instradare "Salva con nome" attraverso
      `BTranslatorRoster` invece di scrivere sempre ASCD a mano.
      Costruito per essere direttamente riusabile quando XLS/XLSX/ODS
      avranno anche loro un writer, non solo per CSV. **Bug scoperto e
      corretto costruendo questa funzione**: vedi sotto.
- [x] Export XLS/XLSX/ODS: **ODS e XLSX fatti; XLS legacy escluso
      deliberatamente**, non lasciato in sospeso per mancanza di tempo.
      `OdsTranslator` ora scrive anche verso ODS (`Identify()`
      riconosce un sorgente ASCD nativo in ingresso, `Translate()`
      instrada nelle due direzioni), riusando lo stesso `CZipReader` in
      lettura e un nuovo `CZipWriter` (voci "stored", non compresse,
      CRC32 via zlib) in `MiniZip.h`/`.cpp` — verificato non solo
      contro il proprio `CZipReader` ma anche contro `unzip` di sistema
      per escludere che l'archivio fosse valido solo per coincidenza.
      Come per l'export CSV, scrive solo i **valori calcolati**, non le
      formule (stessa scelta, stesso motivo: niente sintassi ODF
      arbitraria da ricostruire). Test di round-trip completo
      (ASCD → ODS → ASCD, incluso una cella con formula) in
      `translators/ods/tests/test_ods_translator.cpp`. Stesso approccio
      applicato a `XlsxTranslator` (`translators/xlsx/`): `CZipWriter`
      duplicato nella propria copia di `MiniZip.h`/`.cpp` (stessa scelta
      di non condividere codice tra translator), formato OOXML minimo
      ([Content_Types].xml, _rels/.rels, xl/workbook.xml,
      xl/_rels/workbook.xml.rels, xl/worksheets/sheet1.xml), stringhe
      scritte inline (`t="inlineStr"`, `<is><t>...</t></is>`) invece di
      costruire una tabella di stringhe condivise separata — più
      semplice, richiede pero' di aver esteso anche il parser XLSX in
      lettura per riconoscerle (prima gestiva solo `t="s"` verso
      `sharedStrings.xml`). Anche qui verificato con `unzip` di sistema
      oltre che col proprio `CZipReader`. Test di round-trip completo
      in `translators/xlsx/tests/test_xlsx_translator.cpp`.

      **XLS legacy escluso deliberatamente, non per mancanza di
      tempo**: verificato che non esiste da nessuna parte nel
      repository (né in `legacy/opensumit/` né nella porting `engine/`)
      alcun codice di scrittura BIFF/OLE2 su cui appoggiarsi —
      `Excel.OLE2.cpp` contiene solo `CExcel5Filter::GetBookStream`,
      lettura pura (naviga una FAT/directory OLE2 già esistente, non ne
      costruisce una), e `CExcelStream` ha solo deserializzatori
      (`operator>>`), nessun `operator<<`. Scrivere un export XLS
      legacy da zero richiederebbe sia un contenitore OLE2 Compound
      File completo (allocazione settori, FAT, MSAT per FAT oltre 109
      settori, voci di directory) sia un serializzatore di record BIFF
      — ordini di grandezza più complesso di ZIP+XML (dove è bastato un
      `CZipWriter` di ~100 righe sopra zlib). `translators/xls/
      XlsTranslator.h` documenta già questa scelta esplicitamente fin
      dalla Fase 3 ("l'export verso Excel moderno passerà dal futuro
      translator XLSX"): con ODS e XLSX ora entrambi funzionanti in
      scrittura, quella strategia è completa — un lettore Excel/
      LibreOffice moderno apre XLSX senza problemi, quindi non c'è un
      bisogno reale di un secondo formato di export verso lo stesso
      ecosistema Microsoft.

      **Due bug reali del motore scoperti costruendo l'export ODS**
      (lo stesso fix è stato applicato preventivamente a XLSX, che non
      ha quindi mai manifestato questi bug in prima persona),
      entrambi indipendenti dal formato ODS in sé (si manifestano
      identicamente in CSV/ASCD): vedi sotto.
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
- [x] Icone: il portale autorizzato www.hvif-store.art è risultato
      vuoto a due controlli separati (2026-07-29). Chiesto esplicitamente
      all'utente come procedere — ha scelto di disegnare l'icona da
      zero. Bozza in `ui/icons/atomo123.svg` (griglia bianca 3×3 con
      una cella evidenziata, su sfondo blu arrotondato — richiama
      chiaramente un foglio di calcolo), importata in Icon-O-Matic ed
      esportata in HVIF **direttamente dall'utente**
      (`ui/icons/atomo123.hvif`, verificato: firma `ncif` corretta).
      Incorporata come risorsa `VICN`/`BEOS:ICON` in `ui/Atomo123.rdef`
      (stesso meccanismo — `rc`+`xres` — già usato da altri progetti
      dell'utente su questo sistema, es. HaikuBench), ora parte
      automatica della build (`make` in `ui/` compila e allega la
      risorsa da solo). **Verificato dal vivo**: l'icona compare
      correttamente su Tracker in vista icone (screenshot), e
      `xres -l` conferma la risorsa `VICN` da 440 byte, identica per
      dimensione al file HVIF sorgente.

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

### Bug scoperto: le celle con formula non venivano mai ricalcolate al caricamento

Costruendo l'export CSV (che deve scrivere il *valore* calcolato di
una formula, non il testo della formula — a differenza di ASCD, CSV
non ha alcun concetto di formula) è emerso che una formula importata
da ASCD risultava sempre vuota nell'export, anche se il motore la
ricalcola correttamente quando richiesto esplicitamente (già
verificato nei test dei translator XLSX/ODS).

Causa: `TryToParseString` (usata da `LoadASCD` e dal `ReadASCD` del
translator CSV per popolare le celle da un flusso ASCD) imposta la
formula o il valore di una cella ma **non la calcola** — serve una
chiamata esplicita a `CalcCell` per ciascuna cella. Nessuno dei due
punti la faceva. In pratica questo significava che **qualunque file
aperto nell'app con celle a formula le mostrava vuote nella griglia**
finché l'utente non toccava quella cella a mano (barra formule o
editing in-cella, che *chiamano* `CalcCell` dopo aver scritto) — un
problema di usabilità serio, passato inosservato finora perché ogni
test dei translator (XLSX/ODS) chiamava `CalcCell` esplicitamente
*nel test stesso* per verificare che il motore ricalcolasse
correttamente, mascherando il fatto che il percorso di produzione
(apertura di un file nell'app vera) non lo faceva mai da solo.

**Fix**: aggiunta `RecalculateAll(CContainer*)` (nuova funzione
pubblica in `ui/src/AscdIO.h/.cpp`, usata da `LoadASCD`) e la stessa
logica duplicata nel `ReadASCD` del translator CSV (stesso approccio
di duplicazione intenzionale già usato per `WriteASCD`/`ReadASCD` fra
app e translator, per non introdurre una dipendenza di link). La
funzione itera su tutte le celle e chiama `CalcCell` su ciascuna,
**ripetendo finché nessuna cella cambia più valore** (con un limite di
sicurezza di 50 passate): `CFormula::Calculate` legge i riferimenti ad
altre celle con una semplice `GetValue` non ricorsiva, quindi
l'ordine in cui le celle vengono inserite non garantisce che una
cella referenziata sia già stata calcolata — più passate propagano
correttamente le dipendenze in qualunque ordine, senza dover
implementare un vero ordinamento topologico del grafo delle
dipendenze.

**Verificato**: `ui/tests/test_ascd_io.cpp` ora controlla il valore
di C1 **subito dopo** `LoadASCD`, prima di qualunque `CalcCell`
esplicito nel test (prima controllava solo dopo un `CalcCell` fatto
apposta dal test, che nascondeva il bug) — verde. Anche un test
diretto dell'export CSV (harness a parte, non nella suite committata)
ha confermato che una formula `=A1+B1` con A1=10/B1=20 esporta
correttamente "30" invece che una cella vuota.

### Bug scoperto: la griglia non riempiva la finestra (segnalato dall'utente)

Aprendo l'app a schermo intero, la griglia mostrava solo la colonna A
e le prime 4 righe (~100×100 pixel in alto a sinistra), con il resto
della finestra vuoto — non uno sfondo scorrevole, proprio nessuna
riga/colonna disegnata oltre quel piccolo riquadro.

Causa: `SheetView` veniva costruita con un `Frame()` fisso e minuscolo
(`BRect(0, 0, 100, 100)`, un placeholder mai più ridimensionato).
`Draw()` calcola l'intervallo di celle da disegnare a partire da
`updateRect`, che per una `BView` non può mai eccedere il suo stesso
`Frame()` — quindi, indipendentemente da quanto grande fosse la
finestra o la `BScrollView` a schermo, non veniva mai generato un
`updateRect` più grande di quel riquadro iniziale di 100×100.

**Fix**: `SheetView` ora si costruisce con un `Frame()` che copre
l'intero intervallo virtuale del motore (`kColCount`×`kColWidth` per
`kRowCount`×`kRowHeight`, ~56200×327700 pixel) fin dalla costruzione
— il pattern classico BeOS/Haiku per una vista scorrevole, dove la
`BScrollView` ritaglia e scorre una vista grande invece di ridimensionare
una vista piccola per adattarla al contenuto. Conseguenza collaterale
corretta insieme: `Bounds()` di una vista così ora riflette sempre la
dimensione piena (mai la porzione visibile), quindi sia
`FixupScrollBars()` (calcolo dell'intervallo delle scrollbar) sia
`ScrollToShowSelection()` (scorrimento automatico verso la cella
selezionata) — che prima usavano `Bounds()` assumendo riflettesse
l'area visibile — ora usano `Parent()->Bounds()` (l'area visibile
reale della `BScrollView`) per le dimensioni, e `Bounds().left/top`
(l'unica parte che `ScrollBy`/`ScrollTo` aggiornano davvero) per
l'origine dello scroll.

**Verificato dal vivo**: screenshot dopo il fix mostra colonne A-J e
righe 1-25 che riempiono correttamente la finestra (prima: solo
colonna A, righe 1-4); ridimensionata la finestra due volte via `hey`
per esercitare `FixupScrollBars()` ripetutamente, nessun crash.

### Bug scoperto (parte 2, stesso giorno): lo scroll automatico verso la cella selezionata non funzionava

L'utente ha segnalato che spostandosi con il cursore (o cercando con
"Trova") verso una cella fuori dall'area visibile, la vista non
scorre per mostrarla — nonostante `ScrollToShowSelection()` fosse
gia' stata corretta poche ore prima (vedi bug sopra) per usare
`Parent()->Bounds()` invece di `Bounds()`.

**Diagnosi**: un harness diretto (`ui/tests/test_scroll.cpp`, poi
promosso a test permanente) che costruisce una vera `BWindow` +
`BScrollView` + `SheetView` e chiama `SetSelection()` su una cella
lontana ha riprodotto il bug in isolamento, rivelando la causa reale:
`BScrollView`, costruita con la forma classica (`BScrollView(name,
target, resizeMask, flags, horizontal, vertical)`, non tramite
`BLayoutBuilder`), **eredita di default la dimensione del proprio
target** invece di farsi vincolare dal layout della finestra. Dato
che `SheetView` ha ora un `Frame()` enorme (~56200×327700 pixel, il
fix del bug precedente), anche la `BScrollView` diventava enorme —
`Parent()->Bounds()` restituiva quindi quella stessa dimensione
sbagliata (confermato nel test: `Parent()->Bounds()` risultava
~56217×327717 invece di una viewport ragionevole), rendendo inutile
il fix precedente: la vista *pensava* di essere gia' "abbastanza
grande" da mostrare qualunque cella, quindi non scorreva mai.

**Fix**: un `ResizeTo()` esplicito sulla `BScrollView` subito dopo la
costruzione (`MainWindow::MainWindow`, prima di aggiungerla al
layout) la "sgancia" dalla dimensione ereditata dal target — dopo
quella chiamata il layout la ridimensiona liberamente in base allo
spazio disponibile nella finestra, e `Parent()->Bounds()` torna a
riflettere la vera area visibile.

**Verificato**: `ui/tests/test_scroll.cpp` (richiede una sessione
grafica, target Makefile `test-scroll` separato come `test-clipboard`)
verifica cinque cose: la `BScrollView` ha una dimensione ragionevole
(non eredita il canvas virtuale del target), la selezione iniziale e'
A1, nessuno scroll prima di selezionare una cella lontana, la vista
scorre selezionando una cella fuori schermo (colonna 30, riga 200), e
torna all'origine riselezionando A1 — tutti verdi. Anche la
regressione sul bug precedente (griglia che riempie la finestra)
riverificata dal vivo, nessun cambiamento visivo.

**Nota sugli strumenti di test usati in questa sessione**: durante
l'indagine e' stato scoperto e provato **Pippo**
(`/Magazzino/Pippo`), un server MCP nativo per Haiku dell'utente che
espone iniezione reale di mouse/tastiera, screenshot, scripting app
via HTTP+SSE su `localhost:2607` — vedi la nota dedicata più sotto
("Integrazione con hey e Pippo") con i dettagli su cosa ha funzionato
e cosa no in questo tentativo.

### Bug scoperto (parte 3, sessione successiva): lo scroll tornava a rompersi dopo il primo ricalcolo del layout

L'utente ha rifatto una prova reale dopo il fix della parte 2 e ha
segnalato di nuovo lo stesso identico sintomo (frecce fino a K3,
l'etichetta di riferimento mostra "K3" ma la griglia non scorre) —
nonostante `test_scroll.cpp` continuasse a passare tutti i controlli.
Iniezione di tasti freccia via Pippo/`hey` di nuovo inaffidabile
(niente di nuovo rispetto a quanto già documentato); la diagnosi è
stata fatta interrogando la geometria reale della finestra in
esecuzione con `hey <app> get Frame of View "scroll" of Window 0`
(mai usato prima in questo progetto): risultato `BRect(0, 94, 56217,
327811)` — la `BScrollView` aveva di nuovo la dimensione enorme
ereditata dal target, lo stesso bug della parte 2, ma tornato
indietro.

**Causa**: il `ResizeTo()` della parte 2 sgancia la `BScrollView`
dalla dimensione ereditata solo alla primissima passata di layout;
senza un limite esplicito sulla *view target* (`SheetView`) stessa,
ogni ricalcolo successivo del layout (che nell'app vera, con più
righe sopra la griglia, avviene diversamente che nel test sintetico
con la sola `BScrollView`) torna a interrogare il `Frame()` enorme
del target.

**Fix**: `SetExplicitMinSize`/`MaxSize`/`PreferredSize` su
`SheetView` stessa (`SheetView.cpp`, costruttore) — non solo sulla
`BScrollView`. Verificato dal vivo con la stessa query prima/dopo il
fix, e anche dopo un ridimensionamento forzato della finestra (per
escludere che il bug si ripresentasse a un *terzo* ricalcolo).
Dettaglio tecnico completo, inclusa la tabella con i valori esatti
osservati, in `docs/UI_ARCHITECTURE.md`.

**Lacuna nota**: `test_scroll.cpp` non riproduce questo bug specifico
nonostante un tentativo di arricchire il layout del test per
somigliare a `MainWindow` — resta una verifica solo dal vivo, non
automatica. Onestamente documentato invece di lasciare un'asserzione
fuorviante che passerebbe comunque.

### Nota per il futuro: integrazione con `hey` e con Pippo (MCP)

Richiesta esplicita dell'utente: valutare un'integrazione più a fondo
con `hey` (scripting BeOS nativo, già usato ampiamente in questo
progetto per aprire/testare l'app) e con **Pippo**
(`/Magazzino/Pippo`, server MCP nativo per Haiku dell'utente,
`localhost:2607`, JSON-RPC via `curl -X POST .../mcp`). Nella sessione
in cui e' stato scoperto il bug dello scroll, Pippo e' stato provato
per la prima volta come strumento di test per questo progetto — utile
riassumere cosa ha funzionato e cosa no, per non ripartire da zero la
prossima volta.

**Cosa ha funzionato bene**:
- `focus_window`/`list_windows` — affidabili per portare in primo
  piano la finestra principale di Atomo123 (Window index 0) e
  leggerne il frame.
- `key_stroke` con `chars` (digitazione di testo normale, incluso
  `"\n"` per Invio) — funziona correttamente per digitare in un
  campo con il fuoco tastiera giusto, stessa affidabilità
  dell'incollare da `be_clipboard` già usato altrove nel progetto.
- `screenshot`/`screenshot_region` — alternativa diretta al tool
  `screenshot` da riga di comando già usato in tutta questa sessione,
  con il vantaggio di restituire l'immagine come base64 nella
  risposta MCP invece di scrivere un file da leggere separatamente.

**Cosa non ha funzionato / limiti scoperti**:
1. **`mouse_move`/`mouse_down`/`mouse_up` con `where:{x,y}` falliscono
   sempre** ("mouse_move requires where={x,y}", anche passando
   coordinate valide). Causa (in `McpDispatcher::_BuildHayCommand`,
   `/Magazzino/Pippo/src/pippo/McpDispatcher.cpp`): il parsing JSON→Hay
   fatto a mano dovrebbe riconoscere un oggetto `{"x":N,"y":M}` e
   convertirlo in `BPoint(N,M)`, ma la conversione non scatta nei casi
   provati in questa sessione — non approfondito oltre (non è codice
   di questo progetto), segnalabile all'autore di Pippo.
2. **I codici scancode per le frecce direzionali (`key_down` con
   `key=0x57/0x61/0x62/0x63`, verificati corretti contro
   `keymap -d /boot/system/data/Keymaps/US`) vengono interpretati
   come caratteri normali** (es. `key=0x63` digita la lettera "c"
   invece di muovere il cursore) invece che come tasti speciali —
   suggerisce che l'add-on `hay_input` di Pippo non popoli
   correttamente i campi `bytes`/`raw_char` del `B_KEY_DOWN` sintetico
   per i tasti non stampabili. Non risolto in questa sessione.
3. **Le finestre secondarie (es. `FindWindow`, "Window 1") sono
   inaffidabili da indirizzare sia con `hey` sia con `focus_window` di
   Pippo** — query su "Window 1" con `hey` restanto appese
   indefinitamente (mai un errore rapido, serve interrompere a mano),
   e `focus_window` con un indice diverso da 0 non garantisce che il
   fuoco tastiera vada davvero li'.
4. **Ambiente desktop condiviso**: durante questa sessione erano
   attive contemporaneamente almeno altre due sessioni Claude Code
   indipendenti sullo stesso desktop (una su un progetto
   "VideoChiamate", una su un client Spotify/librespot) — questo ha
   causato interferenze reali (finestre di Atomo123 chiuse
   dall'esterno più volte, un tentativo di `key_down` finito nel
   terminale sbagliato digitando testo non voluto in un prompt di
   un'altra sessione, per fortuna innocuo). Da tenere presente: su un
   desktop condiviso, `focus_window` + azione immediata (senza pause)
   riduce ma non elimina il rischio che un'altra sessione rubi il
   fuoco nel frattempo.

**Conclusione pratica**: per digitare testo e fare screenshot, Pippo
è già utilizzabile oggi. Per simulare tasti di navigazione (frecce) o
click del mouse serve prima risolvere i due bug sopra (non di questo
progetto) — fino ad allora, per quei casi conviene continuare con la
tecnica già consolidata in questo progetto: un harness diretto in
processo che chiama i metodi C++ della vista (come
`ui/tests/test_scroll.cpp`), più affidabile e più preciso di
qualunque iniezione di input quando quello che serve è verificare la
logica, non l'esperienza utente end-to-end.

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
      pubblicato.
- [x] Licenza: l'utente ha scelto **MIT** per il codice nuovo di
      questo progetto. Aggiunto `LICENSE` alla radice del repository,
      con lo scopo chiarito esplicitamente: MIT per `translators/`,
      `ui/`, `docs/`, `packaging/` e i file alla radice; `engine/`
      resta invece sotto la licenza originale BSD a 4 clausole (con
      clausola pubblicitaria) di Sum-It, perché è un'estrazione/
      modifica di quel codice storico, non una riscrittura da zero —
      non si poteva riassegnarlo a MIT senza l'autorizzazione del
      detentore originale del copyright (Hekkelman Programmatuur
      B.V.), quindi non è stato fatto anche se la richiesta dell'utente
      era per "il progetto" nel suo complesso. `legacy/opensumit/`
      resta ovviamente sotto la sua licenza originale invariata.
      `README.md` e il campo `LICENSE` di `packaging/atomo123-0.1.0.recipe`
      aggiornati di conseguenza.
- [ ] Test di compatibilità con corpus di file reali: **primo file XLS
      reale verificato** (scaricato per un test estemporaneo, non un
      vero corpus sistematico), aprendolo sia a livello di translator
      sia dal vivo nell'app vera — ha fatto emergere tre bug reali nel
      lettore BIFF/OLE2 legacy (vedi sezione dedicata in
      `docs/TRANSLATORS.md`), tutti corretti. Resta un vero corpus
      eterogeneo di file XLS/XLSX/ODS di varie versioni generati da
      Excel/LibreOffice/OpenOffice in condizioni non controllate — un
      solo file XLS non basta a dire il formato pienamente compatibile,
      solo che il caso comune (righe/colonne/font/valori) ora funziona.
      Il file di test usato non è stato incluso nel repository (licenza
      di ridistribuzione non chiara): servirebbe un file campione con
      licenza libera per fissarlo come fixture di test automatizzato.

### Bug scoperto: `GetBounds` esclude le celle non ancora calcolate, rompendo il ricalcolo/salvataggio quando sono ai margini del foglio

Costruendo il test di round-trip dell'export ODS (una formula come
cella più a destra di un foglio, senza nessun'altra cella "reale" oltre
di lei) `RecalculateAll`/`SaveASCD` (`ui/src/AscdIO.cpp`) e le funzioni
gemelle nei translator (`ReadASCD`/`WriteASCD` in
`CsvTranslator.cpp`/`OdsTranslator.cpp`) restavano tutte silenziosamente
mute su quella cella: la formula non veniva mai calcolata, e se il
documento veniva risalvato la cella spariva del tutto dal file.

Causa: `CContainer::GetBounds` esclude dal rettangolo calcolato le
celle con `mType == eNoData` — esattamente lo stato di una formula
appena impostata da `TryToParseString` e non ancora passata da
`CalcCell` (che le assegna un tipo reale). Tutte queste funzioni
calcolavano i limiti del foglio con `GetBounds` **prima** di iterare
per ricalcolare/scrivere, quindi se la cella "ancora da calcolare" era
anche quella più a destra o in basso, restava fuori dal rettangolo e
`CCellIterator` non la visitava mai — né in quella passata né in quelle
successive, perché i limiti non venivano più ricalcolati.

Bug reale anche nell'uso dal vivo, non solo nei translator:
`CommitEditing` chiama `RecalculateAll` dopo ogni conferma (fix di Fase
6, vedi sotto), quindi digitare una nuova formula proprio nell'angolo
in basso a destra del foglio la lascerebbe vuota finché non si tocca
un'altra cella che sposti i limiti oltre quel punto.

**Fix**: le quattro funzioni (`RecalculateAll`, `SaveASCD` in
`AscdIO.cpp`, `ReadASCD`/`WriteASCD` in `CsvTranslator.cpp` e
`OdsTranslator.cpp`) ora iterano con `CCellIterator(doc, NULL)` — il
range completo del foglio invece dei limiti di `GetBounds` — anziché
un rettangolo che può escludere proprio la cella che serve raggiungere.
Resta comunque efficiente: `NextExisting` salta direttamente da una
cella esistente alla successiva tramite la mappa ordinata, senza
scandire le celle vuote in mezzo, quindi il costo dipende dal numero di
celle realmente presenti, non dalla dimensione del rettangolo. Test di
non regressione in `ui/tests/test_ascd_io.cpp` (una formula come cella
più a destra del foglio, salvata e ricaricata senza essere mai
calcolata a mano prima del salvataggio).

### Bug scoperto (falso allarme, ma test-harness da correggere): `GetCellFormula` di una formula con una costante numerica si blocca senza un `BApplication`

Scrivendo il test di cui sopra, `SaveASCD` si bloccava indefinitamente
— non nella logica appena descritta, ma dentro `GetCellFormula` per una
formula come `=A1+10` (riferimento a cella più costante numerica).
Isolato con un mini programma a sé stante e tracciando gli opcode
attraversati da `CFormula::UnMangle` (`engine/src/Formula/Formula.cpp`):
il blocco non è nella macchina a pila che ricostruisce il testo della
formula, ma dentro `ftoa()` (`engine/src/Cell/Formatter.cpp`), chiamata
per convertire la costante numerica in testo, che a sua volta chiama
`gFontSizeTable[0].Font().StringWidth("0.000000")` per l'allineamento
decimale. Senza un `BApplication` registrato, quella chiamata resta
bloccata in attesa di una risposta dall'app_server che non arriverà
mai — confermato costruendo due varianti dello stesso mini test,
identiche a parte l'aggiunta di un `BApplication`, che risolve il
blocco all'istante.

A differenza dei bug headless già documentati in
`docs/TRANSLATORS.md` (dereferenziazioni di puntatore nullo che
il debug_server intercetta e sospende in attesa di un'interazione
grafica mai arrivata, facendo *sembrare* un blocco quello che in
realtà sarebbe stato un crash), questo è un vero blocco IPC: una
chiamata legittima all'app_server che semplicemente non ha nessuno
dall'altra parte a rispondere. Non è quindi un bug del motore di
calcolo (nell'app vera un `BApplication` è sempre presente), ma un
buco nell'infrastruttura di test: `ui/tests/test_ascd_io.cpp`
dichiarava di "non richiedere una sessione grafica" ma in realtà può
attraversare codice che ha bisogno di un `BApplication` per le
metriche del font, appena una formula contiene una costante numerica.
**Fix**: aggiunto un `BApplication` (senza mostrare nessuna finestra)
all'inizio del test.

### Tre bug reali scoperti aprendo un vero file XLS legacy

Con l'export ODS/XLSX chiuso, si è colta l'occasione per verificare per
la prima volta `translators/xls` (import BIFF/OLE2 legacy) con un file
`.xls` realmente generato da uno strumento esterno, non costruito a
mano — finora testato solo con OLE2 malformati (Fase 3). Aprirlo, prima
a livello di translator e poi dal vivo nell'app vera, ha fatto emergere
tre bug reali distinti, tutti preesistenti nel codice storico e mai
manifestati prima perché mai esercitati da dati reali:

1. `long`/`unsigned long` a 64 bit su Haiku x86_64 invece dei 32 bit
   per cui il codice storico (BeOS/PPC) era stato scritto, in
   `Excel.OLE2.cpp` — stessa famiglia di bug già corretta altrove nel
   progetto (`cell::operator<` in `Cell.h`), qui rimasta perché mai
   esercitata da un file reale. Corretto sostituendo con `int32`/
   `uint32` fissi.
2. La directory OLE2 non seguiva la propria catena di settori nella
   FAT, assumendo (a torto, per un file con più di quattro voci nella
   directory) che stesse tutta in un solo settore da 512 byte.
   Corretto seguendo la catena, come già faceva il codice per lo
   stream `Workbook` stesso.
3. `fCellView` (sempre `NULL` nei translator headless, per design)
   dereferenziato senza controllo in diversi punti di
   `HandleXLRecordForPass1`/`Selection`/`Name` (altezza righe,
   larghezza colonne, impostazioni finestra, selezione, nomi
   definiti) — record comuni in qualunque foglio reale, quindi il bug
   si manifestava con quasi ogni file `.xls` autentico. Corretto con
   controlli `if (fCellView)`, coerenti con un commento già presente
   nel codice che ne documentava l'intento ("questi metadati vengono
   scartati in questa modalità") senza che il codice lo rispettasse
   davvero.

Dettaglio tecnico completo in `docs/TRANSLATORS.md`, sezione
`translators/xls`. Verificato non solo a livello di translator ma
aprendo davvero il file nell'app (`Atomo123 file.xls`), confermando
che l'intera catena BTranslatorRoster → ASCD → `SheetView` regge senza
crash. Il file usato per il test non è nel repository (licenza di
ridistribuzione non chiara) — resta un gap per un vero test
automatizzato end-to-end con fixture reale, annotato nell'item
"Test di compatibilità con corpus di file reali" qui sopra.

## Fase 6 — Polish e funzionalità avanzate (CHIUSA)

- [x] Funzioni con nome nelle formule (`SUM`, `IF`, `MAX`, ecc.): il
      gap più importante rimasto dalla Fase 3 (l'engine non chiamava
      mai `InitFunctions()`, quindi ogni nome di funzione era trattato
      come identificatore sconosciuto). Risolto costruendo `rez`/`bsl`
      (strumenti storici di Sum-It, ricompilati in Fase 1 ma mai usati
      finora) e usandoli per compilare `engine/resources/funcs_by_nr.r`
      + `FuncNames.txt`/`FuncDescs.txt` (copie immutate dei file
      storici in `legacy/opensumit/sum-it/Resources/`) in una risorsa
      `'Func'`/due `'StrL'` allegate al binario `Atomo123` con `xres`
      (`ui/Makefile`, target `$(RSRC)`, si fondono con la risorsa
      icona già esistente). `App::ReadyToRun()` lega
      `gResourceManager`/`gAppName` al binario e chiama
      `InitFunctions()` prima di creare la finestra, dentro un
      `try`/`catch` che degrada senza crash se le risorse mancassero.
      Un dettaglio non ovvio scoperto scrivendo il test: il separatore
      fra argomenti è `;` (`gListSeparator`), non `,` — `=IF(A1>5,100,200)`
      dà errore di formula, `=IF(A1>5;100;200)` funziona; documentato
      in `docs/USER_GUIDE.md`. Verificato con un nuovo test in
      processo, `engine/tests/named_functions_test.cpp` (`make
      test-functions`): `=SUM(A1:A3)`, `=IF(...)`, `=MAX(...)`
      calcolano il risultato corretto; build completa (`make -C ui
      clean && make -C ui all`) e `make -C ui test`/`test-scroll`
      confermati senza regressioni. Dettaglio tecnico completo in
      `docs/ENGINE_API.md`.
- [x] Grafici (a barre) e tabelle pivot di base: nuovo menu
      "Inserisci" con due voci. Entrambe le funzionalità leggono un
      intervallo di due colonne digitato dall'utente (etichetta/
      categoria testuale, valore numerico) — la griglia oggi supporta
      solo la selezione di una singola cella, non un intervallo
      trascinato col mouse, quindi l'intervallo si scrive a mano
      (es. `A1:B5`), non si seleziona sul foglio.
      **Grafico a barre**: `ChartWindow` (campo intervallo + area di
      anteprima `ChartView`, "Disegna") mostra un'anteprima statica,
      ma il pulsante "Inserisci nel foglio" (con una cella di
      destinazione) lo incorpora davvero come oggetto sulla griglia
      (`ChartObject` in `Chart.h`, disegnato da `SheetView::Draw`) —
      a differenza dell'anteprima, il grafico incorporato **rilegge i
      dati dal vivo a ogni ridisegno** (non un'istantanea), quindi
      riflette da solo le modifiche alle celle sorgente, e sopravvive
      al salvataggio/ricaricamento nel formato nativo ASCD (nuova
      sezione in coda al formato, opzionale e retrocompatibile — un
      file scritto prima di questa modifica si rilegge normalmente
      senza grafici, vedi `ui/src/AscdIO.cpp`). Aggiunta anche una
      correzione collegata: riaprire un file nativo passava comunque
      dal Translation Kit, che lo faceva rileggere/riscrivere dalla
      copia duplicata di `ReadASCD`/`WriteASCD` di un translator
      qualunque (es. `translators/csv/CsvTranslator.cpp`) — innocuo
      prima, ma avrebbe silenziosamente perso i grafici incorporati;
      ora un file già nativo si legge direttamente con `LoadASCD`,
      bypassando quel giro superfluo.
      **Tabella pivot**: `PivotWindow` raggruppa per categoria e
      aggrega (Somma/Conteggio/Media, un solo livello — non un pivot
      multidimensionale come Excel), poi scrive il risultato
      direttamente nel foglio a partire da una cella di destinazione
      scelta dall'utente (con controllo che non si sovrapponga
      all'intervallo sorgente); a differenza del grafico, il
      risultato è una scrittura una tantum (nuove celle vere e
      proprie), non un oggetto che si aggiorna da solo.
      Logica (`ui/src/Chart.cpp`, `ui/src/Pivot.cpp`) separata dalla
      UI apposta per essere testabile senza sessione grafica (`make
      test-chart`, `make test-pivot`, `make test` esteso per la
      persistenza dei grafici — 39 asserzioni in totale) — la cella
      di destinazione/intervallo si analizza con `cell::GetCell`, lo
      stesso parser di riferimenti già usato dal motore per le
      formule. Entrambe le finestre seguono la stessa regola sui
      thread già stabilita da `FindWindow` (mai toccare il documento
      da un thread diverso da quello di `MainWindow`: si scambiano
      `BMessage` via `BMessenger` in entrambe le direzioni).
      Dettaglio tecnico in `docs/UI_ARCHITECTURE.md`.
      **Limiti noti di questa prima versione**: solo grafico a barre
      (niente a linee/torta), un solo livello di raggruppamento nella
      pivot, dimensione del grafico incorporato fissa (non
      ridimensionabile/spostabile dopo l'inserimento), nessuna
      interfaccia per rimuoverne uno già inserito.
- [x] Intestazioni "congelate" durante lo scroll: la riga delle
      lettere di colonna resta fissa in alto durante lo scroll
      verticale, la colonna dei numeri di riga resta fissa a sinistra
      durante lo scroll orizzontale — richiesta esplicita dell'utente.
      `SheetView::Draw` disegna le due bande a `Bounds().top`/
      `Bounds().left` (la porzione attualmente visibile) invece che a
      una posizione fissa nel canvas virtuale. Bug scoperto e corretto
      durante l'implementazione: senza altro, lo scroll causava uno
      sfarfallio (banda dell'intestazione "fantasma" lasciata dal blit
      di `ScrollBy`/`ScrollTo`) — risolto con un override di
      `SheetView::ScrollTo` che invalida solo le bande vecchia/nuova
      delle intestazioni, non l'intera vista. Verificato dal vivo
      dall'utente (non da un test automatico: un tentativo di verifica
      a livello di pixel con una vista offscreen ha causato un
      hang/crash del test, abbandonato). Dettaglio tecnico in
      `docs/UI_ARCHITECTURE.md`.
- [x] Invio non confermava l'editing in-cella (bisognava cliccare col
      mouse): `BTextControl` non generava in modo affidabile il suo
      `Invoke()` automatico su Invio in questo contesto d'uso — causa
      esatta non isolata con certezza, ma non più rilevante dato il
      fix. Risolto intercettando Invio esplicitamente con lo stesso
      `BMessageFilter` già usato per Escape (`CellEditKeyFilter`),
      che manda il messaggio di commit direttamente — non dipende più
      dal comportamento automatico del controllo, come il click del
      mouse (che già funzionava, chiamando `CommitEditing()`
      direttamente). Aggiunto anche l'avanzamento della selezione alla
      cella sotto dopo la conferma (come Excel/LibreOffice Calc, non
      c'era prima).
- [x] Scorciatoie da tastiera in stile Excel/LibreOffice Calc,
      richiesta esplicita dell'utente: Inizio/Ctrl+Inizio, Ctrl+Fine,
      PagSu/PagGiù, Maiusc+Tab, Maiusc+Invio — un sottoinsieme
      implementabile senza nuova architettura (non la selezione di un
      intervallo con Maiusc+frecce, che richiederebbe prima
      introdurre il concetto di intervallo selezionato nella griglia,
      oggi assente). Refactor `SheetView::KeyDown` →
      `HandleKey(char, bool ctrl, bool shift)` pubblico, per restare
      testabile senza un vero dispatch della tastiera. Verificato con
      un nuovo test, `ui/tests/test_navigation.cpp` (`make
      test-navigation`, 9 asserzioni). Dettaglio tecnico in
      `docs/UI_ARCHITECTURE.md`.
- [x] Funzioni aggiuntive: `SUMIF`/`COUNTIF`/`AVERAGEIF`, assenti dalle
      86 funzioni originali di Sum-It (aggregazione condizionata,
      mancava proprio dal set storico) nonostante siano fra le più
      usate in un foglio di calcolo moderno. Il criterio accetta un
      numero (o testo numerico) per confronto esatto, testo per
      confronto letterale (senza distinguere maiuscole/minuscole), o
      un operatore di confronto stile Excel in testa (`">10"`,
      `"<=5"`, `"<>0"`) seguito da un numero. Stesso meccanismo di
      registrazione delle altre 86 funzioni (enum in `Functions.h`,
      voce in `engine/resources/funcs_by_nr.r`, mappatura in
      `SetupDefaultFuncs`) — nessuna modifica architetturale, solo
      nuove voci. **Bug scoperto aggiungendole**: `GetFunctionNr`
      (`engine/src/Utils/Utils.cpp`) scartava per un off-by-one i nomi
      di funzione di esattamente 9 caratteri (`AVERAGEIF` trattato
      come identificatore sconosciuto nonostante fosse nella tabella)
      — nessuno degli 86 nomi originali arrivava a 9 caratteri, quindi
      il bug non si era mai manifestato. Corretto (`sLen >= 9` →
      `sLen >= sizeof(myFunc)`). Verificato estendendo
      `engine/tests/named_functions_test.cpp` (`make test-functions`,
      ora 8 asserzioni): `=SUMIF(D1:D4;"Mela";E1:E4)`,
      `=COUNTIF(D1:D4;"Mela")`, `=AVERAGEIF(D1:D4;"Mela";E1:E4)`,
      `=SUMIF(E1:E4;">8")` calcolano tutti il risultato corretto;
      nessuna regressione nella suite completa (engine + tutti i test
      `ui/`).
- [x] Ricalcolo: non era un problema di velocità su fogli grandi come
      il titolo originale di questa voce ipotizzava, ma un **bug di
      correttezza** più fondamentale, scoperto indagando prima di
      ottimizzare qualunque cosa — `CContainer` non tiene un grafo
      delle dipendenze, e ogni punto di modifica (editing in-cella,
      barra formule, taglia/incolla/cancella, trova e sostituisci)
      chiamava `CalcCell()` **solo sulla cella appena modificata**,
      mai su altre celle che la referenziano in formula altrove.
      Verificato concretamente: `A1=10`, `B1="=A1+5"` (→10 corretto),
      poi `A1` modificato a `20` **senza toccare B1** — `B1` restava
      fermo a `15` invece di aggiornarsi a `25`. Un caso d'uso
      comunissimo (una cella "totale" che referenzia altre celle)
      rotto silenziosamente. **Fix**: tutti i punti di modifica
      chiamano ora `RecalculateAll(fDoc)` (già esistente e testata,
      usata finora solo al caricamento file) invece del solo
      `CalcCell()` sulla cella toccata — anche in `DeleteSelection`/
      Backspace-Canc, che prima non ricalcolava proprio nulla, nemmeno
      la cella cancellata. Il costo (più passate, ma solo sulle celle
      **con contenuto**, non sull'intero foglio virtuale — vedi
      `GetBounds()`) resta accettabile anche su fogli grandi: stesso
      meccanismo già verificato al caricamento file, ora usato anche
      a ogni modifica. Verificato estendendo `ui/tests/test_editing.cpp`
      con un test esplicito di propagazione (B4 si aggiorna da solo
      quando si modifica A4 altrove); nessuna regressione nella suite
      completa. Dettaglio tecnico in `ui/src/AscdIO.h`.
- [x] Documentazione utente: `docs/USER_GUIDE.md` — avvio, editing
      (barra formule e in-cella), formule (con il limite delle
      funzioni con nome ancora non implementate), apertura/salvataggio
      file (solo import da CSV/XLS/XLSX/ODS, export solo verso ASCD
      nativo), taglia/copia/incolla, Trova, stampa (col limite delle
      intestazioni non ripetute per pagina), formattazione numeri
      locale-aware. Aggiornata linkata anche da `README.md`, insieme
      ai documenti tecnici di Fase 2/3/4 che non erano ancora
      referenziati lì.

## Fase 7 — Recupero funzionalità rispetto a Sum-It storico (CHIUSA)

La "Decisione architetturale di fondo" (sopra) ha sempre significato
che la UI riscritta da zero avrebbe coperto solo un sottoinsieme delle
funzionalità del vecchio Sum-It, non un porting 1:1. Con Fase 4 e Fase
6 chiuse, si è fatto un confronto puntuale (menu/comandi/dialoghi,
file per file, sia sul codice storico in `legacy/opensumit/sum-it/`
sia sulla UI nuova in `ui/src/`) per capire con precisione cosa manca
davvero e quanto costerebbe recuperarlo. Risultato in sintesi (analisi
completa nella cronologia della sessione, non duplicata qui):

**Già allineato** (presente in entrambi, seppure con perimetro ridotto
in Atomo123): taglia/copia/incolla, cancella, trova/sostituisci,
formattazione numero (4 categorie invece di ~9), grafici (solo barre,
ma incorporati e aggiornati dal vivo), stampa, import Excel legacy.

**Mancante rispetto a Sum-It** (in ordine di impatto stimato, non
tutto necessariamente da recuperare): Annulla/Ripeti (assente del
tutto fino a questa fase, ora aggiunto, vedi sotto), selezione
multi-cella (assente fino a questa fase), Ordina, Riempi (in
basso/a destra/serie), inserisci/elimina riga e colonna,
ridimensionamento riga/colonna, formattazione font/colore/allineamento,
Incolla speciale, intervalli con nome, Vai a, un vero Blocca riquadri
(quello attuale è solo un effetto grafico di rendering, non
attivabile/disattivabile), una finestra Preferenze, Seleziona tutto
(ora aggiunto, vedi sotto).

**Nuovo in Atomo123, assente in Sum-It**: import/export XLSX e ODS
(formati che non esistevano nell'epoca di Sum-It), tabelle pivot
(Sum-It non le ha mai avute), formattazione numerica locale-aware
(Locale Kit), funzioni SUMIF/COUNTIF/AVERAGEIF.

- [x] **Selezione multi-cella**: base da cui dipendono Ordina, Riempi
      e la formattazione a intervallo (scelta di priorità dell'utente
      esplicita, tra le opzioni proposte). `SheetView` teneva una sola
      cella selezionata (`cell fSelection`); ora tiene anche un'ancora
      (`cell fAnchor`, dove è iniziata la selezione) e espone
      `SelectionRange()` (il rettangolo `range` tra i due, con
      `left/right` e `top/bottom` sempre ordinati indipendentemente da
      quale angolo sia stato trascinato per ultimo). Estesa in tre
      modi, tutti convergenti sullo stesso `ExtendSelection()`:
      - Maiusc+frecce (`SheetView::HandleKey`): muove solo la cella
        attiva, l'ancora resta ferma; una freccia senza Maiusc dopo
        un'estensione ricollassa a una sola cella, come Excel.
      - Trascinamento del mouse: `MouseDown` senza Maiusc arma il
        tracking (`SetMouseEventMask(B_POINTER_EVENTS, ...)`,
        necessario perché il Interface Kit continui a richiamare
        `MouseMoved` anche se il puntatore esce dai confini della
        vista), `MouseMoved` estende finché il bottone resta premuto
        (stato `fDragging`), `MouseUp` lo interrompe.
      - Maiusc+click: estende dall'ancora dell'ultimo click semplice
        (stesso `ExtendSelection`, letto dal `modifiers` del messaggio
        `B_MOUSE_DOWN` corrente in `MouseDown`).
      Backspace/Canc e il comando "Cancella" del menu Modifica ora
      svuotano **tutte** le celle nell'intervallo selezionato (prima
      solo la cella attiva, dato che un intervallo non poteva nemmeno
      esistere) — nuovo metodo `ClearSelection()`, condiviso da
      entrambi i punti d'ingresso, che itera con `CCellIterator`
      invece di toccare `fDoc` cella per cella a mano. L'indirizzo
      mostrato nella barra formula diventa "A1:B5" (angolo alto-sinistra
      : angolo basso-destra, sempre in quest'ordine) quando la
      selezione copre più di una cella, "A1" altrimenti.

      **Bug scoperto e aggirato mentre si implementava Ctrl+A**: su
      Haiku `B_HOME` vale `0x01` — lo stesso byte che Ctrl+A genera
      (`InterfaceDefs.h`: `B_HOME = 0x01, // Ctrl + A`). I due tasti
      sono quindi indistinguibili leggendo solo `bytes[0]`/`raw_char`
      del messaggio `B_KEY_DOWN` in `SheetView::KeyDown`, il
      meccanismo già usato per tutte le altre scorciatoie di questa
      classe: un tasto Ctrl+A finirebbe per attivare Ctrl+Inizio (o
      viceversa), qualunque dei due si scelga di implementare per
      primo. Non risolvibile a livello di `SheetView` senza introdurre
      una lettura diversa del messaggio (es. lo scan code fisico, che
      dipende dal layout di tastiera). **Scelta**: "Seleziona tutto"
      resta disponibile (`SheetView::SelectAll()`, pubblico) ma esposto
      solo dal menu Modifica, senza scorciatoia da tastiera — coerente
      con quanto faceva lo stesso Sum-It storico, che nel menu Edit
      aveva "Select All" con tasto **'A' senza modificatore** (voce di
      menu apposita, non Ctrl+A: evidentemente un problema già noto a
      chi scriveva quel codice).

      Test dedicato `ui/tests/test_selection.cpp` (nuovo target
      `make test-selection`): selezione singola come range di una
      cella, estensione da tastiera, ricollasso dopo una freccia senza
      Maiusc, trascinamento del mouse, nessuna estensione dopo
      `MouseUp`, `SelectAll()`, cancellazione dell'intero intervallo.
      Nessuna regressione nella suite esistente (`test`,
      `test-editing`, `test-navigation`, `test-scroll`, `test-chart`,
      `test-pivot`).

      **Non ancora fatto in questo incremento** (prossimi passi
      naturali, non ancora iniziati): Taglia/Copia/Incolla operano
      ancora sulla sola cella attiva, non sull'intero intervallo
      selezionato; formattazione (Formato menu) idem.

- [x] **Riempi in basso/a destra**: copia la prima riga/colonna di
      `SelectionRange()` nel resto dell'intervallo, spostando i
      riferimenti relativi nelle formule come farebbe la maniglia di
      riempimento di Excel. Riusa `CContainer::CopyCell` (già nel
      motore, ereditato da Sum-It storico) invece di reinventare lo
      spostamento dei riferimenti — un riferimento relativo è sempre
      interpretato rispetto alla posizione della cella che lo
      contiene, quindi copiare lo stesso testo di formula in una
      nuova cella lo fa già puntare al riferimento "spostato"
      corrispondente da solo, **senza** passare `isDragMove=true`
      (quel parametro serve per lo spostamento di una cella, che deve
      *mantenere* i riferimenti puntati alle stesse celle di prima
      compensando lo spostamento — l'esatto opposto di riempire).
      Bug scoperto per tentativi durante l'implementazione: il primo
      tentativo passava `isDragMove=true`, sembrava plausibile "perché
      Sum-It lo chiama così per il trascinamento della maniglia di
      riempimento" ma produceva riferimenti sbagliati (un test scritto
      apposta, `test-fill`, lo ha subito smascherato) — verificato con
      un mini programma a sé stante il comportamento esatto della
      funzione del motore prima di correggere `SheetView`, invece di
      teorizzare a vuoto sulla semantica di `isDragMove`.

      Esposto dal nuovo menu Dati ("Riempi in basso"/"Riempi a
      destra", Ctrl+D/Ctrl+R) come scorciatoie di voce di menu, non
      come casi in `SheetView::HandleKey`: su Haiku `B_END` vale lo
      stesso byte (`0x04`) generato da Ctrl+D — stesso problema già
      visto con Ctrl+A/`B_HOME` per "Seleziona tutto" sopra, stessa
      soluzione (scorciatoia di menu, risolta a un livello diverso
      prima che `KeyDown` veda un byte ambiguo).

      Test dedicato `ui/tests/test_fill.cpp` (nuovo target
      `make test-fill`): copia di un valore semplice, spostamento dei
      riferimenti relativi in entrambe le direzioni (verificato sia il
      testo della formula sia il valore ricalcolato), nessuna azione
      su una selezione di una sola cella. Nessuna regressione nella
      suite esistente.

- [x] **Ordina crescente/decrescente**: ordina `SelectionRange()` per
      righe intere, confrontando solo la colonna più a sinistra
      dell'intervallo come chiave — numericamente se entrambi i valori
      a confronto sono numeri, altrimenti come testo
      (case-insensitive). Ordinamento stabile (`std::stable_sort`): a
      parità di chiave le righe mantengono l'ordine relativo
      originale invece di uno arbitrario. A differenza di Riempi, ogni
      cella si sposta come **testo grezzo** della formula (via
      `GetCellFormula`/riscrittura con `TryToParseString`), senza
      alcun aggiustamento dei riferimenti — un limite noto e
      accettato: ordinare righe con formule che si riferiscono a
      *altre righe della stessa selezione* può produrre riferimenti
      "sbagliati" nello stesso modo in cui lo farebbe Excel/Sum-It in
      quel caso (un riferimento relativo punta sempre alla posizione,
      non alla riga logica che si è spostata).

      Esposto dal nuovo menu Dati ("Ordina crescente"/"Ordina
      decrescente"), senza scorciatoia da tastiera — a differenza di
      Riempi non c'è un tasto Ctrl storico da assegnare, quindi niente
      collisioni `B_HOME`/`B_END` da aggirare stavolta.

      **Bug scoperto mentre si scriveva `test_sort.cpp` (non
      nell'ordinamento stesso)**: il test sembrava bloccarsi a metà
      sotto `timeout`, con un pattern già visto altrove in questo
      progetto ("il processo si blocca invece di andare in crash" —
      vedi la nota su `debug_server` in Fase 5). Il vero problema è
      emerso solo controllando i file `.report` che `debug_server`
      scrive sul Desktop quando intercetta un crash: `Value::Clear()`
      tentava un `free()` doppio, corrompendo l'heap. Causa radice in
      `engine/src/Cell/Value.cpp`, non nel test né in `SortSelection`:
      `Value::Value(CellData&)` faceva `*this = cd;` senza mai
      inizializzare `fType`/`fText`/`fTextIsCopy` — se la memoria dello
      stack lasciata da chiamate precedenti conteneva per caso un
      pattern che sembrava `eTextData` con un puntatore non nullo e
      `fTextIsCopy = true`, `Clear()` (richiamato a cascata da
      `operator=(const char*)`) tentava di liberare un puntatore a
      caso. Bug pre-esistente e generico — colpisce *ogni* chiamata a
      `CContainer::GetCellFormula` su una cella di testo, non solo
      durante l'ordinamento — che spiega perché il test passava quasi
      sempre (3 scenari su 4) e falliva solo in modo intermittente.
      Risolto inizializzando `fType = eNoData` prima della delega a
      `operator=`.

      Test dedicato `ui/tests/test_sort.cpp` (nuovo target
      `make test-sort`): ordinamento crescente e decrescente con una
      colonna "passeggero" che deve seguire la riga, ordinamento
      stabile su chiavi duplicate, nessuna azione su una selezione di
      una sola riga. Nessuna regressione nella suite esistente
      (verificato anche con esecuzioni ripetute, dato che il bug del
      motore era dipendente dallo stato dello stack).

- [x] **Annulla/Ripeti**: assente del tutto in Atomo123 prima di
      questo incremento (il motore ereditato da Sum-It non ha mai
      avuto un concetto di comando/transazione reversibile — solo
      lettura/scrittura diretta delle celle), quindi niente da
      recuperare a livello di motore: implementato interamente nella
      UI, in `SheetView`. Una sola pila di "istantanee" copre tutte
      le operazioni che mutano il documento, invece di un comando
      dedicato per ognuna (Cancella, Riempi, Ordina, Taglia/Incolla,
      Trova e sostituisci, editing in-cella): ogni istantanea è un
      `range` più il testo grezzo di ogni cella al suo interno (lo
      stesso formato usato da Ordina — `GetCellFormula` in lettura,
      `TryToParseString` in scrittura), catturato **prima** della
      mutazione da `SaveUndoState()`. `Undo()` cattura lo stato
      attuale (per poter ripetere), applica l'istantanea salvata,
      seleziona l'intervallo coinvolto. `Redo()` è simmetrico sulla
      pila opposta. Una nuova modifica dopo un annulla svuota la pila
      del ripeti (come in Excel/LibreOffice Calc: non avrebbe senso
      "ripetere" qualcosa che la nuova modifica ha già reso
      incoerente col documento).

      Scelta deliberata (istantanea per intervallo invece di un
      comando/parametri per tipo di operazione): un solo meccanismo
      generico, già usato e verificato da Ordina, riduce la
      superficie di bug rispetto a N implementazioni diverse — un
      costo accettato è che un'istantanea può coprire più celle di
      quelle davvero toccate (es. "Trova e sostituisci tutto" cattura
      il rettangolo che racchiude tutte le celle trovate, non solo
      quelle sparse che contenevano il testo cercato); le celle non
      toccate vengono comunque riscritte con lo stesso testo che
      avevano già, quindi il risultato resta corretto, solo non
      minimale.

      I metodi di `SheetView` che mutano il documento (`ClearSelection`,
      `FillDown`/`FillRight`, `SortSelection`, l'editing in-cella in
      `CommitEditing`) chiamano `SaveUndoState()` da soli. Le mutazioni
      che vivono in `MainWindow` (`CopySelection` con `cut=true`,
      `PasteSelection`, `ReplaceCurrent`, `ReplaceAll`) non hanno
      accesso alle pile private, quindi chiamano lo stesso
      `SaveUndoState()` dall'esterno, pubblico apposta per questo,
      prima di toccare `fDoc` — nessuna duplicazione della logica di
      cattura/ripristino fuori da `SheetView`.

      `SetDocument()` (apertura di un nuovo file, "Nuovo") svuota
      entrambe le pile: le istantanee catturate si riferiscono al
      `CContainer` precedente, applicarle al documento appena aperto
      scambierebbe il contenuto di celle senza nessuna relazione.

      Esposto dal menu Modifica ("Annulla"/"Ripeti", Ctrl+Z/Ctrl+Y) —
      a differenza di Seleziona tutto/Riempi in basso/Riempi a destra,
      qui la scorciatoia di menu **non** serve ad aggirare
      un'ambiguità: su Haiku Ctrl+Z genera `B_SUBSTITUTE` (`0x1a`) e
      Ctrl+Y non ha nemmeno un nome dedicato in `InterfaceDefs.h` —
      nessuno dei due byte corrisponde a un altro tasto già gestito
      da `SheetView::HandleKey`, diversamente dal caso B_HOME/Ctrl+A
      e B_END/Ctrl+D documentato sopra.

      Test dedicato `ui/tests/test_undo.cpp` (nuovo target
      `make test-undo`, 18 verifiche): singola modifica, Cancella,
      Riempi in basso, Ordina, `CanUndo()`/`CanRedo()` a ogni passo,
      invalidazione del ripeti dopo una nuova modifica successiva a
      un annulla, nessun crash annullando/ripetendo oltre i limiti
      della pila, reset di entrambe le pile alla apertura di un nuovo
      documento. Nessuna regressione nella suite esistente (verificato
      anche con esecuzioni ripetute).

- [x] **Taglia/Copia/Incolla e Formato estesi all'intervallo**: le tre
      operazioni (già presenti, elencate fra gli "Già allineato" con
      Sum-It in cima a questa fase) operavano finora solo sulla cella
      attiva — un residuo dall'epoca prima della selezione
      multi-cella, mai aggiornato quando quest'ultima è arrivata.
      `MainWindow::CopySelection`/`PasteSelection`/`SetCellFormat`
      leggono ora `SheetView::SelectionRange()` invece di
      `SheetView::Selection()`.

      Copia/Taglia scrivono l'intero intervallo sugli appunti di
      sistema in formato TSV (colonne separate da tabulazione, righe
      da ritorno a capo): una sola cella resta testo semplice come
      prima (compatibile con qualunque applicazione Haiku), un
      intervallo più grande usa lo stesso formato capito da
      Excel/LibreOffice Calc — copiare/incollare fra Atomo123 e loro
      tramite gli appunti di sistema funziona già da solo, senza
      bisogno di un formato proprietario né di codice dedicato
      all'interoperabilità. Incolla legge lo stesso formato: un
      blocco multi-cella si ancora all'angolo in alto a sinistra della
      selezione corrente ed espande la selezione alla dimensione del
      blocco incollato (non a quella della selezione corrente, che può
      restare una sola cella); un valore singolo incollato su una
      selezione più grande di una cella riempie invece tutto
      l'intervallo, non solo l'angolo — stesso comportamento di
      Excel/LibreOffice Calc in entrambi i casi. Il formato numerico
      (menu Formato) si applica a ogni cella dell'intervallo tramite
      `GetCellStyle`/`SetCellStyle` per cella, che già gestiscono da
      soli le celle senza contenuto (creano una voce solo se lo stile
      differisce da quello di colonna/predefinito, non "sporcano" con
      voci vuote celle che restano senza dati).

      Tutte e tre restano annullabili tramite `SaveUndoState()`, già
      introdotto per Annulla/Ripeti sopra — nessuna logica nuova, solo
      la stessa chiamata prima di mutare il documento.

      `CopySelection`/`PasteSelection`/`SetCellFormat` diventano
      pubblici in `MainWindow` (comunque irraggiungibili se non
      tramite i messaggi di menu nel normale funzionamento
      dell'app), insieme a un nuovo `GetSheetView()`, apposta per
      essere testabili direttamente — stesso principio già usato in
      `SheetView` per Ordina/Riempi/Seleziona tutto. A differenza
      degli altri test di questa fase serve una vera `MainWindow`, non
      la sola `SheetView`, perché le tre operazioni vivono lì.

      Test dedicato `ui/tests/test_paste_range.cpp` (nuovo target
      `make test-paste-range`): copia di un intervallo 2x2 in formato
      TSV, incolla che ricrea la griglia altrove ed estende la
      selezione al blocco incollato, incolla di un valore singolo che
      riempie un intervallo più grande, taglia su un intervallo (con
      annulla), formato esteso a tutte le celle di un intervallo.
      Nessuna regressione nella suite esistente (verificato anche con
      esecuzioni ripetute).

- [x] **Inserisci/Elimina riga e colonna**: ultimo punto rimasto della
      lista dei "mancanti" in cima a questa fase. A differenza di
      tutto il resto fatto finora, il motore ereditato da Sum-It
      storico aveva già tutto il necessario, mai portato in superficie
      dalla UI nuova: `CContainer::MoveCell(dest, src, destLoc, split,
      first, count)` (con `SplitType` `hSplit`/`vSplit`) e
      `GetNextCellInRow`/`GetPreviousCellInRow` — nessuna riga di
      motore nuova, solo la stessa logica di
      `legacy/opensumit/sum-it/Source/main/Commands/InsertCommands.cpp`
      riportata in `SheetView::InsertRows`/`InsertColumns`/
      `DeleteRows`/`DeleteColumns`, senza il livello `CCellView`/
      `NameTable`/larghezze-colonna-variabili che Atomo123 non ha (usa
      `kColWidth`/`kRowHeight` fissi, non colonne ridimensionabili).

      Il punto delicato non è spostare le celle che stanno *dentro* la
      zona interessata, ma **anche** quelle che restano ferme: una
      formula sopra il punto di inserimento può comunque riferirsi a
      una cella sotto che si sposta, quindi il suo *testo* cambia pur
      restando lei stessa ferma. `MoveCell` con uno `split` diverso da
      `noSplit` aggiorna i riferimenti della formula anche quando
      `destLoc == srcLoc`, quindi la scansione tocca ogni cella
      dell'intero documento, non solo quelle nella zona spostata.
      L'ordine di scansione conta ed è lo stesso di Sum-It storico:
      dal basso verso l'alto per inserire righe (la destinazione ha
      sempre riga maggiore o uguale alla sorgente, quindi si evita di
      sovrascrivere celle non ancora spostate), dall'alto verso il
      basso per eliminarle (speculare); da destra a sinistra dentro
      ogni riga per inserire colonne, da sinistra a destra per
      eliminarle. Le celle strettamente dentro la zona eliminata
      spariscono (nessuna destinazione valida per loro). Rifiuta
      l'inserimento, senza modificare nulla, se spingerebbe fuori dal
      limite fisso del foglio (`kColCount`/`kRowCount`) celle che
      contengono già dati — stesso controllo `errCellsWouldFallOf` di
      Sum-It storico, con un `BAlert` al posto della sua eccezione.

      Quattro voci di menu esplicite ("Inserisci riga"/"Inserisci
      colonna"/"Elimina riga"/"Elimina colonna" nel menu Dati) invece
      dell'unico comando "Inserisci"/"Elimina" di Sum-It storico, che
      inferiva riga o colonna dal fatto che la selezione coprisse
      un'intera riga/colonna (click sull'intestazione): Atomo123 non
      ha ancora quel gesto, quindi l'inferenza sarebbe ambigua qui —
      il numero di righe/colonne e il punto vengono semplicemente da
      `SelectionRange()` (selezionare 2 righe qualunque inserisce 2
      righe vuote a partire da lì).

      **Limite noto**: non sposta né ridimensiona i grafici
      incorporati (`ChartObject` vive in `MainWindow`, non in
      `SheetView`) — un grafico la cui area dati era nelle righe/
      colonne spostate resta dov'era, con l'intervallo dati vecchio,
      finché non lo si ricrea a mano.

      **Refactor collaterale nell'istantanea di Annulla/Ripeti**:
      `SaveUndoState`/`CaptureSnapshot`/`ApplySnapshot` passano da un
      blocco denso (una voce anche per ogni cella vuota
      dell'intervallo) a una lista delle sole celle che esistono
      davvero, catturate con `CCellIterator` — necessario perché
      Inserisci/Elimina riga o colonna deve poter catturare l'INTERO
      documento (qualunque formula altrove potrebbe riferirsi a una
      cella che si sposta), e un blocco denso su tutto il foglio
      virtuale (`kColCount` x `kRowCount`, fino a undici milioni di
      posizioni) sarebbe troppo lento anche per un foglio quasi vuoto.
      Il comportamento esterno (`SaveUndoState`/`Undo`/`Redo`) resta
      identico — verificato dalla suite già esistente per Annulla/
      Ripeti e Taglia/Copia/Incolla, che non ha richiesto modifiche.

      Test dedicato `ui/tests/test_insert_delete.cpp` (nuovo target
      `make test-insert-delete`, 21 verifiche): inserimento/
      cancellazione di una o più righe/colonne, celle sopra/a sinistra
      del punto che restano ferme, il caso delicato del riferimento di
      una cella ferma che segue una cella spostata altrove nel
      documento, annulla per entrambe le operazioni, rifiuto
      dell'inserimento quando spingerebbe dati fuori dal foglio.
      Nessuna regressione nella suite esistente (compresi Annulla/
      Ripeti e Taglia/Copia/Incolla, per via del refactor
      dell'istantanea; verificato anche con esecuzioni ripetute).

- [x] **Intervalli con nome**: il motore ereditato da
      Sum-It aveva già tutto l'occorrente (`CNameTable`,
      `CContainer::ResolveName()`, il token `valName`), ma era
      **irraggiungibile** dalla UI reale per lo stesso identico motivo
      già scoperto e risolto in Fase 9 per i riferimenti fra fogli: il
      riconoscimento di un nome in fase di parsing passava da
      `CCellView::IsNamedRange()` (`Container.h`, letta tramite
      `GetOwner()`/`fInView`), e `CCellView` nella UI moderna è uno
      stub sempre-NULL (`EngineViewStub.h`, retaggio dell'interfaccia
      grafica BeOS mai riscritta) — un nome definito non veniva quindi
      **mai** riconosciuto in una formula, a prescindere da quanto
      fosse popolata `CNameTable`.

      Nuovo `CContainer::GetOrCreateNameTable()` (crea la tabella al
      volo, la userà anche la futura finestra di definizione). Il
      parser (`Factor()`, caso `IDENT`) non controlla più se il nome
      esiste già prima di accettarlo: un identificatore che non
      corrisponde a nessuna funzione nota diventa sempre un token
      `valName` vivo — stessa filosofia già scelta per
      `ParseSheetReference` in Fase 9, **mai rifiutare a tempo di
      parsing**, perché un nome può benissimo essere definito *dopo*
      che la formula che lo usa è già stata scritta (o al contrario,
      ridefinito più tardi: la formula deve seguire, non congelarsi).
      La risoluzione avviene sempre e solo a tempo di calcolo, in
      `CFormula::Calculate` (`case valName`), tramite
      `CContainer::ResolveName()`.

      **Bug scoperto e corretto**: `CFormula::IsConstant()` non
      considerava `valName` fra i token "non costanti" (a differenza
      di `valCell`/`valRange`, già gestiti). Una formula come
      "=Totale*2" veniva quindi giudicata costante da
      `TryToParseString`, calcolata **una sola volta** in fase di
      parsing e congelata come valore statico (`NewCell(loc, v,
      NULL)`, puntatore a formula nullo — non più viva): ridefinire
      "Totale" più tardi non aveva alcun effetto sulle celle che lo
      usavano già. Bug preesistente nel codice storico ma sempre
      rimasto invisibile finché il riconoscimento del nome restava
      comunque bloccato più a monte da `IsNamedRange()`.

      **Bug scoperto e corretto, non collegato al nome ma solo
      smascherato scrivendone i test**: il generatore dei valori
      sentinella NaN interni del motore (`Nan()`, usato per
      `gNameNan`/`gRefNan`/tutti gli altri "non calcolabile") si
      basava su una macro `__LITTLE_ENDIAN` che questo GCC/Haiku non
      definisce mai — il ramo sbagliato di `__HI`/`__LO` scriveva il
      pattern di bit nella metà sbagliata del `double`, producendo un
      valore subnormale vicino a zero anziché un vero NaN;
      `Value::IsNan()` (che chiama `std::isnan`) risultava quindi
      sempre falso su qualunque sentinella del motore, non solo su
      `gNameNan`. Corretto in `Utils.h`/`MyMath.h` con un controllo
      portabile su `__BYTE_ORDER__` (commit a parte, non è un problema
      specifico degli intervalli con nome).

      Test dedicato `engine/tests/named_ranges_test.cpp` (nuovo target
      `make test-names`, motore): definizione di un nome su una
      cella, calcolo di una formula che lo usa, ricostruzione testuale
      (`UnMangle` mostra "Totale", non "A1"), propagazione dopo
      ridefinizione, risoluzione di un vero intervallo multi-cella con
      `ResolveName()`, e un nome non ancora definito che calcola
      `gNameNan` (NaN vero, formula viva) invece di restare un
      identificatore testuale morto. Nessuna regressione nella suite
      esistente del motore (`test`, `test-functions`, `test-xsheet`) né
      in quella della UI (tutti i 20 target).

      **Lato UI**: `NameWindow` (nuova voce "Intervalli con nome..."
      nel menu Inserisci) segue lo schema di `FindWindow`/
      `PivotWindow` — non tocca mai `CNameTable` direttamente, solo
      `BMessage` verso `MainWindow` (`kMsgDefineName`/
      `kMsgDeleteName`/`kMsgGoToName`), che possiede `fDoc` e lo può
      leggere/scrivere sul proprio thread. Una `BListView` elenca i
      nomi già definiti (ricostruita da
      `MainWindow::RefreshNameWindow()` a ogni apertura e dopo ogni
      Aggiungi/Aggiorna/Elimina, mai tenuta come copia che potrebbe
      disallinearsi da `CNameTable`); due campi di testo (Nome,
      Intervallo — sintassi "A1" o "A1:B5", lo stesso
      `RangeRef::ParseRangeRef()` già condiviso da grafico e tabella
      pivot); "Vai a" sposta/estende la selezione della `SheetView`
      attiva sull'intervallo risolto (`SetSelection`/
      `ExtendSelection`).

      **Bug scoperto e corretto scrivendo `ui/tests/test_names.cpp`,
      non specifico della UI**: `CContainer::ResolveName()` usava
      `(*fNames)[name]` (`operator[]` di `std::map`), che inserisce
      silenziosamente una voce vuota per una chiave assente invece di
      segnalare l'errore — "Vai a" su un nome appena eliminato con
      "Elimina" si risolveva quindi su `range(0,0,0,0)` (una cella non
      valida) invece di segnalare che il nome non esiste più, e ogni
      lettura ripetuta di un nome inesistente inquinava silenziosamente
      la tabella con voci fantasma. Corretto con `find()` (commit a
      parte).

      Metodi `HandleDefineName`/`HandleDeleteName`/`HandleGoToName`
      pubblici apposta (stesso principio già scelto per
      `CopySelection`/`PasteSelection` in Fase 7): `ui/tests/
      test_names.cpp` (nuovo target `make test-names`, UI) li chiama
      direttamente su una vera `MainWindow`, senza dover gestire un
      giro di dispatch dei messaggi in un test headless — definizione,
      ricalcolo immediato dopo una ridefinizione, "Vai a" su una
      cella singola e su un intervallo multi-cella, eliminazione.
      Nessuna regressione nella suite esistente del motore né in
      quella della UI (tutti i 21 target).

- [x] **Incolla speciale**: `PasteSpecialWindow` (stesso schema di
      `FindWindow`/`PivotWindow`) sul modello del vecchio
      `PasteSpecialDialog` di Sum-It storico
      (`legacy/opensumit/sum-it/Source/main/Dialog/
      PasteSpecialDialog.cpp`), con un perimetro volutamente ridotto:
      niente "Paste Format" (Atomo123 non copia ancora lo stile di
      una cella negli appunti, solo il contenuto — la formattazione
      font/colore/allineamento resta un punto a sé, ancora da fare),
      niente "References"/link (opzione di nicchia del dialogo
      storico). Tre scelte, non le sei della versione storica:

      - **Cosa incollare** — Tutto (stesso testo di un Incolla
        normale, può contenere formule) o Solo valori.
      - **Operazione con la cella di destinazione** — Sovrascrivi
        (come Incolla) oppure Somma/Sottrai/Moltiplica/Dividi.
      - **Trasponi** — scambia righe e colonne del blocco incollato.

      "Solo valori" richiede di sapere il *risultato calcolato* di
      ogni cella copiata, non la sua formula — ma `CopySelection`
      scriveva sugli appunti solo `GetCellFormula` (il testo della
      formula, es. "=A1+B1"). Aggiunto un secondo campo dati sullo
      stesso `BMessage` degli appunti, `"text/x-atomo-values"`,
      costruito con `GetCellResult` (il risultato già calcolato, es.
      "30") invece di `GetCellFormula` — un secondo campo, non un
      secondo giro di `Lock`/`Clear`/`Commit`. `Incolla speciale >
      Solo valori` legge quel campo quando c'è; se manca (appunti
      copiati da un'altra applicazione, o prima che questo campo
      esistesse) ripiega su `"text/plain"`, nessun crash, solo lo
      stesso comportamento di un Incolla normale.

      Le quattro operazioni aritmetiche operano **sempre** su valori,
      mai su formule — come Excel: il risultato è sempre una cella
      statica, non conta se la sorgente copiata era una formula.
      Scritte con `CContainer::NewCell(dest, Value(result), NULL)`
      diretto, non tramite `TryToParseString` su un numero
      convertito in testo — quest'ultima strada avrebbe dovuto
      generare il testo del numero con `snprintf`, che usa sempre il
      punto come separatore decimale (locale "C"), mentre
      `TryToParseString` lo interpreta secondo `gDecimalPoint`
      (impostabile, tipicamente la virgola in italiano): un'ambiguità
      inutile su un numero che il codice stesso genera, evitata del
      tutto scrivendo il `Value` nel documento senza mai passare da
      una rappresentazione testuale.

      La logica di lettura di un blocco TSV dagli appunti (righe
      separate da ritorno a capo, colonne da tabulazione — già
      esistente in `PasteSelection`) è stata estratta in una funzione
      condivisa, `ParseTSVGrid`, usata sia da `PasteSelection` sia da
      `HandlePasteSpecialRequest` — la differenza fra le due sta solo
      in quale campo degli appunti si legge e come la griglia risultante
      viene poi scritta nel documento (l'una sempre come formula/
      testo con `TryToParseString`, l'altra secondo l'operazione
      scelta).

      `HandlePasteSpecialRequest` pubblico apposta (stesso principio
      di `CopySelection`/`PasteSelection`): `ui/tests/
      test_paste_special.cpp` (nuovo target `make test-paste-special`,
      9 verifiche) lo chiama direttamente su una vera `MainWindow` —
      Solo valori (il testo incollato non è più una formula viva),
      le quattro operazioni in sequenza sullo stesso valore di
      destinazione, l'operazione aritmetica è annullabile, Trasponi
      scambia una riga copiata in una colonna incollata. Nessuna
      regressione nella suite esistente (tutti i 22 target).

- [x] **Vai a**: `GoToWindow`, sul modello minimale di `FindWindow`
      (un campo di testo, un pulsante, nessuna lista) — nuova voce
      "Vai a..." nel menu Modifica (Ctrl+G, come Excel). Accetta sia
      una cella singola ("C15") sia un intervallo ("A1:B5"), la stessa
      sintassi già condivisa da grafico e tabella pivot
      (`RangeRef::ParseRangeRef`), invece di introdurre un terzo
      parser di riferimenti. Un testo non valido non fa nulla (nessuno
      spostamento, nessun crash) — stesso principio già scelto per
      `HandleGoToName` (l'equivalente legato a un nome definito,
      Fase 7 sopra): entrambi condividono lo stesso meccanismo di
      spostamento (`SheetView::SetSelection`/`ExtendSelection`, che
      scorre già da sola per mostrare la cella selezionata —
      `ScrollToShowSelection()`, nessun codice nuovo da scrivere per
      quella parte).

      `HandleGoToRequest` pubblico apposta (stesso principio di
      `CopySelection`/`PasteSelection`): `ui/tests/test_goto.cpp`
      (nuovo target `make test-goto`, 3 verifiche) lo chiama
      direttamente su una vera `MainWindow` — cella singola,
      intervallo, testo non valido. Nessuna regressione nella suite
      esistente (tutti i 23 target).

- [x] **Un vero Blocca riquadri**: prima di questo incremento non
      esisteva affatto (il "solo effetto grafico di rendering" citato
      nell'analisi dei punti mancanti in cima a questa fase si
      riferiva alle intestazioni di riga/colonna, sempre "incollate"
      allo schermo durante lo scroll — non un vero blocco per il
      contenuto delle celle, mai implementato). `SheetView::
      ToggleFreezePanes()` congela tutto cio' che sta sopra/a sinistra
      della cella attiva, come il comando "Blocca riquadri" di Excel:
      un secondo `Toggle` sblocca tutto. Nuova voce di menu "Blocca
      riquadri" (menu Dati), con segno di spunta sincronizzato a ogni
      attivazione/cambio foglio/documento.

      Le righe/colonne bloccate restano ferme sullo schermo durante lo
      scroll — sfondo, righe della griglia, testo, non solo le lettere/
      numeri delle intestazioni (che gia' usavano questa tecnica, vedi
      il commento su `ScrollTo()`): `Draw()` disegna fino a 4 bande
      invece di una sola (riquadro scorrevole normale; riga bloccata,
      colonne scorrevoli; colonna bloccata, righe scorrevoli;
      l'angolo, bloccato su entrambi gli assi), tramite un nuovo
      `DrawCellBand()` condiviso che accetta un'origine di disegno
      (`(0,0)` per il riquadro scorrevole, `Bounds().left`/`.top` per
      una banda bloccata — la stessa tecnica delle intestazioni,
      estesa al contenuto). Le etichette delle intestazioni di riga/
      colonna che ricadono nella banda bloccata vengono anch'esse
      "agganciate" sull'asse che gli mancava (le intestazioni di riga
      erano gia' fisse in orizzontale ma no in verticale, e viceversa
      per quelle di colonna), altrimenti la banda bloccata perderebbe
      la propria intestazione scorrendo.

      `CellAt()` "riporta indietro" un clic sulla banda bloccata
      sottraendo lo scroll corrente (`Bounds().left`/`.top`) prima di
      risolverlo a una cella — l'esatto inverso di come `Draw()` la
      disegna: senza questo, un clic sulla banda bloccata dopo aver
      scorso il foglio avrebbe selezionato la cella che si troverebbe
      li' SENZA il blocco, non quella davvero disegnata sotto il dito.
      `ScrollToShowSelection()` non scorre mai per una cella gia'
      bloccata (sempre visibile per definizione — altrimenti
      selezionarla riporterebbe sempre la vista in cima/a sinistra,
      vanificando il blocco) e considera lo spazio occupato dalla
      banda bloccata come le intestazioni, cosi' una cella appena
      rivelata dallo scroll non resta parzialmente coperta.

      **Due bug preesistenti scoperti implementando questa
      funzionalita'**, nello stesso codice di gestione del mouse ma
      non specifici di Blocca riquadri: `MouseDown`/`MouseMoved`
      confrontavano un clic sull'intestazione (per scartarlo, o per
      riconoscere una maniglia di ridimensionamento) con `0`/
      `kHeaderWidth` fissi invece di `Bounds().top`/`.left` +
      `kHeaderHeight`/`kHeaderWidth`, nonostante le intestazioni stesse
      seguissero gia' lo scroll (`Bounds()`) da quando erano state
      "congelate" in una fase precedente. Un clic sull'intestazione
      dopo aver scorso il foglio (senza colpire una vera maniglia di
      ridimensionamento) selezionava percio' una cella quasi a caso
      invece di non fare nulla — mai notato prima perche' nessun altro
      codice aveva motivo di toccare di nuovo quella stessa logica.

      **Limite noto, deliberato**: solo per la sessione corrente, non
      salvato nel formato nativo — stessa scelta gia' presa per
      l'altezza di riga (vedi il commento su `fRowHeights` in
      `SheetView.h`). Estendere `AscdIO`/`AscdSheet` per farlo
      sarebbe stato un cambiamento di formato dati piu' ampio (tocca
      anche i translator che leggono/scrivono lo stesso formato),
      fuori perimetro per questo incremento.

      Test dedicato `ui/tests/test_freeze.cpp` (nuovo target
      `make test-freeze`, 12 verifiche, stesso schema di finestra di
      prova di `test-scroll`): attivazione/disattivazione dalla
      selezione corrente, `SetFreezePanes` con valori fuori limite
      (negativi, enormi) che non escono mai da un intervallo valido,
      nessuno scroll selezionando una cella bloccata anche a foglio
      gia' scorso lontano, `CellAt()` sulla banda bloccata (angolo e
      banda di sole colonne) dopo lo scroll, il clic sull'intestazione
      che non cambia piu' la selezione. Nessuna regressione nella
      suite esistente (tutti i 25 target).

- [x] **Formattazione font/colore/allineamento**: `CellStyle` (motore)
      aveva gia' tutti i campi necessari (`fFont`, `fAlignment`,
      `fLowColor`/`fHighColor`) fin dall'inizio — solo mai esposti da
      nessun controllo UI oltre al percorso di importazione XLSX.
      Nuove voci nel menu Formato: Grassetto/Corsivo (Ctrl+B/Ctrl+I),
      Allinea a sinistra/al centro/a destra, Colore testo/Colore
      sfondo (apre `ColorWindow`, un `BColorControl` piu' un pulsante
      Applica, riusata per entrambi — `SetMode` cambia titolo e colore
      iniziale). Tutti si applicano a tutto `SelectionRange()`, non
      solo alla cella attiva — stesso principio gia' scelto per il
      formato numerico (`SetCellFormat`).

      Grassetto/Corsivo sono gli unici due che richiedono uno stato
      "di partenza" per sapere se attivare o disattivare: si legge
      dalla sola cella attiva (`fSheetView->Selection()`, come il
      pulsante "risulta premuto" o no di Excel) e si applica lo stato
      OPPOSTO a tutta la selezione. Agiscono su `CellStyle::fFont`,
      che non e' un flag diretto ma un indice in `gFontSizeTable`
      (famiglia/stile/dimensione/colore, deduplicati): si legge la
      combinazione corrente, si costruisce la nuova stringa di stile
      ("Bold"/"Italic"/"Bold Italic"/"Regular" — i quattro nomi
      standard usati dalla stragrande maggioranza dei font di
      sistema, limite noto e accettato per una famiglia che non li
      avesse esattamente cosi'), e si registra/riusa l'indice
      risultante con `GetFontID`.

      `SheetView::DrawCellBand` applica ora il font della cella
      (`gFontSizeTable.SetFontID`) prima di disegnarne il testo — non
      lo faceva affatto finora, `CellStyle::fFont` veniva letto e
      scritto ma mai davvero applicato al disegno — e posiziona il
      testo secondo `fAlignment` (`eAlignGeneral`, il valore
      predefinito di ogni cella mai toccata dal menu, resta sempre a
      sinistra: comportamento invariato, cambia solo per le celle che
      l'utente ha esplicitamente allineato). Ripristina il font di
      sistema (`SetFont(be_plain_font)`) alla fine di ogni banda,
      altrimenti l'ultimo font applicato (magari in grassetto)
      resterebbe attivo per le intestazioni di riga/colonna disegnate
      subito dopo, che non devono mai ereditarlo.

      **Bug scoperto scrivendo `ui/tests/test_format.cpp`** (il primo
      tentativo restava bloccato senza stampare nulla, non un crash
      pulito): `CContainer::CContainer` registra un font predefinito
      in `gFontSizeTable` SOLO se costruito con un `CCellView` non
      nullo (`inPane`) — ma la UI moderna lo passa sempre `NULL`
      (stub permanente, vedi `EngineViewStub.h`: la stessa classe di
      bug gia' trovata e corretta per le formule fra fogli e gli
      intervalli con nome in questa stessa fase). `gFontSizeTable`
      resta quindi completamente vuota per un documento nuovo mai
      passato da un'importazione XLSX (che registra font propri a
      parte, in `Excel.pass1.cpp`), e `CFontSizeTable::GetFontInfo`
      — a differenza di `SetFontID`, che ha un controllo esplicito —
      non verifica i limiti dell'indice: leggere lo stile della
      cella attiva era un accesso di memoria non valido. **Non
      corretto in `CContainer::CContainer`**: quel costruttore serve
      anche a costruire documenti headless nei test del motore
      (`CContainer(NULL, NULL)`, dove `NULL` e' legittimo — nessuna
      connessione app_server disponibile, non un bug) — toccarlo
      avrebbe rischiato di rompere quei test. Corretto invece con un
      accessore sicuro in `MainWindow.cpp` (`GetCellFontInfo`), che
      ricade sul font di sistema (`be_plain_font`) quando l'indice
      non e' ancora valido, esattamente l'aspetto che una cella
      "senza font personalizzato" ha gia' oggi.

      **Limite noto, deliberato**: grassetto/corsivo/allineamento
      restano per la sola sessione corrente (non salvati nel formato
      nativo), stessa scelta gia' presa per Blocca riquadri e
      l'altezza di riga — estendere `AscdIO` tocca anche i translator
      che condividono lo stesso formato, fuori perimetro per questo
      incremento. I colori invece si salvano gia' (`fLowColor`/
      `fHighColor`, aggiunti in una fase precedente).

      Test dedicato `ui/tests/test_format.cpp` (nuovo target
      `make test-format`, 8 verifiche): Grassetto/Corsivo applicati a
      un intervallo intero e non solo alla cella attiva, un secondo
      Grassetto che toglie di nuovo lo stato, grassetto E corsivo
      insieme sulla stessa cella, Allinea a destra su un intervallo,
      Colore testo e Colore sfondo indipendenti sulla stessa cella.
      Nessuna regressione nella suite esistente (tutti i 26 target).

- [x] **Una finestra Preferenze**: `PreferencesWindow` (menu File),
      sottoinsieme volutamente ridotto rispetto a Sum-It storico —
      solo le preferenze per cui il motore aveva già un punto di
      estensione pronto, non un pannello con ogni opzione storica:

      - **Mostra griglia** (`BCheckBox`) — per-vista
        (`SheetView::SetShowGrid`/`ShowGrid`), letta all'avvio da
        `gPrefs` se esiste. `DrawCellBand` salta il disegno delle
        linee di griglia quando disattivata.
      - **Separatore decimale** e **separatore di elenco**
        (`BMenuField`, Punto/Virgola e Punto e virgola/Virgola) —
        globali al motore (`gDecimalPoint`/`gListSeparator`), già
        lette da `TryToParseString` quando non si passa un separatore
        esplicito (`CellParser.h`): cambiarle si riflette subito su
        come le formule digitate vengono interpretate, per l'intera
        applicazione — stesso comportamento del Sum-It storico
        (un'unica preferenza globale, non per documento).

      **`gPrefs` (`Preferences.h`, il meccanismo storico di
      lettura/scrittura preferenze su file — coppie chiave=valore in
      `~/config/settings/`) non era mai stato istanziato da nessuna
      parte della UI moderna**: restava sempre `NULL` (dichiarato
      `extern CPreferences *gPrefs = NULL;` in `Preferences.cpp`, mai
      assegnato). Istanziato ora in `App::App()`, con
      `ReadPrefFile()` avvolto in un `try`/`catch` — lancia se il
      file non esiste ancora (prima esecuzione), non è un errore: si
      parte semplicemente dai valori predefiniti di ciascun
      `GetPref*()` (già gestito da `CPreferences` stessa, che scrive
      il default al primo utilizzo se la chiave manca).
      `HandlePreferencesRequest` applica l'effetto in memoria SEMPRE,
      e se `gPrefs` esiste (può essere `NULL` in un test che non
      passa da una vera `App`, come tutti i test UI esistenti) lo
      persiste anche su disco.

      Test dedicato `ui/tests/test_preferences.cpp` (nuovo target
      `make test-preferences`, 7 verifiche): griglia visibile per
      default, `HandlePreferencesRequest` che la nasconde/riattiva,
      i separatori impostati globalmente, e soprattutto che il nuovo
      separatore decimale si applica DAVVERO al parser (`"1,5"` con
      la virgola come decimale si interpreta come il numero 1.5, non
      come testo) — non solo che la variabile globale cambia valore.
      Nessuna regressione nella suite esistente (tutti i 27 target).

      **Con questo si chiude la Fase 7**: tutti e sei i punti mancanti
      individuati nell'analisi in cima a questa fase (formattazione
      font/colore/allineamento, Incolla speciale, intervalli con
      nome, Vai a, un vero Blocca riquadri, una finestra Preferenze)
      sono stati recuperati, oltre a Selezione multi-cella, Riempi,
      Ordina, Inserisci/Elimina riga e colonna e Seleziona tutto
      (nella prima parte di questa stessa fase). Diversi bug
      preesistenti nel codice storico/di transizione sono stati
      scoperti e corretti lungo il percorso — quasi tutti della
      stessa famiglia: qualcosa gated dietro `CCellView`/`inPane`,
      sempre `NULL` nella UI moderna (mai raggiunto), o un confronto
      di coordinate che non teneva conto dello scroll (`Bounds()`).

---

## Fase 8 — Qualità UI/UX (CHIUSA)

Con la Fase 7 sostanzialmente completa (resta solo formattazione
font/colore/allineamento, Incolla speciale, intervalli con nome, Vai
a, un vero Blocca riquadri, una finestra Preferenze), l'utente ha
chiesto un giro dedicato alla qualità dell'interfaccia invece di
nuove funzionalità di calcolo — punti scelti fra quattro proposti
dopo un'analisi rapida del codice esistente: protezione dalle
modifiche non salvate (consigliata, unico rischio concreto di perdita
dati fra le quattro), titolo finestra con nome file, icone sulla
toolbar, ridimensionamento riga/colonna.

- [x] **Protezione dalle modifiche non salvate + titolo con nome
      file**: prima di questo incremento, Nuovo/Apri/Esci scartavano
      il documento corrente senza alcun avviso — un clic sbagliato
      poteva far perdere lavoro non salvato, e la finestra mostrava
      sempre e solo "Atomo123", mai il file aperto. `MainWindow` tiene
      ora `fModified` (bool) e `fDocumentName` (solo il nome, non il
      percorso — basta per il titolo), aggiornati da **ogni**
      mutazione del documento: quelle fatte dentro `SheetView`
      (editing in-cella, Cancella, Riempi, Ordina, Inserisci/Elimina
      riga e colonna, Annulla/Ripeti) tramite un nuovo ponte
      `SheetView::NotifyDocumentChanged()` →
      `MainWindow::DocumentChanged()` (stesso principio già usato per
      `NotifySelectionChanged`/`SelectionChanged`), e quelle fatte
      direttamente in `MainWindow` (Taglia, Incolla, Formato numerico,
      Trova e sostituisci, grafico e tabella pivot incorporati) con
      una chiamata diretta a `MarkModified()`. Diciotto punti di
      mutazione in tutto, uno per uno invece di un unico punto di
      passaggio "furbo" (es. agganciarsi a `SaveUndoState`, già
      chiamato da quasi tutti): `SaveUndoState` non copre Formato
      numerico (le modifiche di stile non sono annullabili, scelta
      già presa in Fase 7) né i grafici/tabelle pivot incorporati, che
      avrebbero comunque richiesto un punto di aggancio separato —
      meglio diciotto chiamate esplicite e verificabili una per una
      che un aggancio implicito che copre l'80% dei casi e va
      integrato ad hoc per il resto.

      Nuovo/Apri/Esci chiedono conferma con un `BAlert` prima di
      scartare, **solo** se ci sono davvero modifiche in sospeso
      (`ConfirmDiscardChanges()`, che con `fModified == false` torna
      `true` subito senza mostrare nulla — nessun avviso invadente su
      un documento già pulito).

      Il titolo mostra il nome del file corrente (o "Nuovo documento"
      se non ancora salvato) con un asterisco in testa quando ci sono
      modifiche non salvate, es. `"* foglio.ascd — Atomo123"`.

      `ConfirmDiscardChanges()`/`IsModified()` diventano pubblici in
      `MainWindow`, esposti apposta per essere testabili direttamente
      — stesso principio già usato per `CopySelection`/
      `PasteSelection`/`SetCellFormat` in Fase 7.

      Test dedicato `ui/tests/test_unsaved_changes.cpp` (nuovo target
      `make test-unsaved-changes`): stato iniziale non modificato,
      titolo senza indicatore e senza nome file, conferma che non
      mostra nessun `BAlert` quando non ci sono modifiche in sospeso
      (l'unico ramo testabile in automatico: un vero clic su un
      dialogo modale non lo è, stesso limite già documentato altrove
      in questo progetto per l'interazione diretta — vedi Maiusc+click
      in `test_selection.cpp`), una mutazione fatta tramite
      `SheetView` che marca correttamente il documento come
      modificato tramite il nuovo ponte. Nessuna regressione nella
      suite esistente.

      **Non ancora fatto in questo incremento**: titolo finestra con
      nome file era già incluso sopra (stesso meccanismo, nessun
      lavoro separato); restano icone sulla toolbar e ridimensionamento
      riga/colonna, entrambi scelti dall'utente ma non ancora
      cominciati.

- [x] **Ridimensionamento di righe e colonne**: larghezza/altezza,
      finora fisse per tutte (`kColWidth`/`kRowHeight`), diventano un
      array per colonna/riga (`fColWidths`/`fRowHeights`), modificabile
      trascinando il confine fra due intestazioni — verticale in cima
      per le colonne, orizzontale a sinistra per le righe — riconosciuto
      entro pochi pixel dal confine stesso (`kResizeGrip`). Non si può
      stringere sotto un minimo (`kMinColWidth`/`kMinRowHeight`), per
      non far sparire la colonna/riga insieme alla maniglia per
      riallargarla.

      `fColOffsets`/`fRowOffsets` tengono la somma cumulativa delle
      larghezze/altezze, ricostruita solo quando qualcosa cambia
      davvero (`RebuildColumnOffsets`/`RebuildRowOffsets`): `CellRect()`
      resta O(1) e la ricerca "che colonna/riga c'è sotto questa
      coordinata" (`ColumnAtX`/`RowAtY`, usata da `CellAt` e dalle
      maniglie di ridimensionamento) diventa O(log n) con una ricerca
      binaria, invece di risommare da zero a ogni chiamata — importante
      perché `Draw()` ne fa diverse per ogni ridisegno. Dopo un
      ridimensionamento la vista si ridimensiona a sua volta
      (`UpdateCanvasSize`, un `ResizeTo` alla nuova dimensione totale
      del foglio) così `FixupScrollBars` (richiamato automaticamente da
      `FrameResized`) riflette il nuovo spazio scorrevole.

      Un dettaglio delicato non ovvio dal solo codice nuovo: le due
      intestazioni sono "congelate" durante lo scroll (l'intestazione
      di colonna segue `Bounds().top`, quella di riga `Bounds().left`
      — vedi `Draw()`), quindi il confronto per riconoscere la
      maniglia di ridimensionamento in `MouseDown` usa `Bounds()`, non
      coordinate assolute fisse: altrimenti il ridimensionamento
      avrebbe smesso di funzionare correttamente una volta scorso il
      foglio.

      **Limiti noti, documentati in `SheetView.h`**: vale solo per la
      sessione corrente, non è salvato nel file `.ascd` (richiederebbe
      una nuova sezione nel formato file, non fatta in questo
      incremento — si torna alla larghezza/altezza predefinita
      riaprendo il foglio); non è annullabile né marcato come
      "documento modificato", essendo una preferenza di sola
      visualizzazione, non parte del contenuto persistito.

      `CellRect`/`CellAt` diventano pubblici, esposti apposta per
      essere testabili direttamente — stesso principio già usato per
      Ordina/Riempi/Seleziona tutto.

      Test dedicato `ui/tests/test_resize.cpp` (nuovo target
      `make test-resize`, 9 verifiche): allargare una colonna sposta
      le colonne successive senza toccarne la larghezza, le celle
      restano contigue (nessun buco né sovrapposizione), il limite
      minimo di larghezza, `CellAt` resta coerente con `CellRect` dopo
      un ridimensionamento, lo stesso per il ridimensionamento di
      riga. Nessuna regressione nella suite esistente (verificato
      anche con esecuzioni ripetute e con un avvio dal vivo
      dell'applicazione).

      **Indizi visivi aggiunti in un secondo momento**, segnalati
      dall'utente dopo aver provato la funzionalità dal vivo: senza
      nessun riferimento a schermo, il ridimensionamento si poteva
      scoprire solo trascinando alla cieca vicino a un'intestazione.
      Aggiunti due indizi (entrambi richiesti): tre puntini disegnati
      su ogni confine ridimensionabile fra due colonne/righe
      nell'intestazione, e un cursore a doppia freccia
      (`B_CURSOR_ID_RESIZE_EAST_WEST`/`NORTH_SOUTH`) quando il mouse
      passa sopra un confine, anche senza trascinare — stessa idea del
      cursore di Excel/LibreOffice Calc. Il confronto per riconoscerli
      usa `Bounds()`, non coordinate assolute fisse, per restare
      corretto anche a foglio scorso.

      Con questo, tre dei quattro punti scelti dall'utente per la
      Fase 8 sono completi; resta solo icone sulla toolbar.

- [x] **Icone sulla toolbar**: quarto e ultimo punto scelto
      dall'utente per questa fase. Il sito autorizzato per le icone
      del progetto (www.hvif-store.art) risultava ancora vuoto ("0
      icons found") al terzo controllo di seguito, stesso esito delle
      volte precedenti (vedi anche l'icona dell'applicazione in Fase
      4, bloccata allo stesso modo a suo tempo). Invece di aspettare
      oltre o passare da Icon-O-Matic, le otto icone (Nuovo/Apri/
      Salva/Stampa/Taglia/Copia/Incolla/Trova) sono disegnate
      direttamente in `ui/src/ToolbarIcons.cpp` con le normali
      funzioni di `BView` (`StrokeRect`/`StrokeLine`/`StrokeEllipse`)
      su un `BBitmap` 16x16 `B_RGBA32` offscreen a sfondo trasparente,
      monocromatiche, nello stesso stile minimale dei toolbar nativi
      Haiku — non HVIF: l'icona dell'applicazione resta l'unica a
      passare da quel formato/pipeline.

      `BButton::SetIcon` copia i bit al suo interno (non ne prende
      possesso): il `BBitmap` temporaneo restituito da ogni funzione
      va eliminato subito dopo (nuovo helper `SetToolbarIcon` in
      `MainWindow.cpp`), non tenuto in vita.

      Nessun test dedicato: sono bitmap disegnati, non logica di
      calcolo/stato da verificare — controllato visivamente avviando
      l'applicazione dal vivo. Nessuna regressione nella suite
      esistente.

      Con questo, tutti e quattro i punti scelti dall'utente per la
      Fase 8 sono completi.

## Fase 9 — Supporto multi-foglio (CHIUSA)

Con la Fase 8 chiusa, l'utente ha indicato un vero file di lavoro —
`Form_Economico_GaraEPC2026_Nord_Est.xlsm`, una gara d'appalto reale —
come banco di prova per continuare lo sviluppo in autonomia "fino al
raggiungimento della piena compatibilità e visualizzazione". Aprirlo
ha subito rivelato che il file ha **38 fogli**, con formule che
attraversano i fogli (es. `RIEPILOGO COMPLETO` referenzia altri fogli
166 volte) — mentre l'importazione XLSX, fin dalla Fase 3, leggeva
solo il primo foglio e `MainWindow` gestiva un solo documento alla
volta. Di fronte a un cambio architetturale di questa portata, prima
di procedere è stata chiesta conferma esplicita all'utente, che ha
risposto: "è assolutamente necessario supportare il multi-sheet".

- [x] **Formato "cartella di lavoro" multi-foglio (`ASCB`)**: nuovo
      livello sopra il formato `ASCD` esistente, in `ui/src/AscdIO.h/
      .cpp`. Magic `"ASCB"` + numero di fogli, poi per ciascuno
      nome + un blocco `ASCD` completo, riusando `SaveASCD`/`LoadASCD`
      così come sono (ognuno si autodelimita tramite i propri
      contatori) invece di duplicarne la logica. `IsASCDBookFile`
      distingue un file `ASCB` da un vecchio `.ascd` a un solo foglio
      (`"ASCD"`, senza wrapper): piena compatibilità all'indietro in
      lettura. Test dedicato `ui/tests/test_ascd_book.cpp` (nuovo
      target `make test-ascd-book`, 13 verifiche): giro completo a due
      fogli, nomi preservati, ricalcolo delle formule, un vecchio file
      a un solo foglio non viene mai scambiato per una cartella.

- [x] **`MainWindow` multi-foglio**: `fDoc`/`fCharts` restano
      deliberatamente gli "alias del foglio attivo" (mai rimossi) così
      le circa quaranta chiamate esistenti che li usano (Copia/Incolla,
      Formato, Trova e sostituisci, grafico/tabella pivot incorporati,
      ecc.) non richiedono nessuna modifica. `std::vector<AscdSheet>
      fSheets` tiene tutti i fogli aperti; `SwitchToSheet(indice)`
      ripunta semplicemente `fDoc` allo stesso `CContainer*` già in
      `fSheets[indice].doc` (nessuna copia) e sincronizza `fCharts` in
      entrambe le direzioni esplicitamente, essendo un vettore per
      valore di cui `SheetView` tiene un puntatore che deve restare
      stabile. Un nuovo `BMenuField` sotto il foglio permette di
      cambiare foglio attivo dal menu a tendina (sostituito da una
      striscia di schede subito dopo, vedi sotto — l'utente l'ha
      provato dal vivo e ha preferito l'aspetto di Excel/LibreOffice
      Calc).

      **Bug reale scoperto e corretto durante lo sviluppo**:
      `ResetWorkbook()`, chiamato quasi subito nel costruttore,
      controllava `if (fSheetView)`/`if (fSheetSelector)` prima che
      l'uno o l'altro fossero mai stati assegnati (nessuna voce nella
      lista di inizializzazione, assegnati più avanti nel corpo del
      costruttore) — un puntatore letto non inizializzato, che quando
      per caso sembrava non-NULL faceva chiamare `SetDocument`/
      `SetCharts` su un puntatore selvaggio, con blocchi intermittenti
      e legati al layout di memoria del momento. Scoperto scrivendo
      `ui/tests/test_multisheet.cpp` (nuovo target `make
      test-multisheet`, 14 verifiche) tramite bisezione con
      checkpoint: corretto assegnando esplicitamente `fSheetView =
      NULL; fSheetSelector = NULL;` prima della prima chiamata a
      `ResetWorkbook()`.

- [x] **Striscia di schede al posto del menu a tendina**: appena
      provato dal vivo, l'utente ha chiesto una striscia di schede in
      basso (come Excel/LibreOffice Calc) al posto del `BMenuField`
      appena aggiunto, notando subito il problema che aveva motivato
      la scelta iniziale — con decine di fogli le schede non entrano
      tutte nella larghezza della finestra, quindi serve un modo per
      scorrerle.

      Nuova `SheetTabView` (`ui/src/SheetTabView.h/.cpp`), un `BView`
      disegnato a mano come le altre viste di questo progetto
      (`SheetView`, `ToolbarIcons`): calcola quante schede entrano
      nello spazio disponibile a partire da `fFirstVisible` (la prima
      scheda visibile), con due frecce a sinistra per scorrere quando
      non entrano tutte. `SetSheets()` porta sempre la scheda attiva
      in vista se non lo è già (dopo Apri o un cambio foglio), ma la
      funzione di solo-disegno (`Layout()`, richiamata anche da
      `Draw()` per adattarsi a un ridimensionamento della finestra) non
      lo fa mai: altrimenti scorrere manualmente con le frecce per
      guardare altre schede verrebbe subito annullato dal ridisegno
      successivo, che raggiungerebbe di nuovo quella attiva. Invia lo
      stesso messaggio `kMsgSwitchSheet` già gestito da
      `MainWindow::MessageReceived` (un `BMessage` con un intero
      "index", esattamente come inviava il vecchio `BMenuItem`), quindi
      il cambio foglio vero e proprio in `MainWindow` non è cambiato
      affatto — solo il widget che lo innesca.

      `TabRectFor`/`LeftArrowRect`/`RightArrowRect`/`IsScrolling`/
      `FirstVisibleIndex` pubblici apposta per essere testabili
      direttamente, stesso principio già usato per `SheetView::
      CellRect`/`CellAt`. Test dedicato `ui/tests/test_sheet_tabs.cpp`
      (nuovo target `make test-sheet-tabs`, 12 verifiche): poche schede
      corte non richiedono scorrimento, venti schede sì, le frecce
      scorrono di una scheda alla volta senza mai cambiare il foglio
      attivo (verificato lasciando un breve `snooze` dopo aver
      rilasciato il lock della finestra di prova, perché l'invio del
      messaggio è asincrono come un vero clic su un menu), un clic su
      una scheda visibile invia l'indice giusto. Nessuna regressione
      nelle altre 17 suite di test.

- [x] **`XlsxTranslator` legge tutti i fogli, non solo il primo**:
      `xl/workbook.xml` elenca nome e `r:id` di ciascun foglio
      nell'ordine delle schede; `xl/_rels/workbook.xml.rels` fa
      corrispondere ogni `r:id` al file XML fisico che contiene i
      dati (i due non sono necessariamente allineati numericamente —
      `rId1` può benissimo puntare a `sheet47.xml`). Se manca anche
      solo uno dei due pezzi, si torna al comportamento precedente (un
      solo foglio, `xl/worksheets/sheet1.xml`) invece di fallire del
      tutto. L'esportazione (ASCD → XLSX) resta a un solo foglio, non
      toccata da questo incremento.

      **Due bug reali scoperti aprendo il file da 38 fogli**, in
      questo ordine:

      1. **Corruzione dello stack su testo lungo**: `GetCellFormula`/
         `GetCellResult` di `CContainer` scrivevano nel buffer del
         chiamante senza mai controllarne la dimensione — `FormatValue`
         (caso testo) e `CFormula::UnMangle` non hanno mai avuto un
         limite in tutta la storia di questo codice, ereditato da
         Sum-It. Il foglio "Indice Pricelist" ha una nota introduttiva
         di circa 2900 caratteri in una singola cella: scritta in un
         tipico `char text[512]` locale del chiamante, corrompeva lo
         stack — non un crash pulito ma un blocco apparente, lo stesso
         pattern di intercettazione di `debug_server` già visto altre
         volte in questo progetto per corruzioni di memoria. Trovato
         bisecando con `fprintf`/checkpoint attraverso l'intera catena
         (`Translate` → `WriteASCDBook` → `WriteASCD` →
         `GetCellFormula`), fino a isolare la cella esatta con uno
         script Python sul contenuto XML grezzo. **Corretto**: le due
         funzioni ora richiedono la dimensione del buffer del
         chiamante e formattano prima in un buffer di appoggio
         dimensionato sul contenuto reale (o un limite esplicito
         generoso per le formule), poi copiano nel buffer del
         chiamante con `strlcpy`, mai scrivendo oltre. Il buffer tipico
         dei chiamanti (traduttori, `SheetView`, `MainWindow`) sale
         anche da 512 a 4096 byte, per non troncare inutilmente testi
         lunghi ma comuni (il più lungo trovato in questo file è di
         3300 caratteri). Alzato anche `kMaxStringLength` di
         `CFormula::UnMangle` (256 → 4096) per lo stesso motivo sulle
         formule, difesa in profondità anche se non era la causa di
         questo blocco specifico.

      2. **Disallineamento fra fogli in un `ASCB`**: `WriteASCD` del
         translator XLSX non scrive mai una sezione grafici (non
         gestisce grafici); `LoadASCD` invece la considera sempre
         presente, distinguendo "fine vera del flusso" (nessun
         grafico, formato vecchio) da "sezione presente" solo provando
         a leggere 4 byte in più e controllando se lo stream è già
         finito lì. Questa euristica funziona per un singolo blocco
         `ASCD` isolato, ma **non** quando `WriteASCDBook` incapsula
         più fogli in sequenza nello stesso flusso: per ogni foglio
         tranne l'ultimo, quei 4 byte "in più" non sono la vera fine
         del flusso ma i primi 4 byte del foglio successivo (la
         lunghezza del suo nome), letti per errore come numero di
         grafici — disallineando la lettura di ogni foglio dopo il
         primo. Il sintomo era subdolo: `MainWindow::OpenFile`
         mostrava solo un foglio invece di 38, senza errori né blocchi
         (il primo foglio si leggeva sempre correttamente per
         coincidenza). **Corretto** scrivendo sempre un contatore
         grafici a zero in coda a `WriteASCD`, eliminando l'ambiguità
         alla radice invece di renderla innocua caso per caso.

      Verificato con una sonda dedicata (non nella suite di test, solo
      per questo controllo) che apre il file reale attraverso
      `MainWindow::OpenFile` — lo stesso percorso di File > Apri: i 38
      fogli si aprono tutti correttamente, coi nomi giusti, navigabili
      dal menu a tendina.

      Nessuna regressione: tutte le 17 suite di test della UI e le 4
      dei translator restano verdi dopo entrambe le correzioni.

- [x] **Larghezza di colonna in funzione del file aperto**: le colonne
      mostravano finora sempre la larghezza predefinita
      (`SheetView::kColWidth`), qualunque fosse la larghezza scelta nel
      file originale — solo il ridimensionamento manuale trascinando
      un'intestazione era supportato (Fase 8), e valeva solo per la
      sessione corrente. Richiesto dall'utente subito dopo aver
      provato la striscia di schede dal vivo su questo stesso file da
      38 fogli.

      `XlsxTranslator` legge `<cols>` dal foglio XLSX (intervalli di
      colonne con una larghezza esplicita in caratteri) e converte
      l'unità di misura di Excel in pixel con un'approssimazione
      ampiamente usata da importatori più semplici (`pixel =
      caratteri*7 + 5`) — non l'algoritmo esatto dipendente dal
      font/DPI del documento originale (ECMA-376, 18.3.1.13), che
      questo motore non modella. Il formato ASCD/ASCB guadagna una
      nuova sezione opzionale in coda, dopo quella dei grafici, stesso
      principio (assente in un file scritto prima di questa modifica,
      mai un errore): le sole colonne la cui larghezza differisce da
      quella predefinita, non un array denso su tutte le colonne.

      `SheetView::SetColumnWidths`/`CustomColumnWidths` applicano/
      catturano queste larghezze (pubblici apposta per essere
      testabili direttamente, stesso principio di `CellRect`/
      `CellAt`); `MainWindow` le sincronizza a ogni apertura file e a
      ogni cambio di foglio attivo (`SwitchToSheet`), cosí un
      ridimensionamento fatto a mano su un foglio non si mostra più
      per errore su un altro (bug preesistente, mai notato perché il
      ridimensionamento non persisteva comunque), e sopravvive al
      salvataggio nel formato nativo.

      **Irrobustimento scoperto strada facendo**: le sezioni opzionali
      di `LoadASCD` (grafici, ora anche larghezze di colonna)
      consumavano i propri byte dallo stream solo quando il chiamante
      passava un puntatore non nullo per riceverli — se un futuro
      chiamante avesse passato `NULL` su un blocco ASCD incapsulato in
      una cartella multi-foglio, la lettura del foglio successivo si
      sarebbe disallineata esattamente come il bug della sezione
      grafici sopra. Nessun chiamante reale lo fa oggi, ma corretto
      comunque alla radice: ora i byte si consumano sempre se presenti,
      indipendentemente da cosa chiede il chiamante.

      Test: round-trip delle larghezze in `AscdIO`
      (`test_ascd_io.cpp`), `SetColumnWidths`/`CustomColumnWidths` e
      interazione col trascinamento diretto (`test_resize.cpp`),
      persistenza per foglio nel cambio foglio avanti e indietro
      (`test_multisheet.cpp`), lettura di `<cols>` dal file XLSX di
      prova (`test_xlsx_translator.cpp`, esteso con un blocco
      `<cols>`). Verificato anche con una sonda dedicata sul file reale
      da 38 fogli: colonne di larghezza diversa foglio per foglio,
      niente più larghezza fissa uguale ovunque. Nessuna regressione
      nelle altre suite.

- [x] **Colori di sfondo/testo in funzione del file aperto**: le celle
      mostravano finora sempre sfondo bianco e testo nero, qualunque
      fosse la colorazione del file originale — chiesto dall'utente
      subito dopo la larghezza di colonna sopra, stesso principio.

      Il motore ha già un campo per il colore di sfondo
      (`CellStyle::fLowColor`, mai letto dalla UI) fin dal porting da
      Sum-It, ma nessun colore di testo (nuovo campo `fHighColor`) né
      alcuna lettura/scrittura effettiva — `GetCellStyle`/
      `SetCellStyle`/`GetColumnStyle`/`SetColumnStyle` erano già
      completamente funzionanti, solo mai usati per altro che il
      formato numerico (menu Formato).

      `XlsxTranslator` legge `xl/theme/theme1.xml` (la tavolozza a 12
      colori del documento) e `xl/styles.xml` (fills/fonts/cellXfs),
      risolvendo sia i colori diretti (`rgb="..."`) sia quelli del
      tema con tinta (`theme="N" tint="..."`), con la stessa
      approssimazione ragionevole già scelta per la larghezza di
      colonna invece dell'esatta conversione RGB↔HSL dello standard.
      L'indice del tema non segue l'ordine degli elementi in
      `<a:clrScheme>`: scambia i primi quattro (0=lt1, 1=dk1, 2=lt2,
      3=dk2), una stranezza nota di OOXML. Applicato sia per cella
      (`s="..."` su ogni `<c>`) sia per colonna (`style="..."` su
      `<col>`, che `GetCellStyle` usa già come ripiego per le celle
      senza uno stile proprio).

      **Bug reale scoperto scrivendo il test dedicato**: applicare il
      colore PRIMA di scrivere il contenuto della cella lo perdeva
      subito dopo, perché `CContainer::NewCell` (chiamata anche per
      scrivere testo/formula) sovrascrive sempre l'intero `CellData`
      della cella, stile compreso — ogni cella con un contenuto
      proprio risultava quindi sempre al colore predefinito nonostante
      `s="..."` fosse letto e risolto correttamente (verificato con
      una sonda dedicata: i valori risolti da tema+tint erano esatti,
      solo mai applicati con successo alla cella finale). Corretto
      invertendo l'ordine: colore sempre dopo il contenuto.

      Il formato ASCD/ASCB guadagna due nuove sezioni opzionali in
      coda (colori di cella e di colonna, stesso principio delle
      sezioni già esistenti per grafici/larghezze) — a differenza
      della larghezza di colonna, che vive solo in `SheetView`, il
      colore vive già dentro `CContainer`: `SaveASCD` lo legge
      direttamente da "doc" e `LoadASCD` lo scrive direttamente lì,
      senza bisogno di un canale esterno separato come `colWidths`.

      `SheetView::Draw` ora riempie lo sfondo di ogni cella col suo
      colore (sotto le righe della griglia, che restano visibili
      sopra, come in Excel/LibreOffice Calc) e disegna il testo col
      suo colore, invece dei valori fissi bianco/nero di sempre.

      Test: risoluzione tema+tint e applicazione `s=`/`style=` in
      `test_xlsx_translator.cpp` (esteso con `xl/theme/theme1.xml` e
      `xl/styles.xml` nel file di prova), round-trip dei colori in
      `AscdIO` (`test_ascd_io.cpp`, incluso il ripiego sul colore di
      colonna per una cella vuota). Verificato anche con una sonda
      dedicata sul file reale da 38 fogli: colori diversi da foglio a
      foglio, non più bianco/nero fisso ovunque. Nessuna regressione
      nelle altre suite.

- [x] **Apertura automatica dal doppio clic in Tracker**: cliccare due
      volte su un file XLSX/XLSM/XLS/ODS/CSV/ASCD non avviava Atomo123
      — segnalato dall'utente. La causa non era mancanza di supporto
      (il Translation Kit sapeva già aprire tutti questi formati), ma
      il database MIME di sistema: Tracker sceglie l'app da avviare in
      base all'"applicazione preferita" registrata per il tipo MIME del
      file (`BMimeType::SetPreferredApp`), non in base a quali app
      *sanno* aprirlo.

      `Atomo123.rdef` guadagna una risorsa `file_types` (`BEOS:
      FILE_TYPES`) che elenca i tipi supportati — necessaria perché
      Atomo123 compaia in "Apri con..." di Tracker, ma **non**
      sufficiente da sola per l'apertura automatica. `App::
      RegisterFileTypes()`, chiamata a ogni avvio da `ReadyToRun()`,
      imposta esplicitamente Atomo123 come applicazione preferita per
      ciascun tipo, ma **solo** se quel tipo non ne ha già una — non
      scavalca mai una scelta già fatta dall'utente o da un'altra
      applicazione installata. Verificato con una sonda dedicata
      (`BMimeType::GetPreferredApp`) sui sei tipi supportati: tutti
      impostati su Atomo123, nessuno era già assegnato ad altro.

      Aggiunto anche `app_flags B_SINGLE_LAUNCH` (esplicito, era già il
      comportamento di default) per chiarezza.

- [x] **Una finestra per ogni file aperto, invece di una sola**:
      aprire un secondo file mentre Atomo123 era già in esecuzione
      rimpiazzava il documento della finestra esistente — segnalato
      dall'utente subito dopo l'apertura automatica sopra, con lo
      stesso rischio pratico di perdita del lavoro già aperto.

      `App` non tiene più un singolo `MainWindow* fWindow`: l'elenco
      delle finestre aperte è quello che `BApplication` gestisce già da
      sé (`CountWindows()`/`WindowAt()`), niente struttura duplicata da
      tenere sincronizzata. `App::RefsReceived` cerca fra le finestre
      esistenti un'eventuale finestra "vergine" (`MainWindow::
      IsUntouched()`, mai modificata e senza nessun file aperto con
      successo — lo stesso stato della finestra creata da `ReadyToRun`
      all'avvio) da riusare; se non ne trova, ne apre una nuova, senza
      mai rimpiazzare un documento già aperto. `ReadyToRun` stesso crea
      la finestra iniziale solo se `RefsReceived` non ne ha già creata
      una (l'avvio con un file passato da Tracker può consegnare
      `B_REFS_RECEIVED` prima o dopo `ReadyToRun`, ordine non
      garantito — la ricerca della finestra vergine funziona
      correttamente in entrambi i casi).

      Rimosso `B_QUIT_ON_WINDOW_CLOSE` dal costruttore di `MainWindow`
      (avrebbe chiuso l'intera applicazione alla chiusura di una
      qualunque finestra, anche con altre ancora aperte):
      `MainWindow::QuitRequested()` ora chiude l'applicazione da sé,
      ma solo quando quella che si sta chiudendo è rimasta l'ultima.

      **Bug reale scoperto scrivendo il test dedicato**: contare le
      finestre rimaste con `BApplication::CountWindows()` grezzo non
      basta — ogni `MainWindow` crea anche due `BFilePanel` (Apri/Salva
      con nome), che sono `BWindow` a loro volta e si registrano
      anch'essi presso l'app, quindi il conteggio grezzo resta sempre
      sopra 1 anche con una sola `MainWindow` aperta: l'applicazione
      non sarebbe mai terminata chiudendo l'ultima finestra. Corretto
      filtrando per tipo (`dynamic_cast<MainWindow*>`) sia in
      `QuitRequested()` sia nella ricerca della finestra vergine.

      Test: `test_multiwindow.cpp`, nuovo — usa la vera classe `App`
      (non una `BApplication` generica) per esercitare la logica reale
      di `RefsReceived`/`FindReusableWindow` senza reimplementarla nel
      test; il target Makefile ricompila `App.cpp` a parte con
      `-DATOMO123_TEST_BUILD` per escluderne il `main()`, che altrimenti
      confliggerebbe con quello del test. Verificato anche dal vivo:
      processo lanciato, primo file aperto, secondo file aperto tramite
      lo stesso meccanismo `B_REFS_RECEIVED`/`BMessenger` usato da
      Tracker — il processo è rimasto in esecuzione con due finestre
      (conteggio thread coerente, titolo della prima finestra
      verificato via scripting BeAPI) invece di terminare o perdere il
      primo documento. Nessuna regressione nelle altre 18 suite.

- [x] **Toolbar con icone HVIF vere, raggruppate per categoria**: la
      toolbar aveva solo otto pulsanti (Nuovo/Apri/Salva/Stampa/Taglia/
      Copia/Incolla/Trova) con icone disegnate a codice, scelta presa
      in Fase 8 perché il sito autorizzato per le icone del progetto
      (www.hvif-store.art) risultava vuoto in tre controlli successivi.
      Il sito è ora popolato — l'utente ha costruito un catalogo
      HVIF scaricato da li' (`Atomo123_icons/`, fuori da questo
      repository: script di raccolta, `catalog.csv`/`CATALOGO.md`, e
      `ATOMO123.md` con una selezione ragionata per le funzioni di
      Atomo123) e ha chiesto di collegarlo alla toolbar, con un layout
      "come Excel": icone imparentate raggruppate, separate da un
      divisore sottile, non una `BToolBar` per categoria (quella
      classe non è nell'SDK pubblico stabile di questo sistema, vedi
      il commento già presente sui `BButton`).

      `ToolbarIcons.h`/`.cpp` (i glifi disegnati a codice) sono stati
      rimossi, sostituiti da `IconCatalog.h`/`.cpp` (rendering vettore
      via `BIconUtils::GetVectorIcon`, stessa API con cui Haiku
      renderizza le icone di Tracker) e `IconData.cpp` (i byte grezzi
      HVIF di 15 icone incorporati come array C, non file `.hvif`
      separati da distribuire a parte — stesso principio già scelto
      per l'icona dell'applicazione in `Atomo123.rdef`). Tutte le 15
      icone hanno licenza MIT (verificata riga per riga in
      `Atomo123_icons/catalog.csv` prima di incorporarle).

      La toolbar stessa non è più scritta a mano un pulsante alla
      volta: `MainWindow.cpp` guadagna una tabella dichiarativa
      (`kToolbarGroups`, un `ToolbarGroupDef` per voce di menu
      principale — File/Modifica/Dati/Inserisci) e `BuildToolbar()`,
      che la percorre creando ogni `BButton` con la sua icona e un
      `BSeparatorView` verticale fra un gruppo e il successivo. Solo
      funzioni già implementate da un comando vero (Annulla/Ripeti,
      Elimina, Ordina crescente/decrescente, Grafico, Tabella pivot
      oltre agli otto originali) — niente pulsanti per funzioni ancora
      "da disegnare" nel catalogo o non presenti in Atomo123
      (formattazione testo, filtro, zoom...): 15 pulsanti in tutto,
      contro gli 8 di prima.

      Verificato dal vivo: processo lanciato, screenshot della
      finestra — icone vettoriali vere (a colori, non più i glifi
      grigi disegnati a mano) visibili e raggruppate correttamente con
      il separatore fra File e Modifica. Nessuna regressione nelle 19
      suite automatiche (nessun test dedicato alla sola toolbar: è
      composizione diretta di `BButton`/`BBitmap` già esercitata
      indirettamente da ogni test che crea una `MainWindow` reale).

      **Rifinitura, stesso giorno**: con l'etichetta di testo sempre
      visibile accanto a ogni icona, i soli quattro pulsanti File
      arrivavano quasi al limite della larghezza predefinita della
      finestra — l'utente ha fatto notare che nascondendo il testo (e
      mostrandolo solo al passaggio del mouse) si risparmia spazio.
      `BuildToolbar()` ora crea ogni `BButton` senza etichetta
      (`NULL`) e imposta il testo con `BView::SetToolTip()` invece —
      pulsanti solo icona, come la barra Standard di Excel vera.
      Verificato di nuovo dal vivo: la toolbar occupa una riga
      sensibilmente più stretta a parità di pulsanti. Nessuna
      regressione nelle 19 suite.

- [x] **Formule che attraversano i fogli** ("NomeFoglio!Cella",
      l'ultimo limite noto rimasto aperto da questo incremento):
      166 riferimenti incrociati in "RIEPILOGO COMPLETO" del file
      reale (es. `+MT_CM_Installazione!I56`) venivano importati come
      testo/formula grezza ma non calcolati — il motore, ereditato da
      Sum-It, non sapeva risolvere un riferimento verso un `CContainer`
      diverso dal proprio.

      **Meccanismo**: `ISheetResolver` (nuova interfaccia in
      `Container.h`), implementata da `MainWindow` (non dal motore
      isolato, che non sa nulla di "cartella di lavoro"), risolve un
      *nome* di foglio verso il `CContainer` corrispondente fra
      `fSheets`. Nuovi token bytecode `valXRef`/`valXRange`
      (`Formula.h/.cpp`) incorporano il nome del foglio come stringa
      (non un indice), esattamente come `valName` già fa per gli
      intervalli con nome — risolto sempre e solo in fase di
      **calcolo**, mai di parsing.

      **Bug reale scoperto per primo (progettazione iniziale)**: la
      prima versione risolveva il nome del foglio in un *indice*
      già in fase di parsing (`CParser` doveva già conoscere l'elenco
      dei fogli). Funzionava nel motore isolato ma falliva sempre dopo
      un giro salva/ricarica: `LoadASCD` ri-analizza ogni foglio
      *singolarmente* (il testo della formula è quello che è, il
      formato nativo salva/ricarica le formule come testo, non come
      bytecode), prima che tutti i fogli della cartella di lavoro
      esistano e siano collegati fra loro — un riferimento incrociato
      diventava quindi un identificatore sconosciuto, silenziosamente
      salvato come testo puro invece che come formula. Riprogettato
      per nome (risoluzione differita al calcolo, come `valName`):
      "NomeFoglio!Cella" è sempre e comunque un riferimento incrociato
      a livello sintattico, si limita a non risolversi (`eNoData`) se
      il foglio non esiste ancora o il resolver non è collegato — mai
      un crash, mai una falsa retrocessione a testo. Scoperto e
      corretto scrivendo `ui/tests/test_xsheet_formulas.cpp`, che apre
      davvero un file con `MainWindow::OpenFile` invece di limitarsi
      al motore isolato.

      **Secondo bug reale scoperto (nomi di foglio fra apici)**:
      ispezionando l'XML grezzo del file di gara reale, 118 dei 143
      riferimenti incrociati usano la sintassi Excel con apici singoli
      (`'BT02 - CM_Installazione'!I29`, necessaria perché il nome
      contiene spazi/trattini) — solo una minoranza (i fogli
      "MT_CM_...", senza spazi) usa la sintassi semplice. Aggiunto un
      token lessicale `QIDENT` (stessa logica di `TEXT`, apice singolo
      invece di doppio, senza gestione di un apostrofo letterale
      sfuggito — semplificazione già accettata per `TEXT`) e la
      relativa produzione grammaticale in `CParser::Factor`/
      `ParseSheetReference`. **Bug scoperto scrivendolo**: il codice
      di chiusura di `GetNextToken` sovrascriveva il nome già ripulito
      dagli apici con il buffer grezzo (apici compresi), perché
      escludeva esplicitamente `TEXT` da quella sovrascrittura ma non
      il nuovo `QIDENT` — un riferimento incrociato fra apici
      risultava sempre non risolto, con il nome del foglio preceduto
      da un apice letterale mai tolto.

      `UnMangle` ricostruisce sempre il nome del foglio fra apici
      singoli (anche quando non sarebbero strettamente necessari):
      il bytecode non registra se era stato scritto con o senza, e
      senza le virgolette un nome con spazi non si rianalizzerebbe
      correttamente al giro salva/ricarica successivo.

      **Limite noto, non affrontato in questo incremento**: un vero
      intervallo multi-cella fra fogli usato come argomento di una
      funzione di aggregazione (es. `SUM('Foglio2'!A1:A10)`) non è
      supportato — le funzioni leggono il proprio argomento range dal
      `CContainer` "corrente", non da uno arbitrario; servirebbe
      estendere anche quel meccanismo. Il file reale non sembra
      usare questo schema (somma cella per cella con `+`, non `SUM`
      su un intervallo fra fogli), quindi non blocca l'uso pratico
      constatato finora. Resta anche il limite già noto sull'indice
      di foglio incorporato in `valXRef`/`valXRange` (ora un nome, non
      più un problema) — ma un foglio *rinominato* dopo che una
      formula lo referenzia romperebbe comunque quel riferimento
      (nessuna UI per rinominare un foglio, quindi non ancora
      raggiungibile).

      Test: `engine/tests/xsheet_test.cpp` (nuovo, 8 verifiche —
      calcolo, round-trip testuale UnMangle, nome fra apici,
      propagazione dopo una modifica, foglio inesistente, nessun
      resolver collegato, resolver rimosso dopo la compilazione,
      round-trip `Write`/`Read` sul formato nativo) e
      `ui/tests/test_xsheet_formulas.cpp` (nuovo, 6 verifiche — stesso
      comportamento end-to-end su una vera `MainWindow::OpenFile`,
      compreso un vero giro salva/ricarica su disco). Verificato anche
      aprendo dal vivo il file reale da 38 fogli: nessun crash,
      titolo/nomi di foglio (con spazi) corretti. Nessuna regressione
      nelle 19 suite UI + 2 suite motore preesistenti.

- [x] **Ricalcolo esteso a tutta la cartella di lavoro**: `MainWindow::
      RecalculateActiveWorkbook()` (nuovo, sostituisce ogni chiamata
      diretta a `RecalculateAll(fDoc)` in `MainWindow.cpp`/
      `SheetView.cpp`) ricalcola tutti i fogli — non solo quello
      attivo — quando la cartella di lavoro ne ha più di uno, così una
      modifica in un foglio si propaga correttamente a un altro
      foglio che lo referenzia in formula; con un solo foglio ricade
      sul più economico `RecalculateAll` di prima, nessun costo
      aggiuntivo per il caso comune. `AscdIO::RecalculateWorkbook`
      (nuovo) applica la stessa logica a convergenza già usata da
      `RecalculateAll` (più passate finché nessuna cella cambia più
      valore) a tutti i fogli insieme in ogni passata, non uno alla
      volta in sequenza — necessario perché una dipendenza circolare
      fra due fogli (A referenzia B, B referenzia A) converga
      correttamente nello stesso numero di passate di un riferimento
      circolare nello stesso foglio.

---

## Fase 10 — Persistenza completa delle preferenze di cella e vista (CHIUSA)

Con Fase 7 chiusa, quattro delle funzionalità appena recuperate sono
rimaste deliberatamente **solo per la sessione corrente**, non
salvate nel formato nativo (`AscdIO`/`AscdSheet`): Blocca riquadri
(righe/colonne bloccate), grassetto/corsivo (`CellStyle::fFont`),
allineamento (`CellStyle::fAlignment`), altezza di riga
(`SheetView::fRowHeights`, un limite noto anche prima di questa fase,
per la larghezza di colonna già persistita). Ognuna era stata
rimandata esplicitamente per non allargare la superficie di un
incremento già di per sé consistente — questa fase chiude quel
debito, estendendo `AscdIO` con lo stesso pattern già usato per
larghezza di colonna e colori (una sezione opzionale in coda al
formato, sempre consumata dallo stream anche se il chiamante non la
richiede, cosicché un file scritto prima di questa fase resti
leggibile senza modifiche).

Punto di attenzione particolare: `CellStyle::fFont` è un indice
**volatile**, valido solo all'interno di `gFontSizeTable` per la
durata del processo corrente (vedi il commento in Fase 7 su
`CFontSizeTable::GetFontID`/`GetFontInfo`) — non si può scrivere
l'indice grezzo su disco e rileggerlo in una sessione successiva
(punterebbe a una voce diversa o inesistente). Va invece serializzata
la tripla famiglia/stile/dimensione (già ottenibile con
`GetFontInfo`) e ricostruito l'indice con `GetFontID` al caricamento,
esattamente come fa già `Excel.pass1.cpp` per un file XLSX importato.

- [x] **Blocca riquadri e altezza di riga** (le due più semplici delle
      quattro: un intero per foglio la prima, una lista di righe non
      predefinite la seconda — nessuna delle due tocca l'indice
      volatile di `fFont`, vedi sopra). `SaveASCD`/`LoadASCD` (in
      `AscdIO.h`/`.cpp`) guadagnano tre nuovi parametri opzionali in
      coda alla firma esistente — `rowHeights`, `frozenRows`,
      `frozenCols` — stesso principio di `colWidths`: puntatori
      (`NULL` = non legge/scrive quella sezione), sempre consumati
      dallo stream se presenti anche quando il chiamante passa `NULL`
      (altrimenti disallineerebbe la lettura del blocco ASCD
      successivo in una cartella di lavoro multi-foglio, bug già
      scoperto per un'altra sezione in Fase 9). `AscdSheet` guadagna i
      campi corrispondenti (`rowHeights`, `frozenRows`, `frozenCols`).
      Nuovi `SheetView::SetRowHeights`/`CustomRowHeights`, speculari a
      `SetColumnWidths`/`CustomColumnWidths` già esistenti.
      `MainWindow::ResetWorkbook`/`SwitchToSheet`/`OpenFile`/
      `SaveToFile` applicano/raccolgono i nuovi campi negli stessi
      punti d'innesto già usati per `colWidths` — `SwitchToSheet` in
      particolare non azzera più il blocco riquadri a ogni cambio
      foglio (comportamento di Fase 7): lo salva nel foglio che si
      lascia e ripristina quello del foglio di destinazione, così
      ciascun foglio di una cartella di lavoro può avere il proprio.

      Test dedicato `ui/tests/test_persistence.cpp` (nuovo target
      `make test-persistence`, 8 verifiche, headless come
      `test_ascd_book.cpp` — solo `AscdIO`, nessuna vera
      `MainWindow`): un giro salva/ricarica con entrambe le
      preferenze impostate, un file scritto SENZA le nuove sezioni
      (chiamante che passa `NULL`, come ogni chiamata esistente prima
      di questa fase) resta leggibile e restituisce i valori
      predefiniti. Nessuna regressione nella suite esistente (tutti i
      28 target).

- [x] **Font e allineamento di cella**: le due restanti, entrambe
      per-cella invece che per-foglio come le prime due sopra. Due
      nuove sezioni in coda ad `AscdSheet`/`SaveASCD`/`LoadASCD`
      (stesso principio delle sezioni colori già esistenti, non nuovi
      parametri: entrambe leggono/scrivono direttamente su `doc`
      tramite `GetCellStyle`/`SetCellStyle`, come i colori):

      - **Font**: per ciascuna cella con `fFont` diverso dal
        predefinito, si scrive la tripla famiglia/stile/dimensione
        già risolta con `CFontSizeTable::GetFontInfo` — mai l'indice
        grezzo, per il motivo descritto sopra. `LoadASCD` la registra
        di nuovo con `GetFontID` (dedup se la stessa combinazione
        esiste già), ottenendo un indice valido per la sessione che
        ricarica — necessariamente diverso da quello scritto, ma
        questo non conta: solo la SheetView::Draw usa `CellStyle::
        fFont` come chiave verso `gFontSizeTable`, mai il valore
        grezzo confrontato altrove. Il colore del font (un campo
        separato dentro `CFontMetrics`, mai usato per disegnare il
        testo — `SheetView::Draw` legge sempre `CellStyle::
        fHighColor`, già persistito nella sezione colori) non serve.
      - **Allineamento**: un solo byte diretto per cella, nessuna
        risoluzione necessaria (a differenza del font, `CellStyle::
        fAlignment` è già il valore finale).

      Esteso `ui/tests/test_persistence.cpp` (ora 12 verifiche): una
      cella in grassetto e una allineata a destra sopravvivono al
      giro salva/ricarica (stessa famiglia/dimensione/"Bold" nello
      stile, indice grezzo diverso ma ininfluente), le celle mai
      toccate restano col predefinito. Nessuna regressione nella
      suite esistente (tutti i 29 target).

      **Con questo si chiude anche la Fase 10**: tutte e quattro le
      preferenze rimaste "solo per sessione" dopo la Fase 7 (Blocca
      riquadri, altezza di riga, font, allineamento) sopravvivono ora
      al salvataggio/riapertura nel formato nativo.

---

## Fase 11 — Bordi delle celle (CHIUSA)

`CellStyle` (motore) riserva già quattro campi per il colore del
bordo di ciascun lato di una cella (`fTBorderColor`/`fLBorderColor`/
`fBBorderColor`/`fRBorderColor`, tutti `uchar`) — ma, a differenza di
`fFont`/`fAlignment`/`fLowColor`/`fHighColor` (Fase 7), **nessun altro
punto del motore li legge o li scrive**: non hanno mai avuto un
significato definito, nessun equivalente dei "campi già pronti, solo
da esporre" trovati ripetutamente in Fase 7. Verificato con una
ricerca dedicata anche nel codice storico di Sum-It
(`legacy/opensumit/`): stessi quattro campi, stesso `memset` a zero
nel costruttore, **mai implementati nemmeno lì** — nessun contratto
storico da rispettare, libertà di definirne il significato da zero.

- [x] **Significato definito**: un byte booleano per lato (0 = nessun
      bordo, diverso da zero = bordo nero pieno spesso un pixel) —
      non un vero colore nonostante il nome storico del campo, e non
      uno spessore configurabile. Scelta deliberata per restare
      minimale: cambiare il layout di `CellStyle` (es. aggiungere
      `rgb_color` veri) avrebbe toccato `CellStyle::operator==`
      (basato su `memcmp` dell'intera struct) e ogni punto che la
      copia/confronta, per un beneficio visivo marginale rispetto a
      un bordo nero semplice. Nessuna modifica a `CContainer::
      GetCellStyle`/`SetCellStyle`: gestiscono già l'intera struct in
      modo generico, i quattro byte viaggiano gratis.
- [x] **Disegno**: `SheetView::DrawCellBand` disegna i bordi in un
      ciclo dedicato — non dentro quello del testo (che salta le
      celle vuote: una cella senza contenuto può comunque avere un
      bordo, stesso motivo per cui lo sfondo colorato non salta le
      celle vuote) — dopo la griglia sottile esistente (cosicché un
      bordo nero pieno si distingua sempre da un confine di griglia
      qualunque) e prima del testo (cosicché non copra mai un
      carattere che vi si sovrapponesse).
- [x] **UI**: menu Formato, cinque voci — Bordo superiore/sinistro/
      inferiore/destro (un lato alla volta, come Grassetto/Corsivo:
      `MainWindow::ToggleBorder(side)` legge lo stato dalla sola
      cella attiva e applica lo stato opposto a tutta
      `SelectionRange()`) e Nessun bordo (`ClearBorders`, toglie
      tutti e quattro i lati in un colpo solo, sempre sull'intera
      selezione). Niente finestra con anteprima: quattro lati
      indipendenti sono già chiari da soli come voci di menu, una
      finestra dedicata avrebbe aggiunto complessità senza un vero
      bisogno.
- [x] **Persistenza**: nuova sezione per-cella in coda ad `AscdIO`
      (stesso pattern per-cella di font/allineamento in Fase 10, non
      nuovi parametri di funzione: legge/scrive direttamente su `doc`
      tramite `GetCellStyle`/`SetCellStyle`) — quattro byte per cella
      con almeno un lato non predefinito.
- [x] Test dedicato `ui/tests/test_borders.cpp` (nuovo target
      `make test-borders`, 6 verifiche): un lato applicato a tutta la
      selezione non solo alla cella attiva, un secondo `ToggleBorder`
      sullo stesso lato lo toglie di nuovo, i quattro lati sono
      indipendenti fra loro e si accumulano su una stessa cella,
      `ClearBorders` li toglie tutti insieme. Esteso anche
      `ui/tests/test_persistence.cpp` (ora 14 verifiche) con un giro
      salva/ricarica su una cella con due lati impostati. Nessuna
      regressione nella suite esistente (tutti i 30 target).

### Bug scoperto: crash all'apertura di file XLSX multi-foglio dopo le nuove sezioni di Fase 10/11

Segnalato dall'utente con un report di crash di Haiku
(`Atomo123-33974-debug-*.report`): assert fuori range
(`inIndex <= fMax`, `RunArray2.cpp`) aprendo un file `.xlsm` reale a
38 fogli, con un `col` letto da uno stream palesemente sbagliato
(25454, ben oltre il massimo valido).

Causa: `SaveASCD`/`LoadASCD` (`ui/src/AscdIO.cpp`) hanno guadagnato
cinque nuove sezioni finali in Fase 10/11 (altezze di riga, Blocca
riquadri, font, allineamento, bordi di cella) — ma la copia duplicata
di `WriteASCD` in `translators/xlsx/XlsxTranslator.cpp` (stessa
duplicazione intenzionale già discussa sopra per non introdurre una
dipendenza di link fra translator e app) non era stata aggiornata di
pari passo. Per un solo foglio l'effetto passava inosservato — fine
flusso pulita viene trattata da `LoadASCD` come "sezione assente", un
fallback già esistente per la compatibilità con documenti vecchi —
ma in una cartella di lavoro multi-foglio (`WriteASCDBook`, un blocco
ASCD dietro l'altro sullo stesso flusso) i byte del foglio
**successivo** venivano letti come se fossero queste sezioni del
foglio corrente, disallineando tutta la lettura da lì in avanti.

**Fix**: aggiunte le cinque sezioni, vuote/a zero, nello stesso ordine
di `AscdIO.cpp`. Nessuna estrazione reale di queste informazioni dal
formato XLSX originale (fuori scopo per questo fix): il translator si
limita a non disallineare più lo stream. Verificato sia con un nuovo
test byte-per-byte in `translators/xlsx/tests/test_xlsx_translator.cpp`
(le cinque sezioni sono presenti e lo stream finisce esattamente alla
fine del buffer per il foglio singolo di `sample.xlsx`) sia dal vivo,
riaprendo il file `.xlsm` da 38 fogli che aveva originato il crash.

`CsvTranslator.cpp`/`XlsTranslator.cpp`/`OdsTranslator.cpp` non
implementano `WriteASCDBook` (nessun supporto multi-foglio in
scrittura, verificato non avere alcun riferimento a
`WriteASCDBook`/`LoadASCDBook`/`kASCDBookMagic`): non esposti a questo
bug, non modificati. Resta un rischio di manutenzione da tenere
presente: qualunque futura modifica al layout delle sezioni di
`SaveASCD` in `AscdIO.cpp` va replicata a mano in ogni `WriteASCD`
duplicato dei translator, non esiste oggi un controllo automatico che
lo verifichi.

---

## Fase 12 — Fedeltà visiva import XLSX (IN CORSO)

Verifica puntuale (grep mirato su `XlsxTranslator.cpp`, non solo
lettura del codice) contro `Form_Economico_GaraEPC2026_Nord_Est.xlsm`
(38 fogli, file di gara reale già usato per motivare Fase 9 e per il
bug corretto subito prima di questa fase): il translator importa oggi
solo valori/formule, larghezza di colonna e colori di sfondo/testo.
Zero righe toccano `mergeCell`, `numFmt`/`numFmtId`, i flag
grassetto/corsivo/sottolineato del font, allineamento/testo a capo,
bordi da stile, formattazione condizionale, tabelle strutturate o
immagini incorporate — tutti presenti nel file reale (537 celle unite
su 39 fogli, sette formati numero personalizzati, nove tabelle
strutturate, un logo incorporato, diverse regole di formattazione
condizionale). Aperto con Atomo123 il file si legge ma non somiglia
affatto a quello che si vede aprendolo con Excel vero: è questo il
gap che la fase chiude.

Per ciascun punto, verificato prima cosa esiste già in motore/UI (non
assunto): grassetto/corsivo, allineamento orizzontale e bordi hanno
già campo in `CellStyle` e disegno in `SheetView` (Fase 7/11) — solo
l'importazione XLSX manca. Celle unite, sottolineato e testo a capo
non hanno invece alcuna infrastruttura, nemmeno nel Sum-It storico:
richiedono progettazione originale come già successo per i bordi in
Fase 11.

- [x] **Formati numero**: legge `numFmtId` per cella (attributo `s=`
      dell'`<xf>` in `cellXfs`, aggiunto a `StylesContext::cellXfs`
      insieme a `fontId`/`fillId`) e traduce il `formatCode`
      associato (esplicito in `<numFmts>` o incorporato da una
      tabella dei più comuni, es. 44/9) riusando direttamente
      `CFormatter`/`CFormatter::FormatID()` invece di duplicarne
      l'euristica — il `formatCode` XLSX (già decodificato dalle
      entità XML) è quasi sempre un template compatibile così com'è.
      Limite onesto dell'engine, verificato leggendo
      `Formatter.template.cpp`/`Formatter.number.cpp`: niente simbolo
      di valuta personalizzato per formato, niente colore condizionale
      per i negativi (es. `0.00;[Red]-0.00`, presente nel file reale)
      — quella parte del formato viene scartata, non una regressione
      ma un'approssimazione dichiarata. I formati data/ora sono
      riconosciuti (euristica sulle lettere y/m/d/h/s fuori da apici e
      parentesi quadre — occhio a falsi positivi come la "d" di
      `[Red]`, bug reale scoperto scrivendo il test) ed esclusi
      esplicitamente: rimandati al punto dedicato più sotto.
      **Scoperta per strada**: `CellStyle::fFormat` non era mai stato
      persistito nel formato nativo, da nessun punto del codice
      (nemmeno dal menu Formato già esistente prima di questa fase) —
      aggiunta la sezione mancante in `ui/src/AscdIO.cpp` e nella
      copia duplicata di `XlsxTranslator.cpp`, sullo stesso modello di
      font/allineamento/bordi (Fase 10/11). Test:
      `translators/xlsx/tests/test_xlsx_translator.cpp` (nuova
      fixture `sample_numfmt.xlsx`, formati incorporati e
      personalizzati) e `ui/tests/test_persistence.cpp` (round-trip).
- [ ] **Grassetto/corsivo**: leggere `<b/>`/`<i/>` dentro ogni
      `<font>` di `styles.xml` (oggi `StylesContext` cattura solo il
      colore del font) e scegliere lo stile del font già supportato
      da `gFontSizeTable`/`CellStyle::fFont` (stessa tripla famiglia/
      stile/dimensione usata dall'export nativo in Fase 10).
- [ ] **Allineamento orizzontale**: leggere `<alignment
      horizontal=".../>` dentro ogni `<xf>` di `cellXfs` e mappare sui
      valori già supportati da `CellStyle::fAlignment`/`EAlignment`.
- [ ] **Bordi da stile**: risolvere l'indice `borderId` di ogni `<xf>`
      contro `<borders>` in `styles.xml` e tradurlo nei quattro campi
      booleani per lato già definiti in Fase 11 (bordo presente/
      assente per lato, indipendentemente da spessore/colore reale
      dell'originale — stesso limite dichiarato per il significato dei
      campi in Fase 11).
- [ ] **Sottolineato**: nessuna infrastruttura esistente (verificato:
      Haiku `BFont` non ha un attributo sottolineato nativo, solo
      stile del font). Nuovo campo booleano in `CellStyle` (stesso
      pattern dei quattro campi bordo di Fase 11) disegnato a mano in
      `SheetView::DrawCellBand` (una linea sotto il testo), letto da
      `<u/>` nel font XLSX, persistito nel formato nativo con lo
      stesso principio delle sezioni opzionali già esistenti.
- [ ] **Testo a capo e altezza di riga automatica**: nessuna
      infrastruttura esistente, il disegno del testo è oggi sempre su
      una riga sola. Nuovo campo `wrapText` in `CellStyle`, a-capo del
      testo per larghezza di colonna in `SheetView`, e ricalcolo
      dell'altezza di riga quando supera quella di default (si
      appoggia alle altezze di riga per-riga già persistite in Fase
      10, non serve un nuovo meccanismo di persistenza, solo il
      calcolo all'importazione/modifica).
- [ ] **Celle unite**: nessuna infrastruttura esistente in motore o
      UI, verificato anche nel Sum-It storico. Serve un nuovo concetto
      a livello di `CContainer` (elenco di rettangoli uniti per
      foglio, non un campo per-cella: una cella unita è un'unica
      entità logica che occupa più coordinate), `SheetView` che non
      ridisegni la griglia interna né ripeta il contenuto dentro
      l'intervallo, persistenza nel formato nativo, e lettura di
      `<mergeCells>` dall'XLSX. Il pezzo più grande della fase insieme
      alla formattazione condizionale.
- [ ] **Tabelle strutturate (bande alternate)**: **approssimate come
      colori di sfondo statici all'importazione**, non come un vero
      oggetto tabella (l'XLSX reale referenzia `TableStyleMedium2` con
      `showRowStripes="1"` in `xl/tables/table*.xml`): risolvere il
      colore di banda per riga pari/dispari e scriverlo come normale
      colore di sfondo per cella, riusando l'infrastruttura colori già
      esistente da Fase 7. Scelta deliberata per restare nello scope
      di "importazione fedele", non "editor di tabelle Excel vive"
      (niente filtro automatico, niente riga totali ricalcolata).
- [ ] **Formattazione condizionale**: **valutata una tantum
      all'importazione e congelata come colore statico**, stesso
      principio delle tabelle sopra — non un motore di regole vive
      che si aggiornano al ricalcolo (richiederebbe uno storage
      per-range delle regole e una valutazione ad ogni `CalcCell`,
      un'estensione del motore molto più grande di tutto il resto
      della fase insieme). L'XLSX reale usa `cellIs`/`duplicateValues`
      contro un `dxfId` (formato differenziale): risolvere il colore
      del `dxf` referenziato e, se la regola è già vera per il valore
      importato, applicarlo come `fLowColor`/`fHighColor` normale.
      Limite dichiarato: il colore non si aggiorna più se il valore
      della cella cambia dopo l'importazione.
- [ ] **Immagini incorporate**: leggere `xl/drawings/`+`xl/media/`
      (un logo nel file reale), ancorarle a un intervallo di celle e
      disegnarle in `SheetView` (o una `BView` figlia posizionata
      sopra il foglio). Nessuna infrastruttura esistente per bitmap
      nel motore o nella UI: nuovo concetto, probabilmente l'ultimo
      punto della fase per complessità.
- [ ] Test dedicato per ciascun punto in
      `translators/xlsx/tests/test_xlsx_translator.cpp` (lettura) ed
      eventualmente `ui/tests/` per il disegno (celle unite, testo a
      capo, sottolineato), sul modello dei test già esistenti per
      colori/larghezza colonna.

---

Ogni fase, a completamento, aggiorna questo file (checkbox + eventuale
nuova sotto-fase emersa) e `docs/PORTING_NOTES.md`/`docs/ENGINE_API.md`
pertinenti, prima di iniziare la fase successiva.
