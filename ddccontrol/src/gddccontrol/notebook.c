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
GtkWidget* restorevalue;
GtkWidget* currentbutton = NULL;
GtkWidget* valuecontrol;

GtkWidget* valuerange; // value
GSList* valuegroup; // list

struct control_db* currentControl = NULL;
unsigned short currentDefault = 1;
unsigned short currentMaximum = 1;

int modified = 0;

static void range_callback(GtkWidget *widget, gpointer data)
{
	double val = gtk_range_get_value(GTK_RANGE(valuerange))/100.0;
	
	/*printf("Would change %#x to %#x (%f, %f, %#x)\n", currentControl->address, (unsigned char)round(val*(double)currentMaximum), val*(double)currentMaximum, val, currentMaximum);*/
	
	if (currentControl)
	{
		unsigned short nval = (unsigned short)round(val*(double)currentMaximum);
		ddcci_writectrl(&mon, currentControl->address, nval);
		
		gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*nval/(double)currentMaximum);
		gtk_widget_set_sensitive(restorevalue, TRUE);
		modified = 1;
	}
}

static void group_callback(GtkWidget *widget, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		unsigned short val = ((struct value_db*)data)->value;
		
		/*printf("Would change %#x to %#x\n", currentControl->address, val);*/
		
		if (currentControl)
		{
			ddcci_writectrl(&mon, currentControl->address, val);
			
			gtk_widget_set_sensitive(restorevalue, TRUE);
			modified = 1;
		}
	}
}

static void command_callback(GtkWidget *widget, gpointer data)
{
	unsigned short val = ((struct value_db*)data)->value;
	
	/*printf("Would change %#x to %#x\n", currentControl->address, val);*/
	
	if (currentControl)
	{
		ddcci_writectrl(&mon, currentControl->address, val);
		
		modified = 1;
	}
}

static void restore_callback(GtkWidget *widget, gpointer data)
{	
	if (currentControl)
	{
		switch (currentControl->type) {
		case value:
			ddcci_writectrl(&mon, currentControl->address, currentDefault);
			gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*currentDefault/(double)currentMaximum);
			gtk_widget_set_sensitive(restorevalue, FALSE);
			break;
		case command:
			g_critical(_("restore_callback should not be called for command controls."));
			break;
		case list: {
				GSList* group = valuegroup;
				while(TRUE) {
					if (group == NULL) {
						g_critical(_("Cannot find current control value in value list."));
						return;
					}
					
					printf("group->data@value_db->value == currentDefault (%d == %d)\n",
						((struct value_db*)g_object_get_data(G_OBJECT(group->data), "value_db"))->value, currentDefault);
					if (((struct value_db*)g_object_get_data(G_OBJECT(group->data), "value_db"))->value == currentDefault) {
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(group->data), TRUE);
						break;
					}
					
					group = group->next;
				}
				
				ddcci_writectrl(&mon, currentControl->address, currentDefault);
			}
			break;
		}
	}
}

