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

#ifndef NOTEBOOK_H
#define NOTEBOOK_H

#include "config.h"

#include <gdk/gdk.h>

#include <gtk/gtk.h>

#include "ddcci.h"
#include "monitor_db.h"
#include "conf.h"

/* constants */
#define GTK_FILL_EXPAND (GtkAttachOptions)(GTK_FILL|GTK_EXPAND)

#ifndef GTK_STOCK_EDIT /* GTK <2.6 */
#define GTK_STOCK_EDIT GTK_STOCK_PREFERENCES
#endif

#ifndef GTK_STOCK_FULLSCREEN /* GTK <2.8 */
#define GTK_STOCK_FULLSCREEN GTK_STOCK_EXECUTE
#endif

/* globals */
extern struct monitor* mon;

extern GtkWidget* main_app_window;

extern GtkWidget* monitor_manager;
extern GtkWidget* profile_manager;

/* notebook.c */

void create_monitor_manager(struct monitorlist* monitor);
void delete_monitor_manager();

void show_profile_checks(gboolean show);
int get_profile_checked_controls(unsigned char* controls);

void refresh_all_controls(GtkWidget *widget, gpointer nval);

short get_control_max(struct control_db *control);

/* gprofile.c */
void create_profile_manager();

void saveprofile_callback(GtkWidget *widget, gpointer data);
void cancelprofile_callback(GtkWidget *widget, gpointer data);

void show_profile_information(struct profile* profile, gboolean new_profile);

/* fspatterns.c */

void fullscreen_callback(GtkWidget *widget, gpointer data);

/* main.c */

/* Set what is now displayed at the center of the main window:
 *  0 - Monitor manager
 *  1 - Profile manager
 */
void set_current_main_component(int component);

/* Set the status message */
void set_status(char* message);

/* Show a message on top of every other controls. */
void set_message(char* message);
void set_message_ok(char* message, int with_ok);

GtkWidget *stock_label_button(const gchar * stockid, const gchar *label_text, const gchar *tool_tip);

extern GtkWidget* profile_manager_button;
extern GtkWidget* saveprofile_button;
extern GtkWidget* cancelprofile_button;
extern GtkWidget* refresh_controls_button;

/* Multimonitor support */
extern int current_monitor; /* current monitor */
extern int num_monitor; /* total number of monitors */

#endif //NOTEBOOK_H
