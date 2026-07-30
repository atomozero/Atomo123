# Guida utente

Guida rapida per chi usa Atomo123, non per chi sviluppa il codice
(per l'architettura interna vedi `docs/ENGINE_API.md`,
`docs/TRANSLATORS.md`, `docs/UI_ARCHITECTURE.md`). Riflette lo stato
attuale del progetto (Fase 4/5 della `ROADMAP.md`): alcune funzioni
descritte qui sotto come "non ancora disponibili" sono già pianificate.

## Avvio

```
cd ui
make
./Atomo123
```

Per aprire file CSV/XLS/XLSX/ODS (non solo il formato nativo ASCD),
vanno prima installati i translator corrispondenti — una tantum, non
a ogni avvio:

```
cd translators/csv && make && make install
cd translators/xls && make && make install
cd translators/xlsx && make && make install
cd translators/ods && make && make install
```

`make install` copia l'add-on in
`~/config/non-packaged/add-ons/Translators/`, da dove Atomo123 lo
trova automaticamente tramite il Translation Kit di sistema.

## La finestra principale

In alto: menu **File**, **Modifica**, **Formato** e **Inserisci**, poi
una riga con il riferimento della cella selezionata (es. "A1") e la
barra formule. Sotto, la griglia del foglio.

- **Selezionare una cella**: click, oppure le frecce direzionali da
  tastiera.
- **Vedere/modificare la formula di una cella**: la barra formule
  mostra sempre il contenuto grezzo (la formula, non il valore
  calcolato) della cella selezionata. Si modifica lì e si conferma con
  Invio.
- **Editing direttamente sulla griglia**: doppio click su una cella,
  oppure si inizia a digitare direttamente mentre è selezionata (il
  testo digitato sostituisce il contenuto esistente, come in Excel/
  LibreOffice Calc). Invio o un click su un'altra cella confermano (e,
  come in Excel, la selezione avanza alla cella sotto); Esc annulla
  (senza spostare la selezione).
- **Cancellare il contenuto di una cella**: tasto Canc/Backspace con
  la cella selezionata, oppure Modifica → Cancella.
- **Intestazioni "congelate"**: la riga con le lettere di colonna
  resta visibile in cima anche scorrendo in basso; la colonna con i
  numeri di riga resta visibile a sinistra anche scorrendo a destra.
