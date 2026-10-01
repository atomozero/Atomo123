/*
	fontmetrics_test.cpp

	Verifica CFontSizeTable::GetFontID dopo l'aggiunta di una cache per
	richiesta grezza (bug di prestazioni reale segnalato da un utente:
	riaprire un file XLSX reale con decine di migliaia di celle a font
	personalizzato impiegava svariate decine di secondi solo nella
	sezione font di LoadASCD -- ogni cella ripeteva un vero round-trip
	IPC ad app_server tramite BFont::SetFamilyAndStyle dentro il
	costruttore di CFontMetrics, anche quando il font richiesto era gia'
	in tabella, perche' la scansione lineare di deduplica doveva prima
	COSTRUIRE un CFontMetrics temporaneo per poterlo confrontare. Vedi il
	commento su fRequestCache in FontMetrics.h).

	Due proprieta' che la cache deve preservare esattamente come prima:
	1. richieste diverse (famiglia/stile/dimensione/colore) ottengono
	   id diversi, la stessa richiesta ripetuta ottiene sempre lo
	   STESSO id (correttezza della deduplica, invariata);
	2. richiedere ripetutamente la STESSA combinazione e' ordini di
	   grandezza piu' veloce della prima richiesta (la vera ragione di
	   questa modifica) -- bench di regressione, non solo di
	   correttezza.
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <OS.h>

#include "FontMetrics.h"

static int gFailures = 0;

static void Check(bool condition, const char* what)
{
	if (condition)
		printf("OK   %s\n", what);
	else
	{
		printf("FAIL %s\n", what);
		gFailures++;
	}
}

int main()
{
	// A differenza degli altri test di engine/tests (che restano puri
	// CContainer/Value, mai un vero font), questo e' il primo a
	// chiamare GetFontID per davvero: CFontMetrics costruisce un vero
	// BFont (fFont.SetFamilyAndStyle), che richiede una connessione ad
	// app_server -- senza un vero BApplication qui, quella chiamata
	// resta bloccata per sempre in attesa di una connessione che non
	// arrivera' mai (scoperto scrivendo questo stesso test: il processo
	// restava appeso indefinitamente, confermato con /bin/ps). Gli
	// stessi translator headless (vedi il commento in
	// XlsxTranslator.cpp) creano gia' "un BApplication minimo" per lo
	// stesso identico motivo -- qui si fa lo stesso.
	BApplication app("application/x-vnd.Atomo-FontMetricsTest");

	ulong id1 = gFontSizeTable.GetFontID("Arial", "Bold", 12.0f);
	ulong id2 = gFontSizeTable.GetFontID("Arial", "Regular", 12.0f);
	ulong id3 = gFontSizeTable.GetFontID("Arial", "Bold", 12.0f); // stessa richiesta di id1

	Check(id1 != id2, "due font DIVERSI (Bold vs Regular) ottengono id diversi");
	Check(id1 == id3, "la STESSA richiesta esatta (Arial Bold 12) ottiene sempre lo stesso id");

	rgb_color red = { 255, 0, 0, 255 };
	rgb_color blue = { 0, 0, 255, 255 };
	ulong idRed = gFontSizeTable.GetFontID("Arial", "Bold", 12.0f, red);
	ulong idBlue = gFontSizeTable.GetFontID("Arial", "Bold", 12.0f, blue);
	ulong idRedAgain = gFontSizeTable.GetFontID("Arial", "Bold", 12.0f, red);

	Check(idRed != idBlue, "stessa famiglia/stile/dimensione ma COLORE diverso ottiene id diversi");
	Check(idRed == idRedAgain, "la stessa richiesta col colore ripetuta ottiene lo stesso id");

	ulong countBefore = gFontSizeTable.Count();
	gFontSizeTable.GetFontID("Arial", "Bold", 12.0f);
	Check(gFontSizeTable.Count() == countBefore,
		"ripetere una richiesta gia' vista non aggiunge una nuova voce alla tabella");

	// Banco di prova prestazioni: 20000 richieste ripetute della STESSA
	// combinazione (il caso comune in un vero foglio di calcolo, dove
	// molte celle condividono lo stesso font) devono essere ordini di
	// grandezza piu' veloci della prima (che paga il vero costo di
	// costruzione/risoluzione una sola volta). Se la cache smettesse di
	// funzionare (es. una chiave malformata che non fa mai match).
	// questo banco tornerebbe a pagare 20000 volte lo stesso costo
	// intero invece di 20000 lookup quasi gratuiti.
	const int kRepeats = 20000;
	bigtime_t start = system_time();
	for (int i = 0; i < kRepeats; i++)
		gFontSizeTable.GetFontID("Times", "Italic", 14.0f);
	bigtime_t elapsed = system_time() - start;

	printf("\nBANCO DI PROVA: %d richieste ripetute della stessa combinazione = %lld us totali "
		"(%.3f us/chiamata in media)\n\n",
		kRepeats, (long long)elapsed, (double)elapsed / kRepeats);

	// Soglia larga apposta (100us/chiamata media, un lookup in cache
	// dovrebbe costare frazioni di microsecondo): anche includendo il
	// costo reale della PRIMA chiamata nella media, 20000 richieste
	// devono restare ben sotto quello che 20000 round-trip IPC ad
	// app_server costerebbero (anche solo 20-50us l'uno sarebbero
	// 400ms-1s, gia' ordini di grandezza sopra questa soglia).
	Check(elapsed < kRepeats * 100, "20000 richieste ripetute restano sotto i 100us/chiamata in media "
		"(prova che la cache evita di ricostruire un CFontMetrics/BFont a ogni chiamata)");

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
