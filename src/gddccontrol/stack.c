/***************************************************************************
 *   Copyright (C) 2004-2005 by Nicolas Boichat                            *
 *   Copyright (C) 2004-2026 by DDCcontrol authors and contributors        *
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

#include "gui.h"
#include "internal.h"
#include "monitor_db_internal.h"

#include "daemon/dbus_client.h"
	
#define GTK_FILL_EXPAND (GtkAttachOptions)(GTK_FILL|GTK_EXPAND)

#define RETRYS 5 // number of read retrys

enum
{
	TITLE_COL,
	PAGE_WIDGET,
	PARENT_CONTAINER,
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

static void set_accessible_name_and_description(GtkWidget *widget, const gchar *name, const gchar *description)
{
	AtkObject *accessible = gtk_widget_get_accessible(widget);

	if (name)
		atk_object_set_name(accessible, name);
	if (description)
		atk_object_set_description(accessible, description);
}

static gboolean is_control_updating(GtkWidget *widget)
{
	return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "ddc_updating"));
}

static void set_control_updating(GtkWidget *widget, gboolean updating)
{
	g_object_set_data(G_OBJECT(widget), "ddc_updating", GINT_TO_POINTER(updating));
}

static guint digits_for_step(double step)
{
	guint digits = 0;

	while (step > 0.0 && step < 1.0 && digits < 4) {
		step *= 10.0;
		digits++;
	}

	return digits;
}

static gboolean can_reset_value_control(GtkWidget *widget)
{
	gboolean has_control_range = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "ddc_has_range"));
	unsigned long Default = (unsigned long)g_object_get_data(G_OBJECT(widget), "ddc_default");
	unsigned long Maximum = (unsigned long)g_object_get_data(G_OBJECT(widget), "ddc_max");
	unsigned long Current = (unsigned long)(long)g_object_get_data(G_OBJECT(widget), "ddc_current");

	return has_control_range && Default <= Maximum && Current != Default;
}

static void update_value_control_range(GtkWidget *widget, unsigned short maximum)
{
	gboolean has_control_range = maximum > 0;
	unsigned long controlMaximum = has_control_range ? maximum : 1;
	GtkWidget *spinButton = (GtkWidget*)g_object_get_data(G_OBJECT(widget), "ddc_spin_button");
	GtkWidget *button = (GtkWidget*)g_object_get_data(G_OBJECT(widget), "restore_button");
	GtkWidget *profileCheckBox = (GtkWidget*)g_object_get_data(G_OBJECT(widget), "profile_check_box");
	double step = 100.0/(double)controlMaximum;
	double page_step = 10.0*step;
	guint digits = digits_for_step(step);

	g_object_set_data(G_OBJECT(widget), "ddc_max", (gpointer)(long)controlMaximum);
	g_object_set_data(G_OBJECT(widget), "ddc_has_range", GINT_TO_POINTER(has_control_range));

	set_control_updating(widget, TRUE);
	gtk_range_set_increments(GTK_RANGE(widget), step, page_step);
	gtk_scale_set_digits(GTK_SCALE(widget), digits);
	if (spinButton)
		gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spinButton), digits);
	set_control_updating(widget, FALSE);

	gtk_widget_set_sensitive(widget, has_control_range);
	if (spinButton)
		gtk_widget_set_sensitive(spinButton, has_control_range);
	if (profileCheckBox)
		gtk_widget_set_sensitive(profileCheckBox, has_control_range);
	if (button)
		gtk_widget_set_sensitive(button, can_reset_value_control(widget));
}

static void write_dbctrl(struct control_db *control, unsigned short nval) {
	ddcci_writectrl(mon, control->address, nval, control->delay);
}
	
static void get_value_and_max(struct control_db *control, unsigned short *currentValue, unsigned short *currentMaximum)
{
	if (control->type == command)
		return;

	int result = ddcci_readctrl(mon, control->address, currentValue, currentMaximum);
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
				if (control->type == value)
					update_value_control_range(list->data, currentMaximum);
				change_control_value(list->data, (gpointer)(long)currentValue);
			}
			tmp = g_strdup_printf(_("Refreshing controls values (%d%%)..."), (current*100)/count);
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
			unsigned short val = ddcci_value_db_value16(value);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), (long)nval == val);
		}
		return;
	}
	
	switch(control->type)
	{
		case value:
		{
			unsigned long Maximum = (unsigned long) g_object_get_data(G_OBJECT(widget),"ddc_max");
			unsigned long Default = (unsigned long) g_object_get_data(G_OBJECT(widget),"ddc_default");
			GtkWidget* button = (GtkWidget*)g_object_get_data(G_OBJECT(widget),"restore_button");
			gboolean has_control_range = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "ddc_has_range"));
			unsigned long value = (unsigned long)nval;

			if (!has_control_range) {
				value = Default;
			}
			else if (value > Maximum) {
				value = Maximum;
			}

			set_control_updating(widget, TRUE);
			gtk_range_set_increments(GTK_RANGE(widget), 100.0/(double)Maximum, 10.0*100.0/(double)Maximum);
			gtk_range_set_value(GTK_RANGE(widget), 100.0*(double)value/(double)Maximum);
			set_control_updating(widget, FALSE);
			g_object_set_data(G_OBJECT(widget), "ddc_current", (gpointer)(long)value);
			if (button)
				gtk_widget_set_sensitive(GTK_WIDGET(button), can_reset_value_control(widget));
			break;
		}
		case command:
			break;
		case list:
			gtk_container_foreach(GTK_CONTAINER(widget), change_control_value, nval);
			break;
	}
}

static void apply_range_value(GtkWidget *widget)
{
	if (is_control_updating(widget)) {
		return;
	}

	struct control_db *control = (struct control_db*)g_object_get_data(G_OBJECT(widget),"ddc_control");
	if (!control) {
		return;
	}
	GtkWidget* button = (GtkWidget*)g_object_get_data(G_OBJECT(widget),"restore_button");
	g_return_if_fail(button);
	
	double val = gtk_range_get_value(GTK_RANGE(widget))/100.0;
	
	unsigned long Maximum = (unsigned long) g_object_get_data(G_OBJECT(widget),"ddc_max");
	unsigned long Current = (unsigned long) g_object_get_data(G_OBJECT(widget),"ddc_current");

#if 0
	printf("Would change %#x to %#x (%f, %f, %#x)\n",
			control->address,
			(unsigned char)round(val*(double)Maximum),
			val*(double)Maximum,
			val,
			Maximum);
#endif
	
	unsigned short nval = (unsigned short)round(val*(double)Maximum);
	if (!refreshing && nval != Current) {
		write_dbctrl(control, nval);
		g_object_set_data(G_OBJECT(widget), "ddc_current", (gpointer)(long)nval);
		modified = 1;
	}
	
	set_control_updating(widget, TRUE);
	gtk_range_set_value(GTK_RANGE(widget), (double)100.0*nval/(double)Maximum);
	set_control_updating(widget, FALSE);
	
	gtk_widget_set_sensitive(GTK_WIDGET(button), can_reset_value_control(widget));
}

static gboolean range_callback(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer data)
{
	(void)scroll;
	(void)value;
	(void)data;
	apply_range_value(GTK_WIDGET(range));
	return FALSE;
}

static void spin_button_commit_callback(GtkWidget *widget, gpointer data)
{
	GtkWidget *range = GTK_WIDGET(data);

	if (is_control_updating(range)) {
		return;
	}

	gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
	apply_range_value(range);
}

static void spin_button_change_value_callback(GtkSpinButton *spinButton, GtkScrollType scroll, gpointer data)
{
	(void)scroll;
	spin_button_commit_callback(GTK_WIDGET(spinButton), data);
}

static gboolean spin_button_focus_out_callback(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	(void)event;
	spin_button_commit_callback(widget, data);
	return FALSE;
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
		unsigned short val = ddcci_value_db_value16(value);
		
#if 0
		printf("Would change %#x to %#x\n", currentControl->address, val);
#endif
	
		if (!refreshing) {
			write_dbctrl(control, val);
			modified = 1;

			/* Refresh if needed */
			if (control->refresh == all) {
				refresh_all_controls(NULL, NULL);
			}
		}
		
		gtk_widget_set_sensitive(GTK_WIDGET(button), val != Default);
	}
}
	
