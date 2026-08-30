/*
	CsvTranslator.cpp

	Vedi CsvTranslator.h per la descrizione generale.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "CsvTranslator.h"

#include <cstdio>
#include <cstring>

#include <Catalog.h>
#include <Cursor.h>
#include <Entry.h>
#include <Font.h>
#include <image.h>
#include <InterfaceDefs.h>
#include <LayoutBuilder.h>
#include <Roster.h>
#include <String.h>
#include <StringView.h>
#include <View.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "CellIterator.h"
#include "Text2Cells.h"
#include "FunctionUtils.h"

static const translation_format sInputFormats[] = {
	{
		kAtomoCsvFormat, kAtomoSheetGroup, 0.8f, 0.8f,
		"text/csv", "Comma-Separated Values (CSV)"
	},
	{
		kAtomoNativeFormat, kAtomoSheetGroup, 1.0f, 1.0f,
		"application/x-vnd.atomo-sheet-data", "Atomo Sheet Cell Data (ASCD)"
	}
};

static const translation_format sOutputFormats[] = {
	{
		kAtomoCsvFormat, kAtomoSheetGroup, 0.8f, 0.8f,
		"text/csv", "Comma-Separated Values (CSV)"
	},
	{
		kAtomoNativeFormat, kAtomoSheetGroup, 1.0f, 1.0f,
		"application/x-vnd.atomo-sheet-data", "Atomo Sheet Cell Data (ASCD)"
	}
};

static const char kASCDMagic[4] = { 'A', 'S', 'C', 'D' };
// Versione 2 (era 1): aggiunge un byte "kind" per cella -- vedi il
// commento su kASCDVersion in ui/src/AscdIO.cpp e nello stesso punto
// di translators/xlsx/XlsxTranslator.cpp (dove il bug gemello e' gia'
// stato corretto, commit 670425b). Senza questo byte, un valore testo
// AMBIGUO (es. "P-EL-a": tre nomi non definiti concatenati da un
// "meno", sintatticamente un'espressione valida) sopravviveva
// all'importazione CSV in memoria ma tornava a corrompersi in una
// formula che calcola NaN non appena WriteASCD lo serializzava e
// ReadASCD lo rileggeva -- il testo grezzo da solo non basta a
// distinguere "era gia' una formula" da "era solo testo che ASSOMIGLIA
// a una formula". Bug reale confermato con un test dedicato
// (tests/test_csv_translator.cpp) prima di questo fix: il testo
// spariva del tutto dal CSV ri-esportato, sostituito dal risultato
// (NaN/errore) della formula spuria.
static const int32 kASCDVersion = 2;
enum { kAscdCellFormula = 0, kAscdCellLiteralOther = 1, kAscdCellLiteralText = 2 };

// Scrive in "dest" tutte le celle non vuote di "doc" nel formato ASCD:
// magic(4) + versione(int32) + numero celle(int32), poi per ciascuna
// cella: riga(int16) colonna(int16) lunghezza testo(int32) kind(uint8)
// testo. Il testo e' quanto restituito da GetCellFormula: la formula
// con "=" se presente, altrimenti il valore formattato — cosi' una
// formula sopravvive al round-trip nativo (a differenza dell'export
// CSV, dove e' corretto che venga "appiattita" al valore calcolato).
static status_t WriteASCD(CContainer* doc, BPositionIO* dest)
{
	// Range completo invece dei limiti di GetBounds: una cella con
	// formula non ancora calcolata (mType eNoData) verrebbe esclusa
	// dai limiti calcolati da GetBounds, e se e' anche la cella piu' a
	// destra/in basso del foglio sparirebbe del tutto dal file
	// prodotto (stesso ragionamento del ciclo di ricalcolo qui sotto).
	int32 count = 0;
	CCellIterator counter(doc, NULL);
	cell c;
	while (counter.NextExisting(c))
		count++;

	if (dest->Write(kASCDMagic, 4) != 4)
		return B_IO_ERROR;
	if (dest->Write(&kASCDVersion, sizeof(kASCDVersion)) != (ssize_t)sizeof(kASCDVersion))
		return B_IO_ERROR;
	if (dest->Write(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_IO_ERROR;

	CCellIterator iter(doc, NULL);
	while (iter.NextExisting(c))
	{
		char text[4096];
		doc->GetCellFormula(c, text, sizeof(text), false);

		int16 row = c.v, col = c.h;
		int32 len = strlen(text);

		// "kind": vedi il commento su kASCDVersion sopra -- senza
		// questo byte, un valore testo AMBIGUO importato da CSV (es.
		// "P-EL-a") sopravviveva in memoria ma tornava a corrompersi
		// in una formula NaN al primo giro salva/ricarica ASCD, perche'
		// ReadASCD (sotto) non ha modo di distinguerlo da una vera
		// formula guardando solo il testo.
		uint8 kind;
		if (doc->GetCellFormula(c) != NULL)
			kind = kAscdCellFormula;
		else
		{
			Value v;
			doc->GetValue(c, v);
			kind = (v.fType == eTextData) ? kAscdCellLiteralText : kAscdCellLiteralOther;
		}

		if (dest->Write(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_IO_ERROR;
		if (dest->Write(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_IO_ERROR;
		if (dest->Write(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_IO_ERROR;
		if (dest->Write(&kind, sizeof(kind)) != (ssize_t)sizeof(kind))
			return B_IO_ERROR;
		if (len > 0 && dest->Write(text, len) != len)
			return B_IO_ERROR;
	}

	return B_OK;
}

// Legge un flusso ASCD e ricostruisce le celle in "doc" (vuoto in
// ingresso), riusando TryToParseString che gia' distingue formule da
// valori costanti.
static status_t ReadASCD(BPositionIO* source, CContainer* doc)
{
	char magic[4];
	if (source->Read(magic, 4) != 4)
		return B_BAD_DATA;
	if (memcmp(magic, kASCDMagic, 4) != 0)
		return B_BAD_DATA;

	int32 version;
	if (source->Read(&version, sizeof(version)) != (ssize_t)sizeof(version))
		return B_BAD_DATA;
	// ui/src/AscdIO.cpp scrive ormai sempre versione 3 (kASCDVersion li',
	// i punti di controllo della scala di colori per la formattazione
	// condizionale -- vedi il commento su kASCDVersion in quel file), ma
	// un file .ascd piu' vecchio (versione 1 genuina, o 2 col solo byte
	// "kind" per cella) resta comunque leggibile: kASCDMaxReadableVersion
	// e' un limite SEPARATO da kASCDVersion apposta, non lo stesso
	// valore (confonderli, come nel primo tentativo di questo fix,
	// "version != 1 && version != kASCDVersion" con kASCDVersion=1
	// equivalente a "version != 1", avrebbe accettato SOLO la versione
	// 1, facendo fallire l'export verso questo formato con
	// B_MISMATCHED_VALUES per qualunque documento reale scritto dalla
	// nuova WriteASCD). Questa ReadASCD non legge MAI la sezione
	// formattazione condizionale (si ferma subito dopo le celle, vedi
	// sotto), quindi il nuovo campo per-regola introdotto in versione 3
	// non serve saltarlo qui: basta accettare la versione, non serve
	// altro.
	static const int32 kASCDMaxReadableVersion = 3;
	if (version < 1 || version > kASCDMaxReadableVersion)
		return B_MISMATCHED_VALUES;

	int32 count;
	if (source->Read(&count, sizeof(count)) != (ssize_t)sizeof(count))
		return B_BAD_DATA;

	for (int32 i = 0; i < count; i++)
	{
		int16 row, col;
		int32 len;

		if (source->Read(&row, sizeof(row)) != (ssize_t)sizeof(row))
			return B_BAD_DATA;
		if (source->Read(&col, sizeof(col)) != (ssize_t)sizeof(col))
			return B_BAD_DATA;
		if (source->Read(&len, sizeof(len)) != (ssize_t)sizeof(len))
			return B_BAD_DATA;

		uint8 kind = kAscdCellFormula;
		if (version >= 2 && source->Read(&kind, sizeof(kind)) != (ssize_t)sizeof(kind))
			return B_BAD_DATA;

		char text[4096];
		if (len < 0 || len >= (int32)sizeof(text))
			return B_BAD_DATA;
		if (len > 0 && source->Read(text, len) != len)
			return B_BAD_DATA;
		text[len] = 0;

		cell c(col, row);

		// Stesso principio di LoadASCD in ui/src/AscdIO.cpp: un valore
		// testo letterale (kind scritto da una versione 2 di WriteASCD)
		// non passa MAI per TryToParseString/Parse().
		if (kind == kAscdCellLiteralText)
		{
			Value v(text);
			doc->NewCell(c, v, NULL);
			continue;
		}

		try
		{
			// inWarnIfError=false, non true: stesso bug e stesso motivo
			// di LoadASCD in ui/src/AscdIO.cpp -- un valore TESTO che
			// assomiglia abbastanza a un numero/data da superare
			// l'analisi grammaticale del parser ma poi fallisce a
			// ridursi a un valore (es. "01.11.10", un codice ATECO
			// reale) fa rilanciare l'eccezione invece di ripiegare sul
			// testo originale, e il catch sotto trasformava l'intero
			// export in un fallimento totale (B_BAD_DATA) per una sola
			// cella di testo innocua.
			TryToParseString(text, c, doc, false);
		}
		catch (...)
		{
			return B_BAD_DATA;
		}
	}

	// TryToParseString imposta la formula/il valore di ogni cella ma
	// non la calcola: CFormula::Calculate legge i riferimenti ad
	// altre celle con una semplice GetValue (non ricorsiva), quindi
	// senza questo passo l'esportazione CSV di una formula (che deve
	// scrivere il valore calcolato, non il testo della formula)
	// risulterebbe vuota. Piu' passate finche' nessuna cella cambia
	// piu' valore propagano correttamente le dipendenze in qualunque
	// ordine siano state inserite; limite di passate per non restare
	// bloccati su un riferimento circolare.
	//
	// L'iteratore usa il range completo del foglio (non i limiti
	// restituiti da GetBounds) perche' GetBounds esclude le celle con
	// mType eNoData -- esattamente lo stato di una formula appena
	// analizzata da TryToParseString e non ancora calcolata. Se quella
	// cella e' anche la piu' a destra/in basso del foglio (nessun'altra
	// cella "reale" oltre di lei), i limiti calcolati la escluderebbero
	// e non verrebbe mai visitata da NextExisting, restando vuota per
	// sempre. NextExisting resta comunque efficiente su un range pieno:
	// salta direttamente da una cella esistente alla successiva tramite
	// la mappa, senza scandire le celle vuote in mezzo.
	{
		bool changed = true;
		int guard = 0;
		while (changed && guard < 50)
		{
			changed = false;
			CCellIterator recalcIter(doc, NULL);
			cell rc;
			while (recalcIter.NextExisting(rc))
			{
				if (doc->CalcCell(rc))
					changed = true;
			}
			guard++;
		}
	}

	return B_OK;
}

CCsvTranslator::CCsvTranslator()
	: BTranslator()
{
}

CCsvTranslator::~CCsvTranslator()
{
}

const char* CCsvTranslator::TranslatorName() const
{
	return "CSV Translator";
}

const char* CCsvTranslator::TranslatorInfo() const
{
	return "Converte fogli di calcolo tra CSV e il formato dati nativo Atomo Sheet";
}

int32 CCsvTranslator::TranslatorVersion() const
{
	return B_TRANSLATION_MAKE_VERSION(1, 0, 0);
}

// Catalogo di localizzazione di QUESTO add-on, non dell'app ospite: un
// translator puo' essere caricato da qualunque host (Atomo123,
// DataTranslations, Tracker per le anteprime), ognuno col PROPRIO
// catalogo -- mai il nostro. Il B_CATALOG predefinito di Catalog.h
// userebbe BLocaleRoster::Default()->GetCatalog() (il catalogo
// dell'HOST), che non contiene affatto le nostre chiavi: si punta
// invece esplicitamente al catalogo incorporato in QUESTO file .so,
// risolto in make_nth_translator sotto tramite l'image_id ricevuto
// (BCatalog(entry_ref), pensato apposta per gli add-on che non sono
// una BApplication con un proprio be_app).
static BCatalog sCatalog;
#undef B_CATALOG
#define B_CATALOG (&sCatalog)
#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "CsvConfigView"

// Link "Offrimi un caffe'" cliccabile, stesso URL/icona gia' presenti
// nella finestra Informazioni dell'app (ui/src/AboutWindow.cpp) --
// qui pero' non c'e' una BWindow nostra che possa ricevere un
// BMessage al click (questa vista vive nella finestra del pannello
// Translators, non controllata da questo add-on), quindi il click
// apre direttamente l'URL invece di passare da un messaggio postato
// alla finestra come fa ClickableStringView in ui/src/.
static const char kCoffeeUrl[] = "https://buymeacoffee.com/atomozero";

class CCsvCoffeeLink : public BStringView {
public:
	CCsvCoffeeLink() : BStringView("coffeeLink", B_TRANSLATE("Offrimi un caffe' \xE2\x98\x95")) {}

	virtual void AttachedToWindow()
	{
		BStringView::AttachedToWindow();
		SetHighColor(40, 80, 200);
	}

	virtual void MouseDown(BPoint)
	{
		const char* arg = kCoffeeUrl;
		be_roster->Launch("application/x-vnd.Be.URL.https", 1, const_cast<char**>(&arg));
	}

	virtual void MouseMoved(BPoint, uint32, const BMessage*)
	{
		BCursor link(B_CURSOR_ID_FOLLOW_LINK);
		SetViewCursor(&link);
	}
};

// Vista "Informazioni" mostrata dal pannello Translators di Haiku
// quando si seleziona questo add-on (MakeConfigurationView sotto):
// prima di questo il pannello restava vuoto, la classe base BTranslator
// non fornisce nessuna vista di default. Nessuna vera opzione di
// configurazione da esporre (il translator non ne ha), quindi la vista
// mostra solo nome/versione/descrizione -- stesso testo gia'
// restituito da TranslatorName/TranslatorInfo/TranslatorVersion sopra,
// solo reso visivamente invece che lasciato leggibile solo via API.
class CCsvConfigView : public BView {
public:
	CCsvConfigView(BRect frame)
		:
		BView(frame, "CsvConfigView", B_FOLLOW_ALL, B_WILL_DRAW)
	{
		SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));

		BStringView* title = new BStringView("title", "CSV Translator");
		title->SetFont(be_bold_font);

		int32 v = B_TRANSLATION_MAKE_VERSION(1, 0, 0);
		BString versionText;
		versionText.SetToFormat(B_TRANSLATE("Versione %d.%d.%d"),
			(int)B_TRANSLATION_MAJOR_VERSION(v), (int)B_TRANSLATION_MINOR_VERSION(v),
			(int)B_TRANSLATION_REVISION_VERSION(v));

		BFont small(be_plain_font);
		small.SetSize(be_plain_font->Size() - 1);

		BStringView* version = new BStringView("version", versionText.String());
		version->SetFont(&small);
		version->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.7));

		BStringView* info = new BStringView("info",
			B_TRANSLATE("Converte fogli di calcolo tra CSV e il formato dati\n"
				"nativo Atomo Sheet (ASCD)."));

		BStringView* copyright = new BStringView("copyright",
			B_TRANSLATE("Copyright (c) 2026 Andrea Bernardi \xC2\xB7 Licenza MIT"));
		copyright->SetFont(&small);
		copyright->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.6));

		CCsvCoffeeLink* coffeeLink = new CCsvCoffeeLink();
		coffeeLink->SetFont(&small);

		BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
			.SetInsets(B_USE_WINDOW_INSETS)
			.Add(title)
			.Add(version)
			.AddStrut(B_USE_SMALL_SPACING)
			.Add(info)
			.AddGlue()
			.Add(coffeeLink)
			.Add(copyright)
		.End();
	}
};

status_t CCsvTranslator::MakeConfigurationView(BMessage* extension, BView** _view,
	BRect* _extent)
{
	if (_view == NULL || _extent == NULL)
		return B_BAD_VALUE;

	CCsvConfigView* view = new CCsvConfigView(BRect(0, 0, 279, 134));
	*_view = view;
	*_extent = view->Bounds();
	return B_OK;
}

const translation_format* CCsvTranslator::InputFormats(int32* _count) const
{
	*_count = 2;
	return sInputFormats;
}

const translation_format* CCsvTranslator::OutputFormats(int32* _count) const
{
	*_count = 2;
	return sOutputFormats;
}

// CSV non ha una firma propria (a differenza di ASCD/XLS/XLSX/ODS):
// un formato di testo puro non ha niente all'inizio del file che lo
// distingua da un altro formato di testo. Prima di questo controllo,
// Identify() accettava incondizionatamente QUALUNQUE contenuto come
// "forse CSV" (qualita' 0.6) -- bug reale, non solo teorico: un JPEG
// (o qualunque altro binario) veniva "riconosciuto" come CSV con
// priorita' sufficiente a battere JPEGTranslator in
// BTranslatorRoster::Translate() con B_TRANSLATOR_BITMAP come uscita,
// che poi falliva con "No translator found" perche' CsvTranslator non
// sa produrre un bitmap -- scoperto verificando perche' un'immagine
// incorporata in un file XLS non veniva mai disegnata nonostante i
// byte estratti fossero un JPEG valido. Un contenuto binario (byte
// NUL, o troppi byte di controllo non tipici del testo) viene quindi
// respinto qui invece di essere accettato come "forse CSV".
static bool LooksLikeText(BPositionIO* source)
{
	off_t pos = source->Position();
	unsigned char sample[512];
	ssize_t read = source->Read(sample, sizeof(sample));
	source->Seek(pos, SEEK_SET);

	if (read <= 0)
		return true; // file vuoto: nessuna prova di essere binario

	int controlBytes = 0;
	for (ssize_t i = 0; i < read; i++)
	{
		unsigned char c = sample[i];
		if (c == 0)
			return false; // un NUL non compare mai in un file di testo genuino
		if (c < 0x20 && c != '\t' && c != '\n' && c != '\r')
			controlBytes++;
	}
	return controlBytes * 10 < read; // oltre il 10% di controllo -> binario
}

status_t CCsvTranslator::Identify(BPositionIO* source,
	const translation_format* format, BMessage* extension,
	translator_info* info, uint32 outType)
{
	off_t pos = source->Position();
	char header[4];
	ssize_t read = source->Read(header, 4);
	source->Seek(pos, SEEK_SET);

	bool isAscd = read == 4 && memcmp(header, kASCDMagic, 4) == 0;
	if (!isAscd && !LooksLikeText(source))
		return B_NO_TRANSLATOR;

	uint32 type = kAtomoCsvFormat;
	float quality = 0.6f, capability = 0.6f;
	const char* name = "Comma-Separated Values (CSV)";
	const char* mime = "text/csv";

	if (isAscd)
	{
		type = kAtomoNativeFormat;
		quality = capability = 1.0f;
		name = "Atomo Sheet Cell Data (ASCD)";
		mime = "application/x-vnd.atomo-sheet-data";
	}

	info->type = type;
	info->group = kAtomoSheetGroup;
	info->quality = quality;
	info->capability = capability;
	strlcpy(info->name, name, sizeof(info->name));
	strlcpy(info->MIME, mime, sizeof(info->MIME));

	return B_OK;
}

status_t CCsvTranslator::Translate(BPositionIO* source,
	const translator_info* info, BMessage* extension, uint32 outType,
	BPositionIO* destination)
{
	// info puo' essere NULL (documentato nel Translation Kit: significa
	// "identifica tu stesso il formato sorgente") -- un vero crash reale
	// di Tracker, non solo teorico: il thumbnail worker chiama
	// BTranslatorRoster::Translate() cosi' per ogni file mentre genera
	// le anteprime, senza mai passare da Identify() prima. Senza questo
	// controllo, "info->type" sotto leggeva un indirizzo qualunque.
	translator_info localInfo;
	if (!info)
	{
		if (Identify(source, NULL, extension, &localInfo, outType) != B_OK)
			return B_NO_TRANSLATOR;
		info = &localInfo;
	}

	if (outType == 0)
		outType = kAtomoNativeFormat;

	if (outType != kAtomoCsvFormat && outType != kAtomoNativeFormat)
		return B_NO_TRANSLATOR;

	CContainer* doc = new CContainer(NULL, NULL);
	status_t err = B_OK;

	try
	{
		if (info->type == kAtomoNativeFormat)
			err = ReadASCD(source, doc);
		else
		{
			// Stesso bug reale gia' corretto per XLSX/ODS (vedi il
			// commento su EnsureFunctionsInitialized in FunctionUtils.h/
			// .cpp): un campo CSV che comincia per "=" e usa una
			// funzione con nome (es. "=SUM(A1:A3)") passa da CParser
			// esattamente come una formula XLSX, con lo stesso bisogno
			// della tabella delle funzioni caricata.
			EnsureFunctionsInitialized();

			range r(1, 1, 1, 1);
			CTextConverter conv(*source, doc);
			conv.SetFieldSeparator(',');
			conv.SetQuoteTextFields(true);
			conv.ConvertFromText(r, NULL, true);
		}
	}
	catch (...)
	{
		err = B_ERROR;
	}

	if (err == B_OK)
	{
		try
		{
			if (outType == kAtomoNativeFormat)
				err = WriteASCD(doc, destination);
			else
			{
				CTextConverter conv(*destination, doc);
				conv.SetFieldSeparator(',');
				conv.SetQuoteTextFields(true);
				conv.ConvertToText(NULL, NULL);
			}
		}
		catch (...)
		{
			err = B_ERROR;
		}
	}

	doc->Release();
	return err;
}

extern "C" BTranslator* make_nth_translator(int32 n, image_id you, uint32 flags, ...)
{
	if (n == 0)
	{
		// Risolve il catalogo di QUESTO add-on tramite il proprio
		// image_id (vedi il commento su sCatalog sopra): "you" e'
		// l'immagine di questo stesso file .so gia' caricato dal
		// Translation Kit, da cui si ricava il percorso su disco per
		// costruire un entry_ref -- InitCheck() evita di rifarlo se
		// make_nth_translator viene chiamata piu' volte nello stesso
		// processo (ogni chiamata a Identify()/Translate() la richiama).
		if (sCatalog.InitCheck() != B_OK)
		{
			image_info info;
			if (get_image_info(you, &info) == B_OK)
			{
				BEntry entry(info.name);
				entry_ref ref;
				if (entry.GetRef(&ref) == B_OK)
					sCatalog.SetTo(ref);
			}
		}
		return new CCsvTranslator();
	}
	return NULL;
}
