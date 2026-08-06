/*
	ValidationWindow.cpp

	Vedi ValidationWindow.h.
*/

#include "ValidationWindow.h"

#include <Button.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TextControl.h>

#include <cstdio>
#include <cstdlib>

static const uint32 kMsgSaveLocal = 'svlc';
static const uint32 kMsgRemoveLocal = 'rmlc';

// Corrispondenza posizionale con ValidationType in Container.h
// (0=eNoValidation, 1=eListValidation, 2=eNumberRangeValidation) --
// stesso principio gia' usato per ChartType/ChartWindow.
ValidationWindow::ValidationWindow(BMessenger target)
	:
	BWindow(BRect(180, 180, 480, 330), "Convalida dati",
		B_FLOATING_WINDOW_LOOK, B_FLOATING_APP_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS
			| B_ASYNCHRONOUS_CONTROLS),
	fTarget(target),
	fRow(-1),
	fCol(-1)
{
	BPopUpMenu* typeMenu = new BPopUpMenu("typeMenu");
	typeMenu->AddItem(new BMenuItem("Nessuna", NULL));
	typeMenu->AddItem(new BMenuItem("Elenco di valori", NULL));
	typeMenu->AddItem(new BMenuItem("Intervallo numerico", NULL));
	typeMenu->ItemAt(0)->SetMarked(true);
	fTypeField = new BMenuField("type", "Tipo:", typeMenu);

	fListField = new BTextControl("list", "Valori (separati da virgola):", "", NULL);
	fMinField = new BTextControl("min", "Minimo:", "", NULL);
	fMaxField = new BTextControl("max", "Massimo:", "", NULL);

	BButton* removeButton = new BButton("remove", "Rimuovi convalida",
		new BMessage(kMsgRemoveLocal));
	removeButton->SetTarget(this);

	BButton* saveButton = new BButton("save", "Salva", new BMessage(kMsgSaveLocal));
	saveButton->SetTarget(this);
	saveButton->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, 8)
		.SetInsets(8, 8, 8, 8)
		.Add(fTypeField)
		.Add(fListField)
		.AddGroup(B_HORIZONTAL)
			.Add(fMinField)
			.Add(fMaxField)
		.End()
		.AddGroup(B_HORIZONTAL)
			.Add(removeButton)
			.AddGlue()
			.Add(saveButton)
		.End();

	fListField->MakeFocus(true);
}

int ValidationWindow::SelectedType() const
{
	return fTypeField->Menu()->IndexOf(fTypeField->Menu()->FindMarked());
}

void ValidationWindow::SetCell(int row, int col, int type, const char* list,
	double min, double max)
{
	fRow = row;
	fCol = col;

	if (type < 0 || type > 2)
		type = 0;
	fTypeField->Menu()->ItemAt(type)->SetMarked(true);

	fListField->SetText(list ? list : "");

	char buf[64];
	snprintf(buf, sizeof(buf), "%g", min);
	fMinField->SetText(buf);
	snprintf(buf, sizeof(buf), "%g", max);
	fMaxField->SetText(buf);
}

void ValidationWindow::MessageReceived(BMessage* message)
{
	switch (message->what)
	{
		case kMsgSaveLocal:
		{
			// Niente riga/colonna nel messaggio: a differenza di
			// commento/collegamento (un'annotazione su UNA cella
			// precisa), la convalida dati si applica a tutto
			// SelectionRange() come il vero "Convalida dati" di Excel
			// -- MainWindow legge la selezione corrente da se stessa
			// (vedi ApplyValidationToSelection). fRow/fCol restano solo
			// per sapere QUALE cella ha fornito i valori di
			// precompilazione mostrati (vedi SetCell), non per
			// l'applicazione.
			BMessage request(kMsgValidationCommit);
			request.AddInt32("type", SelectedType());
			request.AddString("list", fListField->Text());
			request.AddDouble("min", atof(fMinField->Text()));
			request.AddDouble("max", atof(fMaxField->Text()));
			fTarget.SendMessage(&request);
			Hide();
			return;
		}

		case kMsgRemoveLocal:
		{
			BMessage request(kMsgValidationRemove);
			fTarget.SendMessage(&request);
			Hide();
			return;
		}
	}

	BWindow::MessageReceived(message);
}

bool ValidationWindow::QuitRequested()
{
	// Stessa regola di GoToWindow/CommentWindow: resta nascosta e
	// riusabile.
	Hide();
	return false;
}
