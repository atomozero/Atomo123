/*
	test_condformat.cpp

	Verifica la formattazione condizionale VIVA per cella (Fase 13):
	MainWindow::ApplyConditionalFormatToSelection/
	RemoveAllConditionalFormatRules resi pubblici apposta per essere
	testabili senza passare da una vera ConditionalFormatWindow, stesso
	principio di SetCellValidation/RemoveCellValidation.

	La logica di valutazione vera e propria (CContainer::
	EvaluateConditionalFormatting, quali celle corrispondono a quali
	regole) e' gia' testata a fondo in ui/tests/test_ascd_io.cpp
	(compresa la prova del "viva": una rivalutazione dopo aver
	cambiato un valore riflette il cambiamento da sola) -- qui invece
	si verifica che ApplyConditionalFormatToSelection costruisca
	davvero una regola con l'intervallo della selezione corrente, e
	che il colore che ne risulta sia DAVVERO disegnato sui pixel da
	SheetView::Draw (stesso principio delle bitmap offscreen gia' usato
	in test_comments.cpp/test_borders.cpp).
*/

#include <cstdio>
#include <cstring>

#include <Application.h>
#include <Bitmap.h>

#include "Cell.h"
#include "Value.h"
#include "Container.h"
#include "CellParser.h"
#include "SheetView.h"
#include "MainWindow.h"

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
	BApplication app("application/x-vnd.Atomo-TestCondFormat");

	MainWindow* win = new MainWindow();
	win->Show();
	win->Lock();

	SheetView* view = win->GetSheetView();
	CContainer* doc = view->Document();

	Check(doc->GetConditionalFormatRules().empty(),
		"un documento appena creato non ha nessuna regola di formattazione condizionale");

	view->SetSelection(cell(1, 1));
	view->ExtendSelection(cell(1, 3)); // A1:A3

	rgb_color red = { 255, 199, 206, 255 };
	win->ApplyConditionalFormatToSelection(0 /* eCondCellIsEqual */, "Mancante", "", 0, 10, false, false, false, red);

	const std::vector<ConditionalFormatRule>& rules = doc->GetConditionalFormatRules();
	Check(rules.size() == 1, "ApplyConditionalFormatToSelection aggiunge una regola");
	if (rules.size() == 1)
	{
		Check(rules[0].type == eCondCellIsEqual, "la regola ha il tipo scelto (uguale a un letterale)");
		Check(rules[0].compareValue == "Mancante", "la regola ha il valore di confronto scelto");
		Check(rules[0].bgColor.red == 255 && rules[0].bgColor.green == 199
				&& rules[0].bgColor.blue == 206,
			"la regola ha il colore scelto");
		Check(rules[0].ranges.size() == 1 && rules[0].ranges[0].left == 1
				&& rules[0].ranges[0].top == 1 && rules[0].ranges[0].right == 1
				&& rules[0].ranges[0].bottom == 3,
			"la regola si applica esattamente alla selezione corrente (A1:A3), non a tutto il foglio");
	}

	// Una seconda regola su un'altra selezione si AGGIUNGE, non
	// sostituisce la prima -- un foglio reale ha in genere piu' di una
	// regola di formattazione condizionale contemporaneamente.
	view->SetSelection(cell(2, 1));
	view->ExtendSelection(cell(2, 3)); // B1:B3
	rgb_color yellow = { 255, 235, 156, 255 };
	win->ApplyConditionalFormatToSelection(1 /* eCondDuplicateValues */, "", "", 0, 10, false, false, false, yellow);
	Check(doc->GetConditionalFormatRules().size() == 2,
		"una seconda regola su un'altra selezione si aggiunge, non sostituisce la prima");

	win->RemoveAllConditionalFormatRules();
	Check(doc->GetConditionalFormatRules().empty(),
		"RemoveAllConditionalFormatRules toglie davvero tutte le regole insieme");

	// Scala di colori (Fase 33/A punto 3): due colori invece di un
	// valore di confronto + un colore solo -- ApplyColorScaleToSelection
	// e' l'equivalente per questo terzo tipo di regola.
	view->SetSelection(cell(3, 1));
	view->ExtendSelection(cell(3, 4)); // C1:C4
	rgb_color scaleMin = { 255, 0, 0, 255 };
	rgb_color scaleMax = { 0, 0, 255, 255 };
	win->ApplyColorScaleToSelection(scaleMin, scaleMax);
	Check(doc->GetConditionalFormatRules().size() == 1,
		"ApplyColorScaleToSelection aggiunge una regola");
	if (doc->GetConditionalFormatRules().size() == 1)
	{
		const ConditionalFormatRule& scaleRule = doc->GetConditionalFormatRules()[0];
		Check(scaleRule.type == eCondColorScale, "la regola ha il tipo scala di colori");
		Check(scaleRule.colorScalePoints.size() == 2,
			"la scala a due colori ha esattamente due punti di controllo");
		if (scaleRule.colorScalePoints.size() == 2)
		{
			Check(scaleRule.colorScalePoints[0].cfvoType == "min"
					&& scaleRule.colorScalePoints[0].color.red == 255
					&& scaleRule.colorScalePoints[0].color.blue == 0,
				"il primo punto e' il colore minimo scelto");
			Check(scaleRule.colorScalePoints[1].cfvoType == "max"
					&& scaleRule.colorScalePoints[1].color.red == 0
					&& scaleRule.colorScalePoints[1].color.blue == 255,
				"il secondo punto e' il colore massimo scelto");
		}
		Check(scaleRule.ranges.size() == 1 && scaleRule.ranges[0].left == 3
				&& scaleRule.ranges[0].top == 1 && scaleRule.ranges[0].right == 3
				&& scaleRule.ranges[0].bottom == 4,
			"la regola di scala di colori si applica esattamente alla selezione corrente (C1:C4)");
	}

	win->RemoveAllConditionalFormatRules();

	// Barra dei dati (Tier 3, Fase B): un solo colore invece di due --
	// ApplyDataBarToSelection e' l'equivalente per questo quarto tipo
	// di regola.
	view->SetSelection(cell(4, 1));
	view->ExtendSelection(cell(4, 4)); // D1:D4
	rgb_color barColor = { 99, 142, 198, 255 };
	win->ApplyDataBarToSelection(barColor);
	Check(doc->GetConditionalFormatRules().size() == 1,
		"ApplyDataBarToSelection aggiunge una regola");
	if (doc->GetConditionalFormatRules().size() == 1)
	{
		const ConditionalFormatRule& barRule = doc->GetConditionalFormatRules()[0];
		Check(barRule.type == eCondDataBar, "la regola ha il tipo barra dei dati");
		Check(barRule.dataBarColor.red == 99 && barRule.dataBarColor.green == 142
				&& barRule.dataBarColor.blue == 198,
			"la regola ha il colore scelto");
		Check(barRule.colorScalePoints.size() == 2,
			"la barra ha esattamente due soglie (min/max), riusando ColorScalePoint");
		if (barRule.colorScalePoints.size() == 2)
		{
			Check(barRule.colorScalePoints[0].cfvoType == "min", "la prima soglia e' il minimo");
			Check(barRule.colorScalePoints[1].cfvoType == "max", "la seconda soglia e' il massimo");
		}
		Check(barRule.ranges.size() == 1 && barRule.ranges[0].left == 4
				&& barRule.ranges[0].top == 1 && barRule.ranges[0].right == 4
				&& barRule.ranges[0].bottom == 4,
			"la regola di barra dei dati si applica esattamente alla selezione corrente (D1:D4)");
	}

	win->RemoveAllConditionalFormatRules();

	// Icon set (Tier 3, Fase C): nessun colore scelto dall'utente --
	// ApplyIconSetToSelection e' l'equivalente per questo quinto tipo
	// di regola, sempre a 3 livelli/soglie percentuali 0/33/67.
	view->SetSelection(cell(5, 1));
	view->ExtendSelection(cell(5, 4)); // E1:E4
	win->ApplyIconSetToSelection();
	Check(doc->GetConditionalFormatRules().size() == 1,
		"ApplyIconSetToSelection aggiunge una regola");
	if (doc->GetConditionalFormatRules().size() == 1)
	{
		const ConditionalFormatRule& iconRule = doc->GetConditionalFormatRules()[0];
		Check(iconRule.type == eCondIconSet, "la regola ha il tipo icon set");
		Check(iconRule.colorScalePoints.size() == 3,
			"il set a 3 livelli ha esattamente tre soglie, riusando ColorScalePoint");
		if (iconRule.colorScalePoints.size() == 3)
		{
			Check(iconRule.colorScalePoints[0].cfvoType == "percent"
					&& iconRule.colorScalePoints[0].cfvoValue == 0,
				"la prima soglia e' 0%");
			Check(iconRule.colorScalePoints[1].cfvoType == "percent"
					&& iconRule.colorScalePoints[1].cfvoValue == 33,
				"la seconda soglia e' 33%");
			Check(iconRule.colorScalePoints[2].cfvoType == "percent"
					&& iconRule.colorScalePoints[2].cfvoValue == 67,
				"la terza soglia e' 67%");
		}
		Check(iconRule.ranges.size() == 1 && iconRule.ranges[0].left == 5
				&& iconRule.ranges[0].top == 1 && iconRule.ranges[0].right == 5
				&& iconRule.ranges[0].bottom == 4,
			"la regola di icon set si applica esattamente alla selezione corrente (E1:E4)");
	}

	win->RemoveAllConditionalFormatRules();

	win->Unlock();

	// --- Il colore si vede davvero sui pixel (non solo "il codice per
	// disegnarlo e' stato eseguito"), e si ricalcola da solo -- non
	// scrive mai in CellStyle. ---
	{
		CContainer* doc2 = new CContainer(NULL, NULL);
		TryToParseString("Mancante", cell(1, 1), doc2, true); // A1, corrisponde
		TryToParseString("OK", cell(1, 2), doc2, true);       // A2, non corrisponde

		ConditionalFormatRule rule;
		rule.type = eCondCellIsEqual;
		rule.compareValue = "Mancante";
		rule.bgColor = red;
		rule.ranges.push_back(range(1, 1, 1, 2)); // A1:A2
		doc2->AddConditionalFormatRule(rule);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view2 = new SheetView(doc2);
		view2->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view2);

		bool locked = canvas->Lock();
		Check(locked, "la bitmap offscreen per la formattazione condizionale si blocca per disegnarci sopra");

		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// B_RGB32 in memoria: B, G, R, A -- il colore atteso e'
		// (255,199,206) in R,G,B, quindi (206,199,255) in ordine BGRA.
		BRect a1 = view2->CellRect(cell(1, 1));
		uint8* pxA1 = bits + (int32)(a1.top + 3) * bpr + (int32)(a1.left + 3) * 4;
		Check(pxA1[0] > 190 && pxA1[1] > 180 && pxA1[2] > 240,
			"A1 (\"Mancante\", corrisponde alla regola) ha davvero lo sfondo colorato sui pixel");

		BRect a2 = view2->CellRect(cell(1, 2));
		uint8* pxA2 = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.left + 3) * 4;
		Check(pxA2[0] > 250 && pxA2[1] > 250 && pxA2[2] > 250,
			"A2 (\"OK\", non corrisponde) resta bianca");

		// Cambia il valore di A2 DOPO il primo disegno, senza toccare
		// ne' la regola ne' CellStyle -- un secondo Draw() deve
		// riflettere il nuovo valore da solo.
		TryToParseString("Mancante", cell(1, 2), doc2, true);
		canvas->Lock();
		view2->Draw(canvasRect);
		view2->Sync();
		canvas->Unlock();

		uint8* pxA2After = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.left + 3) * 4;
		Check(pxA2After[0] > 190 && pxA2After[1] > 180 && pxA2After[2] > 240,
			"dopo aver cambiato A2 in \"Mancante\", un nuovo Draw() la colora da solo (e' questo che la rende viva)");

		delete canvas;
		doc2->Release();
	}

	// --- Scala di colori: interpolazione REALE sui pixel, non solo sul
	// dato in memoria -- min->max su tre celle numeriche 1,2,3, piu' una
	// cella di testo che deve restare non colorata (fuori scala, non e'
	// un numero). ---
	{
		CContainer* doc3 = new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), doc3, true);   // A1 = 1 (minimo)
		TryToParseString("2", cell(1, 2), doc3, true);   // A2 = 2 (a meta')
		TryToParseString("3", cell(1, 3), doc3, true);   // A3 = 3 (massimo)
		TryToParseString("testo", cell(1, 4), doc3, true); // A4, non numerico

		ConditionalFormatRule rule;
		rule.type = eCondColorScale;
		rule.ranges.push_back(range(1, 1, 1, 4)); // A1:A4
		ColorScalePoint minPoint;
		minPoint.cfvoType = "min";
		minPoint.color = scaleMin; // rosso (255,0,0)
		rule.colorScalePoints.push_back(minPoint);
		ColorScalePoint maxPoint;
		maxPoint.cfvoType = "max";
		maxPoint.color = scaleMax; // blu (0,0,255)
		rule.colorScalePoints.push_back(maxPoint);
		doc3->AddConditionalFormatRule(rule);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view3 = new SheetView(doc3);
		view3->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view3);

		canvas->Lock();
		view3->Draw(canvasRect);
		view3->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// B_RGB32 in memoria: B, G, R, A.
		BRect a1 = view3->CellRect(cell(1, 1));
		uint8* pxA1 = bits + (int32)(a1.top + 3) * bpr + (int32)(a1.left + 3) * 4;
		Check(pxA1[0] < 30 && pxA1[2] > 220,
			"A1 (valore minimo, 1) e' colorata col colore minimo (rosso puro)");

		BRect a2 = view3->CellRect(cell(1, 2));
		uint8* pxA2 = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.left + 3) * 4;
		Check(pxA2[0] > 100 && pxA2[0] < 155 && pxA2[2] > 100 && pxA2[2] < 155,
			"A2 (valore a meta' della scala, 2) e' colorata da un'interpolazione a meta' strada");

		BRect a3 = view3->CellRect(cell(1, 3));
		uint8* pxA3 = bits + (int32)(a3.top + 3) * bpr + (int32)(a3.left + 3) * 4;
		Check(pxA3[0] > 220 && pxA3[2] < 30,
			"A3 (valore massimo, 3) e' colorata col colore massimo (blu puro)");

		BRect a4 = view3->CellRect(cell(1, 4));
		uint8* pxA4 = bits + (int32)(a4.top + 3) * bpr + (int32)(a4.left + 3) * 4;
		Check(pxA4[0] > 250 && pxA4[1] > 250 && pxA4[2] > 250,
			"A4 (testo, non numerico) resta bianca -- la scala di colori ignora le celle non numeriche");

		delete canvas;
		doc3->Release();
	}

	// --- Barra dei dati: a differenza della scala di colori sopra, il
	// risultato non e' un colore diverso per cella ma la STESSA
	// colonna, riempita colorata solo per una FRAZIONE della sua
	// larghezza (kColWidth = 80px, vedi SheetView.h) proporzionale al
	// valore -- min->0% (nessun pixel colorato, nemmeno vicino al
	// bordo sinistro), meta'->50% (colorato vicino al bordo sinistro,
	// bianco vicino a quello destro), max->100% (colorato ovunque). ---
	{
		CContainer* doc6 = new CContainer(NULL, NULL);
		TryToParseString("1", cell(1, 1), doc6, true); // A1 = 1 (minimo)
		TryToParseString("2", cell(1, 2), doc6, true); // A2 = 2 (a meta')
		TryToParseString("3", cell(1, 3), doc6, true); // A3 = 3 (massimo)

		ConditionalFormatRule rule;
		rule.type = eCondDataBar;
		rule.dataBarColor = barColor; // (99,142,198)
		rule.ranges.push_back(range(1, 1, 1, 3)); // A1:A3
		ColorScalePoint minPoint;
		minPoint.cfvoType = "min";
		rule.colorScalePoints.push_back(minPoint);
		ColorScalePoint maxPoint;
		maxPoint.cfvoType = "max";
		rule.colorScalePoints.push_back(maxPoint);
		doc6->AddConditionalFormatRule(rule);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view6 = new SheetView(doc6);
		view6->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view6);

		canvas->Lock();
		view6->Draw(canvasRect);
		view6->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// B_RGB32 in memoria: B, G, R, A -- il colore atteso e'
		// (99,142,198) in R,G,B, quindi (198,142,99) in ordine BGRA.
		BRect a1 = view6->CellRect(cell(1, 1));
		uint8* pxA1Near = bits + (int32)(a1.top + 3) * bpr + (int32)(a1.left + 3) * 4;
		Check(pxA1Near[0] > 250 && pxA1Near[1] > 250 && pxA1Near[2] > 250,
			"A1 (valore minimo, frazione 0): nessuna barra, resta bianca anche vicino al bordo sinistro");

		BRect a2 = view6->CellRect(cell(1, 2));
		uint8* pxA2Near = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.left + 3) * 4;
		Check(pxA2Near[0] > 180 && pxA2Near[0] < 220 && pxA2Near[2] > 80 && pxA2Near[2] < 120,
			"A2 (valore a meta', frazione 0.5): colorata vicino al bordo sinistro");
		uint8* pxA2Far = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.right - 3) * 4;
		Check(pxA2Far[0] > 250 && pxA2Far[1] > 250 && pxA2Far[2] > 250,
			"A2 (valore a meta', frazione 0.5): resta bianca vicino al bordo destro, oltre meta' barra");

		BRect a3 = view6->CellRect(cell(1, 3));
		uint8* pxA3Far = bits + (int32)(a3.top + 3) * bpr + (int32)(a3.right - 3) * 4;
		Check(pxA3Far[0] > 180 && pxA3Far[0] < 220 && pxA3Far[2] > 80 && pxA3Far[2] < 120,
			"A3 (valore massimo, frazione 1): colorata fino al bordo destro");

		delete canvas;
		doc6->Release();
	}

	// --- Icon set: tre celle numeriche (10=minimo, 50=a meta',
	// 90=massimo) devono ricevere tre icone DIVERSE (indici 0/1/2,
	// rosso/giallo/verde) -- non un unico colore su tutta la colonna
	// come la scala di colori, e non una barra parziale come la barra
	// dei dati: un piccolo cerchio pieno nell'angolo in alto a sinistra
	// di ogni cella. Soglie 0/33/67 percento di min/max (10/90):
	// 10 (0%->10), 36,4 (33%), 63,6 (67%) -- 10 supera solo la prima
	// soglia (icona 0), 50 supera anche la seconda (icona 1), 90 le
	// supera tutte e tre (icona 2). ---
	{
		CContainer* doc7 = new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), doc7, true); // A1 = 10 (minimo)
		TryToParseString("50", cell(1, 2), doc7, true); // A2 = 50 (a meta')
		TryToParseString("90", cell(1, 3), doc7, true); // A3 = 90 (massimo)

		ConditionalFormatRule rule;
		rule.type = eCondIconSet;
		rule.ranges.push_back(range(1, 1, 1, 3)); // A1:A3
		const double kPercents[3] = { 0, 33, 67 };
		for (int p = 0; p < 3; p++)
		{
			ColorScalePoint point;
			point.cfvoType = "percent";
			point.cfvoValue = kPercents[p];
			rule.colorScalePoints.push_back(point);
		}
		doc7->AddConditionalFormatRule(rule);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view7 = new SheetView(doc7);
		view7->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view7);

		canvas->Lock();
		view7->Draw(canvasRect);
		view7->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		// B_RGB32 in memoria: B, G, R, A. Icona disegnata come cerchio
		// pieno di raggio 6 centrato vicino al bordo DESTRO della
		// cella (cellRight-8, cellTop+8), non sinistro -- questo
		// motore allinea eAlignGeneral (il default, mai toccato dal
		// menu Formato) sempre a sinistra anche per i numeri, a
		// differenza di Excel: un'icona a sinistra finirebbe coperta
		// dalla cifra stessa, disegnata sopra in un ciclo successivo
		// (vedi il commento gemello in SheetView::DrawCellBand). Bug
		// reale trovato da QUESTO test durante lo sviluppo (icona
		// verde attesa, pixel nero campionato -- la cifra "9", non il
		// cerchio) prima di spostare l'icona a destra.
		BRect a1 = view7->CellRect(cell(1, 1));
		uint8* pxA1 = bits + (int32)(a1.top + 8) * bpr + (int32)(a1.right - 8) * 4;
		Check(pxA1[2] > 180 && pxA1[1] < 100 && pxA1[0] < 100,
			"A1 (valore minimo, 10) ha l'icona rossa (livello 0)");

		BRect a2 = view7->CellRect(cell(1, 2));
		uint8* pxA2 = bits + (int32)(a2.top + 8) * bpr + (int32)(a2.right - 8) * 4;
		Check(pxA2[2] > 200 && pxA2[1] > 150 && pxA2[0] < 100,
			"A2 (valore a meta', 50) ha l'icona gialla/arancio (livello 1)");

		BRect a3 = view7->CellRect(cell(1, 3));
		uint8* pxA3 = bits + (int32)(a3.top + 8) * bpr + (int32)(a3.right - 8) * 4;
		Check(pxA3[1] > 140 && pxA3[2] < 100 && pxA3[0] < 100,
			"A3 (valore massimo, 90) ha l'icona verde (livello 2)");

		delete canvas;
		doc7->Release();
	}

	// --- Confronto contro un RIFERIMENTO DI CELLA (compareIsCellRef),
	// non un letterale fisso -- il caso reale scoperto analizzando
	// agile-kanban-board.xlsx (Vertex42): "cellIs"/"equal" contro
	// $C$29 invece che contro "High"/"Low"/ecc, una "legenda" altrove
	// nel foglio. Verifica sia il colore vero sui pixel sia che sia
	// DAVVERO viva: cambiare il testo della cella di riferimento (non
	// solo il valore della cella regolata) deve cambiare da solo quali
	// celle si colorano al prossimo Draw(), senza toccare la regola. ---
	{
		CContainer* doc4 = new CContainer(NULL, NULL);
		TryToParseString("High", cell(1, 1), doc4, true);   // A1
		TryToParseString("Low", cell(1, 2), doc4, true);    // A2
		TryToParseString("High", cell(2, 1), doc4, true);   // B1, la "legenda" referenziata

		ConditionalFormatRule rule;
		rule.type = eCondCellIsEqual;
		rule.compareIsCellRef = true;
		rule.compareRefCell = cell(2, 1); // B1
		rule.bgColor = red;
		rule.ranges.push_back(range(1, 1, 1, 2)); // A1:A2
		doc4->AddConditionalFormatRule(rule);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view4 = new SheetView(doc4);
		view4->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view4);

		canvas->Lock();
		view4->Draw(canvasRect);
		view4->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		BRect a1 = view4->CellRect(cell(1, 1));
		uint8* pxA1 = bits + (int32)(a1.top + 3) * bpr + (int32)(a1.left + 3) * 4;
		Check(pxA1[0] > 190 && pxA1[1] > 180 && pxA1[2] > 240,
			"A1 (\"High\", uguale al testo ATTUALE di B1) e' colorata, il confronto e' contro B1 non un letterale");

		BRect a2 = view4->CellRect(cell(1, 2));
		uint8* pxA2 = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.left + 3) * 4;
		Check(pxA2[0] > 250 && pxA2[1] > 250 && pxA2[2] > 250,
			"A2 (\"Low\", diverso da B1) resta bianca");

		// Cambia il testo della cella REFERENZIATA (B1), non una delle
		// celle regolate -- un nuovo Draw() deve seguire il nuovo
		// valore di B1 da solo, la prova vera che il confronto e' vivo
		// e non congelato al testo di B1 letto una volta sola.
		TryToParseString("Low", cell(2, 1), doc4, true); // B1 ora "Low"
		canvas->Lock();
		view4->Draw(canvasRect);
		view4->Sync();
		canvas->Unlock();

		uint8* pxA1After = bits + (int32)(a1.top + 3) * bpr + (int32)(a1.left + 3) * 4;
		Check(pxA1After[0] > 250 && pxA1After[1] > 250 && pxA1After[2] > 250,
			"dopo aver cambiato B1 a \"Low\", A1 (\"High\") non corrisponde piu' e torna bianca da sola");

		uint8* pxA2After = bits + (int32)(a2.top + 3) * bpr + (int32)(a2.left + 3) * 4;
		Check(pxA2After[0] > 190 && pxA2After[1] > 180 && pxA2After[2] > 240,
			"A2 (\"Low\") ora corrisponde al nuovo testo di B1 e si colora da sola");

		delete canvas;
		doc4->Release();
	}

	// --- Regola "expression" (eCondExpression): una formula booleana
	// arbitraria con un riferimento RELATIVO, es. "(C1=$C$29)" applicata
	// a un intervallo B1:B3 -- il caso reale scoperto analizzando
	// agile-kanban-board.xlsx (Vertex42): le bande colorate della
	// colonna B derivano da 5 regole cosi', non da eCondCellIsEqual
	// (quello sopra e' per Type/Priority in C/G). "C1" e' relativo alla
	// cella in alto a sinistra del PRIMO intervallo (B1 qui): per B2 si
	// sposta a C2, per B3 a C3 -- esattamente come se la stessa formula
	// fosse stata incollata su ogni riga, la stessa identica prova che
	// CFormula::Calculate risolve da solo l'offset relativo dalla
	// "inLocation" passata cella per cella. ---
	{
		CContainer* doc5 = new CContainer(NULL, NULL);
		TryToParseString("High", cell(3, 1), doc5, true); // C1
		TryToParseString("Low", cell(3, 2), doc5, true);  // C2
		TryToParseString("High", cell(3, 3), doc5, true); // C3
		TryToParseString("High", cell(5, 1), doc5, true); // E1, la "legenda" (equivalente a $C$29 nel file reale)

		ConditionalFormatRule rule;
		rule.type = eCondExpression;
		rule.expressionFormula = "C1=$E$1";
		rule.bgColor = red;
		rule.ranges.push_back(range(2, 1, 2, 3)); // B1:B3 (colonna 2)
		doc5->AddConditionalFormatRule(rule);

		BRect canvasRect(0, 0, 799, 599);
		BBitmap* canvas = new BBitmap(canvasRect, B_RGB32, true);
		SheetView* view5 = new SheetView(doc5);
		view5->ResizeTo(canvasRect.Width(), canvasRect.Height());
		canvas->AddChild(view5);

		canvas->Lock();
		view5->Draw(canvasRect);
		view5->Sync();
		canvas->Unlock();

		uint8* bits = (uint8*)canvas->Bits();
		int32 bpr = canvas->BytesPerRow();

		BRect b1 = view5->CellRect(cell(2, 1));
		uint8* pxB1 = bits + (int32)(b1.top + 3) * bpr + (int32)(b1.left + 3) * 4;
		Check(pxB1[0] > 190 && pxB1[1] > 180 && pxB1[2] > 240,
			"B1 e' colorata: la formula relativa si e' spostata a C1, che vale \"High\" come $E$1");

		BRect b2 = view5->CellRect(cell(2, 2));
		uint8* pxB2 = bits + (int32)(b2.top + 3) * bpr + (int32)(b2.left + 3) * 4;
		Check(pxB2[0] > 250 && pxB2[1] > 250 && pxB2[2] > 250,
			"B2 resta bianca: la formula relativa si e' spostata a C2, che vale \"Low\", diverso da $E$1");

		BRect b3 = view5->CellRect(cell(2, 3));
		uint8* pxB3 = bits + (int32)(b3.top + 3) * bpr + (int32)(b3.left + 3) * 4;
		Check(pxB3[0] > 190 && pxB3[1] > 180 && pxB3[2] > 240,
			"B3 e' colorata: la formula relativa si e' spostata a C3, di nuovo \"High\"");

		delete canvas;
		doc5->Release();
	}

	// --- Path to full Excel parity, Tier 3: cellIs con gli operatori
	// numerici diversi da "equal" (0), containsText/containsBlanks/
	// containsErrors, top10 e aboveAverage -- qui via
	// CContainer::EvaluateConditionalFormatting direttamente (map<cell,
	// rgb_color>), non tramite pixel: il disegno vero per uno sfondo
	// singolo e' gia' provato a fondo sopra per eCondCellIsEqual
	// "equal", stessa identica strada di disegno per tutti i tipi
	// nuovi (SheetView::DrawCellBand legge lo stesso
	// EvaluateConditionalFormatting). ---
	{
		CContainer* doc8 = new CContainer(NULL, NULL);
		TryToParseString("5", cell(1, 1), doc8, true);  // A1 = 5
		TryToParseString("10", cell(1, 2), doc8, true); // A2 = 10
		TryToParseString("15", cell(1, 3), doc8, true); // A3 = 15
		TryToParseString("20", cell(1, 4), doc8, true); // A4 = 20
		TryToParseString("25", cell(1, 5), doc8, true); // A5 = 25

		ConditionalFormatRule rule;
		rule.type = eCondCellIsEqual;
		rule.ruleOperator = 2; // greaterThan
		rule.compareValue = "15";
		rule.bgColor = red;
		rule.ranges.push_back(range(1, 1, 1, 5));
		doc8->AddConditionalFormatRule(rule);

		std::map<cell, rgb_color> result = doc8->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) == result.end(), "cellIs greaterThan 15: A1=5 non corrisponde");
		Check(result.find(cell(1, 3)) == result.end(), "cellIs greaterThan 15: A3=15 non corrisponde (non e' >)");
		Check(result.find(cell(1, 4)) != result.end(), "cellIs greaterThan 15: A4=20 corrisponde");
		Check(result.find(cell(1, 5)) != result.end(), "cellIs greaterThan 15: A5=25 corrisponde");

		doc8->ClearConditionalFormatRules();
		ConditionalFormatRule between;
		between.type = eCondCellIsEqual;
		between.ruleOperator = 6; // between
		between.compareValue = "10";
		between.compareValue2 = "20";
		between.bgColor = red;
		between.ranges.push_back(range(1, 1, 1, 5));
		doc8->AddConditionalFormatRule(between);
		result = doc8->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) == result.end(), "cellIs between 10..20: A1=5 fuori dal limite inferiore");
		Check(result.find(cell(1, 2)) != result.end(), "cellIs between 10..20: A2=10 e' il limite inferiore incluso");
		Check(result.find(cell(1, 3)) != result.end(), "cellIs between 10..20: A3=15 e' dentro");
		Check(result.find(cell(1, 4)) != result.end(), "cellIs between 10..20: A4=20 e' il limite superiore incluso");
		Check(result.find(cell(1, 5)) == result.end(), "cellIs between 10..20: A5=25 fuori dal limite superiore");

		doc8->ClearConditionalFormatRules();
		ConditionalFormatRule notBetween;
		notBetween.type = eCondCellIsEqual;
		notBetween.ruleOperator = 7; // notBetween
		notBetween.compareValue = "10";
		notBetween.compareValue2 = "20";
		notBetween.bgColor = red;
		notBetween.ranges.push_back(range(1, 1, 1, 5));
		doc8->AddConditionalFormatRule(notBetween);
		result = doc8->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) != result.end(), "cellIs notBetween 10..20: A1=5 e' fuori, quindi corrisponde");
		Check(result.find(cell(1, 3)) == result.end(), "cellIs notBetween 10..20: A3=15 e' dentro, quindi non corrisponde");
		Check(result.find(cell(1, 5)) != result.end(), "cellIs notBetween 10..20: A5=25 e' fuori, quindi corrisponde");

		doc8->Release();
	}

	{
		CContainer* doc9 = new CContainer(NULL, NULL);
		TryToParseString("Rosso Mela", cell(1, 1), doc9, true);
		TryToParseString("Verde Lime", cell(1, 2), doc9, true);
		TryToParseString("Blu Cielo", cell(1, 3), doc9, true);

		ConditionalFormatRule contains;
		contains.type = eCondTextRule;
		contains.ruleOperator = 0; // contains, case-insensitive
		contains.compareValue = "rosso";
		contains.bgColor = red;
		contains.ranges.push_back(range(1, 1, 1, 3));
		doc9->AddConditionalFormatRule(contains);
		std::map<cell, rgb_color> result = doc9->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) != result.end(), "containsText \"rosso\" trova \"Rosso Mela\" (case-insensitive)");
		Check(result.find(cell(1, 2)) == result.end(), "containsText \"rosso\" non trova \"Verde Lime\"");

		doc9->ClearConditionalFormatRules();
		ConditionalFormatRule beginsWith;
		beginsWith.type = eCondTextRule;
		beginsWith.ruleOperator = 2; // beginsWith
		beginsWith.compareValue = "Verde";
		beginsWith.bgColor = red;
		beginsWith.ranges.push_back(range(1, 1, 1, 3));
		doc9->AddConditionalFormatRule(beginsWith);
		result = doc9->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 2)) != result.end(), "beginsWith \"Verde\" trova \"Verde Lime\"");
		Check(result.find(cell(1, 1)) == result.end(), "beginsWith \"Verde\" non trova \"Rosso Mela\"");

		doc9->ClearConditionalFormatRules();
		ConditionalFormatRule endsWith;
		endsWith.type = eCondTextRule;
		endsWith.ruleOperator = 3; // endsWith
		endsWith.compareValue = "Cielo";
		endsWith.bgColor = red;
		endsWith.ranges.push_back(range(1, 1, 1, 3));
		doc9->AddConditionalFormatRule(endsWith);
		result = doc9->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 3)) != result.end(), "endsWith \"Cielo\" trova \"Blu Cielo\"");
		Check(result.find(cell(1, 1)) == result.end(), "endsWith \"Cielo\" non trova \"Rosso Mela\"");

		doc9->ClearConditionalFormatRules();
		ConditionalFormatRule notContains;
		notContains.type = eCondTextRule;
		notContains.ruleOperator = 1; // notContains
		notContains.compareValue = "rosso";
		notContains.bgColor = red;
		notContains.ranges.push_back(range(1, 1, 1, 3));
		doc9->AddConditionalFormatRule(notContains);
		result = doc9->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) == result.end(), "notContainsText \"rosso\": \"Rosso Mela\" non corrisponde");
		Check(result.find(cell(1, 2)) != result.end(), "notContainsText \"rosso\": \"Verde Lime\" corrisponde");

		doc9->Release();
	}

	{
		CContainer* doc10 = new CContainer(NULL, NULL);
		TryToParseString("valore", cell(1, 1), doc10, true); // A1, non vuota
		// A2 resta vuota davvero (mai scritta).
		TryToParseString("=1/0", cell(1, 3), doc10, true);   // A3, errore

		ConditionalFormatRule blanks;
		blanks.type = eCondBlankErrorRule;
		blanks.ruleOperator = 0; // containsBlanks
		blanks.bgColor = red;
		blanks.ranges.push_back(range(1, 1, 1, 3));
		doc10->AddConditionalFormatRule(blanks);
		std::map<cell, rgb_color> result = doc10->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) == result.end(), "containsBlanks: A1 (valore) non e' vuota");
		Check(result.find(cell(1, 2)) != result.end(), "containsBlanks: A2 (mai scritta) e' vuota");

		doc10->ClearConditionalFormatRules();
		ConditionalFormatRule notBlanks;
		notBlanks.type = eCondBlankErrorRule;
		notBlanks.ruleOperator = 1; // notContainsBlanks
		notBlanks.bgColor = red;
		notBlanks.ranges.push_back(range(1, 1, 1, 3));
		doc10->AddConditionalFormatRule(notBlanks);
		result = doc10->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) != result.end(), "notContainsBlanks: A1 (valore) corrisponde");
		Check(result.find(cell(1, 2)) == result.end(), "notContainsBlanks: A2 (vuota) non corrisponde");

		doc10->ClearConditionalFormatRules();
		ConditionalFormatRule errors;
		errors.type = eCondBlankErrorRule;
		errors.ruleOperator = 2; // containsErrors
		errors.bgColor = red;
		errors.ranges.push_back(range(1, 1, 1, 3));
		doc10->AddConditionalFormatRule(errors);
		result = doc10->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 3)) != result.end(), "containsErrors: A3 (1/0) e' un errore");
		Check(result.find(cell(1, 1)) == result.end(), "containsErrors: A1 (valore) non e' un errore");

		doc10->ClearConditionalFormatRules();
		ConditionalFormatRule notErrors;
		notErrors.type = eCondBlankErrorRule;
		notErrors.ruleOperator = 3; // notContainsErrors
		notErrors.bgColor = red;
		notErrors.ranges.push_back(range(1, 1, 1, 3));
		doc10->AddConditionalFormatRule(notErrors);
		result = doc10->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) != result.end(), "notContainsErrors: A1 (valore) corrisponde");
		Check(result.find(cell(1, 3)) == result.end(), "notContainsErrors: A3 (errore) non corrisponde");

		doc10->Release();
	}

	{
		// 10 valori: 10,20,...,100 -- primi 3 = {80,90,100}, ultimi 3 =
		// {10,20,30}, primi 20% (2 valori) = {90,100}.
		CContainer* doc11 = new CContainer(NULL, NULL);
		for (int r = 1; r <= 10; r++)
		{
			char buf[8];
			snprintf(buf, sizeof(buf), "%d", r * 10);
			TryToParseString(buf, cell(1, r), doc11, true);
		}

		ConditionalFormatRule top3;
		top3.type = eCondTop10;
		top3.top10Rank = 3;
		top3.bgColor = red;
		top3.ranges.push_back(range(1, 1, 1, 10));
		doc11->AddConditionalFormatRule(top3);
		std::map<cell, rgb_color> result = doc11->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 10)) != result.end(), "top10 rank=3: A10=100 e' fra i primi 3");
		Check(result.find(cell(1, 8)) != result.end(), "top10 rank=3: A8=80 e' fra i primi 3");
		Check(result.find(cell(1, 7)) == result.end(), "top10 rank=3: A7=70 non e' fra i primi 3");

		doc11->ClearConditionalFormatRules();
		ConditionalFormatRule bottom3;
		bottom3.type = eCondTop10;
		bottom3.top10Rank = 3;
		bottom3.top10Bottom = true;
		bottom3.bgColor = red;
		bottom3.ranges.push_back(range(1, 1, 1, 10));
		doc11->AddConditionalFormatRule(bottom3);
		result = doc11->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) != result.end(), "top10 bottom rank=3: A1=10 e' fra gli ultimi 3");
		Check(result.find(cell(1, 3)) != result.end(), "top10 bottom rank=3: A3=30 e' fra gli ultimi 3");
		Check(result.find(cell(1, 4)) == result.end(), "top10 bottom rank=3: A4=40 non e' fra gli ultimi 3");

		doc11->ClearConditionalFormatRules();
		ConditionalFormatRule top20pct;
		top20pct.type = eCondTop10;
		top20pct.top10Rank = 20;
		top20pct.top10Percent = true;
		top20pct.bgColor = red;
		top20pct.ranges.push_back(range(1, 1, 1, 10));
		doc11->AddConditionalFormatRule(top20pct);
		result = doc11->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 10)) != result.end(), "top10 20%: A10=100 e' nel 20% superiore (2 valori su 10)");
		Check(result.find(cell(1, 9)) != result.end(), "top10 20%: A9=90 e' nel 20% superiore");
		Check(result.find(cell(1, 8)) == result.end(), "top10 20%: A8=80 non e' nel 20% superiore");

		doc11->Release();
	}

	{
		// 5 valori: 10,20,30,40,50 -- media 30. Sopra: 40,50. Sotto: 10,20.
		CContainer* doc12 = new CContainer(NULL, NULL);
		TryToParseString("10", cell(1, 1), doc12, true);
		TryToParseString("20", cell(1, 2), doc12, true);
		TryToParseString("30", cell(1, 3), doc12, true);
		TryToParseString("40", cell(1, 4), doc12, true);
		TryToParseString("50", cell(1, 5), doc12, true);

		ConditionalFormatRule above;
		above.type = eCondAboveAverage;
		above.bgColor = red;
		above.ranges.push_back(range(1, 1, 1, 5));
		doc12->AddConditionalFormatRule(above);
		std::map<cell, rgb_color> result = doc12->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 4)) != result.end(), "aboveAverage: A4=40 e' sopra la media (30)");
		Check(result.find(cell(1, 3)) == result.end(), "aboveAverage: A3=30 (la media stessa) non e' sopra");
		Check(result.find(cell(1, 1)) == result.end(), "aboveAverage: A1=10 non e' sopra");

		doc12->ClearConditionalFormatRules();
		ConditionalFormatRule below;
		below.type = eCondAboveAverage;
		below.belowAverage = true;
		below.bgColor = red;
		below.ranges.push_back(range(1, 1, 1, 5));
		doc12->AddConditionalFormatRule(below);
		result = doc12->EvaluateConditionalFormatting();
		Check(result.find(cell(1, 1)) != result.end(), "belowAverage: A1=10 e' sotto la media (30)");
		Check(result.find(cell(1, 3)) == result.end(), "belowAverage: A3=30 (la media stessa) non e' sotto");
		Check(result.find(cell(1, 5)) == result.end(), "belowAverage: A5=50 non e' sotto");

		doc12->Release();
	}

	win->Lock();
	win->Quit();

	printf("\n%s\n", gFailures == 0 ? "TUTTI I TEST SONO PASSATI" : "ALCUNI TEST SONO FALLITI");
	return gFailures == 0 ? 0 : 1;
}
