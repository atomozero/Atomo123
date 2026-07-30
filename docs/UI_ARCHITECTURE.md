# Applicazione nativa (`ui/`)

Prima versione dell'applicazione Interface/Layout Kit (Fase 4), scritta
da zero (non riusa `CellView`/`CellWindow` BeOS-era) sopra il motore di
calcolo isolato (`engine/`) e il Translation Kit per l'interoperabilità
file (`translators/`).

## Struttura

```
ui/src/App.h/.cpp          BApplication: crea la finestra, inoltra i
                            file aperti da Tracker/riga di comando
ui/src/MainWindow.h/.cpp   BWindow: menu File/Modifica, toolbar, barra
                            formule, cella corrente, apertura/salvataggio
ui/src/SheetView.h/.cpp    BView custom: griglia, selezione, editing
ui/src/AscdIO.h/.cpp       Lettura/scrittura del formato nativo ASCD
                            (stessa logica duplicata nei translator,
                            vedi docs/TRANSLATORS.md)
ui/src/FindWindow.h/.cpp   BWindow separata per "Trova e sostituisci"
                            (campi + pulsanti), inoltra a MainWindow
ui/Atomo123.rdef           Risorse dell'app (firma, versione, icona
                            VICN/BEOS:ICON), compilate da rc + xres
ui/icons/                  Icona sorgente: atomo123.svg (disegnata a
                            mano) e atomo123.hvif (esportata da
                            Icon-O-Matic, incorporata in Atomo123.rdef)
```

`SheetView` non usa `BGridLayout`: la griglia è disegnata a mano in
`Draw()` sopra una `BScrollView`. Il `Frame()` della vista copre
l'intero intervallo virtuale del motore fin dalla costruzione
(`kColCount`/`kRowCount` da `engine/src/Config/Constants.h`,
702×16384 celle, ~56200×327700 pixel — il pattern classico BeOS/Haiku
per una vista scorrevole: la `BScrollView` ritaglia e scorre una
vista grande, non viceversa), con calcolo manuale del range delle
scrollbar (`FixupScrollBars()`, agganciato a `BView::ScrollBar()`).
Vedi la nota tecnica più sotto sul perché questo dettaglio — vista
grande, non piccola — è importante e su un bug reale che è nato
proprio da qui.

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

## Toolbar: `BButton` semplici, non `BToolBar`

Una riga di `BButton` di solo testo sotto il menu (Nuovo/Apri/Salva/
Stampa/Taglia/Copia/Incolla/Trova), non la classe `BToolBar` di Haiku:
quella vive solo sotto `develop/headers/private/shared/` su questo
sistema, non nell'SDK pubblico stabile, e il progetto usa
deliberatamente solo API pubbliche documentate (Interface/Locale/
Print/Translation/Clipboard Kit). Ogni pulsante invia lo stesso
`BMessage` già gestito dal menu corrispondente (`SetTarget(this)`,
stesso target di `MessageReceived`) — nessuna logica nuova, solo un
secondo punto di accesso alle stesse azioni. Nessuna icona per i
pulsanti (solo testo): evita di dover disegnare un intero set di
icone HVIF in più, oltre a quella dell'applicazione.

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

## Menu Formato: Generale/Numero/Valuta/Percentuale