static void buttons_callback(GtkWidget *widget, gpointer data)
{
	if (currentbutton != NULL) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (currentbutton), 0);
	}
	
	int retry = 1;
	
	struct control_db* control = (struct control_db*)data;
	currentControl = NULL;
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
		currentbutton = widget;
		gtk_label_set_text(GTK_LABEL(valuelabel), control->name);
		if (control->type != command) { /* Don't read the current value if the control is a command */
			for (retry = RETRYS; retry; retry--) {
				if (ddcci_readctrl(&mon, control->address, &currentDefault, &currentMaximum) >= 0) {
					break;
				}
			}
		}
		if (retry > 0)
		{
			/*printf("def: %d, max: %d\n", currentDefault, currentMaximum);
			printf("incs: %f, val: %f\n", 1.0/(double)currentMaximum, (double)currentDefault/(double)currentMaximum);*/
			
			if (gtk_bin_get_child(GTK_BIN(valuecontrol))) {
				gtk_container_remove(GTK_CONTAINER(valuecontrol), gtk_bin_get_child(GTK_BIN(valuecontrol)));
			}
			
			switch (control->type) {
			case value:
				valuerange = gtk_hscale_new_with_range(0.0, 100.0, 1.0);
				gtk_scale_set_digits(GTK_SCALE(valuerange), 1);
				g_signal_connect_after(G_OBJECT(valuerange), "value-changed", G_CALLBACK(range_callback), NULL);
				gtk_range_set_increments(GTK_RANGE(valuerange), 100.0/(double)currentMaximum, 10.0*100.0/(double)currentMaximum);
				gtk_range_set_value(GTK_RANGE(valuerange), (double)100.0*currentDefault/(double)currentMaximum);
				gtk_widget_show(valuerange);
				gtk_container_add(GTK_CONTAINER(valuecontrol), valuerange);
				gtk_widget_show(restorevalue);
				break;
			case command: {
					GtkWidget* vbox = gtk_vbox_new(TRUE, 0);
					struct value_db* value;
					for (value = control->value_list; value != NULL; value = value->next) {
						GtkWidget* button = gtk_button_new_with_label(value->name);
						g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(command_callback), value);
						gtk_widget_show(button);
						gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
					}
					gtk_widget_show(vbox);
					gtk_container_add(GTK_CONTAINER(valuecontrol), vbox);
					gtk_widget_hide(restorevalue);
				}
				break;
			case list: {
					GtkWidget* vbox = gtk_vbox_new(TRUE, 0);
					struct value_db* value;
					GSList* group = NULL;
					valuegroup = NULL;
					for (value = control->value_list; value != NULL; value = value->next) {
						GtkWidget* radio = gtk_radio_button_new_with_label(group, value->name);
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (value->value == currentDefault));
						g_object_set_data(G_OBJECT(radio), "value_db", value);
						g_signal_connect(G_OBJECT(radio), "toggled", G_CALLBACK(group_callback), value);
						gtk_widget_show(radio);
						gtk_box_pack_start(GTK_BOX(vbox), radio, TRUE, TRUE, 0);
						group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
					}
					valuegroup = group;
					gtk_widget_show(vbox);
					gtk_container_add(GTK_CONTAINER(valuecontrol), vbox);
					gtk_widget_show(restorevalue);
				}
				break;
			}
			
			gtk_widget_show(valuecontrol);
			gtk_widget_set_sensitive(restorevalue, FALSE);
			currentControl = control;
		}
		else
		{
			char buf[256];
			snprintf(buf, 256, _("%s\n\nError while getting value"), control->name);
			gtk_label_set_text(GTK_LABEL(valuelabel), buf);
			//fprintf(stderr, "Error while getting value\n");
			gtk_widget_hide(restorevalue);
			gtk_widget_hide(valuecontrol);
		}
	} 
	else
	{
		currentbutton = NULL;
		gtk_label_set_text(GTK_LABEL(valuelabel), _("Please click on a parameter to the left to change it."));
		gtk_widget_hide(restorevalue);
		gtk_widget_hide(valuecontrol);
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
	currentControl = NULL;
	
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
	gtk_widget_show(valuelabel);
	
	valuecontrol = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
	gtk_table_attach(GTK_TABLE(right), valuecontrol, 0, 1, 1, 2, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
	
	restorevalue = gtk_button_new_with_label(_("Restore default value"));
	gtk_table_attach(GTK_TABLE(right), restorevalue, 0, 1, 2, 3, GTK_EXPAND, GTK_EXPAND, 5, 5);
	g_signal_connect_after(G_OBJECT(restorevalue), "clicked", G_CALLBACK(restore_callback), NULL);
	gtk_widget_set_sensitive(restorevalue, FALSE);
	
	GtkWidget* table = gtk_table_new(1, 2, FALSE);
	gtk_widget_show (notebook);
	gtk_table_attach(GTK_TABLE(table), notebook, 0, 1, 0, 1, 0, GTK_FILL_EXPAND, 3, 0);
	gtk_widget_show (right);
	gtk_table_attach(GTK_TABLE(table), right, 1, 2, 0, 1, GTK_FILL_EXPAND, GTK_EXPAND, 3, 0);
	gtk_widget_show (table);
	
	return table;
}

void deleteNotebook() {
	if (modified) {
		ddcci_save(&mon);
	}
	ddcci_close(&mon);
}
