/*
	MainWindow.cpp

	Vedi MainWindow.h.
*/

#include "MainWindow.h"
#include "SheetView.h"
#include "AscdIO.h"

#include <cstdio>
#include <cstring>

#include <Alert.h>
#include <Application.h>
#include <Clipboard.h>
#include <Directory.h>
#include <File.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Path.h>
#include <ScrollView.h>
#include <String.h>
#include <StringView.h>
#include <TextControl.h>
#include <TranslatorRoster.h>
#include <TranslationDefs.h>

#include "Container.h"
#include "CellParser.h"

static const uint32 kMsgNew = 'anew';
static const uint32 kMsgOpen = 'aopn';
static const uint32 kMsgSaveAs = 'asva';
static const uint32 kMsgFormulaCommit = 'afml';
static const uint32 kMsgCut = 'acut';
static const uint32 kMsgCopy = 'acpy';
static const uint32 kMsgPaste = 'apst';
static const uint32 kMsgClear = 'aclr';

static const uint32 kAtomoNativeFormat = 'ASCD';

// Stessa logica di SheetView::ColumnName (vedi li' per il perche'
// della duplicazione: e' una manciata di righe, non vale la pena
// condividerla tramite un header dedicato).
static void ColumnName(int col, char* out)
{
	char buf[8];
	int n = 0;
	while (col > 0)
	{
		int rem = (col - 1) % 26;
		buf[n++] = 'A' + rem;
		col = (col - 1) / 26;
	}
	for (int i = 0; i < n; i++)
		out[i] = buf[n - 1 - i];
	out[n] = 0;
}

MainWindow::MainWindow()
	:
	BWindow(BRect(80, 80, 900, 700), "Atomo123", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE)
{
	fDoc = new CContainer(NULL, NULL);

	BMenuBar* menuBar = new BMenuBar("menu");
	BMenu* fileMenu = new BMenu("File");
	fileMenu->AddItem(new BMenuItem("Nuovo", new BMessage(kMsgNew), 'N'));
	fileMenu->AddItem(new BMenuItem("Apri" B_UTF8_ELLIPSIS, new BMessage(kMsgOpen), 'O'));
	fileMenu->AddItem(new BMenuItem("Salva con nome" B_UTF8_ELLIPSIS,
		new BMessage(kMsgSaveAs), 'S'));
	fileMenu->AddSeparatorItem();
	fileMenu->AddItem(new BMenuItem("Esci", new BMessage(B_QUIT_REQUESTED), 'Q'));
	menuBar->AddItem(fileMenu);

	// Taglia/copia/incolla passano dal vero BClipboard di sistema (non
	// da un appunti interno all'app): il contenuto della cella (la
	// formula, come mostrata dalla barra formule) diventa testo piano
	// condivisibile anche con altre applicazioni Haiku.
	BMenu* editMenu = new BMenu("Modifica");
	editMenu->AddItem(new BMenuItem("Taglia", new BMessage(kMsgCut), 'X'));
	editMenu->AddItem(new BMenuItem("Copia", new BMessage(kMsgCopy), 'C'));
	editMenu->AddItem(new BMenuItem("Incolla", new BMessage(kMsgPaste), 'V'));
	editMenu->AddSeparatorItem();
	editMenu->AddItem(new BMenuItem("Cancella", new BMessage(kMsgClear)));
	menuBar->AddItem(editMenu);

	fCellLabel = new BStringView("cellLabel", "A1");
	fCellLabel->SetExplicitMinSize(BSize(50, B_SIZE_UNSET));
	fCellLabel->SetExplicitMaxSize(BSize(50, B_SIZE_UNSET));
	fCellLabel->SetAlignment(B_ALIGN_CENTER);

	fFormulaBar = new BTextControl("formula", NULL, "", new BMessage(kMsgFormulaCommit));
	fFormulaBar->SetTarget(this);

	fSheetView = new SheetView(BRect(0, 0, 100, 100), fDoc);
	BScrollView* scroll = new BScrollView("scroll", fSheetView,
		B_FOLLOW_ALL, 0, true, true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(menuBar)
		.AddGroup(B_HORIZONTAL, 4)
			.SetInsets(4, 4, 4, 4)
			.Add(fCellLabel)
			.Add(fFormulaBar)
		.End()
		.Add(scroll);

	fOpenPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this));
	fSavePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this));
}

MainWindow::~MainWindow()
{
	delete fOpenPanel;
	delete fSavePanel;
	if (fDoc)
		fDoc->Release();
}

void MainWindow::NewDocument()
{
	CContainer* newDoc = new CContainer(NULL, NULL);
	if (fDoc)
		fDoc->Release();
	fDoc = newDoc;
	fSheetView->SetDocument(fDoc);
	fFormulaBar->SetText("");
}

