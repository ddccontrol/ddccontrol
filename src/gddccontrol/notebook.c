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
  
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#include "notebook.h"
#include "internal.h"

#include "daemon/dbus_client.h"
	
#define GTK_FILL_EXPAND (GtkAttachOptions)(GTK_FILL|GTK_EXPAND)

#define RETRYS 5 // number of read retrys

enum
{
	TITLE_COL,
	ID_COL,
	WIDGET_COL,
	N_COLS,
};

// Protos
////////////////////
static void change_control_value(GtkWidget *widget, gpointer nval);

// Globals
////////////////////

int modified;
GSList *all_controls;

int refreshing = 0;

extern DDCControl *ddccontrol_proxy;

// Helpers
////////////////////

static void write_dbctrl(struct control_db *control, unsigned short nval) {
	ddcci_writectrl(mon, control->address, nval, control->delay);
}
	
static void get_value_and_max(struct control_db *control, unsigned short *currentValue, unsigned short *currentMaximum)
{
	int result;
	if (control->type != command)
	{
		result = ddcci_readctrl(mon, control->address, currentValue, currentMaximum);
	}
	if(result < 0)
	{
		GtkWidget* dialog = gtk_message_dialog_new(
				GTK_WINDOW(main_app_window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				"%s: %s", _("Error while getting value"), control->name);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

short get_control_max(struct control_db *control) {
	GSList* list = all_controls;
	
	while (list) {
		struct control_db *control_db = (struct control_db*)g_object_get_data(G_OBJECT(list->data), "ddc_control");
		if (control == control_db) {
			return (short)(long)g_object_get_data(G_OBJECT(list->data), "ddc_max");
		}
		list = g_slist_next(list);
	}
	
	return 0;
}

// Callbacks
////////////////////

void refresh_all_controls(GtkWidget *widget, gpointer data)
{
	/* Maybe we could lock a Mutex here, but I don't think it is really necessary... */
	
	gtk_widget_set_sensitive(refresh_controls_button, FALSE);
	
	int current = 0;
	int count = g_slist_length(all_controls);
	GSList* list = all_controls;
	unsigned short currentValue = 1;
	unsigned short currentMaximum = 1;
	
	gchar* tmp = g_strdup_printf(_("Refreshing controls values (%d%%)..."), (current*100)/count);
	set_message(tmp);
	g_free(tmp);
	
	refreshing = 1; /* Tell callbacks not to write values back to the monitor. */
	while (list) {
		struct control_db *control = (struct control_db*)g_object_get_data(G_OBJECT(list->data), "ddc_control");
		if (control) {
			if (control->type != command) {
				get_value_and_max(control, &currentValue, &currentMaximum);
				change_control_value(list->data, (gpointer)(long)currentValue);
			}
			gchar* tmp = g_strdup_printf(_("Refreshing controls values (%d%%)..."), (current*100)/count);
			set_message(tmp);
			g_free(tmp);
		}
		else {
			g_warning(_("Could not get the control_db struct related to a control."));
		}
		list = g_slist_next(list);
		current++;
	}
	refreshing = 0;
	
	set_message("");
	
	gtk_widget_set_sensitive(refresh_controls_button, TRUE);
}

static void change_control_value(GtkWidget *widget, gpointer nval)
{
	struct control_db *control = (struct control_db*)g_object_get_data(G_OBJECT(widget),"ddc_control");
	
	/* No control found on the widget, we are probably on an item of a group (list) */
	if (control == NULL) {
		GtkWidget *parent = gtk_widget_get_parent(widget);
		control = (struct control_db*)g_object_get_data(G_OBJECT(parent),"ddc_control");
		
		if ((control) && (control->type == list)) {
			struct value_db *value = (struct value_db*)g_object_get_data(G_OBJECT(widget), "ddc_value");
			unsigned short val = value->value;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), (long)nval == val);
		}
		return;
	}
	
	switch(control->type)
	{
		case value:
		{
			unsigned long Maximum = (unsigned long) g_object_get_data(G_OBJECT(widget),"ddc_max");
			gtk_range_set_increments(GTK_RANGE(widget), 100.0/(double)Maximum, 10.0*100.0/(double)Maximum);
			gtk_range_set_value(GTK_RANGE(widget), 100.0*(double)(long)nval/(double)Maximum);
			break;
		}
		case command:
			break;
		case list:
			gtk_container_foreach(GTK_CONTAINER(widget), change_control_value, nval);
			break;
	}
}

static void range_callback(GtkWidget *widget, gpointer data)
{
	struct control_db *control = (struct control_db*)g_object_get_data(G_OBJECT(widget),"ddc_control");
	if (!control) {
		return;
	}
	GtkWidget* button = (GtkWidget*)g_object_get_data(G_OBJECT(widget),"restore_button");
	g_return_if_fail(button);
	
	double val = gtk_range_get_value(GTK_RANGE(widget))/100.0;
	
	unsigned long Default = (unsigned long) g_object_get_data(G_OBJECT(widget),"ddc_default");
	unsigned long Maximum = (unsigned long) g_object_get_data(G_OBJECT(widget),"ddc_max");

#if 0
	printf("Would change %#x to %#x (%f, %f, %#x)\n",
			control->address,
			(unsigned char)round(val*(double)Maximum),
			val*(double)Maximum,
			val,
			Maximum);
#endif
	
	unsigned short nval = (unsigned short)round(val*(double)Maximum);
	if (!refreshing)
		write_dbctrl(control, nval);
	
	gtk_range_set_value(GTK_RANGE(widget), (double)100.0*nval/(double)Maximum);
	
	gtk_widget_set_sensitive(GTK_WIDGET(button), nval != Default);
	modified = 1;
}
	
static void group_callback(GtkWidget *widget, gpointer data)
{
	struct value_db *value = (struct value_db*)g_object_get_data(G_OBJECT(widget), "ddc_value");
	g_return_if_fail(value);
	
	GtkWidget *parent = gtk_widget_get_parent(widget);
	g_return_if_fail(parent);
	
	struct control_db *control = (struct control_db*)g_object_get_data(G_OBJECT(parent),"ddc_control");
	if (!control) {
		return;
	}
	GtkWidget* button = (GtkWidget*)g_object_get_data(G_OBJECT(parent),"restore_button");
	g_return_if_fail(button);
	
	unsigned long Default = (unsigned long) g_object_get_data(G_OBJECT(parent),"ddc_default");
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
		unsigned short val = value->value;
		
#if 0
		printf("Would change %#x to %#x\n", currentControl->address, val);
#endif
		
		if (control)
		{
			if (!refreshing)
				write_dbctrl(control, val);
			
			gtk_widget_set_sensitive(GTK_WIDGET(button), val != Default);
			modified = 1;
			
			/* Refresh if needed */
			if (control->refresh == all) {
				refresh_all_controls(NULL, NULL);
			}
		}
	}
}
	
