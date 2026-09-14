/*
	PageSetupWindow.cpp

	Vedi PageSetupWindow.h.

	Copyright (c) 2026 Andrea Bernardi. Licenza MIT (vedi LICENSE alla
	radice del repository).
*/

#include "PageSetupWindow.h"

#include <cstdlib>

#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <RadioButton.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>

#include "PrintLayout.h"

#include "Constants.h"
#include "PrintPreviewView.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "PageSetupWindow"

static const uint32 kMsgApplyLocal = 'apps';
static const uint32 kMsgPrintLocal = 'pspr';
// I quattro BRadioButton di scala si "accorgono" a vicenda solo perche'
// condividono lo stesso genitore (comportamento standard di
// BRadioButton) -- questo messaggio abilita/disabilita il campo
// percentuale in base a quale e' selezionato E rigenera l'anteprima
// (kMsgFieldChanged sotto fa lo stesso per i campi di testo).
static const uint32 kMsgScaleModeChanged = 'scmd';
static const uint32 kMsgFieldChanged = 'fldc';
static const uint32 kMsgPrevPage = 'prvp';
static const uint32 kMsgNextPage = 'nxtp';

PageSetupWindow::PageSetupWindow(BMessenger target)
	:
	// Ridimensionabile (era B_NOT_RESIZABLE, dimensione fissa 630x600):
	// su un display basso quella finestra fissa non ci stava proprio,
	// e non ridimensionabile significava nessun modo per l'utente di
	// rimediare -- bug reale segnalato dall'utente. La colonna opzioni
	// era gia' pensata per scorrere (vedi optionsScroll sotto) proprio
	// perche' le opzioni crescono a ogni fase; il pezzo mancante era
	// lasciare che l'INTERA finestra si restringesse quando anche
	// l'anteprima (l'altro elemento che occupava spazio fisso) puo'
	// farlo. B_AUTO_UPDATE_SIZE_LIMITS ricalcola da solo il minimo dal
	// layout -- ma solo dopo aver dato un minimo esplicito piccolo sia
	// all'anteprima sia alla colonna opzioni scorrevole (vedi
	// optionsScroll sotto): senza, il minimo calcolato era il contenuto
	// INTERO non scorso, riaprendo la finestra grande uguale (verificato
	// dal vivo: si apriva a 880px di altezza). Dimensione di apertura
	// 750x420 (era 580x420 al primo passaggio di questo fix, poi
	// allargata su richiesta esplicita -- l'altezza ridotta e la
	// ridimensionabilita' restano il punto per gli schermi bassi, la
	// larghezza qui e' solo comodita' su schermi normali): mai piu'
	// piccola del minimo vero (vedi sopra), sempre restringibile a mano.
	BWindow(BRect(150, 150, 900, 570), B_TRANSLATE("Imposta pagina"),
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target)
{
	fMarginTopField = new BTextControl("marginTop", B_TRANSLATE("Superiore:"), "2.0",
		new BMessage(kMsgFieldChanged));
	fMarginBottomField = new BTextControl("marginBottom", B_TRANSLATE("Inferiore:"), "2.0",
		new BMessage(kMsgFieldChanged));
	fMarginLeftField = new BTextControl("marginLeft", B_TRANSLATE("Sinistro:"), "2.0",
		new BMessage(kMsgFieldChanged));
	fMarginRightField = new BTextControl("marginRight", B_TRANSLATE("Destro:"), "2.0",
		new BMessage(kMsgFieldChanged));

	BStringView* marginUnitHint = new BStringView("marginUnitHint",
		B_TRANSLATE("Valori in centimetri"));
	marginUnitHint->SetFont(be_plain_font);

	// Due righe da due campi (Superiore/Inferiore, poi Sinistro/Destro)
	// invece di quattro righe da uno: stesso spazio informativo in meta'
	// altezza, contributo diretto allo spazio verticale risparmiato per
	// stare su schermi bassi (vedi il commento sul costruttore sopra).
	BBox* marginsBox = new BBox("marginsBox");
	marginsBox->SetLabel(B_TRANSLATE("Margini"));
	BLayoutBuilder::Group<>(marginsBox, B_VERTICAL, 6)
		.SetInsets(8, marginsBox->TopBorderOffset() + 8, 8, 8)
		.AddGroup(B_HORIZONTAL, 8)
			.Add(fMarginTopField)
			.Add(fMarginBottomField)
		.End()
		.AddGroup(B_HORIZONTAL, 8)
			.Add(fMarginLeftField)
			.Add(fMarginRightField)
		.End()
		.Add(marginUnitHint);

	fScalePercentField = new BTextControl("scalePercent", B_TRANSLATE("Percentuale:"), "100",
		new BMessage(kMsgFieldChanged));

	fScalePercentRadio = new BRadioButton("scalePercentRadio",
		B_TRANSLATE("Adatta al:"), new BMessage(kMsgScaleModeChanged));
	fScaleFitWidthRadio = new BRadioButton("scaleFitWidthRadio",
		B_TRANSLATE("Larghezza di una pagina"), new BMessage(kMsgScaleModeChanged));
	fScaleFitHeightRadio = new BRadioButton("scaleFitHeightRadio",
		B_TRANSLATE("Altezza di una pagina"), new BMessage(kMsgScaleModeChanged));
	fScaleFitBothRadio = new BRadioButton("scaleFitBothRadio",
		B_TRANSLATE("Una sola pagina"), new BMessage(kMsgScaleModeChanged));
	fScalePercentRadio->SetValue(B_CONTROL_ON);

	// Adatta a N x M pagine (come Excel "Fit to N page(s) wide by M
	// tall"): i due campi hanno senso solo con il loro radio, come il
	// campo percentuale col suo -- vedi kMsgScaleModeChanged sotto.
	fScaleFitPagesRadio = new BRadioButton("scaleFitPagesRadio",
		B_TRANSLATE("Pagine:"), new BMessage(kMsgScaleModeChanged));
	fScaleFitWideField = new BTextControl("fitWide", B_TRANSLATE("Larghe:"),
		"1", new BMessage(kMsgFieldChanged));
	fScaleFitTallField = new BTextControl("fitTall", B_TRANSLATE("Alte:"),
		"1", new BMessage(kMsgFieldChanged));

	// Intestazioni di riga/colonna e griglia nella stampa: come in Excel
	// ("Stampa titoli"/griglia), di default attive per conservare
	// l'aspetto di sempre -- ogni cambio rigenera solo l'anteprima
	// (kMsgFieldChanged), come i campi di testo.
	fPrintHeadersBox = new BCheckBox("printHeaders",
		B_TRANSLATE("Stampa intestazioni righe/colonne"),
		new BMessage(kMsgFieldChanged));
	fPrintHeadersBox->SetValue(B_CONTROL_ON);
	fPrintGridBox = new BCheckBox("printGrid",
		B_TRANSLATE("Stampa griglia"),
		new BMessage(kMsgFieldChanged));
	fPrintGridBox->SetValue(B_CONTROL_ON);

	// Centratura del contenuto nella pagina (come Excel "Center on page"):
	// spente di default -- ogni cambio rigenera solo l'anteprima, come
	// le altre checkbox di stampa.
	fCenterHBox = new BCheckBox("centerH",
		B_TRANSLATE("Orizzontale"),
		new BMessage(kMsgFieldChanged));
	fCenterHBox->SetValue(B_CONTROL_OFF);
	fCenterVBox = new BCheckBox("centerV",
		B_TRANSLATE("Verticale"),
		new BMessage(kMsgFieldChanged));
	fCenterVBox->SetValue(B_CONTROL_OFF);

	// Testi di intestazione e pie' di pagina (una riga ciascuno, su OGNI
	// pagina): vuoti di default (= bande assenti). I codici &P/&N/&D si
	// espandono in stampa/anteprima (vedi ExpandPrintHeaderCodes) -- il
	// promemoria sotto elenca i codici senza rubare altro spazio.
	fHeaderTextField = new BTextControl("headerText", B_TRANSLATE("Intestazione:"),
		"", new BMessage(kMsgFieldChanged));
	fFooterTextField = new BTextControl("footerText", B_TRANSLATE("Piè di pagina:"),
		"", new BMessage(kMsgFieldChanged));
	BStringView* codesHint = new BStringView("codesHint",
		B_TRANSLATE("&P pagina, &N totale pagine, &D data"));
	codesHint->SetFont(be_plain_font);

	// Righe/colonne da ripetere su OGNI pagina (titoli di stampa, come
	// Excel "Print titles"): "1:3" o "2" per le righe, "A:C" o "B" per le
	// colonne, vuoto = nessun titolo. Il formato si convalida qui con gli
	// stessi parser puri dell'impaginazione (vedi ParsePrintTitleRows/
	// Cols in PrintLayout.h): testo non valido = nessun titolo, mai valori
	// a meta'. Ogni cambio rigenera solo l'anteprima, come gli altri campi.
	fTitleRowsField = new BTextControl("titleRows", B_TRANSLATE("Righe:"),
		"", new BMessage(kMsgFieldChanged));
	fTitleColsField = new BTextControl("titleCols", B_TRANSLATE("Colonne:"),
		"", new BMessage(kMsgFieldChanged));
	BStringView* titlesHint = new BStringView("titlesHint",
		B_TRANSLATE("Righe come 1:3, colonne come A:C (vuoto = nessuno)"));
	titlesHint->SetFont(be_plain_font);

	// Ordine delle pagine (come Excel "Page order"): prima giu' poi a
	// destra (default, come Excel) oppure prima a destra poi giu'. Due
	// radio fratelli (stesso genitore) per la mutua esclusione standard
	// di BRadioButton -- ogni cambio rigenera solo l'anteprima.
	fOrderDownRadio = new BRadioButton("orderDownRadio",
		B_TRANSLATE("Prima giù, poi a destra"), new BMessage(kMsgScaleModeChanged));
	fOrderAcrossRadio = new BRadioButton("orderAcrossRadio",
		B_TRANSLATE("Prima a destra, poi giù"), new BMessage(kMsgScaleModeChanged));
	fOrderDownRadio->SetValue(B_CONTROL_ON);

	BBox* scaleBox = new BBox("scaleBox");
	scaleBox->SetLabel(B_TRANSLATE("Scala"));
	BLayoutBuilder::Group<>(scaleBox, B_VERTICAL, 6)
		.SetInsets(8, scaleBox->TopBorderOffset() + 8, 8, 8)
		.AddGroup(B_HORIZONTAL)
			.Add(fScalePercentRadio)
			.Add(fScalePercentField)
			.AddGlue()
		.End()
		.Add(fScaleFitWidthRadio)
		.Add(fScaleFitHeightRadio)
		.Add(fScaleFitBothRadio)
		.AddGroup(B_HORIZONTAL)
			.Add(fScaleFitPagesRadio)
			.Add(fScaleFitWideField)
			.Add(fScaleFitTallField)
			.AddGlue()
		.End();

	BBox* printBox = new BBox("printBox");
	printBox->SetLabel(B_TRANSLATE("Stampa"));
	BLayoutBuilder::Group<>(printBox, B_VERTICAL, 6)
		.SetInsets(8, printBox->TopBorderOffset() + 8, 8, 8)
		.Add(fPrintHeadersBox)
		.Add(fPrintGridBox);

	BBox* centerBox = new BBox("centerBox");
	centerBox->SetLabel(B_TRANSLATE("Centratura"));
	BLayoutBuilder::Group<>(centerBox, B_VERTICAL, 6)
		.SetInsets(8, centerBox->TopBorderOffset() + 8, 8, 8)
		.Add(fCenterHBox)
		.Add(fCenterVBox);

	BBox* headerFooterBox = new BBox("headerFooterBox");
	headerFooterBox->SetLabel(B_TRANSLATE("Intestazione/piè di pagina"));
	BLayoutBuilder::Group<>(headerFooterBox, B_VERTICAL, 6)
		.SetInsets(8, headerFooterBox->TopBorderOffset() + 8, 8, 8)
		.Add(fHeaderTextField)
		.Add(fFooterTextField)
		.Add(codesHint);

	BBox* titlesBox = new BBox("titlesBox");
	titlesBox->SetLabel(B_TRANSLATE("Titoli da ripetere"));
	BLayoutBuilder::Group<>(titlesBox, B_VERTICAL, 6)
		.SetInsets(8, titlesBox->TopBorderOffset() + 8, 8, 8)
		.Add(fTitleRowsField)
		.Add(fTitleColsField)
		.Add(titlesHint);

	BBox* orderBox = new BBox("orderBox");
	orderBox->SetLabel(B_TRANSLATE("Ordine pagine"));
	BLayoutBuilder::Group<>(orderBox, B_VERTICAL, 6)
		.SetInsets(8, orderBox->TopBorderOffset() + 8, 8, 8)
		.Add(fOrderDownRadio)
		.Add(fOrderAcrossRadio);

	// Colonna destra scrollabile: le opzioni crescono a ogni fase e la
	// finestra resta a dimensione fissa (non ridimensionabile) -- senza
	// scroll, i box in coda finirebbero fuori schermo su display bassi.
	BGroupView* optionsCol = new BGroupView();
	BLayoutBuilder::Group<>(optionsCol, B_VERTICAL, 8)
		.Add(marginsBox)
		.Add(scaleBox)
		.Add(printBox)
		.Add(centerBox)
		.Add(headerFooterBox)
		.Add(titlesBox)
		.Add(orderBox)
		.AddGlue()
		.End();
	BScrollView* optionsScroll = new BScrollView("optionsScroll", optionsCol,
		0, false, true, B_NO_BORDER);
	// Senza un minimo esplicito qui, B_AUTO_UPDATE_SIZE_LIMITS calcola il
	// minimo della finestra dal contenuto NON scorso di optionsCol (tutti
	// i box distesi), vanificando lo scroll: verificato dal vivo, la
	// finestra si apriva a ~880px di altezza invece dei ~570 richiesti.
	// Un minimo piccolo qui dice al layout "puoi disegnarla anche cosi'
	// stretta, il resto scorre" -- stesso principio di
	// PrintPreviewView::SetExplicitMinSize, stesso bug gia' visto li'.
	optionsScroll->SetExplicitMinSize(BSize(260, 160));

	fPreviewView = new PrintPreviewView();

	fPageLabel = new BStringView("pageLabel", B_TRANSLATE("Nessuna anteprima"));
	fPrevPageButton = new BButton("prevPage", B_TRANSLATE("‹"), new BMessage(kMsgPrevPage));
	fNextPageButton = new BButton("nextPage", B_TRANSLATE("›"), new BMessage(kMsgNextPage));
	fPrevPageButton->SetEnabled(false);
	fNextPageButton->SetEnabled(false);

	BButton* printButton = new BButton("print", B_TRANSLATE("Stampa" B_UTF8_ELLIPSIS),
		new BMessage(kMsgPrintLocal));
	BButton* applyButton = new BButton("apply", B_TRANSLATE("Applica"), new BMessage(kMsgApplyLocal));
	applyButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.AddGroup(B_HORIZONTAL, 8)
			.AddGroup(B_VERTICAL, 4)
				.Add(fPreviewView)
				.AddGroup(B_HORIZONTAL)
					.AddGlue()
					.Add(fPrevPageButton)
					.Add(fPageLabel)
					.Add(fNextPageButton)
					.AddGlue()
				.End()
			.End()
			.Add(optionsScroll)
		.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(printButton)
			.Add(applyButton)
		.End();

	fMarginTopField->SetTarget(this);
	fMarginBottomField->SetTarget(this);
	fMarginLeftField->SetTarget(this);
	fMarginRightField->SetTarget(this);
	fScalePercentField->SetTarget(this);
	fScalePercentRadio->SetTarget(this);
	fScaleFitWidthRadio->SetTarget(this);
	fScaleFitHeightRadio->SetTarget(this);
	fScaleFitBothRadio->SetTarget(this);
	fScaleFitPagesRadio->SetTarget(this);
	fScaleFitWideField->SetTarget(this);
	fScaleFitTallField->SetTarget(this);
	fPrintHeadersBox->SetTarget(this);
	fPrintGridBox->SetTarget(this);
	fCenterHBox->SetTarget(this);
	fCenterVBox->SetTarget(this);
	fHeaderTextField->SetTarget(this);
	fFooterTextField->SetTarget(this);
	fTitleRowsField->SetTarget(this);
	fTitleColsField->SetTarget(this);
	fOrderDownRadio->SetTarget(this);
	fOrderAcrossRadio->SetTarget(this);
	fPrevPageButton->SetTarget(this);
	fNextPageButton->SetTarget(this);
	printButton->SetTarget(this);
	applyButton->SetTarget(this);
}

