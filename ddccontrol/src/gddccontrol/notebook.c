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
  
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "notebook.h"

#define GTK_FILL_EXPAND (GtkAttachOptions)(GTK_FILL|GTK_EXPAND)

#define LINE_LENGTH 15 //Maximum line length in buttons

struct monitor mon;
GtkWidget* valuelabel;
GtkWidget* valuerange;
GtkWidget* restorevalue;
GtkWidget* currentbutton = NULL;
int currentControl = -1;
unsigned short currentDefault = 1;
unsigned short currentMaximum = 1;

static void range_callback(GtkWidget *widget, gpointer data)
{
	double val = gtk_range_get_value(GTK_RANGE(valuerange))/100.0;
	
	/*printf("Would change %#x to %#x (%f, %f, %#x)\n", currentControl, (unsigned char)round(val*(double)currentMaximum), val*(double)currentMaximum, val, currentMaximum);*/
	
	if (currentControl != -1)
	{
		unsigned short nval = (unsigned short)round(val*(double)currentMaximum);
		ddcci_writectrl(&mon, currentControl, nval);
		
		gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*nval/(double)currentMaximum);
	}
}

static void restore_callback(GtkWidget *widget, gpointer data)
{	
	if (currentControl != -1)
	{
		ddcci_writectrl(&mon, currentControl, currentDefault);
		
		gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*currentDefault/(double)currentMaximum);
	}
}

static void buttons_callback(GtkWidget *widget, gpointer data)
{
	if (currentbutton != NULL) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (currentbutton), 0);
	}
	
	struct control_db* control = (struct control_db*)data;
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
		currentbutton = widget;
		currentControl = control->address;
		gtk_label_set_text(GTK_LABEL(valuelabel), control->name);
		if (ddcci_readctrl(&mon, currentControl, &currentDefault, &currentMaximum) > 0)
		{
			/*printf("def: %d, max: %d\n", currentDefault, currentMaximum);
			printf("incs: %f, val: %f\n", 1.0/(double)currentMaximum, (double)currentDefault/(double)currentMaximum);*/
			gtk_range_set_increments(GTK_RANGE(valuerange), 100.0/(double)currentMaximum, 10.0*100.0/(double)currentMaximum);
			gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*currentDefault/(double)currentMaximum);
			gtk_widget_show(restorevalue);
			gtk_widget_show(valuerange);
		}
		else
		{
			char buf[256];
			snprintf(buf, 256, "%s\n\nError while getting value", control->name);
			gtk_label_set_text(GTK_LABEL(valuelabel), buf);
			//fprintf(stderr, "Error while getting value\n");
			gtk_widget_hide(restorevalue);
			gtk_widget_hide(valuerange);
		}
	} 
	else
	{
		currentbutton = NULL;
		currentControl = -1;
		gtk_label_set_text(GTK_LABEL(valuelabel), "Please click on a parameter to the left to change it.");
		gtk_widget_hide(restorevalue);
		gtk_widget_hide(valuerange);
	}
}

/* 
 *  Word wrap text
 */
char* wrapText(const char* const original, char* new)
{
	int last = -1;
	int linelen = 0;
	int i = 0;
	int len = strlen(original);
	
	for (; i < len; i++, linelen++)
	{
		new[i] = original[i];
		if (original[i] == ' ') {
			last = i;
		}
		
		if ((linelen > LINE_LENGTH) && (last != -1)) {
			new[last] = '\n';
			linelen = i-last;
			last = -1;
		}
	}
	
	new[i] = '\0';
	
	return new;
}

void createPage(GtkWidget* notebook, struct group_db* group)
{
	int x, y;
	int width = 0;
	GtkWidget* table = gtk_table_new(1, 1, TRUE);
	GtkWidget* button;
	GtkWidget* label;
	
	x = 0;
	y = 0;
	
	struct control_db* control;
	
	for (control = group->control_list; control != NULL; control = control->next)
	{
		char* buffer = malloc(strlen(control->name));
		button = gtk_toggle_button_new();
		label = gtk_label_new(wrapText(control->name, buffer));
		free(buffer);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
		gtk_widget_show (label);
		gtk_container_add (GTK_CONTAINER (button), label);
		
		gtk_widget_show (button);
		gtk_table_attach(GTK_TABLE(table), button, x, x+1, y, y+1, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
		g_signal_connect_after(G_OBJECT(button), "toggled", 
			G_CALLBACK(buttons_callback), (gpointer)control);
		x++;
		if (x == 3) { /* Temporary */
			x = 0;
			y++;
		}
	}
	
	if (x > width)
		width = x;
	
	gtk_table_resize(GTK_TABLE(table), y+1, width);
	
	label = gtk_label_new(group->name);
	
	gtk_widget_show (label);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);
	gtk_widget_show (table);
}

GtkWidget* createNotebook(struct monitorlist* monitor)
{
	currentbutton = NULL;
	currentControl = -1;
	
	ddcci_open(&mon, monitor->filename);
	
	GtkWidget* notebook = gtk_notebook_new();
	
	if (mon.db)
	{
		struct group_db* group;
		
		for (group = mon.db->group_list; group != NULL; group = group->next) {
			createPage(notebook, group);
		}
	}
	
	GtkWidget* right = gtk_table_new(3, 1, TRUE);
	valuelabel = gtk_label_new ("Please click on a parameter to the left to change it.");
	gtk_table_attach(GTK_TABLE(right), valuelabel, 0, 1, 0, 1, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
	valuerange = gtk_hscale_new_with_range(0.0, 100.0, 1.0 );
	gtk_widget_show(valuelabel);
	//gtk_widget_show(valuerange);
	gtk_scale_set_digits(GTK_SCALE(valuerange), 1);
	gtk_table_attach(GTK_TABLE(right), valuerange, 0, 1, 1, 2, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
	g_signal_connect_after(G_OBJECT(valuerange), "value-changed", G_CALLBACK(range_callback), NULL);
	
	restorevalue = gtk_button_new_with_label("Restore default value");
	gtk_table_attach(GTK_TABLE(right), restorevalue, 0, 1, 2, 3, GTK_EXPAND, GTK_EXPAND, 5, 5);
	g_signal_connect_after(G_OBJECT(restorevalue), "clicked", G_CALLBACK(restore_callback), NULL);
	
	GtkWidget* table = gtk_table_new(1, 2, TRUE);
	gtk_widget_show (notebook);
	gtk_table_attach(GTK_TABLE(table), notebook, 0, 1, 0, 1, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 3, 0);
	gtk_widget_show (right);
	gtk_table_attach(GTK_TABLE(table), right, 1, 2, 0, 1, GTK_FILL_EXPAND, GTK_EXPAND, 3, 0);
	gtk_widget_show (table);
	
	return table;
}

