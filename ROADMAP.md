# Roadmap — foglio di calcolo nativo per Haiku OS

Stato: **Fase 1 e Fase 2 chiuse**; **Fase 3 in corso** (translator CSV
e translator XLS legacy completati, mancano XLSX e ODS). Aggiornato ad
ogni fase completata.

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

## Fase 3 — Translator Kit: import/export XLSX/ODS/CSV/XLS (IN CORSO)

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
- [ ] Translator XLSX: basato su OpenXLSX (BSD-3, C++17, dipendenze
      leggere PugiXML+Zippy/libzip) — valutare porting su Haiku
- [ ] Translator ODS: valutare liborcus (Document Liberation Project,
      storicamente portabile su Haiku out-of-the-box via POSIX) o
      implementazione custom minimale
- [x] Ogni translator dichiara `Identify()`/`Translate()`/
      `InputFormats()`/`OutputFormats()` secondo il framework
      `BTranslatorRoster` (pattern stabilito con il translator CSV,
      riusato per i successivi)

**Test di congruità/compatibilità**: round-trip per ogni formato
(esporta un documento di test, reimportalo, verifica che i dati
coincidano); import di file reali generati da Excel e da LibreOffice
Calc (non solo generati dal nostro export, per verificare vera
interoperabilità) — non ancora fatto, richiede file di test reali.

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

## Fase 4 — UI nativa Interface/Layout Kit

Obiettivo: applicazione con griglia celle, editing, formattazione,
grafici base, scritta da zero (non riusando `CellView`/`CellWindow`
BeOS-era), che usa l'engine di Fase 2 e i translator di Fase 3.

- [ ] Finestra principale, griglia celle (`BGridLayout`/vista custom)
- [ ] Editing in-cella, barra formule
- [ ] Menu/toolbar, dialoghi (trova/sostituisci, formattazione, ecc.)
- [ ] Locale Kit: formattazione numeri/valute/date locale-aware
- [ ] Print Kit: stampa/anteprima
- [ ] Icone: autorizzato l'uso del portale www.hvif-store.art (formato
      HVIF nativo Haiku) come fonte per le icone dell'applicazione

**Test di congruità/compatibilità**: test manuale interattivo (apri,
modifica, salva, ristampa un foglio); nessuna regressione nei test di
Fase 2/3 lanciati contro l'engine sottostante.

## Fase 5 — Integrazione, packaging, compatibilità reale

- [ ] Ricetta pacchetto per HaikuDepot
- [ ] Test di compatibilità con corpus di file reali: Excel (xls/xlsx
      di varie versioni), LibreOffice Calc (ods), OpenOffice legacy
- [ ] Verifica licenze (codice storico Sum-It: BSD 4 clausole con
      advertising clause — va rispettata la clausola pubblicitaria in
      ogni distribuzione binaria che includa quel codice)

## Fase 6 — Polish e funzionalità avanzate

- [ ] Grafici, tabelle pivot base, funzioni aggiuntive
- [ ] Ottimizzazione ricalcolo su fogli grandi
- [ ] Documentazione utente

---

Ogni fase, a completamento, aggiorna questo file (checkbox + eventuale
nuova sotto-fase emersa) e `docs/PORTING_NOTES.md`/`docs/ENGINE_API.md`
pertinenti, prima di iniziare la fase successiva.
