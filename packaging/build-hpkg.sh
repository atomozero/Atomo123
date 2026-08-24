#!/bin/sh
# build-hpkg.sh
#
# Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
# radice del repository).
#
# Costruisce un .hpkg locale per un primo test di installazione, senza
# passare da haikuporter/atomo123-0.2.6.recipe: quella recipe richiede
# un tag + checksum SHA256 reale dell'archivio sorgente pubblicato su
# GitHub per QUESTA versione, che non esistono finche' non si taggia e
# pubblica la release (vedi i commenti nella recipe stessa). Questo
# script invece compila da questo stesso albero di lavoro e
# impacchetta i binari appena prodotti — comodo per verificare che il
# pacchetto si costruisca e si installi bene PRIMA di taggare la
# release, e per allegare l'hpkg risultante alla release stessa su
# GitHub (stessa procedura gia' usata per v0.1.0).
#
# Uso:
#   packaging/build-hpkg.sh
#
# Il pacchetto risultante va provato con:
#   cp atomo123-0.2.6-1-x86_64.hpkg ~/config/packages/   # solo per l'utente corrente
# oppure, per tutto il sistema (richiede privilegi):
#   cp atomo123-0.2.6-1-x86_64.hpkg /boot/system/packages/
# packagefs lo monta automaticamente, senza bisogno di riavviare.

set -e

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
VERSION="0.2.6-1"
OUT="$ROOT/packaging/atomo123-$VERSION-x86_64.hpkg"
WORKDIR="$ROOT/packaging/hpkg-root"

echo "== Ricompilazione (engine, quattro translator, UI) =="
make -C engine
for t in csv xls xlsx ods; do
	make -C translators/$t
done
make -C ui

echo "== Preparazione dell'albero del pacchetto =="
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR/apps/Atomo123"
mkdir -p "$WORKDIR/add-ons/Translators"
cp ui/Atomo123 "$WORKDIR/apps/Atomo123/Atomo123"
cp translators/csv/CsvTranslator "$WORKDIR/add-ons/Translators/"
cp translators/xls/XlsTranslator "$WORKDIR/add-ons/Translators/"
cp translators/xlsx/XlsxTranslator "$WORKDIR/add-ons/Translators/"
cp translators/ods/OdsTranslator "$WORKDIR/add-ons/Translators/"

cat > "$WORKDIR/.PackageInfo" <<EOF
name			atomo123
version			$VERSION
architecture		x86_64
summary			"Foglio di calcolo nativo per Haiku OS"
description		"Atomo123 e' un'applicazione foglio di calcolo (stile Excel) nativa per Haiku OS, scritta con le API native (Interface/Layout Kit, Locale Kit, Print Kit, Translation Kit). E' compatibile con i file generati da Microsoft Excel (XLS/XLSX) e OpenOffice/LibreOffice (ODS/CSV) tramite add-on di conversione basati sul Translation Kit di sistema. Il motore di calcolo e l'importer Excel legacy riusano e modernizzano il codice storico BeOS del progetto Sum-It, portato a compilare su Haiku moderno a 64 bit; la UI e' scritta da zero in Interface/Layout Kit."
packager		"Andrea Bernardi <atomozero@proton.me>"
vendor			"Andrea Bernardi"
licenses {
	"MIT"
	"BSD (4-clause)"
}
copyrights {
	"2026 Andrea Bernardi"
	"1996-2000 Hekkelman Programmatuur B.V."
}
provides {
	atomo123 = 0.2.6
	app:Atomo123 = 0.2.6
}
requires {
	haiku
}
urls {
	"https://github.com/atomozero/Atomo123"
}
EOF

echo "== Creazione del pacchetto =="
rm -f "$OUT"
(cd "$WORKDIR" && package create -v "$OUT")

rm -rf "$WORKDIR"

echo
echo "Pacchetto creato: $OUT"
