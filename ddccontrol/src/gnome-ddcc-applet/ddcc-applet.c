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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */


/* ****************
 * Includes
 * ****************/

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

/* config */
#include "config.h"

/* Popup menu on the applet */
static const BonoboUIVerb ddccapplet_applet_menu_verbs[] = 
{
        BONOBO_UI_UNSAFE_VERB ("DdccAppletProperties", menu_properties_cb),
        BONOBO_UI_UNSAFE_VERB ("DdccAppletAbout", menu_about_cb),
        BONOBO_UI_VERB_END
};


/* ****************
 * Helpers
 * ****************/

/* display [msg] as popup errormessage */
void
error_message (char *msg)
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

/* writes device name of the current monitor into [buffer] 
 * returns [buffer] on success, NULL on failure */
char*
get_monitor_name (char *buffer)
{
	struct monitorlist* monlist;
	struct monitorlist* current;
	
	if (!(monlist = ddcci_load_list ()))
		return NULL;
	
	for (current = monlist; current; current = current->next)
		snprintf (buffer, 256, "%s", current->filename);

	ddcci_free_list (monlist);
	
	return buffer;
}

/* makes the menu appear next to the widget [user_data]
 * instead off right under the cursor */
void
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


/* ****************
 * Callbacks
 * ****************/

void
menu_properties_cb(BonoboUIComponent *uic,
		DdccApplet *applet,
	       	const gchar *verbname)
{

	gtk_widget_show(applet->w_properties_dialog);
}

void
menu_about_cb(BonoboUIComponent *uic,
		DdccApplet *applet,
	       	const gchar *verbname)
{
	static const gchar *authors[] = {
		"Christian Schilling <cschilling@gmx.de>",
		NULL
	};
	
	gtk_show_about_dialog (NULL,
			"name",		_("Ddcc Applet"),
			"version",	VERSION,
			"copyright",	"\xC2\xA9 2005 Christian Schilling",
			"comments",	_("An applet for quick switching of monitor profiles\n"
				"based on libddccontrol and part of the ddccontrol project\n"
				"(http://ddccontrol.sourceforge.net)"),
			"authors",	authors,
			NULL);
}

/* called when the user klicks on the applet */
gboolean
applet_button_cb (GtkWidget	*widget,
		GdkEventButton	*event,
		DdccApplet	*applet)
{

	if (event->button == 1)
	{
		if (applet->error) {
			ddcc_applet_init(applet);
			return FALSE;
		}
		
		build_profiles_menu (applet);
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
gboolean
change_profile_cb (GtkMenuItem*	item,
		  DdccApplet*	applet)
{
	struct profile* profile = NULL;
	
	profile = g_object_get_data (G_OBJECT (item), "ddcc_profile");
	ddcci_apply_profile (profile, applet->monitor);

	applet->profile = profile;
		
	gtk_label_set_label (GTK_LABEL (applet->w_label), profile->name);

	return FALSE;
}

gboolean
dialog_delete_cb(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gtk_widget_hide(widget);

	return TRUE;
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
void
build_profiles_menu (DdccApplet *applet)
{
	struct profile* profile;
	GtkWidget* item;

	if (applet->w_profiles_menu)
		return;
	
	applet->w_profiles_menu = gtk_menu_new ();
	

	for (profile=applet->monitor->profiles;profile;profile=profile->next) {
		item = gtk_menu_item_new_with_label (profile->name);
		g_object_set_data (G_OBJECT (item), "ddcc_profile", profile);
		g_signal_connect (item, "activate",
				 G_CALLBACK (change_profile_cb), applet);
		gtk_widget_show (item);
		gtk_container_add (	GTK_CONTAINER (applet->w_profiles_menu),
					item);
	}
}

/* initializes the ddccontrol library, if initialisation is already
 * partly done it's just completed */
static void
ddcc_applet_init (DdccApplet* applet)
{
	switch(applet->error) {
		case ERR_NO_INIT:
			
		case ERR_DDCCI_INIT:
			if (!ddcci_init (NULL)) {
				error_message (_("Unable to initialize ddcci library\n"));
				applet->error = ERR_DDCCI_INIT;
				break;
			}
		
		case ERR_GET_MONITOR_NAME:
			if (!get_monitor_name (applet->monitor_name)) {
				error_message (_("No monitor configuration found."
						 " Plase run gddccontrol first\n"));
				applet->error = ERR_GET_MONITOR_NAME;
				break;
			}
			
		case ERR_DDCCI_OPEN:
			if (ddcci_open (applet->monitor,
					get_monitor_name (applet->monitor_name), 0) < 0) {
				error_message (_("An error occured while "
						 "opening the monitor device.\n"));
				applet->error = ERR_DDCCI_OPEN;
				break;
			}
			
		case ERR_GET_PROFILES:
			if (!ddcci_get_all_profiles (applet->monitor)) {
				error_message(_("Can't find any profiles\n"));
				applet->error = ERR_GET_PROFILES;
				break;
			}
			
			applet->error = ERR_OK;

		case ERR_OK:
			if (applet->profile)
				gtk_label_set_label (GTK_LABEL(applet->w_label),
						     applet->profile->name);
			else 
				gtk_label_set_label (GTK_LABEL(applet->w_label),
						     "ddcc");
			return;
	}
	
	gtk_label_set_label(GTK_LABEL(applet->w_label),_("error"));
}

/* main entrance point for the applet */
static int
ddcc_applet_main (GtkWidget* root_applet)
{
	DdccApplet* applet;
	applet = g_malloc0 (sizeof (DdccApplet));

	applet->error = ERR_NO_INIT;
	applet->monitor = g_malloc (sizeof (struct monitor));
	applet->w_applet = root_applet;
	
	/* create the label */
	applet->w_label = gtk_label_new ("ddcc");
	gtk_container_add ( GTK_CONTAINER (applet->w_applet), applet->w_label);

	/* add the popup menu */
	panel_applet_setup_menu_from_file(applet->w_applet,
			PKGDATADIR, "GNOME_ddcc-applet.xml",
			NULL, ddccapplet_applet_menu_verbs,
			applet);

	/* create the properties dialog */
	applet->w_properties_dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	applet->w_properties_monitor = gtk_combo_box_new_text ();
	gtk_container_add (GTK_CONTAINER (applet->w_properties_dialog),
			applet->w_properties_monitor);
	gtk_widget_show (applet->w_properties_monitor);
	gtk_combo_box_append_text ( GTK_COMBO_BOX (applet->w_properties_monitor), "sdfsdfsd");


	/* initialize ddcci lib */
	ddcc_applet_init(applet);

	/* connect all callbacks */
	g_signal_connect (G_OBJECT (applet->w_applet), "button-press-event",
			G_CALLBACK (applet_button_cb), applet);
	g_signal_connect (G_OBJECT (root_applet), "destroy",
			G_CALLBACK (destroy_cb), applet);
	g_signal_connect(G_OBJECT (applet->w_properties_dialog),"delete-event",
			G_CALLBACK (dialog_delete_cb), applet);
	
	
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

	return ddcc_applet_main (GTK_WIDGET (applet));
}
PANEL_APPLET_BONOBO_FACTORY ("OAFIID:GNOME_ddcc-applet_Factory",
                             PANEL_TYPE_APPLET,
                             "Monitor profile switcher",
                             "0",
                             ddcc_applet_factory,
                             NULL);



