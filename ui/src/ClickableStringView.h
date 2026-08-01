/*
	ClickableStringView.h

	Un BStringView che invia un messaggio al click e mostra il cursore
	"link" al passaggio del mouse -- usato per il link al progetto
	nella finestra Informazioni (vedi AboutWindow.h). Stesso identico
	componente gia' usato in Brube2000 (altro progetto nativo Haiku
	dello stesso autore), riportato qui pari pari su richiesta esplicita
	per coerenza visiva fra le due app.
*/

#ifndef CLICKABLE_STRING_VIEW_H
#define CLICKABLE_STRING_VIEW_H

#include <StringView.h>

class ClickableStringView : public BStringView {
public:
	ClickableStringView(const char* name, const char* text);
	virtual ~ClickableStringView();

	virtual void AttachedToWindow();
	virtual void MouseDown(BPoint where);
	virtual void MouseMoved(BPoint where, uint32 code, const BMessage* message);

	void SetClickMessage(BMessage* message);

private:
	BMessage* fClickMessage;
};

#endif
