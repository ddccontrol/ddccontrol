/***************************************************************************
 *   Copyright (C) 2004-2005 by Nicolas Boichat                            *
 *   nicolas@boichat.ch                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#include "config.h"
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include <unistd.h>

#define IDLE_TIMEOUT 60

#include "notebook.h"
#include "internal.h"

#include "daemon/dbus_client.h"

GtkWidget* table;

GtkWidget *combo_box;
GtkWidget* refresh_monitors_button;

GtkWidget *messagebox = NULL;
GtkWidget *messagelabel = NULL;
GtkWidget *messagebutton = NULL;
GtkWidget *statuslabel = NULL;

GtkWidget* close_button = NULL;

GtkWidget* choice_hbox = NULL;
GtkWidget* profile_hbox = NULL;
GtkWidget* bottom_hbox = NULL;

GtkWidget* main_app_window = NULL;

GtkWidget* monitor_manager = NULL;
GtkWidget* profile_manager = NULL;

GtkWidget* profile_manager_button = NULL;
GtkWidget* saveprofile_button = NULL;
GtkWidget* cancelprofile_button = NULL;
GtkWidget* refresh_controls_button = NULL;

struct monitor* mon;

int current_monitor; /* current monitor */
int num_monitor; /* total number of monitors */

struct monitorlist* monlist;

gulong combo_box_changed_handler_id = 0;

int mainrow = 0; /* Main center row in the table widget */

/* Indicate what is now displayed at the center of the main window:
 *  0 - Monitor manager
 *  1 - Profile manager
 */
int current_main_component = 0;

/* Current monitor id, and next one. */
int currentid = -1;
int nextid = -1;

static GMutex* combo_change_mutex;

DDCControl *ddccontrol_proxy;

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
    return FALSE;
}

static void destroy( GtkWidget *widget,
                     gpointer   data )
{
    gtk_main_quit ();
}

static void loadprofile_callback(GtkWidget *widget, gpointer data)
{
	set_current_main_component(1);
}

static void combo_change(GtkWidget *widget, gpointer data)
{
	nextid = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	if (!g_mutex_trylock(combo_change_mutex)) {
		if (get_verbosity())
			printf("Tried to change to %d, but mutex locked.\n", gtk_combo_box_get_active(GTK_COMBO_BOX(widget)));
		return;
	}
	
	gtk_widget_set_sensitive(widget, FALSE);
	
	while (currentid != nextid) {
		if (get_verbosity())
			printf("currentid(%d) != nextid(%d), trying...\n", currentid, nextid);
		currentid = nextid;
		int i = 0;
	
		if (monitor_manager != NULL) {
			delete_monitor_manager();
			gtk_widget_destroy(monitor_manager);
			monitor_manager = NULL;
		}
	
		char buffer[256];
		
		struct monitorlist* current;
		
		for (current = monlist;; current = current->next)
		{
			g_assert(current != NULL);
			if (i == currentid)
			{
				snprintf(buffer, 256, "%s: %s", current->filename, current->name);
				create_monitor_manager(current);
				if (monitor_manager) {
					gtk_widget_set_sensitive(refresh_controls_button, TRUE);
				}
				else {
					gtk_widget_set_sensitive(close_button, TRUE);
					gtk_widget_set_sensitive(choice_hbox, TRUE);
				}
				break;
			}
			i++;
		}
		
		if (monitor_manager) {
			gtk_table_attach(GTK_TABLE(table), monitor_manager, 0, 1, mainrow, mainrow+1, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 5, 5);
			gtk_table_attach(GTK_TABLE(table), profile_manager, 0, 1, mainrow, mainrow+1, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 5, 5);
		}
		
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}
	
	if (get_verbosity())
		printf("currentid == nextid (%d)\n", currentid);
	
	gtk_widget_set_sensitive(widget, TRUE);
	g_mutex_unlock(combo_change_mutex);
}