static void command_callback(GtkWidget *widget, gpointer data)
{
	struct value_db *value = (struct value_db*) data;
	struct control_db *control = (struct control_db*)g_object_get_data(G_OBJECT(widget),"ddc_control");
	
	unsigned short val;
	
	val = ddcci_value_db_value16(value);
	
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
		if (control->type == value && !can_reset_value_control(cwidget)) {
			return;
		}

		write_dbctrl(control, Default);
		
		change_control_value(cwidget, (gpointer)Default);
	}
}
	
static void tree_selection_changed_callback(GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkWidget *page;
	GtkWidget *parent_container;
	
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, PAGE_WIDGET, &page, -1);
		gtk_tree_model_get(model, &iter, PARENT_CONTAINER, &parent_container, -1);
		
		gtk_stack_set_visible_child(GTK_STACK(parent_container), page);
	}
}

static void section_cell_data_func(GtkTreeViewColumn *column,
                                   GtkCellRenderer *renderer,
                                   GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   gpointer data)
{
	GtkTreeIter parent;
	gboolean is_group = !gtk_tree_model_iter_parent(model, &parent, iter);

	(void)column;
	(void)data;

	g_object_set(renderer,
			"weight", is_group ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
			"weight-set", TRUE,
			NULL);
}


// Initializers
////////////////////