- **Scorciatoie da tastiera aggiuntive** (in stile Excel/LibreOffice
  Calc): **Tab**/**Maiusc+Tab** spostano a destra/sinistra; **Invio**/
  **Maiusc+Invio** spostano in basso/alto (anche fuori dall'editing,
  non solo per confermare); **Inizio** va alla colonna A della riga
  corrente, **Ctrl+Inizio** va sempre alla cella A1; **Ctrl+Fine** va
  all'ultima cella con contenuto (l'angolo in basso a destra dei
  dati); **PagSu**/**PagGiù** spostano la selezione di una schermata
  intera in alto/basso.

## Formule

Si scrivono cominciando con `=`, ad esempio `=A1+B1*2`. Sono
supportati gli operatori aritmetici, i riferimenti a cella/intervallo
(`A1:A3`) e le funzioni con nome, ad esempio `=SUM(A1:A3)` o
`=IF(A1>5;100;200)` — i nomi delle funzioni sono in inglese (`SUM`,
`IF`, `MAX`, `AVG`, ecc., non `SOMMA`/`SE`/`MEDIA`), eredità diretta
del motore di calcolo originale di Sum-It.

**Attenzione al separatore degli argomenti**: fra più argomenti di
una funzione si usa il **punto e virgola** (`;`), non la virgola —
`=IF(A1>5;100;200)`, non `=IF(A1>5,100,200)` (che dà errore di
formula). La virgola resta il separatore delle migliaia nella
formattazione numero (Formato → Numero), un dettaglio distinto.

**Aggregazione condizionata**: `=SUMIF(A1:A10;"Roma";B1:B10)` somma i
valori di B1:B10 dove la cella corrispondente in A1:A10 è "Roma";
`=COUNTIF(A1:A10;"Roma")` conta invece quante celle corrispondono.
`=AVERAGEIF(...)` funziona come `SUMIF` ma calcola la media. Il
terzo argomento (l'intervallo da sommare/mediare) è opzionale — se
omesso si usa lo stesso primo intervallo. Il criterio può essere un
numero, del testo (senza distinguere maiuscole/minuscole), oppure un
confronto con `>`, `>=`, `<`, `<=` o `<>` seguito da un numero, es.
`=SUMIF(B1:B10;">100")`.

## File: aprire, salvare, nuovo

- **File → Nuovo**: foglio vuoto (il contenuto non salvato attuale va
  perso senza conferma — nessun controllo "modifiche non salvate"
  ancora implementato).
- **File → Apri…**: qualunque file CSV, XLS (Excel 97-2003), XLSX
  (Excel 2007+) o ODS (LibreOffice/OpenOffice Calc) per cui sia
  installato il translator corrispondente, oltre al formato nativo
  ASCD. Il formato viene riconosciuto dal contenuto del file, non
  dall'estensione.
- **File → Salva con nome…**: il formato di uscita si sceglie
  dall'estensione scritta nel nome del file — `.csv` esporta in CSV
  (con i valori delle formule già calcolati, non il testo della
  formula), qualunque altra estensione (o nessuna) scrive nel formato
  nativo ASCD. Non è ancora possibile esportare verso XLS/XLSX/ODS: quei
  translator per ora importano soltanto. Per riaprire un file ASCD
  salvato, usare di nuovo File → Apri (un CSV esportato riapre invece
  solo i valori, non le formule originali — CSV non ha alcun concetto
  di formula).

## Taglia, copia, incolla

Passano dagli appunti di sistema di Haiku (non da un appunti privato
dell'app): si può copiare da una cella di Atomo123 e incollare in un
editor di testo, e viceversa. Il contenuto copiato è la formula della
cella (lo stesso testo mostrato dalla barra formule), non il valore
già calcolato.

## Trova e sostituisci

Modifica → Trova e sostituisci… apre una piccola finestra con un
campo "Cerca:", un campo "Sostituisci con:" e tre pulsanti. La ricerca
(senza distinguere maiuscole/minuscole) parte dalla cella attualmente
selezionata, tornando all'inizio se non trova altri risultati dopo.

- **Trova successivo**: seleziona la cella successiva che contiene il
  testo cercato.
- **Sostituisci**: sostituisce tutte le occorrenze del testo cercato
  nella cella attualmente selezionata, poi passa al risultato
  successivo.
- **Sostituisci tutto**: sostituisce tutte le occorrenze in ogni
  cella del documento che contiene il testo cercato, e mostra quante
  celle sono state modificate.

## Stampa

File → Stampa… apre il dialogo di stampa di sistema (scelta
stampante/opzioni). Si stampa l'area del foglio che contiene dati,
suddivisa automaticamente in più pagine se necessario in base
all'area stampabile scelta. Anche senza una stampante fisica
collegata, Haiku offre di serie i transport "Anteprima" e "Salva come
PDF", utilizzabili dal dialogo di stampa per vedere il risultato senza
stampare davvero.

**Limite noto**: le intestazioni di riga/colonna (le lettere in alto,
i numeri a sinistra) compaiono solo sulla prima pagina di una stampa
multi-pagina, non ripetute su ogni pagina.

## Formattazione numeri

I numeri nella griglia vengono mostrati secondo le preferenze di
formattazione del sistema (separatore delle migliaia,
punto/virgola decimale) — cambiano automaticamente insieme alle
preferenze di sistema (Preferenze → Locale). La barra formule mostra
sempre il valore grezzo, non formattato.

Il menu **Formato** applica uno stile di visualizzazione alla cella
selezionata:

- **Generale**: il comportamento predefinito descritto sopra.
- **Numero**: numero semplice.
- **Valuta**: simbolo di valuta secondo le preferenze di sistema (es.
  "1.234,50 €").
- **Percentuale**: il valore moltiplicato per 100 con il simbolo "%"
  (una cella con 0,42 mostra "42%").

**Non ancora disponibile**: controllo del numero di decimali, font,
colore, bordo, allineamento, formattazione data.

## Grafici e tabelle pivot

Il menu **Inserisci** offre due funzionalità che leggono un
intervallo di **due colonne** (prima colonna: etichetta/categoria di
testo; seconda colonna: valore numerico) — l'intervallo si digita a
mano nella finestra dedicata (es. `A1:B5`), non si seleziona
trascinando sulla griglia (che oggi supporta solo una cella
selezionata alla volta).

- **Grafico a barre…**: apre una finestra con un campo "Intervallo" e
  il pulsante "Disegna", che mostra un'anteprima (ogni riga
  dell'intervallo diventa una barra, etichetta sotto, altezza
  proporzionale al valore) — l'anteprima non si aggiorna da sola,
  bisogna premere di nuovo "Disegna" dopo aver cambiato i dati. Il
  pulsante **"Inserisci nel foglio"**, con una cella di destinazione,
  incorpora invece il grafico davvero nella griglia, come in Excel:
  una volta inserito, il grafico **si aggiorna da solo** ogni volta
  che i dati dell'intervallo cambiano (non serve ridisegnarlo a
  mano), e viene salvato/ricaricato insieme al documento (solo nel
  formato nativo — un export CSV non lo porta con sé, essendo un
  formato di solo testo).
- **Tabella pivot…**: apre una finestra con l'intervallo dati
  sorgente, la cella di destinazione e il tipo di aggregazione
  (Somma, Conteggio o Media). Premendo "Crea", le righe con la stessa
  categoria vengono raggruppate e aggregate; il risultato (due
  colonne: categoria, valore aggregato) viene scritto nel foglio a
  partire dalla cella di destinazione scelta — che non può
  sovrapporsi all'intervallo dati sorgente. A differenza del grafico
  incorporato, il risultato è una scrittura una tantum (celle vere e
  proprie): non si aggiorna da solo se i dati sorgente cambiano dopo;
  per un risultato aggiornato bisogna rilanciare "Crea".

**Limiti di questa prima versione**: solo grafico a barre (niente a
linee o a torta); la tabella pivot raggruppa per una sola categoria e
aggrega una sola colonna di valori (non un pivot multidimensionale
come in Excel/LibreOffice Calc); il grafico incorporato ha una
dimensione fissa (non si ridimensiona/sposta dopo l'inserimento) e
non c'è ancora un modo per rimuoverne uno dalla griglia.

## Cosa manca ancora (in breve)

Vedi `ROADMAP.md` per l'elenco completo e aggiornato. In sintesi, allo
stato attuale: nessun export verso XLS/XLSX/ODS (solo CSV e ASCD
nativo), formattazione cella limitata a Generale/Numero/Valuta/
Percentuale (niente decimali/font/colore/data), nessun supporto per
più fogli in un unico documento.
