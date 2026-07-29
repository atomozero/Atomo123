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

## Locale Kit: numeri formattati secondo le preferenze di sistema

Il motore di calcolo formatta i numeri in modo generico
(`CFormatter`/`eGeneral` in `engine/src/Cell/Formatter.cpp`, nessuna
nozione di locale) — coerente col fatto che è codice storico BeOS
isolato, non pensato per il Locale Kit moderno di Haiku. `SheetView`
aggiunge un livello di presentazione sopra quel testo: se il valore di
una cella è numerico (`CContainer::GetValue` restituisce `eNumData`,
non NaN), il testo mostrato nella griglia viene rigenerato con
`BNumberFormat::Format()`, che usa le preferenze di formattazione del
sistema (separatore delle migliaia, punto o virgola decimale). La
barra formule invece mostra sempre il testo grezzo/editabile
(`GetCellFormula`), non quello formattato — coerente col comportamento
di un vero foglio di calcolo (editing sul valore vero, visualizzazione
formattata solo nella cella).

**Verifica**: aperto un file ASCD di test con A1 = 1234567.89 in una
sessione grafica reale con locale italiano attivo — la griglia mostra
"1.234.567,89" (punto come separatore delle migliaia, convenzione
italiana), la barra formule mostra "1234567.89" invariato.

**Non ancora fatto**: formattazione valuta (`BNumberFormat::
FormatMonetary`) e data (`BDateFormat`) — il motore distingue i tipi
internamente ma la UI non offre ancora un modo per l'utente di
impostare il formato di una cella (nessun menu Formato, vedi limiti
noti sotto).

## Print Kit: stampa con `BPrintJob`

"Stampa…" nel menu File (`MainWindow::PrintDocument`) segue il
pattern standard di Haiku per la stampa:

1. `BPrintJob::ConfigJob()` — mostra il dialogo di sistema
   (stampante/opzioni); se l'utente annulla o non c'è nessuna
   stampante configurata, restituisce un errore e non si stampa
   nulla.
2. `BeginJob()`, poi un ciclo che copre `SheetView::ContentRect()`
   (nuovo metodo pubblico: il rettangolo in pixel, intestazioni
   comprese, che copre le celle con contenuto — calcolato da
   `CContainer::GetBounds()` più `SheetView::CellRect()`, non
   l'intero intervallo virtuale del motore di 702×16384 celle)
   suddiviso in pagine larghe/alte quanto `BPrintJob::
   PrintableRect()`. Per ogni pagina: `DrawView(fSheetView,
   pageSlice, BPoint(0,0))` (Haiku disegna quella porzione della
   view direttamente sul job di stampa) poi `SpoolPage()`.
3. `CommitJob()` se tutto è andato bene (`CanContinue()` ancora
   vero), altrimenti `CancelJob()`.

**Limite noto**: le intestazioni di riga/colonna sono disegnate da
`SheetView::Draw()` solo nella banda fissa `0`–`kHeaderWidth`/
`kHeaderHeight` (stessa scelta della UI a schermo, vedi limite
"intestazioni non congelate" sotto) — nella stampa multi-pagina
questo significa che compaiono solo sulla prima pagina (in alto a
sinistra), non ripetute su ogni pagina come farebbe un foglio di
calcolo maturo. Andrebbe risolto facendo disegnare a `SheetView` le
intestazioni separatamente per ogni pagina durante la stampa, non
insieme al contenuto — rimandato.

**Verifica**: build pulita e test di non-regressione (apertura di un
file reale col nuovo codice presente, nessun crash). Un test
end-to-end di stampa reale non è stato possibile in questa sessione:
`ConfigJob()` apre un dialogo di sistema che richiede una scelta
dell'utente (stampante, opzioni, conferma) — non simulabile
costruendo un `BMessage` a mano come si fa per `B_REFS_RECEIVED`
(stesso limite già incontrato per `B_SAVE_REQUESTED` e per il doppio
click/digitazione diretta in-cella: nessuno strumento di iniezione
mouse/tastiera disponibile in questo ambiente di test). Il sistema ha
comunque i transport "Preview" e "Save as PDF" già disponibili come
add-on (`/boot/system/add-ons/Print/`), utilizzabili da un utente
reale per un test interattivo senza bisogno di una stampante fisica.

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