Il menu Formato applica un `ENumberFormat` (`eGeneral`/`eFixed`/
`eCurrency`/`ePercent`, già definiti in `engine/src/Cell/Formatter.h`)
alla cella selezionata, agendo su `CellStyle::fFormat`
(`MainWindow::SetCellFormat`: `GetCellStyle` → modifica `fFormat` →
`SetCellStyle`, entrambi già esistenti in `CContainer` — nessuna nuova
API dell'engine servita). Ogni voce del menu porta lo stesso
`kMsgSetFormat` con il valore da applicare in un campo `int32
"format"` del `BMessage`, invece di un messaggio diverso per voce.

`SheetView::Draw()` ora legge `CellStyle::fFormat` (via
`CContainer::GetCellStyle`) prima di applicare la formattazione
locale-aware, per rispettare la scelta dell'utente invece di
sovrascriverla sempre con il raggruppamento numerico generico:
`eCurrency` usa `BNumberFormat::FormatMonetary()`, `ePercent` usa
`BNumberFormat::FormatPercent()` (valore atteso come frazione, es.
0.42 → "42%" — stessa convenzione già usata internamente dal motore
per `ePercent`, vedi `exp10 += 2` in
`engine/src/Cell/Formatter.number.cpp`), tutto il resto (incluso
`eGeneral`) usa `BNumberFormat::Format()` come già faceva prima di
questa funzione.

**Verifica**: dal vivo, invocando il menu con `hey` (non simulando un
`BMessage` a mano: `MenuItem 2 of Menu 2 of MenuBar of Window 0` per
"Valuta") su una cella con "1234.5" incollato dagli appunti —
applicato il formato, la griglia mostra "1.234,50 €" (formattazione
Locale Kit italiana), la barra formule resta invariata su "1234.5".

**Non ancora fatto**: controllo del numero di decimali, font, colore,
bordo — il motore li supporta tutti tramite `CellStyle`, ma senza una
UI dedicata restano fissi ai valori predefiniti. Formattazione data
(`BDateFormat`) non ancora esposta.

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

## Trova e sostituisci: una seconda finestra, stessa regola sui thread

"Trova e sostituisci…" nel menu Modifica apre `FindWindow`, una
piccola `BWindow` separata con un campo di ricerca, un campo
"Sostituisci con:", e tre pulsanti ("Trova successivo", "Sostituisci",
"Sostituisci tutto"). Non esegue la ricerca/sostituzione da sé — le
celle appartengono al documento di `MainWindow`, che vive sul thread
della finestra principale, un `BLooper` diverso da quello di
`FindWindow` — quindi invia i testi con un `BMessage`
(`kMsgFindNext`/`kMsgReplaceCurrent`/`kMsgReplaceAll`) a un
`BMessenger` passato dal chiamante, non chiama un metodo di
`MainWindow` direttamente: stessa regola del bug di thread
`BApplication`/`BWindow` descritto sotto, applicata stavolta fra due
finestre invece che fra applicazione e finestra.

`MainWindow::FindNext()` scandisce le celle esistenti del documento
(`CCellIterator`) confrontando il testo (`GetCellFormula`,
case-insensitive, sottostringa) con quanto digitato, e seleziona il
primo risultato dopo la cella correntemente selezionata — con
"wrap-around" al primo risultato assoluto se non ce n'è nessuno dopo.
Nessun iteratore persistito fra una ricerca e l'altra: una scansione
completa ogni volta, scelta deliberata per semplicità (niente rischio
di un iteratore invalidato da una modifica del documento fra due
"Trova successivo") — adeguata alle dimensioni di foglio di questa
prima versione dell'app.

`MainWindow::ReplaceCurrent()`/`ReplaceAll()` riusano la stessa
funzione (`ReplaceAllCaseInsensitive`, locale a `MainWindow.cpp`) per
sostituire tutte le occorrenze del testo cercato dentro il testo di
una cella, cercando senza distinguere maiuscole/minuscole ma
inserendo il testo di sostituzione così com'è scritto (non nella
capitalizzazione originale). `ReplaceAll` prima raccoglie in un
`std::vector<cell>` tutte le celle da modificare, poi le modifica in
un secondo ciclo separato: `CCellIterator` scorre la mappa interna del
documento, che non va alterata (`TryToParseString` può
aggiungere/rimuovere celle) mentre la si sta iterando.

