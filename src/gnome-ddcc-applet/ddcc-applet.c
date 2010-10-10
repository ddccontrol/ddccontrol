/* gnome-ddcc-applet
 * Copyright (C) 2005 Christian Schilling
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */


/* ****************
 * Includes
 * ****************/

/* config */
#include "config.h"

/* libddccontrol */
#include "ddcci.h"
#include "conf.h"

/* libc */
#include <string.h>
#include <stdlib.h>

/* GNOME */
#include <panel-applet.h>
#include <gtk/gtk.h>
#include <gtk/gtklabel.h>

/* private */
#include "ddcc-applet.h"

/* libddccontrol ipc */
#include "ddcpci-ipc.h"

#define GETTEXT_PACKAGE PACKAGE
#define GNOMELOCALEDIR LOCALEDIR

/* ****************
 * Constants
 * ****************/

/* Popup menu on the applet */
static const BonoboUIVerb ddccapplet_applet_menu_verbs[] = 
{
        BONOBO_UI_UNSAFE_VERB ("DdccAppletProperties", menu_properties_cb),
        BONOBO_UI_UNSAFE_VERB ("DdccAppletRunGddcontrol", menu_rungdcc_cb),
        BONOBO_UI_UNSAFE_VERB ("DdccAppletAbout", menu_about_cb),
        BONOBO_UI_VERB_END
};


/* ****************
 * Helpers
 * ****************/

/* display [msg] as popup errormessage */
static void
error_msg (char *msg)
{
	GtkWidget* dialog = 
		gtk_message_dialog_new (NULL,
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_ERROR,
					GTK_BUTTONS_CLOSE,
					msg);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/* makes the menu appear next to the widget [user_data]
 * instead off right under the cursor */
static void
position_menu (	GtkMenu *menu, gint *x, gint *y,
	       	gboolean *push_in, gpointer user_data)
{
	GtkWidget *widget = GTK_WIDGET (user_data);
	GdkScreen *screen;
	gint twidth, theight, tx, ty;
	GtkTextDirection direction;
	GdkRectangle monitor;
	gint monitor_num;
	
	g_return_if_fail (menu != NULL);
	g_return_if_fail (x != NULL);
	g_return_if_fail (y != NULL);
	
	if (push_in) *push_in = FALSE;
	
	direction = gtk_widget_get_direction (widget);
	
	twidth = GTK_WIDGET (menu)->requisition.width;
	theight = GTK_WIDGET (menu)->requisition.height;
	
	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	monitor_num = gdk_screen_get_monitor_at_window (screen, widget->window);
	if (monitor_num < 0) monitor_num = 0;
	gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);
	
	if (!gdk_window_get_origin (widget->window, &tx, &ty)) {
		g_warning ("Menu not on screen");
		return;
	}
	
	tx += widget->allocation.x;
	ty += widget->allocation.y;
	
	if (direction == GTK_TEXT_DIR_RTL)
		tx += widget->allocation.width - twidth;
	
	if ((ty + widget->allocation.height + theight) <=
			monitor.y + monitor.height)
		ty += widget->allocation.height;
	else if ((ty - theight) >= monitor.y)
		ty -= theight;
	else if (monitor.y + monitor.height -
			(ty + widget->allocation.height) > ty)
		ty += widget->allocation.height;
	else
		ty -= theight;
	
	*x = CLAMP (tx, monitor.x,
			MAX (monitor.x, monitor.x + monitor.width - twidth));
	*y = ty;
	gtk_menu_set_monitor (menu, monitor_num);
}

/* sends heartbeat message to ddcpci, so it does not time out */
static gboolean
heartbeat (gpointer data)
{
	ddcpci_send_heartbeat();
	return TRUE;
}


/* ****************
 * Callbacks
 * ****************/

/* called when the user selects the "Properties..." entry from the menu
 * shows (not creates!) the properties dialog */
static void
menu_properties_cb (BonoboUIComponent *uic,
		DdccApplet *applet,
	       	const gchar *verbname)
{
	gtk_widget_show(applet->w_properties_dialog);
}