static void command_callback(GtkWidget *widget, gpointer data)
{
	struct value_db *value = (struct value_db*) data;
	struct control_db *control = (struct control_db*)g_object_get_data(G_OBJECT(widget),"ddc_control");
	
	unsigned short val;
	
	val = value->value;
	
#if 0
	printf("Would change %#x to %#x\n", currentControl->address, val);
#endif
	
	if (control)
	{
		write_dbctrl(control, val);
		
		modified = 1;
		
		/* Refresh if needed */
		if (control->refresh == all) {
			refresh_all_controls(NULL, NULL);
		}
	}
	//g_slist_foreach(all_controls,refresh_control,0);
}
	
static void restore_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget* cwidget = (GtkWidget*)data;
	
	unsigned long Default = (unsigned long) g_object_get_data(G_OBJECT(cwidget),"ddc_default");
	struct control_db *control = (struct control_db*)g_object_get_data(G_OBJECT(cwidget),"ddc_control");
	
	if (control) {
		write_dbctrl(control, Default);
		
		change_control_value(cwidget, (gpointer)Default);
	}
}
	
static void tree_selection_changed_callback(GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint  id;
	GtkWidget *notebook;
	
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model,&iter,ID_COL,&id,-1);
		gtk_tree_model_get(model,&iter,WIDGET_COL,&notebook,-1);
		
		gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),id);
	}
}


// Initializers
////////////////////

void show_profile_checks(gboolean show) {
	GSList* list = all_controls;
	
	while (list) {
		GtkWidget* check_frame = (GtkWidget*)g_object_get_data(G_OBJECT(list->data), "check_frame");
		GtkWidget* check = (GtkWidget*)g_object_get_data(G_OBJECT(list->data), "check");
		if (check_frame) {
			if (show)
				gtk_widget_show(check_frame);
			else
				gtk_widget_hide(check_frame);
		}
		if (check && show) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), FALSE);
		}
		list = g_slist_next(list);
	}
}