`FindWindow::QuitRequested()` non chiude mai davvero la finestra (si
nasconde e basta, restituendo `false`): `MainWindow` tiene un unico
puntatore per tutta la vita dell'app, mostrandola/attivandola di nuovo
a ogni apertura del dialogo invece di ricrearla, e la distrugge per
davvero solo nel proprio distruttore (`Lock()` + `Quit()` diretto, non
tramite `B_QUIT_REQUESTED` che passerebbe da quell'hook).

**Verifica**: la finestra si apre correttamente dal vivo (invocata dal
menu con `hey` — `MenuItem 6 of Menu 1 of MenuBar of Window 0` — non
simulando il `BMessage` a mano), mostrando tutti e tre i controlli.
La logica di sostituzione (`ReplaceAllCaseInsensitive` + la scansione
a due passate di `ReplaceAll`) è stata verificata con un harness
dedicato che riproduce esattamente lo stesso algoritmo su un
documento con due celle contenenti "Mondo"/"mondo": trova entrambe,
sostituisce correttamente producendo "Ciao Terra"/"Terra intero". La
sessione di test in questo momento condivideva il desktop con
un'altra attività grafica indipendente dell'utente (finestre che
apparivano/sparivano, processi chiusi dall'esterno) — non affidabile
per uno screenshot pulito dell'esito finale nella griglia, da qui la
scelta dell'harness diretto invece di un ennesimo tentativo dal vivo.

## Icona dell'applicazione

Il portale autorizzato per le icone (www.hvif-store.art) è risultato
vuoto a due controlli separati; l'utente ha scelto di disegnarla da
zero. `ui/icons/atomo123.svg` è una griglia bianca 3×3 con una cella
evidenziata in arancione su sfondo blu arrotondato (richiama
chiaramente un foglio di calcolo), disegnata a mano in SVG piatto
(niente gradient/filtri, per la massima compatibilità con
l'importatore SVG di Icon-O-Matic). Aperta in Icon-O-Matic (che importa
SVG nativamente, senza bisogno di un `BTranslator` dedicato) ed
esportata in HVIF **direttamente dall'utente** — `ui/icons/atomo123.hvif`
(verificato: firma `ncif` corretta nei primi 4 byte, il magic number
del formato).

L'HVIF viene incorporato come risorsa `VICN`/`BEOS:ICON` in
`ui/Atomo123.rdef` (byte esadecimali diretti nella sintassi `rc`,
stesso meccanismo — non un file `.hvif` distribuito a parte — usato da
altri progetti nativi Haiku sullo stesso sistema, es. HaikuBench).
`ui/Makefile` compila `Atomo123.rdef` con `rc` e allega il risultato al
binario con `xres` come parte automatica della build normale
(`make` in `ui/`), non un passo manuale a parte.

**Verifica**: `xres -l Atomo123` conferma la risorsa `VICN` da 440
byte — la stessa dimensione esatta del file HVIF sorgente. Verificato
anche visivamente: aperta una finestra Tracker sulla cartella `ui/` in
vista icone (passaggio di vista fatto scriptando il menu Finestra di
Tracker con `hey`, stessa tecnica già usata per Atomo123 stesso),
l'icona compare correttamente sul file `Atomo123`.

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

## Export CSV e bug scoperto: formule mai ricalcolate al caricamento

"Salva con nome" instrada ora attraverso `BTranslatorRoster` invece di
scrivere sempre ASCD a mano: serializza prima il documento corrente in
ASCD in memoria (`BMallocIO`), poi chiama
`BTranslatorRoster::Default()->Translate(&ascd, NULL, NULL, &file,
outType)`, lasciando che il roster trovi un translator installato che
sappia leggere ASCD e scrivere il formato scelto — il formato si
decide dall'estensione del nome file scelto nel `BFilePanel` (".csv"
esporta in CSV, altrimenti resta sul nativo ASCD). Il translator CSV
aveva già entrambe le direzioni fin dalla Fase 3
(`CTextConverter::ConvertToText`/`ConvertFromText`): mancava solo
questo instradamento. Disegnato per essere direttamente riusabile
quando XLS/XLSX/ODS avranno anche loro un writer, non solo per CSV.

Costruendo questo export è emerso un bug reale: una cella con formula
importata da ASCD esportava sempre **vuota** in CSV, anche se il
motore la ricalcola correttamente quando richiesto esplicitamente.
Causa: `TryToParseString` (usata da `LoadASCD` e dal `ReadASCD` del
translator CSV per popolare celle da un flusso ASCD) imposta la
formula/il valore di una cella ma **non la calcola** — serve una
`CalcCell` esplicita. Nessuno dei due punti la faceva, il che
significava che **qualunque file aperto nell'app con celle a formula
le mostrava vuote nella griglia** finché l'utente non le toccava a
mano (barra formule/editing in-cella, che chiamano `CalcCell` dopo
aver scritto) — passato inosservato perché ogni test dei translator
XLSX/ODS chiamava `CalcCell` esplicitamente *nel test stesso*,
mascherando che il percorso di produzione non lo faceva mai da solo.

**Fix**: nuova `RecalculateAll(CContainer*)` in `ui/src/AscdIO.h/.cpp`
(usata da `LoadASCD`), più la stessa logica duplicata nel `ReadASCD`
del translator CSV (stessa duplicazione intenzionale già usata per
`WriteASCD`/`ReadASCD`, per non introdurre una dipendenza di link fra
app e translator). Itera su tutte le celle chiamando `CalcCell` su
ciascuna, **ripetendo finché nessuna cella cambia più valore** (limite
di sicurezza: 50 passate): `CFormula::Calculate` legge i riferimenti
ad altre celle con una `GetValue` non ricorsiva, quindi l'ordine di
inserimento non garantisce che una cella referenziata sia già stata
calcolata — più passate propagano correttamente le dipendenze in
qualunque ordine, senza un vero ordinamento topologico del grafo delle
dipendenze.

**Verificato**: `ui/tests/test_ascd_io.cpp` ora controlla il valore di
C1 subito dopo `LoadASCD`, prima di qualunque `CalcCell` esplicito nel
test (prima il test chiamava `CalcCell` apposta, che nascondeva il
bug); un harness diretto dell'export CSV ha confermato che una formula
`=A1+B1` con A1=10/B1=20 esporta "30" invece di una cella vuota.

## Bug scoperto: la griglia non riempiva la finestra (segnalato dall'utente)

Aprendo l'app a schermo intero, la griglia mostrava solo la colonna A
e le prime 4 righe (~100×100 pixel in alto a sinistra), con il resto
della finestra vuoto — non uno sfondo scorrevole, proprio nessuna
riga/colonna disegnata oltre quel piccolo riquadro. Segnalato
dall'utente con uno screenshot, non trovato durante lo sviluppo
perché ogni test precedente aveva usato finestre piccole o si era
concentrato su altre funzionalità senza notare quanto poco della
griglia fosse effettivamente disegnato.

**Causa**: `SheetView` veniva costruita con un `Frame()` fisso e
minuscolo (`BRect(0, 0, 100, 100)`, un placeholder mai più
ridimensionato dopo la costruzione). `Draw()` calcola l'intervallo di
celle da disegnare a partire da `updateRect`, che per una `BView` non
può mai eccedere il proprio `Frame()` — indipendentemente da quanto
grande fosse la finestra o la `BScrollView` a schermo, l'app_server
non genera mai un `updateRect` più grande del `Frame()` della vista
stessa. Il sintomo (~100×100 pixel disegnati, esattamente
`kHeaderWidth + kColWidth` per `kHeaderHeight + 4×kRowHeight`)
combaciava esattamente con quella dimensione di costruzione,
confermando la diagnosi.

**Fix**: `SheetView::FullCanvasFrame()` (nuovo metodo statico)
restituisce un `BRect` che copre l'intero intervallo virtuale del
motore (`kColCount`×`kColWidth` per `kRowCount`×`kRowHeight`,
~56200×327700 pixel), usato nell'inizializzatore del costruttore —
il pattern classico BeOS/Haiku per una vista scorrevole (la
`BScrollView` ritaglia e scorre una vista grande, invece di
ridimensionare una vista piccola per adattarla al contenuto).
Conseguenza collaterale da correggere insieme: `Bounds()` di una
vista così grande riflette sempre la dimensione piena del `Frame()`,
mai la porzione effettivamente visibile a schermo — quindi sia
`FixupScrollBars()` (intervallo delle scrollbar) sia
`ScrollToShowSelection()` (scorrimento automatico verso la cella
selezionata), che prima usavano `Bounds()` assumendo riflettesse
l'area visibile, avevano lo stesso baco latente (mai emerso
visibilmente perché mascherato dal bug più vistoso della vista
minuscola). Corrette entrambe per usare `Parent()->Bounds()` (l'area
visibile reale della `BScrollView`, il vero "genitore" della vista
nella gerarchia) per le dimensioni, e `Bounds().left`/`.top` (l'unica
parte che `ScrollBy()`/`ScrollTo()` aggiornano davvero) per
l'origine corrente dello scroll.