/* runs the gddccontrol programm */
static void
menu_rungdcc_cb (BonoboUIComponent *uic,
		DdccApplet *applet,
	       	const gchar *verbname)
{
	system("gddccontrol &");
}

/* shows the about dialog, when the user selects the coresponding
 * menu entry */
static void
menu_about_cb (BonoboUIComponent *uic,
		DdccApplet *applet,
	       	const gchar *verbname)
{
	static const gchar *authors[] = {
		"Christian Schilling <cschilling@gmx.de>",
		NULL
	};
	
	gtk_show_about_dialog (NULL,
			"name", _("Monitor Profile Switcher"),
			"version", VERSION,
			"copyright", "\xC2\xA9 2005 Christian Schilling",
			"comments",
			_(
				"An applet for quick switching of"
				" monitor profiles.\n"
				"Based on libddccontrol"
				" and part of the ddccontrol project.\n"
				"(http://ddccontrol.sourceforge.net)"
			 ),
			"authors", authors,
			NULL);
}

/* called when the user klicks on the applet */
static gboolean
applet_button_cb (GtkWidget	*widget,
		GdkEventButton	*event,
		DdccApplet	*applet)
{
	if (event->button == 1)
	{
		if (applet->error)
			ddcc_applet_init(applet);
		if (applet->error)
			return FALSE;
		
		if (applet->w_profiles_menu)
		{
			gtk_menu_popup (GTK_MENU (applet->w_profiles_menu),
					NULL, NULL,
					position_menu, applet->w_applet,
					event->button, event->time);
		}
		return TRUE;
	}
	return FALSE;
}

/* called when the user chooses a profile from the profiles menu */
static gboolean
change_profile_cb (GtkMenuItem*	item,
		  DdccApplet*	applet)
{
	struct profile* profile = NULL;
	
	profile = g_object_get_data (G_OBJECT (item), "ddcc_profile");
	ddcci_apply_profile (profile, applet->monitor);

	applet->profile = profile;
		
	gtk_label_set_label (GTK_LABEL (applet->w_label), (gchar*)profile->name);

	return FALSE;
}

/* called when properties window should be closed
 * in fact, it prevents it from being closed and just hides it */
static gboolean
dialog_delete_cb (GtkWidget *widget, GdkEvent *event, DdccApplet* applet)
{
	gtk_widget_hide(applet->w_properties_dialog);

	return TRUE;
}

/* does the same as above function */
static gboolean
dialog_close_button_cb (GtkWidget *widget, DdccApplet* applet)
{
	gtk_widget_hide(applet->w_properties_dialog);

	return TRUE;
}

/* called when the user selects a different monitor, causes some
 * reinitialisation */
static void
monitor_combo_cb (GtkComboBox* widget, DdccApplet* applet)
{
	char buffer[256];

	strncpy (buffer, gtk_combo_box_get_active_text (widget), 256);
	
	/* FIXME: filename never contains spaces?? */
	memset(applet->monitor_name,0,256);
	strncpy (applet->monitor_name, buffer, strcspn(buffer, " ")-1);

	if(applet->error < ERR_DDCCI_OPEN) {
		ddcci_close(applet->monitor);
		applet->error = ERR_DDCCI_OPEN;
	}
}


/* ****************
 * Cleanup
 * ****************/

/* called when the main widget is destroyed
 * (user removes applet from pannel) */
static void
destroy_cb (GtkObject* object, DdccApplet* applet)
{
	ddcci_close (applet->monitor);
	ddcci_release ();
	if (applet->monitor)
		free (applet->monitor);
	g_free (applet);
}


/* ****************
 * Initializers
 * ****************/

/* returns a GtkMenu containing all available profiles for
 * monitor [mon] */