void MainWindow::OpenFile(const entry_ref& ref)
{
	BFile file(&ref, B_READ_ONLY);
	if (file.InitCheck() != B_OK)
	{
		BAlert* alert = new BAlert("Errore",
			"Impossibile aprire il file selezionato.", "OK");
		alert->Go();
		return;
	}

	// BTranslatorRoster sceglie automaticamente il translator
	// installato adatto (CSV/XLS legacy/XLSX/ODS/ASCD nativo) in base
	// al contenuto reale del file, non all'estensione.
	BMallocIO ascd;
	status_t err = BTranslatorRoster::Default()->Translate(&file, NULL, NULL,
		&ascd, kAtomoNativeFormat);
	if (err != B_OK)
	{
		BAlert* alert = new BAlert("Errore",
			"Formato file non riconosciuto da nessun translator installato.",
			"OK");
		alert->Go();
		return;
	}

	CContainer* newDoc = new CContainer(NULL, NULL);
	ascd.Seek(0, SEEK_SET);
	err = LoadASCD(&ascd, newDoc);
	if (err != B_OK)
	{
		newDoc->Release();
		BAlert* alert = new BAlert("Errore",
			"Il file e' stato tradotto ma i dati risultanti non sono validi.",
			"OK");
		alert->Go();
		return;
	}

	if (fDoc)
		fDoc->Release();
	fDoc = newDoc;
	fSheetView->SetDocument(fDoc);
}

void MainWindow::SaveToFile(const entry_ref& dir, const char* name)
{
	BDirectory directory(&dir);
	BFile file(&directory, name, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
	{
		BAlert* alert = new BAlert("Errore", "Impossibile creare il file.", "OK");
		alert->Go();
		return;
	}

	status_t err = SaveASCD(fDoc, &file);
	if (err != B_OK)
	{
		BAlert* alert = new BAlert("Errore", "Scrittura del file fallita.", "OK");
		alert->Go();
	}
}

void MainWindow::CopySelection(bool cut)
{
	if (!fDoc)
		return;

	cell sel = fSheetView->Selection();
	char text[512];
	fDoc->GetCellFormula(sel, text, false);

	if (be_clipboard->Lock())
	{
		be_clipboard->Clear();
		BMessage* clip = be_clipboard->Data();
		if (clip)
			clip->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
		be_clipboard->Commit();
		be_clipboard->Unlock();
	}

	if (cut)
	{
		fDoc->DisposeCell(sel);
		fDoc->CalcCell(sel);
		fSheetView->Invalidate();
		SelectionChanged(sel);
	}
}

void MainWindow::PasteSelection()
{
	if (!fDoc)
		return;

	cell sel = fSheetView->Selection();

	if (!be_clipboard->Lock())
		return;

	BMessage* clip = be_clipboard->Data();
	const char* text = NULL;
	ssize_t len = 0;
	bool found = clip && clip->FindData("text/plain", B_MIME_TYPE,
		(const void**)&text, &len) == B_OK;

	if (found)
	{
		BString pasted(text, len);
		try
		{
			TryToParseString(pasted.String(), sel, fDoc, true);
		}
		catch (...)
		{
		}
		fDoc->CalcCell(sel);
		fSheetView->Invalidate();
		SelectionChanged(sel);
	}

	be_clipboard->Unlock();
}

void MainWindow::DeleteSelection()
{
	if (!fDoc)
		return;

	cell sel = fSheetView->Selection();
	fDoc->DisposeCell(sel);
	fSheetView->Invalidate();
	SelectionChanged(sel);
}

void MainWindow::CommitFormulaBar()
{
	if (!fDoc)
		return;

	cell sel = fSheetView->Selection();
	const char* text = fFormulaBar->Text();

	try
	{
		TryToParseString(text, sel, fDoc, true);
	}
	catch (...)
	{
	}
	fDoc->CalcCell(sel);
	fSheetView->Invalidate();
}

void MainWindow::SelectionChanged(cell c)
{
	char name[16];
	ColumnName(c.h, name);
	int len = strlen(name);
	snprintf(name + len, sizeof(name) - len, "%d", c.v);
	fCellLabel->SetText(name);

	char formula[512];
	if (fDoc)
		fDoc->GetCellFormula(c, formula, false);
	else
		formula[0] = 0;
	fFormulaBar->SetText(formula);
}

void MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgNew:
			NewDocument();
			break;

		case kMsgOpen:
			fOpenPanel->Show();
			break;

		case kMsgSaveAs:
			fSavePanel->Show();
			break;

		case B_REFS_RECEIVED:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				OpenFile(ref);
			break;
		}

		case B_SAVE_REQUESTED:
		{
			entry_ref dir;
			BString name;
			if (message->FindRef("directory", &dir) == B_OK
				&& message->FindString("name", &name) == B_OK)
				SaveToFile(dir, name.String());
			break;
		}

		case kMsgFormulaCommit:
			CommitFormulaBar();
			break;

		case kMsgCut:
			CopySelection(true);
			break;

		case kMsgCopy:
			CopySelection(false);
			break;

		case kMsgPaste:
			PasteSelection();
			break;

		case kMsgClear:
			DeleteSelection();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

bool MainWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}