Rimosso anche il parametro `BRect frame` dal costruttore di
`SheetView` (era fuorviante: sembrava controllare la dimensione
visualizzata, ma da questo fix in poi la vista si dimensiona sempre
da sola) — ora `SheetView(CContainer* doc)`, non più
`SheetView(BRect frame, CContainer* doc)`.

**Verificato dal vivo**: screenshot dopo il fix mostra colonne A-J e
righe 1-25 che riempiono correttamente la finestra (prima: solo
colonna A, righe 1-4); la finestra è stata ridimensionata due volte
via `hey` (`set Frame of Window 0 to "BRect(...)"`) per esercitare
ripetutamente `FixupScrollBars()` con le nuove dimensioni — nessun
crash, nessun errore.

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

### Scoperta successiva: `hey` sa invocare le voci di menu

Dopo aver scritto la nota sopra, si è scoperto che lo strumento di
scripting nativo `hey` espone davvero l'esecuzione delle voci di menu
tramite lo scripting suite standard di `BMenuBar`/`BMenu`
(`MenuItem ... B_EXECUTE_PROPERTY`, "Invokes the specified menu item"),
navigabile con la catena di specificatori `MenuItem <indice> of Menu
<indice> of MenuBar of Window <indice>` (gli indici delle voci si
scoprono con `hey -o Atomo123 get Label of MenuItem <n> of Menu <m> of
MenuBar of Window 0`). Usato per aprire davvero la finestra "Trova"
dal menu (non simulando il `BMessage` a mano) e verificare che
l'intera catena menu → `MainWindow::ShowFindWindow` → `FindWindow`
funzioni senza crash in una sessione grafica reale, con entrambe le
finestre visibili e responsive in uno screenshot. Questo apre la
possibilità di testare in modo simile anche Taglia/Copia/Incolla/
Stampa/Nuovo/Apri tramite la stessa tecnica nelle prossime iterazioni,
invece della sola revisione manuale del codice.

## Limiti noti (prima versione)

- **Intestazioni non "congelate"**: le lettere di colonna e i numeri
  di riga sono disegnati alla loro posizione virtuale assoluta, non
  ancorati al bordo della viewport durante lo scroll — scorrendo oltre
  la prima schermata scorrono via insieme al contenuto. Scelta
  deliberata per evitare la complessità di viste multiple sincronizzate
  (e il flicker da `CopyBits` che ne deriverebbe con questa tecnica).
- **Export solo verso CSV**: "Salva con nome" esporta in CSV (se il
  nome scelto finisce per ".csv") o nel nativo ASCD, ma non ancora
  verso XLS/XLSX/ODS — nessuno dei tre translator ha un writer, solo
  import. Nessun selettore di formato dedicato nel pannello di
  salvataggio: il formato si decide dall'estensione del nome file.
- **Un solo foglio**: coerente col limite già accettato in Fase 3 per
  XLSX/ODS (si importa solo il primo foglio/tabella).
- Menu Formato limitato a Generale/Numero/Valuta/Percentuale: nessun
  controllo su decimali, font, colore, bordo, allineamento, data —
  il motore li supporta tutti tramite `CellStyle`, ma senza una UI
  dedicata restano fissi ai valori predefiniti.