static int
fill_profiles_menu (DdccApplet *applet)
{
	struct profile* profile;
	GtkWidget* item;
			
	if (!ddcci_get_all_profiles (applet->monitor))
		return 0;

	for (profile=applet->monitor->profiles;profile;profile=profile->next) {
		item = gtk_menu_item_new_with_label ((gchar*)profile->name);
		g_object_set_data (G_OBJECT (item), "ddcc_profile", profile);
		g_signal_connect (item, "activate",
				 G_CALLBACK (change_profile_cb), applet);
		gtk_widget_show (item);
		gtk_container_add (GTK_CONTAINER (applet->w_profiles_menu),
				item);
	}

	return 1;
}

/* fills [applet->w_monitor] with entrys for each monitor */
static int
fill_monitor_combo (DdccApplet *applet)
{
	struct monitorlist* monlist;
	struct monitorlist* current;
	char buffer[256];
	
	if (!(monlist = ddcci_load_list ()))
		return 0;
	
	gtk_list_store_clear ( GTK_LIST_STORE ( gtk_combo_box_get_model (
			GTK_COMBO_BOX (applet->w_properties_monitor))));
	
	for (current = monlist; current; current = current->next) {
		snprintf (buffer,
			256,
			"%s: %s",
			current->filename,
			current->name);
		gtk_combo_box_append_text (GTK_COMBO_BOX (
						applet->w_properties_monitor),
					   buffer);
	}

	gtk_combo_box_set_active (
			GTK_COMBO_BOX (applet->w_properties_monitor), 0);

	ddcci_free_list (monlist);

	return 1;
}

/* initializes the ddccontrol library, if initialisation is already
 * partly done it's just completed */
static void
ddcc_applet_init (DdccApplet* applet)
{
#	define START_INIT()					\
		switch (applet->error) {			\
			case ERR_NO_INIT:			\

#	define TRY_INIT(call_func,err_val,err_msg)		\
		case err_val: 					\
			if (call_func) {			\
				error_msg (err_msg);		\
					applet->error = err_val;\
						break;		\
			}

#	define FINISH_INIT()					\
		applet->error = ERR_OK;				\
		case ERR_OK:

#	define END_INIT()					\
		return; }

	START_INIT()

	TRY_INIT (!ddcci_init (NULL),
		ERR_DDCCI_INIT,
		_("Unable to initialize ddcci library"))
		
	TRY_INIT (!fill_monitor_combo (applet),
		ERR_FILL_MONITOR_COMBO,
		_("No monitor configuration found."
		 " Please run gddccontrol first"))
	
	TRY_INIT (ddcci_open (applet->monitor, applet->monitor_name , 0) < 0,
		ERR_DDCCI_OPEN,
		_("An error occured while"
		 " opening the monitor device"))

	TRY_INIT (!fill_profiles_menu (applet),
		ERR_FILL_PROFILES_MENU,
		_("Can't find any profiles"))

	FINISH_INIT()

		if (applet->profile)
			gtk_label_set_label (GTK_LABEL (applet->w_label),
					(gchar*)applet->profile->name);
		else 
			gtk_label_set_label (GTK_LABEL (applet->w_label),
					"ddcc");

		g_timeout_add( IDLE_TIMEOUT*1000, heartbeat, NULL );

	END_INIT()

	/* only reached, if init was not finished */
	gtk_label_set_label (GTK_LABEL (applet->w_label), _("error"));

#	undef START_INIT
#	undef TRY_INIT
#	undef FINISH_INIT
#	undef END_INIT
}