void show_profile_checks(gboolean show) {
	GSList* list = all_controls;
	
	while (list) {
		GtkWidget* profileCheckBox = (GtkWidget*)g_object_get_data(G_OBJECT(list->data), "profile_check_box");
		if (profileCheckBox) {
			if (show)
				gtk_widget_show(profileCheckBox);
			else
				gtk_widget_hide(profileCheckBox);
		}
		if (profileCheckBox && show) {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(profileCheckBox), FALSE);
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
		GtkWidget* profileCheckBox = (GtkWidget*)g_object_get_data(G_OBJECT(list->data), "profile_check_box");
		if (profileCheckBox) {
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(profileCheckBox))) {
				control = (struct control_db*)g_object_get_data(G_OBJECT(list->data), "ddc_control");
				controls[current] = control->address;
				current++;
			}
		}
		list = g_slist_next(list);
	}
	
	return current;
}

static GtkWidget* createControlWidgets(struct control_db *control)
{
	unsigned short currentDefault = 1;
	unsigned short currentMaximum = 1;
	if (control->type != command)
		get_value_and_max(control,&currentDefault,&currentMaximum);
	
	GtkWidget* outerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	GtkWidget* controlWithUndo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	GtkWidget *widget = NULL;
	GtkWidget *displayWidget = NULL;
	GtkWidget *undoButton = NULL;
	GtkWidget *profileCheckBox = NULL;
	
	switch (control->type)
	{
		case value:
		case list:
			{
				const gchar *control_name = (const gchar*)control->name;
				gchar *reset_name = g_strdup_printf(_("Reset %s"), control_name);

				undoButton = button_from_icon_name("edit-undo", _("_Reset"), _("Reset to initial value"));
				set_accessible_name_and_description(undoButton, reset_name, _("Reset to initial value"));
				g_free(reset_name);
			}
			gtk_widget_set_sensitive(GTK_WIDGET(undoButton), FALSE);
			
			profileCheckBox = gtk_check_button_new_with_label(_("Include in new profile"));
			break;
		default:
			break;
	}
	
	switch (control->type)
	{
		case value:
			{
				GtkAdjustment *adjustment;
				GtkWidget *spinButton;
				gboolean has_control_range = currentMaximum > 0;
				unsigned short controlMaximum = has_control_range ? currentMaximum : 1;
				unsigned short controlValue = currentDefault;
				double step = 100.0/(double)controlMaximum;
				double page_step = 10.0*step;
				guint digits = digits_for_step(step);
				double currentPercent;
				const gchar *control_name = (const gchar*)control->name;
				gchar *value_description = g_strdup_printf(_("Value for %s"), control_name);

				if (!has_control_range) {
					g_warning(_("Control %s reports a zero maximum value."), control->name);
					controlValue = 0;
				}
				else if (controlValue > controlMaximum) {
					controlValue = controlMaximum;
				}
				currentPercent = (double)100.0*controlValue/(double)controlMaximum;

				adjustment = gtk_adjustment_new(currentPercent, 0.0, 100.0, step, page_step, 0.0);
				displayWidget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
				widget = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
				spinButton = gtk_spin_button_new(adjustment, 1.0, digits);

				gtk_scale_set_digits(GTK_SCALE(widget), digits);
				gtk_scale_set_draw_value(GTK_SCALE(widget), FALSE);
				gtk_widget_set_hexpand(widget, TRUE);
				gtk_widget_set_size_request(widget, 240, -1);
				gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinButton), TRUE);
				gtk_entry_set_width_chars(GTK_ENTRY(spinButton), 5);
				set_accessible_name_and_description(widget, (gchar*)control->name, value_description);
				set_accessible_name_and_description(spinButton, (gchar*)control->name, value_description);
				g_object_set_data(G_OBJECT(widget), "ddc_default", (gpointer)(long)controlValue);
				g_object_set_data(G_OBJECT(widget), "ddc_max", (gpointer)(long)controlMaximum);
				g_object_set_data(G_OBJECT(widget), "ddc_current", (gpointer)(long)controlValue);
				g_object_set_data(G_OBJECT(widget), "ddc_has_range", GINT_TO_POINTER(has_control_range));
				g_object_set_data(G_OBJECT(widget), "ddc_spin_button", spinButton);
				g_object_set_data(G_OBJECT(widget), "restore_button", undoButton);
				g_object_set_data(G_OBJECT(widget), "profile_check_box", profileCheckBox);

				if (!has_control_range) {
					gtk_widget_set_sensitive(widget, FALSE);
					gtk_widget_set_sensitive(spinButton, FALSE);
					if (undoButton)
						gtk_widget_set_sensitive(undoButton, FALSE);
					if (profileCheckBox)
						gtk_widget_set_sensitive(profileCheckBox, FALSE);
				}
				
				gtk_box_pack_start(GTK_BOX(displayWidget), widget, TRUE, TRUE, 0);
				gtk_box_pack_start(GTK_BOX(displayWidget), spinButton, FALSE, FALSE, 0);
				gtk_widget_show(spinButton);
						
				g_signal_connect_after(G_OBJECT(widget), "change-value", G_CALLBACK(range_callback), NULL);
				g_signal_connect(G_OBJECT(spinButton), "activate", G_CALLBACK(spin_button_commit_callback), widget);
				g_signal_connect_after(G_OBJECT(spinButton), "change-value", G_CALLBACK(spin_button_change_value_callback), widget);
				g_signal_connect(G_OBJECT(spinButton), "focus-out-event", G_CALLBACK(spin_button_focus_out_callback), widget);
				g_free(value_description);
			}
			break;
		case command:
			{
				widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
				gtk_box_set_homogeneous(GTK_BOX(widget), TRUE);
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
				widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
				gtk_box_set_homogeneous(GTK_BOX(widget), TRUE);
				struct value_db* value;
				GSList* group = NULL;
				for (value = control->value_list; value != NULL; value = value->next)
				{
					GtkWidget* radio = gtk_radio_button_new_with_label(group, (char*)value->name);
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (ddcci_value_db_value16(value) == currentDefault));
					g_object_set_data(G_OBJECT(radio), "ddc_value", value);
					set_accessible_name_and_description(radio, (gchar*)value->name, (gchar*)control->name);
					gtk_widget_show(radio);
					gtk_box_pack_start(GTK_BOX(widget), radio, TRUE, TRUE, 0);
					group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
					g_signal_connect(G_OBJECT(radio), "toggled", G_CALLBACK(group_callback), NULL);
				}
				
				g_object_set_data(G_OBJECT(widget), "ddc_default", (gpointer)(long)currentDefault);
				g_object_set_data(G_OBJECT(widget), "restore_button", undoButton);
				g_object_set_data(G_OBJECT(widget), "profile_check_box", profileCheckBox);
				
				break;
			}
		default:
			break;
	}
	if (profileCheckBox) {
		gtk_box_pack_start(GTK_BOX(outerBox), profileCheckBox, 0, 0, 0);
	}
	
	all_controls = g_slist_append(all_controls,widget);
	g_object_set_data(G_OBJECT(widget), "ddc_control", control);
	/*g_print("%i - %i\n",all_controls,g_slist_length(all_controls));*/
	gtk_widget_show(widget);
	if (displayWidget == NULL)
		displayWidget = widget;
	gtk_widget_show(displayWidget);
	gtk_box_pack_start(GTK_BOX(controlWithUndo), displayWidget, 1, 1, 0);
	
	if (undoButton) {
		g_signal_connect(G_OBJECT(undoButton), "clicked", G_CALLBACK(restore_callback), widget);
		gtk_widget_show(undoButton);
		gtk_box_pack_start(GTK_BOX(controlWithUndo), undoButton, 0, 0, 0);
	}

	gtk_box_pack_end(GTK_BOX(outerBox), controlWithUndo, 0, 0, 0);
	gtk_widget_show(controlWithUndo);
	gtk_widget_show(outerBox);
	return outerBox;
}
	