static gboolean window_changed(GtkWidget *widget, 
				GdkEventConfigure *event,
				gpointer user_data)
{
	if (num_monitor == 2) { // If there are more than two monitors, we need a configuration menu
		int i;
		struct monitorlist* current;
		
		if (monlist == NULL) {
			if (get_verbosity())
				printf("monlist == NULL\n");
			return FALSE;
		}
		
		i = 0;
		for (current = monlist; current != NULL; current = current->next) i++;
		
		if (i != 2) { // Every monitor does not support DDC/CI, or there are more monitors plugged-in
			//printf("i : %d\n", i);
			return FALSE;
		}
		
		i = gdk_screen_get_monitor_at_window(gdk_screen_get_default(), gtk_widget_get_window(main_app_window));
		
		if (i != current_monitor) {
			int k = nextid;
			if (get_verbosity())
				printf(_("Monitor changed (%d %d).\n"), i, k);
			k = (k == 0) ? 1 : 0;
			current_monitor = i;
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), k);
		}
		
		//g_warning(_("Could not find the current monitor in Xinerama monitors list."));
	}
	
	return FALSE;
}

/* Enable or disable widgets (monitor choice, close/refresh button) */
static void widgets_set_sensitive(gboolean sensitive)
{
	gtk_widget_set_sensitive(choice_hbox, sensitive);
	gtk_widget_set_sensitive(refresh_controls_button, sensitive && (current_main_component == 0));
	gtk_widget_set_sensitive(close_button, sensitive);
	gtk_widget_set_sensitive(profile_manager_button, sensitive && (current_main_component == 0));
	gtk_widget_set_sensitive(saveprofile_button, sensitive);
	gtk_widget_set_sensitive(cancelprofile_button, sensitive);
}

void set_current_main_component(int component) {
	g_assert((component == 0) || (component == 1));
	
	current_main_component = component;
	
	if (!monitor_manager)
		return;
	
	if (current_main_component == 0) {
		gtk_widget_show(monitor_manager);
		gtk_widget_hide(profile_manager);
		gtk_widget_set_sensitive(refresh_controls_button, TRUE);
		gtk_widget_set_sensitive(profile_manager_button, TRUE);
	}
	else if (current_main_component == 1) {
		gtk_widget_hide(monitor_manager);
		gtk_widget_show(profile_manager);
		gtk_widget_set_sensitive(refresh_controls_button, FALSE);
		gtk_widget_set_sensitive(profile_manager_button, FALSE);
	}
}

/* Set the status message */
void set_status(char* message) {
	gtk_label_set_text(GTK_LABEL(statuslabel), message);
}

static void messagebutton_callback(GtkWidget *widget, gpointer data)
{
	set_message("");
}

void set_message(char* message) {
	set_message_ok(message, 0);
}

/* Show a message on top of every other controls, with a ok button
 * if the user can hide it himself. */
void set_message_ok(char* message, int with_ok)
{
	gtk_widget_show(messagebutton);
	if (!message[0]) {
		gtk_widget_hide(messagebox);
		if (monitor_manager) {
			set_current_main_component(current_main_component);
		}
		
		widgets_set_sensitive(TRUE);
		
		return;
	}
	
	gtk_label_set_markup(GTK_LABEL(messagelabel), message);
	gtk_label_set_selectable(GTK_LABEL(messagelabel), TRUE);

	widgets_set_sensitive(FALSE);
	
	gtk_widget_show(messagebox);
	if (with_ok) {
		gtk_widget_show(messagebutton);
	}
	else {
		gtk_widget_hide(messagebutton);
	}
	
	if (monitor_manager) {
		gtk_widget_hide(monitor_manager);
		gtk_widget_hide(profile_manager);
	}
	
	while (gtk_events_pending())
		gtk_main_iteration();
}

static gboolean heartbeat(gpointer data)
{
	ddcpci_send_heartbeat();
	return TRUE;
}

/* Create a new button with an image and a label packed into it
 * and return the button. */
