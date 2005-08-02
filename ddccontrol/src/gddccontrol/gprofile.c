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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "notebook.h"

void cancelprofile_callback(GtkWidget *widget, gpointer data)
{
	gtk_widget_hide(saveprofile_button);
	gtk_widget_hide(cancelprofile_button);
	gtk_widget_show(profile_manager_button);
	show_profile_checks(FALSE);
}

void saveprofile_callback(GtkWidget *widget, gpointer data)
{
	char controls[256];
	int size;
	struct profile* profile;
	
	gtk_widget_hide(saveprofile_button);
	gtk_widget_hide(cancelprofile_button);
	gtk_widget_show(profile_manager_button);
	show_profile_checks(FALSE);
	
	size = get_profile_checked_controls(&controls[0]);
	
	set_status(_("Creating profile..."));
	
	/* TODO: Ask for a profile name */
	
	profile = create_profile(mon, controls, size, "profname");
	
	if (!profile) {
		GtkWidget* dialog = gtk_message_dialog_new(NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Error while creating profile."));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		set_status("");
		return;
	}
	
	/* TODO: Show profile */
	
	set_status(_("Saving profile..."));
	
	save_profile(profile);
	
	if (!profile) {
		GtkWidget* dialog = gtk_message_dialog_new(NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_CLOSE,
				_("Error while saving profile."));
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		set_status("");
		return;
	}
	
	free_profile(profile);
	
	set_status("");
}

/* Callbacks */
static void apply_callback(GtkWidget *widget, gpointer data)
{
	struct profile* profile = (struct profile*)data;
	
	set_status(_("Applying profile..."));
	
	apply_profile(profile, mon);
	
	refresh_all_controls(widget, data);
	
	set_current_main_component(0);
}


static void create_callback(GtkWidget *widget, gpointer data) {
	show_profile_checks(TRUE);
	
	set_current_main_component(0);
	
	gtk_widget_hide(profile_manager_button);
	gtk_widget_show(saveprofile_button);
	gtk_widget_show(cancelprofile_button);
}

static void close_profile_manager(GtkWidget *widget, gpointer data) {
	set_current_main_component(0);
}

/* Initializers */
void create_profile_manager()
{
	GtkWidget* align;
	GtkWidget* table;
	GtkWidget* label;
	GtkWidget* button;
	GtkWidget* hsep;
	GtkWidget* hbox;
	struct profile* profile;
	int count = 0;
	int crow = 0;
	
	profile = mon->profiles;
	
	while (profile != NULL) {
		count++;
		profile = profile->next;
	}
	
	if (count == 0) {
		profile_manager = NULL;
		return;
	}
	
	align = gtk_alignment_new(0.5, 0.1, 0.8, 0);
	table = gtk_table_new((count*2)+2, 3, FALSE);
	
	profile = mon->profiles;
	
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), g_strdup_printf("<span size='large' weight='ultrabold'>%s</span>", _("Profile Manager")));
	gtk_table_attach(GTK_TABLE(table), label, 0, 3, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 0, 5);
	gtk_widget_show(label);
	crow++;
	
	while (profile != NULL) {
		hsep = gtk_hseparator_new();
		gtk_table_attach(GTK_TABLE(table), hsep, 0, 3, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 0, 5);
		crow++;
		gtk_widget_show (hsep);
		
		label = gtk_label_new(profile->name);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 0, 5);
		gtk_widget_show(label);
		
		button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
		gtk_table_attach(GTK_TABLE(table), button, 1, 2, crow, crow+1, GTK_SHRINK, 0, 0, 5);
		gtk_widget_show(button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(apply_callback), profile);
		
		crow++;
		
		profile = profile->next;
	}
	
	hsep = gtk_hseparator_new();
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 3, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 0, 5);
	crow++;
	gtk_widget_show (hsep);
	
	GtkWidget* salign = gtk_alignment_new(0.5, 0, 0, 0);
	hbox = gtk_hbox_new(FALSE, 10);
	
	button = stock_label_button(GTK_STOCK_SAVE, _("Create profile"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(create_callback), NULL);

	gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 0);
	gtk_widget_show(button);
	
	button = stock_label_button(GTK_STOCK_CLOSE, _("Close profile manager"));
	g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(close_profile_manager), NULL);

	gtk_box_pack_start(GTK_BOX(hbox), button, 0, 0, 0);
	gtk_widget_show(button);
	
	gtk_container_add(GTK_CONTAINER(salign), hbox);
	gtk_widget_show(salign);
	
	gtk_table_attach(GTK_TABLE(table), salign, 0, 3, crow, crow+1, GTK_FILL_EXPAND, GTK_SHRINK, 8, 8);
	gtk_widget_show(hbox);
	crow++;
	
	gtk_container_add(GTK_CONTAINER(align), table);
	gtk_widget_show(table);
	
	profile_manager = align;
}

