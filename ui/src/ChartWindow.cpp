/*
	ChartWindow.cpp

	Vedi ChartWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "ChartWindow.h"
#include "ChartView.h"
#include "Chart.h"
#include "ColorWindow.h"
#include "IconCatalog.h"

#include <map>
#include <string>

#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <GroupLayout.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <View.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ChartWindow"

static const uint32 kMsgDrawLocal = 'drlc';
static const uint32 kMsgInsertLocal = 'inlc';
static const uint32 kMsgTypeChangedLocal = 'tpcl';
// Una checkbox per serie (Fase 19): tutte le checkbox della riga
// condividono lo stesso "what", l'indice della serie viaggia nel
// campo "index" del BMessage di ognuna (vedi RebuildSeriesCheckboxes).
static const uint32 kMsgSeriesToggleLocal = 'stvl';
// Un riquadretto ChartColorSwatch per serie (Fase colori), stesso
// principio esatto di kMsgSeriesToggleLocal sopra: "index" nel BMessage.
static const uint32 kMsgSeriesColorButtonLocal = 'scbl';
// Riquadretto singolo per un grafico a UNA sola serie (barre/linee/
// area/barre orizzontali/dispersione): nessun indice, sempre e solo
// "il colore del grafico".
static const uint32 kMsgChartColorButtonLocal = 'ccbl';
// Pulsante "..." (Fase selettore di intervallo): vedi il commento su
// fRangePickButton in ChartWindow.h.
static const uint32 kMsgRangePickButtonLocal = 'rpbl';

// Piccolo riquadro colorato cliccabile, un colore di serie o del
// grafico intero -- niente BButton (la sua chrome nativa renderebbe
// difficile vedere il colore vero e proprio su un riquadro cosi'
// piccolo): un semplice BView che si riempie del proprio colore e
// manda "what"/"index" al bersaglio dato quando viene cliccato.
class ChartColorSwatch : public BView {
public:
	ChartColorSwatch(int index, BMessenger target, uint32 what)
		:
		BView(BRect(0, 0, 15, 15), "colorSwatch", B_FOLLOW_NONE, B_WILL_DRAW),
		fIndex(index),
		fTarget(target),
		fWhat(what),
		fColor(ui_color(B_PANEL_BACKGROUND_COLOR))
	{
		SetExplicitMinSize(BSize(15, 15));
		SetExplicitMaxSize(BSize(15, 15));
	}

	void SetColor(rgb_color color)
	{
		fColor = color;
		Invalidate();
	}

	virtual void Draw(BRect updateRect)
	{
		SetHighColor(fColor);
		FillRect(Bounds());
		SetHighColor(120, 120, 120);
		StrokeRect(Bounds());
	}

	virtual void MouseDown(BPoint where)
	{
		BMessage msg(fWhat);
		msg.AddInt32("index", fIndex);
		fTarget.SendMessage(&msg);
	}

private:
	int fIndex;
	BMessenger fTarget;
	uint32 fWhat;
	rgb_color fColor;
};

// Pittogrammi 16x16 per le voci del menu Tipo (docs/ICONS.md li elenca
// come lacuna: nessuna icona del catalogo HVIF autorizzato e' un
// candidato adatto per un tipo di grafico specifico) -- disegnati a
// codice con la stessa tecnica di supersampling 4x gia' usata per i
// pittogrammi provvisori della toolbar, estratta in
// IconCatalog::RenderCustom apposta per essere riusabile qui. Corrispondenza
// posizionale con l'ordine dei BMenuItem costruiti sotto (0=Barre,
// 1=Linee, 2=Torta, 3=Area, 4=Dispersione, 5=Combinato, 6=Barre
// orizzontali), stessa convenzione di SelectedType()/LoadForEdit().
static void DrawBarChartTypeIcon(BView* view)
{
	view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	view->FillRect(BRect(2, 9, 4, 14));
	view->FillRect(BRect(6, 6, 8, 14));
	view->FillRect(BRect(10, 3, 12, 14));
}

static void DrawLineChartTypeIcon(BView* view)
{
	view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	view->SetPenSize(1.4f);
	BPoint p1(2, 11), p2(6, 5), p3(10, 9), p4(14, 3);
	view->StrokeLine(p1, p2);
	view->StrokeLine(p2, p3);
	view->StrokeLine(p3, p4);
	BPoint verts[4] = { p1, p2, p3, p4 };
	for (int i = 0; i < 4; i++)
		view->FillEllipse(verts[i], 1, 1);
}

static void DrawPieChartTypeIcon(BView* view)
{
	// Un disco pieno con due raggi disegnati nel colore di sfondo
	// (non un secondo colore in piu': i pittogrammi di questa toolbar
	// restano tutti monocromatici) suggerisce le divisioni fra le
	// fette senza bisogno di una vera tavolozza multicolore per
	// un'icona di 16px.
	view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	view->FillEllipse(BPoint(8, 8), 6, 6);
	view->SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	view->SetPenSize(1.2f);
	view->StrokeLine(BPoint(8, 8), BPoint(8, 2));
	view->StrokeLine(BPoint(8, 8), BPoint(13, 11));
}

static void DrawAreaChartTypeIcon(BView* view)
{
	view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	BPoint poly[6] = {
		BPoint(2, 11), BPoint(6, 5), BPoint(10, 9), BPoint(14, 3),
		BPoint(14, 14), BPoint(2, 14)
	};
	view->FillPolygon(poly, 6);
}

static void DrawScatterChartTypeIcon(BView* view)
{
	view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	BPoint dots[6] = {
		BPoint(3, 12), BPoint(6, 6), BPoint(9, 11),
		BPoint(12, 4), BPoint(13, 13), BPoint(5, 3)
	};
	for (int i = 0; i < 6; i++)
		view->FillEllipse(dots[i], 1.2f, 1.2f);
}

static void DrawComboChartTypeIcon(BView* view)
{
	view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	view->FillRect(BRect(3, 8, 6, 14));
	view->FillRect(BRect(9, 5, 12, 14));
	view->SetPenSize(1.4f);
	BPoint p1(2, 9), p2(8, 4), p3(14, 7);
	view->StrokeLine(p1, p2);
	view->StrokeLine(p2, p3);
	view->FillEllipse(p1, 1, 1);
	view->FillEllipse(p2, 1, 1);
	view->FillEllipse(p3, 1, 1);
}

static void DrawHBarChartTypeIcon(BView* view)
{
	view->SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
	view->FillRect(BRect(2, 3, 8, 5));
	view->FillRect(BRect(2, 7, 11, 9));
	view->FillRect(BRect(2, 11, 14, 13));
}

// BMenuItem in questa versione di Haiku non ha un equivalente di
// BButton::SetIcon (nessuna classe "BBitmapMenuItem" nei suoi header) --
// l'unico modo reale di aggiungere un'icona a una voce di menu e'
// sottoclassare BMenuItem e ridefinire GetContentSize/DrawContent,
// esattamente il punto di estensione per cui questi due metodi sono
// protected virtual. fIcon e' di proprieta' di questa classe (copiato
// nel costruttore, cosi' il chiamante puo' cancellare il proprio BBitmap
// subito dopo come fa ovunque altrove in questo file).
class ChartTypeMenuItem : public BMenuItem {
public:
	ChartTypeMenuItem(const char* label, BMessage* message, const BBitmap* icon)
		:
		BMenuItem(label, message),
		fIcon(icon ? new BBitmap(icon) : NULL),
		fContentHeight(0)
	{
	}

	virtual ~ChartTypeMenuItem()
	{
		delete fIcon;
	}

protected:
	virtual void GetContentSize(float* width, float* height)
	{
		BMenuItem::GetContentSize(width, height);
		if (fIcon)
		{
			float iconWidth = fIcon->Bounds().Width() + 1;
			float iconHeight = fIcon->Bounds().Height() + 1;
			if (width)
				*width += iconWidth + kIconTextGap;
			if (height && *height < iconHeight)
				*height = iconHeight;
		}
		fContentHeight = height ? *height : 0;
	}

	virtual void DrawContent()
	{
		BMenu* menu = Menu();
		if (!menu)
			return;

		BPoint loc = ContentLocation();
		if (fIcon)
		{
			// Centrato verticalmente sull'altezza di contenuto calcolata
			// da GetContentSize sopra (cache in fContentHeight), non
			// sulla base del testo -- l'icona e' quasi sempre piu' alta
			// di una riga di testo a questa dimensione di menu.
			BRect itemFrame = Frame();
			float iconHeight = fIcon->Bounds().Height() + 1;
			BPoint iconPos(loc.x, itemFrame.top + (itemFrame.Height() - iconHeight) / 2.0f);
			menu->SetDrawingMode(B_OP_ALPHA);
			menu->DrawBitmap(fIcon, iconPos);
			menu->SetDrawingMode(B_OP_COPY);
			loc.x += fIcon->Bounds().Width() + 1 + kIconTextGap;
		}
		menu->MovePenTo(loc);
		menu->DrawString(Label());
	}

private:
	static const float kIconTextGap;
	BBitmap* fIcon;
	float fContentHeight;
};
const float ChartTypeMenuItem::kIconTextGap = 6.0f;

ChartWindow::ChartWindow(BMessenger target)
	:
	BWindow(BRect(140, 120, 780, 680), B_TRANSLATE("Grafico"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_ASYNCHRONOUS_CONTROLS),
	fPickingRange(false),
	fTarget(target),
	fEditingChartIndex(-1),
	fChartColorSwatch(NULL),
	fChartColor(rgb_color{0, 0, 0, 0}),
	fSeriesColorWindow(NULL)
{
	// Stesso messaggio di fRangeField (kMsgDrawLocal): digitare un
	// titolo e premere Invio (o "Disegna") lo applica subito
	// all'anteprima, senza bisogno di un pulsante/campo separato.
	fTitleField = new BTextControl("title", B_TRANSLATE("Titolo (facoltativo):"),
		"", new BMessage(kMsgDrawLocal));
	fTitleField->SetTarget(this);

	fRangeField = new BTextControl("range", B_TRANSLATE("Intervallo (due colonne: etichette, valori):"),
		"A1:B5", new BMessage(kMsgDrawLocal));
	fRangeField->SetTarget(this);
	fRangeField->MakeFocus(true);

	// "..." (Fase selettore di intervallo): un solo pulsante per armare E
	// disarmare la selezione sul foglio, stesso principio di un
	// interruttore -- l'etichetta cambia (vedi kMsgRangePickButtonLocal
	// sotto) cosi' non serve un secondo pulsante "Annulla" a parte.
	BButton* rangePickButton = new BButton("rangePick", "...",
		new BMessage(kMsgRangePickButtonLocal));
	rangePickButton->SetTarget(this);
	fRangePickButton = rangePickButton;

	BButton* drawButton = new BButton("draw", B_TRANSLATE("Disegna"), new BMessage(kMsgDrawLocal));
	drawButton->SetTarget(this);

	// "Scambia righe/colonne" (vedi il commento su fRowOrientedCheckbox
	// in ChartWindow.h): spento di default, stesso comportamento di
	// sempre (rowOriented=false) finche' non lo si tocca. Stesso
	// messaggio kMsgDrawLocal di fRangeField/fTitleField: toccarlo
	// ridisegna subito l'anteprima con l'orientamento scelto.
	fRowOrientedCheckbox = new BCheckBox("rowOriented",
		B_TRANSLATE("Scambia righe/colonne"), new BMessage(kMsgDrawLocal));
	fRowOrientedCheckbox->SetTarget(this);

	// Barre come voce predefinita (indice 0), stesso ordine dei valori
	// dell'enum ChartType in Chart.h -- SelectedType() sotto si basa
	// su questa corrispondenza posizionale. ChartTypeMenuItem (sopra)
	// invece del semplice BMenuItem di prima: aggiunge un pittogramma
	// a ogni voce, piu' immediato da riconoscere al volo di un nome
	// testuale, come nella galleria grafici di Excel.
	BPopUpMenu* typeMenu = new BPopUpMenu("typeMenu");
	{
		const char* labels[7] = {
			B_TRANSLATE("Barre"), B_TRANSLATE("Linee"), B_TRANSLATE("Torta"),
			B_TRANSLATE("Area"), B_TRANSLATE("Dispersione (XY)"),
			B_TRANSLATE("Combinato (barre+linee)"), B_TRANSLATE("Barre orizzontali")
		};
		void (*drawIcon[7])(BView*) = {
			DrawBarChartTypeIcon, DrawLineChartTypeIcon, DrawPieChartTypeIcon,
			DrawAreaChartTypeIcon, DrawScatterChartTypeIcon, DrawComboChartTypeIcon,
			DrawHBarChartTypeIcon
		};
		for (int i = 0; i < 7; i++)
		{
			BBitmap* icon = IconCatalog::RenderCustom(drawIcon[i]);
			typeMenu->AddItem(new ChartTypeMenuItem(labels[i],
				new BMessage(kMsgTypeChangedLocal), icon));
			delete icon; // ChartTypeMenuItem ne fa una copia propria
		}
	}
	typeMenu->ItemAt(0)->SetMarked(true);
	fTypeField = new BMenuField("type", B_TRANSLATE("Tipo:"), typeMenu);
	fTypeField->Menu()->SetTargetForItems(this);

	// Colore del grafico (Fase colori, solo grafici a SINGOLA serie --
	// per serie multiple c'e' un riquadretto per serie nella riga sotto,
	// vedi RebuildSeriesCheckboxes): nascosto finche' non arriva un
	// grafico a singola serie (kMsgChartData/kMsgChartDataScatter sotto
	// lo mostrano, kMsgChartDataMulti lo nasconde), stesso principio di
	// fSeriesCheckboxBox per il caso opposto.
	fChartColorSwatch = new ChartColorSwatch(0, BMessenger(this), kMsgChartColorButtonLocal);
	fChartColorSwatch->SetColor(SeriesColor(std::vector<rgb_color>(), 0));
	fChartColorSwatch->Hide();

	fChartView = new ChartView();

	// Riquadro "Mostra valori per serie" (Fase 19): titolo del BBox
	// (stessa convenzione di raggruppamento di PreferencesWindow/
	// ConditionalFormatWindow) + un suggerimento in corpo minore che
	// spiega cosa fanno davvero le checkbox -- senza, la riga di
	// checkbox appariva senza contesto e si poteva credere che togliere
	// la spunta nascondesse l'intera serie invece della sola etichetta
	// numerica. Titolo e suggerimento sono creati una sola volta qui;
	// solo fSeriesCheckboxRow al suo interno viene svuotato/ripopolato
	// da RebuildSeriesCheckboxes/ClearSeriesCheckboxes a ogni richiesta.
	fSeriesCheckboxBox = new BBox("seriesCheckboxBox");
	fSeriesCheckboxBox->SetLabel(B_TRANSLATE("Mostra valori per serie"));

	BStringView* seriesHint = new BStringView("seriesHint",
		B_TRANSLATE("Deseleziona una serie per nascondere solo le sue etichette numeriche "
			"nel grafico; la serie resta comunque visibile."));
	BFont hintFont(be_plain_font);
	hintFont.SetSize(be_plain_font->Size() - 1);
	seriesHint->SetFont(&hintFont);
	seriesHint->SetHighColor(tint_color(ui_color(B_PANEL_TEXT_COLOR), 0.7));

	// BGroupLayout invece di BLayoutBuilder per la riga vera e propria
	// perche' le checkbox al suo interno vanno aggiunte/rimosse
	// dinamicamente dopo la costruzione, non solo una volta come il
	// resto della finestra.
	fSeriesCheckboxRow = new BView("seriesCheckboxes", 0);
	fSeriesCheckboxRow->SetLayout(new BGroupLayout(B_HORIZONTAL, 8));

	BLayoutBuilder::Group<>(fSeriesCheckboxBox, B_VERTICAL, 4)
		.SetInsets(8, fSeriesCheckboxBox->TopBorderOffset() + 6, 8, 6)
		.Add(seriesHint)
		.Add(fSeriesCheckboxRow);
	// Nascosto finche' non arriva un grafico a serie multiple (vedi
	// ClearSeriesCheckboxes/RebuildSeriesCheckboxes): un riquadro con
	// titolo ma senza nemmeno una checkbox sarebbe fuorviante quanto la
	// vecchia riga vuota.
	fSeriesCheckboxBox->Hide();

	fDestField = new BTextControl("dest", B_TRANSLATE("Cella di destinazione nel foglio:"),
		"D1", NULL);

	fInsertButton = new BButton("insert", B_TRANSLATE("Inserisci nel foglio"),
		new BMessage(kMsgInsertLocal));
	fInsertButton->SetTarget(this);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fTitleField)
		.AddGroup(B_HORIZONTAL)
			.Add(fRangeField)
			.Add(fRangePickButton)
			.Add(fTypeField)
			.Add(fChartColorSwatch)
			.Add(drawButton)
		.End()
		.Add(fRowOrientedCheckbox)
		.Add(fSeriesCheckboxBox)
		.Add(fChartView)
		.AddGroup(B_HORIZONTAL)
			.Add(fDestField)
			.AddGlue()
			.Add(fInsertButton)
		.End();
}

BMenu* ChartWindow::TypeMenu() const
{
	return fTypeField->Menu();
}

ChartType ChartWindow::SelectedType() const
{
	// Corrispondenza posizionale con l'ordine di inserimento in
	// BPopUpMenu sopra (0=Barre, 1=Linee, 2=Torta, 3=Area,
	// 4=Dispersione, 5=Combinato, 6=Barre orizzontali) e con i valori
	// dell'enum ChartType in Chart.h -- niente da cercare per nome.
	int32 index = fTypeField->Menu()->IndexOf(fTypeField->Menu()->FindMarked());
	switch (index)
	{
		case 1: return eLineChart;
		case 2: return ePieChart;
		case 3: return eAreaChart;
		case 4: return eScatterChart;
		case 5: return eComboChart;
		case 6: return eHBarChart;
		default: return eBarChart;
	}
}

void ChartWindow::CancelPickingIfArmed()
{
	if (!fPickingRange)
		return;
	fPickingRange = false;
	fRangePickButton->SetLabel("...");
	BMessage request(kMsgChartRangePickRequest);
	request.AddBool("start", false);
	request.AddMessenger("replyTo", BMessenger(this));
	fTarget.SendMessage(&request);
}

void ChartWindow::LoadRange(const char* rangeText)
{
	SetEditingChartIndex(-1);
	// Un selettore lasciato armato da una sessione precedente (finestra
	// mai chiusa, solo un nuovo "Inserisci Grafico" richiesto) non deve
	// restare attivo su un intervallo/campo che non e' piu' quello a cui
	// si riferiva -- stesso disarmo esplicito di QuitRequested.
	CancelPickingIfArmed();
	fRangeField->SetText(rangeText);
	fRowOrientedCheckbox->SetValue(B_CONTROL_OFF);
	// Nessun colore salvato per un grafico nuovo -- azzera qualunque
	// scelta fatta durante una precedente sessione di modifica.
	fSeriesColorOverrides.clear();
	fChartColor = rgb_color{0, 0, 0, 0};
	RequestDraw();
}

void ChartWindow::LoadForEdit(int chartIndex, const char* rangeText, const char* title,
	ChartType type, bool rowOriented, const std::vector<rgb_color>& seriesColors)
{
	SetEditingChartIndex(chartIndex);
	CancelPickingIfArmed();
	fRangeField->SetText(rangeText);
	fTitleField->SetText(title);
	fRowOrientedCheckbox->SetValue(rowOriented ? B_CONTROL_ON : B_CONTROL_OFF);
	// Precompila entrambi i possibili usi dello stesso array salvato
	// (ChartObject::seriesColors e' un unico campo per singola/serie
	// multiple, vedi Chart.h): quale dei due conta davvero dipende dal
	// tipo del grafico, non ancora noto qui -- lo decide il gestore di
	// kMsgChartData/kMsgChartDataMulti quando arriva la risposta di
	// RequestDraw piu' sotto.
	fSeriesColorOverrides = seriesColors;
	fChartColor = !seriesColors.empty() ? seriesColors[0] : rgb_color{0, 0, 0, 0};

	// Corrispondenza inversa di SelectedType() sopra: stessa
	// corrispondenza posizionale (0=Barre, 1=Linee, 2=Torta, 3=Area,
	// 4=Dispersione, 5=Combinato, 6=Barre orizzontali).
	int32 index = 0;
	switch (type)
	{
		case eLineChart: index = 1; break;
		case ePieChart: index = 2; break;
		case eAreaChart: index = 3; break;
		case eScatterChart: index = 4; break;
		case eComboChart: index = 5; break;
		case eHBarChart: index = 6; break;
		default: index = 0; break;
	}
	BMenuItem* item = fTypeField->Menu()->ItemAt(index);
	if (item)
		item->SetMarked(true);
	fChartView->SetChartType(type);

	RequestDraw();
}

void ChartWindow::SetEditingChartIndex(int chartIndex)
{
	fEditingChartIndex = chartIndex;
	if (chartIndex >= 0)
	{
		// La posizione di un grafico gia' incorporato non cambia
		// editandolo (vedi HandleChartUpdate): il campo destinazione non
		// avrebbe alcun effetto, nasconderlo evita di far credere
		// all'utente che spostera' il grafico.
		fDestField->Hide();
		fInsertButton->SetLabel(B_TRANSLATE("Aggiorna"));
	}
	else
	{
		fDestField->Show();
		fInsertButton->SetLabel(B_TRANSLATE("Inserisci nel foglio"));
	}
}

void ChartWindow::RequestDraw()
{
	fChartView->SetTitle(fTitleField->Text());
	BMessage request(kMsgChartRequest);
	request.AddString("range", fRangeField->Text());
	// Il tipo scelto viaggia con la richiesta (Fase 35): un grafico a
	// dispersione (Chart.h, ScatterPoint) legge l'intervallo in un modo
	// completamente diverso (entrambe le colonne numeriche, niente
	// etichetta) da MainWindow::HandleChartRequest, che altrimenti non
	// avrebbe modo di saperlo dalla sola forma dell'intervallo (due
	// colonne e' anche la forma normale di un grafico a barre/linee).
	request.AddInt32("type", (int32)SelectedType());
	// Scambia righe/colonne (semplice, contigue -- vedi il commento su
	// fRowOrientedCheckbox in ChartWindow.h): MainWindow::HandleChartRequest
	// deve saperlo PRIMA di leggere l'intervallo, stesso motivo del tipo
	// sopra.
	request.AddBool("rowOriented", fRowOrientedCheckbox->Value() == B_CONTROL_ON);
	fTarget.SendMessage(&request);
}

void ChartWindow::ShowSeriesColorPicker(int index)
{
	if (!fSeriesColorWindow)
		fSeriesColorWindow = new ColorWindow(BMessenger(this));

	rgb_color initial = (index < 0)
		? (fChartColor.alpha > 0 ? fChartColor : SeriesColor(std::vector<rgb_color>(), 0))
		: SeriesColor(fSeriesColorOverrides, (size_t)index);

	if (fSeriesColorWindow->Lock())
	{
		fSeriesColorWindow->SetTarget(BMessenger(this));
		fSeriesColorWindow->SetSeriesIndex(index);
		fSeriesColorWindow->SetMode(eSeriesColor, initial);
		fSeriesColorWindow->Unlock();
	}
	if (fSeriesColorWindow->IsHidden())
		fSeriesColorWindow->Show();
	fSeriesColorWindow->Activate();
}

void ChartWindow::ClearSeriesCheckboxes()
{
	while (fSeriesCheckboxRow->CountChildren() > 0)
	{
		BView* child = fSeriesCheckboxRow->ChildAt(0);
		fSeriesCheckboxRow->RemoveChild(child);
		delete child;
	}
	fSeriesCheckboxes.clear();
	// I riquadretti colore sono GIA' figli di fSeriesCheckboxRow
	// (aggiunti subito dopo ogni checkbox in RebuildSeriesCheckboxes),
	// quindi il ciclo sopra li cancella gia' -- questo svuota solo il
	// vettore di puntatori ormai invalidi.
	fSeriesColorSwatches.clear();
	// Nessuna serie da elencare: il riquadro intero sparisce invece di
	// restare visibile ma vuoto (vedi il commento nel costruttore).
	if (!fSeriesCheckboxBox->IsHidden())
		fSeriesCheckboxBox->Hide();
}

void ChartWindow::RebuildSeriesCheckboxes(MultiChartData* data)
{
	// Preserva lo stato "spuntata/non spuntata" E il colore scelto per
	// nome di serie: senza questo, ridisegnare lo stesso intervallo (es.
	// premendo di nuovo Invio nel campo Intervallo) resetterebbe ogni
	// volta le checkbox a "tutte spuntate" e i colori alla tavolozza,
	// cancellando una scelta gia' fatta dall'utente.
	std::map<std::string, bool> previousState;
	std::map<std::string, rgb_color> previousColors;
	for (size_t i = 0; i < fSeriesCheckboxes.size(); i++)
	{
		previousState[fSeriesCheckboxes[i]->Label()] = fSeriesCheckboxes[i]->Value() != 0;
		if (i < fSeriesColorOverrides.size())
			previousColors[fSeriesCheckboxes[i]->Label()] = fSeriesColorOverrides[i];
	}

	ClearSeriesCheckboxes();

	data->showValues.resize(data->seriesNames.size());
	fSeriesColorOverrides.assign(data->seriesNames.size(), rgb_color{0, 0, 0, 0});
	for (size_t s = 0; s < data->seriesNames.size(); s++)
	{
		bool checked = true;
		std::map<std::string, bool>::iterator it =
			previousState.find(data->seriesNames[s].String());
		if (it != previousState.end())
			checked = it->second;
		data->showValues[s] = checked;

		std::map<std::string, rgb_color>::iterator colorIt =
			previousColors.find(data->seriesNames[s].String());
		if (colorIt != previousColors.end())
			fSeriesColorOverrides[s] = colorIt->second;

		BMessage* msg = new BMessage(kMsgSeriesToggleLocal);
		msg->AddInt32("index", (int32)s);
		BCheckBox* cb = new BCheckBox(data->seriesNames[s].String(),
			data->seriesNames[s].String(), msg);
		cb->SetTarget(this);
		cb->SetValue(checked ? B_CONTROL_ON : B_CONTROL_OFF);
		fSeriesCheckboxRow->AddChild(cb);
		fSeriesCheckboxes.push_back(cb);

		ChartColorSwatch* swatch = new ChartColorSwatch((int)s, BMessenger(this),
			kMsgSeriesColorButtonLocal);
		swatch->SetColor(SeriesColor(fSeriesColorOverrides, s));
		fSeriesCheckboxRow->AddChild(swatch);
		fSeriesColorSwatches.push_back(swatch);
	}
	// Popolato PRIMA di ChartView::SetMultiData (chiamato subito dopo da
	// chi ci ha invocato), cosi' l'anteprima riflette gia' i colori
	// scelti in precedenza al primo giro, non solo dopo un nuovo clic.
	data->seriesColors = fSeriesColorOverrides;

	// Almeno una serie da elencare: mostra (o tieni visibile) il
	// riquadro col titolo/suggerimento -- vedi ClearSeriesCheckboxes
	// per il caso opposto.
	if (!data->seriesNames.empty() && fSeriesCheckboxBox->IsHidden())
		fSeriesCheckboxBox->Show();
}

void ChartWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgDrawLocal:
			RequestDraw();
			return;

		case kMsgTypeChangedLocal:
			fChartView->SetChartType(SelectedType());
			return;

		case kMsgInsertLocal:
		{
			bool rowOriented = fRowOrientedCheckbox->Value() == B_CONTROL_ON;
			// Colori da inviare (Fase colori): per un grafico a singola
			// serie, fChartColor (se mai scelto, alpha > 0) e' l'unico
			// colore che conta, indice 0; per serie multiple,
			// fSeriesColorOverrides gia' nello stesso ordine di
			// seriesNames. I due casi non capitano mai insieme (un
			// grafico e' o l'uno o l'altro), quindi va bene mandare
			// sempre e solo uno dei due vettori, mai entrambi mescolati.
			std::vector<rgb_color> colorsToSend = !fSeriesColorOverrides.empty()
				? fSeriesColorOverrides
				: (fChartColor.alpha > 0 ? std::vector<rgb_color>(1, fChartColor)
					: std::vector<rgb_color>());
			if (fEditingChartIndex >= 0)
			{
				BMessage request(kMsgChartUpdate);
				request.AddInt32("index", fEditingChartIndex);
				request.AddString("range", fRangeField->Text());
				request.AddInt32("type", (int32)SelectedType());
				request.AddString("title", fTitleField->Text());
				request.AddBool("rowOriented", rowOriented);
				for (size_t i = 0; i < colorsToSend.size(); i++)
					request.AddData("seriesColor", B_RGB_COLOR_TYPE, &colorsToSend[i], sizeof(rgb_color));
				fTarget.SendMessage(&request);
				return;
			}
			BMessage request(kMsgChartInsert);
			request.AddString("range", fRangeField->Text());
			request.AddString("dest", fDestField->Text());
			request.AddInt32("type", (int32)SelectedType());
			request.AddString("title", fTitleField->Text());
			request.AddBool("rowOriented", rowOriented);
			for (size_t i = 0; i < colorsToSend.size(); i++)
				request.AddData("seriesColor", B_RGB_COLOR_TYPE, &colorsToSend[i], sizeof(rgb_color));
			fTarget.SendMessage(&request);
			return;
		}

		case kMsgChartData:
		{
			// Nessuna checkbox per un grafico a singola serie: niente
			// da scegliere, il valore e' sempre mostrato (come da
			// sempre).
			ClearSeriesCheckboxes();

			std::vector<ChartSeries> data;
			BString label;
			double value;
			for (int32 i = 0; message->FindString("label", i, &label) == B_OK
					&& message->FindDouble("value", i, &value) == B_OK; i++)
			{
				ChartSeries s;
				s.label = label;
				s.value = value;
				data.push_back(s);
			}
			fChartView->SetData(data);
			// Riquadretto colore (Fase colori): mai per la torta, che
			// mantiene sempre la sua tavolozza per fetta -- vedi il
			// commento su ChartObject::seriesColors in Chart.h.
			if (SelectedType() == ePieChart)
			{
				if (!fChartColorSwatch->IsHidden())
					fChartColorSwatch->Hide();
			}
			else
			{
				fChartColorSwatch->SetColor(fChartColor.alpha > 0
					? fChartColor : SeriesColor(std::vector<rgb_color>(), 0));
				if (fChartColorSwatch->IsHidden())
					fChartColorSwatch->Show();
			}
			return;
		}

		case kMsgChartDataScatter:
		{
			// Nessuna checkbox qui neanche: un grafico a dispersione non
			// ha serie multiple, stesso principio di kMsgChartData sopra.
			ClearSeriesCheckboxes();

			std::vector<ScatterPoint> data;
			double x, y;
			for (int32 i = 0; message->FindDouble("x", i, &x) == B_OK
					&& message->FindDouble("y", i, &y) == B_OK; i++)
			{
				ScatterPoint p;
				p.x = x;
				p.y = y;
				data.push_back(p);
			}
			fChartView->SetScatterData(data);
			// Un grafico a dispersione e' sempre a singola serie, mai
			// torta -- il riquadretto colore e' sempre pertinente qui.
			fChartColorSwatch->SetColor(fChartColor.alpha > 0
				? fChartColor : SeriesColor(std::vector<rgb_color>(), 0));
			if (fChartColorSwatch->IsHidden())
				fChartColorSwatch->Show();
			return;
		}

		case kMsgChartDataMulti:
		{
			// Codifica piatta dello stesso MultiChartData di Chart.h:
			// tutte le categorie, poi tutti i nomi di serie, poi tutti
			// i valori in ordine "serie-maggiore" (prima tutte le
			// categorie della serie 0, poi tutte quelle della serie 1,
			// ...) -- ricostruito qui nello stesso ordine, vedi
			// MainWindow::HandleChartRequest dove viene scritto.
			MultiChartData data;
			BString category;
			for (int32 i = 0; message->FindString("category", i, &category) == B_OK; i++)
				data.categories.push_back(category);

			BString seriesName;
			for (int32 s = 0; message->FindString("seriesName", s, &seriesName) == B_OK; s++)
				data.seriesNames.push_back(seriesName);

			data.values.resize(data.seriesNames.size());
			int32 k = 0;
			for (size_t s = 0; s < data.seriesNames.size(); s++)
			{
				data.values[s].resize(data.categories.size());
				for (size_t c = 0; c < data.categories.size(); c++)
				{
					double v = 0;
					message->FindDouble("value", k++, &v);
					data.values[s][c] = v;
				}
			}
			// Ricostruisce le checkbox PRIMA di passare i dati a
			// ChartView: popola anche data.showValues (preservando lo
			// stato di prima per nome di serie), cosi' il grafico si
			// disegna gia' con le visibilita' corrette al primo giro.
			RebuildSeriesCheckboxes(&data);
			fChartView->SetMultiData(data);
			// Il riquadretto singolo non vale per serie multiple (c'e'
			// un riquadretto per serie nella riga appena ricostruita).
			if (!fChartColorSwatch->IsHidden())
				fChartColorSwatch->Hide();
			return;
		}

		case kMsgSeriesToggleLocal:
		{
			int32 index;
			int32 value = 0;
			if (message->FindInt32("index", &index) == B_OK)
			{
				// "be:value" e' aggiunto automaticamente da
				// BControl::Invoke() col nuovo stato della checkbox
				// che ha generato il messaggio.
				message->FindInt32("be:value", &value);
				fChartView->SetSeriesShowValues(index, value != 0);
			}
			return;
		}

		case kMsgSeriesColorButtonLocal:
		{
			int32 index;
			if (message->FindInt32("index", &index) == B_OK)
				ShowSeriesColorPicker(index);
			return;
		}

		case kMsgChartColorButtonLocal:
			// -1 = "il colore dell'intero grafico", non un vero indice
			// di serie -- vedi il commento su fChartColor in ChartWindow.h.
			ShowSeriesColorPicker(-1);
			return;

		case kMsgRangePickButtonLocal:
		{
			// Un solo pulsante arma/disarma (vedi il commento su
			// fRangePickButton in ChartWindow.h): il secondo clic mentre
			// e' gia' armato disarma senza scegliere nulla, invece di
			// affidarsi a Escape o alla chiusura della finestra.
			if (fPickingRange)
			{
				CancelPickingIfArmed();
				return;
			}
			fPickingRange = true;
			fRangePickButton->SetLabel(B_TRANSLATE("Seleziona..."));
			BMessage request(kMsgChartRangePickRequest);
			request.AddBool("start", true);
			request.AddMessenger("replyTo", BMessenger(this));
			fTarget.SendMessage(&request);
			return;
		}

		case kMsgRangePicked:
		{
			// Arrivata la risposta del foglio (vedi SheetView::MouseUp):
			// il pulsante torna al suo stato di riposo, il testo scelto
			// sostituisce quello del campo Intervallo e l'anteprima si
			// ridisegna subito, come se l'utente lo avesse digitato a
			// mano e premuto Invio.
			fPickingRange = false;
			fRangePickButton->SetLabel("...");
			BString rangeText;
			if (message->FindString("range", &rangeText) == B_OK)
			{
				fRangeField->SetText(rangeText.String());
				RequestDraw();
			}
			return;
		}

		case kMsgColorRequest:
		{
			int32 target = -1, index = -1;
			message->FindInt32("target", &target);
			if ((ColorTarget)target != eSeriesColor)
				return;
			message->FindInt32("seriesIndex", &index);

			const void* colorData;
			ssize_t size;
			if (message->FindData("color", B_RGB_COLOR_TYPE, &colorData, &size) != B_OK
				|| size != (ssize_t)sizeof(rgb_color))
				return;
			rgb_color color = *(const rgb_color*)colorData;

			if (index < 0)
			{
				fChartColor = color;
				fChartColorSwatch->SetColor(color);
				fChartView->SetChartColor(color);
			}
			else
			{
				if (index >= (int)fSeriesColorOverrides.size())
					fSeriesColorOverrides.resize(index + 1, rgb_color{0, 0, 0, 0});
				fSeriesColorOverrides[index] = color;
				if (index < (int)fSeriesColorSwatches.size())
					fSeriesColorSwatches[index]->SetColor(color);
				fChartView->SetSeriesColor(index, color);
			}
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool ChartWindow::QuitRequested()
{
	// Se il selettore di intervallo era armato, disarmalo: altrimenti
	// SheetView resterebbe in attesa di un clic che non arrivera' mai
	// piu' per questa finestra (nascosta, non chiusa davvero -- vedi
	// sotto), e il pulsante mostrerebbe ancora "Seleziona..." alla
	// prossima riapertura anche se l'utente non ha mai cliccato di
	// nuovo su di esso.
	CancelPickingIfArmed();

	// Stessa regola di FindWindow: resta nascosta e riusabile.
	Hide();
	return false;
}