GtkWidget *stock_label_button(const gchar * stockid, const gchar *label_text, const gchar *tool_tip)
{
	GtkWidget *box;
	GtkWidget *label = NULL;
	GtkWidget *image;
	GtkWidget *button;
	
	button = gtk_button_new();
	
	/* Create box for image and label */
	box = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER (box), 1);
	
	/* Now on to the image stuff */
	image = gtk_image_new_from_stock(stockid, GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 3);
	gtk_widget_show(image);

	if (label_text) {
		/* Create a label for the button */
		label = gtk_label_new(label_text);
		
		/* Pack the image and label into the box */
		gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 3);
		
		gtk_widget_show(label);
	}
	
	gtk_widget_show(box);
	
	gtk_container_add (GTK_CONTAINER(button), box);
	
	g_object_set_data(G_OBJECT(button), "button_label", label);
	
	if (tool_tip) {
		gtk_widget_set_tooltip_text(GTK_WIDGET(button), tool_tip);
	}
	
	return button;
}

static void probe_monitors(GtkWidget *widget, gpointer data) {
	if (combo_box_changed_handler_id)
		g_signal_handler_disconnect(G_OBJECT(combo_box), combo_box_changed_handler_id);
	
	gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combo_box))));
	
	set_message(_("Probing for available monitors..."));
	// TODO: rescan on button, initial get only
	monlist = ddcci_dbus_rescan_monitors(ddccontrol_proxy);

	struct monitorlist* current;
	
	char buffer[256];
	
	for (current = monlist; current != NULL; current = current->next)
	{
		snprintf(buffer, 256, "%s: %s",
			current->filename, current->name);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_box), buffer);
	}
	
	combo_box_changed_handler_id = g_signal_connect(G_OBJECT(combo_box), "changed", G_CALLBACK(combo_change), NULL);
	
	currentid = -1;
	nextid = -1;
	
	if (monlist) {
		widgets_set_sensitive(TRUE);
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);
	}
	else {
		widgets_set_sensitive(FALSE);
		gtk_widget_set_sensitive(close_button, TRUE);
		gtk_widget_set_sensitive(refresh_controls_button, TRUE);
		set_message(_(
			"No monitor supporting DDC/CI available.\n\n"
			"If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver)."
			   ));
	}
}

