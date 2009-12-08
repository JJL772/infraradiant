#include "Dialog.h"

#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkstock.h>

#include "itextstream.h"

namespace gtkutil
{

Dialog::Dialog(const std::string& title, GtkWindow* parent) :
	BlockingTransientWindow(title, parent),
	_result(RESULT_CANCELLED),
	_vbox(gtk_vbox_new(FALSE, 6)),
	_constructed(false)
{
	gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);
	gtk_window_set_type_hint(GTK_WINDOW(getWindow()), GDK_WINDOW_TYPE_HINT_DIALOG);

	gtk_container_add(GTK_CONTAINER(getWindow()), _vbox);
}

void Dialog::setTitle(const std::string& title)
{
	// Dispatch this call to the base class
	BlockingTransientWindow::setTitle(title);
}

ui::IDialog::Result Dialog::run()
{
	if (!_constructed)
	{
		_constructed = true;

		// Call the virtual method, gives subclasses a chance to do their stuff
		construct();
	}

	// Show the dialog (enters gtk_main() and blocks)
	show();

	return _result;
}

void Dialog::construct()
{
	// Pack the buttons
	gtk_box_pack_end(GTK_BOX(_vbox), createButtons(), FALSE, FALSE, 0);
}

GtkWidget* Dialog::createButtons()
{
	GtkWidget* buttonHBox = gtk_hbox_new(FALSE, 6);

	// Pack the button hbox into the window
	gtk_box_pack_end(GTK_BOX(_vbox), buttonHBox, FALSE, FALSE, 0);

	GtkWidget* okButton = gtk_button_new_from_stock(GTK_STOCK_OK);
	g_signal_connect(G_OBJECT(okButton), "clicked", G_CALLBACK(onOK), this);
	gtk_box_pack_end(GTK_BOX(buttonHBox), okButton, FALSE, FALSE, 0);

	GtkWidget* cancelButton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect(G_OBJECT(cancelButton), "clicked", G_CALLBACK(onCancel), this);
	gtk_box_pack_end(GTK_BOX(buttonHBox), cancelButton, FALSE, FALSE, 0);

	return buttonHBox;
}

void Dialog::onCancel(GtkWidget* widget, Dialog* self)
{
	self->_result = ui::IDialog::RESULT_CANCELLED;
	self->destroy(); // breaks gtk_main()
}

void Dialog::onOK(GtkWidget* widget, Dialog* self)
{
	self->_result = ui::IDialog::RESULT_OK;
	self->destroy(); // breaks gtk_main()
}

} // namespace gtkutil