/* returns: Number of selected controls */
int get_profile_checked_controls(unsigned char* controls) {
	int current = 0;
	
	GSList* list = all_controls;
	struct control_db *control;
	
	while (list) {
		GtkWidget* check = (GtkWidget*)g_object_get_data(G_OBJECT(list->data), "check");
		if (check) {
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check))) {
				control = (struct control_db*)g_object_get_data(G_OBJECT(list->data), "ddc_control");
				controls[current] = control->address;
				current++;
			}
		}
		list = g_slist_next(list);
	}
	
	return current;
}

static void createControl(GtkWidget *parent,struct control_db *control)
{
	unsigned short currentDefault = 1;
	unsigned short currentMaximum = 1;
	if (control->type != command)
		get_value_and_max(control,&currentDefault,&currentMaximum);
	
	GtkWidget* hbox = gtk_hbox_new(FALSE,0);
	GtkWidget *widget = NULL;
	GtkWidget *button = NULL;
	GtkWidget *check_frame = NULL; /* Frame around the check */
	GtkWidget *check = NULL;
	
	switch (control->type)
	{
		case value:
		case list:
			button = gtk_button_new();
			GtkWidget *image = gtk_image_new_from_stock(GTK_STOCK_UNDO, GTK_ICON_SIZE_LARGE_TOOLBAR);
			gtk_widget_show(image);
			gtk_container_add(GTK_CONTAINER(button), image);
			gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
			
			check_frame = gtk_frame_new(NULL);
			check = gtk_check_button_new();
			image = gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_LARGE_TOOLBAR);
			gtk_widget_show(image);
			gtk_container_add(GTK_CONTAINER(check), image);
			gtk_widget_show(check);
			gtk_container_add(GTK_CONTAINER(check_frame),check);
			break;
		default:
			button = NULL;
			break;
	}
	
	switch (control->type)
	{
		case value:
			{
				widget = gtk_hscale_new_with_range(0.0, 100.0, 1.0);
				gtk_scale_set_digits(GTK_SCALE(widget), 1);
				g_object_set_data(G_OBJECT(widget), "ddc_default", (gpointer)(long)currentDefault);
				g_object_set_data(G_OBJECT(widget), "ddc_max", (gpointer)(long)currentMaximum);
				g_object_set_data(G_OBJECT(widget), "restore_button", button);
				g_object_set_data(G_OBJECT(widget), "check_frame", check_frame);
				g_object_set_data(G_OBJECT(widget), "check", check);
				
				gtk_range_set_increments(GTK_RANGE(widget),
						100.0/(double)currentMaximum,
						100.0/(double)currentMaximum);
				gtk_range_set_value(GTK_RANGE(widget),
						(double)100.0*currentDefault/(double)currentMaximum);
						
				g_signal_connect_after(G_OBJECT(widget), "change-value", G_CALLBACK(range_callback), NULL);
			}
			break;
		case command:
			{
				widget = gtk_vbox_new(TRUE, 0);
				struct value_db* value;
				for (value = control->value_list; value != NULL; value = value->next)
				{
					GtkWidget* button = gtk_button_new_with_label((char*)value->name);
					g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(command_callback),value);
					g_object_set_data(G_OBJECT(button),"ddc_control",control);
					gtk_widget_show(button);
					gtk_box_pack_start(GTK_BOX(widget), button, FALSE, FALSE, 5);
				}
			}
			break;
		case list:
			{
				widget = gtk_vbox_new(TRUE, 0);
				struct value_db* value;
				GSList* group = NULL;
				for (value = control->value_list; value != NULL; value = value->next)
				{
					GtkWidget* radio = gtk_radio_button_new_with_label(group, (char*)value->name);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (value->value == currentDefault));
					g_object_set_data(G_OBJECT(radio), "ddc_value", value);
					gtk_widget_show(radio);
					gtk_box_pack_start(GTK_BOX(widget), radio, TRUE, TRUE, 0);
					group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
					g_signal_connect(G_OBJECT(radio), "toggled", G_CALLBACK(group_callback), NULL);
				}
				
				g_object_set_data(G_OBJECT(widget), "ddc_default", (gpointer)(long)currentDefault);
				g_object_set_data(G_OBJECT(widget), "restore_button", button);
				g_object_set_data(G_OBJECT(widget), "check_frame", check_frame);
				g_object_set_data(G_OBJECT(widget), "check", check);
				
				break;
			}
		default:
			return;
	}
	if (check) {
		//gtk_widget_show(check_frame);
		gtk_box_pack_start(GTK_BOX(hbox), check_frame, 0, 0, 0);
	}
	
	all_controls = g_slist_append(all_controls,widget);
	g_object_set_data(G_OBJECT(widget), "ddc_control", control);
	/*g_print("%i - %i\n",all_controls,g_slist_length(all_controls));*/
	gtk_widget_show(widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, 1, 1, 0);
	
	if (button) {
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(restore_callback), widget);
		gtk_widget_show(button);
		gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 0);
	}
	
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(parent),hbox);
}
	
