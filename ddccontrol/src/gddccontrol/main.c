/***************************************************************************
 *   Copyright (C) 2004 by Nicolas Boichat                                 *
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "config.h"

#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>

#ifdef HAVE_XINERAMA
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/Xinerama.h>
#endif

#include "ddcci.h"
#include "ddcpci-ipc.h"
#include "monitor_db.h"

#include "notebook.h"

#define GTK_FILL_EXPAND (GtkAttachOptions)(GTK_FILL|GTK_EXPAND)

GtkWidget *window;

GtkWidget* table;
GtkWidget* notebook = NULL;

GtkWidget *combo_box;

GtkWidget *messagelabel = NULL;

GtkWidget* close_button = NULL;

struct monitorlist* monlist;

#ifdef HAVE_XINERAMA
int xineramacurrent = 0; //Arbitrary, should be read from the user
int xineramanumber;
XineramaScreenInfo* xineramainfo;
#endif

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

static void combo_change(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(widget, FALSE);
	
	int id = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	
	if (id == -1) { 
		return;
	}
	
	int i = 0;

	if (notebook != NULL) {
		deleteNotebook();
		gtk_widget_destroy(notebook);
		notebook = NULL;
	}

	char buffer[256];
	
	struct monitorlist* current;
	
	for (current = monlist;; current = current->next)
	{
		g_assert(current != NULL);
		if (i == id)
		{
			snprintf(buffer, 256, "%s: %s", current->filename, current->name);
			notebook = createNotebook(current);
			gtk_widget_show(notebook);
			//gtk_label_set_text(GTK_LABEL(moninfo), buffer);
			break;
		}
		i++;
	}
	
	gtk_table_attach(GTK_TABLE(table), notebook, 0, 1, 1, 2, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 5, 5);
	
	gtk_widget_set_sensitive(widget, TRUE);
}

#ifdef HAVE_XINERAMA
static gboolean window_changed(GtkWidget *widget, 
				GdkEventConfigure *event,
				gpointer user_data)
{
	if (xineramanumber == 2) { // If there are more than two monitors, we need a configuration menu
		int cx, cy, i;
		struct monitorlist* current;
		
		if (monlist == NULL) {
			printf("monlist == NULL\n");
			return FALSE;
		}
		
		i = 0;
		for (current = monlist; current != NULL; current = current->next) i++;
		
		if (i != 2) { // Every monitor does not support DDC/CI, or there are more monitors plugged-in
			printf("i : %d\n", i);
			return FALSE;
		}
		
		cx = event->x + (event->width/2);
		cy = event->y + (event->height/2);
		
		for (i = 0; i < xineramanumber; i++) {
			if (
				(cx >= xineramainfo[i].x_org) && (cx < xineramainfo[i].x_org+xineramainfo[i].width) &&
				(cy >= xineramainfo[i].y_org) && (cy < xineramainfo[i].y_org+xineramainfo[i].height)
			   ) {
				if (xineramacurrent != i) {
					int k = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_box));
					printf(_("Monitor changed (%d %d).\n"), i, k);
					k = (k == 0) ? 1 : 0;
					gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), k);
					xineramacurrent = i;
				}
				return FALSE;
			}
		}
		
		//g_warning(_("Could not find the current monitor in Xinerama monitors list."));
	}
	
	return FALSE;
}
#endif

void setStatus(char* message)
{
	if (!message[0]) {
		gtk_widget_hide(messagelabel);
		if (notebook) {
			gtk_widget_show(notebook);
		}
		gtk_widget_set_sensitive(close_button, TRUE);
		return;
	}
	
	gtk_widget_set_sensitive(close_button, FALSE);
	gtk_label_set_text(GTK_LABEL(messagelabel), message);
	gtk_widget_show(messagelabel);
	if (notebook) {
		gtk_widget_hide(notebook);
	}
	
	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static gboolean heartbeat(gpointer data) {
	ddcpci_send_heartbeat();
	return TRUE;
}

int main( int   argc, char *argv[] )
{ 
	int i, verbosity = 0;
	int event_base, error_base;
	
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	bindtextdomain("ddccontrol-db", LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	bind_textdomain_codeset("ddccontrol-db", "UTF-8");
	textdomain(PACKAGE);
	
	while ((i=getopt(argc,argv,"v")) >= 0)
	{
		switch(i) {
		case 'v':
			verbosity++;
			break;
		}
	}
	
	ddcci_verbosity(verbosity);
	
	gtk_init(&argc, &argv);
	
	if (!ddcci_init()) {
		printf(_("Unable to initialize ddcci library.\n"));
		GtkWidget* dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Unable to initialize ddcci library, see console for more details.\n"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return 1;
	}
	
	g_timeout_add( IDLE_TIMEOUT*1000, heartbeat, NULL );
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window),_("Monitor settings"));
	
	gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);
	
	g_signal_connect (G_OBJECT (window), "delete_event",
				G_CALLBACK (delete_event), NULL);
	
	g_signal_connect (G_OBJECT (window), "destroy",
				G_CALLBACK (destroy), NULL);

	#ifdef HAVE_XINERAMA
	g_signal_connect (G_OBJECT (window), "configure-event",
				G_CALLBACK (window_changed), NULL);
	#endif
	
	gtk_container_set_border_width (GTK_CONTAINER (window), 4);
	
	table = gtk_table_new(3, 1, FALSE);
	gtk_widget_show (table);
	
	GtkWidget* align = gtk_alignment_new(1,1,0,0);
	close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	g_signal_connect(G_OBJECT(close_button),"clicked",G_CALLBACK (destroy), NULL);

	gtk_container_add(GTK_CONTAINER(align),close_button);
	gtk_widget_show (close_button);
	gtk_widget_show (align);
	gtk_table_attach(GTK_TABLE(table), align, 0, 1, 2, 3, GTK_FILL_EXPAND, GTK_SHRINK, 8, 8);
	
	gtk_container_add (GTK_CONTAINER (window), table);
	
	messagelabel = gtk_label_new ("");
	gtk_label_set_line_wrap(GTK_LABEL(messagelabel), TRUE);
	gtk_table_attach(GTK_TABLE(table), messagelabel, 0, 1, 1, 2, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 5, 5);
	
	gtk_widget_show (window);
	
	#ifdef HAVE_XINERAMA
	if (XineramaQueryExtension(GDK_DISPLAY(), &event_base, &error_base)) {
		if (XineramaIsActive(GDK_DISPLAY())) {
			printf(_("Xinerama supported and active\n"));
			
			xineramainfo = XineramaQueryScreens(GDK_DISPLAY(), &xineramanumber);
			int i;
			for (i = 0; i < xineramanumber; i++) {
				printf(_("Display %d: %d, %d, %d, %d, %d\n"), i, 
					xineramainfo[i].screen_number, xineramainfo[i].x_org, xineramainfo[i].y_org, 
					xineramainfo[i].width, xineramainfo[i].height);
			}
		}
		else {
			printf(_("Xinerama supported but inactive.\n"));
		}
	}
	else {
		printf(_("Xinerama not supported\n"));
	}
	#endif
	
	setStatus(_(
	"Probing for available monitors..."
		   ));
	
	monlist = ddcci_probe();
	
	struct monitorlist* current;
	
	combo_box = gtk_combo_box_new_text();
	
	char buffer[256];
	
	for (current = monlist; current != NULL; current = current->next)
	{
		snprintf(buffer, 256, "%s: %s", 
			current->filename, current->name);
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), buffer);
	}
	
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (combo_change), NULL);
	
	gtk_table_attach(GTK_TABLE(table), combo_box, 0, 1, 0, 1, GTK_FILL_EXPAND, 0, 5, 5);
	
/*	moninfo = gtk_label_new ();
	gtk_misc_set_alignment(GTK_MISC(moninfo), 0, 0);
	
	gtk_table_attach(GTK_TABLE(table), moninfo, 0, 1, 1, 2, GTK_FILL_EXPAND, 0, 5, 5);*/
	
	if (monlist) {
		gtk_widget_show (combo_box);
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);
	}
	else {
		setStatus(_(
			"No monitor supporting DDC/CI available.\n\n"
			"If your graphics card need it, please check all the required kernel modules are loaded (i2c-dev, and your framebuffer driver)."
			   ));
	}
	
//	gtk_widget_show (moninfo);
	
	gtk_main();
	
	deleteNotebook();
	
	ddcci_free_list(monlist);
	
	ddcci_release();
	
	#ifdef HAVE_XINERAMA
	XFree(xineramainfo);
	#endif
	
	return 0;
}
