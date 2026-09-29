/*
	NamedStyle.h

	Stili cella con nome + tavolozza tema (Tier 4, "Path to full Excel
	parity", "Named cell styles + live theme palette"): una CellStyle
	(vedi CellStyle.h) puo' ora referenziare uno stile con nome tramite
	CellStyle::fNamedStyleID invece di portare colori/font letterali --
	CContainer::GetCellStyle (Container.styles.cpp) risolve quell'indice
	DAL VIVO a ogni lettura, consultando la definizione CORRENTE dello
	stile e il tema attivo. Ridefinire uno stile, o cambiare il tema
	attivo, cambia quindi l'aspetto di OGNI cella che lo referenzia al
	prossimo ridisegno, senza toccare una sola cella -- e' l'intero
	meccanismo del "vivo" richiesto da questa fase.

	Concetto di CARTELLA DI LAVORO (una NamedStyleTable/ThemePalette
	sola per l'intero file, mai per foglio, vedi ISheetResolver in
	Container.h): vive su MainWindow, non su CContainer, che vi accede
	solo tramite il puntatore fSheetResolver gia' esistente per la
	stessa identica ragione (risoluzione tra fogli).

	SEMPLIFICAZIONE dichiarata rispetto al vero Excel: elenco built-in
	curato (~17 voci, non le ~50 reali di Excel, stesso principio di
	scope gia' dichiarato per TableStyles.h), tavolozza tema a 8 ruoli
	fissi (non i 12 slot completi dell'"Office Theme" con varianti
	chiare/scure separate), e un NamedStyleDef copre SOLO sfondo/testo/
	allineamento/grassetto/corsivo/sottolineato -- bordi e protezione
	(CellStyle::fLocked/fHidden/bordi) NON sono modellati da uno stile
	con nome in questa fase, restano sempre quelli letterali della
	cella. Persistenza XLSX (theme1.xml/<cellStyles>) esplicitamente
	FUORI scope: solo il formato nativo .ascd.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#ifndef NAMED_STYLE_H
#define NAMED_STYLE_H

#include <string>
#include <vector>

#include <GraphicsDefs.h>

#include "CellStyle.h"

// Un set FISSO e PICCOLO di ruoli con nome, non i 12 slot completi del
// vero "Office Theme" di Excel (2 testo + 2 sfondo + 6 accenti + 2
// hyperlink) -- questa app non ha bisogno di un colore hyperlink
// separato ne' di varianti chiare/scure indipendenti in questa fase, e
// ogni stile con nome che referenzia un accento lo fa con uno di
// eAccent1..eAccent6, sufficiente per coprire l'intero elenco
// "Accent1".."Accent6" di Excel uno a uno.
enum ThemeColorRole {
	eThemeText,        // testo "normale" del corpo del foglio
	eThemeBackground,  // sfondo "normale" del corpo del foglio
	eThemeAccent1,
	eThemeAccent2,
	eThemeAccent3,
	eThemeAccent4,
	eThemeAccent5,
	eThemeAccent6,
	kThemeColorRoleCount
};

// Tavolozza tema DELL'INTERO documento (Tier 4): un colore per ruolo,
// VIVA -- scambiare colors[eThemeAccentN] si ripercuote su ogni
// NamedStyleDef che referenzia quel ruolo (vedi useThemeBackground/
// useThemeText sotto) al prossimo ridisegno, senza toccare nessuna
// cella. Non persiste un "nome" di tema (niente "Office"/"Facet" ecc.,
// un solo tema attivo alla volta): solo gli 8 colori stessi.
struct ThemePalette {
	rgb_color colors[kThemeColorRoleCount];

	ThemePalette(); // riempie con 8 colori Office-like di default
};

// Uno stile cella con nome (Tier 4): un bundle di formattazione
// applicabile in un colpo solo, per NOME. Quando una CellStyle
// referenzia questo (fNamedStyleID != 0), TUTTI i campi qui sotto
// diventano l'intero aspetto visivo della cella (sostituzione
// integrale, mai un merge parziale, vedi il commento su
// CellStyle::fNamedStyleID) -- borde/protezione/formato numerico
// restano invece quelli letterali della cella, non modellati qui (vedi
// il commento in cima al file).
struct NamedStyleDef {
	std::string name;
	// true = uno dei kBuiltInStyleCount predefiniti (mai rinominabile
	// ne' eliminabile, stesso principio delle tabelle pivot/nomi
	// riservati altrove in questo motore); false = personalizzato,
	// creato dall'utente.
	bool builtIn;

	// useThemeXxx sceglie se lo sfondo/testo e' un RUOLO della
	// tavolozza tema (segue il tema quando cambia, es. i built-in
	// Accent1-6) oppure un colore letterale fisso (comportamento "ho
	// scelto proprio questo colore", tipico di uno stile personalizzato
	// catturato da una cella).
	bool useThemeBackground;
	ThemeColorRole backgroundRole;
	rgb_color backgroundColor; // usato solo se useThemeBackground == false

	bool useThemeText;
	ThemeColorRole textRole;
	rgb_color textColor; // usato solo se useThemeText == false

	char alignment; // EAlignment (CellStyle.h)
	bool bold;
	bool italic;
	bool underline;

	NamedStyleDef();
	// Applica questa definizione (risolvendo gli eventuali ruoli tema
	// contro "palette") SOPRA "base", restituendo una CellStyle
	// completa pronta per il disegno -- base fornisce fFont (famiglia/
	// dimensione, solo lo STILE grassetto/corsivo viene sovrascritto,
	// stessa tecnica di MainWindow::ToggleBold/ApplyFontToRange) e ogni
	// campo non modellato qui (bordi, fLocked/fHidden, fFormat,
	// fWrapText). Unico punto in cui "definizione dello stile" e "tema
	// corrente" si combinano davvero, usato da
	// CContainer::GetCellStyle.
	CellStyle Resolve(const CellStyle& base, const ThemePalette& palette) const;
};

// Tabella degli stili con nome dell'INTERA cartella di lavoro (Tier 4):
// vive UNA sola volta su MainWindow (vedi ISheetResolver in
// Container.h), MAI duplicata per foglio. Gli ID sono la posizione nel
// vettore interno +1 (0 riservato per "nessuno stile"), STABILI per
// tutta la sessione -- si aggiunge sempre in fondo, mai si riordina,
// cosi' un int gia' scritto in una CellStyle::fNamedStyleID resta
// valido anche dopo che altri stili vengono aggiunti/rimossi.
class NamedStyleTable {
public:
	NamedStyleTable(); // ResetToBuiltIns()

	// Svuota e ricostruisce SOLO i built-in (kBuiltInStyleCount voci,
	// ID 1..kBuiltInStyleCount) -- usato per un documento nuovo/appena
	// aperto senza una sezione stili propria nel file.
	void ResetToBuiltIns();

	// -1 se nessuno stile ha questo nome esatto (case-sensitive, stesso
	// principio di ogni altro confronto per nome in questo motore).
	int FindByName(const std::string& name) const;
	// Aggiunge un nuovo stile PERSONALIZZATO in fondo (builtIn forzato
	// a false indipendentemente da def.builtIn). Ritorna il nuovo ID.
	int AddCustom(const NamedStyleDef& def);
	// Sovrascrive la definizione di uno stile ESISTENTE (built-in o
	// personalizzato) allo STESSO ID -- questo E' il punto "vivo":
	// ogni cella che referenzia styleID cambia aspetto al prossimo
	// ridisegno, senza che nessuna CellStyle::fNamedStyleID cambi.
	// false se styleID non esiste.
	bool Redefine(int styleID, const NamedStyleDef& def);
	// Rimuove uno stile PERSONALIZZATO (false, nessun effetto, se
	// styleID e' 0/fuori range/gia' rimosso/built-in.). Una cella che
	// referenziava questo ID ricade sul suo aspetto letterale (vedi
	// CContainer::GetCellStyle, Get() sotto ritorna NULL).
	bool Remove(int styleID);

	// Numero di stili DEFINITI e non rimossi (esclude lo slot 0
	// "nessuno" e ogni Remove()d) -- IDAtIndex(i) per i in
	// [0, Count()) enumera esattamente questi, mai un ID rimosso.
	int Count() const;
	// L'ID (1-based) dello stile alla posizione "index" (0-based) fra
	// quelli DEFINITI e non rimossi -- per popolare un elenco/galleria
	// in ordine.
	int IDAtIndex(int index) const;

	// NULL se styleID e' 0, fuori range, o e' stato rimosso.
	const NamedStyleDef* Get(int styleID) const;

private:
	// fStyles[i] ha ID i+1; una voce rimossa diventa builtIn=false con
	// un nome vuoto e "removed=true" (mai un vero erase(), altrimenti
	// ogni ID successivo si sposterebbe e romperebbe ogni
	// CellStyle::fNamedStyleID gia' scritto altrove).
	struct Entry {
		NamedStyleDef def;
		bool removed;
	};
	std::vector<Entry> fStyles;
};

#endif