static void createPage(GtkWidget* notebook, struct subgroup_db* subgroup)
{
	int i=0;
	GtkWidget* mainvbox = gtk_vbox_new(FALSE,10);
	GtkWidget* vbox = gtk_vbox_new(FALSE,10);
	GtkWidget* frame;
	
	struct control_db* control;
	for (control = subgroup->control_list; control != NULL; control = control->next)
	{
		frame = gtk_frame_new((char*)control->name);
		gtk_container_set_border_width(GTK_CONTAINER(frame),5);
		createControl(frame,control);
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(vbox),frame,0,0,0);
		i++;
	}
	
	if (subgroup->pattern) {
		GtkWidget* align = gtk_alignment_new(0.5, 0, 0, 0);
		
		GtkWidget* button = stock_label_button(GTK_STOCK_FULLSCREEN, _("Show fullscreen patterns"), NULL);
		gtk_widget_show(button);
		gtk_container_add(GTK_CONTAINER(align), button);
		gtk_widget_show(align);
		gtk_box_pack_end(GTK_BOX(mainvbox), align, TRUE, 5, 5);
		g_object_set_data(G_OBJECT(button), "pattern", subgroup->pattern);
		g_object_set_data(G_OBJECT(button), "mainvbox", mainvbox);
		g_object_set_data(G_OBJECT(button), "vbox", vbox);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(fullscreen_callback), value);
	}
	
	gtk_box_pack_start(GTK_BOX(mainvbox), vbox, 0, 5, 5);
	gtk_widget_show(vbox);
	
	GtkWidget* label;
	label = gtk_label_new((char*)subgroup->name);
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), mainvbox, label);
	gtk_widget_show(mainvbox);
}

	
static GtkWidget* createTree(GtkWidget *notebook)
{
	GtkTreeStore *store = gtk_tree_store_new(N_COLS,G_TYPE_STRING,G_TYPE_INT,G_TYPE_POINTER);
	
	GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Section"),renderer,"text",TITLE_COL,NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree),column);
	
	GtkTreeIter top_iter;
	GtkTreeIter sub_iter;
	
	struct group_db* group;
	struct subgroup_db* subgroup;
	
	if (mon->db)
	{
		int id = 1;
		for (group = mon->db->group_list; group != NULL; group = group->next)
		{
			gtk_tree_store_append(store,&top_iter,NULL);
			gtk_tree_store_set(store,&top_iter,TITLE_COL,group->name,ID_COL,0,WIDGET_COL,notebook,-1);
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next)
			{
				gtk_tree_store_append(store,&sub_iter,&top_iter);
				gtk_tree_store_set(store,&sub_iter,TITLE_COL,subgroup->name,ID_COL,id,WIDGET_COL,notebook,-1);
				id++;
			}
		}
	}
	
	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree),0);
	// gtk_container_set_border_width(GTK_CONTAINER(tree),1);
	
	GtkTreeSelection *select;
	
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (select), "changed",
			G_CALLBACK (tree_selection_changed_callback),
			NULL);
	
	return tree;
}

