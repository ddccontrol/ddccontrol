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

#define RETRYS 3 /* number of retrys */

struct monitor mon;
GtkWidget* valuelabel;
GtkWidget* valuerange;
GtkWidget* restorevalue;
GtkWidget* currentbutton = NULL;
int currentControl = -1;
unsigned short currentDefault = 1;
unsigned short currentMaximum = 1;

int modified = 0;

static void range_callback(GtkWidget *widget, gpointer data)
{
	double val = gtk_range_get_value(GTK_RANGE(valuerange))/100.0;
	
	/*printf("Would change %#x to %#x (%f, %f, %#x)\n", currentControl, (unsigned char)round(val*(double)currentMaximum), val*(double)currentMaximum, val, currentMaximum);*/
	
	if (currentControl != -1)
	{
		unsigned short nval = (unsigned short)round(val*(double)currentMaximum);
		ddcci_writectrl(&mon, currentControl, nval);
		
		gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*nval/(double)currentMaximum);
		gtk_widget_set_sensitive(restorevalue, TRUE);
		modified = 1;
	}
}

static void restore_callback(GtkWidget *widget, gpointer data)
{	
	if (currentControl != -1)
	{
		ddcci_writectrl(&mon, currentControl, currentDefault);
		
		gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*currentDefault/(double)currentMaximum);
		gtk_widget_set_sensitive(restorevalue, FALSE);
	}
}

static void buttons_callback(GtkWidget *widget, gpointer data)
{
	if (currentbutton != NULL) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (currentbutton), 0);
	}
	
	int retry;
	
	struct control_db* control = (struct control_db*)data;
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
		currentbutton = widget;
		currentControl = control->address;
		gtk_label_set_text(GTK_LABEL(valuelabel), control->name);
		for (retry = RETRYS; retry; retry--) {
			if (ddcci_readctrl(&mon, currentControl, &currentDefault, &currentMaximum)) {
				break;
			}
		}
		if (retry > 0)
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
			snprintf(buf, 256, _("%s\n\nError while getting value"), control->name);
			gtk_label_set_text(GTK_LABEL(valuelabel), buf);
			//fprintf(stderr, "Error while getting value\n");
			gtk_widget_hide(restorevalue);
			gtk_widget_hide(valuerange);
		}
		gtk_widget_set_sensitive(restorevalue, FALSE);
	} 
	else
	{
		currentbutton = NULL;
		currentControl = -1;
		gtk_label_set_text(GTK_LABEL(valuelabel), _("Please click on a parameter to the left to change it."));
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

void createPage(GtkWidget* notebook, struct subgroup_db* subgroup)
{
	int x, y;
	GtkWidget* table = gtk_table_new(1, 1, TRUE);
	GtkWidget* button;
	GtkWidget* label;
	
	x = 0;
	y = 0;
	
	struct control_db* control;
	
	for (control = subgroup->control_list; control != NULL; control = control->next)
	{
		char* buffer = malloc(strlen(control->name)+1);
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
		if (x == 3) {
			x = 0;
			y++;
		}
	}
	
	gtk_table_resize(GTK_TABLE(table), y+1, 3);
	
	label = gtk_label_new(subgroup->name);
	
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
	GtkWidget* secnotebook;
	GtkWidget* label;
	
	struct group_db* group;
	struct subgroup_db* subgroup;
	
	if (mon.db)
	{
		for (group = mon.db->group_list; group != NULL; group = group->next) {
			secnotebook = gtk_notebook_new();
			
			gtk_notebook_set_tab_pos(GTK_NOTEBOOK(secnotebook), GTK_POS_LEFT);
			
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next) {
				createPage(secnotebook, subgroup);
			}
			
			label = gtk_label_new(group->name);
			
			gtk_widget_show (label);
			gtk_notebook_append_page (GTK_NOTEBOOK (notebook), secnotebook, label);
			gtk_widget_show (secnotebook);
		}
	}
	
	GtkWidget* right = gtk_table_new(3, 1, TRUE);
	valuelabel = gtk_label_new (_("Please click on a parameter to the left to change it."));
	gtk_table_attach(GTK_TABLE(right), valuelabel, 0, 1, 0, 1, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
	valuerange = gtk_hscale_new_with_range(0.0, 100.0, 1.0 );
	gtk_widget_show(valuelabel);
	//gtk_widget_show(valuerange);
	gtk_scale_set_digits(GTK_SCALE(valuerange), 1);
	gtk_table_attach(GTK_TABLE(right), valuerange, 0, 1, 1, 2, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
	g_signal_connect_after(G_OBJECT(valuerange), "value-changed", G_CALLBACK(range_callback), NULL);
	
	restorevalue = gtk_button_new_with_label(_("Restore default value"));
	gtk_table_attach(GTK_TABLE(right), restorevalue, 0, 1, 2, 3, GTK_EXPAND, GTK_EXPAND, 5, 5);
	g_signal_connect_after(G_OBJECT(restorevalue), "clicked", G_CALLBACK(restore_callback), NULL);
	gtk_widget_set_sensitive(restorevalue, FALSE);
	
	GtkWidget* table = gtk_table_new(1, 2, FALSE);
	gtk_widget_show (notebook);
	gtk_table_attach(GTK_TABLE(table), notebook, 0, 1, 0, 1, 0, 0, 3, 0);
	gtk_widget_show (right);
	gtk_table_attach(GTK_TABLE(table), right, 1, 2, 0, 1, GTK_FILL_EXPAND, GTK_EXPAND, 3, 0);
	gtk_widget_show (table);
	
	return table;
}

void deleteNotebook() {
	ddcci_save(&mon);
	ddcci_close(&mon);
}
