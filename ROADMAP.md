# Roadmap — foglio di calcolo nativo per Haiku OS

Stato: **Fase 1, Fase 2, Fase 3 e Fase 4 chiuse**. Fase 4 (UI nativa):
finestra principale, griglia, apertura file via Translation Kit, barra
formule, editing in-cella, menu Modifica (taglia/copia/incolla/
cancella/trova e sostituisci), menu Formato (Generale/Numero/Valuta/
Percentuale), toolbar, Locale Kit, Print Kit, icona applicazione
(HVIF, disegnata da zero) ed export CSV — tutti fatti e testati dal
vivo in una sessione grafica reale. **Fase 5 (packaging/compatibilità)
chiusa** — ricetta HaikuDepot pronta ma non ancora buildabile (il
repository è ormai pubblico su GitHub, manca solo un tag di release
versionato con checksum reale); licenza **MIT** decisa per il codice
nuovo (vedi LICENSE — il codice storico Sum-It/`engine/` resta sotto
la sua licenza BSD originale); export ODS e XLSX aggiunti (oltre a
CSV) — export XLS legacy escluso deliberatamente (nessun writer
BIFF/OLE2 su cui appoggiarsi, XLSX copre già l'export verso
l'ecosistema Excel); test di compatibilità completato su un corpus di
11 file di lavoro reali dell'utente (mai inclusi nel repository), oltre
al catalogo di icone HVIF ora collegato per intero alla toolbar
(formattazione testo compresa) col relativo troppopieno gestito quando
la finestra si restringe. Nello stesso giro di lavoro, usando l'app dal
vivo, l'utente ha segnalato e sono stati corretti quattro bug: il
grassetto su una sola cella che si propagava a tutto il foglio (font
di default condiviso per errore), la tinta della selezione
multi-cella non trasparente, l'impossibilità di selezionare
un'intera riga/colonna dall'intestazione, e un crash reale ("Looper
must be locked") aprendo Colore sfondo/testo con la finestra già
aperta — quest'ultimo riprodotto dall'utente stesso con un vero
rapporto di crash di Haiku. **Fase 6 chiusa**: guida utente, funzioni
con
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
**Fase 12 (fedeltà visiva import XLSX) chiusa**: aprire un file Excel
reale e complesso (bordi, celle unite, formati numero, formati
data/ora, tabelle, formattazione condizionale, immagini incorporate)
ora somiglia a quello che si vede aprendolo con Excel vero su Windows,
non solo valori e colori grezzi come prima della fase — richiesto
esplicitamente dall'utente dopo aver riaperto il file di gara reale da
38 fogli e trovato la resa "ancora carente". **Fase 5 chiusa** (tutti i
punti pianificati completati). **Fase 13 (colmare il divario con
Excel) in corso**: elenco delle funzionalità mancanti rispetto a Excel,
compilato leggendo il codice reale e ordinato per difficoltà di
implementazione crescente, non per importanza. Aggiornato ad ogni fase
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
- [x] Test end-to-end del doppio click/digitazione diretta: chiuso in
      `ui/tests/test_real_input_edit.cpp` con una terza tecnica,
      diversa sia dalla chiamata diretta a `MouseDown()`/`KeyDown()`
      (usata altrove, vedi `test_editing.cpp`) sia da uno strumento di
      iniezione esterno come `hey`/Pippo (scartati per i problemi di
      affidabilità/targeting cross-finestra già documentati): un vero
      `BMessage(B_MOUSE_DOWN)`/`BMessage(B_KEY_DOWN)`, con gli stessi
      campi che metterebbe l'app_server, consegnato con
      `BMessenger::SendMessage()` (sincrono) a `SheetView` — resta
      in-process ma passa comunque dal vero ciclo di dispatch di
      `BView::DispatchMessage()`, la stessa strada che userebbe un
      evento reale. Il campo delle coordinate in vista locale è
      `"be:view_where"` (non `"where"`, che restava a (0,0); verificato
      empiricamente con un piccolo programma di prova prima di
      scrivere il test vero). Verifica sia il doppio click (apre
      l'editor sul contenuto esistente della cella, senza sostituirlo)
      sia la digitazione diretta (apre l'editor col solo carattere
      digitato, sostituendo il contenuto) — comportamento diverso fra
      i due, entrambi confermati.
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

## Fase 5 — Integrazione, packaging, compatibilità reale (CHIUSA)

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
- [x] Test di compatibilità con corpus di file reali: oltre al primo
      file XLS scaricato per un test estemporaneo e ai file di lavoro
      usati in precedenza (insieme: dieci bug reali corretti nel
      filtro XLS legacy, vedi le sezioni "Bug scoperto" più sotto e
      `docs/TRANSLATORS.md`), l'utente ha fornito una cartella di 11
      file reali propri (2 `.xls`, 8 `.xlsx`, 1 `.xlsm`: fatture,
      budget, documenti tecnici UNI/EN, un file di clustering da
      3,4MB, un campione finanziario pubblico) — **mai inclusi nel
      repository** (file di lavoro personali, licenza di
      ridistribuzione non chiara, per stessa richiesta esplicita
      dell'utente), usati solo localmente per il test. Tutti e 11 si
      convertono in ASCD senza crash tramite `translate` (il tool a
      riga di comando del Translation Kit, con i translator già
      installati come add-on di sistema); i due aperti anche dal vivo
      nell'app vera (il campione finanziario pubblico e il file `.xls`
      con un nome definito che referenzia una funzione sconosciuta,
      vedi sotto) mostrano i dati correttamente. Nessun nuovo bug di
      importazione dati trovato in questo giro. Due osservazioni per
      il futuro (non risolte qui):
      il file da 3,4MB impiega ~94s a importarsi (in linea con quanto
      già misurato per il ricalcolo su fogli grandi, vedi la nota più
      sotto sul ricalcolo a grafo delle dipendenze); e un difetto di
      riconoscimento MIME a livello di sistema (non di Atomo123) su
      uno dei file `.xls` — vedi la nota dedicata più sotto, dove un
      presunto "crash intermittente" si è rivelato essere solo il
      dialogo di Tracker per un tipo di file senza applicazione
      preferita. Resta comunque un vero corpus eterogeneo generato da
      installazioni reali di Excel/LibreOffice in condizioni non
      controllate diverso da questo (file donati da altri utenti, o
      un ambiente con Excel/LibreOffice/OpenOffice reali per generarne
      di nuovi — non disponibili su questo sistema). Fixture
      automatiche sintetiche (mai file utente): `translators/xls/tests/`
      per encoding CP1252/UTF-16, date con formato personalizzato,
      riferimenti a cella BIFF8 in una formula, font/allineamento/testo
      a capo, larghezza colonna e colore di sfondo/testo/bordi;
      `translators/xlsx/tests/` per allineamento, bordi, formattazione
      condizionale, date, stile font, immagini, celle unite, formato
      numero, tabelle, sottolineato, testo a capo.
- [x] "Apri recenti" nel menu File: gli ultimi 5 file aperti,
      persistiti in `gPrefs` come cinque chiavi `recentFileN` (la più
      recente per prima). Il sottomenu si ricostruisce da
      `MenusBeginning` appena prima di essere mostrato invece che
      quando un file viene aperto: così ogni finestra della cartella
      di lavoro (Fase 4, più finestre possibili) legge lo stato
      condiviso al momento giusto, senza bisogno di notificare le
      altre finestre aperte. Una voce non più disponibile (file
      spostato o cancellato) viene tolta dall'elenco al tentativo di
      riapertura invece di restare lì a fallire ogni volta. Test:
      `ui/tests/test_recent_files.cpp`.
- [x] Finestra "Informazioni su Atomo123", raggiungibile dal menu
      File: stesso stile (banner sfumato blu-ardesia, icona su
      riquadro arrotondato, titolo/versione, link cliccabile al
      progetto) della finestra Informazioni di Brube2000, altro
      progetto nativo Haiku dello stesso autore — `ClickableStringView`
      riportato pari pari da lì per coerenza visiva fra le due app.

### Falso allarme: non era un crash, era il dialogo "nessuna applicazione preferita" di Tracker

Testando il corpus di file reali (vedi sopra) con `open` da riga di
comando, aprire il file `.xls` con il nome definito problematico
come seconda finestra sembrava a volte far sparire Atomo123 dal
process tree — inizialmente scambiato per un crash intermittente.
L'utente ha condiviso uno screenshot del proprio schermo che ha
chiarito subito la causa vera: non era affatto un crash, era il
dialogo standard di Tracker "Non è stato possibile trovare
un'applicazione per aprire ... (There is no preferred application for
this type of file)" — apparso al posto della finestra di Atomo123, e
scambiato per una sua scomparsa perché quel dialogo appartiene a un
altro processo, non a lui.

Causa reale, verificata con `catattr`/`mimeset`: l'attributo
`BEOS:TYPE` di quello specifico file viene sniffato in modo
deterministico come `application/msword` (Word) invece di
`application/vnd.ms-excel` — probabilmente il contenitore OLE2 di
questo file in particolare assomiglia abbastanza a un vecchio `.doc`
da far scattare la regola di riconoscimento generica di sistema per
Word invece di quella per Excel. Atomo123 non dichiara affatto
`application/msword` fra i suoi tipi supportati (`kSupportedTypes` in
`App.cpp`), quindi per quel tipo Tracker non trova nessun'app e mostra
il dialogo invece di aprire silenziosamente Atomo123. Il file si
apre invece correttamente (dati importati bene, vedi sopra) ogni
volta che il translator/l'app lo processano direttamente, ignorando
l'attributo cache-ato — è solo la risoluzione "che app lancio"
dell'OS a essere fuorviata per questo file specifico, non
l'importazione dati stessa. Nessun bug in Atomo123 da correggere qui;
nota lasciata solo per non riprovare a "riprodurre un crash" che non
esiste se questo file rispunta in un test futuro.

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

### Bug scoperto: sei crash/blocchi aggiuntivi nel filtro XLS legacy su file reali

Scoperti aprendo altri file `.xls`/`.xlsx` reali di un utente (2 `.xls`,
7 `.xlsx`), oltre ai tre già corretti sopra:

1. `COLINFO` con colonna "first" > "last": violava l'assert di
   `CRunArray2::SetValue`, che richiede un intervallo ordinato.
2. `MapFunction` con un indice di funzione fuori dall'intervallo noto:
   usciva dalla jump table compilata dallo switch, segfault.
3. `PTG_BASE` che restituisce 62 o 63: `kPtgMap`/`ptgLen` coprono solo
   fino a 61, una lettura fuori tabella poteva far arretrare l'indice
   del parser dei token invece di farlo avanzare, ciclo infinito.
4. `gFuncArrayByNr` usata senza controllare che fosse mai stata
   popolata.
5. `MULRK`/`MULBLANK`: una lunghezza di record corrotta rendeva
   negativo il contatore di `while (i--)`, ciclo infinito.
6. Record `NAME`: la lunghezza del nome letta come `char` con segno
   diventava negativa per nomi "lunghi" (≥128), scrivendo prima
   dell'inizio del buffer sullo stack invece che dopo. Tolto anche un
   alert modale bloccante per funzioni sconosciute nello stesso punto,
   che avrebbe bloccato per sempre un'importazione senza sessione
   interattiva.

Verificato con un harness standalone su tutti i file di lavoro reali
forniti dall'utente: tutti si aprono ora senza crash né blocchi.

### Bug scoperto: mancanza completa di SST/LABELSST (stringhe condivise BIFF8/Excel97+) nel filtro XLS legacy

Il filtro Excel storico (ereditato da Sum-It) leggeva solo le stringhe
dirette in linea di BIFF5 (`LABEL`/`RSTRING`), mai aggiornato per BIFF8:
praticamente ogni file `.xls` reale odierno (Excel 97 in poi) usa
invece la tabella di stringhe condivise (record `SST`) con un
riferimento per cella (`LABELSST`), quindi l'importazione perdeva
silenziosamente quasi tutto il testo — numeri e formule sopravvivevano
perché non passano da SST, il testo no. Scoperto confrontando
visivamente con Excel vero l'importazione di una fattura reale di un
utente.

`CExcel5Filter::ReadSST` (`engine/src/Excel/Excel.pass1.cpp`) legge la
tabella intera durante il primo passaggio, attraversando in modo
trasparente eventuali record `CONTINUE` quando la tabella supera la
dimensione massima di un record BIFF (~8KB, il caso comune sui file
reali) — compresa la stranezza BIFF8 per cui un `CONTINUE` che
interrompe una stringa a metà ricomincia con un proprio byte "grbit"
(compresso/non compresso), che può differire da quello della stringa
originale. `LABELSST` nel secondo passaggio si limita a cercare
l'indice nella tabella già pronta, con un controllo sui limiti per un
indice fuori range (file corrotto) invece di leggere fuori dal vector.

Test: due fixture generate con xlwt (Python, licenza BSD, non un file
utente reale) — `sample_sst.xls` (stringhe semplici, duplicate
deduplicate in una sola voce SST, una stringa lunga) e
`sample_sst_large.xls` (400 stringhe uniche, oltre 12KB, forza
l'attraversamento dei record `CONTINUE`). Verificato anche dal vivo
aprendo entrambi i file nell'app vera, nessun crash né blocco.

### Bug scoperto: il filtro XLS legacy portato a livello di Excel su una fattura reale

Confrontando l'importazione della stessa fattura reale con
l'apertura in Excel vero, screenshot alla mano, sono
emersi altri bug reali nel filtro BIFF legacy, oltre a quello SST già
descritto sopra — lo stesso filtro era stato scritto per BIFF5 e mai
davvero aggiornato per i file genuinamente BIFF8 (Excel 97-2003) che
gli vengono dati in pasto oggi:

- **Encoding testo**: le stringhe "compresse" (1 byte/carattere) del
  BIFF venivano lette come Latin-1 invece di Windows-1252 — differiscono
  nell'intervallo 0x80-0x9F (virgolette tipografiche, trattini
  en/em-dash, il simbolo €). Anche il nome del `FONT` veniva letto col
  vecchio formato "compressed string" di BIFF5 invece della
  `ShortXLUnicodeString` vera di BIFF8, producendo nomi font
  "spazzatura" che non corrispondevano a nessun font installato — la
  causa reale per cui grassetto/corsivo sparivano del tutto (vedi il
  punto successivo su `CFontMetrics`).
- **Formati numero**: `ifmt` era trattato come indice sequenziale
  invece che identificativo assoluto (built-in o personalizzato), senza
  nessuna tabella dei formati built-in precaricata, e il record
  `FORMAT` veniva letto col vecchio formato compresso invece della
  stringa Unicode BIFF8 vera — una cella con formato "00000" per gli
  zeri iniziali, o una data con formato personalizzato, tornava al
  formato generico.
- **Date**: nessun rilevamento delle celle data — mostravano il
  seriale Excel grezzo (es. "43241") invece della data formattata,
  stesso bug già risolto per l'import XLSX in Fase 12.
- **`CExcelStream::operator>>(long&)`**: leggeva `sizeof(long)` — 8
  byte su Haiku x86_64 (LP64) — invece dei 4 byte fissi di un campo
  BIFF "long", corrompendo silenziosamente la seconda cella in poi di
  ogni record `MULRK` (più celle numeriche compresse in un solo
  record, il caso comune per righe di numeri consecutivi in un
  foglio reale).
- **`ptgRef`/`ptgArea`/`ptgRefN`/`ptgAreaN`** (`Excel.formula.cpp`):
  usavano ancora la struttura di riferimento a cella di BIFF5 (riga a
  2 byte coi flag di relatività, colonna a 1 byte) invece di quella di
  BIFF8 (riga a 2 byte piatta, colonna a 2 byte coi flag) — causava un
  loop infinito nell'app aprendo la fattura reale (una formula con un
  riferimento a cella, il caso più comune in assoluto). Aggiunta anche
  una rete di sicurezza in `ParseXLFormula` che garantisce comunque un
  avanzamento minimo dell'indice per qualunque token futuro non ancora
  scoperto, indipendentemente dalla causa.
- **`CFontMetrics`** (`engine/src/Cell/FontMetrics.cpp`, condiviso con
  l'import XLSX): il fallback per una famiglia/stile di font non
  installati controllava la stringa letterale `"<unknown family>"` di
  BeOS R5 — Haiku non la produce mai, sostituisce famiglia e stile di
  sistema in silenzio in un solo passo. Il fallback vero (che prova
  prima famiglia di sistema + stile richiesto, poi famiglia e stile di
  sistema) non scattava quindi mai, perdendo lo stile richiesto ogni
  volta che il font del file non era installato. Riscritto sullo
  `status_t` di ritorno di `SetFamilyAndStyle`, l'unico segnale
  affidabile su Haiku.
- **`WriteASCD`** (`translators/xls/XlsTranslator.cpp`) scriveva
  sempre sezioni vuote per font/formato/allineamento/testo a capo/
  larghezza colonna/colore sfondo-testo/bordi, anche quando il motore
  li aveva già risolti correttamente durante il parsing BIFF: un
  titolo in grassetto, una colonna ridimensionata a mano o
  un'intestazione con sfondo colorato sparivano del tutto aprendo il
  file con Atomo123. Il record XF ora viene letto per intero (prima ci
  si fermava agli 8 byte iniziali di font/formato/allineamento): stile
  del bordo per lato — solo presenza, `CellStyle::fTBorderColor` e
  affini sono booleani nonostante il nome, l'app disegna sempre un
  bordo nero pieno o nessuno, mai un colore/spessore reale (Fase 11) —
  e pattern di riempimento risolto sulla tabella colori standard di
  Excel già usata per il colore del font (`kExcelColorTable`,
  `Excel.colors.h`).
- Un'eccezione durante il ricalcolo di una formula (es. una formula
  che referenzia un nome definito che il motore non riesce a
  risolvere, un caso reale nella fattura di prova) sovrascriveva la
  cella con la stringa letterale `"!ERROR"`, scartando il valore già
  letto dalla cache BIFF del file — lo stesso valore che Excel stesso
  mostra (in questo caso vuoto). La cella ora mantiene il valore già
  impostato dalla cache invece di essere sovrascritta.

Test: una fixture xlwt dedicata per ciascun bug in
`translators/xls/tests/` (vedi sopra), verificate anche dal vivo
riaprendo ripetutamente la fattura reale nell'app vera e confrontando
screenshot alla mano con Excel — comprese le larghezze di colonna, gli
sfondi colorati e i bordi della tabella riepilogativa, prima
completamente assenti. Le immagini incorporate (record
`MSODRAWING`/Escher) e le celle unite (record `MERGEDCELLS`) sono
state affrontate subito dopo, vedi le sezioni dedicate più sotto.

### Bug scoperto: immagini incorporate del filtro XLS legacy (Escher/MSODRAWING), e due bug indipendenti scoperti verificandole dal vivo

Ultimo limite dichiarato rimasto sul confronto diretto con Excel vero
della stessa fattura reale (logo aziendale incorporato): il formato
binario Escher usato da BIFF per le immagini (la controparte di
`xl/drawings/+xl/media/` di XLSX, già importato in Fase 12) non era
mai stato letto — struttura verificata byte per byte contro il file
reale prima di scrivere il parser (`engine/src/Excel/Excel.escher.cpp`,
vedi il commento di intestazione per il dettaglio completo), non
dedotta a memoria dalla sola specifica MS-ODRAW. `MSODRAWINGGROUP` (un
record per l'intera cartella di lavoro, quasi sempre esteso su uno o
più record `CONTINUE` anche per una sola immagine di poche decine di
KB) contiene il "blip store" con i byte grezzi di ogni immagine
(JPEG/PNG, così come nel file originale); `MSODRAWING` (per foglio)
contiene l'ancoraggio riga/colonna/scarto e un indice nel blip store
per ciascuna forma — una forma senza quell'indice non è un'immagine
(le caselle di spunta della stessa fattura usano un record diverso, si
escludono da sole senza bisogno di controllare il tipo di forma). Solo
JPEG/PNG supportati, gli unici osservati finora su un file reale.

Verificando dal vivo che il logo si disegnasse davvero nell'app (non
solo che i byte estratti fossero un JPEG valido, già confermato a
livello di translator) sono emersi altri due bug reali, indipendenti
da Escher in senso stretto:

- **`CCsvTranslator::Identify`** (`translators/csv/CsvTranslator.cpp`)
  accettava incondizionatamente qualunque contenuto come "forse CSV"
  (qualità 0.6 di ripiego, dato che il CSV non ha una firma propria) —
  un JPEG veniva quindi "riconosciuto" come CSV con priorità
  sufficiente a battere `JPEGTranslator` in
  `BTranslatorRoster::Translate()` quando la UI chiedeva un bitmap in
  uscita, che poi falliva con "No translator found" perché
  `CsvTranslator` non sa produrre bitmap. Bug di sistema, non solo di
  XLS: qualunque altro punto dell'app (o di un'altra app) che si
  affidi al riconoscimento automatico del Translation Kit per
  un'immagine ne era silenziosamente affetto. Corretto respingendo un
  contenuto con un byte NUL o troppi caratteri di controllo nel
  campione iniziale, invece di accettarlo sempre.
- **`ReadSingleSheetASCD`** (`ui/src/MainWindow.cpp`, il percorso di
  apertura per qualunque file non-cartella di lavoro: CSV, XLS, ODS, e
  XLSX a un solo foglio) passava a `LoadASCD` solo i primi tre
  argomenti opzionali, ignorando altezza di riga/blocca riquadri/
  immagini incorporate (aggiunti a `LoadASCD` in Fase 10/12, mai
  riportati qui): la sezione immagini veniva letta correttamente e poi
  scartata subito, mai passata alla `SheetView`. Lo stesso problema
  riguardava silenziosamente anche un file XLSX a un solo foglio con
  un'immagine incorporata — non un caso ipotetico, semplicemente mai
  notato prima. `LoadASCDBook` (cartelle di lavoro multi-foglio) passa
  già tutti gli argomenti correttamente, da cui l'asimmetria.

Test: `tests/sample_image.xls` non è generato con xlwt (non sa
scrivere immagini) ma con uno script Python dedicato che inserisce a
mano gli stessi record Escher trovati nel file reale, con un piccolo
JPEG rosso sintetico al posto del logo vero — verifica l'ancoraggio e
la validità dei byte estratti. Due nuovi test in
`translators/csv/tests/test_csv_translator.cpp` per il rifiuto di
un'intestazione JPEG e di un byte NUL. Verificato anche dal vivo: il
logo della fattura reale si disegna correttamente nell'app, in
posizione coerente con Excel.

**Correzione successiva sulla dimensione**: la prima versione usava la
dimensione nativa del JPEG/PNG (quella salvata nel file) come
dimensione visualizzata — quasi mai corretta, dato che l'immagine può
essere stata ridimensionata a mano in Excel dopo l'inserimento, e il
suo DPI nativo non ha comunque alcun rapporto con la larghezza di
colonna/altezza di riga del foglio. Il logo della fattura reale
appariva quindi molto più grande del riquadro che Excel disegna
davvero. Corretto calcolando la dimensione dal rettangolo di
ancoraggio Escher (`ClientAnchor`, ora letto per intero — prima solo
l'angolo in alto a sinistra) sommando le colonne/righe vere
attraversate, con le larghezze di colonna reali del file dove
disponibili. La dimensione nativa resta un fallback solo per un
rettangolo di ancoraggio degenere.

### Bug scoperto: formato numero "0" ripetuto (riempimento di zeri a sinistra) perso in due punti diversi

Ultimo confronto diretto con Excel vero sulla stessa fattura reale:
"Preavviso N. 00073" veniva mostrato come "73", nonostante il record
FORMAT del file dichiari correttamente il template `"00000"` per
quella cella. Due bug indipendenti, in due file diversi, che si
sommavano l'uno all'altro — trovati tracciando passo passo l'intero
percorso (parsing BIFF → `CFormatter` → disegno) con tracce mirate,
dato che ciascuno singolarmente avrebbe comunque prodotto lo stesso
sintomo visibile:

- **`CFormatter::ParseTemplate`** (`engine/src/Cell/Formatter.template.cpp`,
  ereditato da Sum-It) riconosceva solo `"$"`/`"%"`/`"."` per
  distinguere valuta/percentuale/cifre decimali: un template fatto
  solo di `'0'`/`'#'` finiva silenziosamente nel formato generico,
  perdendo il riempimento. Aggiunto `ENumberFormat::eZeroPad`
  (`Formatter.h`): resta sotto `eFirstNewFormat` (1024) cosi' l'ID
  risultante e' portabile fra sessioni come i formati "vecchio stile"
  esistenti, riusando "fDigits" per la larghezza totale del
  riempimento invece delle cifre decimali. `FormatDouble`
  (`Formatter.number.cpp`) arrotonda a intero e riempie di zeri a
  sinistra.
- **`SheetView::Draw`** (`ui/src/SheetView.cpp`) sostituiva SEMPRE il
  testo gia' calcolato dal motore per una cella numerica pura con una
  riformattazione locale-aware (separatore migliaia/decimale secondo
  le preferenze di sistema), tranne per valuta/percentuale — corretto
  per General/Fisso (dove il raggruppamento locale ha senso), ma
  scartava anche un testo gia' ESATTO come "00073" per sostituirlo col
  numero grezzo. Bug pre-esistente, indipendente dal filtro XLS: si
  sarebbe manifestato per QUALUNQUE cella con un formato del genere,
  anche impostata a mano nell'app o importata da un altro formato.
  Corretto saltando la riformattazione quando il tipo di formato
  (isolato da `cs.fFormat & 0x000F`) e' `eZeroPad`.

Con solo il primo bug corretto, la cella mostrava ancora "73": e'
stata la traccia diretta in `CContainer::GetCellResult` (temporanea,
rimossa prima del commit) a confermare che il motore calcolava gia'
correttamente "00073" — la sostituzione avveniva un livello piu' sopra,
nel disegno.

Test: nuova fixture `translators/xls/tests/sample_zeropad.xls` (xlwt)
con un valore normale (73 → "00073"), zero (0 → "00000", non una
stringa vuota) e un valore che supera la larghezza del template
(123456 → resta per esteso, non troncato) — verifica sia che il
`fFormat` risolto non sia quello generico sia il testo reso da
`CFormatter` direttamente. Verificato anche dal vivo: "Preavviso N."
mostra ora "00073" nell'app, come in Excel.

### Bug scoperto: il font non veniva mai applicato davvero al disegno (dimensione/grassetto/corsivo/famiglia)

Continuando il confronto con Excel vero sulla stessa fattura reale:
l'intestazione dello studio (20pt nel file originale) appariva alla
stessa dimensione del testo normale circostante.
Tracciando passo passo l'intera catena (translator -> `LoadASCD` ->
`SheetView::Draw`, con tracce temporanee poi rimosse) il font
risultava risolto e registrato correttamente in `gFontSizeTable` a
OGNI passaggio — il problema era un livello più sotto:
`CFontMetrics::SetFontSizeColor` (`engine/src/Cell/FontMetrics.cpp`)
era un no-op totale, mai una vera chiamata a `BView::SetFont`. Il
commento originale ("il disegno è compito della UI, non del motore")
era vero solo a metà: `SheetView::Draw` chiama proprio
`gFontSizeTable.SetFontID(this, cs.fFont)` per applicare il font alla
view PRIMA di disegnare — ma senza quella chiamata mancante, il font
della view non cambiava mai, a prescindere da `CellStyle::fFont`: ogni
cella disegnava sempre con l'ultimo font impostato esplicitamente (o
quello di sistema).

Questo NON è un bug specifico dell'import XLS: riguarda ogni cella
dell'app, in ogni percorso (editing nativo, import XLSX/ODS/CSV, XLS).
Le verifiche precedenti su grassetto/corsivo (Fase 5, sezione sui bug
XLS più sopra) controllavano solo il DATO esportato (es. il campo
stile della sezione font di ASCD, o la stringa "Bold" in un test),
mai il rendering pixel per pixel — motivo per cui è rimasto invisibile
finché non si è confrontato lo screenshot di Atomo123 con quello di
Excel a occhio nudo.

`BView::SetFont` non richiede una connessione app_server "viva" per il
solo aggiornamento locale dello stato (serve solo per un ridisegno
effettivo, già gestito altrove dal ciclo di `Draw`), e `libengine.a`
collega già `-lbe` ovunque (motore, translator, UI) — nessun nuovo
vincolo di link introdotto. Verificato dal vivo: l'intestazione e le
righe in grassetto della fattura reale mostrano ora la dimensione e lo
stile corretti.

**Nota**: durante questa verifica è emerso anche un crash raro e non
riproducibile in modo affidabile (General Protection Fault dentro
`memcmp`, chiamato da qualche parte nella catena di importazione XLS,
osservato una sola volta su una dozzina di aperture dello stesso file,
sia in modalità headless sia nell'app vera — non riprodotto in 150
traduzioni consecutive in un harness isolato). Non ancora
diagnosticato con certezza per mancanza di strumenti adeguati in
questo ambiente (AddressSanitizer non funzionante su questa build di
Haiku, nessun gdb disponibile) — probabilmente una corruzione di
memoria pre-esistente e rara, non necessariamente legata alle modifiche
di questa sessione. Segnalato qui per trasparenza, da investigare con
strumenti migliori se si ripresenta.

### Bug scoperto: celle unite ignorate dal filtro XLS legacy (record MERGEDCELLS)

Ultimo limite dichiarato rimasto sul confronto diretto con Excel vero
della stessa fattura reale: l'intestazione "Corrispettivi"/"Spese
escluse ex art. 15 DPR 633/72" è unita su due righe nel file originale
(centrata verticalmente in Excel), ma il filtro BIFF legacy ignorava
del tutto il record `MERGEDCELLS` (0x0E5) — la stessa intestazione
appariva in Atomo123 come due righe separate, con testo troncato in
alto invece che centrato sull'intero blocco.

`CExcel5Filter::HandleXLRecordForPass1` (`Excel.pass1.cpp`) legge ora
`n` quadruple `(rowFirst,rowLast,colFirst,colLast)`, tutte 0-based
come il resto del BIFF, convertite a 1-based (la stessa convenzione
usata per ogni altra cella) e registrate in un nuovo elenco esposto da
`CExcel5Filter::GetMergedRanges()`. `CXlsTranslator::Translate` lo
recupera e lo passa a `WriteASCD`, che scrive la sezione "merge" del
formato nativo (già esistente, usata finora solo dall'import XLSX di
Fase 12) con i valori reali invece del placeholder a zero usato finora.

Test: nuova fixture dedicata `sample_merge.xls` generata con xlwt
(`sheet.write_merge`, licenza BSD, non un file utente reale) — due
celle unite (orizzontale e verticale) più una cella di controllo non
unita. Verificato anche dal vivo riaprendo la fattura reale:
l'intestazione "Corrispettivi"/"Spese escluse" appare ora come
un'unica cella, senza la riga di separazione interna.

**Nota**: durante lo sviluppo di questa fixture la suite di test di
`translators/xls` si è bloccata una volta, richiedendo `kill -9`.
Un'indagine approfondita con tracce di debug temporanee (poi rimosse)
su ogni record BIFF letto in Pass1/Pass2 non ha individuato alcun
punto deterministico del blocco — le esecuzioni successive, identiche,
sono sempre state pulite (confermato più volte di fila, sia con le
tracce attive sia rimosse). Stessa categoria del crash raro descritto
sopra: probabile instabilità ambientale di questa sandbox, non un bug
nella logica di MERGEDCELLS. Segnalato qui per trasparenza, non
bloccante.

### Bug scoperto: il testo che trabocca sulla colonna vicina veniva sempre troncato

Ultimo punto del confronto diretto con Excel vero sulla stessa fattura
reale: l'intestazione dello studio (e diverse altre righe sotto, tutte
più larghe della colonna A) appariva troncata al bordo della colonna
in Atomo123, mentre Excel
la mostra per intero, traboccando visivamente sulle colonne B/C/…
vuote a destra. `SheetView::DrawCellBand` limitava sempre il
rettangolo di disegno e di ritaglio (`ConstrainClippingRegion`) alla
sola `CellRect(c)` della cella, indipendentemente dal contenuto delle
celle vicine — comportamento non specifico dell'import XLS, riguarda
ogni cella dell'app con testo più largo della propria colonna.

Nuovo metodo `SheetView::ExpandOverflowRect`: parte da `CellRect(c)` e
cammina sulle celle vicine nella riga, nella direzione dettata
dall'allineamento (generale/sinistra verso destra, destra verso
sinistra, centrato su entrambi i lati insieme), finché il testo ci
entra oppure si incontra una cella non vuota, un intervallo già unito
(Fase 12) o il bordo del foglio (`kColCount`) — gli stessi tre casi
che bloccano il trabocco anche in Excel. Non si applica alle celle
unite (hanno già il proprio rettangolo esteso) né al testo a capo
automatico (va già a capo dentro la colonna, Fase 12).

Pubblico apposta per essere testabile direttamente, stesso principio
già usato per `CellRect`/`CopySelection`: nuovo `tests/test_overflow.cpp`
(nuovo target `make test-overflow`, 11 verifiche) passa una larghezza
di testo scelta a mano invece di misurarla con `StringWidth`, così il
test resta deterministico e indipendente dal font di sistema — copre
tutti e tre i casi di blocco (cella occupata, intervallo unito, bordo
del foglio) oltre ai tre allineamenti. Verificato anche dal vivo
riaprendo la fattura reale: l'intestazione e le altre righe più larghe
della colonna A traboccano ora sulle colonne vuote a destra, fermandosi
correttamente alla prima cella colorata non vuota (colonna I),
esattamente come in Excel.

### Bug scoperto: valuta/percentuale senza decimali ne' simbolo per un formato con cifre impacchettate

Ultimo punto del confronto diretto con Excel vero sulla stessa fattura
reale: gli importi in tabella ("€ 450,00", "€ 400,00", …) si
rendevano come numeri puri senza simbolo né decimali ("450", "400").
Due bug distinti nello stesso punto (`SheetView::DrawCellBand`,
riformattazione locale-aware sopra al testo del motore):