void create_monitor_manager(struct monitorlist* monitor)
{
	modified = 0;
	all_controls = NULL;
	
	if (!monitor->supported) {
		set_message(_(
			"The current monitor is in the database but does not support "
			"DDC/CI.\n\nThis often occurs when you connect the VGA/DVI cable "
			"on the wrong input of monitors supporting DDC/CI only on one of "
			"its two inputs.\n\n"
			"If your monitor has two inputs, please try to connect the cable "
			"to the other input, and then click on the refresh button near "
			"the monitor list."));
		monitor_manager = NULL;
		return;
	}
	
	gchar* tmp = g_strdup_printf(_("Opening the monitor device (%s)..."), monitor->filename);
	set_message(tmp);
	g_free(tmp);
	
	if (ddcci_dbus_open(ddccontrol_proxy, &mon, monitor->filename) < 0) {
		set_message(_(
			"An error occurred while opening the monitor device.\n"
			"Maybe this monitor was disconnected, please click on "
			"the refresh button near the monitor list."));
		monitor_manager = NULL;
		return;
	}
	
	if (!mon->db) {
		tmp = g_strdup_printf(_(
			"The current monitor is not supported, please run\n"
			"%s\n"
			"and send the output to ddccontrol-users@lists.sourceforge.net.\n"
			"Thanks."), "<tt>LANG= LC_ALL= ddccontrol -p -c -d</tt>");
		set_message(tmp);
		g_free(tmp);
		monitor_manager = NULL;
		return;
	}
	
	GtkWidget *notebook = gtk_notebook_new();
	
	struct group_db* group;
	struct subgroup_db* subgroup;
	
	GtkWidget *tree = createTree(notebook);
	
	//////////////////////////////	
	GtkWidget* none = gtk_table_new(1, 1, TRUE);
	
	GtkWidget *valuelabel = gtk_label_new (_("Please click on a group name."));
	gtk_table_attach(GTK_TABLE(none), valuelabel, 0, 1, 0, 1, GTK_FILL_EXPAND, GTK_EXPAND, 5, 5);
	
	GtkWidget *label = gtk_label_new("zero");
	
	gtk_widget_show(valuelabel);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), none, label);
	gtk_widget_show(none);
	/////////////////////////////
	
	if (mon->db)
	{
		int count = 0, current = 0;
		for (group = mon->db->group_list; group != NULL; group = group->next)
		{
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next)
				count++;
		}
		
		gchar* tmp = g_strdup_printf(_("Getting controls values (%d%%)..."), (current*100)/count);
		set_message(tmp);
		g_free(tmp);
		for (group = mon->db->group_list; group != NULL; group = group->next)
		{
			gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook),0);
			//	gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook),0);
			
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next) {
				createPage(notebook, subgroup);
				current++;
				gchar* tmp = g_strdup_printf(_("Getting controls values (%d%%)..."), (current*100)/count);
				set_message(tmp);
				g_free(tmp);
			}
		}
	}
	
	GtkWidget* table = gtk_table_new(2, 1, FALSE);
	gtk_widget_show (tree);
	gtk_table_attach(GTK_TABLE(table), tree, 0, 1, 0, 1, 0, GTK_FILL_EXPAND, 3, 0);
	gtk_widget_show (notebook);
	gtk_table_attach(GTK_TABLE(table), notebook, 1, 2, 0, 1, GTK_FILL_EXPAND, GTK_FILL_EXPAND, 3, 0);
	gtk_widget_show (table);
	
	set_message(_("Loading profiles..."));
	
	ddcci_get_all_profiles(mon);
	
	create_profile_manager();
	
	monitor_manager = table;
	
	if (mon->fallback) {
		/* Put a big warning. */
		gchar* message;
		if (mon->fallback == 1) {
			message = g_strdup(_(
				"There is no support for your monitor in the database, but ddccontrol is "
				"using a generic profile for your monitor's manufacturer. Some controls "
				"may not be supported, or may not work as expected.\n"));
		}
		else { /*if (mon->fallback == 2) {*/
			message = g_strdup(_(
				"There is no support for your monitor in the database, but ddccontrol is "
				"using a basic generic profile. Many controls will not be supported, and "
				"some controls may not work as expected.\n"));
		}

		gchar* tmp = g_strconcat("<span size='large' weight='ultrabold'>", _("Warning!"), "</span>\n\n", 
				message, _(
				"Please update ddccontrol-db, or, if you are already using the latest "
				"version, please send the output of the following command to "
				"ddccontrol-users@lists.sourceforge.net:\n"
				),
				"\n<tt>LANG= LC_ALL= ddccontrol -p -c -d</tt>\n\n",
				_("Thank you.\n"), NULL);
		gtk_widget_show(monitor_manager);
		set_message_ok(tmp, 1);
		g_free(message);
		g_free(tmp);
	}
	else {
		set_message("");
		gtk_widget_show(monitor_manager);
	}
}

// Cleanup
////////////////////
	
void delete_monitor_manager()
{
	if (mon) {
		if (modified)
			ddcci_save(mon);
	
		free(mon);
		g_slist_free(all_controls);
	}
	
	if (profile_manager) {
		gtk_widget_destroy(profile_manager);
		profile_manager = NULL;
	}
}