static GtkWidget* createPage(GtkWidget* stack, struct subgroup_db* subgroup)
{
	GtkWidget* scroll_area = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_area), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_show(scroll_area);
	
	GtkWidget* mainvbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
	GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
	
	struct control_db* control;
	for (control = subgroup->control_list; control != NULL; control = control->next)
	{
		GtkWidget* frame = gtk_frame_new((char*)control->name);
		gtk_container_set_border_width(GTK_CONTAINER(frame),5);
		GtkWidget* controlWidget = createControlWidgets(control);
		gtk_container_add(GTK_CONTAINER(frame), controlWidget);
		gtk_widget_show(frame);
		gtk_box_pack_start(GTK_BOX(vbox),frame,0,0,0);
	}
	
	if (subgroup->pattern) {
		GtkWidget* button = button_from_icon_name("view-fullscreen", _("_Show fullscreen patterns"), NULL);
		gtk_widget_show(button);
		gtk_widget_set_halign(button, GTK_ALIGN_CENTER);
		gtk_widget_set_valign(button, GTK_ALIGN_START);
		gtk_box_pack_end(GTK_BOX(mainvbox), button, TRUE, 5, 5);
		g_object_set_data(G_OBJECT(button), "pattern", subgroup->pattern);
		g_object_set_data(G_OBJECT(button), "mainvbox", mainvbox);
		g_object_set_data(G_OBJECT(button), "vbox", vbox);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(fullscreen_callback), NULL);
	}
	
	gtk_box_pack_start(GTK_BOX(mainvbox), vbox, 0, 5, 5);
	gtk_widget_show(vbox);
	
	gtk_container_add(GTK_CONTAINER(scroll_area), mainvbox);
	
	gtk_stack_add_named(GTK_STACK(stack), scroll_area, (const gchar*)subgroup->name);
	gtk_widget_show(mainvbox);

	return scroll_area;
}

