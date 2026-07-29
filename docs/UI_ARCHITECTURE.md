# Applicazione nativa (`ui/`)

Prima versione dell'applicazione Interface/Layout Kit (Fase 4), scritta
da zero (non riusa `CellView`/`CellWindow` BeOS-era) sopra il motore di
calcolo isolato (`engine/`) e il Translation Kit per l'interoperabilità
file (`translators/`).

## Struttura

```
ui/src/App.h/.cpp          BApplication: crea la finestra, inoltra i
                            file aperti da Tracker/riga di comando
ui/src/MainWindow.h/.cpp   BWindow: menu File/Modifica, barra formule,
                            cella corrente, apertura/salvataggio file
ui/src/SheetView.h/.cpp    BView custom: griglia, selezione, editing
ui/src/AscdIO.h/.cpp       Lettura/scrittura del formato nativo ASCD
                            (stessa logica duplicata nei translator,
                            vedi docs/TRANSLATORS.md)
```

`SheetView` non usa `BGridLayout`: la griglia è disegnata a mano in
`Draw()` sopra una `BScrollView`, con calcolo manuale del range di
scroll (`FixupScrollBars()`, agganciato a `BView::ScrollBar()`) — un
foglio di calcolo ha celle sparse in un intervallo enorme
(`kColCount`/`kRowCount` da `engine/src/Config/Constants.h`, 702×16384),
non un numero fisso di sotto-view come `BGridLayout` si aspetta.

## Apertura file: Translation Kit come consumer reale

`MainWindow::OpenFile()` passa ogni file da aprire per
`BTranslatorRoster::Default()->Translate(&file, NULL, NULL, &ascd,
kAtomoNativeFormat)`: il roster interroga ogni translator installato
(`~/config/non-packaged/add-ons/Translators/`) chiamando il suo
`Identify()` finché uno riconosce il formato, poi lo traduce in ASCD —
lo stesso translator CSV riconosce anche l'ASCD nativo tramite la
firma, quindi un unico punto di codice apre CSV/XLS/XLSX/ODS/ASCD
senza bisogno di if-else per estensione. I translator vanno installati
separatamente (`make install` in ciascuna cartella sotto
`translators/`) perché sono add-on caricati a runtime, non linkati
staticamente in questo binario — è la Fase 3 che diventa un
prerequisito runtime reale della Fase 4, non solo una libreria
compilata insieme.

## Editing: barra formule ed editor in-cella

Due percorsi verso la stessa logica di scrittura
(`TryToParseString`/`CalcCell`):

- **Barra formule** (`MainWindow`): sempre visibile, mostra/modifica
  la formula della cella selezionata, Invio conferma
  (`MainWindow::CommitFormulaBar`).
- **Editor in-cella** (`SheetView::StartEditing`/`CommitEditing`):
  doppio click su una cella (rilevato dal campo `"clicks"` del
  messaggio di mouse down corrente, `Window()->CurrentMessage()`) o
  si inizia a digitare direttamente mentre una cella è selezionata
  (il carattere digitato sostituisce il contenuto, come Excel/
  LibreOffice Calc) aprono un `BTextControl` temporaneo posizionato
  sopra la cella con `CellRect()`. Invio conferma (il `BTextControl`
  invoca il proprio messaggio sul target, impostato a `SheetView`
  stesso); un click altrove conferma prima di cambiare selezione
  (`MouseDown` chiama `CommitEditing(false)` se un editor è attivo);
  Escape annulla.

### Perché Escape richiede un `BMessageFilter`, non un `KeyDown` sovrascritto

Un `BTextControl` non riceve `KeyDown` per i tasti digitati durante
l'editing: `BTextControl::MakeFocus()` inoltra il fuoco tastiera alla
sua `BTextView` interna (che gestisce davvero il testo), quindi è
quella a ricevere gli eventi, non il contenitore. Sovrascrivere
`KeyDown` in una sottoclasse di `BTextControl` non avrebbe mai
intercettato Escape durante la digitazione normale. Soluzione:
`CellEditEscapeFilter`, un `BMessageFilter` installato direttamente
sulla `BTextView` interna (`BTextControl::TextView()->AddFilter(...)`),
che intercetta `B_KEY_DOWN` con `raw_char == B_ESCAPE`, lo trasforma
in un messaggio di annullamento per `SheetView` e restituisce
`B_SKIP_MESSAGE` per impedire che venga anche inserito come carattere.

## Taglia/copia/incolla: appunti di sistema veri, non un buffer privato

Il menu Modifica (`MainWindow::CopySelection`/`PasteSelection`/
`DeleteSelection`) passa dal vero **Clipboard Kit** di Haiku
(`be_clipboard`), non da una variabile membro interna all'app: il
contenuto copiato (la formula della cella, la stessa mostrata dalla
barra formule) viene scritto come `text/plain`/`B_MIME_TYPE` dentro il
`BMessage` restituito da `be_clipboard->Data()`, fra un
`Lock()`/`Clear()` e un `Commit()`/`Unlock()` — esattamente il
pattern standard di ogni app Haiku che vuole interoperare con gli
appunti di sistema (copiare in Atomo123 e incollare in un editor di
testo funziona, e viceversa).

