# Foglio di calcolo nativo per Haiku OS

Applicazione foglio di calcolo (stile Excel) nativa per Haiku OS,
scritta con le API native (Interface/Layout Kit, Locale Kit, Translation
Kit), compatibile con i file generati da Microsoft Excel e
OpenOffice/LibreOffice tramite add-on di conversione basati sul
Translation Kit.

## Stato del progetto

Vedi [ROADMAP.md](ROADMAP.md) per le fasi e lo stato di avanzamento
aggiornato. In sintesi:

- Il progetto riusa e modernizza il motore di calcolo e l'importer
  Excel legacy del vecchio progetto BeOS **Sum-It** (fork community
  `OpenSumIt`), portandolo a compilare su Haiku moderno a 64 bit.
- La UI viene scritta da zero in Interface/Layout Kit (il codice UI
  storico è BeOS pre-Layout-Kit e non viene riusato).
- L'interoperabilità con XLSX/ODS/CSV/XLS passa dal **Translation Kit**
  di Haiku (`BTranslator`/`BTranslatorRoster`), lo stesso meccanismo
  usato per le immagini, con un add-on per formato.

## Struttura del repository

```
legacy/opensumit/   sorgente storico Sum-It/OpenSumIt, patchato per
                     compilare su Haiku 64 bit (vedi PORTING_STATUS.md)
engine/              motore di calcolo estratto e isolato (Fase 2)
translators/         add-on Translation Kit per xlsx/ods/csv/xls (Fase 3)
ui/                  applicazione nativa Interface/Layout Kit (Fase 4)
docs/                ricerca tecnica, architettura, note di porting
```

## Licenza del codice storico

Il codice in `legacy/opensumit/` proviene dal progetto Sum-It
(Copyright 1996-2000 Hekkelman Programmatuur B.V.) ed è distribuito
con licenza BSD a 4 clausole, inclusa la clausola pubblicitaria — vedi
`legacy/opensumit/sum-it/Docs/Licence`. Qualunque distribuzione binaria
che includa questo codice deve rispettare tale clausola.

## Build

Vedi `legacy/opensumit/README` per le istruzioni di build del codice
storico (in corso di stabilizzazione, Fase 1). Le istruzioni di build
per `engine/`, `translators/` e `ui/` verranno aggiunte man mano che
quelle fasi vengono completate.

## Documentazione

- [ROADMAP.md](ROADMAP.md) — fasi del progetto e stato di avanzamento
- [docs/RESEARCH.md](docs/RESEARCH.md) — analisi tecnica di partenza
  (API Haiku, librerie formati file, valutazione SumIt)
- [legacy/opensumit/PORTING_STATUS.md](legacy/opensumit/PORTING_STATUS.md) —
  dettaglio tecnico del porting a 64 bit del codice storico