int main( int argc, char *argv[] )
{ 
	int i, verbosity = 0;
	
	mon = NULL;
	monitor_manager = NULL;
	
#ifdef HAVE_GETTEXT
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	bindtextdomain("ddccontrol-db", LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	bind_textdomain_codeset("ddccontrol-db", "UTF-8");
	textdomain(PACKAGE);
#endif

	while ((i=getopt(argc,argv,"v")) >= 0)
	{
		switch(i) {
		case 'v':
			verbosity++;
			break;
		}
	}
	
	ddcci_verbosity(verbosity);

	ddccontrol_proxy = ddcci_dbus_open_proxy();
	if(ddccontrol_proxy == NULL)
		return 1;

	gtk_init(&argc, &argv);
	
	g_thread_init(NULL);
	combo_change_mutex = g_mutex_new();
	
	/* Full screen patterns test */
	/*create_fullscreen_patterns_window();
	show_pattern();
	gtk_main();*/
	
	if (!ddcci_init(NULL)) {
		printf(_("Unable to initialize ddcci library.\n"));
		GtkWidget* dialog = gtk_message_dialog_new(
				GTK_WINDOW(main_app_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Unable to initialize ddcci library, see console for more details.\n"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return 1;
	}
	
	g_timeout_add( IDLE_TIMEOUT*1000, heartbeat, NULL );
	
	gtk_window_set_default_icon_name ("gddccontrol");

	main_app_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	
	gtk_window_set_title(GTK_WINDOW(main_app_window),_("Monitor settings"));
	
	gtk_window_set_default_size(GTK_WINDOW(main_app_window), 500, 500);

	gtk_window_set_position (GTK_WINDOW (main_app_window), GTK_WIN_POS_CENTER);
	
	g_signal_connect (G_OBJECT (main_app_window), "delete_event",
				G_CALLBACK (delete_event), NULL);
	
	g_signal_connect (G_OBJECT (main_app_window), "destroy",
				G_CALLBACK (destroy), NULL);

	g_signal_connect (G_OBJECT (main_app_window), "configure-event",
				G_CALLBACK (window_changed), NULL);
	
	gtk_container_set_border_width (GTK_CONTAINER (main_app_window), 4);
	
	table = gtk_table_new(5, 1, FALSE);
	gtk_widget_show (table);
	int crow = 0; /* Current row */
	
	/* Monitor choice combo box */
	choice_hbox = gtk_hbox_new(FALSE, 10);
	
	GtkWidget* label = gtk_label_new(_("Current monitor: "));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(choice_hbox),label, 0, 0, 0);
	
	combo_box = gtk_combo_box_text_new();
	
	gtk_widget_show(combo_box);

	gtk_box_pack_start(GTK_BOX(choice_hbox),combo_box, 1, 1, 0);
	
	refresh_monitors_button = stock_label_button(GTK_STOCK_REFRESH, NULL, _("Refresh monitor list"));
	g_signal_connect(G_OBJECT(refresh_monitors_button), "clicked", G_CALLBACK(probe_monitors), NULL);
	gtk_widget_show(refresh_monitors_button);
	gtk_box_pack_start(GTK_BOX(choice_hbox),refresh_monitors_button, 0, 0, 0);
	
	gtk_table_attach(GTK_TABLE(table), choice_hbox, 0, 1, crow, crow+1, GTK_FILL_EXPAND, 0, 5, 5);
	crow++;
	gtk_widget_show(choice_hbox);
	
	GtkWidget* hsep = gtk_hseparator_new();
	gtk_widget_show (hsep);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 1, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 0, 0);
	crow++;
	
	/* Toolbar (profile...) */
	profile_hbox = gtk_hbox_new(FALSE, 10);
	
	profile_manager_button = stock_label_button(GTK_STOCK_OPEN, _("Profile manager"), NULL);
	g_signal_connect(G_OBJECT(profile_manager_button), "clicked", G_CALLBACK(loadprofile_callback), NULL);

	gtk_box_pack_start(GTK_BOX(profile_hbox), profile_manager_button, 0, 0, 0);
	gtk_widget_show (profile_manager_button);
	gtk_widget_set_sensitive(profile_manager_button, FALSE);
	
	saveprofile_button = stock_label_button(GTK_STOCK_SAVE, _("Save profile"), NULL);
	g_signal_connect(G_OBJECT(saveprofile_button), "clicked", G_CALLBACK(saveprofile_callback), NULL);

	gtk_box_pack_start(GTK_BOX(profile_hbox), saveprofile_button, 0, 0, 0);
	gtk_widget_set_sensitive(saveprofile_button, FALSE);
	
	cancelprofile_button = stock_label_button(GTK_STOCK_SAVE, _("Cancel profile creation"), NULL);
	g_signal_connect(G_OBJECT(cancelprofile_button), "clicked", G_CALLBACK(cancelprofile_callback), NULL);

	gtk_box_pack_start(GTK_BOX(profile_hbox), cancelprofile_button, 0, 0, 0);
	gtk_widget_set_sensitive(cancelprofile_button, FALSE);
	
	gtk_widget_show (profile_hbox);
	gtk_table_attach(GTK_TABLE(table), profile_hbox, 0, 1, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 8, 8);
	crow++;
	
	hsep = gtk_hseparator_new();
	gtk_widget_show (hsep);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 1, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 0, 0);
	crow++;
	
	/* Status message label (used when loading or refreshing, or when displaying some kind of warnings) */
	messagebox = gtk_vbox_new(FALSE, 10);
	
	messagelabel = gtk_label_new ("");
	gtk_label_set_line_wrap(GTK_LABEL(messagelabel), TRUE);
	gtk_box_pack_start(GTK_BOX(messagebox), messagelabel, 1, 1, 0);
	gtk_widget_show(messagelabel);
	
	GtkWidget* messagealign = gtk_alignment_new(0.5, 0.5, 0, 0);
	messagebutton = stock_label_button(GTK_STOCK_OK, _("OK"), NULL);
	g_signal_connect(G_OBJECT(messagebutton), "clicked", G_CALLBACK(messagebutton_callback), NULL);
	gtk_container_add(GTK_CONTAINER(messagealign), messagebutton);
	gtk_widget_show(messagebutton);
	
	gtk_box_pack_start(GTK_BOX(messagebox), messagealign, 0, 0, 10);
	gtk_widget_show(messagealign);
	
	gtk_table_attach(GTK_TABLE(table), messagebox, 0, 1, crow, crow+1, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 5, 5);
	mainrow = crow;
	crow++;
	
	hsep = gtk_hseparator_new();
	gtk_widget_show (hsep);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 1, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 0, 0);
	crow++;
	
	/* Refresh and close buttons */
	bottom_hbox = gtk_hbox_new(FALSE, 10);
	
	statuslabel = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(statuslabel), TRUE);
	gtk_box_pack_start(GTK_BOX(bottom_hbox),statuslabel, TRUE, TRUE, 0);
	gtk_widget_show(statuslabel);
	
	GtkWidget* align = gtk_alignment_new(1, 0.5, 0, 0);
	GtkWidget* br_hbox = gtk_hbox_new(FALSE, 30);
	
	refresh_controls_button = stock_label_button(GTK_STOCK_REFRESH, _("Refresh"), _("Refresh all controls"));
	g_signal_connect(G_OBJECT(refresh_controls_button),"clicked",G_CALLBACK (refresh_all_controls), NULL);

	gtk_box_pack_start(GTK_BOX(br_hbox),refresh_controls_button,0,0,0);
	gtk_widget_show (refresh_controls_button);
	gtk_widget_set_sensitive(refresh_controls_button, FALSE);
	
	close_button = stock_label_button(GTK_STOCK_CLOSE, _("Close"), NULL);
	g_signal_connect(G_OBJECT(close_button),"clicked",G_CALLBACK (destroy), NULL);

	gtk_box_pack_start(GTK_BOX(br_hbox),close_button,0,0,0);
	gtk_widget_show (close_button);
	
	gtk_container_add(GTK_CONTAINER(align),br_hbox);
	gtk_widget_show (br_hbox);
	gtk_widget_show (align);
	
	gtk_box_pack_start(GTK_BOX(bottom_hbox),align, TRUE, TRUE,0);
	gtk_widget_show (bottom_hbox);
	gtk_table_attach(GTK_TABLE(table), bottom_hbox, 0, 1, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 8, 8);
	crow++;
	
	gtk_container_add (GTK_CONTAINER(main_app_window), table);
	
	gtk_widget_show(main_app_window);
	
	GdkScreen* screen = gdk_screen_get_default();
	
	num_monitor = gdk_screen_get_n_monitors(screen);
	
	/*for (i = 0; i < nscreen; i++) {
		GdkRectangle dest;
		
		gdk_screen_get_monitor_geometry(screen, i, &dest);
		printf("%d: %d, %d %dx%d\n", nscreen, dest.x, dest.y, dest.width, dest.height);
	}*/
	
	current_monitor = gdk_screen_get_monitor_at_window(screen, gtk_widget_get_window(main_app_window));
	
	probe_monitors(NULL, NULL);
	
/*	moninfo = gtk_label_new ();
	gtk_misc_set_alignment(GTK_MISC(moninfo), 0, 0);
	
	gtk_table_attach(GTK_TABLE(table), moninfo, 0, 1, 1, 2, GTK_FILL_EXPAND, 0, 5, 5);*/
	
	gchar* tmp = g_strdup_printf(_("Welcome to gddccontrol %s."), VERSION);
	set_status(tmp);
	g_free(tmp);
	
	gtk_main();
	
	delete_monitor_manager();
	
	ddcci_free_list(monlist);
	
	ddcci_release();
	
	return 0;
}