void PageSetupWindow::SetValues(const AscdPrintSettings& settings)
{
	BString s;
	s << settings.marginTopCm;
	fMarginTopField->SetText(s.String());
	s = "";
	s << settings.marginBottomCm;
	fMarginBottomField->SetText(s.String());
	s = "";
	s << settings.marginLeftCm;
	fMarginLeftField->SetText(s.String());
	s = "";
	s << settings.marginRightCm;
	fMarginRightField->SetText(s.String());

	s = "";
	s << settings.scalePercent;
	fScalePercentField->SetText(s.String());

	fScalePercentRadio->SetValue(settings.scaleMode == 0 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScaleFitWidthRadio->SetValue(settings.scaleMode == 1 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScaleFitHeightRadio->SetValue(settings.scaleMode == 2 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScaleFitBothRadio->SetValue(settings.scaleMode == 3 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScaleFitPagesRadio->SetValue(settings.scaleMode == 4 ? B_CONTROL_ON : B_CONTROL_OFF);
	fScalePercentField->SetEnabled(settings.scaleMode == 0);

	s = "";
	s << settings.fitWide;
	fScaleFitWideField->SetText(s.String());
	s = "";
	s << settings.fitTall;
	fScaleFitTallField->SetText(s.String());
	fScaleFitWideField->SetEnabled(settings.scaleMode == 4);
	fScaleFitTallField->SetEnabled(settings.scaleMode == 4);

	fPrintHeadersBox->SetValue(settings.printHeaders ? B_CONTROL_ON : B_CONTROL_OFF);
	fPrintGridBox->SetValue(settings.printGrid ? B_CONTROL_ON : B_CONTROL_OFF);
	fCenterHBox->SetValue(settings.centerH ? B_CONTROL_ON : B_CONTROL_OFF);
	fCenterVBox->SetValue(settings.centerV ? B_CONTROL_ON : B_CONTROL_OFF);
	fHeaderTextField->SetText(settings.printHeaderText.String());
	fFooterTextField->SetText(settings.printFooterText.String());

	// Titoli formattati come li accetta il parser (vedi sopra): "1:3" e
	// "A:C", riga/colonna singola senza due punti, vuoto se nessuno.
	BString titles;
	if (settings.titleRowFirst >= 1 && settings.titleRowLast >= settings.titleRowFirst)
	{
		titles << settings.titleRowFirst;
		if (settings.titleRowLast != settings.titleRowFirst)
			titles << ":" << settings.titleRowLast;
	}
	fTitleRowsField->SetText(titles.String());
	titles = "";
	if (settings.titleColFirst >= 1 && settings.titleColLast >= settings.titleColFirst)
	{
		char firstName[8], lastName[8];
		PrintColumnName(settings.titleColFirst, firstName, sizeof(firstName));
		PrintColumnName(settings.titleColLast, lastName, sizeof(lastName));
		titles << firstName;
		if (settings.titleColLast != settings.titleColFirst)
			titles << ":" << lastName;
	}
	fTitleColsField->SetText(titles.String());
	fOrderDownRadio->SetValue(!settings.pageOrderAcrossFirst ? B_CONTROL_ON : B_CONTROL_OFF);
	fOrderAcrossRadio->SetValue(settings.pageOrderAcrossFirst ? B_CONTROL_ON : B_CONTROL_OFF);
}

void PageSetupWindow::SetPreviewPages(std::vector<BBitmap*> pages)
{
	fPreviewView->SetPages(pages);
	_UpdatePageNavControls();
}

void PageSetupWindow::_UpdatePageNavControls()
{
	int count = fPreviewView->PageCount();
	if (count == 0)
	{
		fPageLabel->SetText(B_TRANSLATE("Nessuna anteprima"));
		fPrevPageButton->SetEnabled(false);
		fNextPageButton->SetEnabled(false);
		return;
	}

	int index = fPreviewView->PageIndex();
	BString label;
	label << B_TRANSLATE("Pagina") << " " << (index + 1) << " " << B_TRANSLATE("di") << " "
		<< count;
	fPageLabel->SetText(label.String());
	fPrevPageButton->SetEnabled(index > 0);
	fNextPageButton->SetEnabled(index < count - 1);
}

BMessage PageSetupWindow::_BuildSettingsMessage(uint32 what) const
{
	double marginTop = atof(fMarginTopField->Text());
	double marginBottom = atof(fMarginBottomField->Text());
	double marginLeft = atof(fMarginLeftField->Text());
	double marginRight = atof(fMarginRightField->Text());
	// Margini negativi non hanno senso (l'area stampabile diventerebbe
	// piu' grande della pagina stessa, non solo "senza bordo") --
	// azzerati invece di rifiutare silenziosamente la richiesta.
	if (marginTop < 0) marginTop = 0;
	if (marginBottom < 0) marginBottom = 0;
	if (marginLeft < 0) marginLeft = 0;
	if (marginRight < 0) marginRight = 0;

	int scaleMode = 0;
	if (fScaleFitWidthRadio->Value() == B_CONTROL_ON)
		scaleMode = 1;
	else if (fScaleFitHeightRadio->Value() == B_CONTROL_ON)
		scaleMode = 2;
	else if (fScaleFitBothRadio->Value() == B_CONTROL_ON)
		scaleMode = 3;
	else if (fScaleFitPagesRadio->Value() == B_CONTROL_ON)
		scaleMode = 4;

	double scalePercent = atof(fScalePercentField->Text());
	// Una percentuale fuori da un intervallo sensato (Excel stesso
	// limita a 10-400%) ricade sul 100% predefinito, stesso principio
	// gia' seguito per l'intervallo di salvataggio automatico in
	// PreferencesWindow.
	if (scalePercent < 10 || scalePercent > 400)
		scalePercent = 100;

	// Pagine di larghezza/altezza (solo per scaleMode 4): interi >= 1,
	// mai oltre 100 (un "adatta a 1000 pagine" e' indistinguibile dal
	// 100% ma costerebbe un'impaginazione inutile) -- fuori intervallo
	// ricadono su 1x1, stesso principio della percentuale sopra.
	int fitWide = atoi(fScaleFitWideField->Text());
	int fitTall = atoi(fScaleFitTallField->Text());
	if (fitWide < 1 || fitWide > 100) fitWide = 1;
	if (fitTall < 1 || fitTall > 100) fitTall = 1;

	bool printHeaders = fPrintHeadersBox->Value() == B_CONTROL_ON;
	bool printGrid = fPrintGridBox->Value() == B_CONTROL_ON;
	bool centerH = fCenterHBox->Value() == B_CONTROL_ON;
	bool centerV = fCenterVBox->Value() == B_CONTROL_ON;

	BMessage request(what);
	request.AddDouble("marginTop", marginTop);
	request.AddDouble("marginBottom", marginBottom);
	request.AddDouble("marginLeft", marginLeft);
	request.AddDouble("marginRight", marginRight);
	request.AddInt32("scaleMode", scaleMode);
	request.AddDouble("scalePercent", scalePercent);
	request.AddBool("printHeaders", printHeaders);
	request.AddBool("printGrid", printGrid);
	request.AddInt32("fitWide", fitWide);
	request.AddInt32("fitTall", fitTall);
	request.AddBool("centerH", centerH);
	request.AddBool("centerV", centerV);
	request.AddString("headerText", fHeaderTextField->Text());
	request.AddString("footerText", fFooterTextField->Text());
	request.AddBool("pageOrderAcrossFirst",
		fOrderAcrossRadio->Value() == B_CONTROL_ON);

	// Titoli di stampa: testo non valido = nessun titolo (0,0), mai valori
	// a meta' -- gli stessi parser puri dell'impaginazione (vedi sopra).
	int titleRowFirst = 0, titleRowLast = 0, titleColFirst = 0, titleColLast = 0;
	ParsePrintTitleRows(fTitleRowsField->Text(), kRowCount,
		&titleRowFirst, &titleRowLast);
	ParsePrintTitleCols(fTitleColsField->Text(), kColCount,
		&titleColFirst, &titleColLast);
	request.AddInt32("titleRowFirst", titleRowFirst);
	request.AddInt32("titleRowLast", titleRowLast);
	request.AddInt32("titleColFirst", titleColFirst);
	request.AddInt32("titleColLast", titleColLast);
	return request;
}

void PageSetupWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgScaleModeChanged:
		{
			// Il campo percentuale ha senso solo in modalita' percentuale
			// fissa, i campi Larghe/Alte solo in modalita' "Pagine:" --
			// disabilitati (non nascosti, la finestra non e'
			// ridimensionabile) negli altri modi, che calcolano la scala
			// da soli al momento della stampa/anteprima.
			fScalePercentField->SetEnabled(fScalePercentRadio->Value() == B_CONTROL_ON);
			bool fitPages = fScaleFitPagesRadio->Value() == B_CONTROL_ON;
			fScaleFitWideField->SetEnabled(fitPages);
			fScaleFitTallField->SetEnabled(fitPages);
			BMessage preview = _BuildSettingsMessage(kMsgPageSetupPreviewRequest);
			fTarget.SendMessage(&preview);
			break;
		}

		case kMsgFieldChanged:
		{
			BMessage preview = _BuildSettingsMessage(kMsgPageSetupPreviewRequest);
			fTarget.SendMessage(&preview);
			break;
		}

		case kMsgApplyLocal:
		{
			BMessage request = _BuildSettingsMessage(kMsgPageSetupRequest);
			fTarget.SendMessage(&request);
			// L'anteprima mostrava ancora i valori PRECEDENTI se un campo
			// era stato modificato senza Invio (il testo digitato non ha
			// mai generato kMsgFieldChanged): la si rigenera con gli stessi
			// valori appena persistiti, cosi' "quello che si vede e' quello
			// che verra' stampato" resta vero anche in questo percorso.
			// Vale per tutti i campi (margini compresi), non solo per la
			// percentuale -- il messaggio e' identico a quello di un cambio
			// campo, solo costruito qui invece che dal controllo.
			BMessage preview = _BuildSettingsMessage(kMsgPageSetupPreviewRequest);
			fTarget.SendMessage(&preview);
			break;
		}

		case kMsgPrintLocal:
		{
			BMessage request = _BuildSettingsMessage(kMsgPageSetupPrintRequest);
			fTarget.SendMessage(&request);
			// Stesso motivo di kMsgApplyLocal sopra: il dialogo resta
			// aperto dopo la stampa, l'anteprima deve riflettere quanto
			// appena stampato.
			BMessage preview = _BuildSettingsMessage(kMsgPageSetupPreviewRequest);
			fTarget.SendMessage(&preview);
			break;
		}

		case kMsgPrevPage:
			fPreviewView->SetPageIndex(fPreviewView->PageIndex() - 1);
			_UpdatePageNavControls();
			break;

		case kMsgNextPage:
			fPreviewView->SetPageIndex(fPreviewView->PageIndex() + 1);
			_UpdatePageNavControls();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

bool PageSetupWindow::QuitRequested()
{
	// Stessa regola di PreferencesWindow/FindWindow: resta nascosta e
	// riusabile, non viene mai davvero distrutta chiudendola con la X.
	Hide();
	return false;
}