**Verifica**: `ui/tests/test_clipboard.cpp` (richiede una sessione
grafica, il Clipboard Kit passa dall'app_server — `cd ui && make
test-clipboard`, non incluso nel normale `make test` headless-safe)
replica esattamente la logica di `CopySelection`/`PasteSelection` in
isolamento. Oltre al giro autocontenuto, è stata fatta una prova
incrociata reale con il tool a riga di comando di sistema
`clipboard` (già presente su Haiku): scrittura con il binario di test
→ lettura con `clipboard -p` (esito corretto), e scrittura con
`clipboard -c` → lettura con il binario di test (esito corretto) —
prova concreta che si tratta davvero degli appunti di sistema
condivisi, non di uno stato privato del processo.

## Bug scoperto: violazione di thread fra `BApplication` e `BWindow`

Il primo test end-to-end (apertura di un file XLSX reale in una vera
sessione grafica, non headless) si è chiuso con un crash intercettato
da `debug_server` — a differenza di quasi tutti i bug delle fasi
precedenti, qui *c'era* un `app_server` collegato, quindi non poteva
trattarsi della stessa famiglia "codice mai eseguito senza UI".

Causa: `App::RefsReceived` (sul thread di `BApplication`) invocava
direttamente `MainWindow::OpenFile()`, che tocca le `BView` della
finestra (`Invalidate()`, `BTextControl::SetText()`, ecc.) — ma
`BWindow` e le sue `BView` vivono sul thread del *loro* `BLooper`, non
su quello dell'applicazione, e non sono sicure da toccare senza il
lock della finestra (`BWindow::Lock()`). Chiamare quel metodo dal
thread sbagliato senza lock corrompe lo stato mentre l'app_server
potrebbe contemporaneamente elaborare un `Draw()`/`Invalidate()` sullo
stesso oggetto dal thread corretto — una race condition classica
nell'Application Kit di Haiku/BeOS.

**Fix**: `App::RefsReceived` si limita a inoltrare il `BMessage` alla
finestra con `fWindow->PostMessage(message)`, lasciando che sia
`MainWindow::MessageReceived` (già scritto per gestire
`B_REFS_RECEIVED`) a processarlo — l'elaborazione avviene così sul
thread corretto, con il lock preso automaticamente dal ciclo dei
messaggi del `BLooper`. Regola generale per il resto della Fase 4: mai
chiamare direttamente un metodo che tocca le `BView` di una finestra
da un altro thread — sempre passare da un `BMessage` inoltrato con
`PostMessage()`/`SendMessage()`.

**Come è stato diagnosticato**: un harness headless (`BApplication`
senza finestra, chiamata diretta a `BTranslatorRoster::Translate` +
`LoadASCD` + `GetCellResult`) non riproduceva il crash, isolando il
problema alla combinazione specifica finestra+thread. La conferma
finale è arrivata testando dal vivo in una sessione grafica reale
(`app_server`/Deskbar/Tracker attivi sul sistema): un B_REFS_RECEIVED
sintetico inviato con `BMessenger` all'app in esecuzione (simulando un
trascinamento da Tracker) riproduceva il crash prima del fix e non lo
riproduceva più dopo, verificato sia via screenshot sia interrogando
la finestra dal vivo con lo strumento di scripting nativo `hey`.

## Test

`ui/tests/test_ascd_io.cpp` (`cd ui && make test`, non richiede una
sessione grafica) verifica che `SaveASCD`/`LoadASCD` siano l'una
l'inversa dell'altra: numeri, testo e una formula (verificata anche
nel suo ricalcolo dal motore, non solo nel testo confrontato)
sopravvivono a un giro salva→ricarica completo.

**Limite noto**: non esiste un test end-to-end automatizzato per il
doppio click/digitazione diretta in-cella, né per il vero flusso
"Salva con nome" attraverso `BFilePanel`. Un tentativo di simulare
`B_SAVE_REQUESTED` con un messaggio costruito a mano e inviato
all'applicazione non ha funzionato: a differenza di `B_REFS_RECEIVED`
(per cui `App::RefsReceived` inoltra esplicitamente alla finestra),
non c'è nessun inoltro automatico quando `B_SAVE_REQUESTED` arriva
alla `BApplication` — nell'uso reale il `BFilePanel` lo manda
direttamente alla finestra (è il target impostato nel suo costruttore
in `MainWindow::MainWindow()`), non passa mai dall'applicazione. Anche
il doppio click e la digitazione diretta richiederebbero uno strumento
di iniezione mouse/tastiera non disponibile in questo ambiente di
test (a differenza di `B_REFS_RECEIVED`, che si simula costruendo il
`BMessage` a mano). Verificati con test di non-regressione (apertura
file reale con il nuovo codice presente, nessun crash) e revisione
manuale del codice; verifica interattiva reale rimandata a un utente
umano o a un ambiente con tale strumento.

## Limiti noti (prima versione)

- **Intestazioni non "congelate"**: le lettere di colonna e i numeri
  di riga sono disegnati alla loro posizione virtuale assoluta, non
  ancorati al bordo della viewport durante lo scroll — scorrendo oltre
  la prima schermata scorrono via insieme al contenuto. Scelta
  deliberata per evitare la complessità di viste multiple sincronizzate
  (e il flicker da `CopyBits` che ne deriverebbe con questa tecnica).
- **Nessun export**: "Salva con nome" scrive solo in ASCD nativo,
  coerente col limite "solo import" già documentato in Fase 3 (nessun
  translator ha ancora un writer per CSV/XLS/XLSX/ODS).
- **Un solo foglio**: coerente col limite già accettato in Fase 3 per
  XLSX/ODS (si importa solo il primo foglio/tabella).
- Solo i menu File e Modifica: nessun menu Formato, nessun dialogo
  trova/sostituisci.
- Nessuna formattazione (font/colore/numero) esposta all'utente, anche
  se il motore la supporta internamente.