1. `cs.fFormat == eCurrency`/`== ePercent` confrontava il campo
   impacchettato (`CFormatter::FormatID()` somma cifre decimali e
   flag virgola sopra l'enum, vedi `Formatter.h`) direttamente con
   l'enum grezzo — funziona solo per un formato senza cifre né
   virgola, praticamente mai nel mondo reale. La correzione al bug
   analogo di `eZeroPad`, un fix precedente nello stesso punto
   esatto del codice, aveva già introdotto il mascheramento
   `& 0x000F` corretto ma non era stata applicata anche qui.
2. `CFormatter::ParseTemplate` (ereditato da Sum-It) riconosce solo
   `$` come simbolo di valuta — un template con `€` (il caso comune
   per una fattura italiana, es. `\€\ ##,###.00`) finiva nel ramo
   Fisso invece che Valuta, a monte del bug 1.

La logica di riformattazione (prima incorporata nel ciclo di disegno)
è stata estratta in un nuovo metodo pubblico
`SheetView::FormattedCellText`, stesso principio di
`CellRect`/`ExpandOverflowRect` sopra: pubblico apposta per essere
testabile direttamente. Nuovo `tests/test_currency_format.cpp` (nuovo
target `make test-currency-format`, 6 verifiche) usa un formato
impacchettato a mano (cifre/virgola) per riprodurre esattamente il
caso reale, verificando solo la STRUTTURA del risultato (decimali
forzati, simbolo `%`), non il simbolo di valuta esatto — quello
dipende dalla valuta configurata nel Locale Kit di sistema, non dal
codice dell'app.

**Nota**: in questo ambiente sandbox nessuna lingua/valuta è
configurata nelle preferenze Locale di Haiku (nessun file in
`~/config/settings/locale/`) — `BNumberFormat` mostra quindi il
segnaposto generico "¤" invece di "€" anche dopo il fix. Verificato
che non è un bug dell'app: `BNumberFormat` è costruito senza
override, delega sempre alla configurazione di sistema per design
(coerente con l'approccio locale-aware già documentato sopra) — un
sistema Haiku reale con locale italiana configurata mostrerebbe già
il simbolo corretto. Non corretto forzando "€" a mano nel codice: pur
risolvendo il sintomo in questo ambiente, romperebbe la
localizzazione per qualunque valuta diversa dall'euro su un sistema
configurato altrimenti. Verificato anche dal vivo riaprendo la
fattura reale: gli importi mostrano ora i due decimali
("450,00"/"400,00").

### Bug scoperto: le formule con funzioni con nome non venivano mai calcolate nell'import XLS

L'utente ha segnalato dopo le verifiche precedenti: "non vedo il
simbolo dell'euro e i calcoli non vengono fatti". Il primo punto è la
nota sulla configurazione Locale già scritta sopra; il secondo era un
bug reale e serio, non ancora scoperto nonostante tutte le verifiche
precedenti sulla stessa fattura — perché riguardava specificamente le
formule con FUNZIONI CON NOME (`SUM`, non le formule aritmetiche
semplici come `D37*F37` già verificate). La sezione "TOTALI" della
fattura (righe 37-45, `SUM(G23:G36)` e simili) restava vuota o a
zero invece di mostrare i totali reali.

La Fase 6 aveva già risolto questo stesso problema per l'app
principale (`App::ReadyToRun()` chiama `InitFunctions()` all'avvio) —
ma il translator XLS è un add-on caricato a parte, compilato con la
propria copia STATICA di `libengine.a`: ha quindi una propria istanza
SEPARATA di `gFuncCount`/`gFuncArrayByNr`/`gFuncArrayByName`/`gFuncs`,
mai popolata da nessuno. `CExcel5Filter` ora chiama `InitFunctions()`
da solo se non è ancora stato fatto, puntando al VERO binario in
esecuzione tramite `be_app->GetAppInfo()` (che riflette sempre
Atomo123 anche quando il codice gira dentro l'add-on caricato
in-process).

**Bug collaterale scoperto durante il fix**: la prima versione del
fix ha bloccato l'intera app all'apertura di un file .xls reale
(nessun crash, nessun messaggio — un blocco indefinito). Causa:
`gAppName` (una `BPath` globale usata da `LoadPlugIns()` per cercare
una cartella "Functions/" accanto al binario) non veniva impostata
prima di chiamare `InitFunctions()` — `LoadPlugIns()` chiamava
`strcpy` sul risultato di `gAppName.Path()`, `NULL` per una `BPath`
mai inizializzata in questo processo separato dell'add-on,
comportamento indefinito invece dell'eccezione pulita attesa.
Impostare `gAppName` PRIMA di `InitFunctions()` (stesso ordine di
`App::ReadyToRun()`) risolve anche questo.

**Bug scoperto verificando il fix sulla stessa fattura**: una volta
che `SUM(...)` calcolava davvero, è emerso un secondo bug indipendente
nella stessa catena di totali — l'operatore percentuale postfisso di
Excel (`F37%`, diverso dal formato valuta/percentuale di una cella)
era un case vuoto in `Excel.formula.cpp` (codice BIFF5 mai completato
per BIFF8): `D37*F37%` veniva importato come se il "%" non ci fosse,
100 volte troppo grande. Corretto aggiungendo un vero operatore
(`opPercent`, in fondo a `PFToken` apposta per non spostare gli
ordinali di token già persistiti in documenti esistenti) sia nel
parser BIFF sia nella grammatica nativa (`parser.cpp`, che deve poter
ri-analizzare il testo della formula esportata al caricamento del
formato nativo — riconosceva "%" solo dopo un numero letterale come
"5%", mai dopo un riferimento a cella come "F37%").

Test: nessun test dedicato per la chiamata a `InitFunctions()` nel
translator (richiederebbe una risorsa 'Func' allegata al binario di
test, lo stesso limite già documentato per `GetFunctionNr` nei
translator headless) — verificato dal vivo. Due nuove verifiche in
`engine/tests/smoke_test.cpp` per l'operatore percentuale (`"=5%"` e
`"=A1%"`, quest'ultimo prova che funziona anche dopo un riferimento a
cella, non solo un numero letterale). Verificato anche dal vivo
riaprendo la fattura reale: l'intera catena di totali ora corrisponde
esattamente ai valori che Excel stesso aveva calcolato l'ultima volta
(52,50 → 1.102,50 → 242,55 di IVA → 1.345,05 di totale).

### Bug scoperto: le altezze di riga esplicite non venivano mai importate

L'utente ha segnalato: "l'altezza delle righe non risulta coerente
con l'altezza visualizzata in Excel". Stesso limite già corretto per
le larghezze di colonna (record COLINFO) in una fase precedente, mai
applicato al record equivalente per le righe (`ROW`): scriveva
l'altezza esplicita (in twips) solo sulla vecchia `CCellView`
(`fCellView`, sempre `NULL` in un translator headless — vedi lo stub
in `EngineViewStub.h`), mai in un posto che il translator potesse
rileggere dopo `Translate()`. Ogni riga tornava quindi sempre
all'altezza predefinita (`kRowHeight`, 20px), ignorando qualunque riga
ridimensionata a mano nel file originale — il logo della fattura
reale, che occupa più righe di altezze diverse, appariva quindi
visibilmente sproporzionato rispetto a Excel.

`CExcel5Filter` ora cattura le altezze in un nuovo elenco proprio
(`fRowHeights`, esposto da `GetRowHeights()`, stesso principio di
`fColWidths`/`GetColumnWidths()`), indipendente da `fCellView`.
`h/15.0` converte da twips a pixel assumendo 96 DPI (la stessa
risoluzione già assunta altrove in questo file per le immagini
incorporate). `XlsTranslator` la scrive nella sezione "altezze di
riga" del formato nativo, finora sempre vuota.

Test: nuova fixture `sample_rowheight.xls` (tre righe con altezze
molto diverse) e nuovo blocco in `test_xls_translator.cpp` — ha anche
scoperto e corretto un bug latente nel test stesso (il calcolo dei
byte da saltare per questa sezione assumeva 8 byte per riga invece dei
6 reali, mai emerso finora perché la sezione era sempre vuota, quindi
la dimensione per riga non contava). Verificato anche dal vivo
riaprendo la fattura reale: le righe ora hanno altezze visibilmente
diverse fra loro, non più tutte uguali all'altezza predefinita.

**Nota**: durante lo sviluppo di questo fix, aggiungere un nuovo
membro/metodo a `CExcel5Filter` (`Excel.h`) senza poi rifare un
`make clean` completo dell'engine ha prodotto quello che sembrava un
blocco indefinito del test suite. I report di crash di Haiku
(analizzati in una sessione successiva, salvati sul Desktop
dall'utente) hanno poi confermato che non era affatto un blocco: era
un vero Segment violation dentro `std::map` (`fFormats`, un membro di
`CExcel5Filter`), lo stesso genere di disallineamento ABI fra `.o`
compilati con `sizeof(CExcel5Filter)` diversi già documentato altrove
in questo file per `CellStyle`/`Container` — la shell mostrava solo
"Kill Thread" invece del vero stato dell'eccezione. Risolto con un
`make clean && make` completo. Promemoria per ogni futura modifica a
un header del motore, non solo per questo campo: un crash del genere,
se non diagnosticato con i report di Haiku, è facile da scambiare per
un problema ambientale.

### Bug scoperto: l'a capo automatico non teneva conto delle celle unite

L'utente ha segnalato: "perchè le righe 25 o 26 sono così 'alte' il
file aperto con excel non le fa con queste dimensioni". Le righe delle
descrizioni ("Progettazione impianti schemi meccanici", "Redazione
relazione legger 10/91", ecc.) apparivano alte più di 80px, mentre
Excel le mostra a una riga sola (~20px).

Causa: queste celle sono unite su sei colonne nel file originale (una
descrizione in una colonna A stretta, allargata con l'unione per
darle spazio) e hanno il testo a capo attivo — ma
`SheetView::RecalculateWrappedRowHeights` calcolava la larghezza
disponibile per l'a capo usando SOLO la prima colonna della cella
(`fColWidths[c.h - 1]`), ignorando del tutto le celle unite. Contando
solo la prima, stretta colonna invece dell'intera larghezza unita, il
testo risultava spezzato su molte più righe del necessario.

Stesso principio già applicato a `DrawCellBand` per il disegno del
testo (Fase 12): ora, se la cella in alto a sinistra dell'iterazione
fa parte di un intervallo unito, la larghezza usata per l'a capo è la
somma delle larghezze di TUTTE le colonne dell'intervallo, non solo
la prima.

Test: nuovo blocco in `test_borders.cpp` — tre colonne strette unite
in una sola cella con testo a capo, verifica che la riga resti bassa
invece di gonfiarsi contando solo la prima colonna. Verificato anche
dal vivo riaprendo la fattura reale: le righe delle descrizioni ora
hanno l'altezza normale a una riga sola, come in Excel.

### Bug scoperto: un indice di stile in più per il record MULBLANK (celle vuote formattate)

L'utente ha chiesto di analizzare perché una cella in colonna F non
era colorata correttamente. Confrontando pixel per pixel lo screenshot
della fattura reale in Excel con quello di Atomo123 (e leggendo
direttamente `CellStyle::fLowColor` per ogni cella tramite un harness
dedicato) non è emerso un problema in colonna F, ma un bug reale e
distinto altrove: il record BIFF `MULBLANK` (una riga di celle vuote
ma formattate — es. uno sfondo colorato applicato senza testo, comune
per bande decorative) termina con un campo `colLast` (2 byte) DOPO
l'ultimo indice di stile, oltre a `rw`/`colFirst` (4 byte) prima del
primo. Il calcolo del numero di colonne, `(len-4)/2`, non escludeva
quel campo finale: leggeva un indice di stile IN PIÙ rispetto alle
colonne vuote realmente descritte, applicandolo per sbaglio a una
cella fantasma subito dopo l'intervallo vero — `colLast` veniva
travisato da indice XF.

Il record `MULRK` (celle numeriche compresse) ha la stessa identica
struttura ma non soffriva dello stesso problema per puro caso
aritmetico: le sue voci sono più larghe di `colLast` (6 byte contro
2), quindi la troncatura della divisione intera assorbiva il campo
finale invece di contarlo come voce a sé stante — motivo per cui
questo bug è rimasto nascosto finché non si è guardato `MULBLANK`
specificamente.

Fix: `(len-6)/2`, che esclude correttamente `colLast` dal conteggio.
Test: nuova fixture `sample_mulblank.xls` (sei celle vuote colorate di
giallo più una settima, subito dopo, lasciata bianca come controllo)
in `test_xls_translator.cpp`, verifica che la cella fantasma non
risulti colorata per sbaglio.

### Bug scoperto: diversi colori sbagliati nella tavolozza predefinita di Excel

Durante la stessa indagine era emerso anche un vero disallineamento di
colore (non correlato al bug MULBLANK sopra): una banda decorativa
dell'intera fattura usa un ciano che Excel mostra come RGB
(204,255,255) ma che Atomo123 importava come (159,223,223). Prima
ipotesi (poi esclusa): un record `XFEXT` (0x087D, l'estensione BIFF8
per i colori "veri" scelti con la finestra "Altri colori" di Excel)
non gestito da questo motore. Verificato con la specifica ufficiale
Microsoft [MS-XLS] (campi `FrtHeader`/`ixfe`/`cexts`/`ExtProp`/
`FullColorExt` decodificati byte per byte con un harness dedicato):
lo stile in questione non ha nessuna estensione `XFEXT` associata, e
il file non ha nemmeno un record `PALETTE` (0x0092) che personalizzi
la tavolozza — usa semplicemente il ColorIndex 20 standard di Excel
("Light Turquoise") senza alcuna personalizzazione.

La causa reale era `kExcelColorTable` (Excel.colors.h) stessa:
verificata voce per voce contro due elenchi pubblici indipendenti e
concordanti dei 56 ColorIndex standard di Excel, la tavola ereditata
dal codice storico di Sum-It/Hekkelman Programmatuur aveva diversi
valori sbagliati oltre a quello — fra cui gli indici 13 e 14 scambiati
fra loro (non solo imprecisi, proprio invertiti). Corretti tutti e 56
i valori sulla tavolozza standard verificata.

Test: nuova fixture `sample_colorindex.xls` (una cella con sfondo
"light_turquoise", ColorIndex 20) in `test_xls_translator.cpp`,
verifica il valore RGB esatto. Verificato anche dal vivo riaprendo la
fattura reale: la banda decorativa ora mostra esattamente lo stesso
ciano di Excel, campionato pixel per pixel (204,255,255) su entrambi.

### Bug scoperto: il testo delle formule con risultato testuale non veniva mai letto

Continuando l'indagine sulla stessa fattura reale, la riga 48 (un IBAN
bancario ricostruito con una formula di concatenamento testo, tipo
`=A1&" "&A2`) risultava completamente vuota in Atomo123 mentre Excel la
mostra correttamente.

Causa: nel formato BIFF8, quando una formula ha un risultato testuale
già calcolato e messo in cache da Excel (marcatore `num[3]==0` nel
record FORMULA), il testo vero e proprio non è mai incluso nel record
FORMULA stesso — un record `STRING` (0x0207) separato lo segue sempre
subito dopo. `CExcel5Filter::Pass2()` non leggeva mai questo record
STRING, riusando per sbaglio il valore residuo della cella precedente:
la cella restava vuota (o mostrava il testo sbagliato).

Fix: il caso "risultato testo" ora legge il record STRING immediato
successivo (stessa struttura Unicode di FORMAT/SST: `cch` a 2 byte,
poi un byte `grbit` col flag "wide", poi i caratteri) per ottenere il
vero testo, poi torna alla posizione originale così il resto della
gestione della formula (token bytecode, formule condivise) continua
invariato.

Test: nuova fixture `sample_strformula.xls` (costruita correggendo a
mano il marcatore `num[3]` e aggiungendo il record STRING, dato che
xlwt non calcola mai le formule) in `test_xls_translator.cpp`.
Verificato anche dal vivo riaprendo la fattura reale: la riga 48 ora
mostra correttamente il testo della cella (coordinate bancarie del
beneficiario) invece che restare vuota o mostrare il valore residuo
della cella precedente.

### Bug scoperto: un simbolo percento letterale scambiato per formato percentuale

Continuando l'indagine sulla stessa fattura, la cella F37 ("Diritti
Cassa Professionale: ... x 5% ...") mostrava "500%" invece di "x 5%"
— un errore di grandezza di 100 volte.

Causa: `CFormatter::ParseTemplate` (Formatter.template.cpp) cercava il
carattere `%` con una semplice `strchr()` sul template grezzo per
decidere se applicare il formato percentuale (che moltiplica il
valore per 100 in visualizzazione). Il template reale della cella era
`"x "0\%` — un prefisso di testo letterale ("x ") seguito dal numero e
da un simbolo percento anch'esso letterale (preceduto da un backslash
di escape, non un vero codice di formato). La ricerca grezza non
distingueva questo caso da un vero formato percentuale come `0%`.

Fix: prima di cercare `%` (e allo stesso modo `.` per le cifre
decimali e `,` per il separatore delle migliaia), il template viene
ripulito dal testo letterale fra virgolette e dai caratteri preceduti
da un backslash di escape. Il simbolo di valuta (`$`/`€`) resta
cercato sul template grezzo, dato che compare sempre e solo come
carattere letterale. Con questo fix un formato del genere risulta
indistinguibile da quello generico predefinito del foglio (non c'e'
modo di rappresentare un prefisso/suffisso di testo letterale in
questo formatter): la cella mostra "5" invece di "500%" — non piu'
sbagliato di un fattore 100, anche se non identico a "x 5%".

Test: nuova fixture `sample_percentliteral.xls` in
`test_xls_translator.cpp`, verifica che il "%" letterale non triggeri
piu' il formato percentuale e che un vero formato percentuale ("0%")
continui a funzionare come prima. Verificato anche dal vivo riaprendo
la fattura reale: F37 ora mostra "5" invece di "500%".

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

Con la Fase 8 chiusa, l'utente ha indicato un vero file di lavoro
proprio (un `.xlsm` di una gara d'appalto reale) come banco di prova
per continuare lo sviluppo in autonomia "fino al
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

**Lavoro futuro individuato (non ancora pianificato)**: confrontando
l'importazione XLS della fattura reale usata per la verifica in Fase 5
con Excel vero, il file usa davvero bordi di più colori (indici
palette 8/22/23/30/52/62/63 nei record XF, non solo nero) — non solo
un'ipotesi teorica. La semplificazione booleano-nero descritta sopra
resta quindi visibile: alcune celle mostrano un bordo nero dove Excel
lo mostra grigio/colorato. Per un colore reale servirebbe rivedere
`CellStyle::fTBorderColor` e affini da `uchar` booleano a `rgb_color`
per lato (tocca `operator==`/`memcmp`, `SheetView::DrawCellBand`, il
formato ASCD, e sia l'importatore XLS che quello XLSX per coerenza) —
volutamente non affrontato ora su richiesta esplicita dell'utente, che
ha preferito limitarsi ad annotarlo qui piuttosto che allargare lo
scope della Fase 5 in corso.

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

## Fase 12 — Fedeltà visiva import XLSX (CHIUSA)

Verifica puntuale (grep mirato su `XlsxTranslator.cpp`, non solo
lettura del codice) contro il file `.xlsm` di gara reale già usato per
motivare Fase 9 e per il bug corretto subito prima di questa fase
(38 fogli): il translator importa oggi
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
- [x] **Grassetto/corsivo**: legge `<b/>`/`<i/>` dentro ogni `<font>`
      di `styles.xml` (`val="0"` li nega esplicitamente, raro ma
      valido) e sceglie lo stile del font già supportato da
      `gFontSizeTable`/`CellStyle::fFont` — la famiglia originale
      (es. "Calibri") non viene cercata/installata, solo stile e
      dimensione (`<sz val=".."/>`) vengono importati, stesso
      ripiego già usato da `MainWindow::ToggleBold`/`ToggleItalic`
      per una cella senza font esplicito. La sezione font di Fase 10
      in `WriteASCD` (`XlsxTranslator.cpp`), finora sempre vuota, ora
      scrive i valori reali quando presenti. **Scoperta per strada**:
      il primo indice mai assegnato da `CFontSizeTable::GetFontID` in
      un processo è 0, lo stesso valore che `CellStyle` usa come
      sentinella "nessun font esplicito" — se grassetto/corsivo fosse
      il primo font registrato ci finirebbe per caso, sparendo
      silenziosamente (bug reale scoperto scrivendo il test).
      `ParseStyles` riserva ora la voce Regular per prima apposta.
      Test: nuova fixture `sample_fontstyle.xlsx` in
      `translators/xlsx/tests/test_xlsx_translator.cpp`.
- [x] **Allineamento orizzontale**: legge `<alignment
      horizontal="..."/>` dentro ogni `<xf>` di `cellXfs` (figlio
      dell'elemento, non un suo attributo — evento XML separato,
      applicato sempre all'ultimo `<xf>` appena aggiunto) e mappa i
      valori su `CellStyle::fAlignment`/`EAlignment` già supportato —
      "centerContinuous"/"distributed" e simili, senza un
      corrispondente diretto, restano General. La sezione
      allineamento di Fase 10 in `WriteASCD` (`XlsxTranslator.cpp`),
      finora sempre vuota, ora scrive i valori reali quando presenti.
      Test: nuova fixture `sample_align.xlsx`.
- [x] **Bordi da stile**: risolve l'indice `borderId` di ogni `<xf>`
      contro `<borders>` in `styles.xml` — un lato conta come presente
      se il figlio `<left>`/`<right>`/`<top>`/`<bottom>` ha un
      attributo `style` diverso da `"none"` — e lo traduce nei quattro
      campi booleani per lato già definiti in Fase 11 (bordo presente/
      assente per lato, indipendentemente da spessore/colore reale
      dell'originale — stesso limite dichiarato per il significato dei
      campi in Fase 11). La sezione bordi di Fase 11 in `WriteASCD`
      (`XlsxTranslator.cpp`), finora sempre vuota, ora scrive i valori
      reali quando presenti. Test: nuova fixture `sample_borders.xlsx`.
- [x] **Sottolineato**: nuovo campo booleano `CellStyle::fUnderline`
      (Haiku `BFont` non ha un attributo sottolineato nativo, solo
      stile del font), voce "Sottolineato" nel menu Formato
      (`MainWindow::ToggleUnderline`, stesso principio di
      ToggleBold/ToggleItalic/ToggleBorder), disegnato a mano in
      `SheetView::Draw` come una linea sotto il testo (larghezza del
      testo davvero disegnato, colore del testo), persistito nel
      formato nativo con lo stesso principio delle sezioni opzionali
      già esistenti (Fase 11). Import XLSX: legge `<u/>` nel font
      (`val="none"` lo nega esplicitamente, gli altri stili diversi da
      "single" trattati comunque come sottolineato semplice).
      **Scoperta per strada**: la regola `libengine.a` del Makefile di
      `translators/xlsx` non ha prerequisiti — non si ricompila mai
      automaticamente quando cambia un header del motore, serve
      `make clean` esplicito in `engine/` dopo ogni modifica a un
      header di `CellStyle`/`Container`/ecc, altrimenti le librerie
      compilate contro il vecchio layout dello struct restano linkate
      silenziosamente (bug reale scoperto scrivendo il test:
      `SetCellStyle`/`GetCellStyle` sembravano ignorare il nuovo campo
      — in realtà un disallineamento ABI fra `.o` compilati con
      `sizeof(CellStyle)` diversi). Da tenere a mente per ogni
      modifica futura a un header del motore, non solo per questo
      campo.
- [x] **Testo a capo e altezza di riga automatica**: nuovo campo
      `CellStyle::fWrapText` (il motore non fa layout di testo, solo
      la UI). Voce "A capo automatico" nel menu Formato
      (`MainWindow::ToggleWrapText`), disegno a righe multiple in
      `SheetView::Draw` (`WrapTextLines`, a-capo per parola, nessuna
      sillabazione) e `SheetView::RecalculateWrappedRowHeights` che fa
      crescere l'altezza delle righe coinvolte quando serve (mai
      ridotta sotto quella già impostata) — chiamata sia dopo
      `ToggleWrapText` sia dopo l'apertura di un documento (nativo o
      importato): il translator XLSX imposta solo `fWrapText` da
      `wrapText="1"` (nessuna view/font a quel livello per misurare il
      testo), l'altezza vera si calcola quando il documento arriva
      nella UI, che ha sempre un font/view vivi. Si appoggia alle
      altezze di riga per-riga già persistite in Fase 10 per il
      salvataggio, non serve un nuovo meccanismo lì.
- [x] **Celle unite**: nuovo concetto a livello di `CContainer` —
      elenco di rettangoli per foglio (`AddMergedRange`/
      `GetMergedRanges`/`GetMergedRange`), non un campo per-cella.
      `SheetView::DrawCellBand` riempie l'intero rettangolo unito con
      lo sfondo della cella in alto a sinistra (cancella così griglia
      e bordi interni già tracciati), poi ridisegna solo il contorno
      esterno; solo la cella in alto a sinistra disegna il proprio
      contenuto, con un rettangolo esteso a tutto l'intervallo per
      allineamento/a capo. UI: "Unisci celle"/"Dividi celle"
      (`MainWindow::MergeCells`/`UnmergeCells`) — a differenza di
      Excel vero il contenuto delle celle diverse da quella in alto a
      sinistra non viene cancellato all'unione (solo nascosto dal
      disegno), scelta deliberata per restare non distruttiva senza
      una finestra di conferma dedicata. Persistenza: nuova sezione
      in coda (un rettangolo per intervallo, non il pattern per-cella
      delle altre sezioni). Import XLSX: legge `<mergeCell
      ref="A1:C1"/>` da `<mergeCells>`, indipendente dall'ordine
      rispetto alle celle. Verificato anche dal vivo sul file di gara
      reale (537 celle unite su 39 fogli), nessun crash.
- [x] **Tabelle strutturate (bande alternate)**: **approssimate come
      colori di sfondo statici all'importazione**, non come un vero
      oggetto tabella. Legge `xl/tables/tableN.xml` (collegato al
      foglio tramite i _rels del foglio stesso, non quelli della
      cartella di lavoro) per `showRowStripes`: il colore VERO di uno
      stile con nome incorporato (es. `TableStyleMedium2`, quello del
      file reale) non è nel file — è un tema grafico predefinito di
      Excel stesso, non salvato nella cartella di lavoro. Banda grigio
      chiaro neutra invece del colore esatto, scritta come normale
      colore di sfondo per cella (riusa l'infrastruttura colori di
      Fase 7) solo dove non c'è già un colore esplicito. Scelta
      deliberata per restare nello scope di "importazione fedele", non
      "editor di tabelle Excel vive" (niente filtro automatico, niente
      riga totali ricalcolata). **Bug reale scoperto scrivendo il
      test**: la banda usava la riga dell'intestazione come
      riferimento per l'alternanza invece della prima riga dati,
      disallineando la banda di una riga.
- [x] **Formattazione condizionale**: **valutata una tantum
      all'importazione e congelata come colore statico**, stesso
      principio delle tabelle sopra — non un motore di regole vive
      che si aggiornano al ricalcolo (richiederebbe uno storage
      per-range delle regole e una valutazione ad ogni `CalcCell`,
      un'estensione del motore molto più grande di tutto il resto
      della fase insieme). Legge `<conditionalFormatting sqref="...">`
      dal foglio (`sqref` può elencare più intervalli/celle separati
      da spazio) e `<dxfs>` da `styles.xml` per il colore — una
      sezione a parte da `<cellXfs>`, con la stranezza opposta a
      `<fills>`: il colore visibile è in `bgColor`, non `fgColor` (i
      dxf non hanno `patternType`, sono impliciti "solid"). Solo due
      tipi di regola gestiti, i più comuni nel file reale:
      `cellIs`/`equal` (confronto con un letterale) e
      `duplicateValues`; gli altri tipi ECMA-376 (`containsText`,
      `top10`, `colorScale`, `dataBar`, `iconSet`, `expression` con
      formula arbitraria...) sono ignorati in sicurezza, nessun colore
      applicato — richiederebbero un vero motore di valutazione
      formule. Limite dichiarato: il colore non si aggiorna più se il
      valore della cella cambia dopo l'importazione.
- [x] **Formati data/ora**: punto separato dai "Formati numero" più
      sopra perché il rischio è diverso — non un'approssimazione di
      rendering ma la scelta del *tipo* di valore da creare. Le celle
      il cui `numFmtId` risolto è riconosciuto come data (incorporati
      14-22/45-47 o personalizzato individuato da
      `LooksLikeDateFormat`, la stessa euristica già scritta per i
      formati numero, qui riusata per decidere se convertire) vengono
      convertite dal numero seriale Excel a un `Value(time_t)`
      costruito direttamente in `ExcelSerialToTime` — niente parsing
      di stringhe data, che dipenderebbe dal locale a runtime
      (`gDateOrder`/`CellParser.cpp`). Una volta creato un
      `Value(time_t)`, `CFormatter::FormatValue` lo formatta come data
      automaticamente in base al solo `Value::fType`, indipendente da
      `CellStyle::fFormat`: nessuna sezione nuova serve nel formato
      nativo, la data viaggia come normale contenuto di cella già
      coperto dal meccanismo esistente. Offset epoca: 25569 giorni
      (sistema 1900, predefinito) o 24107 (sistema 1904, rilevato da
      `<workbookPr date1904="1"/>` nel workbook). Limite dichiarato:
      la costante 25569 compensa il bug storico di Excel "1900 è
      bisestile" per le date da marzo 1900 in poi, restando sbagliata
      di un giorno per le rare date di gennaio/febbraio 1900. Test:
      nuova fixture `sample_dates.xlsx` (formato incorporato e
      personalizzato, più una cella numerica senza stile data per
      verificare che non venga toccata).
- [x] **Immagini incorporate**: legge `xl/drawings/drawingN.xml` (un
      logo nel file reale) — `<xdr:from>` da colonna/riga 0-based più
      uno scarto in EMU (unità di DrawingML, 9525 per pixel a 96 DPI,
      la risoluzione predefinita di Excel), la dimensione da
      `<xdr:ext>`/`<a:xfrm><a:ext>` — e risolve `<a:blip r:embed="..">`
      tramite i _rels DEL DRAWING stesso (un livello di indirizzamento
      indipendente da quelli del foglio, usati invece per collegare il
      foglio al proprio drawing). Quando l'anchor non dà una
      dimensione esplicita (`cx`/`cy` a 0, il caso comune anche nel
      file di gara reale) si usa la dimensione naturale del PNG, letta
      dal proprio header IHDR senza serve un decodificatore completo.
      Nuovo concetto a livello di modello: `EmbeddedImage`
      (`engine/src/Cell/EmbeddedImage.h`, nuovo file, struct dati pura
      senza dipendenze da Interface/Translation Kit) vive fuori da
      `CContainer`, come i grafici — ancorata a una cella invece di un
      rettangolo assoluto, così segue la cella se righe/colonne
      cambiano dimensione, esattamente come in Excel vero. Il blob PNG
      grezzo viaggia così com'è fino a `SheetView`, che lo decodifica
      in una `BBitmap` solo al disegno (`BTranslatorRoster`, nessuna
      cache: il file reale ne ha una sola, piccola). Persistenza:
      nuova sezione in coda al formato nativo, stesso principio
      opzionale delle sezioni precedenti. **Bug reale scoperto dal vivo
      sul file di gara reale**: dopo aver ricompilato `ui/` (che ora si
      aspetta la sezione immagini per ogni foglio) senza reinstallare
      anche il translator XLSX aggiornato, l'app restava bloccata
      all'apertura — lo stesso disallineamento dello stream ASCB già
      documentato per i grafici in Fase 9, qui riscoperto perché
      `translators/xlsx/Makefile` ha un passo `install` separato da
      `make` (copia l'add-on in `~/config/non-packaged/add-ons/
      Translators/`) facile da dimenticare dopo una modifica al
      formato. Verificato anche dal vivo sul file di gara reale (39
      fogli, un logo su "P-GE_Generali") dopo aver reinstallato il
      translator: apertura senza crash né blocchi. Test: nuova fixture
      `sample_image.xlsx` (ancoraggio, scarto/dimensione espliciti,
      blob PNG verificato byte per byte) in
      `translators/xlsx/tests/test_xlsx_translator.cpp` e round-trip
      completo (ancoraggio/scarto/dimensione/blob) in
      `ui/tests/test_persistence.cpp`.
- [x] Test dedicato per ciascun punto in
      `translators/xlsx/tests/test_xlsx_translator.cpp` (lettura) ed
      eventualmente `ui/tests/` per il disegno (celle unite, testo a
      capo, sottolineato), sul modello dei test già esistenti per
      colori/larghezza colonna.

### Aggiunta: pulsanti toolbar per le funzioni di formattazione

Grassetto, Corsivo, Sottolineato, Allinea sinistra/centro/destra, A
capo automatico, Colore testo e Colore sfondo erano comandi già
implementati da tempo (menu Formato), ma rimasti solo voci di menu
perché il catalogo HVIF (`Atomo123_icons/`) non aveva ancora
un'icona verificata a vista per ciascuno. Nove nuove icone (Bold/
Italic/Underline/Justify Left-Center-Right/Wrap Lines/Color/
Highlight, tutte MIT da hvif-store.art) coprono ora anche queste
funzioni: promosse a pulsante in un nuovo gruppo "Formato" nella
toolbar, seguendo lo stesso schema dei gruppi esistenti
(`kToolbarGroups` in `MainWindow.cpp`).

I tre pulsanti di allineamento condividono lo stesso comando
(`kMsgSetAlignment`) e si distinguono solo per un parametro
`alignment` nel `BMessage`: `ToolbarButtonDef` guadagna due campi
opzionali (`paramName`/`paramValue`) per coprire questo caso, senza
toccare le voci esistenti (restano `NULL`/0 per inizializzazione
automatica degli aggregati).

Test: nuovo `ui/tests/test_format_toolbar.cpp` — verifica che i nove
pulsanti esistano con `MainWindow` come target, e che il loro
`BMessage` (recapitato a `MainWindow::MessageReceived` come farebbe la
`BLooper` reale) produca lo stesso effetto del comando di menu
equivalente, già verificato in `test_format.cpp` con una chiamata
diretta al metodo.

### Aggiunta: troppopieno della toolbar quando la finestra si restringe

Con 24 pulsanti in tutto (dopo il gruppo Formato sopra), restringendo
la finestra quelli in eccesso restavano semplicemente tagliati fuori
dalla vista, irraggiungibili — segnalato dall'utente, che ha scelto
esplicitamente fra due alternative proposte (andare a capo su più
righe, oppure nascondere i pulsanti in eccesso dietro un pulsante di
troppopieno) quella del pulsante di troppopieno, per non far crescere
l'altezza della toolbar a ogni ridimensionamento.

Nuova classe `ToolbarView` (`ui/src/ToolbarView.h`/`.cpp`): un
contenitore per veri `BButton` (non una vista disegnata a mano) che,
quando i pulsanti non entrano più tutti nella larghezza disponibile,
nasconde quelli in eccesso e li sostituisce con un unico pulsante
">>" — cliccandolo si apre un `BPopUpMenu` con le stesse voci
(etichetta e comando) dei pulsanti nascosti. Stesso principio già
usato da `SheetTabView` per le schede dei fogli (scorrimento con due
frecce quando non entrano tutte), qui adattato al pulsante di
troppopieno scelto dall'utente invece che allo scorrimento.
`BuildToolbar()` in `MainWindow.cpp` costruisce ora un `ToolbarView`
invece di un `BGroupView`.

**Bug scoperto scrivendo il test**: `BView::ResizeTo()` non garantisce
che `FrameResized()` sia già stato eseguito quando il chiamante non è
il thread della finestra stessa (il giro passa dall'app_server) —
lo stesso motivo per cui `SheetTabView::SetSheets()` richiama
`Layout()` da sé invece di aspettare un giro di disegno.
`ToolbarView::Layout()` è quindi pubblico apposta, richiamabile
direttamente subito dopo un `ResizeTo()` nei test, oltre che da
`FrameResized()`/`AttachedToWindow()` nell'uso reale.

Test: nuovo `ui/tests/test_toolbar_view.cpp` (venti pulsanti di prova,
nessuna vera `MainWindow`) verifica che una vista larga mostri tutti i
pulsanti senza troppopieno, che una vista stretta ne nasconda una
parte dietro il pulsante ">>", e che riallargare la vista li faccia
ricomparire tutti. Verificato anche dal vivo ridimensionando la
finestra reale (via `hey`, "SET Frame of Window 0 to BRect(...)").

### Bug scoperto: il pulsante ">>" della toolbar non apriva il menu

Segnalato dall'utente subito dopo la funzionalità sopra: il pulsante
">>" appariva correttamente quando la finestra si restringeva, ma
cliccandolo non succedeva nulla — il menu con le icone nascoste non
si apriva mai.

Causa: `ToolbarView::ToolbarView()` chiamava
`fOverflowButton->SetTarget(this)` nel proprio costruttore, prima che
`this` (la `ToolbarView` stessa) fosse agganciata a qualunque
finestra — l'aggancio avviene solo dopo, quando `BuildToolbar()` la
restituisce e `MainWindow` la aggiunge al proprio layout. A quel
punto `Looper()` vale ancora `NULL`, un target inaffidabile da quel
momento in poi per tutta la vita del pulsante.

Fix: invece di affidarsi a `BInvoker::SetTarget()`/`BMessenger`
(fragile per un pulsante il cui target è la vista stessa che lo
contiene, a differenza dei pulsanti veri della toolbar che puntano
tutti a `MainWindow`, una `BLooper` valida fin dalla costruzione), il
pulsante ">>" è ora un'istanza di una piccola sottoclasse locale di
`BButton` che chiama direttamente `ToolbarView::ShowOverflowMenu()`
da un override di `Invoke()` — una chiamata C++ diretta, che non ha
bisogno di un `Looper` valido e funziona a prescindere da
quando/se la vista è agganciata. Corretta anche una piccola perdita
di memoria scoperta nello stesso punto (`BPopUpMenu` asincrono senza
`SetAsyncAutoDestruct()`).

Verificato dal vivo: impostando il valore del pulsante da script
(`hey`, che passa dallo stesso `Invoke()` di un vero clic) il menu ora
si apre correttamente con tutte le voci nascoste.

### Nota per il futuro: ricalcolo a grafo delle dipendenze (misurato, non implementato)

Esistevano due branch remoti sperimentali, partiti da un punto ormai
molto vecchio della cronologia (prima della Fase 11):
`claude/expressionparser-mini-excel-r929jg` (un micro-benchmark del
ricalcolo attuale) e `claude/depgraph-recalc-prototype` (lo stesso
benchmark più un proof-of-concept standalone di un ricalcolo a grafo
delle dipendenze). Su richiesta dell'utente, prima di eliminarli come
branch ormai vecchi e non integrabili puliti (troppo indietro rispetto
a `master` per un merge senza conflitti), sono stati eseguiti in un
worktree temporaneo per misurarli con l'engine reale attuale — nessuna
modifica è stata portata su `master`, solo la misura.

**`engine/tests/recalc_benchmark.cpp`** (replica in-process di
`RecalculateAll`, linkato all'engine vero): conferma che il modello a
punto fisso attuale ha un problema di correttezza silenzioso, non solo
di prestazioni. Sulle catene "all'indietro" (`Ai = A(i+1)+1`, ordine di
visita opposto all'ordine di dipendenza) il ricalcolo non converge già
a partire da N=60: il guard di 50 passate scatta prima della
convergenza e lascia i valori di coda sbagliati **senza segnalare
nessun errore**. Sul fronte prestazioni, il ricalcolo gira sincrono sul
thread della finestra: con fogli grandi ma ben formati (catene in
avanti, il caso "fortunato") il blocco della UI diventa percepibile
oltre ~16-20k celle e arriva a **~3 secondi** con 190.000 celle (38
fogli × 5.000 celle).

**`prototypes/depgraph/depgraph_poc.cpp`** (PoC standalone in C++
puro, stesso modello cella/formula/riferimenti ma senza dipendenze
Haiku, per poter girare ovunque): un ricalcolo a grafo delle
dipendenze (dirty-set + ordine topologico) produce **valori identici**
al punto fisso in tutti i casi in cui questo converge, li corregge
dove il punto fisso fallisce (catene all'indietro: una sola passata,
sempre corretta; riferimenti circolari: rilevati esplicitamente invece
di divergere in silenzio), e riduce drasticamente il lavoro per singola
modifica — fino a 32000x meno valutazioni su una modifica isolata in
un foglio largo con 16.000 celle indipendenti.

**Conclusione pratica**: i numeri giustificano di riprendere in mano
l'idea in futuro (grafo delle dipendenze al posto del punto fisso, più
lo spostamento del ricalcolo su un thread worker — l'inutilizzato
`CCalcThread` già esiste nel motore — per eliminare il freeze della
UI), ma è lavoro non banale sul motore reale e non ancora pianificato
in nessuna fase. I due branch sono stati eliminati dopo la misura (non
contenevano altro che questi due file sperimentali, già catturati per
intero in questa nota); chi riprende il lavoro riparte da zero sul
motore vero, non da quei prototipi.

### Bug scoperto: mettere in grassetto una sola cella metteva in grassetto tutto il foglio

Segnalato dall'utente insieme ad altri due problemi (selezione non
trasparente, impossibile selezionare un'intera riga/colonna — questi
due restano da affrontare): su un documento nuovo, selezionando una
sola cella e cliccando il pulsante "Grassetto" della toolbar,
diventava in grassetto l'intero foglio, non solo la cella selezionata.

Causa, isolata con un piccolo programma diretto in-process (tre celle
A1/A2/B1, selezione solo su A1, clic sul pulsante vero, poi lettura
diretta dello stile di tutte e tre le celle — lo stesso principio
delle altre indagini in questo file, senza passare da `hey`/screenshot):
`CFontSizeTable::GetFontID` (`engine/src/Cell/FontMetrics.cpp`) cerca
un font già presente in tabella o, se non lo trova, lo aggiunge in
coda e ne restituisce l'indice. Su un documento dove nessun font era
ancora stato richiesto (la tabella `gFontSizeTable` è vuota — capita
facilmente, dato che `CContainer` in `MainWindow.cpp` viene sempre
costruito con `CContainer(NULL, NULL)`, senza una vista che ne
inizializzi subito un font di default), il PRIMO font mai richiesto
occupa l'indice 0 — che è anche il valore implicito di
`CellStyle::fFont` per qualunque cella mai formattata esplicitamente
(`CellStyle::CellStyle()` azzera l'intera struct con `memset`). Il
grassetto richiesto per A1 diventava quindi, per puro accidente di
ordine di chiamata, il font "di default" di ogni cella non ancora
formattata dell'intero foglio — A2 e B1 comprese, mai selezionate.

Fix: nuovo `CFontSizeTable::ReserveDefaultSlot()`, che riserva sempre
l'indice 0 con un font segnaposto sicuro (nessun accesso ad
app_server: stesso principio del fallback statico già usato da
`CFontSizeTable::operator[]`, ma esplicitamente azzerato campo per
campo dato che l'oggetto finisce copiato dentro il vector, non è esso
stesso `static`) prima di cercare/aggiungere il font davvero
richiesto — così un font specifico riceve sempre un indice ≥ 1,
indipendentemente da quale sia il primo mai richiesto. Chiamato
anche da `SetFontID`/`GetFontInfo`, che accedevano `fFonts[formatID]`
direttamente senza nessun controllo dei limiti (un accesso fuori dai
limiti su un vettore ancora vuoto, non solo il bug di indice
condiviso).

Test: `ui/tests/test_format_toolbar.cpp` esteso con A2/B1 (mai
selezionate né formattate) accanto ad A1 — verifica che restino senza
grassetto dopo il clic sul pulsante. Nessuna regressione nelle 33
suite UI né in quelle di engine/translator (FontMetrics è condiviso
da tutti).

### Bug scoperto: l'area di selezione multi-cella non era trasparente, nascondeva il contenuto

Secondo problema della stessa segnalazione dell'utente: selezionando
più celle (trascinamento, Maiusc+frecce), la tinta azzurra che
evidenzia l'intervallo copriva per intero il testo delle celle
selezionate (non quella attiva, lasciata sempre bianca) invece di
lasciarlo leggibile in trasparenza, come in Excel/LibreOffice Calc.

Causa: `SheetView::Draw()` disegnava la tinta con
`FillRegion()`/`SetHighColor()` nella modalità di disegno predefinita
di `BView` (`B_OP_COPY`, colore opaco) — un riempimento a colore
pieno *sopra* lo sfondo/testo già disegnati da `DrawCellBand()` poco
prima nello stesso passaggio, che quindi finiva coperto invece che
sfumato.

Fix: `SetDrawingMode(B_OP_ALPHA)` con `SetBlendingMode(B_PIXEL_ALPHA,
B_ALPHA_OVERLAY)` e un colore con alfa parziale (60/255) prima del
`FillRegion()`, che ora mescola la tinta con quanto già disegnato
sotto invece di sostituirlo; `SetDrawingMode(B_OP_COPY)` ripristinato
subito dopo, dato che il resto di `Draw()` (bordi, intestazioni,
celle successive) si aspetta la modalità opaca predefinita.

Verificato dal vivo con uno screenshot (un piccolo programma di prova
diretto in-process, tre celle di testo diverse per riga, selezione
A1:C2): il testo di tutte le celle nell'intervallo resta leggibile
sotto la tinta, la cella attiva (A1) resta bianca col solo bordo,
come previsto. Nessuna infrastruttura di cattura pixel in questo
progetto per un confronto automatico esatto: `ui/tests/test_selection.cpp`
verifica invece che `Draw()` ripristini `B_OP_COPY` dopo aver
disegnato la tinta, cosi' la modalita' alfa non resta "incollata" al
resto del disegno.

### Aggiunta: selezionare l'intera riga/colonna cliccando l'intestazione

Terzo problema della stessa segnalazione dell'utente: cliccare
sull'intestazione di una riga o di una colonna non faceva
assolutamente nulla — mancava del tutto, come ammetteva già un
commento nel codice stesso (`SheetView::MouseDown`, subito prima del
controllo per il ridimensionamento: "che oggi non fa nulla per un
clic sull'intestazione"). L'unica cosa gestita su un'intestazione era
il trascinamento del bordo per ridimensionare; un clic altrove veniva
scartato senza selezionare nulla.

Aggiunto: un clic sull'intestazione di colonna (fuori da una maniglia
di ridimensionamento) seleziona l'intera colonna; un clic
sull'intestazione di riga seleziona l'intera riga; Maiusc+clic estende
dall'ancora corrente fino alla colonna/riga cliccata, come un clic
normale su una cella — comportamento identico a Excel/LibreOffice
Calc.

Un dettaglio non ovvio: l'intervallo selezionato copre sempre tutte le
16384 righe (`kColCount`/`kRowCount`, `Constants.h`) o tutte le 702
colonne, ma la CELLA ATTIVA finale (`fSelection`, quella che
`ScrollToShowSelection()` tiene visibile) non può restare sull'angolo
estremo come una prima implementazione ingenua avrebbe fatto — cliccare
l'intestazione di una colonna avrebbe fatto scorrere la vista fino
alla riga 16384 del foglio a ogni clic, un salto violento e
spiazzante. `SetSelection()` fissa invece entrambi gli angoli
sull'estremo lontano, poi `ExtendSelection()` sposta solo l'angolo
"attivo" all'altro estremo (riga 1 per una colonna, colonna 1 per una
riga) — l'intervallo selezionato resta lo stesso, ma la vista non si
sposta mai lontano da dove si è cliccato. Scoperto scrivendo il primo
test (falliva in modo enigmatico finché non ci si è accorti che il
clic sull'intestazione di riga, subito dopo quello sull'intestazione
di colonna nello stesso test, cadeva su coordinate ormai sbagliate —
la vista si era scorsa altrove nel frattempo).

Test: `ui/tests/test_selection.cpp` esteso con un clic
sull'intestazione della colonna B (verifica che selezioni l'intera
colonna B, non solo la cella cliccata) e uno sull'intestazione della
riga 2 (idem per la riga). Verificato anche dal vivo con uno
screenshot (stesso programma di prova della tinta di selezione sopra,
clic vero su `MouseDown()` sull'intestazione di colonna): la colonna B
intera si evidenzia, la vista non salta altrove.

### Bug scoperto: crash reale "Looper must be locked" aprendo Colore sfondo/testo

Primo crash vero riprodotto dall'utente stesso (non in un test), con
tanto di rapporto di debug di Haiku: cliccando "Colore sfondo" con la
finestra ColorWindow già aperta, l'app si bloccava con "Looper must
be locked" dentro `BColorControl::SetValue` → `BView::Invalidate()` →
`BView::Bounds()`.

Causa: `MainWindow::ShowColorWindow()` chiamava
`fColorWindow->SetMode(...)` direttamente — una chiamata C++ normale,
non un `BMessage` — ma `ColorWindow` è una `BWindow` a sé, con un
thread/`BLooper` proprio (diverso da quello di `MainWindow`).
`SetMode()` tocca `fColorControl` (una sua `BView`) tramite
`SetValue()`, che chiama `Invalidate()`: toccare una `BView` di
un'altra finestra senza aver preso il lock di quella finestra viola le
regole di threading di Haiku, cosa che il sistema intercetta e blocca
con un debugger trap invece di corrompere silenziosamente lo stato.
Capita quando la ColorWindow esiste già (il suo thread è vivo e
attivo) — proprio lo scenario del crash reale: l'utente l'aveva già
aperta una volta.

Lo stesso identico schema (chiamata diretta a un metodo di un'altra
finestra, senza lock) era replicato in altri due punti dello stesso
file, mai andati in crash finora solo per tempistica più favorevole:
`MainWindow::RefreshNameWindow()` (`fNameWindow->SetNames(...)`,
chiamata anche a ogni Aggiungi/Aggiorna/Elimina nome definito, non
solo aprendo la finestra — probabilità di corsa più alta di
ColorWindow) e `MainWindow::ShowPreferencesWindow()`
(`fPreferencesWindow->SetValues(...)`). Il progetto usa già altrove
nello stesso file (`MainWindow::~MainWindow`, righe 590-636 circa) lo
schema corretto `finestra->Lock(); finestra->Quit();` per la
distruzione di queste stesse finestre secondarie — solo queste tre
chiamate "di aggiornamento" (non di chiusura) non lo seguivano.

Fix: `if (finestra->Lock()) { finestra->SetXxx(...); finestra->Unlock(); }`
in tutti e tre i punti.

Test: `ui/tests/test_format_toolbar.cpp` esteso — clic reale (tramite
il pulsante vero della toolbar, non una chiamata diretta) su
"Colore sfondo" due volte di seguito, la seconda con la ColorWindow
già aperta: stesso scenario del crash reale, verifica solo che non
vada in crash (l'esito della scelta colore, asincrono e guidato
dall'utente, resta fuori scopo). Nessuna regressione nelle 34 suite.

### Bug scoperto: la seconda cifra digitata sostituiva la prima invece di aggiungersi

Segnalato dall'utente: digitando un numero su una cella vuota, con una
sola cifra funzionava, ma con due o più il risultato era sbagliato —
"123" diventava "23" (la prima cifra spariva).

Isolato con un programma di prova diretto (variante di
`test_real_input_edit.cpp`, vedi sopra): il primo tasto vero apre
l'editor con quel carattere ("1"), corretto; il SECONDO tasto vero,
consegnato a chi ha davvero il fuoco tastiera a quel punto (l'editor
appena aperto, non più `SheetView`), **sostituiva** tutto il
contenuto ("2" invece di "12"); il terzo tasto si comportava di nuovo
correttamente, aggiungendosi ("23"). Fondamentale aver indirizzato il
secondo tasto sintetizzato al vero possessore del fuoco: un test che
avesse richiamato `KeyDown()` due volte direttamente su `SheetView`
(come `test_editing.cpp` e simili, per i motivi già spiegati nei loro
commenti) non avrebbe mai riprodotto il bug, perché in quel caso il
secondo tasto non passa mai dalla `BTextView` dell'editor.

Causa: `SheetView::StartEditing()` chiamava
`fEditor->TextView()->Select(len, len)` (cursore in fondo al testo,
nessuna selezione) **prima** di `fEditor->MakeFocus(true)` — ma
`BTextView` seleziona tutto il proprio contenuto quando acquisisce il
fuoco per la prima volta (lo stesso comportamento "seleziona tutto" di
un normale campo di testo), sovrascrivendo subito quella posizione del
cursore. Il tasto successivo, premuto su un testo interamente
selezionato, lo sostituiva invece di aggiungersi — proprio come aveva
intuito l'utente stesso ("perché è tipo selezionata").

Fix: invertito l'ordine, `MakeFocus(true)` prima e `Select(len, len)`
subito dopo, così la posizione esplicita del cursore è l'ultima parola
e non viene più sovrascritta dalla selezione automatica del fuoco.

Test: `ui/tests/test_real_input_edit.cpp` esteso con un secondo
`B_KEY_DOWN` sintetizzato, indirizzato esplicitamente alla `BTextView`
dell'editor (non a `SheetView`) subito dopo il primo — verifica che il
testo diventi "78" e non "8", e che il valore scritto nel documento
dopo la conferma sia 78 e non 8. Nessuna regressione nelle 34 suite.

### Bug scoperto: la trasparenza dei PNG incorporati non veniva rispettata a schermo

Chiesto dall'utente: un'immagine con sfondo trasparente inserita nel
foglio (import XLSX, `EmbeddedImage`, vedi Fase 12) manteneva davvero
la trasparenza anche in Atomo123?

Risposta: no. Il canale alpha sopravviveva alla decodifica (il
translator PNG produce un `BBitmap` `B_RGBA32` se il sorgente ne ha
uno) e al salvataggio nel formato nativo `.ascd` (blob grezzo, mai
ri-codificato — vedi `test_persistence.cpp`), ma `SheetView::Draw()`
disegnava le immagini incorporate con `DrawBitmap()` senza mai
impostare una modalità di disegno alpha-aware: restava attiva
`B_OP_COPY` (l'impostazione predefinita di `BView`), che copia i byte
RGB del sorgente così come sono ignorando l'alpha — uno sfondo
"trasparente" appariva quindi con il colore pieno del pixel sorgente
invece dello sfondo della cella sottostante.

Fix: `SetDrawingMode(B_OP_ALPHA); SetBlendingMode(B_PIXEL_ALPHA,
B_ALPHA_OVERLAY);` prima del `DrawBitmap()`, `SetDrawingMode(B_OP_COPY)`
subito dopo — stesso schema già in uso per il tinteggio della
selezione poco più sopra nello stesso `Draw()`.

Test: `ui/tests/test_image_alpha.cpp`, nuovo. Codifica al volo un vero
PNG con canale alpha (metà rosso pieno, metà completamente
trasparente) tramite lo stesso `BTranslatorRoster` usato per
decodificarlo — non un blob finto come in `test_persistence.cpp`, qui
serve un file che il Translation Kit sappia davvero decodificare, per
esercitare il percorso di codice reale che ha causato il bug. Disegna
`SheetView` su una vera `BBitmap` offscreen ("accetta viste", tecnica
mai usata finora nei test di questo progetto: serve leggere i pixel
realmente scritti da `DrawBitmap()`, non solo lo stato interno — un
controllo tipo "modalità di disegno dopo `Draw()`" non avrebbe scoperto
il bug, dato che cambio e ripristino avvengono entrambi dentro la
stessa chiamata). Verificato che il test riproduce davvero il bug:
sull'eseguibile senza il fix la metà "trasparente" risultava rosso
pieno; con il fix mostra correttamente lo sfondo bianco della cella.

Nota sull'ambiente di test in questa sessione: dopo molti lanci
ravvicinati di eseguibili con una vera `MainWindow` (le decine di test
UI di questa fase), alcune suite hanno iniziato a bloccarsi
indefinitamente in fase di creazione/attivazione finestra —
riprodotto anche su un programma minimo isolato che non tocca il
codice di questa correzione, e anche sull'eseguibile *precedente* al
fix (`git stash`), quindi non è una regressione introdotta qui: sembra
un limite del desktop condiviso di questa sessione sotto carico
sostenuto di finestre reali, non del codice. Le suite headless e
quelle senza `MainWindow` restano tutte verdi; `test-image-alpha`
(quella dedicata a questo bug) passa in modo pulito e ripetibile
proprio perché disegna offscreen, senza dipendere da attivazione
finestra/fuoco.

### Bug scoperto: le immagini incorporate importate da XLSX finivano nella posizione sbagliata rispetto a Excel

Segnalato dall'utente su un file di gara reale (loghi istituzionali
ancorati alle prime righe): confrontando lo stesso file aperto in
Excel vero (screenshot da Windows) con Atomo123, l'immagine risultava
molto più in basso/sovrapposta al testo, non semplicemente spostata di
pochi pixel.

Due cause distinte, isolate ispezionando a mano l'XML del `drawing1.xml`
e del `sheet1.xml` dentro il file XLSX (un semplice archivio zip):

1. **Larghezza di colonna approssimata**. `ExcelColWidthToPixels`
   (`XlsxTranslator.cpp`) usava `char*7+5`, un'approssimazione diffusa
   ma non quella vera di Excel (ECMA-376 18.3.1.13: troncamento,
   `floor(((256*w + floor(128/MDW))/256)*MDW)`, con MDW=7 per il font
   predefinito Calibri 11). Sulle tre colonne che precedevano
   l'immagine nel file reale (5.46/45.46/14.46 caratteri) lo scarto era
   di circa 5 pixel per colonna, ~16px cumulati — sposta a destra
   qualunque oggetto ancorato dopo diverse colonne, tanto peggio quante
   più colonne ci sono prima.
2. **Altezze di riga mai importate**. Il file dichiarava
   `<row r="1" ht="48.75" customHeight="1">` (riga alta apposta per
   contenere i loghi) ma `XlsxTranslator.cpp` non aveva NESSUN codice
   che leggesse l'attributo `ht`: ogni riga restava all'altezza
   predefinita di `SheetView` (20px). Un'immagine di altezza fissa
   (73px nell'esempio) ancorata a una riga che nell'originale era alta
   65px ma in Atomo123 restava a 20px si ritrovava a coprire diverse
   righe reali sottostanti invece di stare contenuta in una sola —
   questa, non lo scarto orizzontale, era la causa dominante della
   sovrapposizione vistosa segnalata dall'utente.

Fix:
- `ExcelColWidthToPixels` sostituita con la formula esatta (troncamento,
  non arrotondamento).
- `SheetStart`/`SheetContext` (parsing di `<row>`, un fratello di `<c>`
  dentro `<sheetData>`) ora raccoglie le righe con `ht`+`customHeight="1"`
  espliciti, convertite in pixel con lo stesso fattore 4/3 già usato per
  `SheetView::kRowHeight` (punti tipografici → pixel a 96 DPI). La
  sezione "altezze di riga" nel formato ASCD prodotto da questo
  translator esisteva già (sempre scritta, prima sempre vuota, Fase 10)
  — bastava popolarla, nessuna modifica al formato file né a
  `ui/src/AscdIO.cpp` (che la legge già).

Verificato visivamente: screenshot dell'app prima/dopo il fix
confrontati con lo screenshot reale da Excel su Windows fornito
dall'utente — dopo il fix i loghi restano contenuti nelle prime due
righe (ora alte quanto nell'originale) e il testo che segue
("SOGGETTO PROPONENTE" ecc.) torna a comparire sotto, non più
sovrapposto.

Test: `translators/xlsx/tests/test_xlsx_translator.cpp` esteso —
le tre asserzioni sulla larghezza di colonna aggiornate ai valori
esatti (140/56/56px invece di 145/61/61px dell'approssimazione);
`tests/sample.xlsx` esteso con `ht="30" customHeight="1"` sulla riga 1
(30pt → 40px atteso) e nuova asserzione che verifica l'altezza
importata. 145/145 controlli passati.

### Migliorie di fedeltà XLSX (1/3): griglia nascosta per-foglio

Terza voce emersa dallo stesso confronto file reale/Excel delle due
sopra: il file dichiarava `<sheetView showGridLines="0">` (griglia
nascosta di proposito, un look pulito da documento ufficiale), ma
Atomo123 la mostrava comunque. Causa: `SheetView::ShowGrid` era
pilotata da un'unica preferenza GLOBALE dell'applicazione (`gPrefs`,
lo stesso interruttore per ogni documento), non da un attributo del
foglio stesso.

Fix: `showGrid` diventa un campo per-foglio in `AscdSheet` (stesso
schema di `frozenRows`/`frozenCols`, Fase 10) — persistito nel formato
nativo ASCD (sezione opzionale in coda, un byte, `true` di default per
i file scritti prima di questa modifica), importato da XLSX
(`<sheetView showGridLines="...">`, assente = visibile, il default di
Excel), e risincronizzato in entrambe le direzioni a ogni cambio
scheda (`MainWindow::SwitchToSheet`) esattamente come Blocca riquadri.
La preferenza globale (`gPrefs`) resta, ma solo come valore di partenza
per un foglio nuovo (`ResetWorkbook`) — non più l'unica fonte di
verità per un documento già aperto: attivare/disattivare la griglia
dal pannello Preferenze ora scrive sul foglio attivo, non solo sulla
vista.

Test: `ui/tests/test_persistence.cpp` esteso con un giro salva/ricarica
di `showGrid=false` (compreso il caso "file senza questa sezione, il
default resta `true`"); `translators/xlsx/tests/sample.xlsx` esteso con
`<sheetView showGridLines="0"/>` e nuova asserzione nel translator;
`ui/tests/test_multisheet.cpp` esteso — un foglio con griglia nascosta
in mezzo a due con griglia visibile, verificato che il cambio scheda
avanti e indietro non la confonda con quella degli altri fogli (stesso
principio già usato lì per le larghezze di colonna personalizzate).
Nessuna regressione nelle suite UI (a parte il blocco pre-esistente e
già documentato sopra di `test-paste-range` su questo desktop
condiviso, indipendente da questa modifica).

### Migliorie di fedeltà XLSX (2/3): colore della linguetta del foglio

Seconda voce dello stesso confronto (tasto destro sulla scheda in
Excel > "Colore scheda", `<sheetPr><tabColor rgb="FF00B050"/></sheetPr>`
nel file XML): nessuna traccia in Atomo123, né in lettura né in
disegno — `SheetTabView` disegnava ogni scheda con due soli colori
fissi (bianco se attiva, grigio se no), senza alcun posto per un
colore personalizzato.

Fix: `hasTabColor`/`tabColor` seguono lo stesso schema di `showGrid`
poco sopra — un campo per foglio in `AscdSheet`, persistito nel
formato nativo ASCD (sezione opzionale in coda, un byte "presente" più
tre byte RGB) e importato da XLSX (`HexToColor`, già usata per gli
stessi colori "rgb=" di font/sfondo cella, riusata qui senza
duplicazione). `SheetTabView::SetSheets` accetta ora due vettori
opzionali paralleli ai nomi (colore/presente-o-no per scheda) — resta
comunque ignara di `AscdSheet`/`MainWindow`, riceve solo colori grezzi,
stesso principio di disaccoppiamento già in uso per gli altri campi.

Stile di disegno scelto (`SheetTabView::Draw`): la scheda **non**
attiva con un colore lo mostra a tutta area (si nota anche da lontano,
esattamente come l'originale in Excel); quella **attiva** resta
bianca — deve confondersi con il foglio sopra, non spiccare — con solo
una barra colorata di 3px sul bordo inferiore a ricordare la scelta.
Nessuna interfaccia per sceglierlo a mano, per ora: solo lettura/
persistenza/disegno di quello importato.

Test: `ui/tests/test_sheet_tabs.cpp` esteso con un controllo a livello
di pixel (non di solo stato interno) — disegna `SheetTabView` su una
vera `BBitmap` offscreen ("accetta viste", la stessa tecnica di
`test_image_alpha.cpp`) e legge i colori davvero scritti per la
scheda colorata attiva/non attiva e quella senza colore. Nota tecnica
scoperta scrivendo questo test: `SetSheets()` (che chiama `GetFont()`/
`Invalidate()`) va richiamato con la bitmap offscreen **già bloccata**
(`Lock()`), non solo `Draw()` — chiamato prima manda in crash il
processo (verificato a parte con un piccolo programma di prova prima
di correggere il test). `translators/xlsx/tests/sample.xlsx` esteso
con `<sheetPr><tabColor rgb="FF00B050"/></sheetPr>` (lo stesso verde
del file reale che ha motivato questa fase) e nuova asserzione nel
translator; `ui/tests/test_persistence.cpp` esteso con un giro
salva/ricarica del colore (compreso il caso "file senza questa
sezione, nessun colore di default"). Nessuna regressione nelle suite
UI (stesso blocco pre-esistente di `test-paste-range` già documentato
sopra, indipendente da questa modifica).

### Migliorie di fedeltà XLSX (3/3): AutoFilter — righe nascoste, valori distinti, freccia a discesa

Terza e ultima voce del confronto file reale/Excel: `<autoFilter
ref="A8:N8"/>` (Dati > Filtro in Excel) non veniva importato in alcun
modo, né come dato né come aspetto visivo (le frecce a discesa
sull'intestazione della tabella).

**Righe nascoste, l'infrastruttura mancante**. Un filtro attivo in
Excel nasconde le righe escluse scrivendo `hidden="1"` sull'elemento
`<row>` corrispondente — lo stesso identico attributo usato per un
nascondimento manuale ("tasto destro sull'intestazione > Nascondi"),
Excel non li distingue nel file e nemmeno questo import. Atomo123 non
aveva alcun concetto di "riga nascosta": aggiunto `SheetView::
fRowHidden` (un array parallelo a `fRowHeights`, MAI sovrapposto ad
esso — un'altezza a zero sarebbe stata riportata al minimo da
`SetRowHeights`, e comunque Excel *ricorda* l'altezza vera di una riga
nascosta per quando torna visibile, non la scarta). `RebuildRowOffsets`
tratta una riga nascosta come alta zero pixel nella somma cumulativa,
esattamente come Excel: `SetHiddenRows`/`HiddenRows` (l'elenco sparso
usato dalla persistenza) e `IsRowHidden` sono l'API pubblica.

**AutoFilter vero e proprio**: `SheetView::SetAutoFilter`/
`ClearAutoFilter`/`HasAutoFilter`/`AutoFilterRange` tengono
l'intervallo (intestazione + colonne, `range`, `top==bottom` perché
l'intestazione è sempre una riga sola). `UniqueColumnValues(col)`
elenca i valori distinti di una colonna (in ordine di comparsa,
`GetCellResult`, lo stesso testo che comparirebbe nel menu di Excel).
`SetColumnValueHidden(col, valore, nascondi)` nasconde/mostra tutte le
righe con quel valore in quella colonna — i criteri di più colonne si
combinano in **AND**, come Excel vero (una riga resta nascosta se
esclusa da almeno una colonna filtrata), ricalcolando l'intera
visibilità (`RecomputeAutoFilterVisibility`) a ogni cambio invece di
toccare solo le righe coinvolte, altrimenti l'AND fra colonne non
sarebbe corretto. `ClearColumnFilters()` azzera tutti i criteri
("Mostra tutto"). **Limite dichiarato**: i criteri per colonna
(`fFilterHiddenValues`) non sono persistiti nel formato nativo — solo
il *risultato* (le righe nascoste) sopravvive al giro salvataggio/
ricarica; riaprire un file mostra di nuovo le righe giuste, ma il menu
non "ricorda" quali valori esatti le avevano escluse.

**Interfaccia**: una piccola freccia a discesa nell'angolo in basso a
destra di ogni cella di intestazione nell'intervallo del filtro
(`AutoFilterArrowRect`, usato sia da `Draw()` per disegnarla sia da
`MouseDown` per riconoscere il clic — stesso rettangolo, mai
duplicato). Un clic apre `ShowAutoFilterMenu`: un vero `BPopUpMenu`
**sincrono** (`Go(..., asynchronous=true)` bloccante, non
`deliversMessage` — restituisce direttamente la voce scelta, niente
passaggio di messaggi da gestire altrove) con una voce spuntabile per
valore distinto più "Mostra tutto". Limite dichiarato: un clic per
volta, il menu si chiude dopo ogni scelta (va riaperto per toccare un
altro valore) — stesso compromesso di semplicità già accettato altrove
in questo progetto per i dialoghi modali.

**Import XLSX**: `<row hidden="1">` (indipendentemente da `ht`/
`customHeight`) popola le righe nascoste; `<autoFilter ref="...">`
(riusa `ParseMergeCellRef`, già usata per `<mergeCell>`, stesso formato
"A1:B2") popola l'intervallo. Le condizioni già applicate
(`<filterColumn><filters>`) non si leggono: il risultato (righe
nascoste) basta a mostrare il foglio come in Excel.

**Due bug reali scoperti implementando questo, non ovvi da un solo
sguardo al codice**:

1. *Testo/etichette fantasma sulle righe nascoste.* `RebuildRowOffsets`
   collassa correttamente una riga nascosta a zero pixel, ma DUE punti
   del codice continuavano a usare la sua altezza VERA
   (`fRowHeights`, mai azzerata di proposito, vedi sopra) invece di
   trattarla come zero: l'etichetta della riga nell'intestazione (che
   sommava quell'altezza vera alla somma cumulativa già collassata)
   finiva spinta ben oltre la sua posizione corretta, sovrapposta ad
   altre etichette molto più sotto; il testo della cella (`DrawString`,
   mai vincolato all'altezza zero del proprio rettangolo come
   `FillRect`/`StrokeLine`) restava comunque disegnato, sovrapposto al
   testo della riga visibile che ne aveva preso il posto. **Scoperto
   su un file reale** (non nei test, che allora non coprivano righe
   nascoste): testo illeggibile, ammassato su poche righe, numeri di
   riga fuori ordine. Fix: `continue` esplicito per ogni riga nascosta
   in tutti i cicli per-riga di `DrawCellBand`/`Draw()` (sfondo,
   griglia, bordi, testo, etichette) — una riga nascosta non disegna
   più nulla, esattamente come Excel.
2. *La freccia a discesa non compariva mai.* Codice geometricamente
   corretto e sicuramente raggiunto (verificato passo passo con un
   programma di prova a parte, anche sostituendo `FillTriangle` con un
   `FillRect` enorme a copertura di quasi tutta la vista — sempre
   senza alcun effetto visibile, né offscreen né in una vera finestra).
   Causa: il ciclo che disegna il testo delle celle restringe il
   ritaglio dello schermo (`ConstrainClippingRegion`) al rettangolo
   della singola cella per ogni cella non vuota, ma non lo
   ripristinava mai né fra un'iterazione e l'altra né all'uscita dal
   ciclo — il ritaglio dell'ULTIMA cella disegnata restava attivo per
   tutto ciò che veniva disegnato dopo in quella stessa chiamata, mai
   scoperto prima perché la freccia era il primo codice a disegnare
   qualcosa dopo quel ciclo. Fix: `ConstrainClippingRegion(NULL)`
   esplicito all'uscita dal ciclo del testo.

Test: `ui/tests/test_autofilter.cpp`, nuovo — 25 controlli: valori
distinti/ordine, visibilità per valore, AND fra colonne, ripristino di
un singolo criterio senza toccare gli altri, "Mostra tutto",
`SetHiddenRows`/`HiddenRows`, geometria della freccia, e i due bug
sopra verificati a livello di pixel su bitmap offscreen (non solo
stato interno) — per ciascuno, disattivata temporaneamente la
correzione durante lo sviluppo per confermare che il test la scopra
davvero, poi ripristinata. `translators/xlsx/tests/sample.xlsx` esteso
con `<row r="2" hidden="1"/>` e `<autoFilter ref="A1:D1"/>`. Nessuna
regressione nelle 34 suite UI.

### Spostamento con il mouse delle immagini incorporate

Chiesto dall'utente ("posso spostare le immagini?") dopo aver
verificato l'AutoFilter: le immagini incorporate (import XLSX) erano
finora solo visualizzate, mai interattive.

`SheetView::fImages` non è più `const` (era di sola lettura, un
puntatore al vettore vero di `MainWindow`, mai una copia — vedi il
commento su `SetImages` in `SheetView.h`): un trascinamento scrive
direttamente `offsetX`/`offsetY` sull'elemento del vettore, quindi è
già "nel modello" a ogni fotogramma senza bisogno di ricopiare nulla
indietro verso `MainWindow` né di una sezione di persistenza nuova
(`offsetX`/`offsetY` viaggiano già dentro `EmbeddedImage`, già
salvate/ricaricate da tempo). `ImageFrame(img)` accentra la formula
già usata da `Draw()` per posizionare l'immagine (prima duplicata
inline), ora anche da `MouseDown`/`MouseMoved` per riconoscere clic e
trascinamento — stesso schema già in uso per `AutoFilterArrowRect`,
un solo posto per la formula così i punti che disegnano/riconoscono un
clic non possono disallinearsi.

Stesso principio del ridimensionamento riga/colonna già esistente
(indice + punto di partenza + valore di partenza, armati da
`MouseDown`, applicati da `MouseMoved`) — con una differenza voluta:
il ridimensionamento non segna mai il documento come modificato (una
preferenza di sola visualizzazione), spostare un'immagine sì
(`NotifyDocumentChanged()` in `MouseUp`, così il titolo mostra
l'asterisco e "Salva" scrive davvero la nuova posizione). Un clic in
una zona dove due immagini si sovrappongono afferra quella disegnata
sopra (l'ultima nell'elenco, ciclo di ricerca all'indietro in
`MouseDown`), non la prima trovata. Cursore a icona di spostamento
al passaggio del mouse sopra un'immagine, stesso principio già in uso
per il ridimensionamento (segnalato a suo tempo dall'utente: senza un
indizio visivo l'interazione non è scopribile guardando lo schermo).

Test: `ui/tests/test_image_drag.cpp`, nuovo — due immagini
sovrapposte apposta, verificato che un clic fuori non sposti nulla,
che la zona di sovrapposizione afferri quella sopra e non quella
sotto, che la posizione si aggiorni già al primo `MouseMoved` (non
solo al rilascio) e resti relativa al punto di *partenza* del
trascinamento (non incrementale), e che spostare un'immagine non
tocchi l'altra. Nessuna regressione nelle 35 suite UI.

### Ridimensionamento con il mouse delle immagini incorporate

Chiesto dall'utente ("posso ridimensionare le immagini?") subito dopo
aver verificato lo spostamento sul suo file reale: estensione naturale
dello stesso meccanismo, questa volta su `width`/`height` invece di
`offsetX`/`offsetY`.

`ImageResizeHandle(img)` (pubblico, stesso motivo di `ImageFrame`: un
solo posto per la formula, usato sia da `Draw()` per disegnare la
maniglia sia da `MouseDown` per riconoscere il clic) restituisce un
piccolo quadrato (8×8px) ancorato all'angolo in basso a destra di
`ImageFrame(img)`, sempre visibile — non solo al passaggio del mouse,
stesso motivo già scritto per i puntini di ridimensionamento
riga/colonna: senza un indizio visivo permanente l'interazione non
sarebbe scopribile guardando lo schermo.

La maniglia è un bersaglio più piccolo e più specifico del corpo
dell'immagine, quindi `MouseDown` la controlla PRIMA del trascinamento
di spostamento già esistente: un clic sulla maniglia ridimensiona,
un clic altrove nel corpo dell'immagine sposta — i due percorsi restano
distinti anche se la maniglia è geometricamente contenuta dentro
`ImageFrame`. Stesso schema esatto dello spostamento (indice + punto di
partenza + valore di partenza, armati da `MouseDown`, applicati da
`MouseMoved`), qui su `fResizingImageIndex`/`fResizeImageStart`/
`fResizeImageStartWidth`/`fResizeImageStartHeight` invece dei campi
equivalenti per lo spostamento — mai sotto un minimo di 10px (altrimenti
l'immagine sparirebbe insieme alla maniglia per riallargarla). Segna il
documento come modificato in `MouseUp` come lo spostamento (cambia
`width`/`height` nel modello, non solo una preferenza di
visualizzazione). Cursore a doppia freccia diagonale
(`B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST`) al passaggio del mouse
sopra la maniglia, controllato prima del cursore di spostamento
sull'intera immagine per lo stesso motivo di precedenza.

Test: `ui/tests/test_image_resize.cpp`, nuovo — verificato che un clic
fuori dall'immagine non la ridimensioni, che un clic sulla maniglia
avvii il ridimensionamento (non lo spostamento: `offsetX`/`offsetY`
restano invariati) e aggiorni `width`/`height` già al primo
`MouseMoved` restando relativo al punto di *partenza* del trascinamento
(non incrementale), che il ridimensionamento non tocchi un'altra
immagine, che non scenda mai sotto il minimo anche trascinando molto
oltre, e che un clic dentro l'immagine ma fuori dalla maniglia continui
a spostare invece di ridimensionare. Nessuna regressione nelle 36 suite
UI.

## Fase 13 — Colmare il divario con Excel (IN CORSO)

Con la Fase 5 chiusa e nessuna segnalazione pendente su file reali,
l'utente ha chiesto un confronto sistematico con Excel: cosa manca
davvero, in ordine di difficoltà crescente (dal più facile al più
difficile da realizzare in Atomo123, non per importanza). Elenco
compilato leggendo il codice reale (non a memoria): tabella funzioni
in `engine/src/Functions/`, grafici in `ui/src/Chart*`, pivot in
`ui/src/Pivot.h`, gestione fogli in `MainWindow.cpp`/`SheetTabView.cpp`,
formati file in `translators/`.

- [x] **Funzioni di testo/statistiche mancanti**: TRIM, UPPER/LOWER/
      PROPER, FIND/SEARCH, CONCAT (non `CONCATENATE`: il nome supera
      il limite di 9 caratteri della risorsa `Func`, vedi sotto),
      MEDIAN, MODE. `SUBSTITUTE` (10 caratteri) rimandato per lo
      stesso motivo. Puro lavoro nel motore, nessuna UI: stesso schema
      esatto di ogni funzione già presente in `engine/src/Functions/
      Functions.<categoria>.cpp`. `funcNr` (posizione nell'array della
      risorsa `Func`, deve combaciare con l'enum `kXXXFuncNr` in
      `Functions.h`) assegnati in coda, 89-97 dopo `AVERAGEIF`=88;
      `argCnt` nella risorsa impostato a `65535` (letto come `short`
      diventa `-1`, il segnale di "numero di argomenti variabile") per
      FIND/SEARCH (terzo argomento `start_num` opzionale), CONCAT
      (numero variabile di argomenti) e MEDIAN/MODE (come SUM/AVG/MAX).

      Bug reale scoperto e corretto implementando CONCAT: `ftoa`
      (`Formatter.cpp`, già usata da `NUM2C`) chiama internamente
      `BFont::StringWidth()`, che richiede una connessione app_server
      — senza una `BApplication` vera (come nel test del motore
      isolato, `engine/tests/named_functions_test.cpp`, che non ne
      crea una apposta per restare "isolato, testabile" come da scopo
      dichiarato di `engine/`) la chiamata resta bloccata
      indefinitamente in attesa di una risposta che non arriva mai —
      mai emerso prima perché nessuna funzione testata in quel file
      aveva mai avuto bisogno di convertire un numero in testo.
      `CONCAT` ora usa una conversione diretta con `snprintf("%.10g")`
      invece di appoggiarsi al formattatore grafico.

      Test: `engine/tests/named_functions_test.cpp` esteso con una
      formula per ciascuna delle nove funzioni (inclusi i casi FIND
      senza corrispondenza e SEARCH case-insensitive), verificato
      anche il conteggio totale (`gFuncCount == 98`). Nessuna
      regressione nelle altre suite del motore (`test`, `test-names`,
      `test-xsheet`) né nelle 36 suite UI.
- [x] **Nuovo/Elimina/Rinomina foglio**: "Nuovo foglio" nel menu Dati
      (nome libero scelto da solo, "Foglio1"/"Foglio2"/... come
      Excel/LibreOffice Calc); Rinomina/Elimina dal tasto destro sulla
      scheda (`SheetTabView`, nuovo menu contestuale sincrono, stesso
      principio di `SheetView::ShowAutoFilterMenu`). Conferma con un
      vero `BAlert` per l'eliminazione (rifiuta di eliminare l'unico
      foglio rimasto); `RenameSheet` rifiuta un nome già usato da un
      altro foglio (avrebbe reso ambiguo `NomeFoglio!Cella`,
      `ISheetResolver` cerca per nome esatto). `DeleteSheetNoConfirm`
      esposto pubblicamente accanto a `DeleteSheet` apposta per
      restare testabile senza mostrare un vero dialogo di conferma,
      che bloccherebbe un test automatico in attesa di un clic reale
      (stesso limite già noto per `ConfirmDiscardChanges`, vedi
      `test_unsaved_changes.cpp`).
- [x] **Commenti/note sulla cella**: nuovo `std::map<cell, std::string>`
      sparso in `CContainer` (`fComments`, stesso schema di
      `fMergedRanges` per le celle unite in Fase 12 — la stragrande
      maggioranza delle celle non avrà mai un commento, non ha senso
      un campo diretto in `CellData` per ognuna). `std::string`, non
      `BString`: `Cell.h`/`Range.h`/`CellData.h`/`Container.h` non
      hanno mai avuto una dipendenza dal Support Kit, conversione a
      `BString` solo al confine con la UI. Editing tramite una vera
      finestra `CommentWindow` (stesso schema di `GoToWindow`/
      `RenameSheetWindow`: un `BTextView` multiriga più "Salva"/
      "Rimuovi commento", raggiungibile da Formato → "Commento
      cella…"). Indicatore visivo: piccolo triangolo rosso nell'angolo
      in alto a destra della cella, come Excel/LibreOffice Calc.
      Persistenza nel formato nativo (sezione in coda a `SaveASCD`/
      `LoadASCD`, stesso schema di AutoFilter in Fase precedente) e
      sezione sempre-vuota nell'export XLSX (`WriteASCD` locale di
      `XlsxTranslator.cpp`): l'export scrive solo valori calcolati,
      mai formule né metadati come i commenti, stessa scelta già presa
      per le formule.

      Problema reale scoperto implementando questo punto — non un bug
      di codice ma di build incrementale, inizialmente scambiato per
      un blocco indefinito dentro un semplice inserimento in una
      `std::map` vuota
      (`fComments[c] = text`): `engine/libengine.a` non era stata
      ricompilata dopo l'ultima modifica a `Container.h`, quindi il
      costruttore di `CContainer` compilato nella libreria allocava
      ancora il `sizeof` *precedente* (senza `fComments`), mentre gli
      oggetti UI ricompilati (`MainWindow.o` ecc.) scrivevano secondo
      il nuovo layout più grande — un `new CContainer(...)` che
      allocava troppo poco, con conseguente corruzione dell'heap alla
      prima scrittura oltre il buffer realmente allocato. Il Makefile
      non traccia le dipendenze dagli header del motore per gli
      oggetti della UI, quindi un cambiamento come questo richiede una
      ricompilazione pulita di `engine/` prima di quella di `ui/`.

      Test: `ui/tests/test_comments.cpp`, `SetCellComment`/
      `RemoveCellComment`/`CellComment` resi pubblici apposta (stesso
      principio di `NewSheet`/`RenameSheet`), verifica funzionale
      (imposta/sostituisce/rimuove, stringa vuota equivale a nessun
      commento), round-trip nel formato nativo
      (`SaveASCD`/`LoadASCD`), e verifica sui pixel veri del
      triangolo rosso tramite bitmap offscreen (stesso principio già
      usato in `test_merge_click.cpp`/`test_autofilter.cpp` per non
      dare per scontato che "il codice per disegnare è stato eseguito"
      equivalga a "si vede davvero"). Nessuna regressione nelle altre
      suite del motore, nei tre traduttori (XLSX/ODS/CSV) né nelle
      altre 39 suite UI.
- [x] **Collegamenti ipertestuali**: `std::map<cell, std::string>`
      separato in `CContainer` (`fHyperlinks`), stesso schema esatto
      dei commenti — inclusa la persistenza nel formato nativo (nuova
      sezione in coda, dopo quella dei commenti) e la sezione
      sempre-vuota nell'export XLSX. Editing con una vera finestra
      `HyperlinkWindow` (stesso schema di `CommentWindow`, ma un
      `BTextControl` a una riga invece di un `BTextView` multiriga,
      raggiungibile da Formato → "Collegamento ipertestuale…"), con un
      pulsante "Apri" per lanciare subito l'URL corrente senza dover
      prima salvare. Apertura vera e propria con
      `BUrl::OpenWithPreferredApplication()` (Support Kit, nessuna
      libreria aggiuntiva) su Ctrl+click sulla cella — non un click
      semplice: renderebbe altrimenti impossibile selezionare o
      modificare una cella con un URL senza lanciare ogni volta il
      browser, stessa scelta di LibreOffice Calc (Excel apre invece
      con un click semplice, scartato apposta per questo motivo).
      Indicatore visivo: testo blu e sottolineato, come Excel/
      LibreOffice Calc — solo per il disegno (non tocca
      `CellStyle::fHighColor`/`fUnderline` davvero memorizzati, cosi'
      rimuovere il collegamento in seguito restituisce l'aspetto
      originale della cella), e solo se la cella ha gia' un valore
      visibile: un collegamento su una cella vuota non mostra nulla,
      stesso comportamento di Excel (serve un testo su cui applicare
      il colore).

      Test: `ui/tests/test_hyperlinks.cpp`, `SetCellHyperlink`/
      `RemoveCellHyperlink`/`CellHyperlink` resi pubblici apposta
      (stesso principio dei commenti); `OpenCellHyperlink` NON
      testato: lancia davvero l'applicazione preferita per l'URL,
      stesso limite gia' noto di `DeleteSheet`/`RenameSheet` con un
      vero `BAlert`. Verifica funzionale, round-trip nel formato
      nativo, e verifica sui pixel veri del testo blu tramite bitmap
      offscreen. Insidia scoperta scrivendo quest'ultima parte:
      l'antialiasing subpixel dei font produce, anche sui bordi di
      testo NERO del tutto normale, pixel con una leggera frangia
      colorata (es. component blu alto ma verde altrettanto alto) --
      una soglia larga "componente blu alta" avrebbe scambiato quella
      frangia per il blu del collegamento (falso positivo osservato
      proprio su una cella senza nessun collegamento); corretto
      restringendo la soglia a una fascia stretta intorno al colore
      esatto (20,80,200) su tutti e tre i canali. Nessuna regressione
      nelle altre suite del motore, nei tre traduttori né nelle altre
      40 suite UI.
- [x] **INDEX/MATCH**: `INDEXFunction`/`MATCHFunction` nuove in
      `Functions.spreadsheet.cpp` (funcNr 99/100, gruppo 1 "lookup"
      come VLOOKUP/HLOOKUP). Insidia scoperta subito: il motore aveva
      GIA' `HINDEX`/`VINDEX` (Sum-It storico, funcNr 28/76), ma
      nonostante il nome fanno una ricerca in stile MATCH
      approssimato ("posizione del primo valore ≥ chiave", solo
      ascendente) — non l'INDEX vero di Excel ("valore alla posizione
      N"). Nomi fuorvianti rispetto alla convenzione moderna, lasciati
      intatti (codice storico funzionante, altri fogli potrebbero
      già farci affidamento) e implementate `INDEX`/`MATCH` come
      funzioni nuove e distinte invece di riusarli o rinominarli.

      `INDEX(intervallo, riga, [colonna])`: riga/colonna 1-based
      relative all'angolo in alto a sinistra dell'intervallo. Se
      l'intervallo e' largo una sola riga (o una sola colonna) e
      viene passato un unico argomento numerico, quello seleziona la
      dimensione libera (riga=1 fissa se l'intervallo e' orizzontale,
      e viceversa) — stesso comportamento "opzionale" del vero INDEX
      di Excel. Se riga o colonna vale 0 (esplicito o omesso su un
      intervallo davvero bidimensionale), il risultato e' l'INTERA
      riga/colonna corrispondente come intervallo, non un valore
      singolo — stesso principio gia' usato da `OFFSET` (un `Value`
      di tipo range che una funzione che aggrega, es. `SUM`, consuma
      direttamente: `=SUM(INDEX(A1:C10,0,2))` somma tutta la seconda
      colonna).

      `MATCH(valore, intervallo, [tipo])`: restituisce la POSIZIONE
      relativa (1-based), non il valore — l'intervallo deve essere
      largo una sola riga o una sola colonna. `tipo=0`: corrispondenza
      esatta (non richiede un intervallo ordinato). `tipo=1`
      (predefinito, come Excel): ultimo valore ≤ chiave, intervallo
      assunto crescente. `tipo=-1`: primo valore ≥ chiave, intervallo
      assunto decrescente — nessun controllo esplicito
      dell'ordinamento nei casi approssimati, stessa assunzione gia'
      fatta da VLOOKUP/HLOOKUP.

      Test: `engine/tests/named_functions_test.cpp` (conteggio
      funzioni 99→101), nove formule nuove — `INDEX` su una colonna
      sola, su un intervallo bidimensionale con riga/colonna
      esplicite, su una riga sola (l'unico argomento come colonna),
      con riga 0 consumato da `SUM`; `MATCH` esatto su numeri e testo,
      su una riga sola, senza corrispondenza, approssimato; infine
      `INDEX`+`MATCH` combinati (l'uso reale piu' comune: cerca una
      chiave in una colonna e restituisce il valore corrispondente in
      un'altra). Nessuna regressione nelle altre suite del motore, nei
      tre traduttori né nelle altre 41 suite UI.
- [ ] **Altri tipi di grafico** (linee, torta): `Chart.h`/`ChartView.cpp`
      hanno già l'infrastruttura per i grafici a barre; ogni nuovo tipo
      riusa il framework di disegno ma richiede nuova geometria
      dedicata.
- [ ] **Stili di bordo/colore** (oggi solo presenza/assenza, nero
      fisso): l'infrastruttura c'è già (`CellStyle`), ma tocca disegno,
      persistenza e import XLSX in più punti.
- [ ] **Validazione dati** (elenco a discesa, intervallo numerico):
      nuova finestra di dialogo sullo stile di `PreferencesWindow`/
      `ColorWindow` già esistenti, più un controllo all'editing della
      cella.
- [ ] **Formattazione condizionale viva**: oggi valutata una volta
      sola all'import XLSX e congelata come colore statico (Fase 12);
      richiede ricalcolarla a ogni ricalcolo del foglio, non solo
      leggerla.
- [ ] **Tabella pivot avanzata**: quella attuale in `Pivot.h` è
      volutamente minimale (un solo livello di raggruppamento, solo
      Somma/Conteggio/Media); un vero pivot con più livelli, più
      misure, filtri, richiede in pratica riscriverla.
- [ ] **Formule array** (Ctrl+Maiusc+Invio, spill): tocca il modello
      di valutazione del motore, non un'aggiunta incrementale.
- [ ] **Goal Seek / Solver**: serve un risolutore numerico iterativo
      nuovo di zecca, oltre alla UI.
- [ ] **Scrittura del formato XLS legacy (BIFF/OLE2)**: oggi solo
      import; scrivere BIFF8 da zero è un formato binario notoriamente
      ostico, nessuna libreria su cui appoggiarsi (per questo finora
      escluso deliberatamente, vedi Fase 5).
- [ ] **Macro/VBA**: richiederebbe un motore di scripting incorporato
      — il salto più grande di tutti, sostanzialmente un sottoprogetto
      a sé.

Nota: AutoFilter e Blocca riquadri, inizialmente segnalati come
mancanti da una prima analisi automatica del codice, sono in realtà
già implementati (vedi le sezioni sopra) — tolti dall'elenco.

### Bug scoperto: un testo tipo codice ("01.11.10") spariva del tutto riaprendo il file

Segnalato dall'utente aprendo una tabella di codici ATECO reale
(screenshot): la colonna dei codici a sei cifre (formato
"01.11.10") risultava completamente vuota, mentre la colonna delle
descrizioni accanto si vedeva bene.

Diagnosi con un programma di ispezione dedicato (compilato al volo
contro `translators/xlsx/XlsxTranslator.cpp` + `ui/src/AscdIO.cpp`,
stesso principio già in uso per gli altri bug reali di questa
sessione): il testo veniva importato CORRETTAMENTE dal file XLSX
(confermato leggendo il valore subito dopo `TryToParseString` dentro
`ParseSheet`) e scritto CORRETTAMENTE nel formato nativo intermedio da
`WriteASCD` — la perdita avveniva solo in lettura, in `LoadASCD`
(`ui/src/AscdIO.cpp`, chiamata sempre subito dopo qualunque
import/apertura, anche di un file XLSX: `MainWindow::OpenFile` traduce
prima in nativo con `BTranslatorRoster`, poi rilegge quei byte con
`LoadASCD`/`LoadASCDBook`).

Causa: un testo come "01.11.10" (tre gruppi di cifre separati da
punti) somiglia abbastanza a un'espressione numerica da superare
l'analisi grammaticale iniziale di `CParser::Parse` (usata da
`TryToParseString`, `engine/src/Cell/CellParser.cpp`), ma poi fallisce
a ridursi a un valore vero e proprio — a quel punto
`TryToParseString`, chiamata con `inWarnIfError=true`, RILANCIA
l'eccezione invece di ripiegare sul testo originale (il ripiegamento
esiste già nel codice, ma solo quando `inWarnIfError=false`). `LoadASCD`
intercetta quell'eccezione con un `catch (...)` pensato per "una
singola cella corrotta non deve far fallire l'intero caricamento" — ma
invece di mostrare comunque il testo, la cella spariva senza lasciare
traccia. Verificato con un secondo programma minimo che chiama
`TryToParseString("01.11.10", ..., true)` direttamente: l'eccezione
"Syntax error?" si propaga per davvero, confermando il meccanismo.

Le tre copie locali di questa stessa funzione nei translator (XLSX,
ODS, CSV — duplicate perché quei translator non linkano contro
`ui/src/`, vedi il commento in cima a `WriteASCD` in ciascun file)
hanno lo stesso identico bug nella direzione opposta (esportare un
file nativo verso un formato esterno): lì il `catch` trasformava
l'intero export in un fallimento totale (`B_BAD_DATA`) per colpa di
una sola cella di testo innocua, invece di limitarsi a perderla.

Fix: le quattro chiamate (`LoadASCD` più le tre copie locali)
usano ora `inWarnIfError=false`, lo stesso valore già usato con
successo da `ParseSheet` nei tre translator quando importano testo
direttamente dal formato esterno (XLSX/ODS/CSV) — un parse ambiguo
ripiega sempre sul testo originale invece di sparire o abortire tutto.

Test: nuovo blocco in `ui/tests/test_ascd_io.cpp` — un giro
salva→ricarica di tre celle di testo ("01.11.10", "01.12.00",
"CODICE"), verificato che tutte e tre sopravvivano intatte. Verificato
anche dal vivo riaprendo il file reale con l'app: la colonna dei codici
ATECO ora si vede correttamente. Nessuna regressione nelle suite del
motore, dei tre translator, né nelle 36 suite UI.

### Bug scoperto: un clic su una cella unita si comportava come se non fosse mai stata unita

Segnalato dall'utente sullo stesso file reale (screenshot): cliccando
su D4, angolo in alto a sinistra di un intervallo unito D4:F4, il
riquadro di selezione disegnato restava largo una sola colonna invece
di estendersi a tutto l'intervallo — visivamente indistinguibile da
una cella mai unita.

Indagando è emerso un secondo bug, più subdolo, nello stesso punto:
`SheetView::MouseDown` calcola la cella cliccata con `CellAt(where)`,
che restituisce sempre la cella *fisica* sotto il puntatore, senza mai
controllare se fa parte di un intervallo unito — un clic su una
qualunque cella "nascosta" sotto la fusione (es. E4 o F4 dell'intervallo
D4:F4, che non portano mai contenuto/stile proprio) selezionava quella
cella vuota invece dell'angolo D4, che è l'unica a contenere davvero il
valore. La barra della formula sarebbe apparsa vuota per un clic del
genere, rinforzando l'impressione "mai unita".

Fix, due parti indipendenti:
1. `MouseDown` ora controlla `CContainer::GetMergedRange` sulla cella
   cliccata e, se fa parte di un intervallo, sostituisce la cella con
   il suo angolo in alto a sinistra prima di procedere alla selezione
   — stesso principio già in uso per il disegno del contenuto (vedi
   Fase 12), qui esteso alla selezione.
2. Il riquadro di selezione in `Draw()` (`activeRect`/`selOuter`), per
   il caso comune di una selezione a singola cella che risulta essere
   l'angolo di un intervallo unito, si estende ora all'intero
   intervallo (`CellRect` dell'angolo unito con quello opposto, stesso
   schema già in uso per il rettangolo del testo). Limitato
   deliberatamente al caso di singola cella: un vero trascinamento
   multi-cella che si sovrappone solo in parte a un intervallo unito è
   un caso limite già complesso in Excel stesso.

Test: nuovo `ui/tests/test_merge_click.cpp` — verificato che un clic
sull'angolo D4 selezioni D4, che un clic su F4 (nascosta) selezioni
comunque D4 (non F4), che una cella normale fuori da qualunque
intervallo continui a selezionare se stessa, e — su una bitmap
offscreen, leggendo davvero i pixel del bordo blu di selezione
(30,100,200), stesso principio già in uso in `test_autofilter.cpp` per
lo stesso genere di bug "geometricamente corretto ma mai verificato
sui pixel veri" — che il riquadro arrivi fino al bordo destro
dell'intero intervallo unito e non più al bordo della sola cella D4.
Disabilitato temporaneamente il fix durante lo sviluppo per confermare
che il test lo scopra davvero (3 controlli su 6 falliscono senza),
poi ripristinato. Nessuna regressione nelle 37 suite UI.

### Bug scoperto: un riquadro "fantasma" restava sullo schermo spostando la selezione via da una cella unita

Segnalato dall'utente subito dopo il fix precedente, con un nuovo
screenshot sullo stesso file: più riquadri blu comparivano
contemporaneamente in punti diversi del foglio, come se più celle
fossero "selezionate" insieme (impossibile nel modello a singola
selezione di questa app).

Causa: il fix precedente ha esteso il riquadro *disegnato* da `Draw()`
per una cella unita, ma non l'area *invalidata* quando la selezione si
sposta — `SetSelection`/`ExtendSelection` continuavano a invalidare
solo `PinnedCellRect` grezzo (una singola cella), mai l'intervallo
esteso davvero disegnato. Spostando la selezione via da una cella
unita, la parte del vecchio riquadro fuori dalla stretta zona
invalidata non veniva più ridisegnata dall'Interface Kit (che ridisegna
solo l'area invalidata, non l'intero schermo) — restava visibile come
residuo, un "fantasma" del riquadro precedente.

Fix: estratto `ActiveCellRect(cell)` (pubblico, stesso principio di
`ImageFrame`/`AutoFilterArrowRect` — un solo posto per la formula) che
restituisce il rettangolo di una cella, esteso a tutto l'intervallo se
è l'angolo di una cella unita. Usato ora sia da `Draw()` (rimuove la
logica duplicata inline del fix precedente) sia da
`SetSelection`/`ExtendSelection`, che invalidano l'unione del
rettangolo attivo *prima* e *dopo* lo spostamento — così qualunque area
mai disegnata come riquadro esteso viene sempre ripulita.

Test: `ui/tests/test_merge_click.cpp` invariato nel numero di
controlli (il refactor non cambia il comportamento già verificato),
ma il meccanismo di invalidazione in sé non è verificabile con lo
stesso principio delle bitmap offscreen usato altrove in questa
sessione — nei test `Draw()` viene sempre chiamata a mano con un
rettangolo esplicito, senza mai passare dal meccanismo reale di
regione invalidata/ridisegno parziale dell'Interface Kit (che esiste
solo per una vera finestra sullo schermo). Corretto per costruzione
(un'unione, mai una riduzione, dell'area già invalidata prima del fix
— non può quindi invalidare "troppo poco"). Non è stato possibile
ottenere una verifica visiva dal vivo per questo punto specifico: la
finestra dell'app restava dietro altre finestre dell'utente sul
desktop condiviso, senza un modo affidabile per riportarla in primo
piano da riga di comando in questa sessione. Nessuna regressione nelle
37 suite UI.

### Bug scoperto: le formule non mostravano mai il simbolo "=" iniziale

Domanda dell'utente su un file reale ("è normale che le formule non
abbiano il = all'inizio?"), dopo aver confermato che le celle unite
ora funzionano correttamente — risposta: no, non è normale, è un
interruttore storico mai davvero collegato a nulla.

Causa: `CFormula::UnMangle` (`engine/src/Formula/Formula.cpp`), la
funzione che ricostruisce il testo di una formula per mostrarla,
antepone "=" solo `if (gWithEqualSign && !rcStyle)`. `gWithEqualSign`
è un `bool` globale dichiarato ma **mai impostato a `true` da nessuna
parte in tutto il codice** — resta al valore predefinito C++ per un
bool globale (`false`), un interruttore ereditato da Sum-It storico
mai davvero collegato a un'impostazione o a un default sensato durante
il porting. Il risultato: ogni formula, in ogni cella, veniva sempre
mostrata identica a un numero/testo normale — non un'abitudine di
digitazione dell'utente (il motore accetta comunque "=" opzionale in
ingresso), ma un comportamento fisso di tutta l'app in uscita.

Fix, deliberatamente **non** la riattivazione del flag globale: quello
influenzerebbe uniformemente OGNI chiamante di `GetCellFormula`,
inclusi l'export XLSX/ODS (l'elemento `<f>` di quei formati non deve
mai avere "=", per specifica ECMA-376/OpenDocument — scriverlo
avrebbe rischiato di produrre file che Excel/LibreOffice veri
interpretano male) e il testo scritto da `SaveASCD`/round-trip interno
(dove non serve). Aggiunto "=" solo nei due punti dove l'utente legge
davvero il contenuto di una cella come formula:
`MainWindow::SelectionChanged` (barra della formula) e
`SheetView::StartEditing` (editing in-cella al doppio clic) — usando
`CContainer::GetCellFormula(cell)` (l'overload che restituisce il
puntatore `void*` grezzo, `NULL` se non è una formula) per distinguere
una vera formula da un numero/testo normale, non `CellHasFormula`
(che restituisce `true` anche per una cella vuota/inesistente, un
comportamento pensato per altri usi interni, sbagliato qui).

Test: nuovo `ui/tests/test_formula_display.cpp`, con una vera
`MainWindow` (`FormulaBarText()` esposto pubblicamente apposta) —
verificato che una formula mostri "=A1+B1", un numero semplice mostri
"10" senza "=", un testo normale resti invariato, una cella vuota
resti vuota. Disabilitato temporaneamente il fix durante lo sviluppo
per confermare che il test lo scopra davvero, poi ripristinato.
Nessuna regressione nelle 38 suite UI.

### Bug scoperto: quattro cause distinte dietro lo stesso sintomo — formule XLSX mostrate come testo grezzo

Segnalato dall'utente confrontando side-by-side lo stesso file aperto
in Excel vero e in Atomo123 (due screenshot): celle con `IFERROR`,
`IF` e `VLOOKUP` mostravano il testo grezzo della formula (a volte
tagliato/sovrapposto fra celle vicine, come un testo lungo qualunque)
invece del valore calcolato — "IO vedo ancora molta differenza tra i
due programmi".

Diagnosi in isolamento, un programma di prova alla volta (stesso
principio già in uso in questa sessione per gli altri bug reali):
ricostruite esattamente le formule del file reale (`IFERROR(M27*.../
1000/$L$10,0)`, `IF(M35<>"NO",VLOOKUP($U$10,'CLUSTER Monitoraggio'!
$A$2:$F$12,4,0),"")`) e verificate passo passo con `TryToParseString`
diretto, bypassando l'import XLSX per isolare dove esattamente si
rompeva l'analisi grammaticale. Quattro bug reali distinti, tutti
convergenti sullo stesso sintomo (parse fallito → ripiego sul testo
grezzo della formula, comportamento di sicurezza già corretto nella
sessione precedente per lo stesso motivo):

1. **Separatore degli argomenti mai esplicito nell'import XLSX**: il
   traduttore chiamava `TryToParseString(text, loc, doc, false)` senza
   mai passare `decSep`/`listSep`, quindi usava i valori globali
   `gDecimalPoint`/`gListSeparator` — `';'` di default per l'Italia
   (`App.cpp`). Ma il testo di `<f>` in un file XLSX è **sempre**
   nel formato canonico ECMA-376 (virgola fra gli argomenti, punto per
   i decimali), indipendente dalla lingua con cui è stato scritto in
   Excel — un Excel italiano mostra "=SE(A1>5;100;200)" nella barra
   della formula ma salva sempre "=IF(A1>5,100,200)" nel file. Con
   `gListSeparator=';'` **ogni** formula con più di un argomento
   (`IF`, `VLOOKUP`, `SUMIF`, praticamente qualunque funzione non
   banale) falliva l'analisi grammaticale. Probabilmente il bug reale
   di più ampio impatto scoperto in questa sessione: non specifico di
   una funzione, ma di ogni formula multi-argomento importata da XLSX.
   Fix: `TryToParseString(text.c_str(), loc, ctx->doc, false, '.', ',')`
   in `XlsxTranslator::ParseSheet` — esplicito, non più legato al
   locale dell'utente.
2. **"IFERROR" non esisteva nella tabella funzioni**: solo "IFERR"
   (nome storico di Sum-It, mai usato da un file XLSX vero) era
   registrato. Aggiunta "IFERROR" come nuova voce in
   `engine/resources/funcs_by_nr.r` (`funcNr` 98), stessa
   `IFERRFunction` di "IFERR" — non una nuova implementazione, solo il
   nome standard che un file reale usa davvero.
3. **VLOOKUP/HLOOKUP con `argCnt=3` ESATTO nella risorsa**: il vero
   VLOOKUP di Excel ha un quarto argomento opzionale (corrispondenza
   esatta/approssimata, `0`/`FALSE` per esatta) che un file reale usa
   quasi sempre esplicitamente — un parser che rifiuta 4 argomenti
   fa fallire l'intera formula. `argCnt` cambiato a `65535`
   (variabile, stesso convenzione già in uso per `SUMIF`/`CONCAT`/
   ecc.) per entrambe; `VLOOKUPFunction`/`HLOOKUPFunction` ora leggono
   davvero il quarto argomento (prima veniva semplicemente ignorato,
   la ricerca era **sempre** approssimata/su intervallo ordinato,
   anche quando il file chiedeva esplicitamente una corrispondenza
   esatta su un intervallo non ordinato — avrebbe potuto restituire
   silenziosamente la riga sbagliata).
4. **Bug indipendente, scoperto verificando il punto 3 con valori
   noti**: l'aritmetica dello scarto di colonna/riga di
   `VLOOKUP`/`HLOOKUP` era sfasata di uno (`c.h += offset` invece di
   `c.h += offset - 1`, dato che `c.h` parte già sulla prima colonna
   dell'intervallo) — `VLOOKUP(...,2,...)` restituiva sempre il
   valore della **terza** colonna invece della seconda. Bug
   pre-esistente (non introdotto in questa sessione), mai notato prima
   perché nessun test aveva mai controllato il valore *vero*
   restituito da un `VLOOKUP`/`HLOOKUP` con offset ≠ 1, solo l'assenza
   di crash.

Test: nuovi controlli in `engine/tests/named_functions_test.cpp`
(conteggio funzioni aggiornato a 99, verifica diretta di `IFERROR`/
`VLOOKUP` con corrispondenza esatta e colonna giusta/sbagliata/
`HLOOKUP`/`IF` con virgole esplicite). Nuovo
`translators/xlsx/tests/sample_formulas.xlsx` — fixture ZIP minima
scritta a mano con uno script Python (nessun Excel/LibreOffice
disponibile per generarla, stesso principio delle altre fixture di
questo file) con le tre formule reali (`IF`/`VLOOKUP`/`IFERROR`),
verificate end-to-end attraverso l'intero `XlsxTranslator::Translate`.
Scrivendo quest'ultimo test è emerso un buco ulteriore:
`test_xlsx_translator.cpp` non aveva **mai** chiamato `InitFunctions()`
in tutta la sua storia — senza, `GetFunctionNr` tratta ogni nome di
funzione come identificatore sconosciuto (`gFuncCount` resta 0), quindi
nessuna funzione con nome (non solo quelle di questo fix) veniva mai
davvero calcolata in questo file di test, solo importata come testo —
mai notato perché nessun controllo precedente aveva mai verificato il
*valore calcolato* di una formula con funzione con nome, solo stile e
formattazione. Aggiunta la stessa risorsa `'Func'` di
`named_functions_test.cpp` (nuovo target Makefile `named_functions.rsrc`).
Nessuna regressione nelle suite del motore, dei quattro translator, né
nelle 38 suite UI.

### Aggiunta: Nuovo/Elimina/Rinomina foglio

Secondo punto della Fase 13 (dopo le funzioni di testo/statistiche):
assente fin qui, confermato leggendo il codice (`MainWindow.cpp` aveva
un commento esplicito "in futuro" proprio su questo punto).

"Nuovo foglio" nel menu Dati: sceglie da solo il primo nome libero
("Foglio1", "Foglio2", ... come Excel/LibreOffice Calc, mai un nome
già usato — `UniqueSheetName`), lo aggiunge in coda e ci passa subito,
riusando `SwitchToSheet` per la sincronizzazione UI → `fSheets` invece
di duplicarla. Rinomina/Elimina invece si scelgono con un clic destro
sulla scheda del foglio (nuovo menu contestuale in `SheetTabView`,
stesso schema sincrono già in uso per `SheetView::ShowAutoFilterMenu`:
`BPopUpMenu::Go` blocca finché l'utente non sceglie, nessun passaggio
di messaggi da gestire altrove) — un elenco/indice da scegliere in un
menu fisso sarebbe stato ridondante, dato che il bersaglio è già
scelto dal clic stesso. "Rinomina" apre una piccola finestra dedicata
(`RenameSheetWindow`, stesso schema esatto di `GoToWindow`: un campo
di testo precompilato col nome attuale più un pulsante) invece di
un'editing in-place della scheda, per restare nello scope di questo
punto senza toccare `SheetTabView::Draw()`.

Vincoli, entrambi con un vero `BAlert`: non si può eliminare l'unico
foglio rimasto, e non si può rinominare un foglio con un nome già
usato da un altro (avrebbe reso ambiguo `NomeFoglio!Cella` nelle
formule — `ISheetResolver::ResolveSheetByName` cerca per nome esatto,
il primo che trova). Eliminare il foglio *attivo* passa prima a un
altro foglio valido (`SwitchToSheet`, così tutta la sincronizzazione
UI è già gestita) e solo dopo rimuove quello vecchio, evitando di
lasciare `fDoc`/`fSheetView` a puntare a un `CContainer` appena
rilasciato.

`DeleteSheetNoConfirm` esposto pubblicamente accanto a `DeleteSheet`
apposta per essere testabile: `DeleteSheet` mostra un vero `BAlert` di
conferma, che bloccherebbe un test automatico in attesa di un clic
reale — stesso limite già noto e documentato per
`ConfirmDiscardChanges` in `test_unsaved_changes.cpp`. Per lo stesso
motivo, il caso "nome già usato" di `RenameSheet` (che mostra un
`BAlert` di errore) non è coperto dal test automatico, solo il
percorso normale.

Test: nuovo `ui/tests/test_sheet_management.cpp`, con una vera
`MainWindow` — verificato che `NewSheet` scelga nomi liberi progressivi,
che scrivere in una cella del foglio appena creato/rinominato funzioni
normalmente (nessuna corruzione di `fDoc` durante le due operazioni),
che eliminare un foglio non attivo non sposti la selezione, che
eliminare il foglio attivo la sposti su un foglio ancora valido, e che
l'unico foglio rimasto non si possa eliminare. Nessuna regressione
nelle 39 suite UI.

---

Ogni fase, a completamento, aggiorna questo file (checkbox + eventuale
nuova sotto-fase emersa) e `docs/PORTING_NOTES.md`/`docs/ENGINE_API.md`
pertinenti, prima di iniziare la fase successiva.
