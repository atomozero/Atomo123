# Icone per le barre di Atomo123

Selezione ragionata dal catalogo HVIF per le toolbar del foglio di calcolo.
Ogni voce indica l'icona consigliata e le eventuali alternative.

I path `.hvif` sono quelli da compilare come risorse Haiku; l'`.svg` accanto
serve solo per vedere l'icona durante lo sviluppo.

## Stato dell'integrazione

Le icone di questa selezione entrano nell'app solo quando la funzione
corrispondente esiste gia' in Atomo123 (un comando vero, non solo
un'idea) -- vedi `kToolbarRows` in `ui/src/MainWindow.cpp` e il byte
grezzi in `ui/src/IconData.cpp`. La toolbar e' divisa in cinque righe
per categoria (File, Modifica, Dati, Celle, Formato). Al momento sono
integrate:

- **File**: Nuovo, Apri, Salva, Stampa
- **Modifica**: Taglia, Copia, Incolla, Annulla, Ripeti, Trova, Elimina
- **Dati**: Ordina crescente/decrescente, Vai a, Intervalli con nome
- **Celle e tabella**: Grafico, (Pivot, con l'icona Tabella),
  Collegamento ipertestuale, Commento cella
- **Formato**: Grassetto, Corsivo, Sottolineato, Allinea sinistra/
  centro/destra, A capo automatico, Colore testo, Colore sfondo,
  Colore bordo

Le altre voci in tabella sono selezionate e verificate a vista, ma non
ancora usate da nessun pulsante: o la funzione non esiste ancora in
Atomo123 (Salva con nome come voce distinta, Anteprima di stampa,
Esporta, Filtro, Zoom, i tipi di grafico...), o esiste solo come voce
di menu non ancora promossa a pulsante (Bordi cella, Unisci celle,
Inserisci riga/colonna, Convalida dati, Formattazione condizionale,
Blocca riquadri -- per queste manca comunque un'icona reale nel
catalogo, vedi "Lacune" piu' sotto).

## File

| Funzione | id | Icona | Licenza | File HVIF | Alternative |
|---|---:|---|---|---|---|
| Nuovo foglio | 956 | Document New | MIT | `icons/hvif/956_document-new.hvif` | — |
| Apri | 1101 | Open | MIT | `icons/hvif/1101_open.hvif` | 950 (Open Folder) |
| Salva | 1089 | Save | MIT | `icons/hvif/1089_save.hvif` | — |
| Salva con nome | 913 | Save As | MIT | `icons/hvif/913_save-as.hvif` | 831 (Save All) |
| Stampa | 1181 | Print | MIT | `icons/hvif/1181_print.hvif` | 539 (Printer) |
| Anteprima di stampa | 863 | Preview | MIT | `icons/hvif/863_preview.hvif` | — |
| Esporta | 798 | Export | MIT | `icons/hvif/798_export.hvif` | — |
| Esporta PDF | 392 | Printer PDF | MIT | `icons/hvif/392_printer-pdf.hvif` | 532 (PDF File) |

## Modifica

| Funzione | id | Icona | Licenza | File HVIF | Alternative |
|---|---:|---|---|---|---|
| Taglia | 1168 | Cut | MIT | `icons/hvif/1168_cut.hvif` | 1137 (Cut) |
| Copia | 1010 | Copy | MIT | `icons/hvif/1010_copy.hvif` | 421 (Copy File) |
| Incolla | 852 | Paste | MIT | `icons/hvif/852_paste.hvif` | 1175 (Clipboard) |
| Annulla | 1103 | Undo | MIT | `icons/hvif/1103_undo.hvif` | — |
| Ripeti | 860 | Redo | MIT | `icons/hvif/860_redo.hvif` | — |
| Trova | 1072 | Find | MIT | `icons/hvif/1072_find.hvif` | 979 (Find) |
| Sostituisci | — | *da disegnare* | — | — | — |
| Elimina | 864 | Delete | MIT | `icons/hvif/864_delete.hvif` | 1017 (Trash) |

## Formato

| Funzione | id | Icona | Licenza | File HVIF | Alternative |
|---|---:|---|---|---|---|
| Grassetto | 859 | Bold | MIT | `icons/hvif/859_bold.hvif` | — |
| Corsivo | 1188 | Italic | MIT | `icons/hvif/1188_italic.hvif` | — |
| Sottolineato | 876 | Underline | MIT | `icons/hvif/876_underline.hvif` | — |
| Allinea a sinistra | 1113 | Justify Left | MIT | `icons/hvif/1113_justify-left.hvif` | — |
| Allinea al centro | 802 | Justify Center | MIT | `icons/hvif/802_justify-center.hvif` | — |
| Allinea a destra | 1151 | Justify Right | MIT | `icons/hvif/1151_justify-right.hvif` | — |
| Giustifica | 1156 | Justify Fill | MIT | `icons/hvif/1156_justify-fill.hvif` | — |
| Colore testo | 1163 | Color | MIT | `icons/hvif/1163_color.hvif` | 1144 (Color Picker) |
| Evidenzia | 1086 | Highlight | MIT | `icons/hvif/1086_highlight.hvif` | — |
| Dimensione + | 796 | Font Size More | MIT | `icons/hvif/796_font-size-more.hvif` | — |
| Dimensione - | 877 | Font Size Less | MIT | `icons/hvif/877_font-size-less.hvif` | — |
| Apice | 1201 | Superscript | MIT | `icons/hvif/1201_superscript.hvif` | — |
| Pedice | 957 | Subscript | MIT | `icons/hvif/957_subscript.hvif` | — |
| Aumenta rientro | 1139 | Indent More | MIT | `icons/hvif/1139_indent-more.hvif` | — |
| Interlinea | 1162 | Line Spacing Normal | MIT | `icons/hvif/1162_line-spacing-normal.hvif` | 1060 (Line Spacing Double), 945 (Lline Spacing Triple) |
| Testo a capo | 1036 | Wrap Lines | MIT | `icons/hvif/1036_wrap-lines.hvif` | — |
| Bordi cella | — | *da disegnare* | — | — | — |
| Formato numero | — | *da disegnare* | — | — | — |

## Celle e tabella

| Funzione | id | Icona | Licenza | File HVIF | Alternative |
|---|---:|---|---|---|---|
| Tabella | 1279 | Table | MIT | `icons/hvif/1279_table.hvif` | — |
| Inserisci tabella | 944 | Insert Table | MIT | `icons/hvif/944_insert-table.hvif` | — |
| Dividi orizzontale | 1233 | Split Left Right | MIT | `icons/hvif/1233_split-left-right.hvif` | — |
| Dividi verticale | 1232 | Split Top Bottom | MIT | `icons/hvif/1232_split-top-bottom.hvif` | — |
| Inserisci oggetto | 1155 | Insert Object | MIT | `icons/hvif/1155_insert-object.hvif` | — |
| Inserisci link | 1049 | Insert Link | MIT | `icons/hvif/1049_insert-link.hvif` | — |
| Inserisci riga | — | *da disegnare* | — | — | — |
| Inserisci colonna | — | *da disegnare* | — | — | — |
| Unisci celle | — | *da disegnare* | — | — | — |
| Blocca riquadri | — | *da disegnare* | — | — | — |

## Formule

| Funzione | id | Icona | Licenza | File HVIF | Alternative |
|---|---:|---|---|---|---|
| Somma automatica | — | *da disegnare* | — | — | — |
| Calcolatrice | 53 | Calculator | MIT | `icons/hvif/53_calculator.hvif` | 52 (Calculator), 706 (BeCalc) |
| Percentuale | — | *da disegnare* | — | — | — |
| Valuta | 710 | Finance | MIT | `icons/hvif/710_finance.hvif` | 1211 (Stocks) |
| Funzione (fx) | — | *da disegnare* | — | — | — |

## Dati

| Funzione | id | Icona | Licenza | File HVIF | Alternative |
|---|---:|---|---|---|---|
| Ordina crescente | 952 | Sort Ascending | MIT | `icons/hvif/952_sort-ascending.hvif` | — |
| Ordina decrescente | 974 | Sort Descending | MIT | `icons/hvif/974_sort-descending.hvif` | — |
| Filtro | 810 | Filter | MIT | `icons/hvif/810_filter.hvif` | — |
| Database | 1225 | Database | MIT | `icons/hvif/1225_database.hvif` | — |
| Importa | 977 | Import | MIT | `icons/hvif/977_import.hvif` | — |

## Vista

| Funzione | id | Icona | Licenza | File HVIF | Alternative |
|---|---:|---|---|---|---|
| Zoom avanti | 1045 | Zoom In | MIT | `icons/hvif/1045_zoom-in.hvif` | 1226 (Zoom 1 to 2) |
| Zoom indietro | 1227 | Zoom 2 to 1 | MIT | `icons/hvif/1227_zoom-2-to-1.hvif` | — |
| Zoom originale | 1011 | Zoom Original | MIT | `icons/hvif/1011_zoom-original.hvif` | — |
| Adatta alla pagina | 904 | Zoom Fit Best | MIT | `icons/hvif/904_zoom-fit-best.hvif` | 949 (Zoom Fit Page Width), 1094 (Zoom Fit Height) |
| Schermo intero | 1109 | Fullscreen | MIT | `icons/hvif/1109_fullscreen.hvif` | 1043 (Fullscreen) |

## Grafici

| Funzione | id | Icona | Licenza | File HVIF | Alternative |
|---|---:|---|---|---|---|
| Grafico | 1211 | Stocks | MIT | `icons/hvif/1211_stocks.hvif` | — |
| Grafico a barre | — | *da disegnare* | — | — | — |
| Grafico a torta | — | *da disegnare* | — | — | — |
| Grafico a linee | — | *da disegnare* | — | — | — |

## Lacune — icone da disegnare

Il catalogo non contiene un candidato adatto per queste funzioni. Vanno disegnate
in [Icon-O-Matic](https://www.haiku-os.org/docs/userguide/en/applications/icon-o-matic.html),
seguendo le [linee guida icone Haiku](https://www.haiku-os.org/development/icon-guidelines/).

- **Modifica**: Sostituisci
- **Formato**: Bordi cella, Formato numero, Convalida dati, Formattazione condizionale
- **Celle e tabella**: Inserisci riga, Inserisci colonna, Unisci celle, Blocca riquadri
- **Formule**: Somma automatica, Percentuale, Funzione (fx)
- **Grafici**: Grafico a barre, Grafico a torta, Grafico a linee

Totale: 49 funzioni coperte dal catalogo, 13 da disegnare.

Le lacune si concentrano su due aree, entrambe attese: le operazioni **specifiche
del foglio di calcolo** (righe/colonne, unisci celle, blocca riquadri, formati
numerici, bordi) e i **tipi di grafico**. Sono funzioni che nessun'altra
applicazione Haiku ha, quindi e' normale che manchino dallo store.

### Nota sulla verifica

Le icone in tabella sono state **controllate a vista**, non scelte per
somiglianza del titolo. Il controllo ha scartato tre falsi positivi che un match
testuale avrebbe accettato:

- **Sum It** (1032, 711): non e' una sommatoria ma il logo dell'omonimo
  spreadsheet BeOS — raffigura un mezzo da lavoro.
- **Chart** (55): icona di tipo file, raffigura un piatto di cibo.
- **Chart File** (218, 219): icone di tipo file, non simboli da toolbar.

Se aggiungi voci a `TOOLBARS`, conviene guardare l'`.svg` prima di fidarsi del titolo.