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
#include "monitor_db.h"

#include "notebook.h"

#define GTK_FILL_EXPAND (GtkAttachOptions)(GTK_FILL|GTK_EXPAND)

GtkWidget *window;

GtkWidget* table;
GtkWidget* notebook = NULL;

GtkWidget *combo_box;

GtkWidget *moninfo;

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
			gtk_label_set_text(GTK_LABEL(moninfo), buffer);
			break;
		}
		i++;
	}
	
	gtk_table_attach(GTK_TABLE(table), notebook, 0, 1, 2, 3, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 5, 5);
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
					printf("Monitor changed (%d %d).\n", i, k);
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
	ddcpci_init();
	
	gtk_init(&argc, &argv);
	
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
	
	gtk_widget_show (window);
	
	#ifdef HAVE_XINERAMA
	if (XineramaQueryExtension(GDK_DISPLAY(), &event_base, &error_base)) {
		if (XineramaIsActive(GDK_DISPLAY())) {
			printf("Xinerama supported and active\n");
			
			xineramainfo = XineramaQueryScreens(GDK_DISPLAY(), &xineramanumber);
			int i;
			for (i = 0; i < xineramanumber; i++) {
				printf("Display %d: %d, %d, %d, %d, %d\n", i, 
					xineramainfo[i].screen_number, xineramainfo[i].x_org, xineramainfo[i].y_org, 
					xineramainfo[i].width, xineramainfo[i].height);
			}
		}
		else {
			printf("Xinerama supported but inactive.\n");
		}
	}
	else {
		printf("Xinerama not supported\n");
	}
	#endif
	
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
	
	gtk_widget_show (combo_box);
	
	g_signal_connect (G_OBJECT (combo_box), "changed", G_CALLBACK (combo_change), NULL);
	
	table = gtk_table_new(4, 1, FALSE);
	gtk_widget_show (table);
	
	gtk_table_attach(GTK_TABLE(table), combo_box, 0, 1, 0, 1, GTK_FILL_EXPAND, 0, 5, 5);
	
	moninfo = gtk_label_new (_(
		"No monitor supporting DDC/CI available."
		"Please check all the needed kernel modules are loaded (i2c-dev, and your framebuffer driver)."
	));
	gtk_misc_set_alignment(GTK_MISC(moninfo), 0, 0);
	
	gtk_table_attach(GTK_TABLE(table), moninfo, 0, 1, 1, 2, GTK_FILL_EXPAND, 0, 5, 5);
	
	gtk_container_add (GTK_CONTAINER (window), table);
	
	if (monlist != NULL) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);
	}

    GtkWidget* align = gtk_alignment_new(1,1,0,0);
    GtkWidget* close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    g_signal_connect(G_OBJECT(close_button),"clicked",G_CALLBACK (destroy), NULL);

    
    gtk_container_add(GTK_ALIGNMENT(align),close_button);
	gtk_widget_show (close_button);
	gtk_widget_show (align);
	gtk_table_attach(GTK_TABLE(table), align, 0, 1, 3, 4, GTK_FILL_EXPAND, GTK_SHRINK, 8, 8);

//	gtk_widget_show (moninfo);
	
	gtk_main();
	
	deleteNotebook();
	
	ddcci_free_list(monlist);
	
	ddcpci_release();
	
	#ifdef HAVE_XINERAMA
	XFree(xineramainfo);
	#endif
	
	return 0;
}