/* The GtkTreeView acts as a hierarchical GtkStackSwitcher */
static GtkWidget* createTreeAndPages(GtkWidget *stack)
{
	GtkTreeStore *store = gtk_tree_store_new(N_COLS,G_TYPE_STRING,G_TYPE_POINTER,G_TYPE_POINTER);
	
	GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));
	
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ypad", 4, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Section"),renderer,"text",TITLE_COL,NULL);
	gtk_tree_view_column_set_cell_data_func(column, renderer, section_cell_data_func, NULL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree),column);
	
	GtkTreeIter top_iter;
	GtkTreeIter sub_iter;

	////////////////
	GtkWidget* empty_selection_info = gtk_grid_new();
	
	GtkWidget *valuelabel = gtk_label_new (_("Please click on a group name."));
	gtk_grid_attach(GTK_GRID(empty_selection_info), valuelabel, 0, 0, 1, 1);
	gtk_widget_set_hexpand(valuelabel, TRUE);
	gtk_widget_set_vexpand(valuelabel, TRUE);
	gtk_widget_set_margin_top(valuelabel, 5);
	gtk_widget_set_margin_bottom(valuelabel, 5);
	gtk_widget_set_margin_start(valuelabel, 5);
	gtk_widget_set_margin_end(valuelabel, 5);
	gtk_widget_show(valuelabel);
	
	gtk_stack_add_named(GTK_STACK(stack), empty_selection_info, "__zero");
	gtk_widget_show(empty_selection_info);
	gtk_stack_set_visible_child(GTK_STACK(stack), empty_selection_info);
	///////////////////////
	
	
	struct group_db* group;
	struct subgroup_db* subgroup;


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
			gtk_tree_store_append(store,&top_iter,NULL);
			gtk_tree_store_set(store, &top_iter, TITLE_COL, group->name, PAGE_WIDGET, empty_selection_info, PARENT_CONTAINER, stack,-1);
			for (subgroup = group->subgroup_list; subgroup != NULL; subgroup = subgroup->next)
			{
				gtk_tree_store_append(store,&sub_iter,&top_iter);
				
				GtkWidget* currentPage = createPage(stack, subgroup);
				current++;
				tmp = g_strdup_printf(_("Getting controls values (%d%%)..."), (current*100)/count);
				set_message(tmp);
				g_free(tmp);
				
				gtk_tree_store_set(store, &sub_iter, TITLE_COL, subgroup->name, PAGE_WIDGET, currentPage, PARENT_CONTAINER, stack,-1);
			}
		}
	}
	
	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(tree), 6);
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
	
	if (ddccontrol_proxy != NULL) {
		if (ddcci_dbus_open(ddccontrol_proxy, &mon, monitor->filename) < 0) {
			mon = NULL;
			set_message(_(
				"An error occurred while opening the monitor device.\n"
				"Maybe this monitor was disconnected, please click on "
				"the refresh button near the monitor list."));
			monitor_manager = NULL;
			return;
		}
	} else {
		int open_result;
		mon = malloc(sizeof(struct monitor));
		if (mon == NULL) {
			set_message(_("Memory allocation failed while opening the monitor device."));
			monitor_manager = NULL;
			return;
		}
		open_result = ddcci_open(mon, monitor->filename, 0);
		if (open_result < 0) {
			if (open_result != -3) {
				ddcci_close(mon);
			}
			free(mon);
			mon = NULL;
			set_message(_(
				"An error occurred while opening the monitor device.\n"
				"Maybe this monitor was disconnected, please click on "
				"the refresh button near the monitor list."));
			monitor_manager = NULL;
			return;
		}
	}

	if (mon == NULL) {
		set_message(_(
			"An error occurred while opening the monitor device.\n"
			"Maybe this monitor was disconnected, please click on "
			"the refresh button near the monitor list."));
		monitor_manager = NULL;
		return;
	}
	
	if (!mon->db) {
		tmp = g_strdup_printf(_(
			"Unsupported monitor detected.\n\n"
			"Please update ddccontrol-db, or, if you are already using the latest\n"
			"version, please open a github issue:\n"
			"https://github.com/ddccontrol/ddccontrol-db/issues/new?template=unsupported-monitor.yml"
			"\nThen attach the resulting report file of the following command:"
			"%s"),
			"\n<tt>LANG=C LC_ALL=C ddccontrol -p -c -d &amp;&gt; /tmp/ddccontrol-report.txt</tt>\n\n");
		set_message(tmp);
		g_free(tmp);
		monitor_manager = NULL;
		return;
	}
	
	GtkWidget *stack = gtk_stack_new();
	
	GtkWidget *tree = createTreeAndPages(stack);
	
	GtkWidget* grid = gtk_grid_new();
	gtk_widget_show (tree);
	gtk_grid_attach(GTK_GRID(grid), tree, 0, 0, 1, 1);
	gtk_widget_set_vexpand(tree, TRUE);
	gtk_widget_set_margin_top(tree, 3);
	gtk_widget_set_margin_bottom(tree, 0);
	gtk_widget_set_margin_start(tree, 3);
	gtk_widget_set_margin_end(tree, 0);
	gtk_widget_show (stack);
	
	GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_grid_attach(GTK_GRID(grid), separator, 1, 0, 1, 1);
	gtk_widget_show(separator);
	
	gtk_grid_attach(GTK_GRID(grid), stack, 2, 0, 1, 1);
	gtk_widget_set_hexpand(stack, TRUE);
	gtk_widget_set_vexpand(stack, TRUE);
	gtk_widget_set_margin_top(stack, 3);
	gtk_widget_set_margin_bottom(stack, 0);
	gtk_widget_set_margin_start(stack, 3);
	gtk_widget_set_margin_end(stack, 0);
	gtk_widget_show (grid);
	
	set_message(_("Loading profiles..."));
	
	ddcci_get_all_profiles(mon);
	
	create_profile_manager();
	
	monitor_manager = grid;
	
	if (mon->fallback && !hide_unsupported_monitor_warning) {
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

		tmp = g_strconcat("<span size='large' weight='ultrabold'>", _("Warning!"), "</span>\n\n", 
				message, _(
				"Unsupported monitor detected.\n\n"
				"Please update ddccontrol-db, or, if you are already using the latest\n"
				"version, please open a github issue:\n"
				"https://github.com/ddccontrol/ddccontrol-db/issues/new?template=unsupported-monitor.yml"
				"\nThen attach the resulting report file of the following command:\n"),
				"\n<tt>LANG=C LC_ALL=C ddccontrol -p -c -d &amp;> /tmp/ddccontrol-report.txt</tt>\n\n",
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
		int needs_free = (mon->__vtable == NULL);
		if (modified && needs_free)
			ddcci_save(mon);

		ddcci_close(mon);

		if (needs_free)
			free(mon);
		mon = NULL;
	}
	
	if (all_controls) {
		g_slist_free(all_controls);
	}
}
