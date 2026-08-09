# Guida rapida

L'essenziale per iniziare in un minuto. Per tutto il resto vedi
`docs/USER_GUIDE.md` (guida completa) o `ROADMAP.md` (stato del
progetto).

## Cos'è

Atomo123 è un foglio di calcolo nativo per Haiku OS, compatibile con
i file di Excel (XLS/XLSX) e di OpenOffice/LibreOffice Calc (ODS/CSV).
Basta fare doppio click su uno di questi file in Tracker per aprirlo.

## Le basi

- **Scrivere in una cella**: click per selezionarla, poi si digita
  direttamente (il testo sostituisce il contenuto esistente). Invio
  conferma e passa alla cella sotto, come in Excel; Esc annulla.
- **Formule**: cominciano con `=`, es. `=SUM(A1:A3)` (i nomi delle
  funzioni sono in inglese, non `SOMMA`/`SE`/`MEDIA`). Fra più
  argomenti si usa il **punto e virgola** (`;`), non la virgola.
- **Salvare**: File → Salva con nome…, scegliendo l'estensione nel
  nome del file (`.xlsx`, `.ods`, `.csv`, o nessuna per il formato
  nativo ASCD).
- **Più fogli**: le schede in fondo alla finestra funzionano come in
  Excel/LibreOffice Calc — clic per cambiare foglio, tasto destro per
  aggiungerne/rinominarne/eliminarne uno.
- **Il footer**: in basso a sinistra dice se si sta modificando una
  cella ("Modifica") o no ("Pronto"); a destra mostra Somma/Media/
  Conteggio della selezione — tasto destro per scegliere quali
  statistiche vedere.

## Scorciatoie principali

Ctrl+C/Ctrl+X/Ctrl+V (copia/taglia/incolla), Ctrl+Z/Ctrl+Y
(annulla/ripristina), Ctrl+F (trova e sostituisci), Tab/Maiusc+Tab e
Invio/Maiusc+Invio (spostamento), Ctrl+Inizio/Ctrl+Fine (prima/ultima
cella con dati).

## Se qualcosa non va

Per aprire CSV/XLS/XLSX/ODS serve installare i translator una tantum
— vedi "Quick start" in `README.md`. Per tutto il resto (bug noti,
limiti attuali, roadmap) vedi `ROADMAP.md`.
