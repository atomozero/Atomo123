/*
	OfficeCrypto.h

	Decifratura di un file XLSX protetto da PASSWORD DI APERTURA
	(workbook open-password encryption, Path to full Excel parity Tier
	4 -- gestione completa delle password, la parte separata e piu'
	grande dall'hash di protezione foglio gia' fatto in questa stessa
	sessione). Solo Agile Encryption (Excel 2010+, MS-OFFCRYPTO
	2.3.4.x) -- "Standard Encryption" (2007-2010, EncryptionInfo
	binario invece che XML) resta fuori scope, scelta esplicita
	dell'utente: Agile copre la stragrande maggioranza dei file
	protetti reali. Solo LETTURA: riesportare un file aperto cosi'
	perde la cifratura (torna un .xlsx normale), stessa scelta di scope
	gia' fatta per l'hash di protezione foglio verso strumenti esterni.

	Un file XLSX cifrato non e' affatto uno ZIP: e' un intero
	contenitore OLE2/CFB (vedi CompoundFileReader.h) con due stream,
	"EncryptionInfo" (un piccolo header binario piu' un XML che descrive
	come e' cifrato tutto il resto) e "EncryptedPackage" (il vero pacchetto
	ZIP/OOXML, cifrato AES-CBC a segmenti). L'algoritmo di derivazione
	chiave (hash iterato SHA-512) e' lo STESSO identico usato per l'hash
	di protezione foglio (ExcelPasswordHash.h), solo con costanti "block
	key" diverse mescolate dopo -- vedi il commento su
	DecryptAgileEncryptedXlsx sotto per il resto della catena.

	Verificato: l'algoritmo qui sotto e' stato controllato scrivendo
	PRIMA una coppia indipendente cifra/decifra in Python (pycryptodome,
	dato che nessuno strumento di riferimento esistente -- msoffcrypto-
	tool -- si installa in questo ambiente) e un vero file di prova
	cifrato con una password nota, committato come
	translators/xlsx/tests/sample_encrypted.xlsx -- vedi il test
	dedicato. Resta comunque un'implementazione nuova di una specifica
	pubblica, non verificata contro un vero file scritto da Excel stesso
	(nessuno disponibile in questo ambiente).

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef OFFICE_CRYPTO_H
#define OFFICE_CRYPTO_H

#include <string>
#include <vector>

#include <SupportDefs.h>

// "fileData"/"fileLen" e' l'INTERO file OLE2/CFB (non solo uno stream
// -- vedi CompoundFileReader.h, usato internamente per estrarre
// "EncryptionInfo"/"EncryptedPackage"). Vero solo se la password e'
// quella giusta (verificata contro encryptedVerifierHashInput/Value
// PRIMA di fidarsi di "outPlainZip", non dopo un tentativo di
// decifratura andato storto in silenzio) -- in quel caso "outPlainZip"
// e' il vero pacchetto ZIP/OOXML, pronto per CZipReader/ParseSheet
// esattamente come un file .xlsx mai stato cifrato. Falso per: file non
// OLE2, non Agile Encryption (es. "Standard Encryption" legacy, fuori
// scope), XML di EncryptionInfo malformato, o password sbagliata --
// nessuna distinzione fra questi casi nel valore di ritorno (il
// chiamante non ha comunque nulla di diverso da fare per nessuno di
// questi, vedi MainWindow).
bool DecryptAgileEncryptedXlsx(const uint8* fileData, size_t fileLen,
	const std::string& password, std::vector<uint8>* outPlainZip);

#endif
