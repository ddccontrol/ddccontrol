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

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data ) {
    return FALSE;
}

static void destroy( GtkWidget *widget,
                     gpointer   data ) {
    gtk_main_quit ();
}

static void combo_change(GtkWidget *widget, gpointer data)
{
	int i = 0;

	if (notebook != NULL) {
		deleteNotebook();
		gtk_container_remove(GTK_CONTAINER(table), notebook);
		gtk_widget_destroy(notebook);
	}

	char buffer[256];
	
	int id = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_box));
	
	struct monitorlist* current;
	
	combo_box = gtk_combo_box_new_text();
	
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
	
	gtk_table_attach(GTK_TABLE(table), notebook, 0, 1, 2, 3, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
}

int main( int   argc, char *argv[] )
{ 
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
	
	gtk_init(&argc, &argv);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	
	gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
	
	g_signal_connect (G_OBJECT (window), "delete_event",
				G_CALLBACK (delete_event), NULL);
	
	g_signal_connect (G_OBJECT (window), "destroy",
				G_CALLBACK (destroy), NULL);
	
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);
	
	gtk_widget_show (window);

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
	
	table = gtk_table_new(3, 1, FALSE);
	gtk_widget_show (table);
	
	gtk_table_attach(GTK_TABLE(table), combo_box, 0, 1, 0, 1, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
	
	moninfo = gtk_label_new (_(
		"No monitor supporting DDC/CI available."
		"Please check all the needed kernel modules are loaded (i2c-dev, and your framebuffer driver)."
	));
	gtk_misc_set_alignment(GTK_MISC(moninfo), 0, 0);
	
	gtk_table_attach(GTK_TABLE(table), moninfo, 0, 1, 1, 2, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
	
	gtk_container_add (GTK_CONTAINER (window), table);
	
	if (monlist != NULL) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_box), 0);
	}
	
	gtk_widget_show (moninfo);
	
	gtk_main();
	
	deleteNotebook();
	
	ddcci_free_list(monlist);
	
	return 0;
}