/* creates the properties dialog */
static void
create_properties_dialog (DdccApplet* applet)
{
	GtkWidget* top_hbox;
	GtkWidget* bottom_hbox;
	GtkWidget* main_vbox;
	GtkWidget* close_button;
	GtkWidget* combo_label;
	
	applet->w_properties_dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	applet->w_properties_monitor = gtk_combo_box_new_text ();
	close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	main_vbox = gtk_vbox_new (FALSE,10);
	top_hbox = gtk_hbox_new (FALSE, 10);
	bottom_hbox = gtk_hbox_new (FALSE, 10);
	combo_label = gtk_label_new (_("Monitor:"));

	gtk_window_set_title (GTK_WINDOW (applet->w_properties_dialog),
		       _("Monitor Profile Switcher Properties"));
	gtk_window_set_position (GTK_WINDOW (applet->w_properties_dialog),
			GTK_WIN_POS_CENTER);
	
	gtk_box_pack_start(GTK_BOX (main_vbox),
			top_hbox, 0, 0, 10);
	gtk_box_pack_start(GTK_BOX (main_vbox),
			bottom_hbox,0, 0, 10);
	gtk_box_pack_start(GTK_BOX (top_hbox),
			combo_label, 0, 0, 10);
	gtk_box_pack_start(GTK_BOX (top_hbox),
			applet->w_properties_monitor, 0, 0, 10);
	gtk_box_pack_end(GTK_BOX (bottom_hbox),
			close_button,0,0,10);
	gtk_container_add (GTK_CONTAINER (applet->w_properties_dialog),
			main_vbox);

	gtk_widget_show (top_hbox);
	gtk_widget_show (bottom_hbox);
	gtk_widget_show (main_vbox);
	gtk_widget_show (close_button);
	gtk_widget_show (combo_label);
	gtk_widget_show (applet->w_properties_monitor);

	g_signal_connect (G_OBJECT (close_button), "clicked",
			G_CALLBACK (dialog_close_button_cb), applet);
}

/* main entrance point for the applet */
static int
ddcc_applet_main (PanelApplet* root_applet)
{
	DdccApplet* applet;
	//GtkPixbuff* pixbuf;
	GtkIconTheme* icon_theme;
	GtkWidget* icon;
	GtkWidget* applet_hbox;
	
	applet = g_malloc0 (sizeof (DdccApplet));
	applet->error = ERR_NO_INIT;
	applet->monitor = g_malloc (sizeof (struct monitor));
	applet->w_applet = root_applet;
	
	/* create the label */
	applet->w_label = gtk_label_new ("ddcc");

	icon_theme = gtk_icon_theme_get_default();

	icon = gtk_image_new_from_icon_name ("gddccontrol", GTK_ICON_SIZE_BUTTON);
	applet_hbox = gtk_hbox_new (FALSE,0);

	gtk_box_pack_start (GTK_BOX (applet_hbox), icon, 0, 0, 0);
	gtk_box_pack_start (GTK_BOX (applet_hbox), applet->w_label, 0, 0, 0);

	gtk_container_add ( GTK_CONTAINER (applet->w_applet), applet_hbox);

	/* add the popup menu */
	panel_applet_setup_menu_from_file(applet->w_applet,
			PKGDATADIR, "GNOME_ddcc-applet.xml",
			NULL, ddccapplet_applet_menu_verbs,
			applet);

	/* create the profiles menu (its filled later by
	 * fill_profiles_menu ()) */
	applet->w_profiles_menu = gtk_menu_new ();
	
	/* create the properties dialog */
	create_properties_dialog (applet);
	
	/* connect all callbacks */
	g_signal_connect (G_OBJECT (applet->w_applet), "button-press-event",
			G_CALLBACK (applet_button_cb), applet);
	g_signal_connect (G_OBJECT (root_applet), "destroy",
			G_CALLBACK (destroy_cb), applet);
	g_signal_connect (G_OBJECT (applet->w_properties_dialog),"delete-event",
			G_CALLBACK (dialog_delete_cb), applet);
	g_signal_connect (G_OBJECT (applet->w_properties_monitor), "changed",
			G_CALLBACK (monitor_combo_cb), applet);
	

	/* initialize ddcci lib */
	ddcc_applet_init(applet);

	
	gtk_widget_show_all (GTK_WIDGET (applet->w_applet));
	return TRUE;
}

/* the bonobo activation factory */
static gboolean
ddcc_applet_factory (	PanelApplet *applet,
			const gchar *iid,
			gpointer data)
{
	if (strcmp (iid, "OAFIID:GNOME_ddcc-applet"))
		return FALSE;

	return ddcc_applet_main (applet);
}
PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_ddcc-applet_Factory",
                             PANEL_TYPE_APPLET,
                             "Monitor profile switcher",
                             "0",
                             ddcc_applet_factory,
                             NULL);



