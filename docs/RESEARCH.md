# Ricerca tecnica di partenza

Sintesi della ricerca multi-fonte (verificata con processo adversariale
a 3 voti indipendenti) condotta prima di avviare il progetto. Report
completo con tabelle e fonti citate pubblicato come artifact durante la
sessione di analisi iniziale.

## 1. API native Haiku

| Kit | Ruolo |
|---|---|
| Interface Kit + Layout Kit | Griglia celle, toolbar, editing, dialoghi |
| Application Kit | Ciclo di vita, `BMessage` per comandi/undo-redo |
| Storage Kit | I/O file, attributi estesi BFS per metadata |
| Locale Kit | Formattazione numeri/valute/date locale-aware |
| Translation Kit | Import/export plugin-based (vedi sotto) |
| Print Kit | Stampa/anteprima |

### Translation Kit

Documentato ufficialmente come framework generico per conversione dati
tra formati, non limitato alle immagini (il BeBook storico cita un
word processor che lo usa per HTML/PostScript/ASCII). `BTranslator`
è una classe astratta (`Identify()`, `Translate()`, `InputFormats()`,
`OutputFormats()`); `BTranslatorRoster` scopre e carica gli add-on da
`/system/add-ons/Translators`, `/boot/home/config/add-ons/Translators`,
o da variabile d'ambiente/path privato. Un'app può quindi spedire i
propri translator senza installazione system-wide. Nessun gruppo di
formati "spreadsheet" nativo: un translator XLSX/ODS si registra come
tipo MIME custom.

Fonti: haiku-os.org/docs/api/group__translation.html, classBTranslator.html,
classBTranslatorRoster.html, legacy-docs/bebook/TheTranslationKit_Introduction.html.

## 2. Il progetto storico SumIt

- Origine: Maarten Hekkelman / Hekkelman Programmatuur B.V., BeOS,
  1996-2000. Sviluppo cessato, rilasciato come free software.
- Codice sopravvive come fork community `github.com/beos-zealot/OpenSumIt`
  (non adottato in HaikuArchives — verifica diretta: 404).
- Licenza: BSD a 4 clausole con advertising clause (verificata
  leggendo `sum-it/Docs/Licence` e `Docs/Copyright` nel repository —
  non "BSD" generico come ipotizzato inizialmente).
- Verifica empirica di build (vedi `legacy/opensumit/PORTING_STATUS.md`):
  il motore di calcolo e l'importer Excel legacy sono solidi e portabili
  con fix mirati; il tool di build `rez` aveva bug di troncamento
  puntatore a 32 bit, ora corretti.

## 3. Librerie C/C++ per compatibilità file

| Libreria | Linguaggio | Licenza | Lettura | Scrittura | Note |
|---|---|---|---|---|---|
| OpenXLSX | C++17 | BSD-3 | sì | sì | niente grafici/tabelle/hyperlink; DOM in memoria |
| libxlsxwriter | ANSI C | FreeBSD | no | sì | solo scrittura, dichiarato esplicitamente |
| xlnt | C++14+ | MIT | parziale | parziale | solo XLSX |
| liborcus | C++ | MPL | sì (in sviluppo) | limitata | Document Liberation Project, nato per LibreOffice |

Nessuna libreria singola copre l'intero ciclo lettura+scrittura con
feature complete: serve integrazione di più librerie o sviluppo
custom per grafici/tabelle/hyperlink/XLS legacy.

### Precedente storico: porting LibreOffice su Haiku (FOSDEM 2018)

Le librerie del Document Liberation Project si sono compilate
"out-of-the-box" su Haiku grazie a compatibilità POSIX (unica eccezione
minore: libmwaw/xattr). Il vero collo di bottiglia storico è stato
integrare il toolkit UI estraneo (VCL di LibreOffice), non il parsing
dei formati file — a conferma che scrivere la UI nativa in
Interface/Layout Kit (invece di portare un toolkit estraneo) è la
scelta giusta.

## 4. Raccomandazione architetturale

Separare un motore di calcolo core (parser formule + grafo di
dipendenze celle, testabile in isolamento) dalla UI nativa Haiku, con
il Translation Kit come layer plugin per i formati file. Vedi
`ROADMAP.md` per come questa raccomandazione si traduce in fasi
concrete, ora rafforzata da porting empirico reale (non solo teoria).
