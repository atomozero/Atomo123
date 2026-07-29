# Roadmap — foglio di calcolo nativo per Haiku OS

Stato: **Fase 1 in corso**. Aggiornato ad ogni fase completata.

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

## Fase 1 — Stabilizzare il build del codice storico (IN CORSO)

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
- [ ] Restanti ~15-18 file con lo stesso pattern meccanico
      (`long`/`ulong` BeOS R5 -> `int32`/`uint32`/`type_code`/`status_t`
      Haiku moderno) — vedi lista in `legacy/opensumit/PORTING_STATUS.md`
- [ ] Link finale del binario `sum-it` completo
- [ ] Smoke test: avvio applicazione, apertura/salvataggio file nativo,
      calcolo formule base

**Test di congruità/compatibilità di questa fase**: build pulita
(`make` senza errori) di `bsl`, `rez`, `sum-it`; avvio del binario senza
crash; import di un file `.xls` reale di test; verifica che una formula
semplice (`=SOMMA(A1:A3)`) calcoli il risultato corretto.

## Fase 2 — Estrarre il motore di calcolo come libreria isolata

Obiettivo: separare `Formula/`, `Cell/`, `Excel/` (motore + import
legacy) dal resto di `sum-it` (UI, dialoghi, grafici) in una libreria
statica autonoma, senza dipendenze da `BWindow`/`BView`, compilabile e
testabile in isolamento.

- [ ] Individuare e tagliare le dipendenze dirette da classi UI BeOS
      (`CellView`, `CellWindow`) nel codice di `Formula`/`Cell`
- [ ] Creare una API pulita C++ per: caricare un documento, leggere
      celle, scrivere celle, ricalcolare, salvare
- [ ] Scrivere unit test (formule aritmetiche, riferimenti relativi/
      assoluti, funzioni di base, import di un file XLS reale)
- [ ] Documentare l'API in `docs/ENGINE_API.md`

**Test di congruità/compatibilità**: suite di unit test verde su
build sia debug che release; nessun link a `libbe.so` nella libreria
engine (verificabile con `nm`/`objdump` — l'engine non deve dipendere
dall'Application/Interface Kit).

## Fase 3 — Translator Kit: import/export XLSX/ODS/CSV/XLS

Obiettivo: add-on `BTranslator` installabili, uno per formato, che
usano l'engine di Fase 2 e librerie esterne leggere.

- [ ] Translator XLS legacy: riusa l'importer `Excel*.cpp` già portato
- [ ] Translator XLSX: basato su OpenXLSX (BSD-3, C++17, dipendenze
      leggere PugiXML+Zippy/libzip) — valutare porting su Haiku
- [ ] Translator ODS: valutare liborcus (Document Liberation Project,
      storicamente portabile su Haiku out-of-the-box via POSIX) o
      implementazione custom minimale
- [ ] Translator CSV: parser proprio, banale
- [ ] Ogni translator dichiara `Identify()`/`Translate()`/
      `InputFormats()`/`OutputFormats()` secondo il framework
      `BTranslatorRoster`

**Test di congruità/compatibilità**: round-trip per ogni formato
(esporta un documento di test, reimportalo, verifica che i dati
coincidano); import di file reali generati da Excel e da LibreOffice
Calc (non solo generati dal nostro export, per verificare vera
interoperabilità).

## Fase 4 — UI nativa Interface/Layout Kit

Obiettivo: applicazione con griglia celle, editing, formattazione,
grafici base, scritta da zero (non riusando `CellView`/`CellWindow`
BeOS-era), che usa l'engine di Fase 2 e i translator di Fase 3.

- [ ] Finestra principale, griglia celle (`BGridLayout`/vista custom)
- [ ] Editing in-cella, barra formule
- [ ] Menu/toolbar, dialoghi (trova/sostituisci, formattazione, ecc.)
- [ ] Locale Kit: formattazione numeri/valute/date locale-aware
- [ ] Print Kit: stampa/anteprima

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
